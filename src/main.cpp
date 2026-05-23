#include <Arduino.h>
#include <RCSwitch.h>

#define TX_PIN 7   // 发射引脚
#define RX_PIN 6   // 接收引脚

RCSwitch txSwitch = RCSwitch();
RCSwitch rxSwitch = RCSwitch();

unsigned long lastSendTime = 0;
const unsigned long SEND_INTERVAL = 3000; // 每3秒发送一次
unsigned long sendCode = 5393;            // 要发送的编码

void setup() {
    Serial.begin(115200);

    // 发射初始化
    txSwitch.enableTransmit(TX_PIN);
    txSwitch.setProtocol(1);
    txSwitch.setPulseLength(320);
    txSwitch.setRepeatTransmit(3);

    // 接收初始化
    rxSwitch.enableReceive(RX_PIN);

    Serial.println("433MHz 收发 Demo 已启动");
    Serial.printf("TX Pin: GPIO%d, RX Pin: GPIO%d\n", TX_PIN, RX_PIN);
}

void loop() {
    unsigned long now = millis();

    // 定时发送
    if (now - lastSendTime >= SEND_INTERVAL) {
        lastSendTime = now;
        Serial.printf(">> 发送编码: %lu\n", sendCode);
        txSwitch.send(sendCode, 24);
    }

    // 检查是否收到信号
    if (rxSwitch.available()) {
        unsigned long value = rxSwitch.getReceivedValue();

        if (value == 0) {
            Serial.println("!! 接收到未知编码");
        } else {
            Serial.printf("<< 接收编码: %lu, 位长: %d, 协议: %d\n",
                          value,
                          rxSwitch.getReceivedBitlength(),
                          rxSwitch.getReceivedProtocol());
        }
        rxSwitch.resetAvailable();
    }
}