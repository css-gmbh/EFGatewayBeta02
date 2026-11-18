# Minimal MSP430FR5962 hello-world build (msp430-elf-gcc)
#
# Prerequisites:
#   - TI MSP430-GCC-OPENSOURCE (msp430-elf-gcc on PATH), or set GCC_DIR
#   - Device support headers/linker scripts (set SUPPORT_FILE_DIRECTORY)
#
# Examples:
#   make
#   make flash          # via mspdebug (tilib / eZ-FET)
#   make clean

DEVICE ?= msp430fr5962
TARGET  = $(DEVICE)

# Optional: point at a local TI GCC install, e.g. /opt/msp430-gcc/bin
GCC_DIR ?=
ifneq ($(GCC_DIR),)
  CC  := $(GCC_DIR)/msp430-elf-gcc
  OBJCOPY := $(GCC_DIR)/msp430-elf-objcopy
  SIZE := $(GCC_DIR)/msp430-elf-size
else
  CC  := msp430-elf-gcc
  OBJCOPY := msp430-elf-objcopy
  SIZE := msp430-elf-size
endif

# Device headers + linker scripts from MSP430 GCC support files package.
# Leave empty if your toolchain already finds them via -mmcu=.
SUPPORT_FILE_DIRECTORY ?=

CFLAGS  = -mmcu=$(DEVICE) -Os -g -Wall -Wextra
LDFLAGS = -mmcu=$(DEVICE)

ifneq ($(SUPPORT_FILE_DIRECTORY),)
  CFLAGS  += -I$(SUPPORT_FILE_DIRECTORY)
  LDFLAGS += -L$(SUPPORT_FILE_DIRECTORY) -T $(DEVICE).ld
endif

SRCS = main.c
OBJS = $(SRCS:.c=.o)

.PHONY: all clean flash size

all: $(TARGET).elf $(TARGET).hex size

$(TARGET).elf: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(TARGET).hex: $(TARGET).elf
	$(OBJCOPY) -O ihex $< $@

size: $(TARGET).elf
	$(SIZE) $<

# Flash with mspdebug over TI FET / LaunchPad eZ-FET (tilib backend).
# Requires libmsp430 (MSP Debug Stack) and a connected programmer.
flash: $(TARGET).elf
	mspdebug tilib "prog $(TARGET).elf"

clean:
	rm -f $(OBJS) $(TARGET).elf $(TARGET).hex
