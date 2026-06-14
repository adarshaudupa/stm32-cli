#include "gpio.h"
#include "stm32f4xx.h"
#include "tim2.h"
#include "uart2.h"
#include <string.h>
#define CMD_BUFFER_SIZE 64

volatile led_state_t led_state = LED_MANUAL_OFF;


char cmd_buffer[CMD_BUFFER_SIZE];
 uint8_t cmd_index = 0;

int main(void) {
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
    	char c = UART2_ReadChar(); //Extracts each character from the user enterred command
    	if(c=='\r') //checks if Enter(\r) is pressed
    	{
    		UART2_SendString("\r\n");

    		//Start of task
    		if(strcmp(cmd_buffer, "HELP")==0)
    		{
    		 UART2_SendString("Available commands:\r\n");
    		 UART2_SendString("  LED ON   - Turn LED on\r\n");
    		 UART2_SendString("  LED OFF  - Turn LED off\r\n");
    		 UART2_SendString("  BLINK    - Blink LED at 1Hz\r\n");
    		 UART2_SendString("  STATUS   - Check LED state\r\n");
    		 UART2_SendString("  SET BAUD <rate> - Set UART baud rate\r\n");
    		 UART2_SendString("  HELP     - Show this help\r\n");
    		}
    		 else if(strcmp(cmd_buffer, "LED ON")==0)
			 {
    		  timer_stop();
    		  led_state = LED_MANUAL_ON;
			  LED_ON();
			  UART2_SendString("LED turned ON\r\n");
			 }
			 else if(strcmp(cmd_buffer,"LED OFF")==0)
			 {
			  timer_stop();
			  led_state = LED_MANUAL_OFF;
			  LED_OFF();
			  UART2_SendString("LED turned OFF\r\n");
			 }
			 else if(strcmp(cmd_buffer,"BLINK")==0)
			 {
			  led_state = LED_AUTO_BLINK;
			  timer_start();
			  UART2_SendString("LED auto-blinking at 1 Hz\r\n");
			 }
			 else if(strcmp(cmd_buffer,"STATUS")==0)
			 {
				 if (GPIOA->ODR & (1 << 5)) //if ODR is 1 output is 1 so it means LED is ON at that instant
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
					 // WAIT FOR TRANSMISSION COMPLETE
					 // SR bit 6: TC (Transmission Complete)
					 // Ensure the last character is physically pushed out of the shift register
					 while (!(USART2->SR & (1 << 6)));
					 // Update the hardware registers
					 UART2_SetBaud(baud);
					}
				  else
					  {
					   UART2_SendString("> Error: Invalid baud rate format.\r\n");
					   }
			 }
			 else if (cmd_index > 0)
			 {  // Non-empty unknown command
			  UART2_SendString("Unknown command: ");
			  UART2_SendString(cmd_buffer);
			  UART2_SendString("\r\nType HELP for commands\r\n");
			 }
    		//Emd of task
    		//clear the command buffer
    		cmd_index = 0;
    		UART2_SendString("> ");
    	}
    	else if(c==127 || c==8) //127 is ASCII DEL button and ASCII 8 is Backspace
    	{
    	 if(cmd_index>0)
    	    cmd_index--;
    	   UART2_SendString("\b \b"); //To erase the character on screen
    	}
    	else if(cmd_index < CMD_BUFFER_SIZE-1) //normal characters
    	{
    		cmd_buffer[cmd_index++]=c;
    		cmd_buffer[cmd_index] = '\0';
    		UART2_SendChar(c); //echo the character typed
    	}
    }

}
