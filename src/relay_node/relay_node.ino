/*
  iYum Water Automation - ESP-01 relay node
  Compatible with ESP8266 ESP-01 relay modules that use GPIO0 as relay input.
*/

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <EEPROM.h>
#include <espnow.h>

constexpr uint8_t RELAY_PIN = 0;
constexpr bool RELAY_ACTIVE_LOW = true; // change to false for an active-high board
constexpr char AP_SSID[] = "iYum-Relay-Setup";
constexpr char AP_PASSWORD[] = "iyumsetup";
constexpr unsigned long TOKEN_MISMATCH_TIMEOUT_MS = 60000;

struct Config {
  char marker[5];
  char ssid[33];
  char password[65];
  char controllerMac[18];
  char pairToken[17];
};

struct __attribute__((packed)) RelayMessage {
  char token[17];
  uint8_t type;
  uint8_t pumpOn;
  uint32_t sequence;
};

Config config{};
ESP8266WebServer server(80);
DNSServer dnsServer;
bool provisioning = false;
bool espNowReady = false;
bool relayOn = false;
uint8_t controllerAddress[6]{};
unsigned long lastCommandAt = 0;
unsigned long tokenMismatchStartedAt = 0;

const char SETUP_PAGE[] PROGMEM = R"HTML(
<!doctype html><html><head><meta name=viewport content="width=device-width,initial-scale=1"><title>iYum relay setup</title><style>body{font:16px system-ui;max-width:560px;margin:24px auto;padding:0 16px}label{display:block;margin-top:12px}input{box-sizing:border-box;width:100%;padding:9px}button{margin-top:18px;padding:10px}</style></head><body><h1>iYum relay setup</h1><p>Relay MAC: <b>%MAC%</b></p><form method=post action=/setup><label>Home Wi-Fi name</label><input name=ssid required><label>Home Wi-Fi password</label><input name=password type=password required><label>ESP32 controller MAC address</label><input name=controllerMac placeholder="AA:BB:CC:DD:EE:FF" required><label>Pairing token (same one configured on controller)</label><input name=pairToken type=password minlength=8 maxlength=16 required><button>Save and restart</button></form></body></html>)HTML";

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
      mac[i] = static_cast<uint8_t>(m[i]);
    }
    return true;
  }
  return false;
}

void copyValue(char *d, size_t n, const String &v) {
  v.toCharArray(d, n);
}

void loadConfig() {
  EEPROM.begin(sizeof(Config));
  EEPROM.get(0, config);
  EEPROM.end();
}

void saveConfig() {
  EEPROM.begin(sizeof(Config));
  EEPROM.put(0, config);
  EEPROM.commit();
  EEPROM.end();
}

void setRelay(bool on) {
  relayOn = on;
  digitalWrite(RELAY_PIN, (on ^ RELAY_ACTIVE_LOW) ? HIGH : LOW);
}

volatile bool pendingAck = false;

void sendStatus() {
  if (!espNowReady) return;
  RelayMessage message{};
  strncpy(message.token, config.pairToken, sizeof(message.token) - 1);
  message.type = 2;
  message.pumpOn = relayOn ? 1 : 0;
  message.sequence = 0;
  int res = esp_now_send(controllerAddress, reinterpret_cast<uint8_t*>(&message), sizeof(message));
  if (res == 0) {
    Serial.printf("[ESP-NOW] Sent ACK: pump=%s\n", relayOn ? "ON" : "OFF");
  } else {
    Serial.printf("[ESP-NOW] ACK send queued failed: %d\n", res);
  }
}

