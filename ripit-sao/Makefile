# RIP-IT VetCon SAO firmware
# Target: ATtiny1614 @ 10 MHz internal

MCU       = attiny1614
F_CPU     = 10000000UL
TARGET    = ripit
SRCDIR    = src
BUILDDIR  = build

CC        = avr-gcc
OBJCOPY   = avr-objcopy
SIZE      = avr-size

CFLAGS    = -mmcu=$(MCU) -DF_CPU=$(F_CPU) -Os -std=gnu11 \
            -Wall -Wextra -Wshadow -Wstrict-prototypes \
            -ffunction-sections -fdata-sections \
            -I$(SRCDIR)
LDFLAGS   = -mmcu=$(MCU) -Wl,--gc-sections

SRCS     := $(wildcard $(SRCDIR)/*.c)
OBJS     := $(SRCS:$(SRCDIR)/%.c=$(BUILDDIR)/%.o)
DEPS     := $(OBJS:.o=.d)

# SerialUPDI defaults — override on the command line if needed.
# On Windows, set PROG_DEV=COM4 (or whichever COM port your adapter shows).
PROG_DEV  ?= /dev/ttyUSB0
PROG_BAUD ?= 230400

# Flasher: avrdude (>=7.0 with serialupdi) is preferred and matches the
# user's local install. The original spec mentioned pyupdi as an
# alternative; both work, change FLASHER as desired.
FLASHER   ?= avrdude

.PHONY: all clean flash fuses size

all: $(TARGET).hex

$(BUILDDIR):
	@mkdir -p $(BUILDDIR)

$(BUILDDIR)/%.o: $(SRCDIR)/%.c | $(BUILDDIR)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(TARGET).elf: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

$(TARGET).hex: $(TARGET).elf
	$(OBJCOPY) -O ihex -R .eeprom $< $@
	$(SIZE) --format=berkeley $<

size: $(TARGET).elf
	$(SIZE) --format=berkeley $<

flash: $(TARGET).hex
ifeq ($(FLASHER),avrdude)
	avrdude -c serialupdi -P $(PROG_DEV) -b $(PROG_BAUD) -p t1614 \
	        -U flash:w:$<:i
else
	pyupdi -d tiny1614 -c $(PROG_DEV) -b $(PROG_BAUD) -f $<
endif

# BOD level 2 = 2.6 V, BOD sampled mode (per spec Section 10).
fuses:
ifeq ($(FLASHER),avrdude)
	avrdude -c serialupdi -P $(PROG_DEV) -b $(PROG_BAUD) -p t1614 \
	        -U fuse1:w:0x54:m
else
	pyupdi -d tiny1614 -c $(PROG_DEV) -fs 1:0x54
endif

clean:
	rm -rf $(BUILDDIR) $(TARGET).elf $(TARGET).hex

-include $(DEPS)
