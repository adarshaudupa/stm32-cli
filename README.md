# STM32 UART CLI

**Bare-metal UART command-line interface for STM32F446RE with DMA + Idle Line Detection**

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

---

## Overview

A hardware bring-up CLI for the STM32F446RE Nucleo board, built entirely from register-level programming with zero HAL abstractions. Receives commands over UART using DMA + Idle Line Detection — no per-byte interrupts, no polling. The CPU is notified only when a complete packet arrives.

**Core Philosophy:** No HAL, no Arduino libraries — direct register access, reference manual, and first-principles understanding of ARM Cortex-M4 peripherals.

---

## Features

### UART Communication
- **DMA RX with Idle Line Detection**: DMA1 Stream5 absorbs incoming bytes autonomously; IDLE interrupt fires at packet boundary — zero per-byte CPU overhead
- **Blocking TX**: Spin-on-TXE transmit, suitable for debug CLI output
- **Dynamic baud rate reconfiguration** at runtime via `SET BAUD <rate>` command
- **Echo and backspace handling** for real terminal behaviour
- **Command buffering** with overflow protection

### Command Parser
- Stateful ESC sequence interceptor for arrow key handling
- Command history (last 5 commands) with up/down arrow navigation
- Case-sensitive command matching with built-in error handling
- Extensible command structure

### LED Control
- Direct ODR/BSRR register manipulation
- Manual ON/OFF, STATUS query, and TIM2 interrupt-driven 1Hz auto-blink
- Blink frequency configurable at runtime

### Driver Architecture
- Modular bare-metal drivers: `uart2.c`, `tim2.c`, `gpio.c`
- ISR sets a flag; all processing done in main loop — no business logic in interrupt context
- Zero dynamic memory allocation

---

## Hardware Requirements

| Component | Specification |
|-----------|--------------|
| **Board** | STM32 Nucleo-F446RE |
| **MCU** | STM32F446RET6 (ARM Cortex-M4, 16 MHz HSI) |
| **LED** | PA5 (LD2, onboard green LED) |
| **UART** | USART2 via ST-LINK Virtual COM Port |
| **Connections** | USB cable only (no external hardware) |

### Pinout

| Peripheral | Pin | Function |
|------------|-----|----------|
| USART2_TX | PA2 | Serial output to PC |
| USART2_RX | PA3 | Serial input from PC |
| LED | PA5 | Onboard status LED (LD2) |

---

## Quick Start

### Prerequisites

- **IDE**: STM32CubeIDE or any ARM GCC toolchain
- **Debugger**: ST-LINK (integrated on Nucleo board)
- **Terminal**: PuTTY, Tera Term, minicom, or `screen`

### Build and Flash

#### STM32CubeIDE
```
1. File → Import → Existing Projects into Workspace
2. Select repository directory
3. Project → Build All (Ctrl+B)
4. Run → Debug (F11) or Run (Ctrl+F11)
```

#### Command Line
```bash
git clone https://github.com/adarshaudupa/stm32-cli.git
cd stm32-cli
make clean && make
st-flash write build/stm32-cli.bin 0x8000000
```

### Connect Terminal

**Linux/macOS:**
```bash
screen /dev/ttyACM0 9600
# or
minicom -D /dev/ttyACM0 -b 9600
```

**Windows:**
```
Open PuTTY or Tera Term
COM Port: Check Device Manager for "STMicroelectronics Virtual COM Port"
Baud: 9600
Data: 8 bits, Parity: None, Stop: 1 bit, Flow control: None
```

---

## Usage

### Sample Session

```
---STM32 CLI---
Type HELP for commands

> HELP
Available commands:
  LED ON         - Turn LED on
  LED OFF        - Turn LED off
  BLINK          - Blink LED at 1Hz
  STATUS         - Check LED state
  SET BAUD <rate> - Set UART baud rate
  HELP           - Show this help

> LED ON
LED turned ON

> STATUS
LED is ON

> BLINK
LED auto-blinking at 1 Hz

> SET BAUD 115200
Changing baud rate to 115200...
Please update your serial terminal to match!
```

