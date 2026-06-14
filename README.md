6/14/26, 9:56 PM 

Embedded Systems Skill Building Roadmap ~~-~~ Google Gemini 

## STM32 Bare-Metal CLI 

A deterministic, zero-allocation command-line interface engineered at the silicon level for the ARM Cortex-M4. 

Overview: This project bypasses heavy Hardware Abstraction Layer (HAL) state machines in favor of direct register-level execution. It provides a robust asynchronous serial terminal capable of hot-swapping hardware baud rates, parsing ANSI escape sequences, and managing command history with strict O(1) execution time. 

## Architectural Highlights 

## 1. Float-Free Register Math 

Standard baud rate configuration relies on memory-heavy floating-point division. This architecture replaces standard <math.h> dependencies with highly optimized integer arithmetic and modulo rounding ( pclk % (16 * baud) ) to calculate the USART BRR (Baud Rate Register) mantissa and fraction. 

## 2. Asynchronous UART Ring Buffer 

The core reception pipeline is entirely non-blocking. A dedicated USART2 ~~_~~ IRQHandler absorbs incoming bytes into a circular _r ~~x_~~ buffer atthe silicon level, decoupling the physical wire speed from the main CPU control loop. 

## 3. The ANSI Escape State Machine 

Arrow keys do not send standard ASCII characters; they transmit multi-byte ANSI escape sequences (e.g., \x1B[A ). This CLI features a dedicated UART Interceptor State Machine that catches the \x1B (ESC) byte, silently parses the sequence, and triggers history recall without ever polluting the live volatile command buffer. 

## 4. O(1) Circular History Buffer 

Command history is managed via a statically allocated 2D ring buffer. When saving new commands or browsing history, the system strictly moves a memory pointer ( write ~~_~~ index ), avoiding the massive CPU overhead of shifting memory arrays with memcpy() . 

## Command Reference 

## Command Hardware Action 

## Execution Mechanism 

HELP Lists available commands. Direct string literal transmission. 

https://gemini.google.com/app/cc65156a13aaeb7a 

1/2 

**==> picture [511 x 391] intentionally omitted <==**

**----- Start of picture text -----**<br>
||||||||||||||||||
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
|6/14/26,|9:56|PM|Embedded|Systems|Skill|Building|Roadmap|-|Google|Gemini|
|LED|ON|Drives|PA5|HIGH.|Atomic|assignment|to|BSRR|lower|16|bits.|
|LED|OFF|Drives|PA5|LOW.|Atomic|assignment|to|BSRR|upper|16|bits.|
|BLINK|Blinks|LED|at|1|Hz.|TIM2|Hardware|Timer|ISR|trigger.|
|STATUS|Reads|physical|pin|state.|Hardware|masking|of the|ODR|register.|
|SET|BAUD|Dynamically|hot-swaps|UART|Bypasses|atoi()|.|Custom|ASCIl-to-integer|parser|
|<rate>|baud|rate|(e.g.,|SET|BAUD|writes|to|USART2|-|>BRR|.|Blocks|onthe|TC|flag|to|
|115200|).|prevent|physical|wire|corruption.|
|Use|the UP and DOWN arrow keys|to|navigate|the command history.|
|Hardware|Integration|
|This|firmware|is|targeted|for the STM32|Nucleo-F446RE|development|board.|
|Component|Pin|Peripheral|Configuration|
|UART|TX|PA2|USART2|Alternate|Function|7|(AF7)|
|UART|RX|PA3|USART2|Alternate|Function|7|(AF7)|
|User|LED|PA5|GPIOA|General|Purpose|Output|(Push-Pull)|

**----- End of picture text -----**<br>


## Default Serial Connection: 

- e Baud Rate: 9620 (Configurable via CLI) 

- e Data Bits: 8 

- e Parity: None 

- e Stop Bits: 1 

## Compilation & Flashing 

1. Clone this repository to your local machine. 

2. Import the project directory into STM32CubelDE. 

3. Build the project ( Project -> Build Project ). 

4. Flashthe compiled .elf binary to the STM32F446RE board via the onboard ST-LINK. 

5. Opena serial terminal (PuTTY, Teralerm, etc.) mapped to the ST-LINK Virtual COM port using the default settings above. 

https://gemini.google.com/app/cc65156a13aaeb7a 

2/2 

