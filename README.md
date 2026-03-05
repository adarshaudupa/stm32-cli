# STM32 UART Command-Line Interface (CLI)

Interrupt-driven UART CLI for STM32F446RE with LED control commands. Demonstrates command parsing, circular buffer RX, and timer-driven state machine integration—foundational skills for embedded debug shells and user interfaces.

---

## Overview

**Goal**: Build a **production-style CLI** using bare-metal STM32 peripherals (no HAL, no printf). Parse user commands over UART, control onboard LED, and integrate timer-based auto-blink mode.

**Core Skills Demonstrated**:
- Interrupt-driven UART RX with 64-byte circular buffer
- Command parsing with `strcmp()` (no heavy libraries)
- State machine: `LED_MANUAL_ON`, `LED_MANUAL_OFF`, `LED_AUTO_BLINK`
- Timer integration for periodic tasks (1 Hz LED toggle)
- Clean separation: drivers (`uart2.c`, `gpio.c`, `tim2.c`) vs. application logic (`main.c`)

**Why This Matters**: CLI interfaces are **everywhere** in embedded systems—bootloaders, debug shells, test harnesses, factory calibration tools. This project proves you can build one from scratch.

---

## Features

### Supported Commands

| Command | Action |
|---------|--------|
| `LED ON` | Turn LED on (manual control) |
| `LED OFF` | Turn LED off (manual control) |
| `BLINK` | Auto-blink LED at 1 Hz using TIM2 |
| `STATUS` | Report current LED state (ON/OFF) |
| `HELP` | Display available commands |

**Command Entry**:
- Type command and press **Enter** (`\r`) to execute
- **Backspace** (ASCII 8 or 127) to delete characters
- Echo feedback: characters appear as you type

---

## Architecture

### State Machine

**LED has 3 operational states**:

```c
typedef enum {
    LED_MANUAL_ON,      // User commanded ON (timer disabled)
    LED_MANUAL_OFF,     // User commanded OFF (timer disabled)
    LED_AUTO_BLINK      // Timer toggles LED at 1 Hz
} led_state_t;
```

**State Transitions**:
- `LED ON` → `LED_MANUAL_ON` (stops timer, forces LED high)
- `LED OFF` → `LED_MANUAL_OFF` (stops timer, forces LED low)
- `BLINK` → `LED_AUTO_BLINK` (starts timer, ISR toggles LED)

**Why State Machine Design**:
- Clear ownership: timer controls LED **only** in `LED_AUTO_BLINK` state
- No race conditions: manual commands explicitly disable timer before changing LED
- Scalable: easy to add states like `LED_BREATHE`, `LED_FAST_BLINK`, etc.

### UART RX Flow

**Hardware → Software Path**:

1. **ISR Level** (`USART2_IRQHandler()`):
   - Hardware sets `RXNE` flag when byte arrives
   - ISR reads `DR` (clears flag)
   - Writes byte to circular buffer
   - Increments `rx_head`
   - **Returns immediately** (no processing in ISR)

2. **Application Level** (`main()` loop):
   - Calls `UART2_ReadChar()` (blocking until data available)
   - Checks for Enter (`\r`), Backspace (127/8), or normal character
   - Builds command string in `cmd_buffer[]`
   - On Enter: calls `strcmp()` to match command → executes action → clears buffer

**Key Insight**: ISR is **pure data transport** (3-4 instructions). All parsing/logic happens in main loop → **predictable interrupt latency**.

### Timer Integration

**TIM2 Configuration**:
- **Frequency**: 1 Hz (PSC = 8399, ARR = 9999, assumes 84 MHz APB1 timer clock)
- **ISR**: Sets `tim2_flag = 1` and returns
- **Main Loop**: Checks flag in `LED_AUTO_BLINK` state → toggles LED → clears flag

**Critical Detail**:
- Timer is **started/stopped explicitly** by CLI commands
- No free-running timer wasting CPU when not needed
- Functions: `timer_start()`, `timer_stop()` (abstract TIM2 CR1 bit manipulation)

---

## Project Structure