### Supported Commands

| Command | Description |
|---------|-------------|
| `LED ON` | Force LED on, stops blink if active |
| `LED OFF` | Force LED off, stops blink if active |
| `BLINK` | Enable TIM2-driven 1Hz auto-blink |
| `STATUS` | Report current LED state from ODR |
| `SET BAUD <rate>` | Reconfigure USART2 baud rate at runtime |
| `HELP` | Show command list |

**Arrow key navigation:**
- `↑` — scroll back through command history (last 5 commands)
- `↓` — scroll forward through command history

---

## Technical Deep Dive

### RX Architecture: DMA + Idle Line Detection

The key design decision: instead of an RXNE interrupt firing on every byte, DMA handles all byte transfers autonomously while the CPU waits for an IDLE event signalling end-of-packet.

```
Sender types 'L', 'E', 'D', ' ', 'O', 'N', '\r'
         ↓
DMA1 Stream5 writes each byte to rx_buffer[] as they arrive (no CPU involvement)
         ↓
Line goes idle → IDLE flag fires → USART2_IRQHandler
         ↓
ISR: compute packet_length = RX_BUFFER_SIZE - DMA1_Stream5->NDTR
     reset DMA, set packet_ready = 1
         ↓
Main loop: iterate rx_buffer[0..packet_length], run state machine
```

#### DMA Configuration (Register-Level)

```c
RCC->AHB1ENR |= (1 << 21);           // DMA1 clock

DMA1_Stream5->CR = 0;
while (DMA1_Stream5->CR & (1 << 0)); // Wait until disabled

DMA1_Stream5->CR  |= (4 << 25);      // Channel 4: USART2_RX
DMA1_Stream5->CR  &= ~((3 << 11) | (3 << 13)); // PSIZE=byte, MSIZE=byte
DMA1_Stream5->CR  |= (1 << 10);      // MINC: memory increment
DMA1_Stream5->PAR  = (uint32_t)&USART2->DR;
DMA1_Stream5->M0AR = (uint32_t)rx_buffer;
DMA1_Stream5->NDTR = RX_BUFFER_SIZE;

DMA1_Stream5->CR  |= (1 << 0);       // EN: enable stream
```

#### ISR: Idle Detection + DMA Reset

```c
void USART2_IRQHandler(void)
{
    if (USART2->SR & (1 << 4))        // IDLE flag
    {
        volatile uint32_t tmp = USART2->SR;
        tmp = USART2->DR;              // Mandatory SR→DR read to clear IDLE
        (void)tmp;

        packet_length = RX_BUFFER_SIZE - DMA1_Stream5->NDTR;

        DMA1_Stream5->CR &= ~(1 << 0);
        while (DMA1_Stream5->CR & (1 << 0));
        DMA1->HIFCR |= (0x3F << 6);   // Clear Stream5 interrupt flags (critical)
        DMA1_Stream5->NDTR = RX_BUFFER_SIZE;
        DMA1_Stream5->M0AR = (uint32_t)rx_buffer;
        DMA1_Stream5->CR  |= (1 << 0);

        packet_ready = 1;
    }
}
```

> **Why `HIFCR` matters:** Without clearing Stream5's interrupt flags in `DMA1->HIFCR` before re-enabling, stale transfer-complete or error flags cause the re-armed stream to misbehave immediately. This was identified during debug as the root cause of the "receives one byte, then stalls" failure mode.

#### Main Loop Consumption

```c
if (packet_ready)
{
    packet_ready = 0;

    for (uint16_t i = 0; i < packet_length; i++)
    {
        char c = (char)rx_buffer[i];
        // ESC sequence interceptor + command state machine
    }

    packet_length = 0;
}
```

### Baud Rate Calculation

