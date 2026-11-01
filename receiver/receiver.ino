// receiver.ino —— 接收端 · 台阶 5：输出成 VOFA+ 能认的格式
//
// 前四个台阶把数据解析出来了，但输出的是给人看的调试文本。
// VOFA+ 不认那个，它要的是 FireWater 协议：
//
//     x:473,y:126,conf:87,valid:1\n
//
// 规则就三条：名字:数值、逗号分隔、一行一帧以 \n 结尾。
// VOFA+ 靠"名字"自动建通道，靠 \n 判断一帧结束。
//
// 为什么要有 DEBUG_PRINT 开关：
//   VOFA+ 的数据行必须是【纯数据】，混进一行 "BAD calc=..." 它就解析失败。
//   调试时打开看细节，接 VOFA+ 时改成 0，只留数据行。

#define DEBUG_PRINT 1        // 1 = 串口监视器调试模式    0 = VOFA+ 模式

#if DEBUG_PRINT
  #define DBG(...) Serial.printf(__VA_ARGS__)
#else
  #define DBG(...) ((void)0)
#endif

enum State { WAIT_HEAD, IN_PAYLOAD, IN_CHECKSUM };

const int BUF_SIZE = 64;

char buf[BUF_SIZE];
uint8_t len = 0;
char ck[3];
uint8_t ckLen = 0;

State state = WAIT_HEAD;

struct Target {
  uint16_t x;
  uint16_t y;
  uint8_t  conf;
  uint8_t  valid;
  uint32_t ts;
  uint8_t  seq;
};

Target t;

uint8_t calcXor(const char *s, int n) {
  uint8_t x = 0;
  for (int i = 0; i < n; i++) x ^= (uint8_t)s[i];
  return x;
}

void setup() {
  Serial.begin(115200);
  delay(300);
  DBG("\n=== receiver ready (step 5) ===\n");
}

void loop() {
  while (Serial.available() > 0) {
    char c = Serial.read();

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
          DBG("!! overflow, drop frame\n");
          state = WAIT_HEAD;
          len = 0;
        }
        break;

      case IN_CHECKSUM:
        ck[ckLen++] = c;
        if (ckLen == 2) {
          ck[2] = '\0';
          buf[len] = '\0';

          uint8_t expected = calcXor(buf, len);
          uint8_t got = (uint8_t)strtoul(ck, NULL, 16);

          if (expected == got) {
            int n = sscanf(buf, "%hu,%hu,%hhu,%hhu,%u,%hhu",
                           &t.x, &t.y, &t.conf, &t.valid, &t.ts, &t.seq);

            if (n == 6) {
              // ---- 这一行就是给 VOFA+ 的，格式不能动 ----
              Serial.printf("x:%u,y:%u,conf:%u,valid:%u\n",
                            t.x, t.y, t.conf, t.valid);

              DBG("  parsed: ts=%u seq=%u\n", t.ts, t.seq);
            } else {
              DBG("BAD  fields=%d (expect 6)  payload: %s\n", n, buf);
            }
          } else {
            DBG("BAD  calc=%02X got=%s  payload: %s\n", expected, ck, buf);
          }
          state = WAIT_HEAD;
        }
        break;
    }
  }
}
