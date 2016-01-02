/* 
 * File:   keyboard.h
 * Author: baljemmett
 *
 * Created on 28 December 2015, 15:13
 */

#ifndef KEYBOARD_H
#define	KEYBOARD_H

#include <stdint.h>

#ifdef	__cplusplus
extern "C" {
#endif

    extern void keyboard_init(void);
    extern void keyboard_isr(void);
    extern void keyboard_update(void);
    
    typedef enum
    {
        KEY_NONE = 0,
        KEY_UNKNOWN,
        
        KEY_MAR_REL,
        KEY_CENTS,
        KEY_1,
        KEY_2,
        KEY_3,
        KEY_4,
        KEY_5,
        KEY_6,
        KEY_7,
        KEY_8,
        KEY_9,
        KEY_0,
        KEY_DASH,
        KEY_MU,
        KEY_BACKSPC,
        KEY_PAPER_UP,

        KEY_LMAR,
        KEY_TAB,
        KEY_Q,
        KEY_W,
        KEY_E,
        KEY_R,
        KEY_T,
        KEY_Y,
        KEY_U,
        KEY_I,
        KEY_O,
        KEY_P,
        KEY_AT,
        KEY_BRACKETS,
        KEY_CRTN,
        KEY_PAPER_DOWN,

        KEY_RMAR,
        KEY_LOCK,
        KEY_A,
        KEY_S,
        KEY_D,
        KEY_F,
        KEY_G,
        KEY_H,
        KEY_J,
        KEY_K,
        KEY_L,
        KEY_SEMICOLON,
        KEY_COLON,
        KEY_INDICES,
        KEY_MAR_RTN,

        KEY_TSET,
        KEY_SHIFT,
        KEY_ANGLES,
        KEY_Z,
        KEY_X,
        KEY_C,
        KEY_V,
        KEY_B,
        KEY_N,
        KEY_M,
        KEY_COMMA,
        KEY_FULLSTOP,
        KEY_SLASH,
        KEY_REPEAT,

        KEY_TCLR,
        KEY_CODE,
        KEY_SPACE,
        KEY_ERASE,
        KEY_LINESPACE,
                
        KEY_MAX,
               
    } keyid_t;
    
#if KEY_MAX > 127
# error Too many keys defined in keyid_t enum!
#endif
    
    typedef uint8_t keyevent_t;
    
    extern keyevent_t keyboard_get_next_event(void);
    extern inline bit keyboard_is_down_event(const keyevent_t nEvent);
    extern inline keyid_t keyboard_get_event_key(const keyevent_t nEvent);
    
    extern void keyboard_send_balj(void);

#ifdef	__cplusplus
}
#endif

#endif	/* KEYBOARD_H */

