#include <xc.h>

// #pragma config statements should precede project file includes.
// Use project enums instead of #define for ON and OFF.

// CONFIG1
#pragma config FOSC = HS        // Oscillator Selection (HS Oscillator, High-speed crystal/resonator connected between OSC1 and OSC2 pins)
#pragma config WDTE = OFF       // Watchdog Timer Enable (WDT disabled)
#pragma config PWRTE = ON       // Power-up Timer Enable (PWRT enabled)
#pragma config MCLRE = OFF      // MCLR Pin Function Select (MCLR/VPP pin function is digital input)
#pragma config CP = OFF         // Flash Program Memory Code Protection (Program memory code protection is disabled)
#pragma config BOREN = ON       // Brown-out Reset Enable (Brown-out Reset enabled)
#pragma config CLKOUTEN = OFF   // Clock Out Enable (CLKOUT function is disabled. I/O or oscillator function on the CLKOUT pin)
#pragma config IESO = OFF       // Internal/External Switchover (Internal/External Switchover mode is disabled)
#pragma config FCMEN = OFF      // Fail-Safe Clock Monitor Enable (Fail-Safe Clock Monitor is disabled)

// CONFIG2
#pragma config WRT = OFF        // Flash Memory Self-Write Protection (Write protection off)
#pragma config VCAPEN = OFF     // Voltage Regulator Capacitor Enable bit (VCAP pin function disabled)
#pragma config STVREN = ON      // Stack Overflow/Underflow Reset Enable (Stack Overflow or Underflow will cause a Reset)
#pragma config BORV = LO        // Brown-out Reset Voltage Selection (Brown-out Reset Voltage (Vbor), low trip point selected.)
#pragma config LPBOR = OFF      // Low-Power Brown Out Reset (Low-Power BOR is disabled)
#pragma config LVP = ON         // Low-Voltage Programming Enable (Low-voltage programming enabled)

#include <stdio.h>
#include "uart.h"
#include "keyboard.h"
#include "terminal.h"
#include "timers.h"
#include "leds.h"

//
// This is the main ISR, handling the slowest-latency interrupts; the
// actual ISR is in keyboard.c to ensure the tight timing isn't disturbed.
//
void main_isr(void)
{
    if (TXIF && TXIE)
        uart_tx_isr();
    if (RCIF && RCIE)
        uart_rx_isr();
    if (TMR0IF && TMR0IE)
        timers_isr();
}

int main(int argc, char* argv[])
{
    ANSELA = 0;
    ANSELB = 0;
    ANSELC = 0;
    ANSELD = 0;
    ANSELE = 0;

    leds_init();    
    timers_init();
    uart_init();
    keyboard_init();
    terminal_init();  
    
#if 0   // scan timing tests
    uint8_t target = ~1;
    uint8_t ticks  = 0;
    
    TMR0CS = 0;
    PSA    = 0;
    PS0    = 1;
    PS1    = 1;
    PS2    = 1;
    TMR0   = 0;
    TMR0IF = 0;
    
    while (1)
    {
        LED1 = (PORTB == 0xff);
        LED2 = (PORTB == target);
        
        if (TMR0IF == 1)
        {
            TMR0IF = 0;
            
            if (++ticks == 0)
            {
                target = ~((~target) * 2);
            }
        }
    }
#endif
    
#if 0
    while (1)
    {
        LED1 = 1;
        for (unsigned char c = '0'; c <= '9'; c++)
        {
            LED2 = 1;
            putchar(c);
            LED2 = 0;
        }
        LED1 = 0;
        GIE = 1;
        __delay_ms(20);
    }
#else
    GIE = 1;
    
    while (1)
    {
        keyboard_update();
        terminal_process();
    }
#endif
}