## **STM32 CLI Framework** 

Interrupt-driven command-line interface framework for the STM32F446RE built entirely with register-level programming and CMSIS, without HAL or middleware abstractions. 

Platform 

Architecture 

Language 

Style 

License 

## **Overview** 

This project implements a lightweight command-line interface (CLI) framework running on an STM32F446RE Nucleo board. 

The system combines: 

- Interrupt-driven UART reception 

- Circular-buffer-based data handling 

- Interactive command processing 

- Command history navigation 

- Timer-driven background execution 

- Runtime peripheral reconfiguration 

Unlike typical UART examples, this project focuses on firmware architecture and embedded software design principles commonly used in production systems. 

No HAL drivers are used. All peripherals are configured through direct register access using CMSIS device definitions. 

## **Key Features** 

## **UART Subsystem** 

- USART2 register-level configuration 

- Interrupt-driven RX path 

- Circular receive buffer 

- 

1 

- Runtime baud-rate switching 

- Character echo support • Backspace handling 

- Terminal-compatible operation 

## **CLI Engine** 

- Interactive shell interface 

- Command parsing and execution 

- Command history support 

- Up-arrow / Down-arrow navigation 

- Escape-sequence processing • Help system 

- Invalid command detection 

## **Timer Integration** 

- TIM2 interrupt configuration 

- Autonomous LED blinking 

- Background execution independent of CLI processing 

## **Firmware Design** 

- Bare-metal implementation 

- Modular driver architecture 

- CMSIS-only development 

- No dynamic memory allocation 

- Interrupt-driven communication model 

## **Demonstrated Embedded Concepts** 

|Category|Concepts|
|---|---|
|Peripheral Programming|RCC, GPIO, USART, TIM|
|Interrupts|RXNE interrupts, timer interrupts, NVIC confguration|
|Data Structures|Circular bufers, command history ring bufer|
|CLI Design|Parsing, dispatching, command management|
|State Management|LED state machine|
|Runtime Confguration|Dynamic UART baud-rate changes|
|Embedded Architecture|Driver separation and modular frmware design|



2 

## **System Architecture** 

**==> picture [456 x 381] intentionally omitted <==**

**----- Start of picture text -----**<br>
                    ┌─────────────────┐<br>                    │ Serial Terminal │<br>                    └────────┬────────┘<br>                             │<br>                             ▼<br>                  ┌────────────────────┐<br>                  │      USART2        │<br>                  │ RX Interrupts      │<br>                  └────────┬───────────┘<br>                           │<br>                           ▼<br>                  ┌────────────────────┐<br>                  │ Circular RX Buffer │<br>                  └────────┬───────────┘<br>                           │<br>                           ▼<br>                  ┌────────────────────┐<br>                  │     CLI Engine     │<br>                  └───────┬────────────┘<br>                          │<br>            ┌─────────────┼─────────────┐<br>            │             │             │<br>            ▼             ▼             ▼<br>      LED Control    Timer Control   UART Control<br>          GPIO            TIM2          USART2<br>**----- End of picture text -----**<br>


## **Hardware Platform** 

|Item|Specifcation|
|---|---|
|Board|STM32 Nucleo-F446RE|
|MCU|STM32F446RET6|
|Core|ARM Cortex-M4|
|Clock Source|HSI (16 MHz)|
|UART|USART2|
|Timer|TIM2|



3 

|Item|Specifcation|
|---|---|
|LED|LD2 (PA5)|



## **Pin Configuration** 

|Peripheral|Pin|Function|
|---|---|---|
|USART2_TX|PA2|UART Transmit|
|USART2_RX|PA3|UART Receive|
|LD2|PA5|Onboard User LED|



## **Supported Commands** 

|Command|Description|
|---|---|
|HELP|Display command list|
|LED ON|Turn LED on|
|LED OFF|Turn LED of|
|BLINK|Enable timer-driven blinking|
|STATUS|Display LED state|
|SET BAUD \<rate>|Change UART baud rate|



## **Example Session** 

```
---STM32 CLI---
Type HELP for commands
> HELP
Available commands:
  LED ON
  LED OFF
  BLINK
  STATUS
  SET BAUD <rate>
```

4 

```
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

## **Command History Implementation** 

The CLI supports navigation through previously executed commands using terminal arrow keys. 

```
↑ Previous Command
↓ Next Command
```

A fixed-size circular history buffer is maintained internally. 

```
History Buffer
┌─────┬─────┬─────┬─────┬─────┐
│CMD1 │CMD2 │CMD3 │CMD4 │CMD5 │
└─────┴─────┴─────┴─────┴─────┘
```

## **Runtime Baud Reconfiguration** 

The UART driver supports runtime baud-rate updates without reflashing firmware. 

Example: 

```
SET BAUD 9600
SET BAUD 57600
SET BAUD 115200
```

The BRR register is recalculated dynamically based on the active APB1 clock frequency. 

5 

## **Repository Structure** 

```
Core
├── Inc
│   ├── clock.h
│   ├── gpio.h
│   ├── tim2.h
│   └── uart2.h
│
└── Src
    ├── clock.c
    ├── gpio.c
    ├── main.c
    ├── tim2.c
    └── uart2.c
```

## **Engineering Decisions** 

## **Why Interrupt-Driven RX?** 

Polling wastes CPU cycles and risks missing incoming data. 

Using RXNE interrupts allows asynchronous reception while the main application remains responsive. 

## **Why Circular Buffers?** 

A circular buffer cleanly separates: 

- ISR producer 

- Main-loop consumer 

This is a common pattern in embedded communication stacks. 

## **Why Bare-Metal Development?** 

The objective is to understand: 

- Peripheral registers 

- Clock trees 

- Interrupt systems 

- Memory-mapped I/O 

- Hardware-software interaction 

6 

rather than relying on abstraction layers. 

## **Current Limitations** 

- TX path is blocking 

- UART error conditions are not yet handled 

- RX buffer overflow detection is not implemented 

- Command dispatch uses chained string comparisons 

- No DMA support 

These limitations are intentionally left visible to highlight future architectural improvements. 

## **Future Roadmap** 

## **Communication** 

- DMA-based UART transmission 

- UART error recovery 

- Configurable RX/TX buffers 

## **CLI** 

- Command registration table 

- Command auto-completion 

- Parameter validation framework 

- Command aliases 

## **Firmware Architecture** 

- Event-driven command dispatcher 

- Watchdog integration 

- Low-power operation 

- RTOS-based task separation 

## **Learning Outcomes** 

This project demonstrates practical experience with: 

- STM32 peripheral configuration 

- Interrupt-driven firmware 

- Circular buffer design 

- 

- Command-line interface development 

- Embedded state machines 

7 

- Timer-based scheduling 

- Runtime peripheral configuration 

- Modular firmware architecture 

## **Author** 

## **Adarsha Udupa** 

Electronics & Instrumentation Engineering 

Firmware & Embedded Systems Development 

GitHub: https://github.com/adarshaudupa 

## **License** 

MIT License 

See the LICENSE file for details. 

8 

