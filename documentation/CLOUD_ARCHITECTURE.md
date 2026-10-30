# iYum Water Automation — Cloud Architecture

> How the ESP32 tank controller optionally connects to the Oracle Cloud and how you control it remotely.

---

## Overview

```
                         INTERNET
  ┌──────────────────────┐           ┌──────────────────────────────────┐
  │ ESP32 Tank Controller│           │   Oracle Cloud Server            │
  │ (iYum Water Auto)    │           │   (Django + Channels + Redis)    │
  │                      │ WebSocket │                                  │
  │ CloudClient ─────────┼───────────┼──► /ws/iot/?device_id=X&secret=Y│
  │                      │ ws://     │        │  IoTDeviceConsumer       │
  │  ◄ commands ─────────┼───────────┼────────┤                         │
  │    telemetry ────────┼──────────►│        ├─► Redis Channel Layer    │
  │                      │           │        │        │                 │
  └──────────────────────┘           │        ▼        ▼                │
                                     │  IoTDashboardConsumer            │
  ┌──────────────────────┐           │  /ws/iot/dashboard/iyum-water/   │
  │ You (Browser/API)    │◄──────────┼──────────────────────────────────┤
  │                      │ WebSocket │                                  │
  │  REST commands ──────┼──────────►│  /api/iot/devices/<id>/command/  │
  └──────────────────────┘           └──────────────────────────────────┘
```

---

## Connection Steps

### 1. Boot & Wi-Fi
After provisioning, the ESP32 connects to home Wi-Fi (`WIFI_STA` mode).

### 2. CloudClient Opens WebSocket
If cloud is enabled in the dashboard, it connects to:
```
ws://<cloud-host>:80/ws/iot/?device_id=<device_id>&secret=<token>
```

### 3. Cloud Authenticates Device
- Looks up `device_id` in the `iot_devices` table.
- PBKDF2 hash comparison of the secret.
- On success: marks device **online**, joins channel groups.
- Sends ACK: `{"type": "connected", "device_id": "..."}`

### 4. Telemetry Every 30 Seconds
```json
{
  "type": "telemetry",
  "payload": {
    "level": 67.5,
    "pump": false,
    "mode": "auto",
    "rssi": -54,
    "uptimeSec": 14400,
    "fw": "1.3.0"
  }
}
```

---

## Sending Commands From Cloud

### Via REST API
```http
POST /api/iot/devices/esp32-water-auto-01/command/
Authorization: Token <your_api_token>
Content-Type: application/json

{"action": "pump_on"}
```

**Available actions:**
| Action | Effect | Params |
|---|---|---|
| `pump_on` | Start pump via ESP-NOW → relay | _(none)_ |
| `pump_off` | Stop pump | _(none)_ |
| `set_mode` | Change pump control mode | `{"mode": "auto"}` |

### Via WebSocket Dashboard
```
ws://<cloud-host>/ws/iot/dashboard/iyum-water/
```
Live events you'll receive:
```json
{"type": "telemetry",    "device_id": "esp32-water-auto-01", "payload": {...}}
{"type": "device_status","device_id": "esp32-water-auto-01", "is_online": true}
{"type": "command_ack",  "status": "acknowledged"}
```

---

## Command Flow (Cloud → ESP-NOW → Relay → Pump)

```
REST API call: {"action": "pump_on"}
    │
    ▼
Django IoTDeviceCommandView
    │  device.is_online check
    ▼
Redis Channel Layer
    │  group_send("iot_device_esp32-water-auto-01", {...})
    ▼
IoTDeviceConsumer (live WebSocket to ESP32)
    │  forwards JSON to firmware
    ▼
CloudClient.onCommand() handler in esp32.ino
    │  action == "pump_on"
    ▼
sendRelayCommand() → ESP-NOW to ESP-01 relay
    │
    ▼
ESP-01 relay: pump contactor ON
    │
    ▼ ACK
cloudClient.sendAck(cmdId, "acknowledged", "Pump started")
```

---

## Offline Behaviour

- If internet drops, the device still measures tank level and controls the pump via ESP-NOW.
- Cloud marks device **offline** in the database.
- Dashboard shows `is_online: false`.
- REST commands while offline return `HTTP 202 Accepted / "status": "queued"`.

---

## Device Provisioning (First Time)

### On the Cloud Server
```bash
cd /var/www/cloud
python provision_iot_device.py \
  --project iyum-water \
  --device-id esp32-water-auto-01 \
  --name "Rooftop Tank Controller" \
  --type esp32_water_auto
```
Copy the printed `device_secret` — shown only once.

### On the ESP32 Dashboard
1. Open `http://iyum-water.local`
2. In Cloud Settings, fill in:
   - **Host:** `<your Oracle server IP>`
   - **Port:** `80`
   - **Device ID:** `esp32-water-auto-01`
   - **Device Secret:** _(paste token)_
3. Enable Cloud → Save

### Verify in Serial Monitor
```
[Cloud] Connecting to <IP>:80 -> /ws/iot/?device_id=esp32-water-auto-01&...
[Cloud] Connected to Eyum Cloud
[Cloud] Cloud server confirmed device session: esp32-water-auto-01
```

---

## REST API Quick Reference

| Endpoint | Method | Purpose |
|---|---|---|
| `/api/iot/projects/` | GET / POST | Manage IoT projects |
| `/api/iot/devices/register/` | POST | Register device |
| `/api/iot/devices/?project=iyum-water` | GET | List devices |
| `/api/iot/devices/<device_id>/` | GET | Device details + shadow |
| `/api/iot/devices/<device_id>/command/` | POST | Send command |
| `/api/iot/devices/<device_id>/telemetry/` | GET | History (last N records) |
| `/api/iot/devices/<device_id>/shadow/` | GET | Latest live device state |

---

## Credential Summary

| Item | Value |
|---|---|
| SoftAP SSID | `iYum-Water-Setup` |
| SoftAP Password | `12345678` |
| Local dashboard | `http://iyum-water.local` |
| Cloud project slug | `iyum-water` |
| WS Device URL | `ws://<host>/ws/iot/?device_id=esp32-water-auto-01&secret=<token>` |
| WS Dashboard URL | `ws://<host>/ws/iot/dashboard/iyum-water/` |
