# KartGPS

ESP32 と安価な u-blox M8 GPS モジュール（Beitian BN-220）で作る、**10Hz の Bluetooth GPS** です。
Android の [RaceChrono](https://racechrono.com/) に外部GPSとして接続し、カートのラップ計測・走行データの記録に使います。

はんだ付け不要、部品代は数千円程度です。

## 特徴

- 起動するたびに GPS を自動設定（u-center などのPCソフト不要）
  - 通信速度 115200bps
  - 更新レート 1〜10Hz（初期値 10Hz）
  - Dynamic Model: Automotive
  - NMEA 出力は GGA / RMC のみ（Bluetooth の帯域を節約）
  - 使用する衛星の組み合わせを選択可能（初期値：GPS＋みちびき＋BeiDou）
  - SBAS を日本の MSAS 専用に設定（任意）
- 設定が失敗したら最大3回やり直し、実測の更新レートをシリアルモニタに表示
- Bluetooth Classic（SPP）で送信するので、RaceChrono からは普通の外部GPSとして見える

## 必要なもの

| 部品 | 備考 |
|---|---|
| ESP32-DevKitC（ESP32-WROOM-32E / 32UE） | **無印ESP32が必須**。C3 / S3 / C6 / H2 は Bluetooth Classic 非対応なので不可。32UE は別途 2.4GHz の IPEX アンテナが必要 |
| Beitian BN-220（u-blox M8030） | BN-180 / BN-280 / BN-880 など u-blox M8 系なら同様に動作するはず |
| SH1.0 4ピン ⇔ デュポン（メス）ケーブル | GPS に付属していればそれでOK |
| データ通信対応の microUSB ケーブル | 充電専用ケーブルでは書き込めない |
| モバイルバッテリー | 走行中の電源 |
| Android スマホ ＋ RaceChrono Pro | 外部GPSの利用には Pro 版が必要 |

## 配線

| BN-220 | ESP32-DevKitC |
|---|---|
| TX | IO16 |
| RX | IO17 |
| VCC | 5V |
| GND | GND |

- BN-220 のコネクタは端から **GND / TX / RX / VCC** の順。線の色ではなく、基板の印字で確認してください。
- TX と RX は「たすき掛け」（GPS の TX → ESP32 の受信ピン IO16）。
- **WROVER 版のボードは IO16 / IO17 が内部で使われているため使えません。** `GPS_RX` / `GPS_TX` を 25 / 26 などに変更してください。

## 書き込み

1. [Arduino IDE](https://www.arduino.cc/en/software) をインストール
2. 「ファイル → 基本設定 → 追加のボードマネージャのURL」に以下を追加
   `https://espressif.github.io/arduino-esp32/package_esp32_index.json`
3. ボードマネージャで **esp32 by Espressif Systems** をインストール
4. ボードに **ESP32 Dev Module** を選択し、`KartGPS/KartGPS.ino` を書き込む
   - ポートが出ない場合：ケーブル（充電専用でないか）と CP210x ドライバを確認
   - 「Connecting...」で止まる場合：基板の BOOT ボタンを押し続ける

シリアルモニタ（115200bps）に次のように出れば成功です。

```
Setting: 10 Hz, GNSS_MODE 4, MSAS ON
GPS config try 1...
GPS config OK (115200bps)
RMC rate: 10 Hz
KartGPS started
$GNRMC,...
$GNGGA,...
```

## 設定

`KartGPS.ino` の先頭で変更できます。

```cpp
const int  RATE_HZ   = 10;    // 更新レート：1〜10
const int  GNSS_MODE = 4;     // 使う衛星の組み合わせ
const bool USE_MSAS  = true;  // SBASをMSAS専用にする
```

| GNSS_MODE | 衛星 |
|---|---|
| 1 | GPS＋みちびき（一番軽い） |
| 2 | GPS＋みちびき＋GLONASS（BN-220 工場出荷時と同じ） |
| 3 | GPS＋みちびき＋Galileo |
| 4 | GPS＋みちびき＋BeiDou（日本でのおすすめ） |
| 5 | GPS＋みちびき＋Galileo＋BeiDou |
| 6 | GPS＋みちびき＋GLONASS＋Galileo |

u-blox M8 は GLONASS と BeiDou を同時に使えません。衛星の種類を増やすと更新レートが落ちる場合があるので、シリアルモニタの `RMC rate` で確認してください。

## RaceChrono との接続

1. Android の Bluetooth 設定で「**KartGPS**」とペアリング
2. RaceChrono の設定から外部デバイス（GPS / Bluetooth）として KartGPS を追加
3. ライブ画面で GPS 受信機の更新レートが 10Hz 前後になっていれば完了

## 診断用スケッチ（`tools/`）

うまく動かないときの切り分けに使います。

| スケッチ | 内容 |
|---|---|
| `tools/GPSCheck` | 受信ピン（IO16/IO17）と通信速度を総当たりし、GPS → ESP32 のデータが届いているか表示 |
| `tools/SendCheck` | GPS にバージョンを問い合わせ、ESP32 → GPS の送信が届いているか確認（チップのバージョンも表示） |

## 取り付けのヒント

- GPS のアンテナ面（ベージュの四角い面）は **空に向ける**。下に金属板があると受信が安定する
- GPS は ESP32・モバイルバッテリー・スマホから **できるだけ離す**（ノイズで衛星が減る）
- デュポン接続部はビニールテープ等で抜け止め、USB ケーブルの根元も固定（振動対策）
- 高いビルに囲まれた場所では衛星が数個しかつかめず、軌跡が大きくずれます。性能確認は開けた場所で
- スマホはシャシーに直付けしない（振動でカメラ等が壊れることがある）

## 注意

- ESP32 は電波を出すため、日本で使う場合は **技適マーク付きのモジュール** を使ってください。
- GPS の位置誤差は 1〜3m 程度です。ラップタイムや周回比較には十分ですが、走行ラインの細かい差は参考程度に見てください。
- 自己責任でご利用ください。

## License

[MIT](LICENSE)
