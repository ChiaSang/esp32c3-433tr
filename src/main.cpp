#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <RCSwitch.h>
#include <OneButton.h>
#include <LedControl.h>

#define TX_PIN 7   // 发射引脚
#define RX_PIN 6   // 接收引脚
#define BTN_PIN 10 // 按钮引脚
#define BOOT_PIN 9 // 按钮引脚
#define LED1_PIN 12
#define LED2_PIN 13

// 发射重复次数（可通过 Web 修改）
#define TX_REPEAT_COUNT_DEFAULT 2
int txRepeatCount = TX_REPEAT_COUNT_DEFAULT;

// WiFiManager 实例
WiFiManager wifiManager;

// Web 服务器实例
AsyncWebServer server(80);

// Preferences 用于持久化存储
Preferences preferences;

RCSwitch txSwitch = RCSwitch();
RCSwitch rxSwitch = RCSwitch();
OneButton button = OneButton(BTN_PIN, true, true); // 低电平有效，启用内部上拉
OneButton bootButton = OneButton(BOOT_PIN, true, true);
LedControl led1(LED1_PIN);
LedControl led2(LED2_PIN);

volatile bool learningMode = false;      // 是否处于学习模式
volatile bool btnClickFlag = false;      // 按钮单击标志（中断中置位）
volatile bool btnLongPressFlag = false;  // 按钮长按标志
volatile bool bootLongPressFlag = false; // BOOT 按钮长按标志（重置 WiFi）

unsigned long learningModeStartTime = 0;      // 学习模式开始时间
const unsigned long LEARNING_TIMEOUT = 60000; // 学习模式超时时间（5秒）

// 学习到的信号参数（最多存储10个）
#define MAX_CODES 10
struct RFCode
{
    String name;
    unsigned long code;
    unsigned int bitlength;
    unsigned int protocol;
    unsigned int pulseLength;
    bool enabled;
};

RFCode savedCodes[MAX_CODES];
int currentCodeIndex = 0; // 当前选中的编码索引

// ========== 持久化存储函数 ==========
void loadCodesFromFlash()
{
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
    preferences.begin("rf-codes", false);
    preferences.clear();
    preferences.end();
    for (int i = 0; i < MAX_CODES; i++)
    {
        savedCodes[i] = {"", 0, 24, 1, 320, false};
    }
    Serial.println("已清除所有编码");
}

// ========== 按钮回调函数 ==========
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
}

