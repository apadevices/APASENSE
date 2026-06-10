// APASENSE — Example 02: Pool sensor monitoring (intermediate)
//
// Demonstrates the full sensing side of an APA pool system:
//   - Pressure and current from ADS1015
//   - All four chemical tank-empty sensors via PCF8574AT (P0–P3)
//   - Status LEDs via PCF8574AT (P4–P7)
//   - Buzzer alert on tank empty
//
// The tank-empty callbacks shown here are the exact lines you would
// use when adding APADOSE to a full system.
//
// Hardware:
//   ADS1015    at I2C 0x4B  (default)
//   PCF8574AT  at I2C 0x3B  (default)
//   Buzzer     on D5

#include <APASENSE.h>

ApaSense adc;

// LED assignments (index 0-3 maps to PCF P4-P7)
static constexpr uint8_t LED_SYSTEM = 0;  // P4 — green: system active
static constexpr uint8_t LED_PH     = 1;  // P5 — red:   pH tank empty
static constexpr uint8_t LED_CL     = 2;  // P6 — red:   CL tank empty
static constexpr uint8_t LED_FLOC   = 3;  // P7 — red:   flocculant tank empty

void setup() {
    Serial.begin(115200);

    // ── ADS1015 channels ────────────────────────────────────────────────
    adc.enablePressure();    // AIN0
    adc.enableCurrent();     // AIN1

    // ── Tank sensors — all active-low (optocoupler pulls LOW when empty) ─
    adc.enableTankSensor(0); // P0 — pH
    adc.enableTankSensor(1); // P1 — CL (chlorine)
    adc.enableTankSensor(2); // P2 — flocculant
    adc.enableTankSensor(3); // P3 — algicide / DI AUX

    // ── Buzzer ──────────────────────────────────────────────────────────
    adc.enableBuzzer(5);

    adc.begin();

    // Signal startup: system LED on + short beep
    adc.setLed(LED_SYSTEM, true);
    adc.beep(200);

    Serial.println(F("APASENSE pool sensor monitor started."));
    Serial.println(F("-------------------------------------"));

    // ── APADOSE bridge (uncomment when APADOSE is included) ─────────────
    // dose_ph.setTankEmptyCallback([]() { return adc.isTankEmpty(0); });
    // dose_cl.setTankEmptyCallback([]() { return adc.isTankEmpty(1); });
    // dose_floc.setTankEmptyCallback([]() { return adc.isTankEmpty(2); });
    // dose_alg.setTankEmptyCallback([]() { return adc.isTankEmpty(3); });
}

void loop() {
    adc.update();

    // ── Read all tank states once per loop (live PCF reads) ─────────────
    bool emPh   = adc.isTankEmpty(0);
    bool emCl   = adc.isTankEmpty(1);
    bool emFloc = adc.isTankEmpty(2);
    bool emAlg  = adc.isTankEmpty(3); // no dedicated LED — only 3 tank LEDs available
                                       // (LED_SYSTEM takes slot 0); beep covers algicide

    // ── Reflect tank states on LEDs ──────────────────────────────────────
    adc.setLed(LED_PH,   emPh);
    adc.setLed(LED_CL,   emCl);
    adc.setLed(LED_FLOC, emFloc);

    // ── Beep once when a tank first goes empty ───────────────────────────
    static bool prevEmpty = false;
    bool anyEmpty = emPh || emCl || emFloc || emAlg;
    if (anyEmpty && !prevEmpty) adc.beep(500);
    prevEmpty = anyEmpty;

    // ── Serial report every 2 seconds ───────────────────────────────────
    static uint32_t lastPrint = 0;
    if (millis() - lastPrint >= 2000) {
        lastPrint = millis();

        // Pressure
        float p = adc.getPressure();
        Serial.print(F("P: "));
        if (p < 0) Serial.print(F("--   "));
        else { Serial.print(p, 2); Serial.print(F(" bar  ")); }

        // Current
        float i = adc.getCurrent();
        Serial.print(F("I: "));
        if (i < 0) Serial.print(F("--   "));
        else { Serial.print(i, 2); Serial.print(F(" A  ")); }

        // Tank states (using values already read above — no extra I2C)
        Serial.print(F("pH:"));   Serial.print(emPh   ? F("EMPTY ") : F("ok    "));
        Serial.print(F("CL:"));   Serial.print(emCl   ? F("EMPTY ") : F("ok    "));
        Serial.print(F("Floc:")); Serial.print(emFloc ? F("EMPTY ") : F("ok    "));
        Serial.print(F("Alg:"));  Serial.println(emAlg ? F("EMPTY") : F("ok"));
    }
}
