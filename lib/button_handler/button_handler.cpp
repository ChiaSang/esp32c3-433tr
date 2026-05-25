#include "button_handler.h"

OneButton button = OneButton(BTN_PIN, true, true);
OneButton bootButton = OneButton(BOOT_PIN, true, true);
LedControl led1(LED1_PIN);
LedControl led2(LED2_PIN);

volatile bool btnClickFlag = false;
volatile bool btnLongPressFlag = false;
volatile bool bootLongPressFlag = false;

void onSingleClick()
{
    btnClickFlag = true;
}

void onLongPress()
{
    btnLongPressFlag = true;
}

void onBootLongPress()
{
    bootLongPressFlag = true;
    led1.blink(150);
    led2.blink(150);
}

void initButtons()
{
    pinMode(BTN_PIN, INPUT_PULLUP);
    button.attachClick(onSingleClick);
    button.attachLongPressStart(onLongPress);
    button.setPressMs(3000);
    button.setDebounceMs(50);

    pinMode(BOOT_PIN, INPUT_PULLUP);
    bootButton.attachLongPressStart(onBootLongPress);
    bootButton.setPressMs(5000);
    bootButton.setDebounceMs(50);
}
