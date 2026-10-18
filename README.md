# iYum Water Automation

This project automates an overhead water tank without a cloud server. It uses two boards on the same home Wi-Fi network:

| Device | Job |
| --- | --- |
| ESP32 DevKit V1 (38 pin) | Measures tank level with an SR04M, makes the pump decision, and serves the browser dashboard. |
| ESP-01 relay module | Receives the ESP32 command over ESP-NOW and switches the motor relay. |

The ESP32 owns the safety logic. The relay never starts by itself, turns off after 30 seconds without a valid controller heartbeat, and turns off if it loses Wi-Fi. The controller turns the command off after three invalid sensor samples and limits every run to the configured maximum duration.

## What is included

- [`src/esp32/esp32.ino`](src/esp32/esp32.ino) — ESP32 DevKit V1 (38-pin) firmware (Trigger: GPIO 18, Echo: GPIO 19, Setup: GPIO 23).
- [`src/esp32c3mini/esp32c3mini.ino`](src/esp32c3mini/esp32c3mini.ino) — ESP32-C3 Super Mini firmware (Trigger: GPIO 0, Echo: GPIO 1, Setup: GPIO 9 onboard BOOT).
- [`src/relay_node/relay_node.ino`](src/relay_node/relay_node.ino) — ESP-01 firmware, provisioning portal, authenticated ESP-NOW relay listener, and relay watchdog.

## Parts

- ESP32 DevKit V1, 38-pin version
- ESP-01 or ESP-01S relay board with an ESP8266
- JSN-SR04T / SR04M waterproof ultrasonic sensor in trigger/echo mode
- 5 V supply for the ultrasonic sensor
- 1 kΩ and 2 kΩ resistors for the ultrasonic ECHO voltage divider
- Two isolated, certified 5 V / 2 A power supplies or USB adapters, one per node
- Two 470 µF / 10 V electrolytic capacitors and two 0.1 µF ceramic capacitors for local power stability
- A correctly rated, enclosed contactor or relay arrangement for the pump motor
- USB serial programmer for the ESP-01, if the relay module does not include one

Optional battery backup: a 5 V / 2 A power-path mini UPS for each node that needs backup, plus its compatible protected battery pack.

## Power design: 220–240 V AC, 5 V DC, and battery backup

Use a separate low-voltage supply for each installed node: one at the tank ESP32 and one in the motor-control enclosure for the ESP-01 relay. This keeps the sensor wiring low voltage and lets the two units be installed where they are needed.

### Recommended mains-powered arrangement

The simplest and preferred option is a **quality isolated 5 V, 2 A USB wall adapter** for each node. Keep the mains adapter outside the project enclosure, then run only 5 V DC into the electronics enclosure.

| Node | Supply input | What connects to it |
| --- | --- | --- |
| ESP32 tank controller | 5 V, 2 A | ESP32 `VIN`/`5V` pin or USB connector; SR04M `VCC`; common GND. |
| ESP-01 relay board | 5 V, 2 A | The board's marked `5V`/`VCC` input, **only if it is a 5 V relay board with an onboard 3.3 V regulator**. |
| Bare ESP-01 | Regulated 3.3 V, at least 500 mA | ESP-01 `VCC` and `EN/CH_PD`; do not use an unregulated 5 V feed. |

Add a 470 µF, 10 V electrolytic capacitor and a 0.1 µF ceramic capacitor across 5 V/GND close to each Wi-Fi board. Wi-Fi transmit bursts can cause a weak supply or long cable to brown out the board.

