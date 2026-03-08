# STM32 UART CLI

**Interrupt-driven command-line interface for STM32F446RE using bare-metal register programming**

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

---

##  Overview

A production-grade UART CLI implementation for the STM32F446RE Nucleo board, built entirely from register-level programming without HAL abstractions. This project demonstrates professional embedded firmware practices: interrupt-driven I/O, circular buffer management, robust command parsing, and clean driver architecture.

**Core Philosophy:** No HAL, no Arduino libraries—just direct register access, reference manual, and first-principles understanding of ARM Cortex-M4 peripherals.

---

##  Features

### UART Communication
- **Interrupt-driven RX**: RXNE interrupt with 256-byte circular buffer
- **Polling TX**: Blocking transmit optimized for debug output
- **Manual BRR calculation** from APB1 clock frequency (115200 baud)
- **Echo and backspace handling** for real terminal feel
- **Command buffering** with overflow protection

### Command Parser
- Space-delimited tokenization
- Case-sensitive command matching
- Built-in error handling and help system
- Extensible command table structure

### LED Control
- Direct register manipulation (no abstraction layers)
- Multiple control modes: manual ON/OFF, TOGGLE, STATUS query
- Integrated with timer-based state machine (optional BLINK mode)

### Professional Code Quality
- Clean separation of concerns (driver vs. application logic)
- CMSIS-compliant register access
- Volatile-correct ISR implementation
- Zero dynamic memory allocation

---

##  Hardware Requirements

| Component | Specification |
|-----------|--------------|
| **Board** | STM32 Nucleo-F446RE |
| **MCU** | STM32F446RET6 (ARM Cortex-M4, 180 MHz) |
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

##  Quick Start

### Prerequisites

- **IDE**: STM32CubeIDE or any ARM GCC toolchain
- **Debugger**: ST-LINK (integrated on Nucleo board)
- **Terminal**: PuTTY, Tera Term, minicom, or `screen`

### Build and Flash

#### Option 1: STM32CubeIDE
```bash
1. File → Import → Existing Projects into Workspace
2. Select repository directory
3. Project → Build All (Ctrl+B)
4. Run → Debug (F11) or Run (Ctrl+F11)
```

#### Option 2: Command Line
```bash
git clone https://github.com/adarshaudupa/stm32-uart-cli.git
cd stm32-uart-cli
make clean && make
st-flash write build/stm32-uart-cli.bin 0x8000000
```

### Connect Terminal

**Linux/macOS:**
```bash
screen /dev/ttyACM0 115200
# or
minicom -D /dev/ttyACM0 -b 115200
```

**Windows:**
```
Open PuTTY or Tera Term
COM Port: Check Device Manager for "STMicroelectronics Virtual COM Port"
Baud: 115200
Data: 8 bits
Parity: None
Stop: 1 bit
Flow control: None
```

---

##  Usage

### Sample Session

```
--- STM32 UART CLI ---
Type HELP for commands

> HELP
Available commands:
  LED ON    - Turn LED on
  LED OFF   - Turn LED off
  TOGGLE    - Toggle LED state
  STATUS    - Check LED state
  HELP      - Show this help

> LED ON
LED turned ON

> STATUS
LED is ON

> TOGGLE
LED toggled

> STATUS
LED is OFF

> BLINK
LED auto-blink enabled (1 Hz)
```

### Supported Commands

| Command | Description | Example |
|---------|-------------|---------|
| `LED ON` | Force LED on | `> LED ON` |
| `LED OFF` | Force LED off | `> LED OFF` |
| `TOGGLE` | Toggle LED state | `> TOGGLE` |
| `STATUS` | Display current LED state | `> STATUS` |
| `BLINK` | Enable timer-controlled auto-blink | `> BLINK` |
| `HELP` | Show command list | `> HELP` |

**Error Handling:**
```
> INVALID_CMD
Unknown command: INVALID_CMD
Type HELP for commands
```

---

##  Technical Deep Dive

### UART Configuration (Register-Level)

