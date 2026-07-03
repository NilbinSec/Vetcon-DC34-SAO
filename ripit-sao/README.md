# RIP-IT VetCon SAO

Firmware for the RIP-IT Energy Drink themed SAO (NilbinSec / 2PAC + KYBR), DEF CON 34 / VetCon 2026.

- Target: ATtiny1614-SSN @ 10 MHz internal, 3.0 V nominal
- Toolchain: AVR-GCC + avr-libc
- Programming: SerialUPDI (`pyupdi`)

Build, flash, and fuse with the provided `Makefile`:

```
make
make flash PROG_DEV=/dev/ttyUSB0
make fuses PROG_DEV=/dev/ttyUSB0
```

See the firmware specification document for full register map, pin assignments,
pattern descriptions, and acceptance criteria.
