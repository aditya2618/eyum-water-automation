/*
  Eyum Water Automation - tank controller
  Board: ESP32 DevKit V1 (38 pin)
*/

#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <esp_now.h>
#if __has_include(<esp_arduino_version.h>)
#include <esp_arduino_version.h>
#endif

#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
typedef const wifi_tx_info_t* EspNowSendInfo_t;
typedef const esp_now_recv_info_t* EspNowRecvInfo_t;
#else
typedef const uint8_t* EspNowSendInfo_t;
typedef const uint8_t* EspNowRecvInfo_t;
#endif


constexpr uint8_t TRIGGER_PIN = 18;
constexpr uint8_t ECHO_PIN = 19;
constexpr uint8_t SETUP_BUTTON_PIN = 23; // hold low for 8 seconds to erase Wi-Fi settings
constexpr char AP_SSID[] = "Eyum-Water-Setup";
constexpr char AP_PASSWORD[] = "eyumsetup";
constexpr char MDNS_NAME[] = "eyum";
constexpr unsigned long SAMPLE_INTERVAL_MS = 10000; // trigger every 10 seconds
constexpr unsigned long RELAY_HEARTBEAT_MS = 8000;
constexpr uint8_t INVALID_SAMPLE_LIMIT = 3;

struct Config {
  char ssid[33]{};
  char password[65]{};
  char adminPassword[33]{};
  char relayMac[18]{};
  char pairToken[17]{};
  int tankDepthCm = 120;
  int sensorOffsetCm = 0;
  int startPercent = 25;
  int stopPercent = 95;
  int maxRunMinutes = 30;
};

// Keep this packed: ESP32 and ESP8266 must send exactly the same bytes.
struct __attribute__((packed)) RelayMessage {
  char token[17];
  uint8_t type;       // 1 = command, 2 = relay status acknowledgement
  uint8_t pumpOn;
  uint32_t sequence;
};

Config config;
Preferences preferences;
WebServer server(80);
DNSServer dnsServer;
bool provisioning = false;
bool networkReady = false;
bool espNowReady = false;
bool relayPeerAdded = false;
uint8_t relayAddress[6]{};
float distanceCm = NAN;
float levelPercent = NAN;
uint8_t badSamples = 0;
bool pumpRequested = false;
bool relayReportedOn = false;
String mode = "auto"; // auto, on, off
unsigned long pumpStartedAt = 0;
unsigned long lastSampleAt = 0;
unsigned long lastRelaySendAt = 0;
unsigned long lastRelayAckAt = 0;
unsigned long wifiStartedAt = 0;
uint32_t sequence = 0;

#include "dashboard.h"


const char SETUP_PAGE[] PROGMEM = R"HTML(
<!doctype html><html><head><meta name=viewport content="width=device-width,initial-scale=1"><title>Eyum Water setup</title><style>body{font:16px system-ui;max-width:560px;margin:24px auto;padding:0 16px}label{display:block;margin-top:12px}input{box-sizing:border-box;width:100%;padding:9px}button{margin-top:18px;padding:10px}</style></head><body><h1>Eyum Water setup</h1><p>Connect this controller to your home Wi-Fi. You can add the relay MAC later from the dashboard.</p><form method=post action=/setup><label>Home Wi-Fi name</label><input name=ssid required><label>Home Wi-Fi password</label><input name=password type=password required><label>Dashboard password (at least 8 characters)</label><input name=adminPassword type=password minlength=8 required><label>Tank depth, cm</label><input name=tankDepthCm type=number value=120 required><label>Start pump below, %</label><input name=startPercent type=number value=25 required><label>Stop pump at, %</label><input name=stopPercent type=number value=95 required><label>Pairing token, 8–16 characters</label><input name=pairToken value=change-me type=password minlength=8 maxlength=16 required><button>Save and restart</button></form></body></html>)HTML";

