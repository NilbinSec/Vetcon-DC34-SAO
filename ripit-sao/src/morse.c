#include "morse.h"
#include "pwm.h"
#include "config.h"
#include <avr/io.h>

/*
 * Morse transmitter on MORSE_TX (PB3) mirrored to MORSE_MIR (PB2).
 * Message: "GWOT.ENERGY" @ 10 WPM (spec Section 6).
 *
 *   dot               100 ms
 *   dash              300 ms
 *   intra-character   100 ms (between marks of one letter)
 *   inter-character   300 ms (between letters)
 *   post-message      5000 ms silence, then loop
 *
 * (Message contains no spaces, so the 700 ms inter-word gap never fires.)
 */

#define DOT_MS         100
#define DASH_MS        300
#define INTRA_MS       100
#define INTERCHAR_MS   300
#define POSTMSG_MS     5000

static const char msg[] = "GWOT.ENERGY";
#define MSG_LEN       (sizeof(msg) - 1)

static const char *morse_for(char c) {
    switch (c) {
        case 'G': return "--.";
        case 'W': return ".--";
        case 'O': return "---";
        case 'T': return "-";
        case '.': return ".-.-.-";
        case 'E': return ".";
        case 'N': return "-.";
        case 'R': return ".-.";
        case 'Y': return "-.--";
        default:  return "";
    }
}

typedef enum {
    PH_BEGIN,        /* start of message; next event time == now */
    PH_MARK,         /* key is currently down */
    PH_INTRA_GAP,    /* gap between marks within one character */
    PH_INTER_CHAR,   /* gap between characters */
    PH_POST_MSG,     /* 5 s silence after last character */
} morse_phase_t;

static morse_phase_t phase       = PH_BEGIN;
static uint16_t      next_evt_ms = 0;
static uint8_t       c_idx       = 0;
static uint8_t       s_idx       = 0;
static const char   *cur_p       = "";

static void key_down(void) {
    MORSE_TX_PORT.OUTSET  = MORSE_TX_bm;
    MORSE_MIR_PORT.OUTSET = MORSE_MIR_bm;
}

static void key_up(void) {
    MORSE_TX_PORT.OUTCLR  = MORSE_TX_bm;
    MORSE_MIR_PORT.OUTCLR = MORSE_MIR_bm;
}

void morse_init(void) {
    key_up();
    phase       = PH_BEGIN;
    c_idx       = 0;
    s_idx       = 0;
    cur_p       = "";
    next_evt_ms = pwm_now_ms();
}

static void begin_mark(uint16_t now) {
    /* Caller already set cur_p and s_idx to point at the next symbol. */
    char sym = cur_p[s_idx];
    key_down();
    phase       = PH_MARK;
    next_evt_ms = now + ((sym == '-') ? DASH_MS : DOT_MS);
}

void morse_task(void) {
    uint16_t now = pwm_now_ms();
    if ((int16_t)(now - next_evt_ms) < 0) return;

    switch (phase) {
        case PH_BEGIN:
            c_idx = 0;
            cur_p = morse_for(msg[c_idx]);
            s_idx = 0;
            if (*cur_p == '\0') {
                phase       = PH_INTER_CHAR;
                next_evt_ms = now + INTERCHAR_MS;
                key_up();
            } else {
                begin_mark(now);
            }
            return;

        case PH_MARK:
            key_up();
            s_idx++;
            if (cur_p[s_idx] == '\0') {
                /* End of this character. */
                c_idx++;
                if (c_idx >= MSG_LEN) {
                    phase       = PH_POST_MSG;
                    next_evt_ms = now + POSTMSG_MS;
                } else {
                    phase       = PH_INTER_CHAR;
                    next_evt_ms = now + INTERCHAR_MS;
                }
            } else {
                phase       = PH_INTRA_GAP;
                next_evt_ms = now + INTRA_MS;
            }
            return;

        case PH_INTRA_GAP:
            begin_mark(now);
            return;

        case PH_INTER_CHAR:
            cur_p = morse_for(msg[c_idx]);
            s_idx = 0;
            if (*cur_p == '\0') {
                /* Skip unknown character — treat as extra inter-char gap. */
                c_idx++;
                if (c_idx >= MSG_LEN) {
                    phase       = PH_POST_MSG;
                    next_evt_ms = now + POSTMSG_MS;
                } else {
                    next_evt_ms = now + INTERCHAR_MS;
                }
            } else {
                begin_mark(now);
            }
            return;

        case PH_POST_MSG:
            phase       = PH_BEGIN;
            next_evt_ms = now;
            return;
    }
}