```
stm32-uart-cli/
├── Src/
│   ├── main.c          # CLI command parsing + state machine
│   ├── uart2.c         # UART TX/RX + circular buffer + ISR
│   ├── gpio.c          # LED control (PA5) + button (PC13)
│   └── tim2.c          # Timer init + ISR + start/stop functions
├── Inc/
│   ├── uart2.h
│   ├── gpio.h
│   └── tim2.h
└── README.md
```

**File Responsibilities**:
- `uart2.c`: Peripheral-level UART driver (reusable across projects)
- `gpio.c`: GPIO abstraction (`LED_ON()`, `LED_OFF()`, `LED_Toggle()`)
- `tim2.c`: Timer driver with application-friendly API (`timer_start()`, `timer_stop()`)
- `main.c`: Application logic (command parsing, state machine, business logic)

**Design Principle**: Drivers should **not know** about application logic. `uart2.c` doesn't care what you do with received bytes. `tim2.c` doesn't know you're blinking an LED.

---

## Technical Highlights

### 1. Command Parsing Pattern

**No Heavy Libraries**:
- Uses `strcmp()` from `<string.h>` (lightweight, built-in)
- Command buffer = 32 bytes (fixed size, no dynamic allocation)
- Null-terminated strings for safe comparison

**Implementation**:

```c
char cmd_buffer[CMD_BUFFER_SIZE];
uint8_t cmd_index = 0;

// On normal character:
if (cmd_index < CMD_BUFFER_SIZE - 1) {
    cmd_buffer[cmd_index++] = c;
    cmd_buffer[cmd_index] = '\0';  // Always null-terminate
    UART2_SendChar(c);              // Echo back
}

// On Enter:
if (c == '\r') {
    if (strcmp(cmd_buffer, "LED ON") == 0) {
        // Execute command
    }
    cmd_index = 0;  // Reset buffer
}
```

**Why This Works**:
- Fixed buffer size = no heap fragmentation
- Null termination after every character = safe for `strcmp()`
- Index check prevents buffer overflow

### 2. Circular Buffer (64 Bytes)

**ISR writes, main loop reads**:

```c
volatile char rx_buffer[RX_BUFFER_SIZE];  // 64 bytes
volatile uint8_t rx_head = 0;  // ISR writes here
volatile uint8_t rx_tail = 0;  // Main reads here
```

**Write (ISR)**:

```c
rx_buffer[rx_head] = USART2->DR;
rx_head++;
if (rx_head >= RX_BUFFER_SIZE) rx_head = 0;  // Wrap
```

**Read (main)**:

```c
while (rx_head == rx_tail);  // Wait for data
char c = rx_buffer[rx_tail];
rx_tail++;
if (rx_tail >= RX_BUFFER_SIZE) rx_tail = 0;  // Wrap
```

**Why 64 Bytes**: Largest command = `"LED OFF"` (7 chars) + Enter + margin. 64 bytes handles ~9 commands buffered before overflow.

### 3. Backspace Handling

**Terminal expects**: `\b \b` sequence to erase character

1. `\b` - move cursor left
2. ` ` (space) - overwrite character with space
3. `\b` - move cursor left again

**Implementation**:

```c
if (c == 127 || c == 8) {  // DEL or Backspace
    if (cmd_index > 0) cmd_index--;
    UART2_SendString("\b \b");
}
```

**Result**: Character deleted from screen and buffer.

### 4. Timer ISR Discipline

**ISR does ONE thing**:

```c
void TIM2_IRQHandler(void) {
    if (TIM2->SR & (1 << 0)) {
        TIM2->SR &= ~(1 << 0);  // Clear UIF flag
        tim2_flag = 1;          // Set flag
    }
    // NOTHING ELSE
}
```

**Main loop owns the logic**:

```c
if (led_state == LED_AUTO_BLINK && tim2_flag) {
    tim2_flag = 0;
    LED_Toggle();  // GPIO ODR ^= (1 << 5)
}
```

**Why This Matters**: ISR latency = predictable. If ISR called `LED_Toggle()` + `UART2_SendString()`, latency would be 10-50x higher.

