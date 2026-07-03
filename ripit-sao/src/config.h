#ifndef CONFIG_H
#define CONFIG_H

#include <avr/io.h>
#include <stdint.h>

#ifndef F_CPU
#define F_CPU 10000000UL
#endif

/* ====== LED pin map (spec Section 2) ====== */
/* Top row, left to right on the can */
#define LED_D5_PORT    PORTA
#define LED_D5_PIN_bm  PIN4_bm

#define LED_D1_PORT    PORTA
#define LED_D1_PIN_bm  PIN5_bm

#define LED_D2_PORT    PORTA
#define LED_D2_PIN_bm  PIN6_bm

#define LED_D3_PORT    PORTA
#define LED_D3_PIN_bm  PIN7_bm

/* Center hero — P of RIP, amber */
#define LED_D4_PORT    PORTB
#define LED_D4_PIN_bm  PIN0_bm

/* ====== SAO J1 GPIO (Morse transmit + mirror) ====== */
#define MORSE_TX_PORT  PORTB
#define MORSE_TX_bm    PIN3_bm   /* GPIO1 — primary Morse output */

#define MORSE_MIR_PORT PORTB
#define MORSE_MIR_bm   PIN2_bm   /* GPIO2 — mirrors GPIO1 */

/* ====== Button SW1 ====== */
#define SW1_PORT       PORTB
#define SW1_bm         PIN1_bm

/* ====== LED index ordering used by pwm / patterns ====== */
enum led_index {
    LED_IDX_D5 = 0,   /* bit 4 in override register */
    LED_IDX_D1 = 1,   /* bit 3 */
    LED_IDX_D2 = 2,   /* bit 2 */
    LED_IDX_D3 = 3,   /* bit 1 */
    LED_IDX_D4 = 4,   /* bit 0 */
    NUM_LEDS   = 5
};

/* ====== Pattern IDs ====== */
enum pattern_id {
    PATTERN_KNIGHT_RIDER = 0x01,
    PATTERN_HEARTBEAT    = 0x02,
    PATTERN_TRANSMISSION = 0x03,
    PATTERN_NIBBLE       = 0x04,
    PATTERN_FIRST        = PATTERN_KNIGHT_RIDER,
    PATTERN_LAST         = PATTERN_NIBBLE
};

/* ====== I2C ====== */
#define I2C_SLAVE_ADDR   0x69

/* ====== Shared constants ====== */
#define BRIGHTNESS_DEFAULT  0xFF
#define LED_OVERRIDE_OFF    0x00   /* no override active */

/* CTF + Easter egg strings live in i2c_slave.c (declared in i2c_slave.h). */
/* The puzzle string and Morse message live in patterns.c / morse.c respectively. */

#endif /* CONFIG_H */
