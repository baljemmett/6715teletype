#include <xc.h>
#include "timers.h"
#include "leds.h"

#define TMR0_PRESCALER    32
#define TMR0_RELOAD_VALUE (256 - (_XTAL_FREQ/(4 * TMR0_PRESCALER * 1000)))

#if TMR0_RELOAD_VALUE < 0
# error Crystal frequency does not allow 1ms with selected TMR0 prescaler value.
#endif

static volatile uint16_t g_cmsHoldoff = 0;

void timers_init(void)
{
    //
    //  Configure Timer0 as our 1ms tick counter.
    //
    //  With our 18.432MHz system oscillator, our execution clock is 4.608MHz;
    //  setting Timer1 to use a 32:1 prescaler gives us 144 counts per ms.
    //  However, the macros above should allow this to be recalculated at
    //  compile time; this comment is just an illustrated working...
    //
    //  (This assumes that the two instruction cycle delay after reloading TMR0
    //  won't cause a problem; with the 32:1 prescaler we can't correct for it
    //  anyway!)
    //
        
    PSA    = 0;
    
#if   TMR0_PRESCALER ==   1
    PSA    = 1;
#elif TMR0_PRESCALER ==   2
    PS2    = 0;    PS1    = 0;    PS0    = 0;
#elif TMR0_PRESCALER ==   4
    PS2    = 0;    PS1    = 0;    PS0    = 1;
#elif TMR0_PRESCALER ==   8
    PS2    = 0;    PS1    = 1;    PS0    = 0;
#elif TMR0_PRESCALER ==  16
    PS2    = 0;    PS1    = 1;    PS0    = 1;
#elif TMR0_PRESCALER ==  32
    PS2    = 1;    PS1    = 0;    PS0    = 0;
#elif TMR0_PRESCALER ==  64
    PS2    = 1;    PS1    = 0;    PS0    = 1;
#elif TMR0_PRESCALER == 128
    PS2    = 1;    PS1    = 1;    PS0    = 0;
#elif TMR0_PRESCALER == 256
    PS2    = 1;    PS1    = 1;    PS0    = 1;
#else
# error Unsupported TMR0 prescaler value - must be 2^[0..8]
#endif
    
    TMR0CS = 0;
    TMR0   = TMR0_RELOAD_VALUE;
    TMR0IE = 1;
}

void timers_isr(void)
{
    if (TMR0IF)
    {
        //
        //  Reload counter for 1ms tick...
        //
        TMR0  += TMR0_RELOAD_VALUE;
        TMR0IF = 0;
        
        //
        //  ... and handle any running timers
        //
        if (g_cmsHoldoff)
            --g_cmsHoldoff;
    }
}

void timers_start_holdoff_ms(uint16_t cmsDelay)
{
    g_cmsHoldoff += cmsDelay;
}

bit timers_is_holdoff_running(void)
{
    return g_cmsHoldoff > 0;
}