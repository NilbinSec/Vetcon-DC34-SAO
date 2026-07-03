#include "button.h"
#include "config.h"
#include "pwm.h"
#include <avr/io.h>

/*
 * SW1 (PB1, active-LOW with internal pull-up).
 * 25 ms software debounce, edge-triggered on the falling edge.
 * The pin direction + pull-up are configured in main.c::init_gpio().
 */

#define DEBOUNCE_MS  25

static uint8_t  last_raw    = 1;   /* 1 = released (pin high) */
static uint8_t  stable      = 1;
static uint16_t change_ms   = 0;
static bool     pending     = false;

void button_init(void) {
    last_raw  = (SW1_PORT.IN & SW1_bm) ? 1 : 0;
    stable    = last_raw;
    change_ms = pwm_now_ms();
    pending   = false;
}

bool button_pressed_edge(void) {
    uint8_t  raw = (SW1_PORT.IN & SW1_bm) ? 1 : 0;
    uint16_t now = pwm_now_ms();

    if (raw != last_raw) {
        last_raw  = raw;
        change_ms = now;
    } else if (raw != stable &&
               (uint16_t)(now - change_ms) >= DEBOUNCE_MS) {
        stable = raw;
        if (stable == 0) {
            pending = true;
        }
    }

    if (pending) {
        pending = false;
        return true;
    }
    return false;
}
