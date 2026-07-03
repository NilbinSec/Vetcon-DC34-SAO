#include "patterns.h"
#include "pwm.h"
#include "config.h"
#include <string.h>

/* ============================================================
 * Spec Section 4 — four LED patterns.
 *
 *   P1 Knight Rider:       chase D5->D1->D2->D3 then D4 bounce, reverse,
 *                          D4 bounce. 120 ms/step. 10 steps per loop.
 *   P2 Heartbeat:          all five LEDs fade as one (pulse-pulse-rest)
 *                          via pattern envelope. Master brightness still
 *                          scales the result.
 *   P3 Incoming Trans.:    D4 steady; top row lights cumulatively
 *                          (120 ms/step), holds 500 ms, all off 1000 ms.
 *   P4 Binary Nibble:      encodes "tinyurl/vetconpuzz" (18 chars).
 *                          Each char = 8 s (high nibble 4 s with D4 on,
 *                          low nibble 4 s with D4 off). 3 s pause then loop.
 * ============================================================ */

static volatile uint8_t current = PATTERN_FIRST;

/* ---- bit-mask helpers ---- */
#define M(idx)  ((uint8_t)(1u << (idx)))

/* Map top-row chase positions (0..3 left-to-right) to led_index. */
static const uint8_t top_pos[4] = {
    LED_IDX_D5, LED_IDX_D1, LED_IDX_D2, LED_IDX_D3
};

/* ---- Pattern 1: Knight Rider ---- */

typedef struct {
    uint16_t step_ms;
    uint8_t  step;      /* 0..9: 0-3 fwd, 4 bounce, 5-8 rev, 9 bounce */
} p1_t;

static p1_t p1;

static uint8_t p1_mask_for(uint8_t step) {
    if (step < 4)      return M(top_pos[step]);
    if (step == 4)     return M(LED_IDX_D4);
    if (step < 9)      return M(top_pos[8 - step]);
    return M(LED_IDX_D4);   /* step == 9 */
}

static void p1_reset(void) {
    p1.step    = 0;
    p1.step_ms = pwm_now_ms();
    pwm_set_lit_mask(p1_mask_for(0));
}

static void p1_step(void) {
    uint16_t now = pwm_now_ms();
    if ((uint16_t)(now - p1.step_ms) < 120) return;
    p1.step_ms = now;
    p1.step = (uint8_t)((p1.step + 1) % 10);
    pwm_set_lit_mask(p1_mask_for(p1.step));
}

/* ---- Pattern 2: Heartbeat ----
 *   t (ms)      phase            envelope
 *   [   0,  80) pulse1 fade up   0   -> 255
 *   [  80, 120) pulse1 hold      255
 *   [ 120, 200) pulse1 fade down 255 -> 0
 *   [ 200, 320) gap              0
 *   [ 320, 400) pulse2 fade up   0   -> 255
 *   [ 400, 440) pulse2 hold      255
 *   [ 440, 520) pulse2 fade down 255 -> 0
 *   [ 520,1120) long rest        0
 *   period = 1120 ms
 */

typedef struct { uint16_t start_ms; } p2_t;
static p2_t p2;

#define P2_PERIOD_MS  1120

static void p2_reset(void) {
    p2.start_ms = pwm_now_ms();
    pwm_set_lit_mask(0x1F);   /* all five LEDs requested on */
    pwm_set_pattern_brightness(0);
}

static uint8_t p2_envelope(uint16_t t) {
    if (t <   80) return (uint8_t)((uint32_t)t * 255U / 80U);
    if (t <  120) return 255;
    if (t <  200) return (uint8_t)((uint32_t)(200 - t) * 255U / 80U);
    if (t <  320) return 0;
    if (t <  400) return (uint8_t)((uint32_t)(t - 320) * 255U / 80U);
    if (t <  440) return 255;
    if (t <  520) return (uint8_t)((uint32_t)(520 - t) * 255U / 80U);
    return 0;
}

static void p2_step(void) {
    uint16_t now = pwm_now_ms();
    uint16_t t   = (uint16_t)(now - p2.start_ms);
    if (t >= P2_PERIOD_MS) {
        p2.start_ms = now;
        t = 0;
    }
    pwm_set_lit_mask(0x1F);
    pwm_set_pattern_brightness(p2_envelope(t));
}

/* ---- Pattern 3: Incoming Transmission ----
 *   t=  0 ms:  D4 + D5 on
 *   t=120 ms:  + D1
 *   t=240 ms:  + D2
 *   t=360 ms:  + D3 (all top row + D4 lit; hold)
 *   t=860 ms:  all off (D4 off too)
 *   t=1860 ms: loop
 */

typedef struct { uint16_t start_ms; } p3_t;
static p3_t p3;

#define P3_PERIOD_MS  1860
#define P3_OFF_AT_MS  860

static void p3_reset(void) {
    p3.start_ms = pwm_now_ms();
    pwm_set_lit_mask(M(LED_IDX_D4) | M(LED_IDX_D5));
}

