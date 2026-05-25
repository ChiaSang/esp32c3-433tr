#include "web_server.h"
#include "../../include/html.h"
#include "../rf_codes/rf_codes.h"
#include "../button_handler/button_handler.h"
#include <WiFi.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <RCSwitch.h>

extern RCSwitch txSwitch;

AsyncWebServer server(80);
volatile bool learningMode = false;
unsigned long learningModeStartTime = 0;

void setupWebServer()
{
    // 主页 - HTML 界面
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send_P(200, "text/html", INDEX_HTML); });

    // API: 获取所有编码
    server.on("/api/codes", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        JsonDocument doc;
        JsonArray codes = doc["codes"].to<JsonArray>();

        for (int i = 0; i < MAX_CODES; i++) {
            JsonObject code = codes.add<JsonObject>();
            code["name"] = savedCodes[i].name;
            code["code"] = String(savedCodes[i].code);
            code["bitlength"] = savedCodes[i].bitlength;
            code["protocol"] = savedCodes[i].protocol;
            code["pulseLength"] = savedCodes[i].pulseLength;
            code["enabled"] = savedCodes[i].enabled;
        }
        doc["currentIndex"] = currentCodeIndex;

        String output;
        serializeJson(doc, output);
        request->send(200, "application/json", output); });

    // API: 发射指定编码
    server.on("/api/send/*", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        String path = request->url().substring(String("/api/send/").length());
        int index = path.toInt();

        if (index >= 0 && index < MAX_CODES && savedCodes[index].enabled) {
            RFCode &code = savedCodes[index];
            txSwitch.setProtocol(code.protocol);
            txSwitch.setPulseLength(code.pulseLength);
            for (int r = 0; r < txRepeatCount; r++) {
                if (r > 0) delay(50);
                txSwitch.send(code.code, code.bitlength);
            }
            Serial.printf("Web 发射编码 #%d: %lu (x%d)\n", index, code.code, txRepeatCount);
            request->send(200, "text/plain", "OK");
        } else {
            request->send(400, "text/plain", "Invalid index");
        } });

    // API: 选择当前编码
    server.on("/api/select/*", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        String path = request->url().substring(String("/api/select/").length());
        int index = path.toInt();

        if (index >= 0 && index < MAX_CODES && savedCodes[index].enabled) {
            currentCodeIndex = index;
            Serial.printf("已选择编码 #%d\n", index);
            request->send(200, "text/plain", "OK");
        } else {
            request->send(400, "text/plain", "Invalid index");
        } });

    // API: 删除编码
    server.on("/api/delete/*", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        String path = request->url().substring(String("/api/delete/").length());
        int index = path.toInt();

        if (index >= 0 && index < MAX_CODES) {
            savedCodes[index] = {"", 0, 24, 1, 320, false};
            saveCodeToFlash(index);
            Serial.printf("已删除编码 #%d\n", index);
            request->send(200, "text/plain", "OK");
        } else {
            request->send(400, "text/plain", "Invalid index");
        } });

    // API: 重命名编码
    server.on("/api/rename/*", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        String path = request->url().substring(String("/api/rename/").length());
        int index = path.toInt();

        if (index >= 0 && index < MAX_CODES && savedCodes[index].enabled) {
            if (request->hasParam("name")) {
                String newName = request->getParam("name")->value();
                if (newName.length() > 0) {
                    savedCodes[index].name = newName;
                    saveCodeToFlash(index);
                    Serial.printf("已重命名编码 #%d 为: %s\n", index, newName.c_str());
                    request->send(200, "text/plain", "OK");
                } else {
                    request->send(400, "text/plain", "Name cannot be empty");
                }
            } else {
                request->send(400, "text/plain", "Missing name parameter");
            }
        } else {
            request->send(400, "text/plain", "Invalid index");
        } });

    // API: 开始学习模式
    server.on("/api/learn", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        learningMode = true;
        learningModeStartTime = millis(); // 记录开始时间
        led1.blink(200);
        Serial.println("Web 触发学习模式");
        request->send(200, "text/plain", "OK"); });

    // API: 获取状态
    server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        JsonDocument doc;
        doc["learning"] = learningMode;
        doc["currentIndex"] = currentCodeIndex;
        doc["txRepeat"] = txRepeatCount;

        String output;
        serializeJson(doc, output);
        request->send(200, "application/json", output); });

    // API: 设置发射重复次数
    server.on("/api/repeat/*", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        String path = request->url().substring(String("/api/repeat/").length());
        int count = path.toInt();
        if (count >= 1 && count <= 10) {
            txRepeatCount = count;
            Preferences preferences;
            preferences.begin("rf-codes", false);
            preferences.putInt("txRepeat", txRepeatCount);
            preferences.end();
            Serial.printf("发射重复次数已设为: %d\n", txRepeatCount);
            request->send(200, "text/plain", "OK");
        } else {
            request->send(400, "text/plain", "Invalid count (1-10)");
        } });

    // API: 清除所有编码
    server.on("/api/clear", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        clearAllCodes();
        request->send(200, "text/plain", "OK"); });

    // API: 获取网络信息
    server.on("/api/network", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        JsonDocument doc;
        doc["ip"] = WiFi.localIP().toString();
        doc["gateway"] = WiFi.gatewayIP().toString();
        doc["subnet"] = WiFi.subnetMask().toString();
        doc["dns"] = WiFi.dnsIP().toString();
        doc["mac"] = WiFi.macAddress();
        doc["rssi"] = WiFi.RSSI();
        doc["ssid"] = WiFi.SSID();
        doc["bssid"] = WiFi.BSSIDstr();
        doc["channel"] = WiFi.channel();
        char uptime[32];
        unsigned long sec = millis() / 1000;
        snprintf(uptime, sizeof(uptime), "%02lu:%02lu:%02lu", sec / 3600, (sec % 3600) / 60, sec % 60);
        doc["uptime"] = String(uptime);
        doc["heap"] = ESP.getFreeHeap();

        String output;
        serializeJson(doc, output);
        request->send(200, "application/json", output); });

    // API: 导出备份
    server.on("/api/backup", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        String backup = "#ESP32-433TR-v1\n";
        backup += "#repeat=" + String(txRepeatCount) + "\n";
        for (int i = 0; i < MAX_CODES; i++) {
            if (savedCodes[i].enabled) {
                // 名称中的冒号替换为下划线
                String name = savedCodes[i].name;
                name.replace(':', '_');
                backup += name + ":" + String(savedCodes[i].code) + ":" +
                          String(savedCodes[i].bitlength) + ":" +
                          String(savedCodes[i].protocol) + ":" +
                          String(savedCodes[i].pulseLength) + "\n";
            }
        }
        AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", backup);
        response->addHeader("Content-Disposition", "attachment; filename=backup.txt");
        request->send(response);
        Serial.println("已导出备份"); });

    // API: 导入备份
    server.on("/api/restore", HTTP_POST, [](AsyncWebServerRequest *request)
              {
                  // 此回调在请求完成时调用，body 由 onBody 收集
              },
              NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
              {
        static String body;
        if (index == 0) body = "";
        body.concat((const char *)data, len);
        if (index + len < total) return; // 等待全部 body 接收完毕

        // 校验文件头
        int firstLineEnd = body.indexOf('\n');
        if (firstLineEnd < 0 || !body.startsWith("#ESP32-433TR-v1")) {
            request->send(400, "text/plain", "无效的备份文件");
            return;
        }

        // 清空现有编码
        clearAllCodes();

        int count = 0;
        int skipped = 0;
        int lineNum = 0;
        int pos = 0;

        while (pos >= 0 && pos < (int)body.length()) {
            int nextPos = body.indexOf('\n', pos);
            String line;
            if (nextPos < 0) {
                line = body.substring(pos);
                pos = -1;
            } else {
                line = body.substring(pos, nextPos);
                pos = nextPos + 1;
            }
            line.trim();
            lineNum++;

            if (line.length() == 0 || line.startsWith("#")) {
                // 解析 repeat 设置
                if (line.startsWith("#repeat=")) {
                    int rep = line.substring(8).toInt();
                    if (rep >= 1 && rep <= 10) {
                        txRepeatCount = rep;
                        Preferences preferences;
                        preferences.begin("rf-codes", false);
                        preferences.putInt("txRepeat", txRepeatCount);
                        preferences.end();
                    }
                }
                continue;
            }

            if (count >= MAX_CODES) { skipped++; continue; }

            // 解析 name:code:bits:proto:pulse
            int c1 = line.indexOf(':');
            if (c1 < 0) { skipped++; continue; }
            int c2 = line.indexOf(':', c1 + 1);
            if (c2 < 0) { skipped++; continue; }
            int c3 = line.indexOf(':', c2 + 1);
            if (c3 < 0) { skipped++; continue; }
            int c4 = line.indexOf(':', c3 + 1);
            if (c4 < 0) { skipped++; continue; }

            String name = line.substring(0, c1);
            unsigned long code = strtoul(line.substring(c1+1, c2).c_str(), NULL, 10);
            unsigned int bits = line.substring(c2+1, c3).toInt();
            unsigned int proto = line.substring(c3+1, c4).toInt();
            unsigned int pulse = line.substring(c4+1).toInt();

            if (code == 0 || bits == 0 || proto == 0 || pulse == 0) { skipped++; continue; }

            savedCodes[count].name = name;
            savedCodes[count].code = code;
            savedCodes[count].bitlength = bits;
            savedCodes[count].protocol = proto;
            savedCodes[count].pulseLength = pulse;
            savedCodes[count].enabled = true;
            saveCodeToFlash(count);
            count++;
        }

        if (count > 0) currentCodeIndex = 0;
        String msg = "导入成功：" + String(count) + " 条";
        if (skipped > 0) msg += "，跳过 " + String(skipped) + " 行";
        Serial.printf("备份导入完成：%d 条成功，%d 条跳过\n", count, skipped);
        request->send(200, "text/plain", msg); });

    server.begin();
    Serial.println("Web 服务器已启动");
}
