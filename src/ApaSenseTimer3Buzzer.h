#pragma once
// Optional passive-buzzer driver for APASENSE — AVR boards with Timer3 (Mega2560).
//
// A passive (magnetic) buzzer needs a square wave; digitalWrite() alone only makes it tick.
// Arduino tone() would do it, but on AVR it uses Timer2, and tone()/noTone() disconnect the
// PWM of the Timer2 pins (Mega D9/D10, Uno D3/D11). This driver uses Timer3 instead: CTC
// mode, the compare ISR toggles the buzzer pin. Timer3's own PWM outputs (Mega D2/D3/D5)
// stay disconnected, so encoders or other inputs on those pins keep working.
//
//   #include <ApaSenseTimer3Buzzer.h>
//   timer3Buzzer.begin(6, 4000);            // pin, tone frequency in Hz
//   adc.setBuzzerDriver(timer3Buzzer.set);  // before adc.begin()
//
// Rules:
//   - Include this header in ONE .cpp/.ino file only — it defines the Timer3 compare ISR.
//   - Nothing else may use Timer3 (Servo on the Mega, TimerThree, ...) or analogWrite()
//     on D2/D3/D5.

#include <Arduino.h>

#if !defined(TCCR3A) || !defined(TIMER3_COMPA_vect)
#error "ApaSenseTimer3Buzzer.h needs an AVR board with Timer3 (e.g. Arduino Mega2560)"
#else

namespace apasense_t3 {
static volatile uint8_t* pinReg;   // PINx — writing the bit mask toggles the output
static volatile uint8_t* outReg;   // PORTx — used to leave the pin LOW when silent
static uint8_t           mask;
}

ISR(TIMER3_COMPA_vect) { *apasense_t3::pinReg = apasense_t3::mask; }

struct ApaSenseTimer3Buzzer {
    // Sets the pin to OUTPUT LOW and Timer3 to the tone frequency (timer stopped).
    // hz: about 16 Hz to 65 kHz (Timer3 at prescaler 8); use the buzzer's rated frequency.
    static void begin(uint8_t pin, uint16_t hz) {
        uint8_t port = digitalPinToPort(pin);
        if (port == NOT_A_PIN) return;   // invalid pin: set() stays a no-op
        pinMode(pin, OUTPUT);
        digitalWrite(pin, LOW);
        apasense_t3::pinReg = portInputRegister(port);
        apasense_t3::outReg = portOutputRegister(port);
        apasense_t3::mask   = digitalPinToBitMask(pin);
        TCCR3A = 0;   // COM3A/B/C = 0: Timer3's PWM pins stay disconnected
        TCCR3B = 0;   // stopped until set(true)
        // CTC, prescaler 8; the ISR toggles the pin, so it runs at twice the tone frequency
        uint32_t top = (uint32_t)F_CPU / 8 / 2 / (hz ? hz : 1) - 1;
        OCR3A = top > 0xFFFF ? 0xFFFF : (uint16_t)top;
    }

    // true = start the tone, false = stop it (pin ends LOW). Pass to adc.setBuzzerDriver().
    static void set(bool on) {
        if (!apasense_t3::mask) return;   // begin() not called
        if (on) {
            if (TCCR3B) return;           // already sounding
            TCNT3  = 0;
            TIFR3  = _BV(OCF3A);          // drop a stale compare flag (no instant toggle)
            TCCR3B = _BV(WGM32) | _BV(CS31);
            TIMSK3 |= _BV(OCIE3A);
        } else {
            uint8_t sreg = SREG;          // PORTx is shared with other pins: atomic clear
            cli();
            TIMSK3 &= ~_BV(OCIE3A);
            TCCR3B = 0;
            *apasense_t3::outReg &= ~apasense_t3::mask;   // LOW: driver transistor off
            SREG = sreg;
        }
    }
};

static ApaSenseTimer3Buzzer timer3Buzzer;

#endif
