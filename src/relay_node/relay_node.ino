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
constexpr unsigned long COMMAND_TIMEOUT_MS = 30000;

struct Config { char marker[5]; char ssid[33]; char password[65]; char controllerMac[18]; char pairToken[17]; };
struct __attribute__((packed)) RelayMessage { char token[17]; uint8_t type; uint8_t pumpOn; uint32_t sequence; };
Config config{};
ESP8266WebServer server(80); DNSServer dnsServer; bool provisioning=false, espNowReady=false, relayOn=false; uint8_t controllerAddress[6]{}; unsigned long lastCommandAt=0;

const char SETUP_PAGE[] PROGMEM = R"HTML(
<!doctype html><html><head><meta name=viewport content="width=device-width,initial-scale=1"><title>iYum relay setup</title><style>body{font:16px system-ui;max-width:560px;margin:24px auto;padding:0 16px}label{display:block;margin-top:12px}input{box-sizing:border-box;width:100%;padding:9px}button{margin-top:18px;padding:10px}</style></head><body><h1>iYum relay setup</h1><p>Relay MAC: <b>%MAC%</b></p><form method=post action=/setup><label>Home Wi-Fi name</label><input name=ssid required><label>Home Wi-Fi password</label><input name=password type=password required><label>ESP32 controller MAC address</label><input name=controllerMac placeholder="AA:BB:CC:DD:EE:FF" required><label>Pairing token (same one configured on controller)</label><input name=pairToken type=password minlength=8 maxlength=16 required><button>Save and restart</button></form></body></html>)HTML";

bool parseMac(const char *text,uint8_t mac[6]){return sscanf(text,"%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",&mac[0],&mac[1],&mac[2],&mac[3],&mac[4],&mac[5])==6;}
void copyValue(char *d,size_t n,const String &v){v.toCharArray(d,n);}
void loadConfig(){EEPROM.begin(sizeof(Config));EEPROM.get(0,config);EEPROM.end();}
void saveConfig(){EEPROM.begin(sizeof(Config));EEPROM.put(0,config);EEPROM.commit();EEPROM.end();}
void setRelay(bool on){relayOn=on;digitalWrite(RELAY_PIN,(on^RELAY_ACTIVE_LOW)?HIGH:LOW);}
void sendStatus(){if(!espNowReady)return;RelayMessage message{};strncpy(message.token,config.pairToken,sizeof(message.token)-1);message.type=2;message.pumpOn=relayOn;esp_now_send(controllerAddress,reinterpret_cast<uint8_t*>(&message),sizeof(message));}
void receiveCallback(uint8_t *sender,uint8_t *data,uint8_t length){if(length!=sizeof(RelayMessage)||memcmp(sender,controllerAddress,6)!=0)return;RelayMessage msg;memcpy(&msg,data,sizeof(msg));if(msg.type!=1||strncmp(msg.token,config.pairToken,sizeof(msg.token))!=0)return;setRelay(msg.pumpOn);lastCommandAt=millis();sendStatus();}
void startEspNow(){if(esp_now_init()!=0)return;esp_now_set_self_role(ESP_NOW_ROLE_COMBO);uint8_t channel=WiFi.status()==WL_CONNECTED?WiFi.channel():0;esp_now_add_peer(controllerAddress,ESP_NOW_ROLE_COMBO,channel,nullptr,0);esp_now_register_recv_cb(receiveCallback);espNowReady=true;}
void startProvisioning(){provisioning=true;WiFi.mode(WIFI_AP);WiFi.softAP(AP_SSID,AP_PASSWORD);dnsServer.start(53,"*",WiFi.softAPIP());server.on("/",HTTP_GET,[](){String page=FPSTR(SETUP_PAGE);page.replace("%MAC%",WiFi.macAddress());server.send(200,"text/html",page);});server.on("/setup",HTTP_POST,[](){if(!parseMac(server.arg("controllerMac").c_str(),controllerAddress)||server.arg("pairToken").length()<8){server.send(400,"text/plain","Enter a valid controller MAC and an 8-character pairing token.");return;}memset(&config,0,sizeof(config));strcpy(config.marker,"IYUM");copyValue(config.ssid,sizeof(config.ssid),server.arg("ssid"));copyValue(config.password,sizeof(config.password),server.arg("password"));copyValue(config.controllerMac,sizeof(config.controllerMac),server.arg("controllerMac"));copyValue(config.pairToken,sizeof(config.pairToken),server.arg("pairToken"));saveConfig();server.send(200,"text/plain","Saved. The relay is restarting.");delay(700);ESP.restart();});server.onNotFound([](){server.sendHeader("Location","/");server.send(302,"text/plain","");});server.begin();}
void setup(){pinMode(RELAY_PIN,OUTPUT);setRelay(false);Serial.begin(115200);loadConfig();if(strcmp(config.marker,"IYUM")!=0&&strcmp(config.marker,"EYUM")!=0){startProvisioning();return;}parseMac(config.controllerMac,controllerAddress);WiFi.mode(WIFI_STA);WiFi.begin(config.ssid,config.password);unsigned long started=millis();while(WiFi.status()!=WL_CONNECTED&&millis()-started<20000)delay(100);startEspNow();}
void loop(){if(provisioning){dnsServer.processNextRequest();server.handleClient();return;}if(relayOn&&millis()-lastCommandAt>COMMAND_TIMEOUT_MS)setRelay(false);if(WiFi.status()!=WL_CONNECTED){setRelay(false);WiFi.disconnect();WiFi.begin(config.ssid,config.password);delay(500);}}
