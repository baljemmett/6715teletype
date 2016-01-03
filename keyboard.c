#include <xc.h>
#include <stdint.h>
#include <stdio.h>
#include "keyboard.h"

#define _XTAL_FREQ 18432000
#define KEYSTROKE_GAP   20
#define KEYSTROKE_TICKS 30

//
//  Table of internal key IDs based on the order of the bits that represent
//  them in the raw scan data (bits 0->12 of rows 0->7).
//
static const keyid_t g_aKeyIDs[] = {
    /* row 0 */
    KEY_UNKNOWN, KEY_UNKNOWN, KEY_UNKNOWN, KEY_UNKNOWN,
    KEY_UNKNOWN, KEY_UNKNOWN, KEY_COLON, KEY_UNKNOWN,
    KEY_UNKNOWN, KEY_TCLR, KEY_UNKNOWN, KEY_G, KEY_H,
    
    /* row 1 */
    KEY_UNKNOWN, KEY_A, KEY_S, KEY_D,
    KEY_K, KEY_L, KEY_SEMICOLON, KEY_MAR_RTN,
    KEY_UNKNOWN, KEY_UNKNOWN, KEY_TSET, KEY_F, KEY_J,
    
    /* row 2 */
    KEY_UNKNOWN, KEY_CENTS, KEY_UNKNOWN, KEY_UNKNOWN,
    KEY_MU, KEY_UNKNOWN, KEY_DASH, KEY_BACKSPC,
    KEY_UNKNOWN, KEY_UNKNOWN, KEY_MAR_REL, KEY_5, KEY_6,
    
    /* row 3 */
    KEY_UNKNOWN, KEY_1, KEY_2, KEY_3,
    KEY_8, KEY_9, KEY_0, KEY_PAPER_UP,
    KEY_UNKNOWN, KEY_UNKNOWN, KEY_UNKNOWN, KEY_4, KEY_7,
    
    /* row 4 */
    KEY_UNKNOWN, KEY_Q, KEY_W, KEY_E,
    KEY_I, KEY_O, KEY_P, KEY_PAPER_DOWN,
    KEY_UNKNOWN, KEY_LMAR, KEY_TAB, KEY_R, KEY_U,
    
    /* row 5 */
    KEY_UNKNOWN, KEY_UNKNOWN, KEY_UNKNOWN, KEY_UNKNOWN,
    KEY_BRACKETS, KEY_UNKNOWN, KEY_AT, KEY_UNKNOWN,
    KEY_UNKNOWN, KEY_UNKNOWN, KEY_RMAR, KEY_T, KEY_Y,
    
    /* row 6 */
    KEY_UNKNOWN, KEY_Z, KEY_X, KEY_C,
    KEY_COMMA, KEY_FULLSTOP, KEY_INDICES, KEY_CRTN,
    KEY_UNKNOWN, KEY_REPEAT, KEY_LOCK, KEY_V, KEY_M,
    
    /* row 7 */
    KEY_SHIFT, KEY_ANGLES, KEY_UNKNOWN, KEY_UNKNOWN,
    KEY_UNKNOWN, KEY_UNKNOWN, KEY_SLASH, KEY_LINESPACE,
    KEY_CODE, KEY_SPACE, KEY_ERASE, KEY_B, KEY_N,  
};

typedef struct
{
    uint8_t row;
    uint8_t columns[2];
} keyscan_t;

static keyscan_t g_aKeyScans[KEY_MAX] = { 0 };

static void keyboard_init_scan_table(void)
{
    uint8_t nKeyCode = 0;
    
    for (uint8_t nRow = 0; nRow < 8; nRow++)
    {
        for (uint8_t nColumn = 0; nColumn < 13; nColumn++, nKeyCode++)
        {
            keyid_t nKey = g_aKeyIDs[nKeyCode];
            
            g_aKeyScans[nKey].row        = 0xff;
            g_aKeyScans[nKey].columns[0] = 0xff;
            g_aKeyScans[nKey].columns[1] = 0x3e;
                
            if (nKey != KEY_NONE && nKey != KEY_UNKNOWN && nKey < KEY_MAX)
            {
                g_aKeyScans[nKey].row &= ~(1<<nRow);
                
                if (nColumn < 8)
                {
                    g_aKeyScans[nKey].columns[0] &= ~(1<<nColumn);
                }
                else
                {
                    g_aKeyScans[nKey].columns[1] &= ~(1<<(nColumn-7));
                }
            }
        }
    }
}

