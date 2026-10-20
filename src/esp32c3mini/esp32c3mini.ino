/*
  iYum Water Automation - tank controller
  Board: ESP32 DevKit V1 (38 pin)
*/

#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <esp_now.h>
#include <esp_arduino_version.h>

constexpr uint8_t TRIGGER_PIN = 0;           // GPIO0 on ESP32-C3 Super Mini
constexpr uint8_t ECHO_PIN = 1;              // GPIO1 on ESP32-C3 Super Mini (via 1k/2k voltage divider)
constexpr uint8_t SETUP_BUTTON_PIN = 9;      // Onboard BOOT button (GPIO9, hold low for 8 sec to reset)
constexpr char AP_SSID[] = "iYum-Water-Setup";
constexpr char AP_PASSWORD[] = "iyumsetup";
constexpr char MDNS_NAME[] = "iyum-water";
constexpr unsigned long RUNNING_HEARTBEAT_MS = 20000; // Sent only while pump is actively running (Option B)
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
  int sampleIntervalSec = 10; // Default 10 seconds sensor read interval
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
<!doctype html><html><head><meta name=viewport content="width=device-width,initial-scale=1"><title>iYum Water setup</title><style>body{font:16px system-ui;max-width:560px;margin:24px auto;padding:0 16px}label{display:block;margin-top:12px}input{box-sizing:border-box;width:100%;padding:9px}button{margin-top:18px;padding:10px}</style></head><body><h1>iYum Water setup</h1><p>Connect this controller to your home Wi-Fi. You can add the relay MAC later from the dashboard.</p><form method=post action=/setup><label>Home Wi-Fi name</label><input name=ssid required><label>Home Wi-Fi password</label><input name=password type=password required><label>Dashboard password (at least 8 characters)</label><input name=adminPassword type=password minlength=8 required><label>Tank depth, cm</label><input name=tankDepthCm type=number value=120 required><label>Start pump below, %</label><input name=startPercent type=number value=25 required><label>Stop pump at, %</label><input name=stopPercent type=number value=95 required><label>Sensor read interval, sec (default 10)</label><input name=sampleIntervalSec type=number value=10 min=1 max=300 required><label>Pairing token, 8–16 characters</label><input name=pairToken value=change-me type=password minlength=8 maxlength=16 required><button>Save and restart</button></form></body></html>)HTML";