#### Clock Setup
```c
// Enable peripheral clocks
RCC->AHB1ENR |= (1 << 0);   // GPIOA clock
RCC->APB1ENR |= (1 << 17);  // USART2 clock (APB1 bus)
```

#### GPIO Alternate Function
```c
// PA2 (TX) and PA3 (RX) to AF mode
GPIOA->MODER &= ~((3 << 4) | (3 << 6));  // Clear mode bits
GPIOA->MODER |= (2 << 4) | (2 << 6);     // Set AF mode (0b10)

// Select AF7 (USART2) for PA2/PA3
GPIOA->AFR[0] &= ~((0xF << 8) | (0xF << 12));
GPIOA->AFR[0] |= (7 << 8) | (7 << 12);
```

#### Baud Rate Calculation
```c
// Formula: BRR = fPCLK / (16 × baud)
// APB1 clock = 16 MHz (HSI, default after reset)
// Target baud = 115200
// BRR = 16000000 / (16 × 115200) ≈ 8.68 → 0x8B (8 mantissa, 11 fractional)
USART2->BRR = 0x8B;
```

#### Enable USART and Interrupts
```c
USART2->CR1 |= (1 << 13);  // UE: USART enable
USART2->CR1 |= (1 << 3);   // TE: Transmitter enable
USART2->CR1 |= (1 << 2);   // RE: Receiver enable
USART2->CR1 |= (1 << 5);   // RXNEIE: RX interrupt enable

// Enable USART2 interrupt in NVIC
NVIC_EnableIRQ(USART2_IRQn);
```

### Circular Buffer (Interrupt-Driven RX)

#### Buffer Structure
```c
#define RX_BUFFER_SIZE 256
volatile char rx_buffer[RX_BUFFER_SIZE];
volatile uint8_t rx_head = 0;  // ISR writes here
volatile uint8_t rx_tail = 0;  // Main reads here
```

#### ISR (Producer)
```c
void USART2_IRQHandler(void) {
    if (USART2->SR & (1 << 5)) {  // RXNE flag set?
        char received_byte = USART2->DR;  // Reading DR clears RXNE
        
        rx_buffer[rx_head] = received_byte;
        rx_head = (rx_head + 1) % RX_BUFFER_SIZE;  // Circular wrap
        
        // Note: Overflow handling omitted for brevity
    }
}
```

#### Main Loop (Consumer)
```c
char UART2_ReadChar(void) {
    while (rx_head == rx_tail);  // Wait for data
    
    char c = rx_buffer[rx_tail];
    rx_tail = (rx_tail + 1) % RX_BUFFER_SIZE;
    return c;
}
```

### Command Parser Architecture

```c
// Command buffer with overflow protection
#define CMD_BUFFER_SIZE 32
char cmd_buffer[CMD_BUFFER_SIZE];
uint8_t cmd_index = 0;

// Main parsing loop
while (1) {
    char c = UART2_ReadChar();
    
    if (c == '\r' || c == '\n') {  // Enter pressed
        cmd_buffer[cmd_index] = '\0';
        execute_command(cmd_buffer);
        cmd_index = 0;
    }
    else if (c == 127 || c == 8) {  // Backspace/DEL
        if (cmd_index > 0) {
            cmd_index--;
            UART2_SendString("\b \b");  // Erase character on screen
        }
    }
    else if (cmd_index < CMD_BUFFER_SIZE - 1) {
        cmd_buffer[cmd_index++] = c;
        UART2_SendChar(c);  // Echo
    }
}
```

---

##  Project Structure

```
stm32-uart-cli/
├── Core/
│   ├── Inc/
│   │   ├── main.h
│   │   ├── uart.h          # USART2 driver header
│   │   └── gpio.h          # GPIO helper functions
│   └── Src/
│       ├── main.c          # CLI application logic
│       ├── uart.c          # USART2 driver implementation
│       ├── gpio.c          # GPIO initialization
│       └── syscalls.c      # Newlib stubs
├── Drivers/
│   └── CMSIS/              # Vendor CMSIS headers (unchanged)
├── Debug/                  # Build artifacts (gitignored)
├── .gitignore
├── LICENSE
└── README.md
```

