#ifndef LED_CONTROL_H
#define LED_CONTROL_H

#include <Arduino.h>
#include <jled.h>

/**
 * LED 控制库
 * 基于 JLed 封装，支持开/关/反转/闪烁/呼吸功能
 */
class LedControl
{
public:
    /**
     * @param pin LED 引脚号
     * @param activeLow 是否低电平点亮（默认 false）
     */
    LedControl(uint8_t pin, bool activeLow = false);

    ~LedControl();

    /** 开灯（最大亮度） */
    void on();

    /** 关灯 */
    void off();

    /** 反转当前状态 */
    void toggle();

    /**
     * 闪烁模式
     * @param periodMs 闪烁周期（毫秒），即一次亮+灭的总时长
     * @param durationMs 闪烁持续时间（毫秒），0 表示持续闪烁
     */
    void blink(unsigned int periodMs = 500, unsigned long durationMs = 0);

    /**
     * 呼吸灯模式
     * @param periodMs 一个呼吸周期（毫秒），值越小呼吸越快
     * @param durationMs 呼吸持续时间（毫秒），0 表示持续呼吸
     */
    void breathe(unsigned int periodMs = 2000, unsigned long durationMs = 0);

    /** 停止当前效果 */
    void stop();

    /**
     * 必须在 loop() 中循环调用
     * @return true 表示效果仍在运行
     */
    bool update();

private:
    JLed *_led;
    uint8_t _pin;
    bool _activeLow;
    bool _state;       // 当前开/关状态
    uint8_t _onValue;  // 点亮时的电平
    uint8_t _offValue; // 熄灭时的电平
};

#endif // LED_CONTROL_H
