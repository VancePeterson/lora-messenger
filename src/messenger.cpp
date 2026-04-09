// LoRa Messenger - Web-based chat with configurable settings
// Both devices run the same firmware

#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <ArduinoJson.h>
#include <Preferences.h>

// Pin definitions
#define LORA_RX 33
#define LORA_TX 32

// Defaults
#define DEFAULT_DEVICE_NAME "LoRa-Chat"
#define DEFAULT_ADDRESS 1
#define DEFAULT_NETWORK_ID 5
#define DEFAULT_BAND 915000000
#define DEFAULT_SPREADING_FACTOR 9
#define DEFAULT_BANDWIDTH 7
#define DEFAULT_CODING_RATE 1
#define DEFAULT_PREAMBLE 12
#define DEFAULT_TX_POWER 22

HardwareSerial LoRa(2);
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
Preferences prefs;

// Configuration
struct Config {
  char deviceName[32];
  uint16_t address;
  uint8_t networkId;
  uint32_t band;
  uint8_t spreadingFactor;
  uint8_t bandwidth;
  uint8_t codingRate;
  uint16_t preamble;
  uint8_t txPower;
} config;

// Message history (last 50 messages)
#define MAX_MESSAGES 50
struct Message {
  bool incoming;
  uint16_t fromAddress;
  char text[128];
  int rssi;
  int snr;
};
Message messages[MAX_MESSAGES];
int messageCount = 0;
int messageIndex = 0;

