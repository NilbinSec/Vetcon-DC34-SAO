#ifndef BUTTON_H
#define BUTTON_H

#include <stdbool.h>

/*
 * SW1 (PB1) — active-LOW, internal pull-up, 25 ms software debounce,
 * edge-triggered on falling edge (spec Section 5).
 */

void button_init(void);

/* Call from main loop. Returns true exactly once per confirmed press. */
bool button_pressed_edge(void);

#endif /* BUTTON_H */