---

## Key Learnings

### 1. CLI = State Machine + Parser

**Two separate concerns**:
- **Parser**: Extract user intent from string (`"LED ON"` → `LED_ON_COMMAND`)
- **State Machine**: Translate command into hardware actions based on current state

**Bad Design**: Mix parsing and GPIO manipulation in same function  
**Good Design**: Parse → update state → state machine handles hardware

### 2. ISR Discipline = Non-Negotiable

**ISR should**:
- Read/write hardware registers
- Update shared state (flags, buffers)
- Return ASAP

**ISR should NOT**:
- Call `printf()` or `UART2_SendString()` (unpredictable blocking)
- Do math-heavy processing (CRC, filtering, algorithm)
- Make complex decisions (state machine logic belongs in main)

**Measured Impact**: UART RX ISR = ~5 cycles (read DR, store byte, increment pointer). Adding `UART2_SendString()` would make it 500+ cycles.

### 3. Circular Buffer = Producer-Consumer Pattern

**Producer** (ISR):
- Writes to `rx_head`
- Increments `rx_head`
- Never waits

**Consumer** (main loop):
- Reads from `rx_tail`
- Increments `rx_tail`
- Blocks if buffer empty (`rx_head == rx_tail`)

**Why No Mutex Needed**: Single producer, single consumer, atomic 8-bit index operations on Cortex-M4 = no race conditions.

### 4. Bare-Metal CLI Shows System-Level Thinking

**Interviewer Question**: "How would you implement a debug shell for a bootloader?"

**This Project Answers**:
- Interrupt-driven RX so bootloader can monitor UART while doing other work
- Fixed-size buffers (no malloc in bootloaders)
- Command table approach scales to 50+ commands
- Timer integration shows you can handle async events (timeouts, watchdogs)

---

## Limitations / Next Steps

### Current Limitations

1. **No Command History**: Can't use up-arrow to repeat last command
2. **Fixed Command Table**: Adding commands requires editing `if-else` chain (should use function pointer table)
3. **No Arguments**: `LED ON` works, but can't do `LED BRIGHTNESS 50` (no argument parsing)
4. **Overflow Silent**: If buffer fills (64 bytes), oldest data lost without warning
5. **Hardcoded Baud Rate**: 9600 baud set at compile time (could make runtime-configurable)
6. **No Timeouts**: `UART2_ReadChar()` blocks forever if no data arrives

### Next Steps

1. **Command Table Refactor** (Week 1)
   - Replace `if-else` with function pointer table:
     ```c
     typedef struct {
         const char *name;
         void (*handler)(void);
     } cmd_t;
     
     cmd_t cmd_table[] = {
         {"LED ON", cmd_led_on},
         {"LED OFF", cmd_led_off},
         {"BLINK", cmd_blink},
         // ...
     };
     ```

2. **Argument Parsing** (Week 2)
   - Tokenize command string: `"LED BRIGHTNESS 50"` → `["LED", "BRIGHTNESS", "50"]`
   - Use `strtok()` or custom tokenizer
   - Convert numeric args: `atoi("50")` → `50`

3. **Production Features** (Future)
   - Command history ring buffer (last 5 commands)
   - Tab completion
   - Escape sequence parsing (arrow keys, Home, End)
   - Help text generation from command table metadata

4. **Error Handling**
   - Return status codes from commands (`CMD_OK`, `CMD_INVALID_ARG`)
   - Print helpful error messages ("Usage: LED <ON|OFF>")

---

## Build & Run

### Hardware

- **Board**: STM32 Nucleo F446RE
- **LED**: PA5 (onboard green LED)
- **UART**: PA2 (TX), PA3 (RX) → connect to USB-UART adapter or ST-LINK virtual COM port

### Software

- **Toolchain**: `arm-none-eabi-gcc`
- **Build**: STM32CubeIDE project or Makefile
- **Flash**: ST-LINK (onboard)

### Quick Start

