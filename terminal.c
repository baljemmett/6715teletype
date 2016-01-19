#include <xc.h>
#include <stdio.h>
#include "terminal.h"
#include "keyboard.h"
#include "uart.h"

#define _XTAL_FREQ 18432000
#define RETURN_DELAY 1000

static const keyid_t g_aAsciiKeys[128] = {
    
    /* 00-03 */ KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, 
    /* 04-07 */ KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, 
    /* 08-0B */ KEY_BACKSPC, KEY_TAB, KEY_CRTN, KEY_NONE, 
    /* 0C-0F */ KEY_NONE, KEY_CRTN, KEY_NONE, KEY_NONE, 

    /* 10-13 */ KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, 
    /* 14-17 */ KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, 
    /* 18-1B */ KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, 
    /* 1C-1F */ KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, 

    /* 20-23 */ KEY_SPACE,              KEY_1 | KEY_SHIFTED,    KEY_2 | KEY_SHIFTED,        KEY_MU | KEY_SHIFTED, 
    /* 24-27 */ KEY_4 | KEY_SHIFTED,    KEY_5 | KEY_SHIFTED,    KEY_6 | KEY_SHIFTED,        KEY_7 | KEY_SHIFTED, 
    /* 28-2B */ KEY_8 | KEY_SHIFTED,    KEY_9 | KEY_SHIFTED,    KEY_COLON | KEY_SHIFTED,    KEY_SEMICOLON | KEY_SHIFTED, 
    /* 2C-2F */ KEY_COMMA,              KEY_DASH,               KEY_FULLSTOP,               KEY_SLASH, 

    /* 30-33 */ KEY_0,      KEY_1,                  KEY_2,                      KEY_3, 
    /* 34-37 */ KEY_4,      KEY_5,                  KEY_6,                      KEY_7, 
    /* 38-3B */ KEY_8,      KEY_9,                  KEY_COLON,                  KEY_SEMICOLON, 
    /* 3C-3F */ KEY_ANGLES, KEY_0 | KEY_SHIFTED,    KEY_ANGLES | KEY_SHIFTED,   KEY_SLASH | KEY_SHIFTED, 

    /* 40-43 */ KEY_AT, KEY_A | KEY_SHIFTED, KEY_B | KEY_SHIFTED, KEY_C | KEY_SHIFTED, 
    /* 44-47 */ KEY_D | KEY_SHIFTED,  KEY_E | KEY_SHIFTED, KEY_F | KEY_SHIFTED, KEY_G | KEY_SHIFTED, 
    /* 48-4B */ KEY_H | KEY_SHIFTED,  KEY_I | KEY_SHIFTED, KEY_J | KEY_SHIFTED, KEY_K | KEY_SHIFTED, 
    /* 4C-4F */ KEY_L | KEY_SHIFTED,  KEY_M | KEY_SHIFTED, KEY_N | KEY_SHIFTED, KEY_O | KEY_SHIFTED, 

    /* 50-53 */ KEY_P | KEY_SHIFTED, KEY_Q | KEY_SHIFTED, KEY_R | KEY_SHIFTED, KEY_S | KEY_SHIFTED, 
    /* 54-57 */ KEY_T | KEY_SHIFTED, KEY_U | KEY_SHIFTED, KEY_V | KEY_SHIFTED, KEY_W | KEY_SHIFTED, 
    /* 58-5B */ KEY_X | KEY_SHIFTED, KEY_Y | KEY_SHIFTED, KEY_Z | KEY_SHIFTED, KEY_BRACKETS | KEY_SHIFTED, 
    /* 5C-5F */ KEY_AT | KEY_SHIFTED, KEY_BRACKETS, KEY_CENTS | KEY_SHIFTED, KEY_DASH | KEY_SHIFTED, 

    /* 60-63 */ KEY_7 | KEY_SHIFTED, KEY_A, KEY_B, KEY_C, 
    /* 64-67 */ KEY_D, KEY_E, KEY_F, KEY_G, 
    /* 68-6B */ KEY_H, KEY_I, KEY_J, KEY_K, 
    /* 6C-6F */ KEY_L, KEY_M, KEY_N, KEY_O, 

    /* 70-73 */ KEY_P,  KEY_Q, KEY_R, KEY_S, 
    /* 74-77 */ KEY_T,  KEY_U, KEY_V, KEY_W, 
    /* 78-7B */ KEY_X,  KEY_Y, KEY_Z, KEY_BRACKETS | KEY_SHIFTED, 
    /* 7C-7F */ KEY_MU, KEY_BRACKETS, KEY_CENTS, KEY_ERASE, 

};

static char g_achKeys[KEY_MAX | KEY_SHIFTED] = { 0 };

static bit g_bIsLocked   = 0;
static bit g_bIsLockDown = 0;
static bit g_bIsShifted  = 0;
static bit g_bIsCode     = 0;

static uint8_t g_cchPosition = 0;
static bit g_bAutoReturn     = 0;
static uint8_t g_cchMargin   = 0;
static uint8_t g_cchPitch    = 0;

static void terminal_init_ascii_table(void)
{
    for (char ch = 0; ch < 128; ch++)
    {
        keyid_t nKey = g_aAsciiKeys[ch];
        
        if ((nKey & ~KEY_SHIFTED) < KEY_MAX)
        {
            if (g_achKeys[nKey] == 0)
                g_achKeys[nKey] = ch;
        }
    }
}

static void terminal_auto_return_toggled(void)
{
    g_bAutoReturn ^= 1;
}

