#include "rf_codes.h"

RFCode savedCodes[MAX_CODES];
int currentCodeIndex = 0;
int txRepeatCount = TX_REPEAT_COUNT_DEFAULT;

void loadCodesFromFlash()
{
    Preferences preferences;
    preferences.begin("rf-codes", true); // 只读模式
    for (int i = 0; i < MAX_CODES; i++)
    {
        String prefix = "code" + String(i) + "_";
        savedCodes[i].name = preferences.getString((prefix + "name").c_str(), "");
        savedCodes[i].code = preferences.getULong((prefix + "code").c_str(), 0);
        savedCodes[i].bitlength = preferences.getUInt((prefix + "bits").c_str(), 24);
        savedCodes[i].protocol = preferences.getUInt((prefix + "proto").c_str(), 1);
        savedCodes[i].pulseLength = preferences.getUInt((prefix + "pulse").c_str(), 320);
        savedCodes[i].enabled = preferences.getBool((prefix + "en").c_str(), false);
    }
    preferences.end();
    // 加载发射重复次数
    preferences.begin("rf-codes", true);
    txRepeatCount = preferences.getInt("txRepeat", TX_REPEAT_COUNT_DEFAULT);
    preferences.end();
    Serial.println("已从 Flash 加载编码数据");
}

void saveCodeToFlash(int index)
{
    if (index < 0 || index >= MAX_CODES)
        return;
    Preferences preferences;
    preferences.begin("rf-codes", false); // 读写模式
    String prefix = "code" + String(index) + "_";
    preferences.putString((prefix + "name").c_str(), savedCodes[index].name);
    preferences.putULong((prefix + "code").c_str(), savedCodes[index].code);
    preferences.putUInt((prefix + "bits").c_str(), savedCodes[index].bitlength);
    preferences.putUInt((prefix + "proto").c_str(), savedCodes[index].protocol);
    preferences.putUInt((prefix + "pulse").c_str(), savedCodes[index].pulseLength);
    preferences.putBool((prefix + "en").c_str(), savedCodes[index].enabled);
    preferences.end();
    Serial.printf("已保存编码 #%d 到 Flash\n", index);
}

void clearAllCodes()
{
    Preferences preferences;
    preferences.begin("rf-codes", false);
    preferences.clear();
    preferences.end();
    for (int i = 0; i < MAX_CODES; i++)
    {
        savedCodes[i] = {"", 0, 24, 1, 320, false};
    }
    Serial.println("已清除所有编码");
}
