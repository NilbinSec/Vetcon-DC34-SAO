#ifndef MORSE_H
#define MORSE_H

/*
 * Morse transmitter on PB3 (GPIO1), mirrored on PB2 (GPIO2).
 * Message: "GWOT.ENERGY" at 10 WPM (spec Section 6).
 *
 * Non-blocking: morse_task() advances a small state machine off a
 * millisecond tick. Runs continuously, independent of LED patterns.
 */

void morse_init(void);
void morse_task(void);

#endif /* MORSE_H */
