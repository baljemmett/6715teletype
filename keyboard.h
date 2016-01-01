/* 
 * File:   keyboard.h
 * Author: baljemmett
 *
 * Created on 28 December 2015, 15:13
 */

#ifndef KEYBOARD_H
#define	KEYBOARD_H

#ifdef	__cplusplus
extern "C" {
#endif

    typedef struct scanstate
    {
        unsigned char row;
        unsigned char columns[2];
    } scanstate;

    extern void keyboard_init(void);
    extern void keyboard_capture(scanstate *pScanstate);

#ifdef	__cplusplus
}
#endif

#endif	/* KEYBOARD_H */

