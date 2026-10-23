// APASENSE — 04_passive_buzzer (Arduino Mega2560 only: needs Timer3)
//
// Plays every buzzer pattern in turn on a PASSIVE (magnetic) buzzer, one every 4 s.
//
// Why a driver: a passive buzzer needs a square wave — digitalWrite() HIGH alone only makes
// it tick. Arduino tone() would make the wave, but on AVR it uses Timer2 and disconnects the
// PWM of the Timer2 pins (Mega D9/D10, Uno D3/D11) — a pump or LED dimmed on those pins
// would stop on every beep. ApaSenseTimer3Buzzer uses Timer3 instead.
//
// An ACTIVE buzzer (built-in oscillator, sounds on plain DC) needs no driver:
//   adc.enableBuzzer(BUZZER_PIN);   instead of the two driver lines below.
//
// Wiring (APA reference board): D6 -> 1k -> NPN transistor -> buzzer -> 5 V, flyback diode
// across the buzzer.

#include <APASENSE.h>
#include <ApaSenseTimer3Buzzer.h>   // include in ONE file only (defines the Timer3 ISR)

const uint8_t       BUZZER_PIN   = 6;      // passive buzzer driver input
const uint16_t      BUZZER_HZ    = 4000;   // the buzzer's rated (loudest) frequency
const unsigned long DEMO_STEP_MS = 4000;   // longer than the longest pattern (~2.6 s)

ApaSense adc;

const BuzzerAlert DEMO[] = {
    BUZZER_CONFIRM, BUZZER_MORSE_N, BUZZER_MORSE_W, BUZZER_MORSE_F,
    BUZZER_INFO, BUZZER_WARNING, BUZZER_ALARM
};
const char N0[] PROGMEM = "BUZZER_CONFIRM  Morse E  .     (operator action accepted)";
const char N1[] PROGMEM = "BUZZER_MORSE_N  Morse N  -.    (normal)";
const char N2[] PROGMEM = "BUZZER_MORSE_W  Morse W  .--   (warning)";
const char N3[] PROGMEM = "BUZZER_MORSE_F  Morse F  ..-.  (fault / alarm)";
const char N4[] PROGMEM = "BUZZER_INFO     1 beep";
const char N5[] PROGMEM = "BUZZER_WARNING  2 beeps";
const char N6[] PROGMEM = "BUZZER_ALARM    3 beeps";
const char* const DEMO_NAME[] = { N0, N1, N2, N3, N4, N5, N6 };
const uint8_t DEMO_LEN = sizeof(DEMO) / sizeof(DEMO[0]);

uint8_t       demoIdx  = 0;
unsigned long demoLast = 0;

void playDemo() {
    Serial.println((const __FlashStringHelper*)DEMO_NAME[demoIdx]);
    adc.alert(DEMO[demoIdx]);              // plays once; alert(x, true) repeats until stopAlert()
    demoIdx = (demoIdx + 1) % DEMO_LEN;
}

void setup() {
    Serial.begin(115200);
    timer3Buzzer.begin(BUZZER_PIN, BUZZER_HZ);
    adc.setBuzzerDriver(timer3Buzzer.set);  // before adc.begin()
    adc.begin();
    Serial.println(F("APASENSE passive buzzer demo (Timer3, no tone())"));
    playDemo();
    demoLast = millis();
}

void loop() {
    adc.update();                           // runs the pattern timing — call every loop()
    if (millis() - demoLast >= DEMO_STEP_MS) {
        demoLast = millis();
        playDemo();
    }
}
