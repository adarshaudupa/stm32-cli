/*
 * uart.h
 *
 * Minimal UART driver for STM32F446RE
 * USART2: PA2 (TX), PA3 (RX)
 *
 * Register-level implementation (no HAL)
 */
#include<stdint.h>

#ifndef INC_UART2_H_
#define INC_UART2_H_

#define RX_BUFFER_SIZE 256
extern volatile uint8_t  packet_ready;
extern volatile uint16_t packet_length;
extern char rx_buffer[RX_BUFFER_SIZE];




// ============================================================================
// FUNCTION PROTOTYPES
// ============================================================================

/**
 * @brief  Initialize USART2 for TX and RX
 * @param  baudrate: Desired baud rate (e.g., 9600, 115200)
 * @note   Configures PA2 (TX) and PA3 (RX) as AF7
 * @note   Assumes 16 MHz APB1 clock
 */
void UART2_Init(void);
/**
 * @brief  Transmit a single character (blocking)
 * @param  ch: Character to send
 * @note   Waits for TXE flag before writing to DR
 */
void DMA1_Init(void);
void UART2_SetBaud(uint32_t baudrate);
void UART2_SendChar(char ch);

/**
 * @brief  Transmit a null-terminated string (blocking)
 * @param  str: Pointer to string
 */
void UART2_SendString(char *str);

/**
 * @brief  Receive a single character (blocking)
 * @return Received character
 * @note   Waits for RXNE flag before reading from DR
 */
uint8_t UART2_DataAvailable(void);

#endif /* INC_UART2_H_ */
