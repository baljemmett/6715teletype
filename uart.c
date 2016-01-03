#include <xc.h>
#include "uart.h"

#define BUFFER_SIZE 16

#if BUFFER_SIZE > 0
static volatile char achTxBuffer[BUFFER_SIZE];
static volatile unsigned char idxTxRead = 0, idxTxWrite = 0;

static volatile char achRxBuffer[BUFFER_SIZE];
static volatile unsigned char idxRxRead = 0, idxRxWrite = 0;
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
    
#if BUFFER_SIZE > 0
    RCIE = 1;
#endif
    
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

void uart_rx_isr(void)
{
#if BUFFER_SIZE > 0
    achRxBuffer[idxRxWrite++] = RCREG;
    if (idxRxWrite == BUFFER_SIZE)
        idxRxWrite = 0;
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

char uart_get_rx_byte(void)
{
#if BUFFER_SIZE > 0
    if (idxRxRead == idxRxWrite)
        return 0;
    
    char ch = achRxBuffer[idxRxRead++];
    
    if (idxRxRead == BUFFER_SIZE)
        idxRxRead = 0;
    
    return ch;
#else
    if (! RCIF)
        return 0;
    
    if (FERR)
        ;   // we dropped a byte, oops.
    
    return RCREG;
#endif
}