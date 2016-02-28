/* 
 * File:   timers.h
 * Author: baljemmett
 *
 * Created on 06 February 2016, 18:55
 */

#ifndef TIMERS_H
#define	TIMERS_H

#include <stdint.h>

#define _XTAL_FREQ 18432000

#define timers_block_ms(N) __delay_ms(N)

#ifdef	__cplusplus
extern "C" {
#endif

    extern void timers_init(void);
    extern void timers_isr(void);
    
    extern void timers_start_holdoff_ms(uint16_t cmsDelay);
    extern bit  timers_is_holdoff_running(void);
    
    extern void timers_start_blink_ms(uint16_t cmsDelay);
    extern bit  timers_is_blink_running(void);
    
    extern void timers_start_typematic_ms(uint16_t cmsDelay);
    extern void timers_stop_typematic(void);
    extern bit  timers_is_typematic_running(void);

#ifdef	__cplusplus
}
#endif

#endif	/* TIMERS_H */

