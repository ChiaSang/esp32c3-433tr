#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <RCSwitch.h>
#include <OneButton.h>
#include <LedControl.h>

#define TX_PIN 7  // 发射引脚
#define RX_PIN 6  // 接收引脚
#define BTN_PIN 10 // 按钮引脚
#define BOOT_PIN 9 // 按钮引脚
#define LED1_PIN 12
#define LED2_PIN 13

// WiFiManager 实例
WiFiManager wifiManager;

RCSwitch txSwitch = RCSwitch();
RCSwitch rxSwitch = RCSwitch();
OneButton button = OneButton(BTN_PIN, true, true); // 低电平有效，启用内部上拉
OneButton bootButton = OneButton(BOOT_PIN, true, true);
LedControl led1(LED1_PIN);
LedControl led2(LED2_PIN);

unsigned long lastSendTime = 0;
const unsigned long SEND_INTERVAL = 3000; // 每3秒发送一次
unsigned long sendCode = 5393;            // 要发送的编码

volatile bool learningMode = false;     // 是否处于学习模式
volatile bool btnClickFlag = false;     // 按钮单击标志（中断中置位）
volatile bool btnLongPressFlag = false; // 按钮长按标志
volatile bool bootLongPressFlag = false; // BOOT 按钮长按标志（重置 WiFi）

// 学习到的信号参数
unsigned long learnedCode = 0;
unsigned int learnedBitlength = 24;
unsigned int learnedProtocol = 1;
unsigned int learnedPulseLength = 320;

// 按钮单击回调（发射信号）
void onSingleClick()
{
    btnClickFlag = true;
}

// 按钮长按回调（进入学习模式）
void onLongPress()
{
    btnLongPressFlag = true;
}

// BOOT 按钮长按回调（重置 WiFi）
void onBootLongPress()
{
    bootLongPressFlag = true;
}

void setup()
{
    Serial.begin(115200);
    delay(1000); // 等待串口稳定

    Serial.println("\n\n=== ESP32-C3 433MHz 收发器启动 ===");

    // WiFi 配网初始化
    // 设置 WiFiManager 超时时间（秒）
    wifiManager.setConfigPortalTimeout(180); // 3分钟超时

    // 自定义配网页面标题
    wifiManager.setTitle("ESP32-C3 433MHz");

    // 尝试自动连接，如果失败则启动配网门户
    // AP名称: ESP32-C3-433TR
    // AP密码: 12345678
    Serial.println("正在连接 WiFi...");

    if (!wifiManager.autoConnect("ESP32-C3-433TR", "12345678"))
    {
        Serial.println("WiFi 连接失败，重启设备...");
        delay(3000);
        ESP.restart();
    }

    // WiFi 连接成功
    Serial.println("WiFi 连接成功！");
    Serial.print("IP 地址: ");
    Serial.println(WiFi.localIP());
    Serial.print("信号强度: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");

    // 发射初始化
    txSwitch.enableTransmit(TX_PIN);
    txSwitch.setProtocol(1);
    txSwitch.setPulseLength(320);
    txSwitch.setRepeatTransmit(3);

    // 接收初始化
    rxSwitch.enableReceive(RX_PIN);

    // 按钮初始化（BOOT 按键在 GPIO9，需显式设置上拉）
    pinMode(BTN_PIN, INPUT_PULLUP);
    button.attachClick(onSingleClick);
    button.attachLongPressStart(onLongPress);
    button.setPressMs(3000); // 长按3秒触发
    button.setDebounceMs(50);

    // BOOT 按钮初始化
    pinMode(BOOT_PIN, INPUT_PULLUP);
    bootButton.attachLongPressStart(onBootLongPress);
    bootButton.setPressMs(5000); // 长按5秒触发
    bootButton.setDebounceMs(50);

    Serial.println("\n433MHz 收发 Demo 已启动（支持信号学习）");
    Serial.printf("TX Pin: GPIO%d, RX Pin: GPIO%d, BTN Pin: GPIO%d\n", TX_PIN, RX_PIN, BTN_PIN);
    Serial.println("单击 [GPIO9] 发射信号，长按3秒进入学习模式");
    Serial.println("=====================================\n");
}

void loop()
{
    button.tick();    // 处理按钮事件
    bootButton.tick(); // 处理 BOOT 按钮事件
    led1.update();
    led2.update();

    // 处理 BOOT 长按（重置 WiFi 并重启）
    if (bootLongPressFlag)
    {
        bootLongPressFlag = false;
        Serial.println(">>> 长按 BOOT 5秒，正在重置 WiFi 设置...");
        led2.blink(100);
        wifiManager.resetSettings();
        Serial.println(">>> WiFi 设置已清除，正在重启...");
        delay(1000);
        ESP.restart();
    }

    // 处理长按（任何时候都可进入/退出学习模式）
    if (btnLongPressFlag)
    {
        btnLongPressFlag = false;
        learningMode = !learningMode;
        if (learningMode)
        {
            Serial.println(">>> 已进入学习模式，请发射信号...");
            Serial.println(">>> 再次长按3秒可退出学习模式");
            led1.blink(200); // 快闪，200ms 周期
        }
        else
        {
            Serial.println(">>> 已退出学习模式");
            led1.off();
        }
    }

    // 处理单击（正常模式下发射信号）
    if (btnClickFlag)
    {
        btnClickFlag = false;
        if (!learningMode)
        {
            Serial.printf(">> 单击发射编码: %lu (协议:%d 脉冲:%d 位长:%d)\n",
                          sendCode, learnedProtocol, learnedPulseLength, learnedBitlength);
            txSwitch.send(sendCode, learnedBitlength);
        }
    }

    unsigned long now = millis();

    if (learningMode)
    {
        // 学习模式：仅监听接收信号
        if (rxSwitch.available())
        {
            unsigned long value = rxSwitch.getReceivedValue();

            if (value == 0)
            {
                Serial.println("!! 接收到未知编码，请重试");
            }
            else
            {
                learnedCode = value;
                learnedBitlength = rxSwitch.getReceivedBitlength();
                learnedProtocol = rxSwitch.getReceivedProtocol();
                learnedPulseLength = rxSwitch.getReceivedDelay();

                sendCode = learnedCode;
                txSwitch.setProtocol(learnedProtocol);
                txSwitch.setPulseLength(learnedPulseLength);

                Serial.printf("+++ 学习成功！\n");
                Serial.printf("    编码: %lu, 位长: %d, 协议: %d, 脉冲: %d\n",
                              learnedCode, learnedBitlength, learnedProtocol, learnedPulseLength);

                learningMode = false;
                led1.off();
                Serial.println(">>> 已自动退出学习模式，将定时发送学习到的编码");
            }
            rxSwitch.resetAvailable();
        }
    }
    else
    {
        // 正常模式：定时发送
        if (now - lastSendTime >= SEND_INTERVAL)
        {
            lastSendTime = now;
            Serial.printf(">> 定时发送编码: %lu (协议:%d 脉冲:%d 位长:%d)\n",
                          sendCode, learnedProtocol, learnedPulseLength, learnedBitlength);
            txSwitch.send(sendCode, learnedBitlength);
        }

        // 显示接收到的信号
        if (rxSwitch.available())
        {
            unsigned long value = rxSwitch.getReceivedValue();

            if (value == 0)
            {
                Serial.println("!! 接收到未知编码");
            }
            else
            {
                Serial.printf("<< 接收编码: %lu, 位长: %d, 协议: %d\n",
                              value,
                              rxSwitch.getReceivedBitlength(),
                              rxSwitch.getReceivedProtocol());
            }
            rxSwitch.resetAvailable();
        }
    }
}