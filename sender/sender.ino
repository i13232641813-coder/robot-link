/*
 * sender/sender.ino — 视觉电控通信协议 · A板发送端 ESP32
 * 协议版本 v0.9，依据仓库README.md实现
 * 帧格式：$<x>,<y>,<conf>,<valid>,<ts>,<seq>*<XX>\r\n
 * A板TX(GPIO17) → B板RX；GND必须共地
 */

// ------------配置区，和README文档保持一致------------
#define LINK_SERIAL     Serial2
#define LINK_TX_PIN     17      // A板TX引脚，接接收板B的RX
#define LINK_RX_PIN     16      // 单向发送，RX仅占位
#define LINK_BAUD       115200
#define PERIOD_MS       20      // 50Hz发送周期

uint8_t seq = 0;   //帧序号，0~255循环

/**
 * @brief 组帧并且通过LINK_SERIAL发送，严格遵守协议
 * @param x int16 横坐标mm
 * @param y int16 纵坐标mm
 * @param conf uint8 置信度0~100
 * @param valid uint8 0无目标 /1有目标
 * @param ts uint32 millis()发送端毫秒时间戳
 */
void buildAndSend(int16_t x, int16_t y, uint8_t conf, uint8_t valid, uint32_t ts)
{
    // payload：$ 和 * 中间全部内容，6个逗号分隔字段
    String payload = String(x) + ","
                   + String(y) + ","
                   + String(conf) + ","
                   + String(valid) + ","
                   + String(ts) + ","
                   + String(seq);

    // 异或校验：只对payload全部字节运算，不含$ * \r\n
    uint8_t xorCheck = 0;
    for (size_t i = 0; i < payload.length(); i++)
    {
        xorCheck ^= (uint8_t)payload.charAt(i);
    }

    // 输出完整协议帧 $payload*XX\r\n
    LINK_SERIAL.print('$');
    LINK_SERIAL.print(payload);
    LINK_SERIAL.print('*');
    // 输出两位大写十六进制，小于0x10前面补'0'
    if(xorCheck < 0x10){
        LINK_SERIAL.print('0');
    }
    LINK_SERIAL.print(xorCheck, HEX);
    LINK_SERIAL.print("\r\n");  //文档规定行结束符\r\n

    // =========调试选项：开启后USB Serial输出裸CSV给VOFA调试（正式双板联调不依赖此段）=========
    // Serial.printf("%d,%d,%d,%d,%lu,%u\n", x, y, conf, valid, ts, seq);

    seq = (seq + 1) & 0xFF;  // 0‑255回绕
}

void setup()
{
    //板间通信串口
    LINK_SERIAL.begin(LINK_BAUD, SERIAL_8N1, LINK_RX_PIN, LINK_TX_PIN);
    // 如需VOFA直接看发送端数据，取消下面这行注释
    // Serial.begin(115200);
}

void loop()
{
    uint32_t ts = millis();
    int16_t x = 0, y = 0;
    uint8_t conf = 0, valid = 0;

    //模拟数据源：每6秒里面有1秒 valid=0（无目标，照常发帧，禁止停发）
    if ((ts / 1000UL) % 6UL == 5UL)
    {
        valid = 0;
        x = 0;
        y = 0;
        conf = 0;
    }
    else
    {
        float angle = ts * 0.0008f;
        x = (int16_t)(500 * cos(angle));
        y = (int16_t)(500 * sin(angle));
        conf = 95;
        valid = 1;
    }

    buildAndSend(x, y, conf, valid, ts);
    delay(PERIOD_MS);
}


