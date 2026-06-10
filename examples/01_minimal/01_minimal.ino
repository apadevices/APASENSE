// APASENSE — Example 01: Minimal
//
// The smallest working sketch: reads pool pressure and pump current,
// prints values to Serial every second without blocking update().
//
// Key rule: call enable*() methods BEFORE begin().
// begin() auto-zeroes the current sensor — it needs to know
// which channel to use and that the pump is off.
//
// Hardware: ADS1015 at 0x4B (default address).

#include <APASENSE.h>

ApaSense adc;

void setup() {
    Serial.begin(115200);

    // Configure channels BEFORE begin() — current zero-cal runs inside begin()
    adc.enablePressure();   // AIN0, 0–6.9 bar (≈100 PSI)
    adc.enableCurrent();    // AIN1, ACS712-20A (100 mV/A)

    adc.begin();
}

void loop() {
    // update() must be called every loop() iteration.
    // Never use delay() — it blocks the ADC cycle and timers.
    adc.update();

    static uint32_t lastPrint = 0;
    if (millis() - lastPrint >= 1000) {
        lastPrint = millis();

        float pressure = adc.getPressure();
        float current  = adc.getCurrent();

        Serial.print(F("Pressure: "));
        if (pressure < 0.0f) Serial.print(F("calibrating..."));
        else { Serial.print(pressure, 2); Serial.print(F(" bar")); }

        Serial.print(F("   Current: "));
        if (current < 0.0f) Serial.print(F("calibrating..."));
        else { Serial.print(current, 2); Serial.print(F(" A")); }

        Serial.println();
    }
}