static void terminal_pitch_cycled(void)
{
    switch (g_cchPitch)
    {
        case 10:
            g_cchPitch  = 12;
            g_cchMargin = 80 - g_cchPitch;
            break;
            
        case 12:
            g_cchPitch  = 15;
            g_cchMargin = 100 - g_cchPitch; 
            break;
            
        case 15:
        default:
            g_cchPitch  = 10;
            g_cchMargin = 67 - g_cchPitch;
            break;
    }
}

static void terminal_char_printed(uint8_t bCanBreak)
{
    if (g_cchPosition < 165)
    {
        g_cchPosition++;
    }

    //
    //  Will the typewriter have inserted an automatic return here?
    //
    if (bCanBreak && g_bAutoReturn && g_cchPosition > g_cchMargin)
    {
        __delay_ms(RETURN_DELAY);        
        g_cchPosition = 0;
    }
}

static void terminal_handle_motion(keyid_t nKey)
{
    switch (nKey)
    {
        case KEY_BACKSPC:
        case KEY_ERASE:
            if (g_cchPosition > 0)
            {
                g_cchPosition--;
            }
            break;
            
        case KEY_CRTN:
        case KEY_MAR_RTN:
            if (g_cchPosition)
            {
                __delay_ms(RETURN_DELAY);
            }
            
            g_cchPosition = 0;
            break;
            
        case KEY_SPACE:
        case KEY_TAB:
            terminal_char_printed(1);
            break;
            
        default:
            terminal_char_printed(0);
            break;
    }
}

static void terminal_inject_ascii(char ch)
{
    keyid_t nKey = (ch < 128) ? g_aAsciiKeys[ch] : KEY_NONE;
    
    if (nKey == KEY_NONE)
        return;
    
    if (nKey & KEY_SHIFTED)
    {
        if (g_bIsShifted || g_bIsLocked)
        {
            keyboard_send_keystroke(nKey & ~KEY_SHIFTED);
        }
        else
        {
            keyboard_send_keychord(KEY_SHIFT, nKey & ~KEY_SHIFTED);
        }
    }
    else
    {
        if (g_bIsLocked)
        {
            keyboard_send_keystroke(KEY_SHIFT);
            keyboard_send_keystroke(nKey);
            keyboard_send_keystroke(KEY_LOCK);
        }
        else if (g_bIsShifted)
        {
            //
            //  Problem - can't lop typist's finger off to remove shift state!
            //  For now, send as-is and whatever.  In future, maybe stop and
            //  flash attention light or something?
            //
            keyboard_send_keystroke(nKey);
        }
        else
        {
            keyboard_send_keystroke(nKey);
        }
    }
    
    terminal_handle_motion(nKey & ~KEY_SHIFTED);
}

static void terminal_keyevent(keyevent_t nEvent)
{
    keyid_t nKey = keyboard_get_event_key(nEvent);
    
    //
    //  Shifted state follows motion of Shift key exactly.
    //
    if (nKey == KEY_SHIFT)
    {
        g_bIsShifted = keyboard_is_down_event(nEvent);
        g_bIsLocked  = g_bIsShifted ? 0 : g_bIsLockDown;        
        return;
    }
    
    //
    //  Shifted state latches on as soon as the Lock key is pressed.
    //
    if (nKey == KEY_LOCK)
    {
        g_bIsLocked   = g_bIsShifted ? 0 : 1;
        g_bIsLockDown = keyboard_is_down_event(nEvent);
        return;
    }

    //
    //  Track the Code-shift state as well, following the Code key motion
    //
    if (nKey == KEY_CODE)
    {
        g_bIsCode = keyboard_is_down_event(nEvent);
        return;
    }
    
    //
    //  Handle or ignore any known Code-key combinations
    //
    if (g_bIsCode)
    {
        switch (nKey)
        {
            case KEY_P:
                terminal_pitch_cycled();
                return;
                
            case KEY_R:
                terminal_auto_return_toggled();
                return;
                
            case KEY_Q:
            case KEY_T:
            case KEY_U:
            case KEY_AT:
            case KEY_3:
            case KEY_6:
            case KEY_F:
            case KEY_J:
            case KEY_TAB:
            case KEY_COLON:
            case KEY_INDICES:
            case KEY_C:
            case KEY_BACKSPC:
                return;
        }
    }
    
    //
    //  Only key-down events can generate keystrokes at the moment
    //
    if (! keyboard_is_down_event(nEvent))
        return;
    
    //
    //  Drop invalid keys (just in case)
    //
    if (nKey == KEY_NONE || nKey == KEY_UNKNOWN || nKey >= KEY_MAX)
        return;
    
    //
    //  Set the shift bit if keyboard state requires...
    //
    if (g_bIsShifted || g_bIsLocked)
        nKey |= KEY_SHIFTED;
    
    //
    //  ... and spit out the keystroke if it maps to an ASCII character.
    //
    char ch = g_achKeys[nKey];
    
    if (ch != 0)
        putchar(ch);
    
    terminal_handle_motion(nKey & ~KEY_SHIFTED);
}

void terminal_init(void)
{
    terminal_init_ascii_table();
    terminal_pitch_cycled();
}

void terminal_process(void)
{
    keyevent_t nEvent;
    char       ch;
    
    while ((nEvent = keyboard_get_next_event()) != KEY_NONE)
    {
        terminal_keyevent(nEvent);
    }
    
    while ((ch = uart_get_rx_byte()) != 0)
    {
        static bit s_bSwallowLf = 0;
        
        if (! (ch == '\n' && s_bSwallowLf))
        {
            terminal_inject_ascii(ch);
        }
        
        s_bSwallowLf = (ch == '\r');
    }
}