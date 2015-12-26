#include <xc.h>
#include "uart.h"

void uart_init(void)
{
    BRGH  = 0;
    BRG16 = 0;
    SPBRG = 29;

    TRISC6 = 1;
    TRISC7 = 1;

    SYNC  = 0;
    SPEN  = 1;
    TXEN  = 1;
    
    TRISC6 = 0;
}

void putch(char c)
{
    while (! TXIF)
        ;
    
    TXREG = c;
}