void onEspNowSent(uint8_t *mac, uint8_t status) {
  if (status == 0) {
    Serial.printf("[ESP-NOW] ACK frame delivered to controller (%02X:%02X:%02X:%02X:%02X:%02X)\n",
                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  } else {
    Serial.printf("[ESP-NOW] ACK delivery FAILED (no 802.11 ACK from controller %02X:%02X:%02X:%02X:%02X:%02X)\n",
                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  }
}

void receiveCallback(uint8_t *sender, uint8_t *data, uint8_t length) {
  if (length != sizeof(RelayMessage)) return;
  if (memcmp(sender, controllerAddress, 6) != 0) {
    Serial.printf("[ESP-NOW] MAC mismatch! Received from %02X:%02X:%02X:%02X:%02X:%02X, expected %02X:%02X:%02X:%02X:%02X:%02X\n",
                  sender[0], sender[1], sender[2], sender[3], sender[4], sender[5],
                  controllerAddress[0], controllerAddress[1], controllerAddress[2],
                  controllerAddress[3], controllerAddress[4], controllerAddress[5]);
    return;
  }
  RelayMessage msg;
  memcpy(&msg, data, sizeof(msg));
  if (msg.type != 1) return;

  if (strncmp(msg.token, config.pairToken, sizeof(msg.token)) != 0) {
    Serial.println("[ESP-NOW] Token mismatch!");
    if (tokenMismatchStartedAt == 0) {
      tokenMismatchStartedAt = millis();
    }
    if (millis() - tokenMismatchStartedAt >= TOKEN_MISMATCH_TIMEOUT_MS) {
      Serial.println("[SAFETY] Token mismatch persisted for >1 minute! Forcing relay OFF.");
      setRelay(false);
    }
    return;
  }

  // Valid token received: reset mismatch timer
  tokenMismatchStartedAt = 0;
  setRelay(msg.pumpOn != 0);
  lastCommandAt = millis();
  Serial.printf("[ESP-NOW] Received cmd: pump=%s (seq=%u)\n", msg.pumpOn ? "ON" : "OFF", msg.sequence);
  pendingAck = true; // Defer send to main loop to avoid SDK re-entrancy failure
}

void updateEspNowPeer() {
  if (!espNowReady) return;
  uint8_t channel = WiFi.status() == WL_CONNECTED ? WiFi.channel() : 1;
  esp_now_del_peer(controllerAddress);
  int res = esp_now_add_peer(controllerAddress, ESP_NOW_ROLE_COMBO, channel, nullptr, 0);
  Serial.printf("[ESP-NOW] Controller registered on channel %d (status=%d)\n", channel, res);
}

void startEspNow() {
  if (espNowReady) return;
  if (esp_now_init() != 0) {
    Serial.println("[ESP-NOW] Init failed!");
    return;
  }
  esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
  esp_now_register_send_cb(onEspNowSent);
  esp_now_register_recv_cb(receiveCallback);
  espNowReady = true;
  updateEspNowPeer();
  Serial.println("[ESP-NOW] Initialized successfully.");
  sendStatus();
}

void startProvisioning() {
  provisioning = true;
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  dnsServer.start(53, "*", WiFi.softAPIP());
  Serial.printf("[SETUP] Provisioning AP active: %s, IP: %s\n", AP_SSID, WiFi.softAPIP().toString().c_str());

  server.on("/", HTTP_GET, []() {
    String page = FPSTR(SETUP_PAGE);
    page.replace("%MAC%", WiFi.macAddress());
    server.send(200, "text/html", page);
  });

  server.on("/setup", HTTP_POST, []() {
    if (!parseMac(server.arg("controllerMac").c_str(), controllerAddress) || server.arg("pairToken").length() < 8) {
      server.send(400, "text/plain", "Enter a valid controller MAC and an 8-character pairing token.");
      return;
    }
    memset(&config, 0, sizeof(config));
    strcpy(config.marker, "IYUM");
    copyValue(config.ssid, sizeof(config.ssid), server.arg("ssid"));
    copyValue(config.password, sizeof(config.password), server.arg("password"));
    copyValue(config.controllerMac, sizeof(config.controllerMac), server.arg("controllerMac"));
    copyValue(config.pairToken, sizeof(config.pairToken), server.arg("pairToken"));
    saveConfig();
    server.send(200, "text/plain", "Saved. The relay is restarting.");
    delay(700);
    ESP.restart();
  });

  server.onNotFound([]() {
    server.sendHeader("Location", "/");
    server.send(302, "text/plain", "");
  });

  server.begin();
}

void setup() {
  pinMode(RELAY_PIN, OUTPUT);
  setRelay(false);
  Serial.begin(115200);
  delay(300);
  Serial.println("\n\n========================================");
  Serial.println("  iYum Water Automation Relay Node     ");
  Serial.println("========================================");

  loadConfig();
  if (strcmp(config.marker, "IYUM") != 0 && strcmp(config.marker, "EYUM") != 0) {
    Serial.println("[CONFIG] No valid configuration found. Entering setup mode...");
    startProvisioning();
    return;
  }

  if (!parseMac(config.controllerMac, controllerAddress)) {
    Serial.printf("[CONFIG] Invalid controller MAC '%s'. Entering setup mode...\n", config.controllerMac);
    startProvisioning();
    return;
  }

  Serial.printf("[CONFIG] Controller MAC: %s\n", config.controllerMac);
  Serial.printf("[WIFI] Connecting to SSID: %s\n", config.ssid);

  WiFi.mode(WIFI_STA);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  WiFi.setAutoConnect(true);
  WiFi.setAutoReconnect(true);
  WiFi.begin(config.ssid, config.password);

  unsigned long started = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - started < 20000) {
    delay(200);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("[WIFI] Connected! IP: %s, Channel: %d\n", WiFi.localIP().toString().c_str(), WiFi.channel());
  } else {
    Serial.println("[WIFI] Could not connect to Wi-Fi. Entering setup mode...");
    startProvisioning();
    return;
  }

  startEspNow();
}

void loop() {
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd.equalsIgnoreCase("RESET")) {
      memset(&config, 0, sizeof(config));
      saveConfig();
      Serial.println("\n[SETUP] Settings erased! Restarting into setup hotspot mode...");
      delay(500);
      ESP.restart();
    }
  }

  if (provisioning) {
    dnsServer.processNextRequest();
    server.handleClient();
    delay(5);
    return;
  }

  // Safely transmit status ACK in loop context
  if (pendingAck) {
    pendingAck = false;
    sendStatus();
  }

  // Smart Watchdog: if motor is RUNNING and no controller keep-alive arrived for 60s (ESP32 blank out), shut off!
  if (relayOn && (millis() - lastCommandAt > 60000UL)) {
    Serial.println("[SAFETY] Controller heartbeat lost for 60s while motor was running! Shutting off relay.");
    setRelay(false);
  }

  // Safety failsafe: if token mismatch persists for more than 1 minute, relay must remain OFF
  if (tokenMismatchStartedAt != 0 && (millis() - tokenMismatchStartedAt >= TOKEN_MISMATCH_TIMEOUT_MS)) {
    if (relayOn) {
      Serial.println("[SAFETY] Token mismatch active for >1 minute! Shutting off relay.");
      setRelay(false);
    }
  }

  // Check Wi-Fi link status periodically
  static unsigned long lastWifiCheck = 0;
  if (millis() - lastWifiCheck > 5000) {
    lastWifiCheck = millis();
    if (WiFi.status() != WL_CONNECTED) {
      if (relayOn) {
        Serial.println("[WIFI] Wi-Fi lost! Shutting off relay.");
        setRelay(false);
      }
    }
  }
}
