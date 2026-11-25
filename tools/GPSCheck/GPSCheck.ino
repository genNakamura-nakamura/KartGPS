// GPS配線チェック用：ESP32の受信ピン（IO16/IO17）と通信速度を総当たりして、GPSのデータ（$で始まるNMEA）が見えるか調べる
const int pinsA[2][2] = {{16, 17}, {17, 16}};   // {受信ピン, 送信ピン}
const long bauds[] = {9600, 38400, 57600, 115200};

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("=== GPS check start ===");
}

void loop() {
  for (int p = 0; p < 2; p++) {
    for (long b : bauds) {
      Serial2.end();
      Serial2.begin(b, SERIAL_8N1, pinsA[p][0], pinsA[p][1]);
      delay(200);
      while (Serial2.available()) Serial2.read();   // 古いデータを捨てる
      int bytes = 0, dollars = 0;
      String sample = "";
      unsigned long t = millis();
      while (millis() - t < 2000) {
        while (Serial2.available()) {
          char c = Serial2.read();
          bytes++;
          if (c == '$') dollars++;
          if (sample.length() < 60 && c >= 32 && c < 127) sample += c;
        }
      }
      Serial.printf("RX=IO%d  %6ld bps : %4d bytes, '$' x %d  | %s\n",
                    pinsA[p][0], b, bytes, dollars, sample.c_str());
    }
  }
  Serial.println("=== done (repeat in 5s) ===");
  delay(5000);
}
