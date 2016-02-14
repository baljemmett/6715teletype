#include <xc.h>
#include <stdio.h>
#include "terminal.h"
#include "keyboard.h"
#include "uart.h"
#include "timers.h"
#include "leds.h"

#define RETURN_DELAY 1000

//
//  'X-units per inch'; we use 120 because 10/12/15cpi all evenly divide it,
//  even for half-character widths (for the half-backspace key or centred text)
//
#define XPI                     120
#define POWERUP_CPI             10
#define POWERUP_LEFT_MARGIN     10
#define POWERUP_RIGHT_MARGIN    75
#define MARGIN_BELL_CHARS       8

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
static bit g_bCodePress  = 0;
static bit g_bSendCtrl   = 0;

static uint8_t  g_cxCharacter   =                            XPI  / POWERUP_CPI;
static uint16_t g_cxPosition    = (POWERUP_LEFT_MARGIN     * XPI) / POWERUP_CPI;
static uint16_t g_cxLeftMargin  = (POWERUP_LEFT_MARGIN     * XPI) / POWERUP_CPI;
static uint16_t g_cxRightMargin = (POWERUP_RIGHT_MARGIN    * XPI) / POWERUP_CPI;
static uint16_t g_cxBell        = ((POWERUP_RIGHT_MARGIN - MARGIN_BELL_CHARS)
                                                           * XPI) / POWERUP_CPI;
static bit      g_bAutoReturn   = 0;

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
    switch (g_cxCharacter)
    {
        case XPI / 10:
            g_cxCharacter  = XPI / 12;
            break;
            
        case XPI / 12:
            g_cxCharacter  = XPI / 15;
            break;
            
        case XPI / 15:
        default:
            g_cxCharacter  = XPI / 10;
            break;
    }
    
    g_cxBell = g_cxRightMargin - (MARGIN_BELL_CHARS * g_cxCharacter);
}

static void terminal_char_printed(uint8_t bCanBreak)
{
    if (g_cxPosition < (11 * XPI))
    {
        g_cxPosition += g_cxCharacter;
    }

    //
    //  Will the typewriter have inserted an automatic return here?
    //
    if (bCanBreak && g_bAutoReturn && g_cxPosition > g_cxBell)
    {
        timers_start_holdoff_ms(RETURN_DELAY);        
        g_cxPosition = g_cxLeftMargin;
    }
}

static void terminal_handle_motion(keyid_t nKey)
{
    switch (nKey)
    {
        case KEY_BACKSPC:
        case KEY_ERASE:
            if (g_cxPosition > g_cxLeftMargin)
            {
                g_cxPosition -= g_cxCharacter;
            }
            break;
            
        case KEY_CRTN:
        case KEY_MAR_RTN:
            if (g_cxPosition > g_cxLeftMargin)
            {
                timers_start_holdoff_ms(RETURN_DELAY);
            }
            
            g_cxPosition = g_cxLeftMargin;
            break;
            
        case KEY_MAR_REL:
        case KEY_LMAR:
        case KEY_RMAR:
        case KEY_TSET:
        case KEY_TCLR:
        case KEY_PAPER_UP:
        case KEY_PAPER_DOWN:
        case KEY_LINESPACE:
            break;
            
        case KEY_SPACE:
        case KEY_TAB:
        case KEY_DASH:
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
        if (keyboard_is_down_event(nEvent))
        {
            //
            //  Code went down, so start looking for any other keys...
            //
            g_bIsCode    = 1;
            g_bCodePress = 1;
        }
        else
        {
            //
            //  Code went back up...
            //
            g_bIsCode = 0;
            
            //
            //  ... and if no other keys were pressed, it's a Ctrl composition
            //
            if (g_bCodePress)
            {
                g_bCodePress = 0;
                g_bSendCtrl  = 1;
                LED1         = 1;
            }
        }
        
        return;
    }
    
    //
    //  Only key-down events can generate keystrokes at the moment
    //
    if (! keyboard_is_down_event(nEvent))
        return;
    
    //
    //  A non-shift/Code key was pressed, so clear the Code-pressed flag
    //
    g_bCodePress = 0;
    
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
    //  ... track the carriage position...
    //
    terminal_handle_motion(nKey & ~KEY_SHIFTED);

    //
    //  ... and spit out the keystroke if it maps to an ASCII character.
    //
    char ch = g_achKeys[nKey];
    
    if (g_bSendCtrl)
    {
        g_bSendCtrl = 0;
        LED1        = 0;
        
        if (ch >= 'A' && ch <= 'Z')
        {
            ch = (ch - 'A') + 1;
            
            keyboard_send_keystroke(KEY_BACKSPC);
            terminal_handle_motion(KEY_BACKSPC);
            
            keyboard_send_keychord(KEY_SHIFT, KEY_CENTS);
            terminal_handle_motion(KEY_CENTS);
        }
        else if (ch >= 'a' && ch <= 'z')
        {
            ch = (ch - 'a') + 1;
            
            keyboard_send_keystroke(KEY_BACKSPC);
            terminal_handle_motion(KEY_BACKSPC);
            
            keyboard_send_keychord(KEY_SHIFT, KEY_CENTS);
            terminal_handle_motion(KEY_CENTS);
        }
    }

    if (ch != 0)
        putchar(ch);
}

void terminal_init(void)
{
    terminal_init_ascii_table();
}

void terminal_process(void)
{
    keyevent_t nEvent;
    char       ch;
    
    while ((nEvent = keyboard_get_next_event()) != KEY_NONE)
    {
        terminal_keyevent(nEvent);
    }
    
    if (g_bSendCtrl || g_bIsCode || timers_is_holdoff_running())
    {
        //
        //  Don't attempt to process any input if we're Code-shifted, waiting
        //  for the second key of a Code, <key> combination, or in the waiting
        //  period between injected keystrokes watching for user interaction.
        //
        return;
    }
    
    if ((ch = uart_get_rx_byte()) != 0)
    {
        static bit s_bSwallowLf = 0;
        
        if (! (ch == '\n' && s_bSwallowLf))
        {
            terminal_inject_ascii(ch);
        }
        
        s_bSwallowLf = (ch == '\r');
    }
}