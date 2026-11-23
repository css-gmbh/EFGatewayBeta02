# EFGatewayBeta02

Minimal **MSP430FR5962** hello-world example for the EF Gateway Beta 02 platform.

## What it does

The program in [`hello_world/main.c`](hello_world/main.c):

1. Stops the watchdog and sets **DCO / SMCLK / MCLK = 8 MHz**
2. Configures **eUSCI_A0 UART** — **TX P2.0**, **RX P2.1**, **19200 baud 8N1**
3. Enables **interrupt RX** into a linear **256-byte** buffer (from index 0); each byte restarts **Timer_A0 (~50 ms idle)**
4. On idle timeout, passes the buffer pointer to `rx_frame_placeholder()`, then resumes RX at index 0
5. Provides **interrupt TX** via `uart_send()` / `uart_send_str()` (`UCTXIE` drains a 256-byte TX buffer in the UCA0 ISR)
6. Initializes **RS-485** via [`rs485_uca1`](hello_world/rs485_uca1.h) — **TX P2.5**, **RX P2.6**, **DE P4.3**, **/RE P4.2**, **57600 baud 8N1** (inverted soft-UART)
7. Unlocks GPIO (`LOCKLPM5`)
8. In a loop: toggles **P1.0** and queues `Hello World\r\n` for interrupt TX on UCA0

P1.0 is the conventional TI LaunchPad demo LED pin (remap if your board differs). Pinmux matches the requested UCA0 / UCA1 mappings on this MCU.

## Layout

```
hello_world/
  main.c                         # blink + UCA0 UART RX framing + interrupt TX
  rs485_uca1.c/.h                # UCA1 half-duplex RS-485 (DE/RE, IRQ TX/RX)
  crc16_iso15693.c/.h            # ISO 15693-style CRC-16 helper (poly 0x0810)
  Makefile                       # open-toolchain build (msp430-elf-gcc)
ccs/msp430fr5962_hello_world/    # Code Composer Studio project
  .project / .cproject / .ccsproject
  lnk_msp430fr5962.cmd
  targetConfigs/MSP430FR5962.ccxml
```

The CCS project **links** the shared `hello_world/` sources (no duplicated application logic).

### CRC helper

[`crc16_iso15693()`](hello_world/crc16_iso15693.h) implements an ISO 15693-style CRC-16 (init `0xFFFF`, reflected/LSB-first, final XOR `0xFFFF`) with generator polynomial **`0x0810`**. The RX frame placeholders call it on each completed buffer.

### RS-485 (UCA1)

[`rs485_init()`](hello_world/rs485_uca1.h) / `rs485_send()` drive half-duplex RS-485 on UCA1:

- **TX P2.5 / RX P2.6**, **57600 8N1 with inverted UART polarity** (idle low / start high). FR5962 eUSCI has no TX/RX invert, so this path is a **Timer_A2 soft-UART** on those pins
- **DE P4.3** (active high), **/RE P4.2** (active low) — idle = receive; TX asserts DE and disables the receiver
- Interrupt RX into a 256-byte buffer; **Timer_A1 ~50 ms idle** → `rs485_rx_frame_placeholder()`

---

## Build with msp430-gcc (Makefile)

### Prerequisites

- [MSP430-GCC-OPENSOURCE](https://www.ti.com/tool/MSP430-GCC-OPENSOURCE) (`msp430-elf-gcc` on your `PATH`)
- Matching **MSP430 GCC support files** (device headers + linker scripts), if not already bundled with your install
- To flash: a TI FET / LaunchPad eZ-FET and [`mspdebug`](https://github.com/dlbeer/mspdebug) (`tilib`) or [UniFlash](https://www.ti.com/tool/UNIFLASH)

### Build

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

### Flash

```bash
cd hello_world
make flash
# or: mspdebug tilib "prog msp430fr5962.elf"
```

---

## Build with Code Composer Studio

### Prerequisites

- [Code Composer Studio](https://www.ti.com/tool/CCSTUDIO) with **MSP430** device support and the TI MSP430 compiler (project metadata targets **21.6.0.LTS**; CCS will offer to migrate if you have a nearby version)
- A TI FET / LaunchPad eZ-FET (or compatible) for download/debug

### Import / open

1. Start CCS and choose (or create) a workspace.
2. **File → Open Projects from File System…** (CCS Theia / recent CCS),  
   or **Project → Import CCS Projects…** (Eclipse-based CCS).
3. Select search directory:

   `…/EFGatewayBeta02/ccs/msp430fr5962_hello_world`

4. Import the discovered project **`msp430fr5962_hello_world`**.
5. Confirm **Project Explorer** shows a linked `main.c` (from `hello_world/main.c`) and device **MSP430FR5962**.

If the linked file appears broken after a copy/move, refresh the project or re-import from the repo path above so `PARENT-2-PROJECT_LOC` still resolves to the repository root.

### Build

- Select the project, then **Project → Build Project** (or the hammer toolbar button).
- Default configuration is **Debug**; switch to **Release** via the build-configuration dropdown if desired.
- Output: `ccs/msp430fr5962_hello_world/Debug/msp430fr5962_hello_world.out` (or `Release/…`).

### Flash / debug

1. Connect the FET / LaunchPad USB.
2. **Run → Debug** (bug toolbar button). CCS uses `targetConfigs/MSP430FR5962.ccxml` (TI MSP430 USB).
3. After load, resume to run the P1.0 blink and UART output.

Connect a USB–TTL adapter to **P2.0 (TX)** / **P2.1 (RX)** / GND at **19200 8N1**:
- You should see periodic `Hello World` lines from the MCU
- Bytes you send fill `rx_buf[]` from index 0; after ~50 ms silence, `rx_frame_placeholder(buf, len)` is called and RX restarts at 0

For RS-485, connect the transceiver to **P2.5 (TX)** / **P2.6 (RX)** / **P4.3 (DE)** / **P4.2 (/RE)** at **57600 8N1** (inverted idle-low). Use `rs485_send()` to transmit; received frames end after ~50 ms idle via `rs485_rx_frame_placeholder()`.

You can also load the `.out` with UniFlash onto **MSP430FR5962**.
