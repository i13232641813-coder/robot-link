// receiver.ino —— 接收端 · 台阶 4：把载荷拆成六个数字
//
// 前三台阶解决了"字节怎么进来、帧在哪、数据是不是真的"。
// 这一台阶把字符串 "473,126,87,1,12345,42" 变成六个能直接用的数字。
//
// 用的工具是 sscanf。它按格式串逐个匹配，返回【成功解析了几个】。
// 这个返回值必须检查 —— 少一个字段它不会报错，只是少填一个变量，
// 而没被填的变量里是内存里的随机值，后面的逻辑就全乱了。
//
// 格式符对照（ESP32 上）：
//   uint16_t → %hu      uint8_t → %hhu      uint32_t → %u
//   （如果以后换到 AVR 板子，uint32_t 要改成 %lu，这是平台差异）

enum State { WAIT_HEAD, IN_PAYLOAD, IN_CHECKSUM };

const int BUF_SIZE = 64;

char buf[BUF_SIZE];
uint8_t len = 0;
char ck[3];
uint8_t ckLen = 0;

State state = WAIT_HEAD;

// 解析结果放这里，六个字段和协议第 4 节一一对应
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
  Serial.println("\n=== receiver ready (step 4) ===");
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
          Serial.println("!! overflow, drop frame");
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
            // ---- 台阶 4 新增的部分 ----
            int n = sscanf(buf, "%hu,%hu,%hhu,%hhu,%u,%hhu",
                           &t.x, &t.y, &t.conf, &t.valid, &t.ts, &t.seq);
            if (n == 6) {
              Serial.printf("x=%u  y=%u  conf=%u  valid=%u  ts=%u  seq=%u\n",
                            t.x, t.y, t.conf, t.valid, t.ts, t.seq);
              // 台阶 5 会在这里把 t 转成 VOFA+ 能认的格式
            } else {
              Serial.printf("BAD  fields=%d (expect 6)  payload: %s\n", n, buf);
            }
          } else {
            Serial.printf("BAD  calc=%02X got=%s  payload: %s\n", expected, ck, buf);
          }
          state = WAIT_HEAD;
        }
        break;
    }
  }
}
