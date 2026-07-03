# RIP-IT VetCon SAO — Firmware Specification

**Target:** DEF CON 34 / VetCon 2026
**Hardware:** RIP-IT Energy Drink themed SAO (NilbinSec / 2PAC + KYBR)
**Spec version:** 1.0 — final
**Status:** implemented; see [Appendix A](#appendix-a--deviations-from-spec) for the small set of deltas between this document and the shipped firmware.

---

## 1. Hardware Platform
| Item | Value |
|---|---|
| MCU | ATtiny1614-SSN |
| Package | SOIC-14 |
| Programming | UPDI (SerialUPDI / pyupdi / avrdude) |
| Clock | 10 MHz internal (recommended) |
| Supply (VDD) | 3.0 V nominal, 2.7–3.3 V operating range |
| Brown-Out Detector fuse | 2.6 V |
| Toolchain | AVR-GCC + avr-libc, target `attiny1614` |

---

## 2. Pin Assignments
| Pin | Port | Net | Function | Direction | Active |
|---|---|---|---|---|---|
| 1 | VCC | VCC | 3.0 V supply | — | — |
| 2 | PA4 | D5 | Top row LED #1 (leftmost, red) | OUT | HIGH |
| 3 | PA5 | D1 | Top row LED #2 (red) | OUT | HIGH |
| 4 | PA6 | D2 | Top row LED #3 (red) | OUT | HIGH |
| 5 | PA7 | D3 | Top row LED #4 (rightmost, red) | OUT | HIGH |
| 6 | PB3 | GPIO1 | SAO J1 pin 5 — Morse transmit | OUT | HIGH |
| 7 | PB2 | GPIO2 | SAO J1 pin 6 — Morse mirror | OUT | HIGH |
| 8 | PB1 | SW1 | Pattern cycle button | IN, internal pull-up | LOW |
| 9 | PB0 | D4 | Center hero LED (P in RIP, amber) | OUT | HIGH |
| 10 | PA0 | UPDI | Programming only — leave default | — | — |
| 11 | PA1 | NC | Unused | — | — |
| 12 | PA2 | SDA | I²C slave data (bit-banged — see Appendix A.6) | bidir | — |
| 13 | PA3 | SCL | I²C slave clock (bit-banged) | in | — |
| 14 | GND | GND | Ground | — | — |

**LED physical layout on the can:**
```
[D5] [D1] [D2] [D3]   ← top row, left to right
       [D4]            ← center, P of RIP, amber hero
```

---

## 3. System State Machine
```
BOOT
  └─ init clock, BOD, GPIO, bit-bang I²C, TCA0 (32 kHz PWM ISR)
  └─ set current_pattern = 1
  └─ enter MAIN LOOP
MAIN LOOP
  ├─ poll SW1 (debounced) → on press, current_pattern = (current_pattern % 4) + 1
  ├─ run current_pattern step (non-blocking, time-sliced)
  ├─ run Morse transmitter step (non-blocking, time-sliced)
  ├─ service PORTA pin-change interrupt (background — I²C slave)
  └─ apply brightness from I²C register 0x20 to PWM duty
```
No persistence. Power cycle always returns to Pattern 1.

---

## 4. Patterns

### Pattern 1 — Knight Rider Chase
- Sequence: D5 → D1 → D2 → D3, with a D4 bounce pulse at the right endpoint
- Then reverse: D3 → D2 → D1 → D5, with a D4 bounce pulse at the left endpoint
- Step rate: **120 ms per LED**
- D4 bounce pulse length: 120 ms (one step)
- Only one LED of the top row lit at a time; D4 fires once per direction change as the bounce
- Loop indefinitely until SW1 press

### Pattern 2 — Heartbeat
- All five LEDs pulse in unison via software PWM envelope
- Pattern (cardiac rhythm):
  - Pulse 1: fade up 80 ms → hold 40 ms → fade down 80 ms
  - Gap: 120 ms
  - Pulse 2: fade up 80 ms → hold 40 ms → fade down 80 ms
  - Long rest: 600 ms
- Total period: 1120 ms
- Envelope is scaled by the master brightness register (0x20). Writing 0x40 to 0x20 during Pattern 2 caps the heartbeat peak at ~25%.

### Pattern 3 — Incoming Transmission
- D4 holds steady for the entire propagation + hold
- Top row propagation: D5 ON → +120 ms → D1 ON → +120 ms → D2 ON → +120 ms → D3 ON
- All four hold lit (with D4) for 500 ms
- All five LEDs extinguish simultaneously
- Rest: 1000 ms
- Repeat (total period 1860 ms)

### Pattern 4 — Binary Nibble CTF Puzzle
- Encodes the string `"tinyurl/vetconpuzz"` (18 characters) as sequential 4-bit nibbles
- Each ASCII character produces two nibbles: high nibble first, then low nibble
- Total nibbles per data pass: 18 × 2 = **36 nibbles**

**Nibble-to-LED mapping (MSB to LSB):**

| Bit | LED |
|---|---|
| Bit 3 (MSB) | D5 |
| Bit 2 | D1 |
| Bit 1 | D2 |
| Bit 0 (LSB) | D3 |

**Display timing per byte (two nibbles):**
- **High nibble** displayed on the top row for 4 s, with **D4 ON** the whole time (D4 = "this is the high nibble" indicator)
- **Low nibble** displayed on the top row for 4 s, with **D4 OFF**
- No inter-nibble gap; the transition is instant

Total per byte: 8 s. Total data pass: 18 × 8 s = 144 s. After the full string transmits: 3 s all-off pause, then loop from the start.

**Example encoding for 't' = 0x74:**
- High nibble = 0x7 = 0b0111 → D5 off, D1 on, D2 on, D3 on; D4 ON; hold 4 s
- Low nibble  = 0x4 = 0b0100 → D5 off, D1 on, D2 off, D3 off; D4 OFF; hold 4 s

---

## 5. Button Handling (SW1)
- Connected PB1 → GND, internal pull-up enabled in firmware (`PORTB.PIN1CTRL = PORT_PULLUPEN_bm`)
- Active LOW
- Debounce: 25 ms software debounce, edge-triggered on falling edge
- Action on confirmed press: increment `current_pattern`, wrap 4 → 1
- No long-press or multi-press behavior
- Button press during Pattern 4 mid-transmission resets to start of next pattern cleanly

---

## 6. Morse Code Transmitter (GPIO1)
Runs continuously on PB3 (SAO J1 pin 5), independent of LED patterns.

**Message:** `"GWOT.ENERGY"`

**Timing (10 WPM standard):**

| Element | Duration |
|---|---|
| Dot | 100 ms |
| Dash | 300 ms |
| Intra-character gap | 100 ms |
| Inter-character gap | 300 ms |
| Inter-word gap | 700 ms |
| Post-transmission silence | 5000 ms |

**Morse encoding:**

| Char | Code |
|---|---|
| G | `--.` |
| W | `.--` |
| O | `---` |
| T | `-` |
| . | `.-.-.-` |
| E | `.` |
| N | `-.` |
| R | `.-.` |
| Y | `-.--` |

**Output:** PB3 driven HIGH = key down (mark), LOW = key up (space).
**PB2 behavior:** mirrors PB3 — drives the identical signal for easier probing and to look intentional under analysis.

Implementation: state machine driven by the 1 ms tick, no blocking delays. Morse step interleaves with LED pattern step in the main loop.

---

## 7. I²C Slave Interface

| Property | Value |
|---|---|
| Mode | I²C slave (bit-banged on PA2/PA3 — see Appendix A.6) |
| Address | **0x69** (7-bit) |
| Speed | 100 kHz standard mode; 50 kHz recommended for margin |
| Pins | PA2 SDA, PA3 SCL |
| Pull-ups | External, provided by host badge — do not enable internal |

### Register Map
| Reg | R/W | Bytes | Value / Behavior |
|---|---|---|---|
| 0x00 | R | 1 | `0x01` — Protocol version |
| 0x01 | R | 1 | `0x56` — VID high ('V') |
| 0x02 | R | 1 | `0x43` — VID low ('C') |
| 0x03 | R | 1 | `0x52` — PID high ('R') |
| 0x04 | R | 1 | `0x49` — PID low ('I') |
| 0x05 | R | 1 | `0xBB` — Supported interfaces bitmask |
| 0x06 | R | 1 | `0x00` — Serial high |
| 0x07 | R | 1 | `0x00` — Serial low |
| 0x10 | W | 2 | Pattern select: byte 1 must be `0x57` ('W'), byte 2 = pattern 0x01–0x04. Invalid byte 1 ignores the write. |
| 0x11 | R | 1 | Current pattern number (0x01–0x04) |
| 0x20 | W | 1 | LED master brightness 0x00–0xFF — applied to PWM duty on next ISR tick |
| 0x21 | W | 1 | LED bitmask override — bits 4..0 force D5/D1/D2/D3/D4 state. Write 0x00 to clear override and resume pattern. |
| 0x80–0x99 | R | 26 | CTF hint string: `"VETCON DC34 // 2PAC & KYBR"` |
| 0xFF | R | 11 | Easter egg ASCII string: `"Hit 'Em Up"` (10 chars + null) |

**Notes:**
- All other registers return `0xFF` on read
- Writes to read-only registers are silently ignored
- Multi-byte reads auto-increment the register pointer (standard SMBus block-read behavior); reads of 0xFF stay on 0xFF and stream successive bytes of the easter-egg string
- Register 0x20 brightness has no persistence; it resets to 0xFF on power-on

---

## 8. Brightness Implementation
- TCA0 generates a ~32 kHz overflow interrupt
- ISR maintains an 8-bit counter 0–255, incrementing each tick → 125 Hz visible PWM refresh
- For each LED pin: if `counter < effective_brightness` AND (pattern says lit OR override says lit) → pin HIGH, else LOW
- `effective_brightness = (master_brightness * pattern_envelope) >> 8`
- Pattern envelope is 0xFF for patterns 1/3/4; Pattern 2 modulates it for the heartbeat fade
- Master brightness (reg 0x20) defaults to 0xFF at boot

---

## 9. Initialization Sequence (pseudocode)
```c
void init(void) {
    // Clock: 10 MHz (OSC20M / 2)
    _PROTECTED_WRITE(CLKCTRL.MCLKCTRLA, CLKCTRL_CLKSEL_OSC20M_gc);
    _PROTECTED_WRITE(CLKCTRL.MCLKCTRLB, CLKCTRL_PDIV_2X_gc | CLKCTRL_PEN_bm);

    // GPIO directions
    PORTA.DIRSET = PIN4_bm | PIN5_bm | PIN6_bm | PIN7_bm;    // D5, D1, D2, D3
    PORTB.DIRSET = PIN0_bm | PIN2_bm | PIN3_bm;              // D4, GPIO2, GPIO1
    PORTB.PIN1CTRL = PORT_PULLUPEN_bm;                       // SW1 pull-up

    // TCA0 for 32 kHz PWM/tick ISR
    TCA0.SINGLE.PER     = (F_CPU / 32000) - 1;
    TCA0.SINGLE.INTCTRL = TCA_SINGLE_OVF_bm;
    TCA0.SINGLE.CTRLA   = TCA_SINGLE_CLKSEL_DIV1_gc | TCA_SINGLE_ENABLE_bm;

    // Bit-bang I²C on PA2/PA3
    PORTA.DIRCLR   = PIN2_bm | PIN3_bm;
    PORTA.OUTCLR   = PIN2_bm | PIN3_bm;
    PORTA.PIN2CTRL = PORT_ISC_BOTHEDGES_gc;
    PORTA.PIN3CTRL = PORT_ISC_BOTHEDGES_gc;

    sei();
}
```

---

## 10. Fuse Settings
| Fuse | Value | Notes |
|---|---|---|
| BODCFG | `0x54` | BOD sampled mode, level = 2.6 V (`BODLEVEL2`) |
| OSCCFG | `0x02` | 20 MHz osc selected (divided to 10 MHz at runtime) |
| SYSCFG0 | `0xF6` | UPDI enabled, EESAVE on |
| WDTCFG | `0x00` | Watchdog off |

---

## 11. File Structure
```
ripit-sao/
├── Makefile
├── README.md
├── spec.md          (this file)
└── src/
    ├── main.c       // init + main loop
    ├── patterns.c/.h  // 4 LED patterns
    ├── morse.c/.h     // Morse transmitter
    ├── i2c_slave.c/.h // bit-bang I²C slave + register map
    ├── button.c/.h    // SW1 debounce
    ├── pwm.c/.h       // software PWM ISR + ms tick
    └── config.h       // pin defs, constants
```

---

## 12. Build & Flash
See [README.md](README.md) for the full dependency list, per-OS install snippets, and wiring.

Short version (avrdude 7.x with SerialUPDI):
```bash
make                                                    # produces ripit.hex
make flash PROG_DEV=COM4                                # or /dev/ttyUSB0
make fuses PROG_DEV=COM4                                # once per chip
# Or in a single UPDI session:
avrdude -c serialupdi -P COM4 -b 230400 -p t1614 \
        -U fuse1:w:0x54:m -U flash:w:ripit.hex:i
```

---

## 13. Acceptance Criteria
A working build must demonstrate:
1. Power-on boots to Pattern 1 within 100 ms
2. SW1 single press advances pattern 1 → 2 → 3 → 4 → 1 reliably
3. Pattern 4 displays the full `"tinyurl/vetconpuzz"` string as nibbles (D4 = high-nibble indicator), 4 s per nibble, looping cleanly
4. Morse transmitter runs continuously on PB3, independent of LED state
5. I²C scan from a master finds device at 0x69
6. Read of register 0x00 returns 0x01
7. Read of registers 0x01–0x04 returns `"VCRI"`
8. Read of registers 0x80–0x99 returns the full CTF hint string
9. Read of register 0xFF returns `"Hit 'Em Up"`
10. Write of `[0x57, 0x03]` to register 0x10 immediately switches to Pattern 3
11. Write of 0x40 to register 0x20 immediately dims all LEDs to ~25%
12. Current draw at full brightness with all LEDs lit: < 30 mA at 3.0 V
13. Clean operation from 2.7 V to 3.3 V supply

---

## Appendix A — Deviations from spec

The as-shipped firmware differs from the original v1.0 spec in the following ways. Everything else matches literally.

### A.1 PWM ISR rate (§8, §9)
The original pseudocode wrote **"1 kHz PWM ISR"** with an 8-bit counter. That yields a 256 ms PWM period (~4 Hz visible refresh) which flickers badly. Implementation runs TCA0 at **32 kHz** overflow instead, giving a **125 Hz** visible PWM refresh; the 1 ms tick is derived by counting 32 ISRs (exact). Observable brightness semantics are unchanged.

### A.2 Pattern 4 D4 semantics (§4)
The original spec described D4 as a per-nibble clock separator with a 200 ms pulse, 200 ms gap, 4 s hold, 100 ms tail (per-nibble total = 4.5 s). Clarified during design review to: **D4 = high-nibble indicator** (ON for the high nibble of each byte, OFF for the low nibble), each nibble is a flat **4 s** display with no inter-nibble gap. Total per byte = 8 s. Total data pass = 144 s + 3 s all-off pause = 147 s per cycle.

### A.3 Pattern 1 D4 bounce duration (§4)
Original spec left this unspecified. Fixed at **120 ms** (one chase step) at each endpoint.

### A.4 Pattern 2 brightness composition (§4, §8)
Original spec did not state how master brightness (0x20) interacts with the heartbeat envelope. Fixed as: `effective = (master * envelope) >> 8`. Writing 0x40 to 0x20 during Pattern 2 caps the heartbeat peak at ~25%.

### A.5 Register 0x21 override — no dedicated "all-off" value
Writing `0x00` to reg 0x21 means "clear override, resume pattern" (as spec'd). There is deliberately **no** magic value for "override all LEDs off" — use `master brightness = 0x00` (reg 0x20) if you want everything dark.

### A.6 Bit-banged I²C slave, not hardware TWI (§7)
The ATtiny1614 hardware TWI peripheral cannot route to PA2/PA3. Its only two routings are:
- default: `SCL=PB0, SDA=PB1` (collides with D4 and SW1 in this design)
- alt: `SDA=PA1, SCL=PA2`

Neither matches the schematic (`SDA=PA2, SCL=PA3`), so the I²C slave is bit-banged in software via the PORTA pin-change interrupt (open-drain emulated with `DIRSET`/`DIRCLR` while `OUT` is held at 0). Consequences:
- **Practical top speed ≈ 100 kHz.** Software ISR entry latency is ~2.5 µs, worst-case SCL-falling → SDA-drive path is ~5 µs. At 100 kHz standard mode (4.7 µs SCL low), timing is tight but works with well-behaved masters; **50 kHz is the recommended host bus speed** for margin.
- **No clock stretching.** The slave does not drive SCL.
- The spec's "tolerate up to 400 kHz" line is **not** met — 400 kHz SCL low is 1.3 µs, less than ISR latency. If the host must run at 400 kHz, a hardware rev with I²C on PA1/PA2 (or PB0/PB1 with rerouted D4 and SW1) is required.

### A.7 Flasher tooling (§12)
Original spec called out `pyupdi`. Makefile defaults to `avrdude` ≥ 7.0 with the `serialupdi` programmer since that's the more common modern install; `pyupdi` remains supported via `make flash FLASHER=pyupdi`.

### A.8 Pattern 4 total-cycle wall-clock (§4)
Spec claimed "Total loop duration: 36 × 4 sec = 2 min 24 sec." That number counts only the 144 s data portion of the loop. The full cycle including the 3 s inter-pass blackout is **147 s** (2:27). The 144 s data figure is unchanged.
