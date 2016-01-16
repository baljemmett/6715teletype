/* 
 * File:   uart.h
 * Author: baljemmett
 *
 * Created on 25 December 2015, 22:49
 */

#ifndef UART_H
#define	UART_H

#ifdef	__cplusplus
extern "C" {
#endif

    extern void uart_init(void);
    extern void uart_tx_isr(void);
    extern void uart_rx_isr(void);
    extern char uart_get_rx_byte(void);
    extern void uart_block_sender(void);
    extern void uart_unblock_sender(void);


#ifdef	__cplusplus
}
#endif

#endif	/* UART_H */