//
//  The keyboard event queue contains one record for each key-down or key-up
//  event, containing the up/down event flag in the top bit and the internal
//  key ID code in the remainder.
//
#define EVENTQUEUE_LEN 16

static keyevent_t g_anEvents[EVENTQUEUE_LEN] = { KEY_NONE };
static uint8_t    g_idxEventRead  = 0;
static uint8_t    g_idxEventWrite = 0;

static void keyboard_queue_event(keyevent_t nEvent)
{
    g_anEvents[g_idxEventWrite++] = nEvent;
    
    if (g_idxEventWrite >= EVENTQUEUE_LEN)
        g_idxEventWrite = 0;
}

keyevent_t keyboard_get_next_event(void)
{
    keyevent_t nEvent = KEY_NONE;
    
    if (g_idxEventRead != g_idxEventWrite)
    {
        nEvent = g_anEvents[g_idxEventRead++];
        
        if (g_idxEventRead >= EVENTQUEUE_LEN)
        {
            g_idxEventRead = 0;
        }
    }
    
    return nEvent;
}

inline bit keyboard_is_down_event(const keyevent_t nEvent)
{
    return (nEvent & KEY_RELEASED) == 0x00;
}

inline keyid_t keyboard_get_event_key(const keyevent_t nEvent)
{
    return nEvent & ~KEY_RELEASED;
}

//
//  Find the lowest set bit in n; implemented using a lookup table rather than
//  the obvious loop over (1<<nBit) because I'm pretty sure free XC8 will do
//  something horrendous with that...
//
static const uint8_t g_anBits[8] =
    { 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80 };

static uint8_t lowest_bit(uint8_t n)
{   
    for (char nBit = 0; nBit < 8; nBit++)
        if (n & g_anBits[nBit])
            return nBit + 1;
    
    return 0;
}

//
//  The keyboard ISR comes in two halves; a fast half and a medium-speed half.
//  The medium-speed half is here, and handles reading the current state of
//  the keyboard in sync with the typewriter's scanning of it.  This is rather
//  time-critical, as we only get 70-ish instruction cycles before it moves on
//  to the next scan row...
//
//  To handle this properly, we use interrupt-on-falling-edge to trigger the
//  capture of the currently-scanned row into this data structure, which also
//  tracks which rows have been seen so far.  Then, in non-interrupt context,
//  a much slower routine runs each time we've seen every row; this routine
//  updates an internal map of the entire keyboard matrix, and generates
//  key-up/key-down events as appropriate.
//
static struct
{
    uint8_t pending;
    uint8_t scan_state[8][2];
} g_ISRdata;

//
//  The medium-speed ISR
//
void keyboard_isr(void)
{
    //
    //  We start on the critical path, so need to capture the state of the
    //  keyboard scanning pins before the typewriter yanks them from under us!
    //
    uint8_t row_pins = PORTB;
    uint8_t columns[2];
    
    columns[0] = PORTD;
    columns[1] = PORTC & 0x3e;

    //
    //  ... now we can relax and do things in a more leisurely fashion.
    //
    IOCIF = 0;
    
    if (row_pins == 0xff)
    {
        TRISD  = 0xff;
        TRISC |= 0x3e;
        IOCBF  = 0;
        return; // nothing to do, we were too late to see the strobe pins
    }
    
    //
    //  Only store the captured data in the ISR state if this row hasn't been
    //  seen already...
    //
    if (g_ISRdata.pending & ~row_pins)
    {
        uint8_t row = lowest_bit(~row_pins) - 1;
        g_ISRdata.scan_state[row][0] = ~columns[0];
        g_ISRdata.scan_state[row][1] = ~columns[1];
        g_ISRdata.pending &= row_pins;
    }
    
    IOCBF &= row_pins;
    
    if (PORTB == 0xff)
    {
        TRISD  = 0xff;
        TRISC |= 0x3e;
    }
}

