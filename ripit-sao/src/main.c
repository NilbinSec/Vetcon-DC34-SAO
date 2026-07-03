#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/cpufunc.h>

#include "config.h"
#include "pwm.h"
#include "button.h"
#include "patterns.h"
#include "morse.h"
#include "i2c_slave.h"

static void init_clock(void) {
    /* 10 MHz: OSC20M with /2 prescaler. */
    _PROTECTED_WRITE(CLKCTRL.MCLKCTRLA, CLKCTRL_CLKSEL_OSC20M_gc);
    _PROTECTED_WRITE(CLKCTRL.MCLKCTRLB,
                     CLKCTRL_PDIV_2X_gc | CLKCTRL_PEN_bm);
}

static void init_gpio(void) {
    /* LED outputs on PORTA: D5 PA4, D1 PA5, D2 PA6, D3 PA7. */
    PORTA.DIRSET = LED_D5_PIN_bm | LED_D1_PIN_bm
                 | LED_D2_PIN_bm | LED_D3_PIN_bm;
    PORTA.OUTCLR = LED_D5_PIN_bm | LED_D1_PIN_bm
                 | LED_D2_PIN_bm | LED_D3_PIN_bm;

    /* LED + Morse outputs on PORTB: D4 PB0, GPIO2 PB2, GPIO1 PB3. */
    PORTB.DIRSET = LED_D4_PIN_bm | MORSE_TX_bm | MORSE_MIR_bm;
    PORTB.OUTCLR = LED_D4_PIN_bm | MORSE_TX_bm | MORSE_MIR_bm;

    /* SW1 input on PB1 with internal pull-up. */
    PORTB.PIN1CTRL = PORT_PULLUPEN_bm;

    /* PA2/SDA, PA3/SCL: leave default — TWI driver owns them, no internal
     * pull-ups (external pull-ups provided by host badge per spec). */
}

int main(void) {
    init_clock();
    init_gpio();

    pwm_init();
    button_init();
    patterns_init();
    morse_init();
    i2c_slave_init();

    sei();

    for (;;) {
        if (button_pressed_edge()) {
            patterns_advance();
        }
        patterns_task();
        morse_task();
        /* TWI is fully interrupt-driven. */
    }
}