static void p3_step(void) {
    uint16_t now = pwm_now_ms();
    uint16_t t   = (uint16_t)(now - p3.start_ms);
    if (t >= P3_PERIOD_MS) {
        p3.start_ms = now;
        t = 0;
    }

    uint8_t mask = 0;
    if (t < P3_OFF_AT_MS) {
        mask  = M(LED_IDX_D4) | M(LED_IDX_D5);
        if (t >= 120) mask |= M(LED_IDX_D1);
        if (t >= 240) mask |= M(LED_IDX_D2);
        if (t >= 360) mask |= M(LED_IDX_D3);
    }
    pwm_set_lit_mask(mask);
}

/* ---- Pattern 4: Binary Nibble CTF puzzle ----
 *   String:  "tinyurl/vetconpuzz" (18 chars)
 *   Per char: high nibble for 4 s (D4 on), low nibble for 4 s (D4 off).
 *   After all 18 chars: 3 s all-off pause, then loop.
 *
 *   Total animated time = 18 * 8 s + 3 s pause = 147 s per cycle.
 *   (The "2:24" claim in the spec covers only the 144 s data portion.)
 */

static const char puzzle_str[] = "tinyurl/vetconpuzz";
#define PUZZLE_LEN     (sizeof(puzzle_str) - 1)   /* 18 */
#define P4_NIBBLE_MS   4000U
#define P4_PAUSE_MS    3000U

typedef struct {
    uint16_t start_ms;
    uint8_t  char_idx;
    uint8_t  high_phase;     /* 1 = showing high nibble, 0 = low */
    uint8_t  in_pause;       /* 1 = end-of-loop 3 s blackout */
} p4_t;

static p4_t p4;

static uint8_t p4_mask_for(uint8_t nibble, uint8_t high_phase) {
    /* Top-row mapping: bit 3 -> D5, bit 2 -> D1, bit 1 -> D2, bit 0 -> D3. */
    uint8_t mask = 0;
    if (nibble & 0x8) mask |= M(LED_IDX_D5);
    if (nibble & 0x4) mask |= M(LED_IDX_D1);
    if (nibble & 0x2) mask |= M(LED_IDX_D2);
    if (nibble & 0x1) mask |= M(LED_IDX_D3);
    if (high_phase)   mask |= M(LED_IDX_D4);
    return mask;
}

static void p4_show_current(void) {
    uint8_t c   = (uint8_t)puzzle_str[p4.char_idx];
    uint8_t nib = p4.high_phase ? (uint8_t)(c >> 4) : (uint8_t)(c & 0x0F);
    pwm_set_lit_mask(p4_mask_for(nib, p4.high_phase));
}

static void p4_reset(void) {
    p4.start_ms   = pwm_now_ms();
    p4.char_idx   = 0;
    p4.high_phase = 1;
    p4.in_pause   = 0;
    p4_show_current();
}

static void p4_step(void) {
    uint16_t now = pwm_now_ms();
    uint16_t t   = (uint16_t)(now - p4.start_ms);

    if (p4.in_pause) {
        if (t >= P4_PAUSE_MS) {
            p4.in_pause   = 0;
            p4.char_idx   = 0;
            p4.high_phase = 1;
            p4.start_ms   = now;
            p4_show_current();
        } else {
            pwm_all_off();
        }
        return;
    }

    if (t < P4_NIBBLE_MS) return;   /* still displaying current nibble */

    /* Advance to next nibble. */
    if (p4.high_phase) {
        p4.high_phase = 0;
    } else {
        p4.high_phase = 1;
        p4.char_idx++;
        if (p4.char_idx >= PUZZLE_LEN) {
            p4.in_pause = 1;
            p4.start_ms = now;
            pwm_all_off();
            return;
        }
    }
    p4.start_ms = now;
    p4_show_current();
}

/* ---- Module API ---- */

void patterns_init(void) {
    current = PATTERN_FIRST;
    memset(&p1, 0, sizeof p1);
    memset(&p2, 0, sizeof p2);
    memset(&p3, 0, sizeof p3);
    memset(&p4, 0, sizeof p4);
    pwm_set_pattern_brightness(BRIGHTNESS_DEFAULT);
    p1_reset();
}

void patterns_set(uint8_t id) {
    if (id < PATTERN_FIRST || id > PATTERN_LAST) return;
    current = id;
    pwm_set_pattern_brightness(BRIGHTNESS_DEFAULT);
    pwm_all_off();
    switch (id) {
        case PATTERN_KNIGHT_RIDER: p1_reset(); break;
        case PATTERN_HEARTBEAT:    p2_reset(); break;
        case PATTERN_TRANSMISSION: p3_reset(); break;
        case PATTERN_NIBBLE:       p4_reset(); break;
        default: break;
    }
}

uint8_t patterns_get(void) {
    return current;
}

void patterns_advance(void) {
    uint8_t next = (current >= PATTERN_LAST) ? PATTERN_FIRST
                                              : (uint8_t)(current + 1);
    patterns_set(next);
}

void patterns_task(void) {
    switch (current) {
        case PATTERN_KNIGHT_RIDER: p1_step(); break;
        case PATTERN_HEARTBEAT:    p2_step(); break;
        case PATTERN_TRANSMISSION: p3_step(); break;
        case PATTERN_NIBBLE:       p4_step(); break;
        default: break;
    }
}