void copyValue(char *destination, size_t size, const String &value) { value.toCharArray(destination, size); }
void loadConfig() {
  preferences.begin("eyum-water", true);
  copyValue(config.ssid, sizeof(config.ssid), preferences.getString("ssid", ""));
  copyValue(config.password, sizeof(config.password), preferences.getString("pass", ""));
  copyValue(config.adminPassword, sizeof(config.adminPassword), preferences.getString("admin", ""));
  copyValue(config.relayMac, sizeof(config.relayMac), preferences.getString("relaymac", ""));
  copyValue(config.pairToken, sizeof(config.pairToken), preferences.getString("token", "change-me"));
  config.tankDepthCm = preferences.getInt("depth", 120);
  config.sensorOffsetCm = preferences.getInt("offset", 0);
  config.startPercent = preferences.getInt("start", 25);
  config.stopPercent = preferences.getInt("stop", 95);
  config.maxRunMinutes = preferences.getInt("maxrun", 30);
  preferences.end();
  if (strlen(config.pairToken) == 0) {
    strcpy(config.pairToken, "change-me");
  }
}
void saveConfig() {
  preferences.begin("eyum-water", false);
  preferences.putString("ssid", config.ssid); preferences.putString("pass", config.password);
  preferences.putString("admin", config.adminPassword); preferences.putString("relaymac", config.relayMac);
  preferences.putString("token", config.pairToken); preferences.putInt("depth", config.tankDepthCm);
  preferences.putInt("offset", config.sensorOffsetCm); preferences.putInt("start", config.startPercent);
  preferences.putInt("stop", config.stopPercent); preferences.putInt("maxrun", config.maxRunMinutes);
  preferences.end();
}
bool authenticated() {
  if (strlen(config.adminPassword) == 0) return true;
  return server.authenticate("admin", config.adminPassword);
}
void requireAuth() { if (!authenticated()) server.requestAuthentication(); }
bool parseMac(const char *text, uint8_t mac[6]) {
  if (!text) return false;
  while (*text == ' ' || *text == '\t') text++;
  if (strlen(text) < 11) return false;
  int v[6];
  if (sscanf(text, "%x:%x:%x:%x:%x:%x", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5]) == 6 ||
      sscanf(text, "%x-%x-%x-%x-%x-%x", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5]) == 6) {
    for (int i = 0; i < 6; i++) {
      if (v[i] < 0 || v[i] > 255) return false;
      mac[i] = static_cast<uint8_t>(v[i]);
    }
    return true;
  }
  return false;
}
void replyJson(const String &body) {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", body);
}
void sendRelayCommand();

void startProvisioning() {
  provisioning = true; networkReady = false; espNowReady = false;
  WiFi.mode(WIFI_AP); WiFi.softAP(AP_SSID, AP_PASSWORD);
  dnsServer.start(53, "*", WiFi.softAPIP());
  Serial.println("\n[SETUP MODE ACTIVE]");
  Serial.print("  Wi-Fi Name (SSID): "); Serial.println(AP_SSID);
  Serial.print("  Wi-Fi Password:    "); Serial.println(AP_PASSWORD);
  Serial.print("  Setup Page URL:    http://"); Serial.println(WiFi.softAPIP());
  Serial.println("Connect your phone/PC to this Wi-Fi and open the URL above to configure your home Wi-Fi.\n");
  server.on("/", HTTP_GET, [](){ server.send_P(200, "text/html; charset=utf-8", SETUP_PAGE); });
  server.on("/setup", HTTP_POST, [](){
    if (server.arg("adminPassword").length() < 8 || server.arg("pairToken").length() < 8) { server.send(400, "text/plain", "Password and pairing token must be at least 8 characters."); return; }
    copyValue(config.ssid, sizeof(config.ssid), server.arg("ssid")); copyValue(config.password, sizeof(config.password), server.arg("password"));
    copyValue(config.adminPassword, sizeof(config.adminPassword), server.arg("adminPassword")); copyValue(config.pairToken, sizeof(config.pairToken), server.arg("pairToken"));
    config.tankDepthCm = server.arg("tankDepthCm").toInt(); config.startPercent = server.arg("startPercent").toInt(); config.stopPercent = server.arg("stopPercent").toInt();
    saveConfig(); server.send(200, "text/html", "Saved. The controller is restarting; reconnect your phone to the home Wi-Fi, then open the IP shown in your router or http://eyum-water.local."); delay(1000); ESP.restart();
  });
  server.onNotFound([](){ server.sendHeader("Location", "/"); server.send(302, "text/plain", ""); });
  server.begin();
}

