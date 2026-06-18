#include "uart2.h"
#include "stm32f4xx.h"
#include "clock.h"

char rx_buffer[RX_BUFFER_SIZE];
volatile uint8_t packet_ready  = 0;
volatile uint16_t packet_length = 0;

void UART2_Init(void)
{
	RCC->AHB1ENR |= (1 << 0); // GPIOA clock
    RCC->APB1ENR |= (1 << 17);         // USART2 clock

    // PA2=TX, PA3=RX → AF mode
    GPIOA->MODER &= ~((3 << 4) | (3 << 6));
    GPIOA->MODER |=  ((2 << 4) | (2 << 6));
    GPIOA->AFR[0] &= ~((0xF << 8) | (0xF << 12));
    GPIOA->AFR[0] |=  ((7 << 8)   | (7 << 12));

    USART2->BRR = 0x0683;              // 9600 baud
    USART2->CR3 |= (1 << 6);          // DMAR: enable DMA on RX
    USART2->CR1 |= (1 << 4);          // IDLEIE: idle line interrupt
    USART2->CR1 |= (1 << 13) | (1 << 3) | (1 << 2); // UE | TE | RE

    NVIC->ISER[1] |= (1 << 6);        // USART2 IRQ (IRQ38 → ISER[1] bit 6)
}

void DMA1_Init(void)
{
    RCC->AHB1ENR |= (1 << 21);        // DMA1 clock

    DMA1_Stream5->CR = 0;
    while (DMA1_Stream5->CR & (1 << 0)); // Wait until disabled

    DMA1_Stream5->CR  |= (4 << 25);   // Channel 4: USART2_RX
    DMA1_Stream5->CR  &= ~((3 << 11) | (3 << 13)); // PSIZE=byte, MSIZE=byte
    DMA1_Stream5->CR  |= (1 << 10);   // MINC: memory increment
    // No CIRC bit — linear mode
    DMA1_Stream5->PAR  = (uint32_t)&USART2->DR; // Peripheral address
    DMA1_Stream5->M0AR = (uint32_t)rx_buffer; // Memory address
    DMA1_Stream5->NDTR = RX_BUFFER_SIZE; // Number of data items to transfer

    DMA1_Stream5->CR  |= (1 << 0);    // EN: enable stream
}

void UART2_SetBaud(uint32_t baud)
{
    USART2->CR1 &= ~(1 << 13); // Disable USART before changing baud rate
    uint32_t pclk     = get_apb1_freq_hz(); // Get APB1 clock frequency
    uint32_t mantissa = pclk / (16 * baud); // Calculate mantissa
    uint32_t fraction = ((pclk % (16 * baud)) * 16 + (8 * baud)) / (16 * baud); // Calculate fraction with rounding
    if (fraction >= 16)
    {
    	mantissa++;
    	fraction = 0;
    } // Handle rounding overflow
    USART2->BRR = (mantissa << 4) | (fraction & 0xF);// Set new baud rate
    USART2->CR1 |= (1 << 13); // Re-enable USART
}

void UART2_SendChar(char ch)
{
    while (!(USART2->SR & (1 << 7)));
    USART2->DR = ch;
}

void UART2_SendString(char *str)
{
    while (*str) UART2_SendChar(*str++);
}

// ============================================================================
// ISR — sets flag, main does the work
// ============================================================================
void USART2_IRQHandler(void)
{
    if (USART2->SR & (1 << 4))
    {
        volatile uint32_t tmp = USART2->SR; // Read SR to clear IDLE flag
        tmp = USART2->DR; // Read DR to clear IDLE flag
        (void)tmp; // Prevent unused variable warning

        packet_length = RX_BUFFER_SIZE - DMA1_Stream5->NDTR; // Calculate how many bytes were received

        DMA1_Stream5->CR &= ~(1 << 0); // Disable DMA stream
        while (DMA1_Stream5->CR & (1 << 0)); // Wait until disabled

        // ← ADD THIS: clear DMA interrupt flags before re-enable
        DMA1->HIFCR |= (0x3F << 6); // Clear all Stream5 flags

        DMA1_Stream5->NDTR = RX_BUFFER_SIZE; // Reset number of data items to transfer
        DMA1_Stream5->M0AR = (uint32_t)rx_buffer; // Reset memory address
        DMA1_Stream5->CR  |= (1 << 0); // Re-enable DMA stream

        packet_ready = 1; // Set flag for main loop to process the received packet
    }
}
