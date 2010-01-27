/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
 * Copyright (C) 2006 Michael Emmel mike.emmel@gmail.com
 * Copyright (C) 2007 Holger Hans Peter Freyther
 * Copyright (C) 2008 Collabora, Ltd.  All rights reserved.
 * Copyright (C) 2008, 2009 Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// windowsKeyCodeForKeyEvent is copied from platform/gtk/KeyEventGtk.cpp

#include "config.h"
#include "KeyCodeConversion.h"

#include "KeyboardCodes.h"

#include <gdk/gdkkeysyms.h>

namespace WebCore {

int windowsKeyCodeForKeyEvent(unsigned keycode)
{
    switch (keycode) {
    case GDK_KP_0:
        return VKEY_NUMPAD0; // (60) Numeric keypad 0 key
    case GDK_KP_1:
        return VKEY_NUMPAD1; // (61) Numeric keypad 1 key
    case GDK_KP_2:
        return  VKEY_NUMPAD2; // (62) Numeric keypad 2 key
    case GDK_KP_3:
        return VKEY_NUMPAD3; // (63) Numeric keypad 3 key
    case GDK_KP_4:
        return VKEY_NUMPAD4; // (64) Numeric keypad 4 key
    case GDK_KP_5:
        return VKEY_NUMPAD5; //(65) Numeric keypad 5 key
    case GDK_KP_6:
        return VKEY_NUMPAD6; // (66) Numeric keypad 6 key
    case GDK_KP_7:
        return VKEY_NUMPAD7; // (67) Numeric keypad 7 key
    case GDK_KP_8:
        return VKEY_NUMPAD8; // (68) Numeric keypad 8 key
    case GDK_KP_9:
        return VKEY_NUMPAD9; // (69) Numeric keypad 9 key
    case GDK_KP_Multiply:
        return VKEY_MULTIPLY; // (6A) Multiply key
    case GDK_KP_Add:
        return VKEY_ADD; // (6B) Add key
    case GDK_KP_Subtract:
        return VKEY_SUBTRACT; // (6D) Subtract key
    case GDK_KP_Decimal:
        return VKEY_DECIMAL; // (6E) Decimal key
    case GDK_KP_Divide:
        return VKEY_DIVIDE; // (6F) Divide key

    case GDK_KP_Page_Up:
        return VKEY_PRIOR; // (21) PAGE UP key
    case GDK_KP_Page_Down:
        return VKEY_NEXT; // (22) PAGE DOWN key
    case GDK_KP_End:
        return VKEY_END; // (23) END key
    case GDK_KP_Home:
        return VKEY_HOME; // (24) HOME key
    case GDK_KP_Left:
        return VKEY_LEFT; // (25) LEFT ARROW key
    case GDK_KP_Up:
        return VKEY_UP; // (26) UP ARROW key
    case GDK_KP_Right:
        return VKEY_RIGHT; // (27) RIGHT ARROW key
    case GDK_KP_Down:
        return VKEY_DOWN; // (28) DOWN ARROW key

    case GDK_BackSpace:
        return VKEY_BACK; // (08) BACKSPACE key
    case GDK_ISO_Left_Tab:
    case GDK_3270_BackTab:
    case GDK_Tab:
        return VKEY_TAB; // (09) TAB key
    case GDK_Clear:
        return VKEY_CLEAR; // (0C) CLEAR key
    case GDK_ISO_Enter:
    case GDK_KP_Enter:
    case GDK_Return:
        return VKEY_RETURN; //(0D) Return key
    case GDK_Shift_L:
    case GDK_Shift_R:
        return VKEY_SHIFT; // (10) SHIFT key
    case GDK_Control_L:
    case GDK_Control_R:
        return VKEY_CONTROL; // (11) CTRL key
    case GDK_Menu:
        return VKEY_APPS;  // (5D) Applications key (Natural keyboard)
    case GDK_Alt_L:
    case GDK_Alt_R:
        return VKEY_MENU; // (12) ALT key

    case GDK_Pause:
        return VKEY_PAUSE; // (13) PAUSE key
    case GDK_Caps_Lock:
        return VKEY_CAPITAL; // (14) CAPS LOCK key
    case GDK_Kana_Lock:
    case GDK_Kana_Shift:
        return VKEY_KANA; // (15) Input Method Editor (IME) Kana mode
    case GDK_Hangul:
        return VKEY_HANGUL; // VKEY_HANGUL (15) IME Hangul mode
        // VKEY_JUNJA (17) IME Junja mode
        // VKEY_FINAL (18) IME final mode
    case GDK_Hangul_Hanja:
        return VKEY_HANJA; // (19) IME Hanja mode
    case GDK_Kanji:
        return VKEY_KANJI; // (19) IME Kanji mode
    case GDK_Escape:
        return VKEY_ESCAPE; // (1B) ESC key
        // VKEY_CONVERT (1C) IME convert
        // VKEY_NONCONVERT (1D) IME nonconvert
        // VKEY_ACCEPT (1E) IME accept
        // VKEY_MODECHANGE (1F) IME mode change request
    case GDK_space:
        return VKEY_SPACE; // (20) SPACEBAR
    case GDK_Page_Up:
        return VKEY_PRIOR; // (21) PAGE UP key
    case GDK_Page_Down:
        return VKEY_NEXT; // (22) PAGE DOWN key
    case GDK_End:
        return VKEY_END; // (23) END key
    case GDK_Home:
        return VKEY_HOME; // (24) HOME key
    case GDK_Left:
        return VKEY_LEFT; // (25) LEFT ARROW key
    case GDK_Up:
        return VKEY_UP; // (26) UP ARROW key
    case GDK_Right:
        return VKEY_RIGHT; // (27) RIGHT ARROW key
    case GDK_Down:
        return VKEY_DOWN; // (28) DOWN ARROW key
    case GDK_Select:
        return VKEY_SELECT; // (29) SELECT key
    case GDK_Print:
        return VKEY_PRINT; // (2A) PRINT key
    case GDK_Execute:
        return VKEY_EXECUTE;// (2B) EXECUTE key
        //dunno on this
        //case GDK_PrintScreen:
        //      return VKEY_SNAPSHOT; // (2C) PRINT SCREEN key
    case GDK_Insert:
        return VKEY_INSERT; // (2D) INS key
    case GDK_Delete:
        return VKEY_DELETE; // (2E) DEL key
    case GDK_Help:
        return VKEY_HELP; // (2F) HELP key
    case GDK_0:
    case GDK_parenright:
        return VKEY_0;    //  (30) 0) key
    case GDK_1:
    case GDK_exclam:
        return VKEY_1; //  (31) 1 ! key
    case GDK_2:
    case GDK_at:
        return VKEY_2; //  (32) 2 & key
    case GDK_3:
    case GDK_numbersign:
        return VKEY_3; //case '3': case '#';
    case GDK_4:
    case GDK_dollar: //  (34) 4 key '$';
        return VKEY_4;
    case GDK_5:
    case GDK_percent:
        return VKEY_5; //  (35) 5 key  '%'
    case GDK_6:
    case GDK_asciicircum:
        return VKEY_6; //  (36) 6 key  '^'
    case GDK_7:
    case GDK_ampersand:
        return VKEY_7; //  (37) 7 key  case '&'
    case GDK_8:
    case GDK_asterisk:
        return VKEY_8; //  (38) 8 key  '*'
    case GDK_9:
    case GDK_parenleft:
        return VKEY_9; //  (39) 9 key '('
    case GDK_a:
    case GDK_A:
        return VKEY_A; //  (41) A key case 'a': case 'A': return 0x41;
    case GDK_b:
    case GDK_B:
        return VKEY_B; //  (42) B key case 'b': case 'B': return 0x42;
    case GDK_c:
    case GDK_C:
        return VKEY_C; //  (43) C key case 'c': case 'C': return 0x43;
    case GDK_d:
    case GDK_D:
        return VKEY_D; //  (44) D key case 'd': case 'D': return 0x44;
    case GDK_e:
    case GDK_E:
        return VKEY_E; //  (45) E key case 'e': case 'E': return 0x45;
    case GDK_f:
    case GDK_F:
        return VKEY_F; //  (46) F key case 'f': case 'F': return 0x46;
    case GDK_g:
    case GDK_G:
        return VKEY_G; //  (47) G key case 'g': case 'G': return 0x47;
    case GDK_h:
    case GDK_H:
        return VKEY_H; //  (48) H key case 'h': case 'H': return 0x48;
    case GDK_i:
    case GDK_I:
        return VKEY_I; //  (49) I key case 'i': case 'I': return 0x49;
    case GDK_j:
    case GDK_J:
        return VKEY_J; //  (4A) J key case 'j': case 'J': return 0x4A;
    case GDK_k:
    case GDK_K:
        return VKEY_K; //  (4B) K key case 'k': case 'K': return 0x4B;
    case GDK_l:
    case GDK_L:
        return VKEY_L; //  (4C) L key case 'l': case 'L': return 0x4C;
    case GDK_m:
    case GDK_M:
        return VKEY_M; //  (4D) M key case 'm': case 'M': return 0x4D;
    case GDK_n:
    case GDK_N:
        return VKEY_N; //  (4E) N key case 'n': case 'N': return 0x4E;
    case GDK_o:
    case GDK_O:
        return VKEY_O; //  (4F) O key case 'o': case 'O': return 0x4F;
    case GDK_p:
    case GDK_P:
        return VKEY_P; //  (50) P key case 'p': case 'P': return 0x50;
    case GDK_q:
    case GDK_Q:
        return VKEY_Q; //  (51) Q key case 'q': case 'Q': return 0x51;
    case GDK_r:
    case GDK_R:
        return VKEY_R; //  (52) R key case 'r': case 'R': return 0x52;
    case GDK_s:
    case GDK_S:
        return VKEY_S; //  (53) S key case 's': case 'S': return 0x53;
    case GDK_t:
    case GDK_T:
        return VKEY_T; //  (54) T key case 't': case 'T': return 0x54;
    case GDK_u:
    case GDK_U:
        return VKEY_U; //  (55) U key case 'u': case 'U': return 0x55;
    case GDK_v:
    case GDK_V:
        return VKEY_V; //  (56) V key case 'v': case 'V': return 0x56;
    case GDK_w:
    case GDK_W:
        return VKEY_W; //  (57) W key case 'w': case 'W': return 0x57;
    case GDK_x:
    case GDK_X:
        return VKEY_X; //  (58) X key case 'x': case 'X': return 0x58;
    case GDK_y:
    case GDK_Y:
        return VKEY_Y; //  (59) Y key case 'y': case 'Y': return 0x59;
    case GDK_z:
    case GDK_Z:
        return VKEY_Z; //  (5A) Z key case 'z': case 'Z': return 0x5A;
    case GDK_Meta_L:
        return VKEY_LWIN; // (5B) Left Windows key (Microsoft Natural keyboard)
    case GDK_Meta_R:
        return VKEY_RWIN; // (5C) Right Windows key (Natural keyboard)
        // VKEY_SLEEP (5F) Computer Sleep key
        // VKEY_SEPARATOR (6C) Separator key
        // VKEY_SUBTRACT (6D) Subtract key
        // VKEY_DECIMAL (6E) Decimal key
        // VKEY_DIVIDE (6F) Divide key
        // handled by key code above

    case GDK_Num_Lock:
        return VKEY_NUMLOCK; // (90) NUM LOCK key

    case GDK_Scroll_Lock:
        return VKEY_SCROLL; // (91) SCROLL LOCK key

        // VKEY_LSHIFT (A0) Left SHIFT key
        // VKEY_RSHIFT (A1) Right SHIFT key
        // VKEY_LCONTROL (A2) Left CONTROL key
        // VKEY_RCONTROL (A3) Right CONTROL key
        // VKEY_LMENU (A4) Left MENU key
        // VKEY_RMENU (A5) Right MENU key
        // VKEY_BROWSER_BACK (A6) Windows 2000/XP: Browser Back key
        // VKEY_BROWSER_FORWARD (A7) Windows 2000/XP: Browser Forward key
        // VKEY_BROWSER_REFRESH (A8) Windows 2000/XP: Browser Refresh key
        // VKEY_BROWSER_STOP (A9) Windows 2000/XP: Browser Stop key
        // VKEY_BROWSER_SEARCH (AA) Windows 2000/XP: Browser Search key
        // VKEY_BROWSER_FAVORITES (AB) Windows 2000/XP: Browser Favorites key
        // VKEY_BROWSER_HOME (AC) Windows 2000/XP: Browser Start and Home key
        // VKEY_VOLUME_MUTE (AD) Windows 2000/XP: Volume Mute key
        // VKEY_VOLUME_DOWN (AE) Windows 2000/XP: Volume Down key
        // VKEY_VOLUME_UP (AF) Windows 2000/XP: Volume Up key
        // VKEY_MEDIA_NEXT_TRACK (B0) Windows 2000/XP: Next Track key
        // VKEY_MEDIA_PREV_TRACK (B1) Windows 2000/XP: Previous Track key
        // VKEY_MEDIA_STOP (B2) Windows 2000/XP: Stop Media key
        // VKEY_MEDIA_PLAY_PAUSE (B3) Windows 2000/XP: Play/Pause Media key
        // VKEY_LAUNCH_MAIL (B4) Windows 2000/XP: Start Mail key
        // VKEY_LAUNCH_MEDIA_SELECT (B5) Windows 2000/XP: Select Media key
        // VKEY_LAUNCH_APP1 (B6) Windows 2000/XP: Start Application 1 key
        // VKEY_LAUNCH_APP2 (B7) Windows 2000/XP: Start Application 2 key

        // VKEY_OEM_1 (BA) Used for miscellaneous characters; it can vary by keyboard. Windows 2000/XP: For the US standard keyboard, the ';:' key
    case GDK_semicolon:
    case GDK_colon:
        return VKEY_OEM_1; //case ';': case ':': return 0xBA;
        // VKEY_OEM_PLUS (BB) Windows 2000/XP: For any country/region, the '+' key
    case GDK_plus:
    case GDK_equal:
        return VKEY_OEM_PLUS; //case '=': case '+': return 0xBB;
        // VKEY_OEM_COMMA (BC) Windows 2000/XP: For any country/region, the ',' key
    case GDK_comma:
    case GDK_less:
        return VKEY_OEM_COMMA; //case ',': case '<': return 0xBC;
        // VKEY_OEM_MINUS (BD) Windows 2000/XP: For any country/region, the '-' key
    case GDK_minus:
    case GDK_underscore:
        return VKEY_OEM_MINUS; //case '-': case '_': return 0xBD;
        // VKEY_OEM_PERIOD (BE) Windows 2000/XP: For any country/region, the '.' key
    case GDK_period:
    case GDK_greater:
        return VKEY_OEM_PERIOD; //case '.': case '>': return 0xBE;
        // VKEY_OEM_2 (BF) Used for miscellaneous characters; it can vary by keyboard. Windows 2000/XP: For the US standard keyboard, the '/?' key
    case GDK_slash:
    case GDK_question:
        return VKEY_OEM_2; //case '/': case '?': return 0xBF;
        // VKEY_OEM_3 (C0) Used for miscellaneous characters; it can vary by keyboard. Windows 2000/XP: For the US standard keyboard, the '`~' key
    case GDK_asciitilde:
    case GDK_quoteleft:
        return VKEY_OEM_3; //case '`': case '~': return 0xC0;
        // VKEY_OEM_4 (DB) Used for miscellaneous characters; it can vary by keyboard. Windows 2000/XP: For the US standard keyboard, the '[{' key
    case GDK_bracketleft:
    case GDK_braceleft:
        return VKEY_OEM_4; //case '[': case '{': return 0xDB;
        // VKEY_OEM_5 (DC) Used for miscellaneous characters; it can vary by keyboard. Windows 2000/XP: For the US standard keyboard, the '\|' key
    case GDK_backslash:
    case GDK_bar:
        return VKEY_OEM_5; //case '\\': case '|': return 0xDC;
        // VKEY_OEM_6 (DD) Used for miscellaneous characters; it can vary by keyboard. Windows 2000/XP: For the US standard keyboard, the ']}' key
    case GDK_bracketright:
    case GDK_braceright:
        return VKEY_OEM_6; // case ']': case '}': return 0xDD;
        // VKEY_OEM_7 (DE) Used for miscellaneous characters; it can vary by keyboard. Windows 2000/XP: For the US standard keyboard, the 'single-quote/double-quote' key
    case GDK_quoteright:
    case GDK_quotedbl:
        return VKEY_OEM_7; // case '\'': case '"': return 0xDE;
        // VKEY_OEM_8 (DF) Used for miscellaneous characters; it can vary by keyboard.
        // VKEY_OEM_102 (E2) Windows 2000/XP: Either the angle bracket key or the backslash key on the RT 102-key keyboard
        // VKEY_PROCESSKEY (E5) Windows 95/98/Me, Windows NT 4.0, Windows 2000/XP: IME PROCESS key
        // VKEY_PACKET (E7) Windows 2000/XP: Used to pass Unicode characters as if they were keystrokes. The VKEY_PACKET key is the low word of a 32-bit Virtual Key value used for non-keyboard input methods. For more information, see Remark in KEYBDINPUT,SendInput, WM_KEYDOWN, and WM_KEYUP
        // VKEY_ATTN (F6) Attn key
        // VKEY_CRSEL (F7) CrSel key
        // VKEY_EXSEL (F8) ExSel key
        // VKEY_EREOF (F9) Erase EOF key
        // VKEY_PLAY (FA) Play key
        // VKEY_ZOOM (FB) Zoom key
        // VKEY_NONAME (FC) Reserved for future use
        // VKEY_PA1 (FD) PA1 key
        // VKEY_OEM_CLEAR (FE) Clear key
    case GDK_F1:
    case GDK_F2:
    case GDK_F3:
    case GDK_F4:
    case GDK_F5:
    case GDK_F6:
    case GDK_F7:
    case GDK_F8:
    case GDK_F9:
    case GDK_F10:
    case GDK_F11:
    case GDK_F12:
    case GDK_F13:
    case GDK_F14:
    case GDK_F15:
    case GDK_F16:
    case GDK_F17:
    case GDK_F18:
    case GDK_F19:
    case GDK_F20:
    case GDK_F21:
    case GDK_F22:
    case GDK_F23:
    case GDK_F24:
        return VKEY_F1 + (keycode - GDK_F1);
    default:
        return 0;
    }
}

} // namespace WebCore
