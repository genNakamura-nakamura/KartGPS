// ESP32 → GPS（IO17 → GPSのRX）の送信が届いているか確認する
// GPSにバージョンを問い合わせ（UBX-MON-VER）、返事が来ればOK。チップのバージョンも表示される
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial2.begin(9600, SERIAL_8N1, 16, 17);
  Serial.println("=== send check start ===");
}

void loop() {
  const uint8_t q[] = {0xB5, 0x62, 0x0A, 0x04, 0x00, 0x00, 0x0E, 0x34}; // バージョン問い合わせ
  const uint8_t head[] = {0xB5, 0x62, 0x0A, 0x04};
  while (Serial2.available()) Serial2.read();
  Serial2.write(q, sizeof(q));

  int match = 0, got = 0;
  String text = "";
  unsigned long t = millis();
  while (millis() - t < 1500) {
    while (Serial2.available()) {
      uint8_t c = Serial2.read();
      if (match < 4) {
        match = (c == head[match]) ? match + 1 : (c == 0xB5 ? 1 : 0);
      } else if (got < 200) {
        got++;
        if (c >= 32 && c < 127) text += (char)c;
        else if (c == 0 && !text.endsWith(" ")) text += ' ';
      }
    }
  }
  if (match == 4) {
    Serial.println("OK: response from GPS -> " + text);
  } else {
    Serial.println("NG: no response (check ESP32 IO17 -> GPS RX)");
  }
  delay(3000);
}