```c
// Formula: BRR = fPCLK / (16 × baud), fractional part in lower nibble
// APB1 clock = 16 MHz (HSI default)
// 9600 baud: BRR = 0x0683

void UART2_SetBaud(uint32_t baud)
{
    USART2->CR1 &= ~(1 << 13);        // Disable USART
    uint32_t pclk     = get_apb1_freq_hz();
    uint32_t mantissa = pclk / (16 * baud);
    uint32_t fraction = ((pclk % (16 * baud)) * 16 + (8 * baud)) / (16 * baud);
    if (fraction >= 16) { mantissa++; fraction = 0; }
    USART2->BRR = (mantissa << 4) | (fraction & 0xF);
    USART2->CR1 |= (1 << 13);         // Re-enable USART
}
```

### Command History

Circular buffer of the last 5 commands. Arrow key ESC sequences (`\x1B`, `[`, `A`/`B`) are intercepted by a dedicated state machine before the main command processor sees them.

```c
#define HISTORY_SIZE 5
char history[HISTORY_SIZE][CMD_BUFFER_SIZE];
uint8_t write_index  = 0;   // Next slot to write
uint8_t browse_index = 0;   // Current scroll position
uint8_t browse_depth = 0;   // How far back we've scrolled
```

---

## Project Structure

```
stm32-cli/
├── Core/
│   ├── Inc/
│   │   ├── uart2.h         # USART2 + DMA driver header
│   │   ├── tim2.h          # TIM2 driver + LED state enum
│   │   ├── gpio.h          # GPIO helpers
│   │   └── clock.h         # APB clock frequency utilities
│   └── Src/
│       ├── main.c          # CLI application + command state machine
│       ├── uart2.c         # USART2 + DMA1 driver (ISR included)
│       ├── tim2.c          # TIM2 init + IRQ handler (LED blink)
│       ├── gpio.c          # PA5 LED, PC13 button init
│       ├── clock.c         # RCC clock tree read utilities
│       └── syscalls.c      # Newlib stubs
├── Drivers/
│   └── CMSIS/              # Vendor CMSIS headers (unchanged)
├── .gitignore
├── LICENSE
└── README.md
```

---

## Known Issues and Limitations

| Issue | Impact | Status |
|-------|--------|--------|
| TX is blocking | Main loop spins during long prints | Acceptable for CLI; DMA TX is a future upgrade |
| Fixed buffer size (256B) | Commands beyond 256 bytes truncated | Non-issue for CLI use case |
| No hardware flow control | Potential loss at very high baud rates | Use `SET BAUD` conservatively |

---

## Roadmap

- [ ] **DMA TX** — non-blocking transmit for high-throughput logging
- [ ] **SPI driver** — register-level SPI peripheral driver
- [ ] **I2C sensor commands** — read IMU data via CLI
- [ ] **PWM commands** — timer-based LED brightness control
- [ ] **Watchdog integration** — safety reset on CLI hang

---

## References

- [STM32F446xx Reference Manual (RM0390)](https://www.st.com/resource/en/reference_manual/rm0390-stm32f446xx-advanced-armbased-32bit-mcus-stmicroelectronics.pdf)
- [STM32F446RE Datasheet](https://www.st.com/resource/en/datasheet/stm32f446re.pdf)
- [STM32 Nucleo-64 User Manual (UM1724)](https://www.st.com/resource/en/user_manual/um1724-stm32-nucleo64-boards-mb1136-stmicroelectronics.pdf)
- [ARM Cortex-M4 Technical Reference Manual](https://developer.arm.com/documentation/100166/0001)

---

## License

MIT License — see [LICENSE](LICENSE) for details.

---

## Author

**Adarsha Udupa Baikady**
Undergraduate | Electronics & Instrumentation Engineering
Focus: Embedded Systems & Bare-Metal Firmware

- GitHub: [@adarshaudupa](https://github.com/adarshaudupa)
- LinkedIn: [adarsha-udupa-baikady](https://www.linkedin.com/in/adarsha-udupa-baikady-327a54219)
- Email: adarsha8505@gmail.com

---

**Built with no HAL, no Arduino — just registers, reference manuals, and first principles.**
