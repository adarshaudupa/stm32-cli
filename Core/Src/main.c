#include "gpio.h"
#include "stm32f4xx.h"
#include "tim2.h"
#include "uart2.h"
#include <string.h>
#define CMD_BUFFER_SIZE 64
#define HISTORY_SIZE 5
char history[HISTORY_SIZE][CMD_BUFFER_SIZE];
uint8_t browse_index = 0;
uint8_t write_index = 0;
uint8_t esc_state = 0;
uint8_t history_count = 0; // Tracks how many commands are stored in history

void Save_To_History(char* new_cmd, uint8_t length)
{
    // 1. Copy the new command into the current write slot
    // We add +1 to the length to ensure the '\0' null terminator is copied
    memcpy(history[write_index], new_cmd, length + 1);

    // 2. Advance the index and wrap around mathematically
    write_index = (write_index + 1) % HISTORY_SIZE;
    if (history_count < HISTORY_SIZE)
    	history_count++; // Increment count until full
}

volatile led_state_t led_state = LED_MANUAL_OFF;


char cmd_buffer[CMD_BUFFER_SIZE];
 uint8_t cmd_index = 0;
 uint8_t browse_depth = 0;

int main(void)
{
	cmd_index = 0;
	cmd_buffer[0] = '\0';
	PA5_Init(); // Enable GPIOA clock (AHB1 bus, bit 0)
    UART2_Init(); // Initialize UART2
    TIM2_Init(); // Initialize TIM2 (but don't start it)
    timer_stop();  // Make sure it's stopped initially
    UART2_SendString("---STM32 CLI---\r\n");
    UART2_SendString("Type HELP for commands\r\n\r\n");
    UART2_SendString("> ");

    while(1)
        {
            char c = UART2_ReadChar(); // Extracts each character

            // ====================================================================
            // STATE MACHINE 1: ARROW KEY INTERCEPTOR
            // ====================================================================
            if (esc_state == 1)
            {
                if (c == '[') esc_state = 2;
                else esc_state = 0;
                continue;
            }
            else if (esc_state == 2)
            {
                if (c == 'A') // UP ARROW
                {
                    if(browse_depth >= history_count)
                    {
						// If we've browsed through all history, don't go further back
                        esc_state = 0;
                        continue;  // don't move
					}
                    browse_depth++;
                    browse_index = (browse_index - 1 + HISTORY_SIZE) % HISTORY_SIZE;
                	// Move backward in the circular buffer

                    strcpy(cmd_buffer, history[browse_index]);
                    cmd_index = strlen(cmd_buffer);

                    // UI TRICK: Clear the current terminal line with spaces, then print history
                    UART2_SendString("\r>                                 \r> ");
                    UART2_SendString(cmd_buffer);
                }

                else if (c == 'B') // DOWN ARROW
                {
                	if(browse_depth == 0)
					{
						esc_state = 0;
						continue;  // don't move
					}
                	browse_depth--;
                	browse_index = (browse_index + 1) % HISTORY_SIZE;
                    // Move forward in the circular buffer
                    strcpy(cmd_buffer, history[browse_index]);
                    cmd_index = strlen(cmd_buffer);

                    // UI TRICK: Clear the current terminal line with spaces, then print history
                    UART2_SendString("\r>                                 \r> ");
                    UART2_SendString(cmd_buffer);
                }
                esc_state = 0; // Sequence complete, reset interceptor

                continue;
            }

            if (c == '\x1B') // ESC character detected
            {
                esc_state = 1;
                continue;
            }

            // ====================================================================
            // STATE MACHINE 2: NORMAL CHARACTER PROCESSING
            // ====================================================================
            else if(c == '\r') // checks if Enter (\r) is pressed
            {
                UART2_SendString("\r\n");

                // 1. SAVE TO HISTORY (While state is still valid)
                if (cmd_index > 0)
                {
                    Save_To_History(cmd_buffer, cmd_index);
                }

                // 2. RESET BROWSE INDEX (Snap back to the present)
                browse_index = write_index;

                // 3. EXECUTE COMMAND
                if(strcmp(cmd_buffer, "HELP") == 0)
                {
                    UART2_SendString("Available commands:\r\n");
                    UART2_SendString("  LED ON   - Turn LED on\r\n");
                    UART2_SendString("  LED OFF  - Turn LED off\r\n");
                    UART2_SendString("  BLINK    - Blink LED at 1Hz\r\n");
                    UART2_SendString("  STATUS   - Check LED state\r\n");
                    UART2_SendString("  SET BAUD <rate> - Set UART baud rate\r\n");
                    UART2_SendString("  HELP     - Show this help\r\n");
                }
                else if(strcmp(cmd_buffer, "LED ON") == 0)
                {
                    timer_stop();
                    led_state = LED_MANUAL_ON;
                    LED_ON();
                    UART2_SendString("LED turned ON\r\n");
                }
                else if(strcmp(cmd_buffer, "LED OFF") == 0)
                {
                    timer_stop();
                    led_state = LED_MANUAL_OFF;
                    LED_OFF();
                    UART2_SendString("LED turned OFF\r\n");
                }
                else if(strcmp(cmd_buffer, "BLINK") == 0)
                {
                    led_state = LED_AUTO_BLINK;
                    timer_start();
                    UART2_SendString("LED auto-blinking at 1 Hz\r\n");
                }
                else if(strcmp(cmd_buffer, "STATUS") == 0)
                {
                    // Checking ODR to verify physical state
                    if (GPIOA->ODR & (1 << 5))
                    {
                        UART2_SendString("LED is ON\r\n");
                    }
                    else
                    {
                        UART2_SendString("LED is OFF\r\n");
                    }
                }
                else if (strncmp(cmd_buffer, "SET BAUD ", 9) == 0)
                {
                    uint32_t baud = 0;
                    uint8_t i = 9;
                    while (cmd_buffer[i] >= '0' && cmd_buffer[i] <= '9' && cmd_buffer[i] != '\0')
                    {
                        baud = (baud * 10) + (cmd_buffer[i] - '0');
                        i++;
                    }
                    if (baud > 0)
                    {
                        UART2_SendString("Changing baud rate to ");
                        UART2_SendString(&cmd_buffer[9]);
                        UART2_SendString("...\r\n");
                        UART2_SendString("Please update your serial terminal to match!\r\n");

                        // WAIT FOR TC FLAG (Transmission Complete)
                        while (!(USART2->SR & (1 << 6)));

                        UART2_SetBaud(baud);
                    }
                    else
                    {
                        UART2_SendString("> Error: Invalid baud rate format.\r\n");
                    }
                }
                else if (cmd_index > 0)
                {
                    UART2_SendString("Unknown command: ");
                    UART2_SendString(cmd_buffer);
                    UART2_SendString("\r\nType HELP for commands\r\n");
                }

                // 4. CLEANUP (Wipe the volatile state for the next command)
                cmd_index = 0;
                cmd_buffer[0] = '\0';
                UART2_SendString("> ");
            }
            else if(c == 127 || c == 8) // Backspace handling
            {
                if(cmd_index > 0)
                {
                    cmd_index--;
                    UART2_SendString("\b \b"); // Erase character from screen
                }
            }
            else if(cmd_index < CMD_BUFFER_SIZE - 1) // Normal typing
            {
                cmd_buffer[cmd_index++] = c;
                cmd_buffer[cmd_index] = '\0'; // Always maintain null termination
                UART2_SendChar(c); // Echo character
            }
        }
    }