//
//  These values are used by the fast ISR to inject keystrokes; ticks is the
//  number of scan cycles the keystroke should be injected for, row is the
//  raw value of the row strobe port corresponding to the keystroke, and col0/1
//  contain the same raw values as should be seen in the column outputs
//  (i.e. a 0 means the key is pressed, a 1 means the driver should be hi-Z)
//
//  N.B.: these MUST be in the 0xN70-0xN7f range so they can be accessed without
//  switching memory banks; this saves a few cycles in the ISR.
//
static volatile uint8_t g_inject_ticks @ 0x070;
static volatile uint8_t g_inject_row   @ 0x071;
static volatile uint8_t g_inject_col0  @ 0x072;
static volatile uint8_t g_inject_col1  @ 0x073;

//
//  The fast half of the keyboard ISR is here; it's placed as the main ISR
//  for the entire application, and calls back to the medium/slow ISR defined
//  in main.c.
//
extern void main_isr(void);

asm("FNCALL _main,_fast_isr");

void fast_isr(void) @ 0x0004
{
#asm
_asm
    PAGESEL $
    BTFSS   INTCON, 0               ; in core registers, no need to select bank
    GOTO    done                    ; bit 0 = IOCIF; have the scan pins changed?

    BANKSEL PORTB
    MOVF    PORTB, W
    XORWF    _g_inject_row, W       ; all in common RAM, no need to select bank
    BTFSS   STATUS, 2
    GOTO    not_inject_row
    
    BANKSEL TRISD                   ; inject the first low byte of column data
    MOVF    _g_inject_col0, W       ; by turning on the relevant pin drivers
    MOVWF   TRISD & 0x7f
            
    MOVLW   0xc1                    ; inject the high bits of column data,
    ANDWF   TRISC & 0x7f, W         ; without breaking any existing tristate
    IORWF   _g_inject_col1, W       ; settings on the same port...
    MOVWF   TRISC & 0x7f
            
;    FCALL   _keyboard_isr           ; call medium-latency keyboard ISR
    
    MOVF    _g_inject_ticks, W      ; if there are ticks left on the counter...
    BTFSS   STATUS, 2
    DECFSZ  _g_inject_ticks, F      ; decrement and clean up if now out of time
    GOTO    check_overrun           ; otherwise leave injection data in place
    
    MOVLW   0xff                    ; all columns to hi-Z...
    MOVWF   _g_inject_col0
    MOVLW   0x3e
    MOVWF   _g_inject_col1
    CLRF    _g_inject_row           ; and no rows matter
    GOTO    check_overrun
    
not_inject_row:
    BANKSEL TRISD                   ; this isn't our row, so all columns to hi-Z
    MOVLW   0xff
    MOVWF   TRISD & 0x7f
    MOVLW   0xbf                    ; TRISC6 is low (for UART Tx)
    MOVWF   TRISC & 0x7f
            
    FCALL _keyboard_isr             ; call medium-latency keyboard ISR

check_overrun:                      ; make sure we haven't taken too long
    BANKSEL PORTB
    INCF    PORTB, W                ; are any strobes active?
    BTFSS   STATUS, 2               
    GOTO    done                    ; yes, so don't worry

    BANKSEL TRISD                   ; no, so all columns to hi-Z
    MOVLW   0xff
    MOVWF   TRISD & 0x7f
    MOVLW   0xbf                    ; TRISC6 is low (for UART Tx)
    MOVWF   TRISC & 0x7f

done:
_endasm
#endasm
    
    main_isr();
    asm("RETFIE");
}

//
//  The non-interrupt-context routines to track the keyboard state and
//  generate key-up/key-down events.
//
static uint8_t g_aKeystates[8 * 13] = { 0 };

//
//  Given a row's worth of keyboard scan data, generate appropriate events.
//
static void keyboard_update_row_state(uint8_t row, uint8_t columns[2])
{
    keyid_t nKey = row * 13;
    uint8_t nBit;
    
    for (nBit = 0x01; nBit; nKey++, nBit += nBit)
    {
        if (columns[0] & nBit)
        {            
            //
            //  The key is down...
            //
            if (! g_aKeystates[nKey])
            {
                g_aKeystates[nKey] = 1;
                keyboard_queue_event(g_aKeyIDs[nKey]);
            }
        }
        else
        {
            //
            //  ... the key is up
            //
            if (g_aKeystates[nKey])
            {
                g_aKeystates[nKey] = 0;
                keyboard_queue_event(g_aKeyIDs[nKey] | KEY_RELEASED);
            }
        }
    }
    
    for (nBit = 0x02; nBit < 0x40; nKey++, nBit += nBit)
    {
        if (columns[1] & nBit)
        {
            //
            //  The key is down...
            //
            if (! g_aKeystates[nKey])
            {
                g_aKeystates[nKey] = 1;
                keyboard_queue_event(g_aKeyIDs[nKey]);
            }
        }
        else
        {
            //
            //  ... the key is up
            //
            if (g_aKeystates[nKey])
            {
                g_aKeystates[nKey] = 0;
                keyboard_queue_event(g_aKeyIDs[nKey] | KEY_RELEASED);
            }
        }
    }
}

