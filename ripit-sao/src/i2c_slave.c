#include "i2c_slave.h"
#include "config.h"
#include "patterns.h"
#include "pwm.h"
#include <avr/io.h>
#include <avr/interrupt.h>

/*
 * Bit-banged I2C slave on PA2 (SDA) and PA3 (SCL).
 *
 * The ATtiny1614 TWI peripheral can only route to (PB0/PB1) default or
 * (PA1/PA2) alternate — neither matches the board's wiring of
 * SDA=PA2 / SCL=PA3. So we drive the slave protocol in software via the
 * PORTA pin-change ISR.
 *
 * Open-drain emulation:
 *   "high" = pin set as input (DIR=0); external pull-up holds the bus high.
 *   "low"  = pin set as output (DIR=1); the OUT register is held at 0,
 *            so DIRSET drives the bus low.
 *
 * Bus speed:
 *   Designed for 100 kHz. The spec's "tolerate up to 400 kHz" cannot be
 *   met by a software bit-bang on a 10 MHz core — at 400 kHz the SCL low
 *   period is ~1.3 us, shorter than ISR latency. If the host badge runs
 *   above ~125 kHz, slow it down.
 *
 * No clock stretching: this slave never drives SCL.
 */

#define I2C_PORT     PORTA
#define SDA_bm       PIN2_bm
#define SCL_bm       PIN3_bm

static const char hint_str[] = "VETCON DC34 // 2PAC & KYBR";   /* 26 + null */
static const char egg_str[]  = "Hit 'Em Up";                    /* 10 + null = 11 */

#define HINT_LEN        26
#define HINT_REG_BASE   0x80
#define HINT_REG_LAST   0x99

#define EGG_LEN         11      /* 10 chars plus trailing null */
#define EGG_REG         0xFF

/* ---- register-interface state ---- */
static volatile uint8_t reg_ptr      = 0;
static volatile uint8_t w10_step     = 0;     /* 0 = expecting 'W' magic, 1 = pattern id */
static volatile uint8_t w10_magic_ok = 0;
static volatile uint8_t egg_off      = 0;
static volatile uint8_t write_phase  = 0;     /* 0 = next byte is reg pointer */

/* ---- bit-bang state machine ---- */
typedef enum {
    BB_IDLE,            /* waiting for START */
    BB_RX_BITS,         /* receiving bits of a byte */
    BB_RX_DRIVE,        /* byte complete; on next falling, drive SDA low (ACK) */
    BB_RX_HELD,         /* holding SDA low for ACK; on next falling, release & advance */
    BB_TX_BITS,         /* transmitting bits of a byte */
    BB_TX_ACK_OK,       /* released SDA; rising will sample master's ACK */
    BB_TX_ACK_NACK,     /* master NACK'd; next falling, go idle */
} bb_state_t;

static volatile bb_state_t bb_state       = BB_IDLE;
static volatile uint8_t    bb_shift       = 0;
static volatile uint8_t    bb_bit_n       = 0;
static volatile uint8_t    bb_byte_num    = 0;     /* 0 = address byte, 1+ = data */
static volatile uint8_t    bb_master_read = 0;

/* ---- SDA line control ---- */
static inline void sda_release(void) {
    I2C_PORT.DIRCLR = SDA_bm;
}
static inline void sda_drive_low(void) {
    I2C_PORT.DIRSET = SDA_bm;
}
static inline void sda_drive_bit(uint8_t bit) {
    if (bit) I2C_PORT.DIRCLR = SDA_bm;
    else     I2C_PORT.DIRSET = SDA_bm;
}

/* ---- register read/write ---- */

static uint8_t read_byte(void) {
    uint8_t v;
    if (reg_ptr == EGG_REG) {
        v = (egg_off < EGG_LEN) ? (uint8_t)egg_str[egg_off] : 0xFF;
        if (egg_off < EGG_LEN) egg_off++;
        return v;
    }
    switch (reg_ptr) {
        case 0x00: v = 0x01;           break;
        case 0x01: v = (uint8_t)'V';   break;
        case 0x02: v = (uint8_t)'C';   break;
        case 0x03: v = (uint8_t)'R';   break;
        case 0x04: v = (uint8_t)'I';   break;
        case 0x05: v = 0xBB;           break;
        case 0x06: v = 0x00;           break;
        case 0x07: v = 0x00;           break;
        case 0x11: v = patterns_get(); break;
        default:
            if (reg_ptr >= HINT_REG_BASE && reg_ptr <= HINT_REG_LAST) {
                v = (uint8_t)hint_str[reg_ptr - HINT_REG_BASE];
            } else {
                v = 0xFF;
            }
            break;
    }
    reg_ptr++;
    return v;
}

static void handle_write(uint8_t d) {
    if (write_phase == 0) {
        reg_ptr      = d;
        write_phase  = 1;
        w10_step     = 0;
        w10_magic_ok = 0;
        egg_off      = 0;
        return;
    }
    switch (reg_ptr) {
        case 0x10:
            if (w10_step == 0) {
                w10_magic_ok = (d == 0x57);    /* 'W' */
                w10_step     = 1;
            } else {
                if (w10_magic_ok) patterns_set(d);
                w10_step     = 0;
                w10_magic_ok = 0;
                reg_ptr++;
            }
            return;
        case 0x20: pwm_set_brightness(d); reg_ptr++; return;
        case 0x21: pwm_set_override(d);   reg_ptr++; return;
        default:                          reg_ptr++; return;
    }
}

