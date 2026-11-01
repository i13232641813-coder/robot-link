// ============================================================
//  receiver.ino —— 双板通信 · 接收端（最终版 v1.0）
//  协议见同仓库 README.md v1.0
//
//  数据流：
//    发送端板子 --UART--> [Serial2] --> 本程序 --> [Serial/USB] --> VOFA+
//
//  接线（只发一个方向，所以最少两根线）：
//    发送端 TX  --->  接收端 GPIO16 (RX)      ← 必须交叉
//    发送端 GND ---  接收端 GND               ← 必须共地，漏了就是乱码
//    接收端 GPIO17 (TX) ---> 发送端 RX        ← 可选，本协议单向不需要
//
//  注意：ESP32-WROVER（带 PSRAM）的 GPIO16/17 被 PSRAM 占用，
//        如果是 WROVER 板子，把下面两个引脚改成 25/26。
// ============================================================

// ==================== 配置区（只改这里） ====================

#define DEBUG_PRINT   1     // 1 = 调试模式，串口监视器能看细节
                            // 0 = 正式模式，只输出 FireWater 数据行

#define TEST_WITH_USB 1     // 1 = 单板自测：从电脑串口监视器收帧
                            // 0 = 正式：从另一块板子收（Serial2）

#define LINK_RX_PIN   16    // 接收端 RX，接发送端的 TX
#define LINK_TX_PIN   17    // 接收端 TX，接发送端的 RX
#define LINK_BAUD     115200

#define LINK_TIMEOUT_MS 200 // 协议第 6 节：200ms 没收到有效帧算超时
#define BUF_SIZE      64    // 载荷上限，协议第 8 节

// ==================== 以下不用改 ====================

#if DEBUG_PRINT
  #define DBG(...) Serial.printf(__VA_ARGS__)
#else
  #define DBG(...) ((void)0)
#endif

enum State { WAIT_HEAD, IN_PAYLOAD, IN_CHECKSUM };

char    buf[BUF_SIZE];
uint8_t len   = 0;
char    ck[3];
uint8_t ckLen = 0;
State   state = WAIT_HEAD;

// 协议第 4 节的六个字段（x/y 用有符号，允许负坐标）
struct Target {
  int16_t  x;
  int16_t  y;
  uint8_t  conf;
  uint8_t  valid;
  uint32_t ts;
  uint8_t  seq;
};

Target t;

Stream* link = nullptr;              // 数据来源，setup() 里决定

unsigned long lastGoodMs  = 0;       // 最近一次收到有效帧的时刻
bool          linkAlive   = false;
unsigned long rxBytes     = 0;       // 一共收到多少字节（排障用）
unsigned long goodFrames  = 0;
unsigned long badFrames   = 0;

// 对 buf 前 n 个字节逐字节异或（协议第 5 节）
uint8_t calcXor(const char *s, int n) {
  uint8_t v = 0;
  for (int i = 0; i < n; i++) v ^= (uint8_t)s[i];
  return v;
}

// 一帧收齐了，校验 + 拆分 + 输出
void handleFrame() {
  buf[len] = '\0';

  uint8_t expected = calcXor(buf, len);
  uint8_t got      = (uint8_t)strtoul(ck, NULL, 16);

  if (expected != got) {
    badFrames++;
    DBG("BAD  checksum calc=%02X got=%s  payload: %s\n", expected, ck, buf);
    return;
  }

  int n = sscanf(buf, "%hd,%hd,%hhu,%hhu,%u,%hhu",
                 &t.x, &t.y, &t.conf, &t.valid, &t.ts, &t.seq);
  if (n != 6) {
    badFrames++;
    DBG("BAD  fields=%d (expect 6)  payload: %s\n", n, buf);
    return;
  }

  goodFrames++;
  lastGoodMs = millis();

  if (!linkAlive) {
    linkAlive = true;
    DBG("[link] up\n");
  }

  // ---- 唯一给 VOFA+ 的数据行，格式不能动 ----
  Serial.printf("x:%d,y:%d,conf:%u,valid:%u\n", t.x, t.y, t.conf, t.valid);

  DBG("  parsed: ts=%u seq=%u\n", t.ts, t.seq);
}

// 逐字节推进状态机
void feed(char c) {
  switch (state) {

    case WAIT_HEAD:
      if (c == '$') {
        state = IN_PAYLOAD;
        len = 0;
      }
      break;

    case IN_PAYLOAD:
      if (c == '*') {
        state = IN_CHECKSUM;
        ckLen = 0;
      } else if (len < BUF_SIZE - 1) {
        buf[len++] = c;
      } else {
        badFrames++;
        DBG("BAD  overflow, drop frame\n");
        state = WAIT_HEAD;
        len = 0;
      }
      break;

    case IN_CHECKSUM:
      ck[ckLen++] = c;
      if (ckLen == 2) {
        ck[2] = '\0';
        handleFrame();
        state = WAIT_HEAD;
      }
      break;
  }
}

// 超时检测：只在状态翻转那一刻输出一次，不刷屏
void checkTimeout() {
  if (linkAlive && millis() - lastGoodMs > LINK_TIMEOUT_MS) {
    linkAlive = false;
    DBG("!! link timeout (%lu ms)\n", millis() - lastGoodMs);
    // 保持上一次坐标，但明确告诉上层这帧无效
    Serial.printf("x:%d,y:%d,conf:0,valid:0\n", t.x, t.y);
  }
}

// 每秒报一次收发统计，只在有新字节时才打
// rx  = 收到的原始字节数（接线通不通看这个）
// good/bad = 解析结果，两者都是 0 而 rx 在涨，说明字节进来了但成不了帧
void reportStats() {
#if DEBUG_PRINT
  static unsigned long lastReport = 0;
  static unsigned long lastRx     = 0;
  if (rxBytes != lastRx && millis() - lastReport >= 1000) {
    lastReport = millis();
    lastRx     = rxBytes;
    DBG("[stat] rx=%lu  good=%lu  bad=%lu\n", rxBytes, goodFrames, badFrames);
  }
#endif
}

void setup() {
  Serial.begin(115200);
  delay(300);
  DBG("\n=== receiver ready (final v1.0) ===\n");

#if TEST_WITH_USB
  link = &Serial;
  DBG("[cfg] 数据源 = USB 串口（单板自测模式）\n");
#else
  Serial2.begin(LINK_BAUD, SERIAL_8N1, LINK_RX_PIN, LINK_TX_PIN);
  link = &Serial2;
  DBG("[cfg] 数据源 = Serial2  RX=%d TX=%d  %d bps\n",
      LINK_RX_PIN, LINK_TX_PIN, LINK_BAUD);
#endif

  DBG("[cfg] 超时阈值 %d ms\n", LINK_TIMEOUT_MS);
}

void loop() {
  while (link->available() > 0) {
    rxBytes++;
    feed((char)link->read());
  }
  checkTimeout();
  reportStats();
}