//
//  The main routine to drive the keyboard event generation
//
void keyboard_update(void)
{
    //
    //  Early exit if we haven't seen scan data from every row yet.
    //
    if (g_ISRdata.pending)
        return;
    
    LATA0 = 1;
    
    //
    //  Now work through the accumulated scan data by rows, looking for changes;
    //  since we haven't yet reset the pending row flags, the ISR won't change
    //  any of the data out from under us.
    // 
    for (uint8_t nRow = 0; nRow < 8; nRow++)   
    {
        keyboard_update_row_state(nRow, g_ISRdata.scan_state[nRow]);
        
        g_ISRdata.scan_state[nRow][0] = 0xff;
        g_ISRdata.scan_state[nRow][1] = 0x3e;
    }
    
    LATA0 = 0;
    
    //
    //  Now allow the ISR to accumulate scan data once more.
    //
    g_ISRdata.pending = 0xff;
}

//
//  Initialize the keyboard driver
//
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
    LATD   = 0x00;
    LATC   = 0x00;
    
    //
    //  Get our key tables in order...
    //
    keyboard_init_scan_table();
    
    //
    //  We want an interrupt every time a pin goes low (-> a row is scanned)
    //
    IOCBN = 0xff;
    IOCBP = 0xff;
    
    //
    //  ... wait until we're in sync with the typewriter's scan sequence, by
    //  waiting for a scan pulse on the row strobes and then delaying long
    //  enough to land in the gap between scans.
    //
    while (PORTB == 0xff)
        ;
    
    __delay_ms(4);
    
    g_inject_ticks = 0;
    g_inject_row   = 0;
    g_inject_col0  = 0xff;
    g_inject_col1  = 0x3e;
    
    g_ISRdata.pending = 0xff;
    IOCIF = 0;
    IOCIE = 1;
}

static char keyboard_complete_scan_disable_interrupts()
{
    char enabled = GIE;
    
    if (enabled)
    {
        // wait for the next scan to complete, if running
        while (g_ISRdata.pending)
            ;
    }
    
    GIE = 0;
    return enabled;
}

static void keyboard_send_key(uint8_t row, uint8_t col0, uint8_t col1)
{
    char interrupts_enabled = keyboard_complete_scan_disable_interrupts();

    g_inject_row   = 0;     // row 0 never matches, so this disables injection
    g_inject_ticks = KEYSTROKE_TICKS; // until we're done setting up the values.
    g_inject_col0  = col0;
    g_inject_col1  = col1;
    
    do
    {
        // wait for scanning to be idle before selecting the target row
        while (PORTB == 0xff)
            ;   // now we're in a scan pulse...
        
        __delay_ms(4);  // ... so this should land us in the dead period
    }
    while (PORTB != 0xff);  // but make sure it has before continuing
    
    g_inject_row = row;
    
    GIE = interrupts_enabled;
}

static void keyboard_wait_sent(void)
{
    while (g_inject_ticks)
        ;
    
    __delay_ms(KEYSTROKE_GAP);
}

void keyboard_send_balj(void)
{
    keyboard_send_key(0x7f, 0xff, 0x2e);
    keyboard_wait_sent();
    keyboard_send_key(0xfd, 0xfd, 0x3e);
    keyboard_wait_sent();
    keyboard_send_key(0xfd, 0xdf, 0x3e);
    keyboard_wait_sent();
    keyboard_send_key(0xfd, 0xff, 0x1e);
    keyboard_wait_sent();
}

void keyboard_send_keystroke(keyid_t nKey)
{
    if (nKey >= KEY_MAX)
        return;
    
    uint8_t nRow = g_aKeyScans[nKey].row;
    
    if (nRow == 0 || nRow == 0xff)
        return;
    
    keyboard_send_key(nRow,
                      g_aKeyScans[nKey].columns[0],
                      g_aKeyScans[nKey].columns[1]);
    
    keyboard_wait_sent();
}