---

##  Learning Outcomes

This project demonstrates:

1. **Peripheral Programming**
   - RCC clock tree management (AHB1, APB1 buses)
   - GPIO alternate function configuration
   - USART register-level setup (BRR, CR1, SR, DR)

2. **Interrupt Handling**
   - NVIC priority and enable sequence
   - ISR flag clearing and data handling
   - Avoiding race conditions with `volatile`

3. **Data Structures**
   - Circular buffer implementation
   - Modulo arithmetic for index wrapping
   - Producer-consumer pattern

4. **String Processing**
   - Custom tokenization without `strtok()`
   - Command parsing and validation
   - Memory-safe buffer handling

5. **Embedded Design Patterns**
   - Interrupt-driven I/O (non-blocking RX)
   - Polling TX (acceptable for debug output)
   - State machine integration (LED control modes)

---

##  Known Issues and Limitations

| Issue | Impact | Workaround |
|-------|--------|------------|
| TX is blocking | Main loop freezes during long prints | Use DMA for TX (planned) |
| No command history | Can't recall previous commands | Add ring buffer with up/down arrow support |
| Fixed buffer size | Long commands truncated | Increase `CMD_BUFFER_SIZE` or add dynamic allocation |
| No hardware flow control | Potential data loss at high speeds | Implement RTS/CTS if needed |

---

##  Roadmap

### Planned Features
- [ ] **Timer Integration**: Non-blocking `BLINK` command using TIM2 interrupts
- [ ] **DMA TX**: High-throughput logging without blocking
- [ ] **I2C Sensor Commands**: Read accelerometer/gyro data via CLI
- [ ] **Command History**: Up/down arrow key navigation
- [ ] **Auto-completion**: Tab completion for commands
- [ ] **Multi-LED Patterns**: PWM-based brightness control

### Architecture Improvements
- [ ] Migrate to event-driven state machine
- [ ] Add unit tests for command parser
- [ ] Implement watchdog timer for safety
- [ ] Low-power modes between commands

---

##  References

- [STM32F446xx Reference Manual (RM0390)](https://www.st.com/resource/en/reference_manual/rm0390-stm32f446xx-advanced-armbased-32bit-mcus-stmicroelectronics.pdf)
- [STM32F446RE Datasheet](https://www.st.com/resource/en/datasheet/stm32f446re.pdf)
- [STM32 Nucleo-64 User Manual (UM1724)](https://www.st.com/resource/en/user_manual/um1724-stm32-nucleo64-boards-mb1136-stmicroelectronics.pdf)
- [ARM Cortex-M4 Technical Reference Manual](https://developer.arm.com/documentation/100166/0001)

---

##  Contributing

This is a personal learning project, but suggestions and improvements are welcome:

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/improvement`)
3. Commit changes with clear messages
4. Push and open a Pull Request

**Focus areas for contributions:**
- Bug fixes in edge cases
- Performance optimizations
- Additional command implementations
- Documentation improvements

---

##  License

MIT License - see [LICENSE](LICENSE) file for details.

**TL;DR:** Free to use, modify, and distribute. Just keep the copyright notice.

---

##  Author

**Adarsha Udupa Baikady**  
Undergraduate | Electronics & Instrumentation Engineering  
Focus: Embedded Systems & Firmware Development

- GitHub: [@adarshaudupa](https://github.com/adarshaudupa)
- LinkedIn: [adarsha-udupa-baikady](https://www.linkedin.com/in/adarsha-udupa-baikady-327a54219)
- Email: adarsha8505@gmail.com

---

##  Acknowledgments

Built as part of my bare-metal STM32 learning journey, with guidance from:
- STM32 community forums and Discord servers
- Fastbit Embedded Brain Academy YouTube tutorials
- *Mastering STM32* by Carmine Noviello
- Direct mentorship and code reviews

---

**Built with no HAL, no Arduino—just registers, datasheets, and determination.**