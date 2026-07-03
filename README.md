# Vetcon-DC34-SAO
Repo for the Vetcon SAO for DEF CON 34

PCB Files contains gerbers, BOM, and graphics

# Flashing Instructions
# RIP-IT VetCon SAO

Firmware for the RIP-IT Energy Drink themed SAO (NilbinSec / 2PAC + KYBR), DEF CON 34 / VetCon 2026.

- Target: **ATtiny1614-SSN** @ 10 MHz internal, 3.0 V nominal (2.7–3.3 V range)
- Toolchain: AVR-GCC + avr-libc, targeting `attiny1614`
- Programming: SerialUPDI (one-wire UPDI over USB-serial)

---

## Dependencies

### Build toolchain
| Tool | Minimum version | Purpose |
|---|---|---|
| `avr-gcc` | 5.4+ (any recent build) | C compiler with tinyAVR-1 device support (`iotn1614.h`) |
| `avr-libc` | any | Standard headers (`avr/io.h`, `avr/interrupt.h`, `util/atomic.h`) |
| `binutils-avr` | any | `avr-objcopy`, `avr-size`, `avr-objdump` |
| GNU `make` | 3.8+ | Drives the build |

Any modern AVR-GCC distribution bundles all four. Verified working with **avr-gcc 15.2.0**.

### Flash toolchain (pick one)
| Tool | Minimum version | Notes |
|---|---|---|
| `avrdude` | **7.0** | Adds the `serialupdi` programmer; recommended |
| `pyupdi` / `pymcuprog` | any | Python alternative; older, simpler |

### UPDI adapter (hardware)
Any of the following works — pick one:
- **Adafruit UPDI Friend** (CH340-based, plug-and-play) ← easiest
- **DIY SerialUPDI**: any USB-serial adapter (CH340, FT232, CP2102) + 1N4148 diode + 4.7 kΩ resistor. Wiring: [SerialUPDI docs](https://github.com/SpenceKonde/AVR-Guidance/blob/master/UPDI/jtag2updi.md#making-your-own-updi-programmer)
- **Microchip PICkit 4/5 or SNAP** (needs Microchip Studio or MPLAB IPE)
- **JTAG2UPDI** (Arduino Uno/Nano running the JTAG2UPDI sketch)

### USB-serial driver
Required for CH340/CP2102-based adapters on Windows:
- CH340: [wch-ic.com driver](https://www.wch-ic.com/downloads/CH341SER_ZIP.html)
- CP2102: [SiLabs driver](https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers)
- FT232: usually built into Windows Update; FTDI has [official drivers](https://ftdichip.com/drivers/vcp-drivers/)

Linux and macOS ship these drivers in the kernel — nothing to install.

---

## Install snippets

### Windows
```powershell
# AVR-GCC — grab a prebuilt release from https://blog.zakkemble.net/avr-gcc-builds/
# extract to C:\avr-gcc, then add C:\avr-gcc\bin to PATH.

# avrdude 7.x — https://github.com/avrdudes/avrdude/releases  (avrdude-vX.Y-windows-x64.zip)

# make — via Chocolatey, MSYS2, or ezwinports.
choco install make        # if using Chocolatey
```

### Linux (Debian / Ubuntu)
```bash
sudo apt install gcc-avr avr-libc binutils-avr avrdude make
# Verify avrdude version — Ubuntu 22.04 ships 6.x; you need 7.0+ for serialupdi.
# If old, grab a build from https://github.com/avrdudes/avrdude/releases
avrdude -c ? 2>&1 | grep -i serialupdi   # should print "serialupdi = SerialUPDI programmer"
```

### Linux (Arch)
```bash
sudo pacman -S avr-gcc avr-libc avrdude make
```

### Linux (Fedora)
```bash
sudo dnf install avr-gcc avr-libc avr-binutils avrdude make
```

### macOS (Homebrew)
```bash
brew tap osx-cross/avr
brew install avr-gcc avrdude make
```

---

## Verify the install
```bash
avr-gcc --version          # any modern version prints e.g. "avr-gcc (GCC) 15.2.0"
avr-gcc -mmcu=attiny1614 -E -x c /dev/null > /dev/null    # confirms tiny1614 headers exist
avrdude -c serialupdi ?    # should list serialupdi (needs avrdude >= 7.0)
make --version
```

---

## Build & flash
```bash
# Build
make                                              # produces ripit.hex

# Flash (Linux/macOS — UPDI adapter appears as /dev/ttyUSB0 or /dev/tty.usbserial-*)
make flash PROG_DEV=/dev/ttyUSB0
make fuses PROG_DEV=/dev/ttyUSB0

# Flash (Windows — the UPDI adapter shows up in Device Manager under Ports)
make flash PROG_DEV=COM4
make fuses PROG_DEV=COM4

# Both in one avrdude session (skip if fuses are already set)
avrdude -c serialupdi -P COM4 -b 230400 -p t1614 \
        -U fuse1:w:0x54:m -U flash:w:ripit.hex:i
```

Override the flasher to `pyupdi` with `make flash FLASHER=pyupdi PROG_DEV=...`.

If UPDI init fails at 230400 baud, drop to `PROG_BAUD=57600` — some clone adapters can't keep up.

---

## Wiring the UPDI adapter to the SAO
Three connections. **Set the UPDI adapter's power jumper to 3.3 V** — do not use 5 V.

| UPDI adapter | SAO ATtiny1614 pin |
|---|---|
| UPDI / DAT | Pin 10 · PA0 |
| GND | Pin 14 · GND |
| VCC (3.3 V) | Pin 1 · VCC (only if powering from the adapter) |

Do not connect the SAO to a host badge while UPDI is attached — power should come from exactly one source.

---

See `spec.md` (or the firmware specification handed to you) for pin map,
register map, pattern definitions, and acceptance criteria.


Acknowledgements: Thanks to 2PAC, Kybr, and F**K OFF! for making this SAO a reality for DC34!