void sampleTank();

void handleConfigPost() {
  if (!authenticated()) { requireAuth(); return; }

  if (!server.hasArg("tankDepthCm") || !server.hasArg("startPercent") || !server.hasArg("stopPercent")) {
    server.send(400, "text/plain", "Missing required configuration fields (Tank Depth, Start %, Stop %).");
    return;
  }

  int depth = server.arg("tankDepthCm").toInt();
  int start = server.arg("startPercent").toInt();
  int stop = server.arg("stopPercent").toInt();
  int offset = server.hasArg("sensorOffsetCm") ? server.arg("sensorOffsetCm").toInt() : config.sensorOffsetCm;
  int maxRun = server.hasArg("maxRunMinutes") ? server.arg("maxRunMinutes").toInt() : config.maxRunMinutes;

  if (depth < 10 || depth > 1000) {
    server.send(400, "text/plain", "Tank depth must be between 10 cm and 1000 cm.");
    return;
  }
  if (start < 0 || start > 99) {
    server.send(400, "text/plain", "Start pump level must be between 0% and 99%.");
    return;
  }
  if (stop < 1 || stop > 100) {
    server.send(400, "text/plain", "Stop pump level must be between 1% and 100%.");
    return;
  }
  if (start >= stop) {
    server.send(400, "text/plain", "Start level must be lower than stop level (e.g. Start 25%, Stop 80%).");
    return;
  }
  if (maxRun < 1 || maxRun > 240) {
    server.send(400, "text/plain", "Maximum pump run time must be between 1 and 240 minutes.");
    return;
  }

  // Handle Relay MAC: optional if wireless relay is not connected yet
  String reqMac = server.arg("relayMac");
  reqMac.trim();
  if (reqMac.length() > 0) {
    uint8_t tempMac[6];
    if (!parseMac(reqMac.c_str(), tempMac)) {
      server.send(400, "text/plain", "Invalid Relay MAC address format. Expected: AA:BB:CC:DD:EE:FF or AA-BB-CC-DD-EE-FF.");
      return;
    }
    copyValue(config.relayMac, sizeof(config.relayMac), reqMac);
  }

  // Handle Pairing Token: optional to change
  String reqToken = server.arg("pairToken");
  reqToken.trim();
  if (reqToken.length() > 0) {
    if (reqToken.length() < 8 || reqToken.length() > 16) {
      server.send(400, "text/plain", "Pairing token must be between 8 and 16 characters.");
      return;
    }
    copyValue(config.pairToken, sizeof(config.pairToken), reqToken);
  }

  // Apply new validated settings
  config.tankDepthCm = depth;
  config.sensorOffsetCm = offset;
  config.startPercent = start;
  config.stopPercent = stop;
  config.maxRunMinutes = maxRun;

  bool restart = false;
  if (server.arg("ssid").length() > 0) {
    copyValue(config.ssid, sizeof(config.ssid), server.arg("ssid"));
    restart = true;
  }
  if (server.arg("password").length() > 0) {
    copyValue(config.password, sizeof(config.password), server.arg("password"));
    restart = true;
  }
  if (server.arg("adminPassword").length() > 0) {
    if (server.arg("adminPassword").length() < 8) {
      server.send(400, "text/plain", "Dashboard password must be at least 8 characters.");
      return;
    }
    copyValue(config.adminPassword, sizeof(config.adminPassword), server.arg("adminPassword"));
  }

  saveConfig();

  // Safely manage ESP-NOW peer without breaking existing registration
  if (espNowReady && parseMac(config.relayMac, relayAddress)) {
    if (esp_now_is_peer_exist(relayAddress)) {
      esp_now_del_peer(relayAddress);
    }
    esp_now_peer_info_t peer{};
    memcpy(peer.peer_addr, relayAddress, 6);
    peer.channel = 0;
    peer.encrypt = false;
    if (esp_now_add_peer(&peer) == ESP_OK) {
      relayPeerAdded = true;
      Serial.printf("[ESP-NOW] Relay peer registered: %s\n", config.relayMac);
    } else {
      relayPeerAdded = false;
      Serial.println("[ESP-NOW] Failed to register relay peer.");
    }
  }

  // Immediately re-sample tank with updated depth/thresholds so level recalculates instantly
  sampleTank();

  Serial.printf("[CONFIG] Saved: TankDepth=%d cm, Offset=%d cm, Start=%d%%, Stop=%d%%, MaxRun=%d min\n",
                config.tankDepthCm, config.sensorOffsetCm, config.startPercent, config.stopPercent, config.maxRunMinutes);

  server.send(200, "text/plain", restart ? "Saved. Restarting controller for new Wi-Fi..." : "Configuration saved successfully.");
  if (restart) {
    delay(700);
    ESP.restart();
  }
}

