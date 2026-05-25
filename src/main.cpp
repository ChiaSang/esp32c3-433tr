#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <RCSwitch.h>
#include "rf_codes.h"
#include "button_handler.h"
#include "web_server.h"

#define TX_PIN 7 // 发射引脚
#define RX_PIN 6 // 接收引脚

const unsigned long LEARNING_TIMEOUT = 60000; // 学习模式超时时间（60秒）

RCSwitch txSwitch = RCSwitch();
RCSwitch rxSwitch = RCSwitch();

// WiFiManager 实例
WiFiManager wifiManager;

void setup()
{
    Serial.begin(115200);
    delay(1000); // 等待串口稳定

    Serial.println("\n\n=== ESP32-C3 433MHz 收发器启动 ===");

    // 加载保存的编码
    loadCodesFromFlash();

    // WiFi 配网初始化
    wifiManager.setConfigPortalTimeout(180); // 3分钟超时
    wifiManager.setTitle("ESP32-C3 433MHz");

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

    // 启动 Web 服务器
    setupWebServer();

    // 发射初始化
    txSwitch.enableTransmit(TX_PIN);
    txSwitch.setProtocol(1);
    txSwitch.setPulseLength(320);
    txSwitch.setRepeatTransmit(3);

    // 接收初始化
    rxSwitch.enableReceive(RX_PIN);

    // 按钮初始化
    initButtons();

    Serial.println("\n433MHz 收发器已启动");
    Serial.printf("Web 界面: http://%s\n", WiFi.localIP().toString().c_str());
    Serial.printf("TX Pin: GPIO%d, RX Pin: GPIO%d, BTN Pin: GPIO%d\n", TX_PIN, RX_PIN, BTN_PIN);
    Serial.println("=====================================\n");
}

void loop()
{
    button.tick();     // 处理按钮事件
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
            learningModeStartTime = millis(); // 记录开始时间
            Serial.println(">>> 已进入学习模式，请发射信号...");
            Serial.println(">>> 60秒后自动退出，或再次长按3秒手动退出");
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
        if (!learningMode && savedCodes[currentCodeIndex].enabled)
        {
            RFCode &code = savedCodes[currentCodeIndex];
            Serial.printf(">> 单击发射编码: %lu (协议:%d 脉冲:%d 位长:%d)\n",
                          code.code, code.protocol, code.pulseLength, code.bitlength);
            txSwitch.setProtocol(code.protocol);
            txSwitch.setPulseLength(code.pulseLength);
            for (int r = 0; r < txRepeatCount; r++)
            {
                if (r > 0)
                    delay(50);
                txSwitch.send(code.code, code.bitlength);
            }
        }
    }

    unsigned long now = millis();

    if (learningMode)
    {
        // 检查学习模式超时
        if (now - learningModeStartTime >= LEARNING_TIMEOUT)
        {
            learningMode = false;
            led1.off();
            Serial.println(">>> 学习模式超时（5秒），已自动退出");
        }

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
                // 找到第一个空闲位置保存
                int saveIndex = -1;
                for (int i = 0; i < MAX_CODES; i++)
                {
                    if (!savedCodes[i].enabled)
                    {
                        saveIndex = i;
                        break;
                    }
                }

                if (saveIndex >= 0)
                {
                    savedCodes[saveIndex].name = "Code_" + String(saveIndex + 1);
                    savedCodes[saveIndex].code = value;
                    savedCodes[saveIndex].bitlength = rxSwitch.getReceivedBitlength();
                    savedCodes[saveIndex].protocol = rxSwitch.getReceivedProtocol();
                    savedCodes[saveIndex].pulseLength = rxSwitch.getReceivedDelay();
                    savedCodes[saveIndex].enabled = true;

                    saveCodeToFlash(saveIndex);
                    currentCodeIndex = saveIndex;

                    Serial.printf("+++ 学习成功！已保存到位置 #%d\n", saveIndex);
                    Serial.printf("    编码: %lu, 位长: %d, 协议: %d, 脉冲: %d\n",
                                  savedCodes[saveIndex].code, savedCodes[saveIndex].bitlength,
                                  savedCodes[saveIndex].protocol, savedCodes[saveIndex].pulseLength);
                }
                else
                {
                    Serial.println("!! 存储已满，请先删除一些编码");
                }

                learningMode = false;
                led1.off();
                Serial.println(">>> 已自动退出学习模式");
            }
            rxSwitch.resetAvailable();
        }
    }
    else
    {
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