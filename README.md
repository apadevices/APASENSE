<p align="center"><img src="extras/apasense-logo.png" width="600" alt="APASENSE"></p>

# APASENSE

**ADS1015 analog sensing and PCF8574AT I/O library for APA pool automation**
![Version](https://img.shields.io/badge/version-1.2.0-blue)
![Platforms](https://img.shields.io/badge/platform-AVR%20%7C%20ESP32%20%7C%20ESP8266%20%7C%20STM32-green)

---

## Key Features

**Sensing (ADS1015, I2C)**
- Pool pressure — parametric range (any 0.5–4.5 V ratiometric transducer), auto zero-cal with plausibility and stability checks
- Pump current — true AC RMS via non-blocking 32-sample accumulator (ACS712)
- Pump apparent power — `getPower()` returns V × I_rms; supply mains voltage once in setup
- Solar irradiance — 0–100 % using user-supplied calibration constants
- AUX voltage — raw 0–5 V input for any future sensor

**Digital I/O (PCF8574AT, I2C)**
- 4 × chemical tank-empty sensor inputs — per-sensor configurable polarity
- 4 × status LED outputs (active-low open-drain)

**Indicators**
- Buzzer — single beep, three severity-matched rhythm patterns (`BUZZER_INFO / WARNING / ALARM`) and four Morse letters (`BUZZER_CONFIRM` E, `BUZZER_MORSE_N / W / F`)
- Alert patterns repeat until `stopAlert()` — designed for integration with APAPUMP alarm callbacks
- Active or passive buzzer — plain pin for an active buzzer, or your own driver via `setBuzzerDriver()`; optional `ApaSenseTimer3Buzzer.h` drives a passive buzzer on the Mega without `tone()` (keeps Timer2 PWM working)

**Integration**
- Designed as the sensor layer for APAPUMP and APADOSE
- Callback bridges connect readings to APAPUMP safety engine in four lines
- Pressure re-zero automatically triggered after every pump stop — never taken while pressure is still trapped in the pipe
- Pure `Wire.h` dependency — no Adafruit or external libraries required

---

## Installation

**Arduino IDE**
1. Download the latest ZIP from the [Releases](https://github.com/apadevices/APASENSE/releases) page
2. Sketch → Include Library → Add .ZIP Library

**PlatformIO**
```ini
; Replace with the actual path to your local copy of APA-SENSE_LIB
lib_extra_dirs = ../APA-SENSE_LIB          ; relative (sibling folder)
; lib_extra_dirs = C:/projects/APA-SENSE_LIB  ; absolute (Windows)
; lib_extra_dirs = N:\kecup\PlatformIO\APA-SENSE_LIB  ; NAS drive
```

---

## What It Does

APASENSE is the hardware abstraction layer for the analog sensing side of an APA pool controller. It reads four analog inputs via an ADS1015 12-bit ADC, manages binary tank-empty sensors and status LEDs through a PCF8574AT I/O expander, and drives a buzzer. All operations are non-blocking — the library cycles through ADC conversions one at a time in `update()` so the main loop never stalls.

Pressure and current readings are delivered to APAPUMP through callbacks. When the pump stops, APASENSE automatically re-zeroes the pressure transducer after a 30-second settle period — only if the reading is steady and close to the transducer's 0 bar output — and saves the result to EEPROM, so pressure readings are accurate from the very first update() cycle on every subsequent boot. If mains voltage is supplied to `enableCurrent()`, `getPower()` returns apparent power in VA. Three built-in buzzer patterns (`BUZZER_INFO`, `BUZZER_WARNING`, `BUZZER_ALARM`) and four Morse letters can repeat continuously until `stopAlert()` is called — intended for APAPUMP alarm callbacks. The buzzer can be an active one on a plain pin or a passive one through a driver function (a Timer3 driver for the Mega is included).

---

## How It Works

### Non-blocking ADC cycle

`update()` drives one ADS1015 single-shot conversion per call, cycling round-robin through enabled channels. The 1 ms conversion time is tracked with a lightweight 16-bit millis() comparison — no blocking delays anywhere in the library.

```
update() called every loop():
  ┌────────────────────────────────────────────────────────────┐
  │  1. Buzzer sequencer — advance pattern step / expire beep  │
  │  2. Settle timer     — re-zero pressure if 30 s elapsed    │
  │  3. ADC cycle:                                             │
  │       conversion pending?                                  │
  │         yes + 1 ms elapsed → read result → process        │
  │       no pending conversion?                               │
  │         → find next enabled sensor → start conversion     │
  └────────────────────────────────────────────────────────────┘
```

Channels cycle in order: pressure → current → AUX → LDR (skipping disabled ones).
At a 1 ms update() rate, each enabled channel is sampled approximately every 4 ms.

### Pressure zero-calibration

The pressure transducer outputs 0.5 V at zero pressure and 4.5 V at `maxBar`. The library measures the ADC count at zero pressure (pump off) and stores it as `_pressureZero`. All subsequent readings are referenced to this offset.

```
onPumpState(false) called
        │
        ▼  wait 20 s ── note the reading (stability reference)
        ▼  wait 10 s more (30 s total, APASENSE_PRESSURE_SETTLE_MS)
        │
reading moved > ~0.02 bar? ── yes ──► still settling (leak, slow drain): wait another 30 s
        │ no
        ▼
calibratePressureZero() ── reading within ±0.2 V of 0.5 V? ── no ──► keep the previous zero
        │ yes
        ▼
new zero-pressure reference ── save to EEPROM (survives reboot)
```

If `onPumpState(true)` is called before the 30 s completes, the settle is cancelled — the pump restarted and readings are no longer stable.

**Why the checks matter (added in 1.1.2).** Whatever the library reads at the end of the settle becomes "0 bar". If pressure is still trapped in the pipe — a closed valve, a check valve, a clogged filter, a leaking test fitting — that pressure would be stored as zero and **every later reading would be too low by that amount**: high-pressure protection could miss a real overpressure, and a normal running pressure could read as 0 (false dry-run alarms). Since 1.1.2 a new zero is accepted only if:

- the reading is within **±0.2 V of the transducer's 0.5 V** output (`APASENSE_PRESSURE_ZERO_WINDOW`, about ±0.35 bar with a 6.9 bar sensor), and
- it changed by no more than **~0.02 bar during the last 10 s** (`APASENSE_PRESSURE_STABLE_COUNTS`).

A rejected zero keeps the previous one (or none on a fresh install). A zero stored by an older version that fails the window is ignored at `begin()`.

> **Installation tip:** the pressure must actually fall to 0 bar after the pump stops. If the filter sits below the water level, the static head (water above the sensor) stays on the sensor: a head under ~0.35 bar is accepted as "zero" and all readings are lower by that amount; a higher head is rejected and no zero is taken at all. Mount the sensor where the pressure drops to 0 when the pump is off.

### AC current measurement (RMS)

The ACS712 outputs instantaneous voltage proportional to instantaneous current, centred at VCC/2 = 2.5 V. APASENSE accumulates `(sample − zero)²` over 32 samples, then computes `sqrtf(mean_square) × scale / sensitivity`.

```
begin() → 8-sample average → zero-current offset (the 2.5 V centre)

Each current sample in update():
  centered = raw − zero offset
  sum_sq  += centered²
  count++
  if count == 32:
    current_rms = sqrtf(sum_sq / 32) × (6.144 / 2048) / sensitivity
    reset accumulator
```

`getCurrent()` returns −1.0 until the first full 32-sample cycle completes.

### LDR calibration

The LDR circuit uses a hardware pot (R16) as the primary sensitivity adjustment. Software calibration captures the raw ADC count at two known extremes, then interpolates. Run example `00_ldr_calibration`, note the two values, and hardcode them in your sketch:

```cpp
adc.enableLDR(3, 42, 1520);   // rawDark=42, rawSun=1520 (your measured values)
```

`getSolarPct()` maps the current raw count linearly to 0–100 % using those references.

---

## Quick Start

```cpp
#include <APASENSE.h>

ApaSense adc;

void setup() {
    // Configure channels BEFORE begin() — current auto-zero runs in begin()
    adc.enablePressure();   // AIN0, 0–6.9 bar
    adc.enableCurrent();    // AIN1, ACS712-20A

    adc.begin();
}

void loop() {
    adc.update();   // call every loop() — never use delay()

    float pressure = adc.getPressure();   // bar, or -1.0 until calibrated
    float current  = adc.getCurrent();    // amps RMS, or -1.0 until ready
}
```

### Connecting to APAPUMP

```cpp
// In setup(), after adc.begin():
pump.enablePressure([]() { return adc.getPressure(); }, 4.0f);
pump.setCurrentCallback([]() { return adc.getCurrent(); });
pump.setPumpStateCallback([](bool on) { adc.onPumpState(on); });

pump.setPumpAlarmCallback([](PumpAlarm a) {
    bool alarm = (a != PUMP_ALARM_NONE);
    adc.setLed(0, alarm);
    if (alarm) adc.alert(BUZZER_ALARM, true);   // repeat until cleared
    else        adc.stopAlert();
});
```

### Passive buzzer (Mega2560, Timer3)

An **active** buzzer has its own oscillator and sounds on plain DC — `adc.enableBuzzer(pin)` is all it needs. A **passive** (magnetic) buzzer needs a square wave at its rated frequency; on plain DC it only ticks. Use the optional Timer3 driver:

```cpp
#include <APASENSE.h>
#include <ApaSenseTimer3Buzzer.h>   // in ONE file only — it defines the Timer3 interrupt

void setup() {
    timer3Buzzer.begin(6, 4000);            // pin, buzzer's rated frequency (Hz)
    adc.setBuzzerDriver(timer3Buzzer.set);  // instead of enableBuzzer(); before adc.begin()
    adc.begin();
    adc.alert(BUZZER_MORSE_N);              // every beep(), alert() and setBuzzer() now sounds
}
```

**Why not `tone()`:** on AVR, Arduino `tone()` uses Timer2, and `tone()`/`noTone()` disconnect the PWM of the Timer2 pins — D9/D10 on the Mega, D3/D11 on the Uno. A pump or LED dimmed on those pins stops on every beep. The Timer3 driver leaves Timer2 alone. In return, nothing else may use Timer3 (Servo on the Mega, TimerThree, `analogWrite()` on D2/D3/D5). Boards without Timer3 (Uno) get a clear compile error — write your own `fn(bool on)` for `setBuzzerDriver()` there. See example `04_passive_buzzer`.

| Pattern | Sound | Suggested meaning |
|---------|-------|-------------------|
| `BUZZER_CONFIRM` | Morse E `.` | operator action accepted |
| `BUZZER_MORSE_N` | Morse N `-.` | normal / all clear |
| `BUZZER_MORSE_W` | Morse W `.--` | warning |
| `BUZZER_MORSE_F` | Morse F `..-.` | fault / alarm |

### Connecting tank sensors to APADOSE

```cpp
adc.enableTankSensor(0);   // PCF P0, active-low (default)

dose_ph.setTankEmptyCallback([]() { return adc.isTankEmpty(0); });
```

---

## API Reference

See [docs/API.md](docs/API.md) for the complete method reference.

| Group | Methods |
|-------|---------|
| Core | `begin()`, `update()` |
| ADS channels | `enablePressure()`, `enableCurrent()`, `enableAux()`, `enableLDR()` |
| Getters | `getPressure()`, `getCurrent()`, `getPower()`, `getAuxVoltage()`, `getSolarPct()`, `getRawLDR()` |
| Pressure cal | `calibratePressureZero()`, `onPumpState()` |
| Tank sensors | `enableTankSensor()`, `isTankEmpty()` |
| LEDs | `setLed()`, `getLed()` |
| Buzzer | `enableBuzzer()`, `setBuzzerDriver()`, `setBuzzer()`, `beep()`, `alert()`, `stopAlert()` |
| Timer3 driver (optional header) | `timer3Buzzer.begin()`, `timer3Buzzer.set()` |

---

## Platform Verification

Measured with `examples/01_minimal` (pressure + current enabled).

| Platform | RAM used | Flash used |
|----------|----------|------------|
| Arduino Mega 2560 | 481 B / 8 192 B (6 %) | 9 300 B / 253 952 B (4 %) |
| Arduino Uno | 481 B / 2 048 B (23 %) | 8 552 B / 32 256 B (27 %) |
| ESP32 | 21.9 KB / 320 KB (7 %) | 297 KB / 1.3 MB (23 %) |
| ESP8266 | 28.7 KB / 80 KB (35 %) | 274 KB / 1.0 MB (26 %) |
| STM32 (Nucleo F411RE) | 9.7 KB / 128 KB (7 %) | 24 KB / 512 KB (5 %) |

---

## License

| Use | Terms |
|-----|-------|
| Personal, private, educational, hobby | Free — use, copy, modify, distribute |
| Commercial (selling hardware, paid services, OEM, revenue-generating) | Requires separate written agreement — strictly prohibited without it |

Contact for commercial licensing: [kecup@vazac.eu](mailto:kecup@vazac.eu)

---

*APASENSE — APA Devices · [kecup@vazac.eu](mailto:kecup@vazac.eu)*