// ========== Web 服务器路由设置 ==========
void setupWebServer()
{
    // 主页 - HTML 界面
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        String html = R"rawliteral(
<!DOCTYPE html>
<html lang="zh-CN">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ESP32-C3 433MHz 控制器</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body { font-family: Arial, sans-serif; background: #f0f2f5; padding: 12px; -webkit-text-size-adjust: 100%; }
        .container { max-width: 800px; margin: 0 auto; }
        .header { background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
                  color: white; padding: 16px; border-radius: 10px; margin-bottom: 12px; }
        .header h1 { font-size: 18px; }
        .header p { font-size: 13px; margin-top: 4px; }
        .card { background: white; border-radius: 10px; padding: 14px; margin-bottom: 12px;
                box-shadow: 0 2px 10px rgba(0,0,0,0.1); }
        .card h2 { font-size: 16px; margin-bottom: 10px; }
        .btn { padding: 7px 12px; border: none; border-radius: 5px; cursor: pointer;
               font-size: 13px; transition: all 0.3s; white-space: nowrap; }
        .btn-sm { padding: 5px 8px; font-size: 12px; }
        .btn-primary { background: #667eea; color: white; }
        .btn-primary:hover { background: #5568d3; }
        .btn-success { background: #48bb78; color: white; }
        .btn-success:hover { background: #38a169; }
        .btn-danger { background: #f56565; color: white; }
        .btn-danger:hover { background: #e53e3e; }
        .btn-warning { background: #ed8936; color: white; }
        .btn-warning:hover { background: #dd6b20; }
        .code-item { border: 1px solid #e2e8f0; padding: 10px; margin-bottom: 8px;
                     border-radius: 8px; }
        .code-item.active { border-color: #667eea; background: #f7fafc; }
        .code-info { margin-bottom: 8px; }
        .code-name { font-weight: bold; font-size: 14px; margin-bottom: 3px; }
        .code-details { font-size: 11px; color: #718096; line-height: 1.4; }
        .code-actions { display: flex; gap: 6px; flex-wrap: wrap; }
        input[type="text"], input[type="number"] { width: 100%; padding: 10px;
                border: 1px solid #e2e8f0; border-radius: 5px; margin-bottom: 10px; }
        .status { padding: 10px; border-radius: 5px; margin-bottom: 10px; }
        .status.success { background: #c6f6d5; color: #22543d; }
        .status.error { background: #fed7d7; color: #742a2a; }
        .learning-mode { background: #fef5e7; border: 2px solid #f39c12; padding: 12px;
                        border-radius: 8px; text-align: center; margin-bottom: 12px; }
        .learning-mode h3 { font-size: 15px; }
        .learning-mode p { font-size: 13px; }
        .system-actions { display: flex; gap: 8px; flex-wrap: wrap; }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>🎛️ ESP32-C3 433MHz 控制器</h1>
        </div>

        <div id="learningStatus" class="learning-mode" style="display:none;">
            <h3>🔴 学习模式已激活</h3>
            <p>请发射 433MHz 信号...</p>
        </div>

        <div class="card">
            <h2>📡 已保存的编码</h2>
            <div id="codeList"></div>
        </div>

        <div class="card">
            <h2>➕ 学习新编码</h2>
            <button class="btn btn-warning" onclick="startLearning()">开始学习模式</button>
            <p style="margin-top:10px; color:#718096; font-size:12px;">
                点击后，请在5秒内发射433MHz信号
            </p>
        </div>

        <div class="card">
            <h2>⚙️ 系统操作</h2>
            <div style="margin-bottom:10px; display:flex; align-items:center; gap:8px; flex-wrap:wrap;">
                <span style="font-size:13px;">发射重复次数:</span>
                <button class="btn btn-sm btn-primary" onclick="setRepeat()" id="repeatBtn">2次</button>
                <input type="range" id="repeatSlider" min="1" max="10" value="2"
                       style="width:120px; margin:0;" oninput="updateRepeatLabel()">
            </div>
            <div class="system-actions">
                <button class="btn btn-danger" onclick="clearAll()">清除所有编码</button>
                <button class="btn btn-primary" onclick="location.reload()">刷新页面</button>
            </div>
        </div>
        <div class="card" id="netInfo" style="margin-top:10px; font-size:12px; color:#718096;">正在获取网络信息...</div>
    </div>

    <script>
        let currentIndex = 0;

        async function loadCodes() {
            const res = await fetch('/api/codes');
            const data = await res.json();
            currentIndex = data.currentIndex;

            const list = document.getElementById('codeList');
            if (data.codes.filter(c => c.enabled).length === 0) {
                list.innerHTML = '<p style="color:#a0aec0;">暂无保存的编码</p>';
                return;
            }

            list.innerHTML = data.codes.map((code, i) => {
                if (!code.enabled) return '';
                return `
                    <div class="code-item ${i === currentIndex ? 'active' : ''}">
                        <div class="code-info">
                            <div class="code-name">${code.name}</div>
                            <div class="code-details">
                                编码:${code.code} | ${code.bitlength}位 | P${code.protocol} | ${code.pulseLength}μs
                            </div>
                        </div>
                        <div class="code-actions">
                            <button class="btn btn-sm btn-success" onclick="sendCode(${i})">发射</button>
                            <button class="btn btn-sm btn-primary" onclick="selectCode(${i})">选择</button>
                            <button class="btn btn-sm btn-warning" onclick="renameCode(${i}, '${code.name}')">命名</button>
                            <button class="btn btn-sm btn-danger" onclick="deleteCode(${i})">删除</button>
                        </div>
                    </div>
                `;
            }).join('');
        }

        async function sendCode(index) {
            await fetch('/api/send/' + index);
            alert('已发射编码 #' + index);
        }

        async function selectCode(index) {
            await fetch('/api/select/' + index);
            await loadCodes();
        }

        async function deleteCode(index) {
            if (!confirm('确定删除此编码？')) return;
            await fetch('/api/delete/' + index);
            await loadCodes();
        }

        async function renameCode(index, oldName) {
            const newName = prompt('请输入新名称：', oldName);
            if (newName === null || newName.trim() === '') return;
            await fetch('/api/rename/' + index + '?name=' + encodeURIComponent(newName.trim()));
            await loadCodes();
        }

        async function startLearning() {
            await fetch('/api/learn');
            document.getElementById('learningStatus').style.display = 'block';
            setTimeout(checkLearningStatus, 1000);
        }

        async function checkLearningStatus() {
            const res = await fetch('/api/status');
            const data = await res.json();
            if (data.learning) {
                setTimeout(checkLearningStatus, 1000);
            } else {
                document.getElementById('learningStatus').style.display = 'none';
                await loadCodes();
            }
        }

        async function clearAll() {
            if (!confirm('确定清除所有编码？')) return;
            await fetch('/api/clear');
            await loadCodes();
        }

        function updateRepeatLabel() {
            document.getElementById('repeatBtn').textContent = document.getElementById('repeatSlider').value + '次';
        }

        async function setRepeat() {
            const count = document.getElementById('repeatSlider').value;
            await fetch('/api/repeat/' + count);
        }

        async function loadNetwork() {
            try {
                const res = await fetch('/api/network');
                const n = await res.json();
                const rssiLevel = n.rssi > -50 ? 'excellent' : n.rssi > -60 ? 'good' : n.rssi > -70 ? 'fair' : 'weak';
                document.getElementById('netInfo').innerHTML =
                    `📶 ${n.ssid} | IP: ${n.ip}` +
                    `<br><span style="font-size:11px;opacity:0.85">` +
                    `网关: ${n.gateway} | 信号: ${n.rssi}dBm | CH: ${n.channel}` +
                    ` | MAC: ${n.mac} | 运行: ${n.uptime} | 内存: ${n.heap}B` +
                    `</span>`;
            } catch(e) {
                document.getElementById('netInfo').textContent = '网络信息获取失败';
            }
        }

        loadCodes();
        loadNetwork();
        // 同步重复次数
        fetch('/api/status').then(r=>r.json()).then(d => {
            if (d.txRepeat) { document.getElementById('repeatSlider').value = d.txRepeat; updateRepeatLabel(); }
        });
        setInterval(loadCodes, 5000);
        setInterval(loadNetwork, 60000);
    </script>
</body>
</html>
)rawliteral";
        request->send(200, "text/html", html); });

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

    server.begin();
    Serial.println("Web 服务器已启动");
}

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
    pinMode(BTN_PIN, INPUT_PULLUP);
    button.attachClick(onSingleClick);
    button.attachLongPressStart(onLongPress);
    button.setPressMs(3000);
    button.setDebounceMs(50);

    // BOOT 按钮初始化
    pinMode(BOOT_PIN, INPUT_PULLUP);
    bootButton.attachLongPressStart(onBootLongPress);
    bootButton.setPressMs(5000);
    bootButton.setDebounceMs(50);

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
            for (int r = 0; r < txRepeatCount; r++) {
                if (r > 0) delay(50);
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