#ifndef PWM_H
#define PWM_H

#include <stdint.h>
#include <stdbool.h>
#include "config.h"

/*
 * Software PWM + 1 ms tick (both driven from TCA0 overflow ISR).
 *
 * The pattern layer requests per-LED on/off state via pwm_set_lit*().
 * The ISR ANDs that against the I2C override mask (0x21) and a master
 * brightness compare (0x20 scaled by a per-pattern envelope) to decide
 * pin state on each tick.
 */

/* Lifecycle */
void     pwm_init(void);

/* Pattern layer: which LEDs would like to be on (bit per led_index). */
void     pwm_set_lit(uint8_t led_idx, bool lit);
void     pwm_set_lit_mask(uint8_t mask);
void     pwm_all_off(void);

/* Master brightness (I2C reg 0x20). 0x00 = off, 0xFF = full. */
void     pwm_set_brightness(uint8_t value);
uint8_t  pwm_get_brightness(void);

/* Per-pattern envelope, 0x00..0xFF. Multiplied with master brightness in
 * the ISR. Patterns 1/3/4 leave this at 0xFF; Pattern 2 modulates it for
 * the heartbeat fade so the master 0x20 still scales the result. */
void     pwm_set_pattern_brightness(uint8_t value);

/* I2C override (reg 0x21). Non-zero values force exact LED state per bits 4..0.
 * 0x00 clears the override and resumes the active pattern. */
void     pwm_set_override(uint8_t mask);

/* Millisecond tick. Wraps at 65535; use (uint16_t)(now - then) for deltas. */
uint16_t pwm_now_ms(void);

#endif /* PWM_H */
