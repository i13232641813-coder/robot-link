/*
 * sender/sender.ino  —  视觉电控通信协议 · 发送端（ESP32）
 * 严格按仓库根目录 README.md（v0.9 草案）实现，改协议前请先和接收端确认。
 *
 * A 板（发送端 / 视觉侧）：算出目标 -> 组帧 -> 从 UART 单向发给 B 板（接收端）。
 *
 * 帧格式（README 第 3、4、5 节）：
 *   $<x>,<y>,<conf>,<valid>,<ts>,<seq>*<XX>\r\n
 *     - x,y    : int16，毫米，允许负值（原点由视觉侧定义，推荐场地中心，x 向右 y 向上）
 *     - conf   : uint8，0~100，置信度百分比
 *     - valid  : uint8，0/1，本帧是否含有效目标
 *     - ts     : uint32，发送端 millis() 毫秒时间戳
 *     - seq    : uint8，每帧 +1，255 后回绕到 0
 *     - XX     : 对 $ 与 * 之间全部字符逐字节异或，两位大写十六进制
 *
 * 物理层（README 第 2 节）：UART / 115200 / 8N1，单向 A 只发 B 只收。
 * 接线：A.TX -> B.RX，GND 必须共地。
 *   接收端监听 Serial2 的 RX=16，所以本端用 Serial2，TX=17 接过去（A.TX -> B.RX）。
 *   若对端接收引脚不是 16，改下面的 LINK_TX_PIN 即可。
 */

// ---------------- 可改配置 ----------------
#define LINK_SERIAL   Serial2
#define LINK_TX_PIN   17      // 接接收端的 RX（交叉）：A.TX -> B.RX
#define LINK_RX_PIN   16      // 单向发送，RX 仅占位（填一个存在的 GPIO 即可）
#define LINK_BAUD     115200
#define PERIOD_MS     20      // 50 Hz（每 20ms 一帧，README 第 6 节）
#define DEBUG_PRINT   1       // 1 = 同时把发出的帧打到 USB 串口监视器，方便看；0 = 关闭

uint8_t seq = 0;

// 组帧并发送：$x,y,conf,valid,ts,seq*XX\r\n
// 校验范围（两端必须一致）：仅 $ 与 * 之间的 payload，不含 $ * XX \r\n
void buildAndSend(int16_t x, int16_t y, uint8_t conf, uint8_t valid, uint32_t ts) {
  // 用 String 拼十进制 ASCII；负值会带 '-'，同样是 ASCII 字符，照常参与异或
  String payload = String(x) + "," + String(y) + "," + String(conf) + "," +
                   String(valid) + "," + String(ts) + "," + String(seq);

  // 对 payload 逐字节异或
  uint8_t xorv = 0;
  for (unsigned int i = 0; i < payload.length(); i++) {
    xorv ^= (uint8_t)payload[i];
  }

  LINK_SERIAL.print('$');
  LINK_SERIAL.print(payload);
  LINK_SERIAL.print('*');
  if (xorv < 16) LINK_SERIAL.print('0');   // 补零到两位
  LINK_SERIAL.print(xorv, HEX);            // Arduino 默认输出大写十六进制
  LINK_SERIAL.print("\r\n");

  if (DEBUG_PRINT) {                       // 同一帧打到 USB，方便在发送端串口监视器里肉眼看
    Serial.print('$'); Serial.print(payload); Serial.print('*');
    if (xorv < 16) Serial.print('0');
    Serial.print(xorv, HEX); Serial.print("\r\n");
  }

  seq = (seq + 1) & 0xFF;                  // 0~255 回绕
}

void setup() {
  Serial.begin(115200);                    // USB 调试口
  LINK_SERIAL.begin(LINK_BAUD, SERIAL_8N1, LINK_RX_PIN, LINK_TX_PIN);
  delay(300);
}

void loop() {
  uint32_t ts = millis();

  // ===== 数据源（真正作业时把这里换成你的视觉算法输出） =====
  // 演示：目标绕场地中心 (0,0) 做半径 500mm 的圆周运动（int16，含负值，顺带验证负数坐标）。
  // 每 6 秒丢 1 秒目标：valid=0，x/y/conf 置 0，但 ts/seq 照常递增
  //   （README 第 7 节：没有目标也照常发帧，不允许停发，否则接收端分不清掉线和无目标）。
  int16_t x = 0, y = 0;
  uint8_t conf = 0, valid = 0;
  if ((ts / 1000) % 6 == 5) {
    valid = 0; x = 0; y = 0; conf = 0;          // 无目标：照常发帧，valid=0
  } else {
    float a = ts * 0.0008f;
    x = (int16_t)(500 * cos(a));
    y = (int16_t)(500 * sin(a));
    conf = 95; valid = 1;
  }
  // ============================

  buildAndSend(x, y, conf, valid, ts);
  delay(PERIOD_MS);                              // 50 Hz
}
