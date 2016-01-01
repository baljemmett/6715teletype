#include <xc.h>
#include "keyboard.h"

void keyboard_init(void)
{
    //
    //  First off, we're going to want to read the row strobes; since they're
    //  open collector lines, we'll also need to provide our own pullups so
    //  we can read them even if no keys are down...
    //
    TRISB  = 0xff;
    WPUB   = 0xff;
    nWPUEN = 0;
    
    //
    //  ... and we're also going to want to read the column outputs from the
    //  keyboard, which have their own pullups on the typewriter itself.
    //
    TRISD  = 0xff;
    TRISC |= 0x3e;
}

void keyboard_capture(scanstate *pScanstate)
{    
    char ints_enabled;

    //
    //  Wait for a row strobe, any row strobe.  (Otherwise we'll just sit in
    //  a loop with interrupts disabled nearly continuously, which is no good!)
    //
    while (PORTB == 0xff)
        ;
 
    //
    //  Now read the selected strobe and all column outputs, atomically ish.
    //
    ints_enabled = GIE;
    GIE = 0;
    
    pScanstate->row        = PORTB;
    pScanstate->columns[0] = PORTD;
    pScanstate->columns[1] = PORTC & 0x3e;
    
    GIE = ints_enabled;
}
