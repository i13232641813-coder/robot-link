/*
 * sender/sender.ino  —  视觉电控通信协议 · 发送端（ESP32）
 * 依据仓库根目录 README.md（v0.9）实现，改协议前请先和接收端确认。
 *
 * A 板（发送端 / 视觉侧）：算出目标 -> 组帧 -> 从 UART 单向发给 B 板（接收端）。
 * 帧格式：$<x>,<y>,<conf>,<valid>,<ts>,<seq>*<XX>\r\n
 *   - x,y    : int16，毫米
 *   - conf   : uint8，0~100
 *   - valid  : uint8，0/1（无目标时=0，但照常发帧，不停车）
 *   - ts     : uint32，发送端 millis() 毫秒时间戳
 *   - seq    : uint8，每帧 +1，255 后回绕到 0
 *   - XX     : 对 $ 与 * 之间全部字符逐字节异或，两位大写十六进制
 */

// ---------- 可改配置 ----------
#define LINK_SERIAL   Serial2
#define LINK_TX_PIN   17
#define LINK_RX_PIN   16
#define LINK_BAUD     115200
#define PERIOD_MS     20

uint8_t seq = 0;

void buildAndSend(int16_t x, int16_t y, uint8_t conf, uint8_t valid, uint32_t ts) {
  String payload = String(x) + "," + String(y) + "," + String(conf) + "," +
                   String(valid) + "," + String(ts) + "," + String(seq);

  uint8_t xorv = 0;
  for (char c : payload) xorv ^= (uint8_t)c;

  LINK_SERIAL.print('$');
  LINK_SERIAL.print(payload);
  LINK_SERIAL.print('*');
  if (xorv < 16) LINK_SERIAL.print('0');
  LINK_SERIAL.print(xorv, HEX);
  LINK_SERIAL.print("\r\n");

  //=====新增USB给VOFA输出=====
  Serial.printf("%d,%d,%d,%d,%lu,%u\n", x, y, conf, valid, ts, seq);

  seq = (seq + 1) & 0xFF;
}

void setup() {
  LINK_SERIAL.begin(LINK_BAUD, SERIAL_8N1, LINK_RX_PIN, LINK_TX_PIN);
  Serial.begin(115200);   // 开启USB串口！！必须加
}

void loop() {
  uint32_t ts = millis();
  int16_t x = 0, y = 0;
  uint8_t conf = 0, valid = 0;
  if ((ts / 1000) % 6 == 5) {
    valid = 0; x = 0; y = 0; conf = 0;
  } else {
    float a = ts * 0.0008f;
    x = (int16_t)(500 * cos(a));
    y = (int16_t)(500 * sin(a));
    conf = 95; valid = 1;
  }

  buildAndSend(x, y, conf, valid, ts);
  delay(PERIOD_MS);
}

