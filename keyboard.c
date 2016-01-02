#include <xc.h>
#include <stdint.h>
#include <stdio.h>
#include "keyboard.h"

#define _XTAL_FREQ 18432000

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
    return (nEvent & 0x80) == 0x00;
}

inline keyid_t keyboard_get_event_key(const keyevent_t nEvent)
{
    return nEvent & 0x7f;
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
        return; // nothing to do, we were too late to see the strobe pins
    
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
                keyboard_queue_event(g_aKeyIDs[nKey] | 0x80);
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
                keyboard_queue_event(g_aKeyIDs[nKey] | 0x80);
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
    //  we do this with interrupts disabled so nothing changes beneath us, but
    //  maybe that's a bit pointless?
    //
    char ints_enabled = GIE;
    GIE = 0;
    
    for (uint8_t nRow = 0; nRow < 8; nRow++)   
    {
        keyboard_update_row_state(nRow, g_ISRdata.scan_state[nRow]);
        
        g_ISRdata.scan_state[nRow][0] = 0xff;
        g_ISRdata.scan_state[nRow][1] = 0x3e;
    }
    
    LATA0 = 0;
    
    //
    //  Now allow the interrupt to accumulate scan data once more.
    //
    g_ISRdata.pending = 0xff;
    GIE = ints_enabled;
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
    
    //
    //  We want an interrupt every time a pin goes low (-> a row is scanned)
    //
    IOCBN = 0xff;
    
    //
    //  ... wait until we're in sync with the typewriter's scan sequence, by
    //  waiting for a scan pulse on the row strobes and then delaying long
    //  enough to land in the gap between scans.
    //
    while (PORTB == 0xff)
        ;
    
    __delay_ms(4);  
    
    g_ISRdata.pending = 0xff;
    IOCIF = 0;
    IOCIE = 1;
}
