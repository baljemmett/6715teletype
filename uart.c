#include <xc.h>
#include "uart.h"

#define BUFFER_SIZE 0

#if BUFFER_SIZE > 0
static volatile char achTxBuffer[BUFFER_SIZE];
static volatile unsigned char idxTxRead = 0, idxTxWrite = 0;
#endif

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
    
    PEIE = 1;
}

void uart_tx_isr(void)
{
#if BUFFER_SIZE > 0
    TXREG = achTxBuffer[idxTxRead++];
    if (idxTxRead == BUFFER_SIZE)
        idxTxRead = 0;
    
    if (idxTxRead == idxTxWrite)
        TXIE = 0;
#endif
    
}

void putch(char c)
{
#if BUFFER_SIZE > 0
    if (idxTxRead == idxTxWrite && TXIF)
    {
        TXREG = c;
        return;
    }
    
    achTxBuffer[idxTxWrite++] = c;
    if (idxTxWrite == BUFFER_SIZE)
        idxTxWrite = 0;
    
    TXIE = 1;
#else
    while (! TXIF)
        ;
    
    TXREG = c;
#endif
}