# Changelog — APASENSE

## [1.2.0] — 2026-10-23

### Added
- **Morse patterns** for `alert()`: `BUZZER_CONFIRM` (E `.`), `BUZZER_MORSE_N` (`-.`), `BUZZER_MORSE_W` (`.--`), `BUZZER_MORSE_F` (`..-.`). Dot 120 ms, dash 3 dots, 1-dot gap inside a letter, 1.5 s silence after. Stored in PROGMEM — no RAM. `BUZZER_INFO / WARNING / ALARM` are unchanged.
- **`setBuzzerDriver(fn)`** — the buzzer is switched by your function `fn(bool on)` instead of `digitalWrite()`. Needed for a **passive** (magnetic) buzzer, which only ticks on plain DC. Setting a driver enables the buzzer; no pin is needed.
- **`ApaSenseTimer3Buzzer.h`** — optional, header-only passive-buzzer driver for AVR boards with Timer3 (Mega2560): `timer3Buzzer.begin(pin, hz)` + `adc.setBuzzerDriver(timer3Buzzer.set)`. Timer3 CTC compare interrupt toggles the pin; Timer3's PWM pins (D2/D3/D5) stay disconnected. Not `tone()`: on AVR `tone()` uses Timer2 and disconnects the PWM of D9/D10 (Mega) or D3/D11 (Uno). Stops the build with a clear `#error` on boards without Timer3. Include it in one file only (it defines the ISR). 5 bytes of RAM, only when included.
- Example `04_passive_buzzer` (Mega2560) — plays every pattern on a passive buzzer.

### Changed
- All buzzer output goes through one private `_buzzerOut()` (driver or pin). `enableBuzzer(pin)` behaves exactly as before.
- RAM: +2 bytes (driver pointer).

## [1.1.2] — 2026-10-01

### Fixed
- **Pressure zero could be taken under pressure.** The automatic re-zero after a pump stop stored whatever the sensor read 30 s later as "0 bar" and saved it to EEPROM. With pressure still trapped in the pipe (closed valve, check valve, clogged filter, leaking fitting), every later reading was too low by that amount: high-pressure protection could miss a real overpressure and a normal running pressure could read 0 (false dry-run alarms). Found on the APA-CONTROLLER bench: a leaking test fitting at 2.2 bar was stored as zero. Now a zero is accepted only if:
  - the reading is within ±0.2 V of the transducer's 0.5 V zero output (`APASENSE_PRESSURE_ZERO_WINDOW`), and
  - it held steady (±`APASENSE_PRESSURE_STABLE_COUNTS`, ~0.02 bar) over the last 10 s of the settle; a still-falling pressure restarts the 30 s settle instead.
  - A rejected zero keeps the previous one. `calibratePressureZero()` applies the same window; a stored zero that fails it is ignored at `begin()`.
- 6 bytes of RAM on AVR: the buzzer pattern `switch` compiled into a pointer table in RAM — now an explicit `PROGMEM` table.

### Changed
- RAM: +2 bytes (`_settleRefRaw`), −6 bytes (pattern table) → 4 bytes less on AVR.
- Docs: pressure zero-calibration section (README), `getPressure()` / `calibratePressureZero()` / `onPumpState()` (API.md), installation tip about sensors below the water level; API.md header and constants block showed version 1.1.0.

## [1.1.1] — 2026-06-11

### Added
- Library logo (`extras/apasense-logo.png`) — now displayed in README header.

## [1.1.0] — 2026-06-10

### Added
- `BuzzerAlert` enum: `BUZZER_INFO`, `BUZZER_WARNING`, `BUZZER_ALARM`
- `alert(severity, repeat)` — plays a rhythm pattern matched to alert severity; `repeat=true` loops until `stopAlert()`
- `stopAlert()` — stops any active repeating alert
- `getPower()` — returns instantaneous power in watts (V × I RMS); returns −1.0 if voltage not configured
- `APASENSE_MAINS_EU` (230 V) and `APASENSE_MAINS_US` (120 V) convenience constants

### Changed
- `enableCurrent()` gains optional third parameter `voltageV` (default 0.0 — no power calc, fully backward compatible)
- `setBuzzer(false)` now also cancels any active `alert()` pattern
- `beep()` now cancels any active pattern before starting a single beep

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
