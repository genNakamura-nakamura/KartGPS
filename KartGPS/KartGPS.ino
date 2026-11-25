// ============================================================
// KartGPS : ESP32 + u-blox M8 GPS で作る 10Hz Bluetooth GPS
//           （Android + RaceChrono でカートのラップ計測用）
//
// 構成 : ESP32-DevKitC（ESP32-WROOM-32E / 32UE）+ Beitian BN-220（u-blox M8）
// 配線 : GPS TX  → ESP32 IO16
//        GPS RX  → ESP32 IO17
//        GPS VCC → ESP32 5V
//        GPS GND → ESP32 GND
//
// 動作 : 起動時にGPSを 115200bps / 指定Hz / Automotive / GGA+RMCのみ に設定し、
//        NMEAをBluetooth Classic（SPP）で "KartGPS" という名前で送信する
//
// 注意 : ESP32-C3 / S3 / C6 / H2 は Bluetooth Classic 非対応のため使えません
//        WROVER版のボードは IO16/17 が使えないので、GPS_RX/GPS_TX を 25/26 などに変更してください
// ============================================================

// ===== 設定（ここだけ変えればOK） =====
const int  RATE_HZ   = 10;    // 更新レート：1〜10
const int  GNSS_MODE = 4;     // 使う衛星の組み合わせ（下の表から選ぶ）
const bool USE_MSAS  = true;  // SBASを日本のMSAS（PRN129/137）専用にする
// GNSS_MODE
// 1: GPS＋みちびき（一番軽い）
// 2: GPS＋みちびき＋GLONASS（BN-220の工場出荷時と同じ）
// 3: GPS＋みちびき＋Galileo
// 4: GPS＋みちびき＋BeiDou（日本でのおすすめ）
// 5: GPS＋みちびき＋Galileo＋BeiDou
// 6: GPS＋みちびき＋GLONASS＋Galileo
// ※ u-blox M8 は GLONASS と BeiDou を同時に使えません
// ======================================

#include "BluetoothSerial.h"

BluetoothSerial SerialBT;
const int GPS_RX = 16;  // ESP32が受信するピン（GPSのTXをつなぐ）
const int GPS_TX = 17;  // ESP32が送信するピン（GPSのRXをつなぐ）

// ---- u-bloxの設定コマンド(UBX)を送る ----
void sendUBX(uint8_t cls, uint8_t id, const uint8_t *payload, uint16_t len) {
  uint8_t ckA = 0, ckB = 0;
  uint8_t head[4] = {cls, id, (uint8_t)(len & 0xFF), (uint8_t)(len >> 8)};
  Serial2.write(0xB5); Serial2.write(0x62);
  for (int i = 0; i < 4; i++) { Serial2.write(head[i]); ckA += head[i]; ckB += ckA; }
  for (int i = 0; i < len; i++) { Serial2.write(payload[i]); ckA += payload[i]; ckB += ckA; }
  Serial2.write(ckA); Serial2.write(ckB);
  Serial2.flush();
}

// ---- 使う衛星の種類を設定する（CFG-GNSS） ----
void setGNSS(int mode) {
  bool glo = (mode == 2 || mode == 6);
  bool gal = (mode == 3 || mode == 5 || mode == 6);
  bool bds = (mode == 4 || mode == 5);

  uint8_t g[60];
  int k = 0;
  g[k++] = 0x00; g[k++] = 0x00; g[k++] = 0x20; g[k++] = 0x07;  // 使用チャンネル32、設定ブロック7個
  auto block = [&](uint8_t id, uint8_t res, uint8_t mx, bool on) {
    g[k++] = id; g[k++] = res; g[k++] = mx; g[k++] = 0x00;
    g[k++] = on ? 0x01 : 0x00; g[k++] = 0x00; g[k++] = 0x01; g[k++] = 0x00;
  };
  block(0, 8, 16, true);   // GPS
  block(1, 1, 3,  true);   // SBAS
  block(2, 4, 8,  gal);    // Galileo
  block(3, 8, 16, bds);    // BeiDou
  block(4, 0, 8,  false);  // IMES
  block(5, 0, 3,  true);   // みちびき（QZSS）
  block(6, 8, 14, glo);    // GLONASS
  sendUBX(0x06, 0x3E, g, sizeof(g));
  delay(1000);             // GPSが内部で再起動するのを待つ
}