void setupServer() {
  server.on("/", HTTP_GET, [](){
    if (!authenticated()) { requireAuth(); return; }
    server.send_P(200, "text/html; charset=utf-8", DASHBOARD);
  });

  server.on("/api/status", HTTP_GET, [](){
    if (!authenticated()) { requireAuth(); return; }
    String ip = networkReady ? WiFi.localIP().toString() : "";
    unsigned long age = lastRelayAckAt ? (millis() - lastRelayAckAt) / 1000 : 9999;
    
    float waterCm = NAN;
    if (!isnan(distanceCm)) {
      float effDist = distanceCm - config.sensorOffsetCm;
      float d = config.tankDepthCm > 0 ? static_cast<float>(config.tankDepthCm) : 120.0f;
      waterCm = d - effDist;
      if (waterCm < 0.0f) waterCm = 0.0f;
      if (waterCm > d) waterCm = d;
    }

    String body = "{"
      "\"distance\":" + (isnan(distanceCm) ? String("null") : String(distanceCm, 1)) + ","
      "\"waterCm\":" + (isnan(waterCm) ? String("null") : String(waterCm, 1)) + ","
      "\"level\":" + (isnan(levelPercent) ? String("null") : String(levelPercent, 1)) + ","
      "\"pumpRequested\":" + (pumpRequested ? "true" : "false") + ","
      "\"relayOn\":" + (relayReportedOn ? "true" : "false") + ","
      "\"mode\":\"" + mode + "\","
      "\"mac\":\"" + WiFi.macAddress() + "\","
      "\"ip\":\"" + ip + "\","
      "\"relayAge\":" + String(age) + ","
      "\"tankDepthCm\":" + String(config.tankDepthCm) + ","
      "\"sensorOffsetCm\":" + String(config.sensorOffsetCm) + ","
      "\"startPercent\":" + String(config.startPercent) + ","
      "\"stopPercent\":" + String(config.stopPercent) +
    "}";
    replyJson(body);
  });

  server.on("/api/config", HTTP_GET, [](){
    if (!authenticated()) { requireAuth(); return; }
    replyJson("{\"tankDepthCm\":" + String(config.tankDepthCm) +
              ",\"sensorOffsetCm\":" + String(config.sensorOffsetCm) +
              ",\"startPercent\":" + String(config.startPercent) +
              ",\"stopPercent\":" + String(config.stopPercent) +
              ",\"maxRunMinutes\":" + String(config.maxRunMinutes) +
              ",\"relayMac\":\"" + String(config.relayMac) +
              "\",\"pairToken\":\"" + String(config.pairToken) + "\"}");
  });

  server.on("/api/config", HTTP_POST, handleConfigPost);

  server.on("/api/control", HTTP_POST, [](){
    if (!authenticated()) { requireAuth(); return; }
    String requested = server.arg("command");
    if (requested != "auto" && requested != "on" && requested != "off") {
      server.send(400, "text/plain", "Unknown command");
      return;
    }
    mode = requested;
    if (mode == "on") {
      // If water is already at or above stop threshold, do not start
      if (!isnan(levelPercent) && levelPercent >= static_cast<float>(config.stopPercent)) {
        server.send(400, "text/plain", "Tank level is already at or above stop threshold (" + String(config.stopPercent) + "%).");
        return;
      }
      pumpRequested = true;
    }
    if (mode == "off") pumpRequested = false;
    if (pumpRequested && pumpStartedAt == 0) pumpStartedAt = millis();
    if (!pumpRequested) pumpStartedAt = 0;
    sendRelayCommand();
    server.send(200, "text/plain", "Command accepted: " + mode);
  });

  server.begin();
}