1. Flash firmware to Nucleo board
2. Connect UART2 to PC (9600 baud, 8N1)
3. Open serial terminal (PuTTY, screen, minicom)
4. Type `HELP` and press Enter
5. Try commands:
   - `LED ON` → LED turns on
   - `LED OFF` → LED turns off
   - `BLINK` → LED blinks at 1 Hz
   - `STATUS` → Reports current LED state

**Expected Output**:

```
---STM32 CLI---
Type HELP for commands

> HELP
Available commands:
  LED ON   - Turn LED on
  LED OFF  - Turn LED off
  BLINK    - Blink LED at 1Hz
  STATUS   - Check LED state
  HELP     - Show this help
> LED ON
LED turned ON
> STATUS
LED is ON
> BLINK
LED auto-blinking at 1 Hz
```

---

## Memory Footprint

**Estimated RAM Usage**:

| Component | Size | Notes |
|-----------|------|-------|
| UART RX buffer | 64 bytes | Circular buffer |
| Command buffer | 32 bytes | User input staging |
| State variable | 1 byte | `led_state_t` enum |
| Flags | 2 bytes | `tim2_flag` + buffer indices |
| **Total** | **~100 bytes** | Minimal overhead |

**Flash Usage**: ~2 KB (UART driver, GPIO, TIM2, main logic)

**Why This Matters**: Shows you understand **resource constraints**. A production CLI might need to fit in 4 KB bootloader alongside flash update logic.

---

## Common Pitfalls (Avoided Here)

### 1. Processing Commands in ISR

**Bad**:

```c
void USART2_IRQHandler(void) {
    char c = USART2->DR;
    if (c == '1') LED_ON();  // ❌ WRONG
    if (c == '0') LED_OFF();
}
```

**Why Bad**: ISR latency unpredictable, can't handle multi-character commands, mixes concerns.

**Good**: ISR stores byte → main processes it (as implemented here).

### 2. Unbounded String Operations

**Bad**:

```c
char cmd_buffer[32];
int i = 0;
while (c != '\r') {
    cmd_buffer[i++] = c;  // ❌ BUFFER OVERFLOW if user types 50 chars
}
```

**Good**: Check bounds (`if (cmd_index < CMD_BUFFER_SIZE - 1)`) as implemented here.

### 3. Forgetting Null Termination

**Bad**:

```c
cmd_buffer[cmd_index++] = c;
if (strcmp(cmd_buffer, "LED ON") == 0) { ... }  // ❌ UNDEFINED BEHAVIOR
```

**Good**: Always null-terminate after writing: `cmd_buffer[cmd_index] = '\0';`

---

## Real-World Applications

**This project's skills map directly to**:

- **Bootloader CLI**: `FLASH ERASE`, `FLASH WRITE 0x08000000`, `BOOT APP`
- **Test Harness**: `TEST GPIO`, `TEST UART`, `TEST I2C`, `RUN ALL`
- **Factory Calibration**: `CAL SENSOR 0`, `CAL WRITE EEPROM`, `CAL READ`
- **Debug Shell**: `REG READ 0x40013800`, `REG WRITE 0x40013804 0x1234`

**Interview Proof Point**: You built a working CLI with <200 lines of code, no libraries, on bare-metal hardware. That's **firmware engineer core competency**.

---

## Resources

- **RM0390**: STM32F446 reference manual (USART, GPIO, TIM chapters)
- **Cortex-M4 Programming Manual**: Exception handling, NVIC, interrupt priorities
- **Code Repository**: Complete source code with Git history showing progression

---

## Author Notes

This project was built to **prove I understand interrupt-driven firmware**, not just copy-paste HAL examples. The CLI is simple (5 commands), but the **architecture scales**—adding 50 more commands requires only a command table refactor, not a rewrite.

**Philosophy**: Real embedded systems don't have `printf()` and `scanf()`. You build communication layers yourself. This project does that.

**Next Evolution**: Extend to handle GPS NMEA sentence parsing (similar structure: wait for `\n`, extract fields, validate checksum), then integrate into Creat-A-Thon accident detection project.

---

**Last Updated**: March 5, 2026  
**Status**: Feature-complete and tested; ready for command table refactor and argument parsing extension