// ---- SBASをMSAS（日本）専用にする（CFG-SBAS） ----
void setMSAS() {
  const uint8_t sbas[8] = {
    0x01,                    // SBASを有効にする
    0x03,                    // 測距＋補正情報を使う（integrityは使わない＝衛星を減らさない）
    0x03,                    // SBAS衛星を最大3つまで使う
    0x00,                    // PRN152〜158は探さない
    0x00, 0x02, 0x02, 0x00   // PRN129とPRN137（MSAS）だけを探す
  };
  sendUBX(0x06, 0x16, sbas, sizeof(sbas));
  delay(100);
}

void configureGPS() {
  // 1) 通信速度を115200bpsに変更（今の速度が分からないので全部の速度で送る）
  const uint8_t prt[20] = {0x01,0x00,0x00,0x00, 0xD0,0x08,0x00,0x00,
                           0x00,0xC2,0x01,0x00, 0x03,0x00,0x02,0x00,
                           0x00,0x00,0x00,0x00};
  const long bauds[] = {9600, 38400, 57600, 115200};
  for (long b : bauds) {
    Serial2.end();
    Serial2.begin(b, SERIAL_8N1, GPS_RX, GPS_TX);
    delay(100);
    sendUBX(0x06, 0x00, prt, sizeof(prt));
    delay(200);
  }
  Serial2.end();
  Serial2.begin(115200, SERIAL_8N1, GPS_RX, GPS_TX);
  delay(300);

  // 2) 使う衛星の種類とSBAS
  setGNSS(GNSS_MODE);
  if (USE_MSAS) setMSAS();

  // 3) 更新レート（1000ミリ秒 ÷ RATE_HZ）
  uint16_t ms = 1000 / RATE_HZ;
  uint8_t rate[6] = {(uint8_t)(ms & 0xFF), (uint8_t)(ms >> 8), 0x01,0x00, 0x01,0x00};
  sendUBX(0x06, 0x08, rate, sizeof(rate)); delay(100);

  // 4) Automotiveモード
  uint8_t nav5[36] = {0};
  nav5[0] = 0x01; nav5[2] = 4;
  sendUBX(0x06, 0x24, nav5, sizeof(nav5)); delay(100);

  // 5) GLL, GSA, GSV, VTG を止めて、GGA と RMC だけ出す
  const uint8_t offIds[] = {0x01, 0x02, 0x03, 0x05};
  for (uint8_t mid : offIds) { uint8_t m[3] = {0xF0, mid, 0}; sendUBX(0x06, 0x01, m, 3); delay(50); }
  const uint8_t onIds[] = {0x00, 0x04};
  for (uint8_t mid : onIds)  { uint8_t m[3] = {0xF0, mid, 1}; sendUBX(0x06, 0x01, m, 3); delay(50); }
}

// ---- 115200bpsでGPSのデータ($)が届いているか確認 ----
bool nmeaOK() {
  while (Serial2.available()) Serial2.read();
  int dollars = 0;
  unsigned long t = millis();
  while (millis() - t < 1500) {
    while (Serial2.available()) if (Serial2.read() == '$') dollars++;
  }
  return dollars >= 3;
}

// ---- 1秒あたりのRMCの数（=更新レート）を数える ----
int rmcPerSec() {
  while (Serial2.available()) Serial2.read();
  int count = 0;
  char p2 = 0, p1 = 0;
  unsigned long t = millis();
  while (millis() - t < 2000) {
    while (Serial2.available()) {
      char c = Serial2.read();
      if (p2 == 'R' && p1 == 'M' && c == 'C') count++;
      p2 = p1; p1 = c;
    }
  }
  return count / 2;
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.printf("Setting: %d Hz, GNSS_MODE %d, MSAS %s\n", RATE_HZ, GNSS_MODE, USE_MSAS ? "ON" : "OFF");
  bool ok = false;
  for (int i = 1; i <= 3 && !ok; i++) {   // うまくいくまで最大3回やり直す
    Serial.printf("GPS config try %d...\n", i);
    configureGPS();
    ok = nmeaOK();
  }
  if (ok) {
    Serial.println("GPS config OK (115200bps)");
    Serial.printf("RMC rate: %d Hz\n", rmcPerSec());
  } else {
    Serial.println("GPS config FAILED");
  }
  SerialBT.begin("KartGPS");     // Androidにはこの名前で表示される
  Serial.println("KartGPS started");
}

void loop() {
  static uint8_t buf[256];
  int n = Serial2.available();
  if (n > 0) {
    if (n > (int)sizeof(buf)) n = sizeof(buf);
    int r = Serial2.readBytes(buf, n);
    SerialBT.write(buf, r);      // スマホへ
    Serial.write(buf, r);        // PCのシリアルモニタへ（確認用）
  }
}