void onEspNowSent(EspNowSendInfo_t, esp_now_send_status_t) {}
void onEspNowReceived(EspNowRecvInfo_t, const uint8_t *data, int length) {
  if (length != sizeof(RelayMessage)) return; RelayMessage message; memcpy(&message,data,sizeof(message));
  if (message.type==2 && strncmp(message.token,config.pairToken,sizeof(message.token))==0) { relayReportedOn=message.pumpOn; lastRelayAckAt=millis(); }
}
void startEspNow() {
  if (espNowReady || !networkReady) return;
  if (esp_now_init()!=ESP_OK) return;
  esp_now_register_send_cb(onEspNowSent); esp_now_register_recv_cb(onEspNowReceived); espNowReady=true;
}
void sendRelayCommand() {
  if (!espNowReady || !parseMac(config.relayMac, relayAddress)) return;
  if (!relayPeerAdded) {
    if (esp_now_is_peer_exist(relayAddress)) {
      esp_now_del_peer(relayAddress);
    }
    esp_now_peer_info_t peer{};
    memcpy(peer.peer_addr, relayAddress, 6);
    peer.channel = 0;
    peer.encrypt = false;
    if (esp_now_add_peer(&peer) != ESP_OK) {
      Serial.println("[ESP-NOW] Error: Could not add relay peer.");
      return;
    }
    relayPeerAdded = true;
  }
  RelayMessage message{};
  strncpy(message.token, config.pairToken, sizeof(message.token) - 1);
  message.type = 1;
  message.pumpOn = pumpRequested ? 1 : 0;
  message.sequence = ++sequence;
  esp_err_t res = esp_now_send(relayAddress, reinterpret_cast<const uint8_t*>(&message), sizeof(message));
  if (res == ESP_OK) {
    lastRelaySendAt = millis();
    Serial.printf("[ESP-NOW] Sent cmd: pumpOn=%d (seq=%u)\n", message.pumpOn, message.sequence);
  } else {
    Serial.printf("[ESP-NOW] Send failed: err=%d\n", res);
  }
}

float readDistanceCm() {
  digitalWrite(TRIGGER_PIN, LOW);
  delayMicroseconds(4);
  digitalWrite(TRIGGER_PIN, HIGH);
  delayMicroseconds(12);
  digitalWrite(TRIGGER_PIN, LOW);
  unsigned long duration = pulseIn(ECHO_PIN, HIGH, 35000);
  if (duration == 0) {
    Serial.printf("[SENSOR] No Echo detected! Check wiring: TRIG=GPIO%d, ECHO=GPIO%d, VCC=5V, GND=GND\n", TRIGGER_PIN, ECHO_PIN);
    return NAN;
  }
  float dist = (duration * 0.0343f) / 2.0f;
  return dist;
}

