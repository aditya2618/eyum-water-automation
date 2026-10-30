# iYum Water Automation — Quick Reference

## SoftAP Setup
| Item | Value |
|---|---|
| Wi-Fi SSID | `iYum-Water-Setup` |
| Password | `12345678` |
| Setup page | `http://192.168.4.1` |
| Local dashboard | `http://iyum-water.local` |

---

## Flash & Upload Commands

```powershell
# Flash ESP32 controller firmware
pio run -e esp32 -t upload

# Flash ESP32-C3 Mini controller firmware
pio run -e esp32c3mini -t upload

# Flash ESP-01 relay node firmware
pio run -e esp01_relay -t upload

# Monitor serial output
pio device monitor --baud 115200
```

---

## Serial Console Commands (ESP32)

| Command | Action |
|---|---|
| `STATUS` | Print live tank status |
| `RESET` | Erase Wi-Fi credentials and restart |
| `SET_WIFI:MySSID,mypassword` | Set Wi-Fi credentials directly from serial |

---

## Pin Reference

### ESP32 DevKit V1 (38-pin)
| Function | GPIO |
|---|---|
| Ultrasonic TRIGGER | GPIO 18 |
| Ultrasonic ECHO | GPIO 19 (3.3V via voltage divider) |
| Setup Button (hold 8s to reset) | GPIO 23 |

### ESP32-C3 Super Mini
| Function | GPIO |
|---|---|
| Ultrasonic TRIGGER | GPIO 0 |
| Ultrasonic ECHO | GPIO 1 |
| Setup Button (BOOT) | GPIO 9 |

---

## NVS Quick Reset

To clear all settings and re-enter setup mode:
- **Hold GPIO 23 (ESP32) or GPIO 9 (C3)** LOW for **8 seconds** at boot.
- Device will restart into setup AP mode.

---

## Default Config Values

| Parameter | Default |
|---|---|
| Tank Depth | 120 cm |
| Start pump below | 25% |
| Stop pump at | 95% |
| Max run time | 30 minutes |
| Sensor interval | 10 seconds |
| Relay watchdog | 30 seconds |

---

## Pairing Relay Node

1. Flash relay node with `pio run -e esp01_relay -t upload`
2. Connect to `iYum-Relay-Setup` AP
3. Set the **same pairing token** as configured in the controller
4. In controller dashboard → Settings → enter Relay MAC address

---

## Cloud Quick Commands (curl)

```bash
# Check device status
curl -H "Authorization: Token <token>" \
  http://<cloud-ip>/api/iot/devices/esp32-water-auto-01/shadow/

# Start pump remotely
curl -X POST -H "Authorization: Token <token>" \
  -H "Content-Type: application/json" \
  -d '{"action": "pump_on"}' \
  http://<cloud-ip>/api/iot/devices/esp32-water-auto-01/command/

# Stop pump remotely
curl -X POST -H "Authorization: Token <token>" \
  -H "Content-Type: application/json" \
  -d '{"action": "pump_off"}' \
  http://<cloud-ip>/api/iot/devices/esp32-water-auto-01/command/

# Get last 10 telemetry records
curl -H "Authorization: Token <token>" \
  "http://<cloud-ip>/api/iot/devices/esp32-water-auto-01/telemetry/?limit=10"
```

---

## Troubleshooting

| Symptom | Likely Cause | Fix |
|---|---|---|
| Relay not responding | Wrong or empty relay MAC | Dashboard → Settings → set Relay MAC |
| Relay turns off on its own | No heartbeat (watchdog fired) | Check controller Wi-Fi / ESP-NOW range |
| Level shows NaN | Sensor cable / 5V supply issue | Check wiring; verify voltage divider |
| Can't connect to AP | Wrong password | Default: `12345678` |
| Cloud not connecting | device_id or secret wrong | Re-run provisioning script on server |
| Relay won't turn on | Pairing token mismatch | Set same token on both devices |
