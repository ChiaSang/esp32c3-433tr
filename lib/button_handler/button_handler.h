#ifndef BUTTON_HANDLER_H
#define BUTTON_HANDLER_H

#include <Arduino.h>
#include <OneButton.h>
#include <LedControl.h>

#define BTN_PIN 10
#define BOOT_PIN 9
#define LED1_PIN 12
#define LED2_PIN 13

extern OneButton button;
extern OneButton bootButton;
extern LedControl led1;
extern LedControl led2;

extern volatile bool btnClickFlag;
extern volatile bool btnLongPressFlag;
extern volatile bool bootLongPressFlag;

void initButtons();

#endif // BUTTON_HANDLER_H