void sampleTank() {
  bool previousPumpRequest = pumpRequested;
  float reading = readDistanceCm();
  if (isnan(reading) || reading < 2.0f) {
    badSamples++;
    if (badSamples >= INVALID_SAMPLE_LIMIT) {
      distanceCm = NAN;
      levelPercent = NAN;
      pumpRequested = false;
      pumpStartedAt = 0;
      if (previousPumpRequest) sendRelayCommand();
    }
    return;
  }

  badSamples = 0;
  distanceCm = reading;

  float depth = config.tankDepthCm > 0 ? static_cast<float>(config.tankDepthCm) : 120.0f;
  float effectiveDist = distanceCm - config.sensorOffsetCm;
  levelPercent = ((depth - effectiveDist) * 100.0f) / depth;
  levelPercent = constrain(levelPercent, 0.0f, 100.0f);

  Serial.printf("[SENSOR] Distance: %.1f cm (Offset: %d cm, Depth: %.0f cm) -> Water Level: %.1f%%\n",
                distanceCm, config.sensorOffsetCm, depth, levelPercent);

  // 1. Auto mode start logic
  if (mode == "auto") {
    if (reading <= depth + 50.0f) {
      if (levelPercent <= static_cast<float>(config.startPercent)) {
        if (!pumpRequested) {
          Serial.printf("[PUMP] Water level low (%.1f%% <= %d%%). Starting pump.\n", levelPercent, config.startPercent);
          pumpRequested = true;
          pumpStartedAt = millis();
        }
      }
    } else {
      if (pumpRequested) {
        Serial.printf("[PUMP] Reading out of tank range (%.1f cm). Stopping pump.\n", reading);
        pumpRequested = false;
        pumpStartedAt = 0;
      }
    }
  }

  // 2. Critical Safety & Target Cutoff:
  // If water level reaches or exceeds stopPercent (e.g. 80%), ALWAYS stop the pump!
  // This applies in BOTH auto mode and manual mode (if someone clicked "Run Pump")!
  if (levelPercent >= static_cast<float>(config.stopPercent)) {
    if (pumpRequested) {
      Serial.printf("[PUMP] Stop threshold reached (%.1f%% >= %d%%). Stopping pump.\n", levelPercent, config.stopPercent);
      pumpRequested = false;
      pumpStartedAt = 0;
      if (mode == "on") {
        mode = "auto"; // Reached target, reset to automatic standby
      }
    }
  }

  // 3. Off mode enforcement
  if (mode == "off") {
    pumpRequested = false;
    pumpStartedAt = 0;
  }

  if (pumpRequested != previousPumpRequest) {
    sendRelayCommand();
  }
}
void maintainWifi() {
  if (WiFi.status() == WL_CONNECTED) {
    if (!networkReady) {
      networkReady = true;
      Serial.println("\n[WIFI] Connected to home Wi-Fi network!");
      Serial.print("  IP Address:     http://"); Serial.println(WiFi.localIP());
      Serial.print("  mDNS Address:   http://eyum.local (or http://eyum-water.local)\n");
      Serial.print("  Controller MAC: "); Serial.println(WiFi.macAddress());
      Serial.println("Open the IP address above in your browser to access the dashboard.\n");
      MDNS.begin(MDNS_NAME);
      MDNS.addService("http", "tcp", 80);
      startEspNow();
    }
    return;
  }
  networkReady = false;
  if (millis() - wifiStartedAt > 15000) {
    Serial.print("[WIFI] Still connecting to "); Serial.print(config.ssid); Serial.println("...");
    WiFi.disconnect();
    WiFi.begin(config.ssid, config.password);
    wifiStartedAt = millis();
  }
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n\n========================================");
  Serial.println("  Eyum Water Automation Tank Controller ");
  Serial.println("========================================");
  pinMode(TRIGGER_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(SETUP_BUTTON_PIN, INPUT_PULLUP);
  loadConfig();
  if (strlen(config.ssid) == 0) {
    Serial.println("[SETUP] No saved Wi-Fi configuration found.");
    startProvisioning();
    return;
  }
  Serial.print("[WIFI] Saved Wi-Fi SSID found: "); Serial.println(config.ssid);
  Serial.println("[WIFI] Connecting to Wi-Fi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(config.ssid, config.password);
  wifiStartedAt = millis();
  setupServer();
}
void loop() {
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd.startsWith("SET_WIFI:")) {
      String rest = cmd.substring(9);
      int comma = rest.indexOf(',');
      if (comma != -1) {
        String newSsid = rest.substring(0, comma);
        String newPass = rest.substring(comma + 1);
        newSsid.trim(); newPass.trim();
        Serial.printf("\n[WIFI] Saving new Wi-Fi: '%s' and restarting...\n", newSsid.c_str());
        copyValue(config.ssid, sizeof(config.ssid), newSsid);
        copyValue(config.password, sizeof(config.password), newPass);
        saveConfig();
        delay(500);
        ESP.restart();
      } else {
        Serial.println("[ERROR] Format must be: SET_WIFI:YourSSID,YourPassword");
      }
    } else if (cmd.startsWith("SET_DEPTH:")) {
      int d = cmd.substring(10).toInt();
      if (d >= 10 && d <= 1000) {
        config.tankDepthCm = d;
        saveConfig();
        sampleTank();
        Serial.printf("[CONFIG] Tank depth updated to %d cm.\n", d);
      } else {
        Serial.println("[ERROR] Depth must be between 10 and 1000 cm.");
      }
    } else if (cmd.startsWith("SET_THRESHOLDS:")) {
      String rest = cmd.substring(15);
      int comma = rest.indexOf(',');
      if (comma != -1) {
        int st = rest.substring(0, comma).toInt();
        int sp = rest.substring(comma + 1).toInt();
        if (st >= 0 && sp <= 100 && st < sp) {
          config.startPercent = st;
          config.stopPercent = sp;
          saveConfig();
          sampleTank();
          Serial.printf("[CONFIG] Thresholds updated: Start <= %d%%, Stop >= %d%%\n", st, sp);
        } else {
          Serial.println("[ERROR] Invalid thresholds. Format: SET_THRESHOLDS:25,80 (start < stop)");
        }
      }
    } else if (cmd.startsWith("SET_OFFSET:")) {
      config.sensorOffsetCm = cmd.substring(11).toInt();
      saveConfig();
      sampleTank();
      Serial.printf("[CONFIG] Sensor offset updated to %d cm.\n", config.sensorOffsetCm);
    } else if (cmd.equalsIgnoreCase("RESET")) {
      preferences.begin("eyum-water", false); preferences.clear(); preferences.end();
      Serial.println("\n[SETUP] Settings erased! Restarting into setup hotspot mode...");
      delay(500);
      ESP.restart();
    } else if (cmd.equalsIgnoreCase("STATUS") || cmd.equalsIgnoreCase("IP")) {
      Serial.printf("[STATUS] IP: http://%s | Distance: %.1f cm | Water: %.1f%% | Depth: %d cm | Start: %d%% | Stop: %d%% | Pump: %s | Relay: %s\n",
                    WiFi.localIP().toString().c_str(), distanceCm, levelPercent, config.tankDepthCm, config.startPercent, config.stopPercent, pumpRequested?"ON":"OFF", relayReportedOn?"ON":"OFF");
    }
  }

  if (provisioning) { dnsServer.processNextRequest(); server.handleClient(); return; }
  server.handleClient();
  maintainWifi();
  if (millis() - lastSampleAt >= SAMPLE_INTERVAL_MS) { lastSampleAt = millis(); sampleTank(); }
  if (pumpRequested && pumpStartedAt && millis() - pumpStartedAt > static_cast<unsigned long>(config.maxRunMinutes) * 60000UL) {
    pumpRequested = false; pumpStartedAt = 0; mode = "off"; sendRelayCommand();
  }
  if (espNowReady && millis() - lastRelaySendAt >= RELAY_HEARTBEAT_MS) sendRelayCommand();
  static unsigned long buttonDownAt = 0;
  if (!digitalRead(SETUP_BUTTON_PIN)) {
    if (!buttonDownAt) buttonDownAt = millis();
    if (millis() - buttonDownAt > 8000) {
      preferences.begin("eyum-water", false); preferences.clear(); preferences.end(); ESP.restart();
    }
  } else buttonDownAt = 0;
}