// Web page HTML
const char index_html[] = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>LoRa Messenger</title>
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; background: #1a1a2e; color: #eee; height: 100vh; display: flex; flex-direction: column; }
    .tabs { display: flex; background: #16213e; }
    .tab { flex: 1; padding: 15px; text-align: center; cursor: pointer; border-bottom: 3px solid transparent; }
    .tab.active { border-bottom-color: #0f3460; background: #0f3460; }
    .page { flex: 1; display: none; flex-direction: column; overflow: hidden; }
    .page.active { display: flex; }

    /* Chat styles */
    #messages { flex: 1; overflow-y: auto; padding: 10px; display: flex; flex-direction: column; gap: 8px; }
    .msg { max-width: 80%; padding: 10px 14px; border-radius: 12px; }
    .msg.in { background: #0f3460; align-self: flex-start; border-bottom-left-radius: 4px; }
    .msg.out { background: #533483; align-self: flex-end; border-bottom-right-radius: 4px; }
    .msg .meta { font-size: 11px; opacity: 0.7; margin-top: 4px; }
    .msg .text { word-wrap: break-word; }
    #chatForm { display: flex; padding: 10px; gap: 10px; background: #16213e; }
    #chatInput { flex: 1; padding: 12px; border: none; border-radius: 8px; background: #0f3460; color: #eee; font-size: 16px; }
    #chatForm button { padding: 12px 20px; border: none; border-radius: 8px; background: #533483; color: #eee; font-size: 16px; cursor: pointer; }

    /* Settings styles */
    .settings { padding: 15px; overflow-y: auto; }
    .setting { margin-bottom: 15px; }
    .setting label { display: block; margin-bottom: 5px; font-size: 14px; opacity: 0.8; }
    .setting input, .setting select { width: 100%; padding: 12px; border: none; border-radius: 8px; background: #0f3460; color: #eee; font-size: 16px; }
    .setting small { display: block; margin-top: 4px; font-size: 12px; opacity: 0.6; }
    .btn { width: 100%; padding: 15px; border: none; border-radius: 8px; background: #533483; color: #eee; font-size: 16px; cursor: pointer; margin-top: 10px; }
    .btn.danger { background: #6b2d5c; }
    .status { padding: 10px 15px; background: #0f3460; font-size: 13px; text-align: center; }
    .status.connected { background: #1e5128; }
    .status.disconnected { background: #6b2d5c; }
  </style>
</head>
<body>
  <div class="tabs">
    <div class="tab active" onclick="showPage('chat')">Chat</div>
    <div class="tab" onclick="showPage('settings')">Settings</div>
  </div>

  <div id="chatPage" class="page active">
    <div id="messages"></div>
    <form id="chatForm">
      <input type="text" id="chatInput" placeholder="Type a message..." autocomplete="off">
      <button type="submit">Send</button>
    </form>
  </div>

  <div id="settingsPage" class="page">
    <div class="settings">
      <div class="setting">
        <label>Device Name (WiFi SSID)</label>
        <input type="text" id="deviceName" maxlength="31">
      </div>
      <div class="setting">
        <label>Device Address</label>
        <input type="number" id="address" min="1" max="65535">
        <small>Unique ID for this device (1-65535)</small>
      </div>
      <div class="setting">
        <label>Target Address</label>
        <input type="number" id="targetAddress" min="1" max="65535">
        <small>Address to send messages to</small>
      </div>
      <div class="setting">
        <label>Network ID</label>
        <input type="number" id="networkId" min="0" max="16">
        <small>Must match on both devices (0-16)</small>
      </div>
      <div class="setting">
        <label>Frequency Band</label>
        <select id="band">
          <option value="915000000">915 MHz (US)</option>
          <option value="868000000">868 MHz (EU)</option>
          <option value="433000000">433 MHz</option>
        </select>
      </div>
      <div class="setting">
        <label>Spreading Factor</label>
        <select id="spreadingFactor">
          <option value="7">SF7 (Fastest)</option>
          <option value="8">SF8</option>
          <option value="9">SF9 (Default)</option>
          <option value="10">SF10</option>
          <option value="11">SF11</option>
          <option value="12">SF12 (Longest Range)</option>
        </select>
        <small>Higher = longer range, slower speed</small>
      </div>
      <div class="setting">
        <label>Bandwidth</label>
        <select id="bandwidth">
          <option value="7">125 kHz (Default)</option>
          <option value="8">250 kHz</option>
          <option value="9">500 kHz</option>
        </select>
      </div>
      <div class="setting">
        <label>TX Power (dBm)</label>
        <input type="number" id="txPower" min="0" max="22">
        <small>0-22 dBm</small>
      </div>
      <button type="button" class="btn" onclick="saveSettings()">Save &amp; Apply</button>
      <button type="button" class="btn danger" onclick="reboot()">Reboot Device</button>
    </div>
  </div>

  <div id="status" class="status disconnected">Connecting...</div>

  <script>
    let ws;
    let targetAddress = 2;

    function connect() {
      ws = new WebSocket('ws://' + location.host + '/ws');
      ws.onopen = () => {
        document.getElementById('status').className = 'status connected';
        document.getElementById('status').textContent = 'Connected';
        ws.send(JSON.stringify({type: 'getConfig'}));
        ws.send(JSON.stringify({type: 'getMessages'}));
      };
      ws.onclose = () => {
        document.getElementById('status').className = 'status disconnected';
        document.getElementById('status').textContent = 'Disconnected - Reconnecting...';
        setTimeout(connect, 2000);
      };
      ws.onmessage = (e) => {
        const data = JSON.parse(e.data);
        if (data.type === 'config') {
          document.getElementById('deviceName').value = data.deviceName;
          document.getElementById('address').value = data.address;
          document.getElementById('targetAddress').value = data.targetAddress;
          document.getElementById('networkId').value = data.networkId;
          document.getElementById('band').value = data.band;
          document.getElementById('spreadingFactor').value = data.spreadingFactor;
          document.getElementById('bandwidth').value = data.bandwidth;
          document.getElementById('txPower').value = data.txPower;
          targetAddress = data.targetAddress;
        } else if (data.type === 'message') {
          addMessage(data);
        } else if (data.type === 'messages') {
          document.getElementById('messages').innerHTML = '';
          data.messages.forEach(addMessage);
        }
      };
    }

    function addMessage(msg) {
      const div = document.createElement('div');
      div.className = 'msg ' + (msg.incoming ? 'in' : 'out');
      let meta = msg.incoming ? 'From: ' + msg.from : 'Sent';
      if (msg.rssi) meta += ' | RSSI: ' + msg.rssi + ' dBm';
      div.innerHTML = '<div class="text">' + escapeHtml(msg.text) + '</div><div class="meta">' + meta + '</div>';
      document.getElementById('messages').appendChild(div);
      document.getElementById('messages').scrollTop = document.getElementById('messages').scrollHeight;
    }

    function escapeHtml(text) {
      const div = document.createElement('div');
      div.textContent = text;
      return div.innerHTML;
    }

    function showPage(page) {
      document.querySelectorAll('.tab').forEach(t => t.classList.remove('active'));
      document.querySelectorAll('.page').forEach(p => p.classList.remove('active'));
      document.querySelector('.tab:nth-child(' + (page === 'chat' ? 1 : 2) + ')').classList.add('active');
      document.getElementById(page + 'Page').classList.add('active');
    }

    function saveSettings() {
      targetAddress = parseInt(document.getElementById('targetAddress').value);
      ws.send(JSON.stringify({
        type: 'saveConfig',
        deviceName: document.getElementById('deviceName').value,
        address: parseInt(document.getElementById('address').value),
        targetAddress: targetAddress,
        networkId: parseInt(document.getElementById('networkId').value),
        band: parseInt(document.getElementById('band').value),
        spreadingFactor: parseInt(document.getElementById('spreadingFactor').value),
        bandwidth: parseInt(document.getElementById('bandwidth').value),
        txPower: parseInt(document.getElementById('txPower').value)
      }));
      alert('Settings saved! Some changes require reboot.');
    }

    function reboot() {
      if (confirm('Reboot device?')) {
        ws.send(JSON.stringify({type: 'reboot'}));
      }
    }

    document.getElementById('chatForm').onsubmit = (e) => {
      e.preventDefault();
      const input = document.getElementById('chatInput');
      const text = input.value.trim();
      if (text && ws.readyState === WebSocket.OPEN) {
        ws.send(JSON.stringify({type: 'send', text: text, to: targetAddress}));
        input.value = '';
      }
    };

    connect();
  </script>
</body>
</html>
)rawliteral";

// Load config from NVS
void loadConfig() {
  prefs.begin("lora", true);
  strlcpy(config.deviceName, prefs.getString("deviceName", DEFAULT_DEVICE_NAME).c_str(), sizeof(config.deviceName));
  config.address = prefs.getUShort("address", DEFAULT_ADDRESS);
  config.networkId = prefs.getUChar("networkId", DEFAULT_NETWORK_ID);
  config.band = prefs.getUInt("band", DEFAULT_BAND);
  config.spreadingFactor = prefs.getUChar("sf", DEFAULT_SPREADING_FACTOR);
  config.bandwidth = prefs.getUChar("bw", DEFAULT_BANDWIDTH);
  config.codingRate = prefs.getUChar("cr", DEFAULT_CODING_RATE);
  config.preamble = prefs.getUShort("preamble", DEFAULT_PREAMBLE);
  config.txPower = prefs.getUChar("txPower", DEFAULT_TX_POWER);
  prefs.end();
}

// Save config to NVS
void saveConfig() {
  prefs.begin("lora", false);
  prefs.putString("deviceName", config.deviceName);
  prefs.putUShort("address", config.address);
  prefs.putUChar("networkId", config.networkId);
  prefs.putUInt("band", config.band);
  prefs.putUChar("sf", config.spreadingFactor);
  prefs.putUChar("bw", config.bandwidth);
  prefs.putUChar("cr", config.codingRate);
  prefs.putUShort("preamble", config.preamble);
  prefs.putUChar("txPower", config.txPower);
  prefs.end();
}

uint16_t targetAddress = 2;

void saveTargetAddress() {
  prefs.begin("lora", false);
  prefs.putUShort("target", targetAddress);
  prefs.end();
}

void loadTargetAddress() {
  prefs.begin("lora", true);
  targetAddress = prefs.getUShort("target", 2);
  prefs.end();
}

// Send AT command and wait for response
String sendATCommand(const String& cmd, unsigned long timeout = 1000) {
  // Clear buffer
  while (LoRa.available()) LoRa.read();

  LoRa.println(cmd);
  Serial.println("TX: " + cmd);

  String response = "";
  unsigned long start = millis();
  while (millis() - start < timeout) {
    while (LoRa.available()) {
      char c = LoRa.read();
      response += c;
      if (response.endsWith("\r\n")) {
        response.trim();
        Serial.println("RX: " + response);
        return response;
      }
    }
  }
  response.trim();
  return response;
}

// Configure RYLR998
void configureLoRa() {
  delay(100);

  // Set address
  sendATCommand("AT+ADDRESS=" + String(config.address));
  delay(50);

  // Set network ID
  sendATCommand("AT+NETWORKID=" + String(config.networkId));
  delay(50);

  // Set band
  sendATCommand("AT+BAND=" + String(config.band));
  delay(50);

  // Set parameters: SF, BW, CR, Preamble
  String params = String(config.spreadingFactor) + "," +
                  String(config.bandwidth) + "," +
                  String(config.codingRate) + "," +
                  String(config.preamble);
  sendATCommand("AT+PARAMETER=" + params);
  delay(50);

  // Set TX power
  sendATCommand("AT+CRFOP=" + String(config.txPower));
  delay(50);

  Serial.println("LoRa configured");
}

// Add message to history
void addMessage(bool incoming, uint16_t from, const char* text, int rssi = 0, int snr = 0) {
  Message& msg = messages[messageIndex];
  msg.incoming = incoming;
  msg.fromAddress = from;
  strlcpy(msg.text, text, sizeof(msg.text));
  msg.rssi = rssi;
  msg.snr = snr;

  messageIndex = (messageIndex + 1) % MAX_MESSAGES;
  if (messageCount < MAX_MESSAGES) messageCount++;
}

// Broadcast message to all WebSocket clients
void broadcastMessage(bool incoming, uint16_t from, const char* text, int rssi = 0) {
  JsonDocument doc;
  doc["type"] = "message";
  doc["incoming"] = incoming;
  doc["from"] = from;
  doc["text"] = text;
  if (rssi != 0) doc["rssi"] = rssi;

  String json;
  serializeJson(doc, json);
  ws.textAll(json);
}

// Send config to client
void sendConfig(AsyncWebSocketClient* client) {
  JsonDocument doc;
  doc["type"] = "config";
  doc["deviceName"] = config.deviceName;
  doc["address"] = config.address;
  doc["targetAddress"] = targetAddress;
  doc["networkId"] = config.networkId;
  doc["band"] = config.band;
  doc["spreadingFactor"] = config.spreadingFactor;
  doc["bandwidth"] = config.bandwidth;
  doc["txPower"] = config.txPower;

  String json;
  serializeJson(doc, json);
  client->text(json);
}

// Send message history to client
void sendMessages(AsyncWebSocketClient* client) {
  JsonDocument doc;
  doc["type"] = "messages";
  JsonArray arr = doc["messages"].to<JsonArray>();

  // Send messages in order
  int start = (messageCount < MAX_MESSAGES) ? 0 : messageIndex;
  for (int i = 0; i < messageCount; i++) {
    int idx = (start + i) % MAX_MESSAGES;
    JsonObject msg = arr.add<JsonObject>();
    msg["incoming"] = messages[idx].incoming;
    msg["from"] = messages[idx].fromAddress;
    msg["text"] = messages[idx].text;
    if (messages[idx].rssi != 0) msg["rssi"] = messages[idx].rssi;
  }

  String json;
  serializeJson(doc, json);
  client->text(json);
}

// Handle WebSocket events
void onWsEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
               AwsEventType type, void* arg, uint8_t* data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    Serial.println("WebSocket client connected");
  } else if (type == WS_EVT_DISCONNECT) {
    Serial.println("WebSocket client disconnected");
  } else if (type == WS_EVT_DATA) {
    AwsFrameInfo* info = (AwsFrameInfo*)arg;
    if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
      data[len] = 0;

      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, (char*)data);
      if (error) return;

      const char* msgType = doc["type"];

      if (strcmp(msgType, "getConfig") == 0) {
        sendConfig(client);
      } else if (strcmp(msgType, "getMessages") == 0) {
        sendMessages(client);
      } else if (strcmp(msgType, "send") == 0) {
        const char* text = doc["text"];
        uint16_t to = doc["to"];

        if (text && strlen(text) > 0) {
          // Send via LoRa
          String cmd = "AT+SEND=" + String(to) + "," + String(strlen(text)) + "," + String(text);
          String response = sendATCommand(cmd, 2000);

          // Add to history and broadcast
          addMessage(false, config.address, text);
          broadcastMessage(false, config.address, text);
        }
      } else if (strcmp(msgType, "saveConfig") == 0) {
        strlcpy(config.deviceName, doc["deviceName"] | DEFAULT_DEVICE_NAME, sizeof(config.deviceName));
        config.address = doc["address"] | DEFAULT_ADDRESS;
        targetAddress = doc["targetAddress"] | 2;
        config.networkId = doc["networkId"] | DEFAULT_NETWORK_ID;
        config.band = doc["band"] | DEFAULT_BAND;
        config.spreadingFactor = doc["spreadingFactor"] | DEFAULT_SPREADING_FACTOR;
        config.bandwidth = doc["bandwidth"] | DEFAULT_BANDWIDTH;
        config.txPower = doc["txPower"] | DEFAULT_TX_POWER;

        saveConfig();
        saveTargetAddress();
        configureLoRa();

        // Send updated config back
        sendConfig(client);
      } else if (strcmp(msgType, "reboot") == 0) {
        ESP.restart();
      }
    }
  }
}

// Parse incoming LoRa message
// Format: +RCV=<address>,<length>,<data>,<RSSI>,<SNR>
void parseLoRaMessage(const String& msg) {
  if (!msg.startsWith("+RCV=")) return;

  int idx1 = msg.indexOf(',');
  int idx2 = msg.indexOf(',', idx1 + 1);
  int idx3 = msg.indexOf(',', idx2 + 1);
  int idx4 = msg.indexOf(',', idx3 + 1);

  if (idx1 < 0 || idx2 < 0 || idx3 < 0) return;

  uint16_t from = msg.substring(5, idx1).toInt();
  int length = msg.substring(idx1 + 1, idx2).toInt();
  String text = msg.substring(idx2 + 1, idx3);
  int rssi = (idx4 > 0) ? msg.substring(idx3 + 1, idx4).toInt() : 0;
  int snr = (idx4 > 0) ? msg.substring(idx4 + 1).toInt() : 0;

  Serial.println("Message from " + String(from) + ": " + text + " (RSSI: " + String(rssi) + ")");

  // Add to history and broadcast
  addMessage(true, from, text.c_str(), rssi, snr);
  broadcastMessage(true, from, text.c_str(), rssi);
}

void setup() {
  Serial.begin(115200);
  LoRa.begin(115200, SERIAL_8N1, LORA_RX, LORA_TX);

  // Load configuration
  loadConfig();
  loadTargetAddress();

  Serial.println("\n\nLoRa Messenger starting...");
  Serial.println("Device: " + String(config.deviceName));
  Serial.println("Address: " + String(config.address));

  // Configure LoRa module
  delay(1000);
  configureLoRa();

  // Start WiFi AP
  WiFi.softAP(config.deviceName);
  Serial.println("WiFi AP started: " + String(config.deviceName));
  Serial.println("IP: " + WiFi.softAPIP().toString());

  // Setup WebSocket
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  // Serve web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(200, "text/html", index_html);
  });

  server.begin();
  Serial.println("Web server started");
}

String loraBuffer = "";

void loop() {
  // Clean up WebSocket clients
  ws.cleanupClients();

  // Read from LoRa
  while (LoRa.available()) {
    char c = LoRa.read();
    loraBuffer += c;

    if (loraBuffer.endsWith("\r\n")) {
      loraBuffer.trim();
      if (loraBuffer.length() > 0) {
        Serial.println("LoRa: " + loraBuffer);
        parseLoRaMessage(loraBuffer);
      }
      loraBuffer = "";
    }
  }

  // Prevent buffer overflow
  if (loraBuffer.length() > 256) {
    loraBuffer = "";
  }
}
