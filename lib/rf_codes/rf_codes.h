#ifndef RF_CODES_H
#define RF_CODES_H

#include <Arduino.h>
#include <Preferences.h>

#define MAX_CODES 50
#define TX_REPEAT_COUNT_DEFAULT 2

struct RFCode
{
    String name;
    unsigned long code;
    unsigned int bitlength;
    unsigned int protocol;
    unsigned int pulseLength;
    bool enabled;
};

extern RFCode savedCodes[];
extern int currentCodeIndex;
extern int txRepeatCount;

void loadCodesFromFlash();
void saveCodeToFlash(int index);
void clearAllCodes();

#endif // RF_CODES_H
