#include "LedControl.h"

LedControl::LedControl(uint8_t pin, bool activeLow)
    : _pin(pin), _activeLow(activeLow), _state(false)
{
    _onValue = _activeLow ? 0 : 255;
    _offValue = _activeLow ? 255 : 0;
    _led = new JLed(pin);
}

LedControl::~LedControl()
{
    stop();
    delete _led;
}

void LedControl::on()
{
    _state = true;
    *_led = JLed(_pin).On();
}

void LedControl::off()
{
    _state = false;
    *_led = JLed(_pin).Off();
}

void LedControl::toggle()
{
    if (_state)
    {
        off();
    }
    else
    {
        on();
    }
}

void LedControl::blink(unsigned int periodMs, unsigned long durationMs)
{
    _state = true;
    uint16_t halfPeriod = periodMs / 2;
    if (halfPeriod < 1)
        halfPeriod = 1;

    if (durationMs > 0)
    {
        unsigned int cycles = durationMs / periodMs;
        if (cycles < 1)
            cycles = 1;
        *_led = JLed(_pin).Blink(halfPeriod, halfPeriod).Repeat(cycles).DelayBefore(0).DelayAfter(0);
    }
    else
    {
        *_led = JLed(_pin).Blink(halfPeriod, halfPeriod).Forever().DelayBefore(0).DelayAfter(0);
    }
}

void LedControl::breathe(unsigned int periodMs, unsigned long durationMs)
{
    _state = true;

    if (durationMs > 0)
    {
        unsigned int cycles = durationMs / periodMs;
        if (cycles < 1)
            cycles = 1;
        *_led = JLed(_pin).Breathe(periodMs).Repeat(cycles).DelayBefore(0).DelayAfter(0);
    }
    else
    {
        *_led = JLed(_pin).Breathe(periodMs).Forever().DelayBefore(0).DelayAfter(0);
    }
}

void LedControl::stop()
{
    _led->Stop(JLed::eStopMode::KEEP_CURRENT);
}

bool LedControl::update()
{
    return _led->Update();
}
