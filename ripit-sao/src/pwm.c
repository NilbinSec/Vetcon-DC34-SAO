#include "pwm.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>

/*
 * TCA0 overflow ISR drives:
 *   - 8-bit software PWM on the 5 LED pins (~125 Hz refresh)
 *   - an exact 1 ms tick counter for patterns and Morse
 *
 * Deviation from spec Section 8/9 pseudocode: the spec writes "1 kHz PWM
 * ISR" with an 8-bit counter, which gives a 256 ms PWM period (~4 Hz
 * visible refresh) — flickers badly. We run the ISR at 32 kHz, giving
 * 32000/256 = 125 Hz refresh, well above flicker threshold; the 1 ms
 * tick is derived by counting 32 ISRs (exact, no jitter).
 */

#define PWM_ISR_HZ      32000UL
#define MS_DIVIDER      ((uint8_t)(PWM_ISR_HZ / 1000UL))   /* 32 */
#define TCA0_PER_VAL    ((F_CPU / PWM_ISR_HZ) - 1)         /* 311 @ 10 MHz */

/* Pattern-layer "would like to be on" mask. Bit per led_index. */
static volatile uint8_t lit_mask          = 0;

/* I2C override (reg 0x21). 0 = inactive; otherwise forces LEDs exactly. */
static volatile uint8_t override_mask     = LED_OVERRIDE_OFF;

/* Master brightness (reg 0x20). */
static volatile uint8_t master_brightness = BRIGHTNESS_DEFAULT;

/* Per-pattern envelope; modulated by Pattern 2 for heartbeat. */
static volatile uint8_t pattern_envelope  = BRIGHTNESS_DEFAULT;

/* Millisecond tick. */
static volatile uint16_t ms_counter       = 0;

#define ALL_LEDS_MASK   ((uint8_t)((1u << NUM_LEDS) - 1))  /* 0x1F */

void pwm_init(void) {
    TCA0.SINGLE.PER     = TCA0_PER_VAL;
    TCA0.SINGLE.INTCTRL = TCA_SINGLE_OVF_bm;
    TCA0.SINGLE.CTRLA   = TCA_SINGLE_CLKSEL_DIV1_gc | TCA_SINGLE_ENABLE_bm;
}

void pwm_set_lit(uint8_t led_idx, bool lit) {
    if (led_idx >= NUM_LEDS) return;
    uint8_t bit = (uint8_t)(1u << led_idx);
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        if (lit) lit_mask |= bit;
        else     lit_mask &= (uint8_t)~bit;
    }
}

void pwm_set_lit_mask(uint8_t mask) {
    mask &= ALL_LEDS_MASK;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        lit_mask = mask;
    }
}

void pwm_all_off(void) {
    pwm_set_lit_mask(0);
}

void pwm_set_brightness(uint8_t v) {
    master_brightness = v;   /* single-byte writes are atomic on AVR */
}

uint8_t pwm_get_brightness(void) {
    return master_brightness;
}

void pwm_set_pattern_brightness(uint8_t v) {
    pattern_envelope = v;
}

void pwm_set_override(uint8_t mask) {
    override_mask = mask & ALL_LEDS_MASK;
}

uint16_t pwm_now_ms(void) {
    uint16_t snap;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        snap = ms_counter;
    }
    return snap;
}

ISR(TCA0_OVF_vect) {
    static uint8_t pwm_counter = 0;
    static uint8_t ms_div      = 0;

    /* Effective LED-on mask: override > pattern. */
    uint8_t eff = override_mask ? override_mask : lit_mask;

    /* Effective brightness = master * envelope (0..0xFF * 0..0xFF >> 8). */
    uint8_t eff_b = (uint8_t)(((uint16_t)master_brightness * pattern_envelope) >> 8);

    uint8_t on = (pwm_counter < eff_b) ? eff : 0;

    /* PORTA: D5=PA4, D1=PA5, D2=PA6, D3=PA7. */
    uint8_t pa_on = 0, pa_off = 0;
    if (on & (1u << LED_IDX_D5)) pa_on  |= LED_D5_PIN_bm; else pa_off |= LED_D5_PIN_bm;
    if (on & (1u << LED_IDX_D1)) pa_on  |= LED_D1_PIN_bm; else pa_off |= LED_D1_PIN_bm;
    if (on & (1u << LED_IDX_D2)) pa_on  |= LED_D2_PIN_bm; else pa_off |= LED_D2_PIN_bm;
    if (on & (1u << LED_IDX_D3)) pa_on  |= LED_D3_PIN_bm; else pa_off |= LED_D3_PIN_bm;
    PORTA.OUTSET = pa_on;
    PORTA.OUTCLR = pa_off;

    /* PORTB: D4=PB0. */
    if (on & (1u << LED_IDX_D4)) PORTB.OUTSET = LED_D4_PIN_bm;
    else                          PORTB.OUTCLR = LED_D4_PIN_bm;

    pwm_counter++;
    if (++ms_div >= MS_DIVIDER) {
        ms_div = 0;
        ms_counter++;
    }

    TCA0.SINGLE.INTFLAGS = TCA_SINGLE_OVF_bm;
}
