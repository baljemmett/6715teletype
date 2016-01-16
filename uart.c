#include <xc.h>
#include "uart.h"

#define TX_BUFFER_SIZE 8
#define RX_BUFFER_SIZE 64
#define RX_BUFFER_HIGHWATER 32
#define RX_BUFFER_LOWWATER  8

#if TX_BUFFER_SIZE > 0
static volatile char achTxBuffer[TX_BUFFER_SIZE];
static volatile unsigned char idxTxRead = 0, idxTxWrite = 0;
#endif

#if RX_BUFFER_SIZE > 0
static volatile char achRxBuffer[RX_BUFFER_SIZE];
static volatile unsigned char idxRxRead = 0, idxRxWrite = 0;
#endif

#define nDTR LATA3
#define nDSR PORTA2

void uart_block_sender(void)
{
    nDTR = 1;
}

void uart_unblock_sender(void)
{
    nDTR = 0;
}

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
    CREN  = 1;
    
    TRISC6 = 0;
    
    TRISA3 = 0; // DTR output
    TRISA2 = 1; // DSR input

#if RX_BUFFER_SIZE > 0
    RCIE = 1;
#endif
    
    PEIE = 1;

    uart_unblock_sender();
}

void uart_tx_isr(void)
{
#if TX_BUFFER_SIZE > 0
    TXREG = achTxBuffer[idxTxRead++];
    if (idxTxRead == TX_BUFFER_SIZE)
        idxTxRead = 0;
    
    if (idxTxRead == idxTxWrite)
        TXIE = 0;
#endif
}

#if RX_BUFFER_SIZE > 0
static unsigned char uart_rx_buffer_remaining(void)
{
    return (idxRxWrite > idxRxRead) ?
           (idxRxWrite - idxRxRead) :
           (RX_BUFFER_SIZE - idxRxRead + idxRxWrite);
}
#endif

void uart_rx_isr(void)
{
#if RX_BUFFER_SIZE > 0
    achRxBuffer[idxRxWrite++] = RCREG;
    if (idxRxWrite == RX_BUFFER_SIZE)
    {
        idxRxWrite = 0;
    }
    
    if (uart_rx_buffer_remaining() < RX_BUFFER_HIGHWATER)
    {
        uart_block_sender();
    }
#endif   
}

void putch(char c)
{
#if TX_BUFFER_SIZE > 0
    if (idxTxRead == idxTxWrite && TXIF)
    {
        TXREG = c;
        return;
    }
    
    achTxBuffer[idxTxWrite++] = c;
    if (idxTxWrite == TX_BUFFER_SIZE)
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
#if RX_BUFFER_SIZE > 0
    if (idxRxRead == idxRxWrite)
        return 0;
    
    char ch = achRxBuffer[idxRxRead++];
    
    if (idxRxRead == RX_BUFFER_SIZE)
        idxRxRead = 0;
    
    if (uart_rx_buffer_remaining() > RX_BUFFER_LOWWATER)
    {
        uart_unblock_sender();
    }

    return ch;
#else
    if (! RCIF)
        return 0;
    
    if (FERR)
        ;   // we dropped a byte, oops.
    
    return RCREG;
#endif
}