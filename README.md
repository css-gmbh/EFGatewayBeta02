# EFGatewayBeta02

Minimal **MSP430FR5962** hello-world example for the EF Gateway Beta 02 platform.

## What it does

The program in [`hello_world/`](hello_world/) is a conventional TI FRAM blink demo:

1. Stops the watchdog
2. Unlocks GPIO (`LOCKLPM5`)
3. Toggles **P1.0** in a software delay loop

P1.0 is the standard TI demo LED pin on many MSP430 LaunchPads (for example LED2 on MSP-EXP430FR5969). This repo does not yet document custom EF Gateway LED wiring, so the example stays on that conventional pin rather than inventing board-specific hardware. Remap `P1.0` in `main.c` if your board uses a different LED.

UART “Hello World” is intentionally omitted until board UART pinmux is documented.

## Prerequisites

- [MSP430-GCC-OPENSOURCE](https://www.ti.com/tool/MSP430-GCC-OPENSOURCE) (`msp430-elf-gcc` on your `PATH`)
- Matching **MSP430 GCC support files** (device headers + linker scripts), if not already bundled with your install
- To flash: a TI FET / LaunchPad eZ-FET and either:
  - [`mspdebug`](https://github.com/dlbeer/mspdebug) with the `tilib` backend (MSP Debug Stack), or
  - [UniFlash](https://www.ti.com/tool/UNIFLASH) / CCS

## Build

```bash
cd hello_world
make
```

If support files are not found automatically:

```bash
make SUPPORT_FILE_DIRECTORY=/path/to/msp430-gcc-support-files/include
```

If the toolchain is not on `PATH`:

```bash
make GCC_DIR=/path/to/msp430-gcc/bin SUPPORT_FILE_DIRECTORY=/path/to/msp430-gcc-support-files/include
```

Outputs: `msp430fr5962.elf` and `msp430fr5962.hex`.

## Flash

With `mspdebug` and a connected FET:

```bash
cd hello_world
make flash
```

Or manually:

```bash
mspdebug tilib "prog msp430fr5962.elf"
```

With UniFlash / CCS, load `msp430fr5962.elf` (or `.hex`) onto device **MSP430FR5962**.

## Layout

```
hello_world/
  main.c      # blink P1.0
  Makefile    # msp430-elf-gcc build + optional mspdebug flash
```

CCS users can create a new MSP430FR5962 project and add `hello_world/main.c` as the application source.
