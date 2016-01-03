#include <stdio.h>
#include "terminal.h"
#include "keyboard.h"

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

static void terminal_inject_ascii(char ch)
{
    keyid_t nKey = (ch < 128) ? g_aAsciiKeys[ch] : KEY_NONE;
    
    if (nKey == KEY_NONE)
        return;
    
    if (nKey & KEY_SHIFTED)
    {
        keyboard_send_keystroke(KEY_LOCK);
        keyboard_send_keystroke(nKey & ~KEY_SHIFTED);
        keyboard_send_keystroke(KEY_SHIFT);
    }
    else
    {
        keyboard_send_keystroke(nKey);
    }
}

static void terminal_keyevent(keyevent_t nEvent)
{
    static bit s_bIsShifted = 0;
    static bit s_bIsCode    = 0;
    
    keyid_t nKey = keyboard_get_event_key(nEvent);
    
    //
    //  Shifted state follows motion of Shift key exactly.
    //
    if (nKey == KEY_SHIFT)
    {
        s_bIsShifted = keyboard_is_down_event(nEvent);
        return;
    }
    
    //
    //  Shifted state latches on as soon as the Lock key is pressed.
    //
    if (nKey == KEY_LOCK)
    {
        if (keyboard_is_down_event(nEvent))
            s_bIsShifted = 1;
        
        return;
    }

    //
    //  Track the Code-shift state as well, following the Code key motion
    //
    if (nKey == KEY_CODE)
    {
        s_bIsCode = keyboard_is_down_event(nEvent);
        return;
    }
    
    //
    //  Ignore all keys pressed when Code is down, and all releases (for now)
    //
    if (s_bIsCode || ! keyboard_is_down_event(nEvent))
        return;
    
    //
    //  Drop invalid keys (just in case)
    //
    if (nKey == KEY_NONE || nKey == KEY_UNKNOWN || nKey >= KEY_MAX)
        return;
    
    //
    //  Set the shift bit if keyboard state requires...
    //
    if (s_bIsShifted)
        nKey |= KEY_SHIFTED;
    
    //
    //  ... and spit out the keystroke if it maps to an ASCII character.
    //
    char ch = g_achKeys[nKey];
    
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
    
    while ((nEvent = keyboard_get_next_event()) != KEY_NONE)
    {
        terminal_keyevent(nEvent);
    }
}