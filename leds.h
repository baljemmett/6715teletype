/* 
 * File:   leds.h
 * Author: baljemmett
 *
 * Created on 06 February 2016, 20:38
 */

#ifndef LEDS_H
#define	LEDS_H

#ifdef	__cplusplus
extern "C" {
#endif

#define LED1    LATA0
#define LED2    LATA1
    
#define leds_init()     \
{                       \
    TRISA0 = 0;         \
    TRISA1 = 0;         \
    LED1  = 0;          \
    LED2  = 0;          \
}


#ifdef	__cplusplus
}
#endif

#endif	/* LEDS_H */

