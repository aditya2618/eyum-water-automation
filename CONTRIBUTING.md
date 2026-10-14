# Contributing to Eyum Water Automation

Thank you for your interest in contributing to **Eyum Water Automation**! We welcome contributions to enhance hardware support, improve safety heuristics, refine the web dashboard, and optimize power efficiency.

---

## Code of Conduct

Please maintain a respectful, welcoming, and collaborative environment for all contributors.

---

## Development Setup

1. **Clone the repository:**
   ```bash
   git clone https://github.com/aditya2618/eyum-water-automation.git
   cd eyum-water-automation
   ```

2. **Prerequisites:**
   - [PlatformIO Core (CLI)](https://platformio.org/install/cli) or PlatformIO IDE for VS Code.
   - Python 3.10+.

3. **Building Firmware:**
   ```bash
   # Build both targets
   pio run

   # Or build individual targets
   pio run -e esp32_controller
   pio run -e esp01_relay
   ```

4. **PowerShell Helper Scripts (Windows):**
   ```powershell
   .\tools\build-firmware.ps1
   .\tools\flash-esp32.ps1 -Port COM5
   .\tools\flash-esp01.ps1 -Port COM7
   ```

---

## Guidelines for Contributions

### Safety & Failsafe Integrity
This project interfaces with physical high-voltage water pump motors and ultrasonic sensors. Any PR modifying core logic must uphold the following rules:
- **Relay failsafe:** The relay MUST turn off automatically if communication with the controller is lost for more than 30 seconds.
- **Sensor anomaly handling:** Repeated invalid or missing distance readings must immediately halt the pump.
- **Max run timer:** The pump must never run longer than the configured maximum duration (`maxRunMinutes`), regardless of sensor feedback.
- **Network isolation:** Automation and failsafes must operate strictly peer-to-peer (ESP-NOW) on the local LAN without dependency on external cloud services.

### Submitting Pull Requests
1. Fork the repo and create a topic branch from `main` (e.g. `feature/sensor-filtering` or `fix/relay-timeout`).
2. Verify both firmware targets compile with zero warnings or errors (`pio run`).
3. Commit your changes with clear, descriptive commit messages.
4. Open a Pull Request detailing the changes, testing methodology, and any hardware validated.

---

## Reporting Issues & Feature Requests
- Check existing issues before opening a new one.
- For bugs, please include board revisions, sensor model, serial monitor logs, and reproduction steps.