/* ---- bit-bang helpers ---- */

static inline void start_tx_byte(void) {
    bb_shift = read_byte();
    sda_drive_bit((uint8_t)(bb_shift >> 7));
    bb_shift = (uint8_t)(bb_shift << 1);
    bb_bit_n = 0;
    bb_state = BB_TX_BITS;
}

static inline void process_rx_byte(void) {
    if (bb_byte_num == 0) {
        uint8_t addr    = (uint8_t)(bb_shift >> 1);
        bb_master_read  = (uint8_t)(bb_shift & 1);
        if (addr != I2C_SLAVE_ADDR) {
            bb_state = BB_IDLE;
            return;
        }
        write_phase  = 0;
        w10_step     = 0;
        w10_magic_ok = 0;
        egg_off      = 0;
    } else {
        handle_write(bb_shift);
    }
    bb_state = BB_RX_DRIVE;
}

/* ---- init ---- */

void i2c_slave_init(void) {
    /* SDA and SCL as inputs (high-Z); external pull-ups required.
     * OUT register cleared for both, so DIRSET drives the line low. */
    I2C_PORT.DIRCLR   = SDA_bm | SCL_bm;
    I2C_PORT.OUTCLR   = SDA_bm | SCL_bm;
    I2C_PORT.PIN2CTRL = PORT_ISC_BOTHEDGES_gc;   /* SDA edges */
    I2C_PORT.PIN3CTRL = PORT_ISC_BOTHEDGES_gc;   /* SCL edges */
}

/* ---- ISR ----
 * Single PORTA pin-change vector handles both SDA and SCL edges.
 * SDA edges are only meaningful while SCL is high (START / STOP);
 * SCL edges drive the data-bit and ACK state machine.
 */

ISR(PORTA_PORT_vect) {
    uint8_t flags = PORTA.INTFLAGS;
    PORTA.INTFLAGS = flags;                    /* W1C — clear immediately */

    uint8_t in  = PORTA.IN;
    uint8_t sda = (uint8_t)((in & SDA_bm) ? 1 : 0);
    uint8_t scl = (uint8_t)((in & SCL_bm) ? 1 : 0);

    /* --- SDA edge: START or STOP detection (ignored if SCL low) --- */
    if (flags & SDA_bm) {
        if (scl) {
            if (!sda) {
                /* START: SDA falling while SCL high. */
                bb_state       = BB_RX_BITS;
                bb_shift       = 0;
                bb_bit_n       = 0;
                bb_byte_num    = 0;
                bb_master_read = 0;
                sda_release();
                write_phase    = 0;
                w10_step       = 0;
                w10_magic_ok   = 0;
                egg_off        = 0;
            } else {
                /* STOP: SDA rising while SCL high. */
                bb_state = BB_IDLE;
                sda_release();
            }
        }
    }

    /* --- SCL edge: clock data and ACK --- */
    if (flags & SCL_bm) {
        if (scl) {
            /* Rising edge: master is asserting / sampling. */
            switch (bb_state) {
                case BB_RX_BITS:
                    bb_shift = (uint8_t)((bb_shift << 1) | sda);
                    bb_bit_n++;
                    if (bb_bit_n == 8) {
                        process_rx_byte();
                    }
                    break;
                case BB_TX_BITS:
                    /* Master samples the bit we already placed. */
                    bb_bit_n++;
                    break;
                case BB_TX_ACK_OK:
                    /* 9th rising: read master's ACK level. */
                    if (sda) bb_state = BB_TX_ACK_NACK;
                    break;
                default:
                    break;
            }
        } else {
            /* Falling edge: time-critical — must finish before next rising. */
            switch (bb_state) {
                case BB_RX_DRIVE:
                    sda_drive_low();
                    bb_state = BB_RX_HELD;
                    break;
                case BB_RX_HELD:
                    sda_release();
                    bb_byte_num++;
                    if (bb_master_read) {
                        start_tx_byte();
                    } else {
                        bb_shift = 0;
                        bb_bit_n = 0;
                        bb_state = BB_RX_BITS;
                    }
                    break;
                case BB_TX_BITS:
                    if (bb_bit_n == 8) {
                        sda_release();          /* let master drive ACK */
                        bb_state = BB_TX_ACK_OK;
                    } else {
                        sda_drive_bit((uint8_t)(bb_shift >> 7));
                        bb_shift = (uint8_t)(bb_shift << 1);
                    }
                    break;
                case BB_TX_ACK_OK:
                    /* Master ACK'd previous byte — send the next one. */
                    bb_byte_num++;
                    start_tx_byte();
                    break;
                case BB_TX_ACK_NACK:
                    bb_state = BB_IDLE;
                    sda_release();
                    break;
                default:
                    break;
            }
        }
    }
}
