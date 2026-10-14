## Description
Briefly describe the change and motivation behind it.

## Type of Change
- [ ] Bug fix (non-breaking change which fixes an issue)
- [ ] New feature (non-breaking change which adds functionality)
- [ ] Breaking change (fix or feature that would cause existing setups not to work as expected)
- [ ] Documentation update
- [ ] CI / tooling improvement

## Target Hardware Tested
- [ ] ESP32 DevKit V1 (Tank Controller)
- [ ] ESP-01 / ESP8266 (Relay Node)
- [ ] JSN-SR04T / AJ-SR04M Ultrasonic Sensor
- [ ] Simulated / desk test

## Failsafe & Safety Verification
- [ ] Relay turns off on 30s communication timeout
- [ ] Maximum run time limiter verified
- [ ] Invalid ultrasonic sensor readings (>3 failed samples) verified to disengage pump
- [ ] Both environments compiled successfully (`pio run`)

## Checklist
- [ ] My code follows the project style and conventions
- [ ] I have commented my code where necessary
- [ ] I have updated documentation / README where applicable
- [ ] Local build passes with zero errors
