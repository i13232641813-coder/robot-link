// receiver.ino —— 接收端 · 台阶 2：找到帧的边界
//
// 台阶 1 证明了"字节能进来"，但那时候所有字节都一视同仁。
// 这一台阶要回答一个问题：这一堆字节里，哪一段才是一帧？
//
// 做法：一个两状态的状态机。
//   inPayload = false  —— 我在等帧头，不是 '$' 的字节全部丢掉
//   inPayload = true   —— 我看到 '$' 了，开始把字节攒进 buf，直到看见 '*'
//
// 为什么不能"等一整行再解析"？
//   串口是字节流，你不知道一帧什么时候到齐。万一中途丢了一个字节
//   （干扰、板子复位、电脑卡了一下），整行就会错位，而且你不会发现。
//   用 '$' 当同步点，接收端随时能重新对齐 —— 这叫自恢复能力。
//
// 这一台阶还不校验，所以坏帧也会被打印出来。这是故意的，校验是台阶 3 的事。

const int BUF_SIZE = 64;        // 载荷最长 64 字节，协议第 8 节定的

char buf[BUF_SIZE];
uint8_t len = 0;
bool inPayload = false;         // false = 等帧头，true = 正在攒载荷

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n=== receiver ready (step 2) ===");
}

void loop() {
  while (Serial.available() > 0) {
    char c = Serial.read();

    if (!inPayload) {
      // 状态一：等帧头。只有 '$' 能让我进入下一状态
      if (c == '$') {
        inPayload = true;
        len = 0;              // 重新开始，清掉上一帧的残留
      }
      // 其他字符一律无视 —— 这就是"丢弃垃圾数据"
    } else {
      // 状态二：正在攒载荷
      if (c == '*') {
        buf[len] = '\0';      // C 字符串必须以 0 结尾，否则 printf 会读飞
        Serial.printf("payload[%d]: %s\n", len, buf);
        inPayload = false;    // 这一帧结束，回去等下一个 '$'
      } else if (len < BUF_SIZE - 1) {
        buf[len++] = c;       // 存一个字节，计数器 +1
      } else {
        // 攒了 63 个字节还没见到 '*'，判定为垃圾数据，放弃这一帧
        Serial.println("!! overflow, drop frame");
        inPayload = false;
        len = 0;
      }
    }
  }
}