For an integrated product enclosure, use a certified isolated AC/DC module such as a **Mean Well IRM-10-5** (5 V, 10 W / 2 A) per node, installed by a qualified electrician on a properly designed PCB and inside a flame-rated insulated enclosure. Its data sheet specifies an 85–305 VAC input range and safety approvals; the smaller IRM-05-5 is 5 V / 1 A, but the 2 A option gives better margin for Wi-Fi current bursts and relay coils. [Mean Well IRM-10 data sheet](https://www.meanwell.com/Upload/PDF/IRM-10/IRM-10-SPEC.PDF), [Mean Well IRM-05 data sheet](https://www.meanwell.com/Upload/PDF/IRM-05/IRM-05-SPEC.pdf).

```text
220–240 V AC
    │
    ├── protected, enclosed, certified 5 V / 2 A AC/DC supply ── 5 V ── ESP32 VIN + SR04M
    │
    └── protected, enclosed, certified 5 V / 2 A AC/DC supply ── 5 V ── ESP-01 relay board
```

Do not use a bare phone-charger PCB, a non-isolated capacitive-dropper circuit, or an exposed AC/DC board in a plastic project box. The AC input, fuse, terminals, strain relief, creepage distances, earthing method, contactor, and relay-side mains wiring need to follow the supply maker's instructions and local electrical rules. Have this part assembled and inspected by a qualified electrician.

### Battery-backed option

Use a **5 V mini-UPS/power-path module rated for at least 2 A** between the mains 5 V adapter and the ESP32. It must accept a 5 V charger input, charge its protected battery pack, and keep a regulated 5 V output alive when AC fails.

```text
5 V wall adapter ── 5 V UPS / power-path module ── ESP32 VIN + SR04M
                         │
                         └── protected 1S Li-ion battery pack / 18650 cells
```

The same arrangement can power the ESP-01 relay node, but it does not keep an AC pump running through a power failure. It only keeps the controller and relay electronics alive; the pump's own AC supply and contactor still determine whether the motor can run.

Avoid connecting an ESP32 directly to a single 3.7 V Li-ion cell. Use the UPS module's regulated 5 V output. Also avoid using a basic TP4056 charger board as a permanent UPS: TP4056 is a one-cell charger and needs separate load sharing/power-path management for a device that stays powered while charging. Choose a protected cell or pack and a UPS board that explicitly provides charging, protection, regulated output, and automatic source switchover. [TP4056 reference](https://pcbwiki.com/parts/tp4056).

If the installation already has a 12 V battery or solar system, use a fused 12 V-to-5 V DC/DC buck converter rated for at least 2 A instead of a single-cell UPS. Use a proper charger and battery management system matched to the battery chemistry.

> **Mains warning:** The ESP-01 relay output must not be used directly with a high-power pump unless the relay and enclosure are rated for it. In most installations the relay drives a suitably rated contactor. Have the mains side installed by a qualified electrician.

## How it works

1. The ESP32 measures the empty space between the sensor and water surface at the configured **Sensor Read Sensitivity Interval** (default: 10 seconds, adjustable in the UI from 1s to 300s with quick presets).
2. It converts that distance to a water percentage using the configured tank depth and sensor offset.
3. In automatic mode, whenever the water level is between **0% and the Start pump below threshold**, it turns the pump ON. When the level reaches or exceeds **Stop pump at**, it turns the pump OFF. Between the start and stop points, hysteresis prevents motor flapping.
4. The ESP32 sends the requested state to the ESP-01 every six seconds (and immediately on any level or state transition) using ESP-NOW.
5. The ESP-01 accepts a command only from the saved controller MAC address and only when its pairing token matches. It returns an acknowledgement that the dashboard shows as relay status.

Both boards join the same router. This matters because ESP-NOW uses the current Wi-Fi channel. There is no cloud service and the automation keeps working if the internet is down.

## Wiring

### ESP32 and SR04M

| SR04M pin | ESP32 pin | Notes |
| --- | --- | --- |
| VCC | 5 V / VIN | Use the sensor's required 5 V supply. |
| GND | GND | The grounds must be common. |
| TRIG | GPIO 18 | Direct connection is fine. |
| ECHO | GPIO 19 through a 5 V → 3.3 V divider | **Do not connect ECHO directly.** ESP32 GPIO is 3.3 V only. Use 1 kΩ from ECHO to GPIO 19 and 2 kΩ from GPIO 19 to GND. |

The firmware also reserves GPIO 23 as an optional setup reset button. Connect a normally-open push button from GPIO 23 to GND. Holding it for eight seconds clears only the ESP32 saved settings and reopens its setup portal.

Set the SR04M's mode jumper for ordinary trigger/echo operation. Some waterproof modules use UART by default; see the label or data sheet for your exact board.

### ESP-01 relay board

The firmware expects the relay input on **GPIO 0**, which is common on ESP-01 relay boards. It uses active-low logic by default. Change `RELAY_ACTIVE_LOW` in `relay_node.ino` if your board works the other way around.

GPIO 0 affects the ESP-01 boot mode. Follow the relay board's own flashing instructions; many boards require the relay/load connector to be disconnected or GPIO 0 held low during flashing. Power the ESP-01 from a stable 3.3 V supply that can provide Wi-Fi current peaks. Do not feed 5 V into an ESP-01 GPIO.

## Build and flash firmware

The repository includes [`platformio.ini`](platformio.ini) and repeatable PowerShell commands under [`tools`](tools). PlatformIO is the recommended route because one command builds exactly the right firmware for each board.

### Install PlatformIO Core on Windows

1. Install [Python 3](https://www.python.org/downloads/) and enable **Add Python to PATH** during installation.
2. Open PowerShell and run:

   ```powershell
   py -m pip install --user platformio
   ```

3. Close and reopen PowerShell. Verify the installation:

   ```powershell
   pio --version
   ```

4. From this project folder, build both firmware images:

   ```powershell
   .\tools\build-firmware.ps1
   ```

   If Windows blocks local PowerShell scripts, run this once in that PowerShell window, then repeat the command:

   ```powershell
   Set-ExecutionPolicy -Scope Process Bypass
   ```

PlatformIO downloads the ESP32 and ESP8266 board packages on the first build. Successful images are created under `.pio\build\esp32_controller\firmware.bin` and `.pio\build\esp01_relay\firmware.bin`.

### Flash the ESP32 controller

1. Disconnect the SR04M and any external wiring during the first upload if the board is behaving unexpectedly.
2. Connect the ESP32 DevKit to the computer by a data-capable USB cable.
3. In Windows Device Manager, find the serial port, for example `COM5`. Install the board's CP210x or CH340 driver if no port appears.
4. Flash it from the project folder:

   ```powershell
   .\tools\flash-esp32.ps1 -Port COM5
   ```

5. If the upload stalls at `Connecting`, hold the ESP32 **BOOT** button, start the command, and release BOOT when writing begins. Press **EN/RST** once after a successful upload if it does not restart by itself.
6. Open a serial monitor when diagnosis is needed:

   ```powershell
   pio device monitor -e esp32_controller --port COM5
   ```

### Flash the ESP-01 relay node

The ESP-01 has no USB interface. Use a 3.3 V USB-to-TTL serial adapter that can reliably supply at least 500 mA. **Never connect its VCC or GPIO pins to 5 V.**

| USB-to-TTL adapter | ESP-01 | Purpose |
| --- | --- | --- |
| 3.3 V | VCC and EN / CH_PD | Stable power and chip enable |
| GND | GND | Shared ground |
| TXD | RX | Serial input to the ESP-01 |
| RXD | TX | Serial output from the ESP-01 |
| GND, temporarily | GPIO 0 | Selects firmware-download mode at reset |
| 3.3 V | GPIO 2 | Required boot strap state |
| 3.3 V, through 10 kΩ | RST | Reset pull-up; briefly connect RST to GND to reset |

1. Disconnect the pump and mains supply. If the ESP-01 is mounted on a relay board, disconnect its relay load and follow the board maker's programming-jumper instructions.
2. Connect the USB-to-TTL adapter as above. Keep GPIO 0 tied to GND.
3. Reset or power-cycle the ESP-01 while GPIO 0 is low. It is now in flash mode.
4. Find the adapter port in Device Manager, for example `COM7`, then run:

   ```powershell
   .\tools\flash-esp01.ps1 -Port COM7
   ```

5. Wait for `SUCCESS`, remove the GPIO 0-to-GND connection, then reset or power-cycle the ESP-01. It must boot with GPIO 0 released.
6. To inspect boot messages:

   ```powershell
   pio device monitor -e esp01_relay --port COM7
   ```

Some ESP-01 relay boards place the relay on GPIO 0 and interfere with its boot strap. If flashing fails, remove the relay board's jumper or use the board-specific programming header. Do not force GPIO 0 high or low against the relay board circuitry.

### Arduino IDE alternative

### Arduino IDE

1. Install Arduino IDE 2.x.
2. In **Preferences → Additional Boards Manager URLs**, add:

   `https://espressif.github.io/arduino-esp32/package_esp32_index.json`

   `https://arduino.esp8266.com/stable/package_esp8266com_index.json`
### Flashing the boards

- **Classic ESP32 (38-pin DevKit V1)** (Trigger: GPIO 18, Echo: GPIO 19):
  ```powershell
  .\tools\flash-esp32.ps1 -Port COM<X>
  ```
- **ESP32-C3 Super Mini** (Trigger: GPIO 0, Echo: GPIO 1):
  ```powershell
  .\tools\flash-esp32c3mini.ps1 -Port COM<X>
  ```
- **ESP-01 relay node**:
  ```powershell
  pio run -e esp01_relay -t upload --upload-port COM<X>
  ```

## First-time setup

Use this order so each board's MAC address is available when needed.

1. Flash both boards using the steps above. Keep the ESP-01 disconnected from the motor circuit during commissioning.
2. Power the ESP32. From a phone or laptop, join Wi-Fi network **iYum-Water-Setup** with password **iyumsetup**.
3. Open `http://192.168.4.1`. Save the home Wi-Fi details, a dashboard password, tank values, and a pairing token of 8–16 characters. Keep the pairing token for the next board.
4. Join the home Wi-Fi. Open `http://iyum-water.local` or find the ESP32's IP address in the router's connected-device list. Log in as user `admin` with the password chosen above.
5. Copy the **controller MAC** shown on the dashboard.
6. Power the ESP-01. Join **iYum-Relay-Setup** with password **iyumsetup**, then open `http://192.168.4.1`. The page displays the relay MAC. Save the same home Wi-Fi details, the copied controller MAC, and the exact same pairing token.
7. Return to the ESP32 dashboard. In Configuration, enter the relay MAC shown by the relay setup page, fill the tank depth and control limits, then save.
8. Test with the dashboard's **Run pump** and **Stop pump** buttons before connecting the motor circuit. Confirm the dashboard relay status changes and that the relay switches.
9. Choose **Automatic**. Test carefully with safe simulated levels or an empty tank before relying on the installation.

## Commissioning checklist

Complete these checks in order before the motor is connected:

1. **ESP32 setup:** the controller joins Wi-Fi, the dashboard opens, and its MAC address is visible.
2. **ESP-01 setup:** the relay joins the same 2.4 GHz Wi-Fi, uses the same pairing token, and has the exact ESP32 MAC saved.
3. **Wireless link:** save the ESP-01 MAC in the ESP32 dashboard. Within 10 seconds, the dashboard's `relay seen` value should stay below 10 seconds.
4. **Relay logic:** press **Run pump** and **Stop pump** while observing only the low-voltage relay indicator or a safe test lamp. Change `RELAY_ACTIVE_LOW` and reflash if its action is reversed.
5. **Sensor reading:** move a flat object beneath the ultrasonic sensor and check that the distance changes plausibly. Confirm the displayed tank percentage rises as the object moves closer.
6. **Automatic switching:** temporarily set safe test thresholds around the observed level. Confirm the controller requests ON below the start value and OFF at the stop value.
7. **Failsafe:** while the relay is ON, turn off the ESP32. The ESP-01 must turn the relay OFF within 30 seconds. Restore normal firmware and values after this test.
8. **Motor connection:** only after every low-voltage test passes, have the final contactor and pump circuit connected and tested by a qualified electrician.

## Daily use

Open `http://iyum-water.local` from any device on the same home Wi-Fi. If the name does not resolve on that device or router, use the ESP32's LAN IP address shown in the router. A DHCP reservation for the ESP32 keeps that IP stable.

The dashboard provides:

- live water level and ultrasonic distance;
- requested pump state and relay acknowledgement;
- automatic, manual run, and manual stop modes;
- tank depth, offset, limits, maximum runtime, relay MAC, Wi-Fi, password, and pairing-token settings.

Use **Automatic** after a manual test. Manual run still ends when the maximum run time expires. The default maximum is 30 minutes.

## Calibration

1. Measure from the ultrasonic sensor face to the water surface when the tank is empty. Set **Tank depth** to that distance in centimetres.
2. Fill the tank to the desired full point. If the reported percentage is off, use **Sensor offset** to correct the measurement. A positive offset means the measured distance is treated as slightly smaller.
3. Begin with Start = 25% and Stop = 95%. Keep at least a 10% gap between them.
4. Set Maximum run time longer than the normal filling time, but short enough that a failed sensor or dry supply cannot run the pump for hours.

## Troubleshooting

| Symptom | Check |
| --- | --- |
| Dashboard never appears after setup | Confirm the ESP32 joined the correct 2.4 GHz Wi-Fi and use its router-listed IP. ESP32 does not join 5 GHz-only networks. |
| Dashboard says `relay seen 9999s ago` | Confirm both boards are on the same Wi-Fi network, the relay MAC is correct, and pairing tokens match exactly. |
| Relay works backward | Set `RELAY_ACTIVE_LOW` to `false`, upload the relay sketch again, and retest with the pump disconnected. |
| Sensor distance is invalid | Check common GND, 5 V sensor supply, selected trigger/echo mode, and the ECHO voltage divider. |
| Pump runs too often | Increase the distance between start and stop percentages. |
| ESP-01 is stuck in setup mode | Reflash it, then complete the setup form. Check that EEPROM data is not being erased by an unstable supply. |

## Security and limits

The dashboard uses HTTP Basic Authentication on the local network. It is suitable for a trusted home LAN, but HTTP does not encrypt the password. Do not expose the ESP32 directly to the internet with port forwarding. Use a VPN or integrate it with your existing iYum/Django service if remote access is needed.

This is a single independent water controller. It can later be integrated into a larger iYum home system by having that system read or control the ESP32's authenticated HTTP endpoints, but no server is needed for normal local operation.
