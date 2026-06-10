# Changelog — APASENSE

## [1.0.0] — 2026-06-10

### Added
- ADS1015 four-channel ADC driver (raw Wire I2C, no Adafruit dependency)
- Pressure transducer reading — parametric `maxBar`, auto zero-cal, EEPROM persistence
- AC current measurement (ACS712) — true RMS via non-blocking 32-sample accumulator
- LDR solar irradiance — 0–100 % output, user-supplied calibration constants
- AUX raw voltage input
- PCF8574AT I/O expander — 4 tank-empty sensor inputs (configurable polarity) + 4 status LEDs
- Buzzer management — `setBuzzer()` direct and `beep()` non-blocking single beep
- `onPumpState()` bridge for APAPUMP — triggers 30 s pressure re-zero after pump stop
- EEPROM persistence for pressure zero-point (6 bytes at address 582)
- All 5 platform builds verified: AVR Uno, Mega 2560, ESP32, ESP8266, STM32
- 4 examples: LDR calibration, minimal, pool sensors, full APA integration
