#ifndef PATTERNS_H
#define PATTERNS_H

#include <stdint.h>

/*
 * LED pattern engine (spec Section 4).
 *
 * patterns_task() is called every main-loop iteration; the active pattern
 * owns a non-blocking state machine driven off a millisecond tick. Pattern
 * switching resets the active pattern's state to its first step.
 */

void    patterns_init(void);
void    patterns_task(void);

/* Select pattern 1..4. Out-of-range values are ignored. */
void    patterns_set(uint8_t id);
uint8_t patterns_get(void);

/* Advance to next pattern (1->2->3->4->1). Used by button. */
void    patterns_advance(void);

#endif /* PATTERNS_H */
