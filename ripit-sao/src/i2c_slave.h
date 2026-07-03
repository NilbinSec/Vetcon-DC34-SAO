#ifndef I2C_SLAVE_H
#define I2C_SLAVE_H

#include <stdint.h>

/*
 * TWI slave at 0x69 (spec Section 7).
 *
 * Standard SMBus-style register interface:
 *   master writes a 1-byte register pointer, then either
 *     - writes 1+ data bytes (register pointer auto-increments), or
 *     - issues a repeated start and reads N bytes (auto-increments).
 *
 * Register map lives entirely inside i2c_slave.c. Writes are dispatched
 * to the appropriate module (patterns_set, pwm_set_brightness, etc.).
 */

void i2c_slave_init(void);

/* No polling task — TWI is fully interrupt-driven. */

#endif /* I2C_SLAVE_H */