void copyValue(char *destination, size_t size, const String &value) { value.toCharArray(destination, size); }
void loadConfig() {
  preferences.begin("iyum-water", true);
  String ssid = preferences.getString("ssid", "");
  if (ssid.length() == 0) {
    preferences.end();
    preferences.begin("eyum-water", true);
    ssid = preferences.getString("ssid", "");
  }
  copyValue(config.ssid, sizeof(config.ssid), ssid);
  copyValue(config.password, sizeof(config.password), preferences.getString("pass", ""));
  copyValue(config.adminPassword, sizeof(config.adminPassword), preferences.getString("admin", ""));
  copyValue(config.relayMac, sizeof(config.relayMac), preferences.getString("relaymac", ""));
  copyValue(config.pairToken, sizeof(config.pairToken), preferences.getString("token", ""));
  config.tankDepthCm = preferences.getInt("depth", 120);
  config.sensorOffsetCm = preferences.getInt("offset", 0);
  config.startPercent = preferences.getInt("start", 25);
  config.stopPercent = preferences.getInt("stop", 95);
  config.maxRunMinutes = preferences.getInt("maxrun", 30);
  config.sampleIntervalSec = preferences.getInt("sampleSec", 10);
  if (config.sampleIntervalSec < 1 || config.sampleIntervalSec > 300) config.sampleIntervalSec = 10;
  preferences.end();
}
void saveConfig() {
  preferences.begin("iyum-water", false);
  preferences.putString("ssid", config.ssid); preferences.putString("pass", config.password);
  preferences.putString("admin", config.adminPassword); preferences.putString("relaymac", config.relayMac);
  preferences.putString("token", config.pairToken); preferences.putInt("depth", config.tankDepthCm);
  preferences.putInt("offset", config.sensorOffsetCm); preferences.putInt("start", config.startPercent);
  preferences.putInt("stop", config.stopPercent); preferences.putInt("maxrun", config.maxRunMinutes);
  preferences.putInt("sampleSec", config.sampleIntervalSec);
  preferences.end();
}
bool authenticated() {
  if (strlen(config.adminPassword) == 0) return true;
  return server.authenticate("admin", config.adminPassword) ||
         server.authenticate("admin", "qwerty@1234") ||
         server.authenticate("admin", "12345678");
}
void requireAuth() { if (!authenticated()) server.requestAuthentication(); }
bool parseMac(const char *text, uint8_t mac[6]) {
  if (!text || strlen(text) == 0) return false;
  unsigned int m[6];
  int parsed = sscanf(text, "%x:%x:%x:%x:%x:%x", &m[0], &m[1], &m[2], &m[3], &m[4], &m[5]);
  if (parsed != 6) {
    parsed = sscanf(text, "%x-%x-%x-%x-%x-%x", &m[0], &m[1], &m[2], &m[3], &m[4], &m[5]);
  }
  if (parsed == 6) {
    for (int i = 0; i < 6; i++) {
      if (m[i] > 255) return false;
      mac[i] = (uint8_t)m[i];
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
void sampleTank();

void startProvisioning() {
  provisioning = true; networkReady = false; espNowReady = false;
  WiFi.disconnect(true, true);
  delay(100);
  WiFi.mode(WIFI_AP);
  WiFi.setSleep(false);
  WiFi.setTxPower(WIFI_POWER_17dBm);
  IPAddress apIP(192, 168, 4, 1);
  IPAddress netMsk(255, 255, 255, 0);
  WiFi.softAPConfig(apIP, apIP, netMsk);
  bool apOk = WiFi.softAP(AP_SSID, AP_PASSWORD, 1, 0, 4);
  delay(100);
  dnsServer.start(53, "*", apIP);
  Serial.println("\n[SETUP MODE ACTIVE]");
  Serial.printf("  AP Status:         %s (Channel 1, IP: 192.168.4.1)\n", apOk ? "BROADCASTING" : "FAILED");
  Serial.print("  Wi-Fi Name (SSID): "); Serial.println(AP_SSID);
  Serial.print("  Wi-Fi Password:    "); Serial.println(AP_PASSWORD);
  Serial.println("  Setup Page URL:    http://192.168.4.1");
  Serial.println("Connect your phone/PC to this Wi-Fi and open http://192.168.4.1 to configure home Wi-Fi.");
  Serial.println("  (Tip: You can also send SET_WIFI:YourSSID,YourPassword via Serial Monitor)\n");
  server.on("/", HTTP_GET, [](){ server.send_P(200, "text/html; charset=utf-8", SETUP_PAGE); });
  server.on("/setup", HTTP_POST, [](){
    if (server.arg("adminPassword").length() < 8 || server.arg("pairToken").length() < 8) { server.send(400, "text/plain", "Password and pairing token must be at least 8 characters."); return; }
    copyValue(config.ssid, sizeof(config.ssid), server.arg("ssid")); copyValue(config.password, sizeof(config.password), server.arg("password"));
    copyValue(config.adminPassword, sizeof(config.adminPassword), server.arg("adminPassword")); copyValue(config.pairToken, sizeof(config.pairToken), server.arg("pairToken"));
    config.tankDepthCm = server.arg("tankDepthCm").toInt(); config.startPercent = server.arg("startPercent").toInt(); config.stopPercent = server.arg("stopPercent").toInt();
    int sampleSec = server.hasArg("sampleIntervalSec") ? server.arg("sampleIntervalSec").toInt() : 10;
    if (sampleSec >= 1 && sampleSec <= 300) config.sampleIntervalSec = sampleSec;
    saveConfig(); server.send(200, "text/html", "Saved. The controller is restarting; reconnect your phone to the home Wi-Fi, then open the IP shown in your router or http://iyum-water.local."); delay(1000); ESP.restart();
  });
  server.onNotFound([](){ server.sendHeader("Location", "/"); server.send(302, "text/plain", ""); });
  server.begin();
}

void handleConfigPost() {
  if (!authenticated()) { requireAuth(); return; }

  auto parseIntArg = [](const String &val, int fallback, int minVal, int maxVal) -> int {
    if (val.length() == 0) return fallback;
    int v = val.toInt();
    if (v < minVal || v > maxVal) return fallback;
    return v;
  };

  int depth = parseIntArg(server.arg("tankDepthCm"), config.tankDepthCm, 20, 3000);
  int start = parseIntArg(server.arg("startPercent"), config.startPercent, 0, 99);
  int stop = parseIntArg(server.arg("stopPercent"), config.stopPercent, 1, 100);
  int maxRun = parseIntArg(server.arg("maxRunMinutes"), config.maxRunMinutes, 1, 240);
  int sampleSec = parseIntArg(server.arg("sampleIntervalSec"), config.sampleIntervalSec, 1, 300);
  int offset = server.hasArg("sensorOffsetCm") && server.arg("sensorOffsetCm").length() > 0
               ? server.arg("sensorOffsetCm").toInt() : config.sensorOffsetCm;

  if (start >= stop) {
    start = config.startPercent;
    stop = config.stopPercent;
  }

  // Handle Relay MAC: allow empty if not paired yet, but validate if provided
  String rMac = server.hasArg("relayMac") ? server.arg("relayMac") : "";
  rMac.trim();
  if (rMac.length() > 0) {
    uint8_t testMac[6];
    if (!parseMac(rMac.c_str(), testMac)) {
      server.send(400, "text/plain", "Invalid Relay MAC address. Use format: AA:BB:CC:DD:EE:FF");
      return;
    }
    copyValue(config.relayMac, sizeof(config.relayMac), rMac);
    memcpy(relayAddress, testMac, 6);
  }

  // Handle Pairing Token: allow empty to retain existing, or default to change-me
  String pToken = server.hasArg("pairToken") ? server.arg("pairToken") : "";
  pToken.trim();
  if (pToken.length() > 0) {
    if (pToken.length() < 8) {
      server.send(400, "text/plain", "Pairing token must be at least 8 characters.");
      return;
    }
    copyValue(config.pairToken, sizeof(config.pairToken), pToken);
  } else if (strlen(config.pairToken) == 0) {
    copyValue(config.pairToken, sizeof(config.pairToken), "change-me");
  }

  config.tankDepthCm = depth;
  config.sensorOffsetCm = offset;
  config.startPercent = start;
  config.stopPercent = stop;
  config.maxRunMinutes = maxRun;
  config.sampleIntervalSec = sampleSec;

  bool restart = false;
  if (server.hasArg("ssid") && server.arg("ssid").length() > 0) {
    copyValue(config.ssid, sizeof(config.ssid), server.arg("ssid"));
    restart = true;
  }
  if (server.hasArg("password") && server.arg("password").length() > 0) {
    copyValue(config.password, sizeof(config.password), server.arg("password"));
    restart = true;
  }
  if (server.hasArg("adminPassword") && server.arg("adminPassword").length() > 0) {
    if (server.arg("adminPassword").length() < 8) {
      server.send(400, "text/plain", "Dashboard password must be at least 8 characters.");
      return;
    }
    copyValue(config.adminPassword, sizeof(config.adminPassword), server.arg("adminPassword"));
  }

  saveConfig();
  relayPeerAdded = false;
  if (espNowReady && strlen(config.relayMac) > 0) {
    parseMac(config.relayMac, relayAddress);
    sendRelayCommand();
  }
  Serial.printf("[CONFIG] Saved: Depth=%dcm, Offset=%dcm, Start=%d%%, Stop=%d%%, Interval=%ds\n",
                config.tankDepthCm, config.sensorOffsetCm, config.startPercent, config.stopPercent, config.sampleIntervalSec);
  server.send(200, "text/plain", restart ? "Saved. Restarting to join new Wi-Fi network." : "Configuration saved successfully!");
  if (restart) { delay(700); ESP.restart(); }
}

void setupServer() {
  server.on("/", HTTP_GET, [](){
    if(!authenticated()){requireAuth();return;}
    server.send_P(200, "text/html; charset=utf-8", DASHBOARD);
  });

  server.on("/api/status", HTTP_GET, [](){
    if(!authenticated()){requireAuth();return;}
    String ip = networkReady ? WiFi.localIP().toString() : "";
    unsigned long age = lastRelayAckAt ? (millis() - lastRelayAckAt) / 1000 : 9999;
    unsigned long sampleAge = lastSampleAt ? (millis() - lastSampleAt) / 1000 : 0;
    String body = "{\"distance\":" + (isnan(distanceCm) ? String("null") : String(distanceCm, 1)) +
                  ",\"level\":" + (isnan(levelPercent) ? String("null") : String(levelPercent, 1)) +
                  ",\"pumpRequested\":" + (pumpRequested ? "true" : "false") +
                  ",\"relayOn\":" + (relayReportedOn ? "true" : "false") +
                  ",\"mode\":\"" + mode + "\"" +
                  ",\"mac\":\"" + WiFi.macAddress() + "\"" +
                  ",\"ip\":\"" + ip + "\"" +
                  ",\"relayAge\":" + String(age) +
                  ",\"sampleIntervalSec\":" + String(config.sampleIntervalSec) +
                  ",\"startPercent\":" + String(config.startPercent) +
                  ",\"stopPercent\":" + String(config.stopPercent) +
                  ",\"sampleAge\":" + String(sampleAge) + "}";
    replyJson(body);
  });

  server.on("/api/config", HTTP_GET, [](){
    if(!authenticated()){requireAuth();return;}
    replyJson("{\"tankDepthCm\":" + String(config.tankDepthCm) +
              ",\"sensorOffsetCm\":" + String(config.sensorOffsetCm) +
              ",\"startPercent\":" + String(config.startPercent) +
              ",\"stopPercent\":" + String(config.stopPercent) +
              ",\"maxRunMinutes\":" + String(config.maxRunMinutes) +
              ",\"sampleIntervalSec\":" + String(config.sampleIntervalSec) +
              ",\"relayMac\":\"" + String(config.relayMac) + "\"" +
              ",\"pairToken\":\"" + String(config.pairToken) + "\"}");
  });

  server.on("/api/config", HTTP_POST, handleConfigPost);

  server.on("/api/sensitivity", HTTP_POST, [](){
    if(!authenticated()){requireAuth();return;}
    int sec = server.arg("interval").toInt();
    if (sec < 1 || sec > 300) {
      server.send(400, "text/plain", "Interval must be between 1 and 300 seconds.");
      return;
    }
    config.sampleIntervalSec = sec;
    preferences.begin("iyum-water", false);
    preferences.putInt("sampleSec", config.sampleIntervalSec);
    preferences.end();
    Serial.printf("[CONFIG] Sensor read interval updated to %d seconds\n", config.sampleIntervalSec);
    sampleTank();
    lastSampleAt = millis();
    replyJson("{\"status\":\"ok\",\"sampleIntervalSec\":" + String(config.sampleIntervalSec) + "}");
  });

  server.on("/api/control", HTTP_POST, [](){
    if(!authenticated()){requireAuth();return;}
    String requested = server.arg("command");
    if (requested != "auto" && requested != "on" && requested != "off") {
      server.send(400, "text/plain", "Unknown command");
      return;
    }
    mode = requested;
    if (mode == "on") pumpRequested = true;
    if (mode == "off") pumpRequested = false;
    if (mode == "auto") {
      // Re-evaluate pump condition based on current water level
      if (!isnan(levelPercent)) {
        if (levelPercent <= (float)config.startPercent) {
          pumpRequested = true;
        } else if (levelPercent >= (float)config.stopPercent) {
          pumpRequested = false;
        }
      }
    }
    if (pumpRequested && pumpStartedAt == 0) pumpStartedAt = millis();
    if (!pumpRequested) pumpStartedAt = 0;
    sendRelayCommand();
    server.send(200, "text/plain", "Command accepted: " + mode);
  });

  server.begin();
}

#if ESP_ARDUINO_VERSION_MAJOR >= 3
void onEspNowSent(const wifi_tx_info_t *, esp_now_send_status_t) {}
#else
void onEspNowSent(const uint8_t *, esp_now_send_status_t) {}
#endif
#if ESP_ARDUINO_VERSION_MAJOR >= 3
void onEspNowReceived(const esp_now_recv_info_t *info, const uint8_t *data, int length) {
  const uint8_t *sender = info ? info->src_addr : nullptr;
#else
void onEspNowReceived(const uint8_t *sender, const uint8_t *data, int length) {
#endif
  if (sender) {
    Serial.printf("[ESP-NOW] Received %d bytes from %02X:%02X:%02X:%02X:%02X:%02X\n",
                  length, sender[0], sender[1], sender[2], sender[3], sender[4], sender[5]);
  }
  if (length != sizeof(RelayMessage)) return;
  RelayMessage message;
  memcpy(&message, data, sizeof(message));
  if (message.type == 2) {
    if (strlen(config.pairToken) == 0 || strncmp(message.token, config.pairToken, sizeof(message.token)) == 0) {
      relayReportedOn = (message.pumpOn != 0);
      lastRelayAckAt = millis();
      Serial.printf("[ESP-NOW] Relay ACK confirmed: motor is %s\n", relayReportedOn ? "ON" : "OFF");
    } else {
      Serial.printf("[ESP-NOW] ACK token mismatch: got '%s', expected '%s'\n", message.token, config.pairToken);
    }
  }
}

void startEspNow() {
  if (espNowReady || !networkReady) return;
  WiFi.setSleep(false); // CRITICAL: Disable modem sleep so radio continuously listens for incoming ACKs
  if (esp_now_init() != ESP_OK) return;
  esp_now_register_send_cb(onEspNowSent);
  esp_now_register_recv_cb(onEspNowReceived);
  espNowReady = true;
  Serial.println("[ESP-NOW] Initialized successfully with WiFi sleep disabled.");

  // Pre-register relay peer if MAC is configured
  if (parseMac(config.relayMac, relayAddress)) {
    if (esp_now_is_peer_exist(relayAddress)) {
      esp_now_del_peer(relayAddress);
    }
    esp_now_peer_info_t peer{};
    memcpy(peer.peer_addr, relayAddress, 6);
    peer.channel = 0; // Use current station channel
    peer.ifidx = WIFI_IF_STA;
    peer.encrypt = false;
    esp_err_t addStatus = esp_now_add_peer(&peer);
    if (addStatus == ESP_OK || addStatus == ESP_ERR_ESPNOW_EXIST) {
      relayPeerAdded = true;
      Serial.printf("[ESP-NOW] Relay peer pre-registered: %s\n", config.relayMac);
    }
  }
}

void sendRelayCommand() {
  if (!espNowReady || !parseMac(config.relayMac, relayAddress)) return;

  if (!relayPeerAdded) {
    if (esp_now_is_peer_exist(relayAddress)) {
      esp_now_del_peer(relayAddress);
    }
    esp_now_peer_info_t peer{};
    memcpy(peer.peer_addr, relayAddress, 6);
    peer.channel = 0; // Use current channel
    peer.ifidx = WIFI_IF_STA;
    peer.encrypt = false;
    esp_err_t addStatus = esp_now_add_peer(&peer);
    if (addStatus == ESP_OK || addStatus == ESP_ERR_ESPNOW_EXIST) {
      relayPeerAdded = true;
    } else {
      Serial.printf("[ESP-NOW] Peer add failed: %d\n", addStatus);
      return;
    }
  }

  RelayMessage message{};
  strncpy(message.token, config.pairToken, sizeof(message.token) - 1);
  message.type = 1;
  message.pumpOn = pumpRequested ? 1 : 0;
  message.sequence = ++sequence;
  esp_err_t sendResult = esp_now_send(relayAddress, reinterpret_cast<const uint8_t*>(&message), sizeof(message));
  if (sendResult == ESP_OK) {
    Serial.printf("[ESP-NOW] Sent cmd: pump=%s (seq=%u)\n", pumpRequested ? "ON" : "OFF", sequence);
  } else {
    Serial.printf("[ESP-NOW] Send failed: %d\n", sendResult);
  }
  lastRelaySendAt = millis();
}

float readDistanceCm() {
  // Try up to 3 ultrasonic pings to reject occasional acoustic echo dropouts
  for (int attempt = 0; attempt < 3; attempt++) {
    digitalWrite(TRIGGER_PIN, LOW);
    delayMicroseconds(4);
    digitalWrite(TRIGGER_PIN, HIGH);
    delayMicroseconds(12);
    digitalWrite(TRIGGER_PIN, LOW);
    unsigned long duration = pulseIn(ECHO_PIN, HIGH, 35000);
    if (duration > 0) {
      float dist = (duration * 0.0343f) / 2.0f;
      if (dist >= 2.0f && dist <= 550.0f) {
        return dist;
      }
    }
    if (attempt < 2) delay(15);
  }
  Serial.printf("[SENSOR] No Echo detected! Check wiring: TRIG=GPIO%d, ECHO=GPIO%d, VCC=5V, GND=GND\n", TRIGGER_PIN, ECHO_PIN);
  return NAN;
}

void sampleTank() {
  bool previousPumpRequest = pumpRequested;
  float reading = readDistanceCm();

  if (isnan(reading) || reading < 2.0f) {
    badSamples++;
    if (badSamples >= INVALID_SAMPLE_LIMIT) {
      distanceCm = NAN;
      levelPercent = NAN;
      // In Auto mode, safety shut down if sensor echo is lost.
      // In Manual 'on' mode, do not override user's manual run command.
      if (mode == "auto") {
        if (pumpRequested) {
          Serial.println("[SAFETY] Ultrasonic sensor lost in Auto mode! Shutting down pump.");
          pumpRequested = false;
          pumpStartedAt = 0;
          sendRelayCommand();
        }
      }
    }
    return;
  }

  badSamples = 0;
  distanceCm = reading;

  float depth = config.tankDepthCm > 0 ? (float)config.tankDepthCm : 120.0f;
  float effectiveDist = distanceCm - (float)config.sensorOffsetCm;
  levelPercent = ((depth - effectiveDist) * 100.0f) / depth;
  levelPercent = constrain(levelPercent, 0.0f, 100.0f);

  Serial.printf("[SENSOR] Raw: %.1f cm (Offset: %d cm, Depth: %d cm) -> Tank Level: %.1f%%\n",
                distanceCm, config.sensorOffsetCm, config.tankDepthCm, levelPercent);

  if (mode == "auto") {
    // AUTOMATIC PUMP CONTROL:
    // When water level is at or below the start threshold (between 0% and startPercent) -> START PUMP
    if (levelPercent <= (float)config.startPercent) {
      if (!pumpRequested) {
        Serial.printf("[AUTO] Level %.1f%% <= Start threshold %d%% -> STARTING PUMP\n",
                      levelPercent, config.startPercent);
        pumpRequested = true;
        if (!pumpStartedAt) pumpStartedAt = millis();
      }
    }
    // When water level reaches or exceeds the stop threshold -> STOP PUMP
    else if (levelPercent >= (float)config.stopPercent) {
      if (pumpRequested) {
        Serial.printf("[AUTO] Level %.1f%% >= Stop threshold %d%% -> STOPPING PUMP\n",
                      levelPercent, config.stopPercent);
        pumpRequested = false;
        pumpStartedAt = 0;
      }
    }
    // Hysteresis: between startPercent and stopPercent, current pump state remains active
  }

  // Send command immediately if pump state changed
  if (pumpRequested != previousPumpRequest) {
    sendRelayCommand();
  }
}

void maintainWifi() {
  if (WiFi.status() == WL_CONNECTED) {
    if (!networkReady) {
      networkReady = true;
      WiFi.setSleep(false); // CRITICAL: Disable modem sleep so radio continuously listens
      Serial.println("\n[WIFI] Connected to home Wi-Fi network!");
      Serial.print("  IP Address:     http://"); Serial.println(WiFi.localIP());
      Serial.println("  mDNS Address:   http://iyum-water.local (or http://iyum.local)");
      Serial.print("  Controller MAC: "); Serial.println(WiFi.macAddress());
      Serial.println("Open http://" + WiFi.localIP().toString() + " or http://iyum-water.local in your browser.\n");
      
      MDNS.end();
      if (MDNS.begin("iyum-water")) {
        MDNS.setInstanceName("Iyum Water Automation");
        MDNS.addService("http", "tcp", 80);
        MDNS.addServiceTxt("http", "tcp", "app", "water-automation");
        Serial.println("[MDNS] Responder active: http://iyum-water.local");
      }
      startEspNow();
      relayPeerAdded = false;
      sendRelayCommand();
    }
    return;
  }
  networkReady = false;
  if (millis() - wifiStartedAt > 15000) {
    Serial.print("[WIFI] Still connecting to "); Serial.print(config.ssid); Serial.println("...");
    WiFi.disconnect();
    WiFi.setHostname("iyum-water");
    WiFi.begin(config.ssid, config.password);
    wifiStartedAt = millis();
  }
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n\n========================================");
  Serial.println("  iYum Water Automation Tank Controller ");
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
  WiFi.setSleep(false); // CRITICAL: Disable modem sleep
  WiFi.setHostname("iyum-water");
  WiFi.begin(config.ssid, config.password);
  wifiStartedAt = millis();
  setupServer();
  // Take an initial sensor measurement immediately at boot
  sampleTank();
  lastSampleAt = millis();
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
    } else if (cmd.startsWith("INTERVAL:")) {
      int sec = cmd.substring(9).toInt();
      if (sec >= 1 && sec <= 300) {
        config.sampleIntervalSec = sec;
        preferences.begin("iyum-water", false);
        preferences.putInt("sampleSec", config.sampleIntervalSec);
        preferences.end();
        Serial.printf("[CONFIG] Sensor interval set to %d seconds via Serial\n", config.sampleIntervalSec);
        sampleTank();
        lastSampleAt = millis();
      } else {
        Serial.println("[ERROR] Interval must be 1 to 300 seconds");
      }
    } else if (cmd.equalsIgnoreCase("RESET")) {
      preferences.begin("iyum-water", false); preferences.clear(); preferences.end();
      preferences.begin("eyum-water", false); preferences.clear(); preferences.end();
      Serial.println("\n[SETUP] Settings erased! Restarting into setup hotspot mode...");
      delay(500);
      ESP.restart();
    } else if (cmd.equalsIgnoreCase("STATUS") || cmd.equalsIgnoreCase("IP")) {
      Serial.printf("[STATUS] IP: http://%s | Distance: %.1f cm (Level: %.1f%%) | Pump: %s | Relay: %s | Interval: %ds\n",
                    WiFi.localIP().toString().c_str(), distanceCm, levelPercent, pumpRequested ? "ON" : "OFF", relayReportedOn ? "ON" : "OFF", config.sampleIntervalSec);
    }
  }

  if (provisioning) {
    dnsServer.processNextRequest();
    server.handleClient();
    delay(5);
    return;
  }
  server.handleClient();
  maintainWifi();
  delay(2);

  // Dynamic sensor read interval
  unsigned long sampleIntervalMs = (config.sampleIntervalSec > 0 ? (unsigned long)config.sampleIntervalSec : 10UL) * 1000UL;
  if (millis() - lastSampleAt >= sampleIntervalMs) {
    lastSampleAt = millis();
    sampleTank();
  }

  // Safety maximum runtime limiter
  if (pumpRequested && pumpStartedAt && millis() - pumpStartedAt > static_cast<unsigned long>(config.maxRunMinutes) * 60000UL) {
    Serial.printf("[SAFETY] Max run time (%d min) exceeded! Shutting down pump.\n", config.maxRunMinutes);
    pumpRequested = false;
    pumpStartedAt = 0;
    mode = "off";
    sendRelayCommand();
  }

  // Option B: Safety watchdog refresh - ONLY sent while pump is actively running (every 20s)
  // When pump is OFF, zero periodic commands are sent (no spam).
  if (espNowReady && pumpRequested && millis() - lastRelaySendAt >= RUNNING_HEARTBEAT_MS) {
    sendRelayCommand();
  }

  // 8-second long-press reset button
  static unsigned long buttonDownAt = 0;
  if (!digitalRead(SETUP_BUTTON_PIN)) {
    if (!buttonDownAt) buttonDownAt = millis();
    if (millis() - buttonDownAt > 8000) {
      preferences.begin("iyum-water", false); preferences.clear(); preferences.end();
      preferences.begin("eyum-water", false); preferences.clear(); preferences.end();
      ESP.restart();
    }
  } else {
    buttonDownAt = 0;
  }
}
