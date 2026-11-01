// receiver.ino —— 接收端 · 台阶 3：校验，拒绝坏帧
//
// 台阶 2 能找到边界了，但那时候不管数据对不对，收到就打。
// 这一台阶开始回答：这帧数据是真的，还是半路被干扰坏的？
//
// 做法：多一个状态。看见 '*' 之后不是马上完事，还要再收两个字符（校验值），
//       然后把载荷逐字节异或，跟收到的比对。一致才认，不一致整帧丢掉。
//
// 一个必须记住的细节：
//   校验值 20 在串口上是【两个字符】'2' 和 '0'（0x32 0x30），
//   不是一个字节 0x20。所以要用 strtoul 把字符串转成数值再比。
//   直接比字符串的话，'2a' 和 '2A' 会被判成不一样，其实数值都是 0x2A。

enum State { WAIT_HEAD, IN_PAYLOAD, IN_CHECKSUM };

const int BUF_SIZE = 64;        // 载荷最长 64 字节，协议第 8 节定的

char buf[BUF_SIZE];
uint8_t len = 0;

char ck[3];                     // 两位校验字符 + 结尾的 '\0'
uint8_t ckLen = 0;

State state = WAIT_HEAD;

// 对 buf 里前 n 个字节逐字节异或 —— 这就是协议第 5 节的算法
uint8_t calcXor(const char *s, int n) {
  uint8_t x = 0;
  for (int i = 0; i < n; i++) x ^= (uint8_t)s[i];
  return x;
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n=== receiver ready (step 3) ===");
}

void loop() {
  while (Serial.available() > 0) {
    char c = Serial.read();

    switch (state) {

      // 状态一：等帧头，不是 '$' 的字节全部丢掉
      case WAIT_HEAD:
        if (c == '$') {
          state = IN_PAYLOAD;
          len = 0;
        }
        break;

      // 状态二：攒载荷，直到看见 '*'
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

      // 状态三：收两位校验，凑齐就比对
      case IN_CHECKSUM:
        ck[ckLen++] = c;
        if (ckLen == 2) {
          ck[2] = '\0';
          buf[len] = '\0';

          uint8_t expected = calcXor(buf, len);          // 自己算的
          uint8_t got = (uint8_t)strtoul(ck, NULL, 16);  // 收到的（按十六进制解析）

          if (expected == got) {
            Serial.printf("OK   payload[%d]: %s\n", len, buf);
            // 台阶 4 会在这里把 buf 拆成六个数字
          } else {
            Serial.printf("BAD  calc=%02X got=%s  payload: %s\n", expected, ck, buf);
            // 坏帧只报告，不回复、不要求重传 —— 协议第 8 节
          }
          state = WAIT_HEAD;
        }
        break;
    }
  }
}
