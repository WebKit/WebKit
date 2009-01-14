/*
 * Copyright (C) 2006 Michael Emmel mike.emmel@gmail.com. All rights reserved.
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
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES, LOSS OF USE, DATA, OR
 * PROFITS, OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef KeyboardCodesPosix_h
#define KeyboardCodesPosix_h

namespace WebCore {

    enum {
        // VKEY_LBUTTON (01) Left mouse button
        // VKEY_RBUTTON (02) Right mouse button
        // VKEY_CANCEL (03) Control-break processing
        // VKEY_MBUTTON (04) Middle mouse button (three-button mouse)
        // VKEY_XBUTTON1 (05)
        // VKEY_XBUTTON2 (06)

        // VKEY_BACK (08) BACKSPACE key
        VKEY_BACK = 0x08,

        // VKEY_TAB (09) TAB key
        VKEY_TAB = 0x09,

        // VKEY_CLEAR (0C) CLEAR key
        VKEY_CLEAR = 0x0C,

        // VKEY_RETURN (0D)
        VKEY_RETURN = 0x0D,

        // VKEY_SHIFT (10) SHIFT key
        VKEY_SHIFT = 0x10,

        // VKEY_CONTROL (11) CTRL key
        VKEY_CONTROL = 0x11,

        // VKEY_MENU (12) ALT key
        VKEY_MENU = 0x12,

        // VKEY_PAUSE (13) PAUSE key
        VKEY_PAUSE = 0x13,

        // VKEY_CAPITAL (14) CAPS LOCK key
        VKEY_CAPITAL = 0x14,

        // VKEY_KANA (15) Input Method Editor (IME) Kana mode
        VKEY_KANA = 0x15,

        // VKEY_HANGUEL (15) IME Hanguel mode (maintained for compatibility, use VKEY_HANGUL)
        // VKEY_HANGUL (15) IME Hangul mode
        VKEY_HANGUL = 0x15,

        // VKEY_JUNJA (17) IME Junja mode
        VKEY_JUNJA = 0x17,

        // VKEY_FINAL (18) IME final mode
        VKEY_FINAL = 0x18,

        // VKEY_HANJA (19) IME Hanja mode
        VKEY_HANJA = 0x19,

        // VKEY_KANJI (19) IME Kanji mode
        VKEY_KANJI = 0x19,

        // VKEY_ESCAPE (1B) ESC key
        VKEY_ESCAPE = 0x1B,

        // VKEY_CONVERT (1C) IME convert
        VKEY_CONVERT = 0x1C,

        // VKEY_NONCONVERT (1D) IME nonconvert
        VKEY_NONCONVERT = 0x1D,

        // VKEY_ACCEPT (1E) IME accept
        VKEY_ACCEPT = 0x1E,

        // VKEY_MODECHANGE (1F) IME mode change request
        VKEY_MODECHANGE = 0x1F,

        // VKEY_SPACE (20) SPACEBAR
        VKEY_SPACE = 0x20,

        // VKEY_PRIOR (21) PAGE UP key
        VKEY_PRIOR = 0x21,

        // VKEY_NEXT (22) PAGE DOWN key
        VKEY_NEXT = 0x22,

        // VKEY_END (23) END key
        VKEY_END = 0x23,

        // VKEY_HOME (24) HOME key
        VKEY_HOME = 0x24,

        // VKEY_LEFT (25) LEFT ARROW key
        VKEY_LEFT = 0x25,

        // VKEY_UP (26) UP ARROW key
        VKEY_UP = 0x26,

        // VKEY_RIGHT (27) RIGHT ARROW key
        VKEY_RIGHT = 0x27,

        // VKEY_DOWN (28) DOWN ARROW key
        VKEY_DOWN = 0x28,

        // VKEY_SELECT (29) SELECT key
        VKEY_SELECT = 0x29,

        // VKEY_PRINT (2A) PRINT key
        VKEY_PRINT = 0x2A,

        // VKEY_EXECUTE (2B) EXECUTE key
        VKEY_EXECUTE = 0x2B,

        // VKEY_SNAPSHOT (2C) PRINT SCREEN key
        VKEY_SNAPSHOT = 0x2C,

        // VKEY_INSERT (2D) INS key
        VKEY_INSERT = 0x2D,

        // VKEY_DELETE (2E) DEL key
        VKEY_DELETE = 0x2E,

        // VKEY_HELP (2F) HELP key
        VKEY_HELP = 0x2F,

        // (30) 0 key
        VKEY_0 = 0x30,

        // (31) 1 key
        VKEY_1 = 0x31,

        // (32) 2 key
        VKEY_2 = 0x32,

        // (33) 3 key
        VKEY_3 = 0x33,

        // (34) 4 key
        VKEY_4 = 0x34,

        // (35) 5 key,

        VKEY_5 = 0x35,

        // (36) 6 key
        VKEY_6 = 0x36,

        // (37) 7 key
        VKEY_7 = 0x37,

        // (38) 8 key
        VKEY_8 = 0x38,

        // (39) 9 key
        VKEY_9 = 0x39,

        // (41) A key
        VKEY_A = 0x41,

        // (42) B key
        VKEY_B = 0x42,

        // (43) C key
        VKEY_C = 0x43,

        // (44) D key
        VKEY_D = 0x44,

        // (45) E key
        VKEY_E = 0x45,

        // (46) F key
        VKEY_F = 0x46,

        // (47) G key
        VKEY_G = 0x47,

        // (48) H key
        VKEY_H = 0x48,

        // (49) I key
        VKEY_I = 0x49,

        // (4A) J key
        VKEY_J = 0x4A,

        // (4B) K key
        VKEY_K = 0x4B,

        // (4C) L key
        VKEY_L = 0x4C,

        // (4D) M key
        VKEY_M = 0x4D,

        // (4E) N key
        VKEY_N = 0x4E,

        // (4F) O key
        VKEY_O = 0x4F,

        // (50) P key
        VKEY_P = 0x50,

        // (51) Q key
        VKEY_Q = 0x51,

        // (52) R key
        VKEY_R = 0x52,

        // (53) S key
        VKEY_S = 0x53,

        // (54) T key
        VKEY_T = 0x54,

        // (55) U key
        VKEY_U = 0x55,

        // (56) V key
        VKEY_V = 0x56,

        // (57) W key
        VKEY_W = 0x57,

        // (58) X key
        VKEY_X = 0x58,

        // (59) Y key
        VKEY_Y = 0x59,

        // (5A) Z key
        VKEY_Z = 0x5A,

        // VKEY_LWIN (5B) Left Windows key (Microsoft Natural keyboard)
        VKEY_LWIN = 0x5B,

        // VKEY_RWIN (5C) Right Windows key (Natural keyboard)
        VKEY_RWIN = 0x5C,

        // VKEY_APPS (5D) Applications key (Natural keyboard)
        VKEY_APPS = 0x5D,

        // VKEY_SLEEP (5F) Computer Sleep key
        VKEY_SLEEP = 0x5F,

        // VKEY_NUMPAD0 (60) Numeric keypad 0 key
        VKEY_NUMPAD0 = 0x60,

        // VKEY_NUMPAD1 (61) Numeric keypad 1 key
        VKEY_NUMPAD1 = 0x61,

        // VKEY_NUMPAD2 (62) Numeric keypad 2 key
        VKEY_NUMPAD2 = 0x62,

        // VKEY_NUMPAD3 (63) Numeric keypad 3 key
        VKEY_NUMPAD3 = 0x63,

        // VKEY_NUMPAD4 (64) Numeric keypad 4 key
        VKEY_NUMPAD4 = 0x64,

        // VKEY_NUMPAD5 (65) Numeric keypad 5 key
        VKEY_NUMPAD5 = 0x65,

        // VKEY_NUMPAD6 (66) Numeric keypad 6 key
        VKEY_NUMPAD6 = 0x66,

        // VKEY_NUMPAD7 (67) Numeric keypad 7 key
        VKEY_NUMPAD7 = 0x67,

        // VKEY_NUMPAD8 (68) Numeric keypad 8 key
        VKEY_NUMPAD8 = 0x68,

        // VKEY_NUMPAD9 (69) Numeric keypad 9 key
        VKEY_NUMPAD9 = 0x69,

        // VKEY_MULTIPLY (6A) Multiply key
        VKEY_MULTIPLY = 0x6A,

        // VKEY_ADD (6B) Add key
        VKEY_ADD = 0x6B,

        // VKEY_SEPARATOR (6C) Separator key
        VKEY_SEPARATOR = 0x6C,

        // VKEY_SUBTRACT (6D) Subtract key
        VKEY_SUBTRACT = 0x6D,

        // VKEY_DECIMAL (6E) Decimal key
        VKEY_DECIMAL = 0x6E,

        // VKEY_DIVIDE (6F) Divide key
        VKEY_DIVIDE = 0x6F,

        // VKEY_F1 (70) F1 key
        VKEY_F1 = 0x70,

        // VKEY_F2 (71) F2 key
        VKEY_F2 = 0x71,

        // VKEY_F3 (72) F3 key
        VKEY_F3 = 0x72,

        // VKEY_F4 (73) F4 key
        VKEY_F4 = 0x73,

        // VKEY_F5 (74) F5 key
        VKEY_F5 = 0x74,

        // VKEY_F6 (75) F6 key
        VKEY_F6 = 0x75,

        // VKEY_F7 (76) F7 key
        VKEY_F7 = 0x76,

        // VKEY_F8 (77) F8 key
        VKEY_F8 = 0x77,

        // VKEY_F9 (78) F9 key
        VKEY_F9 = 0x78,

        // VKEY_F10 (79) F10 key
        VKEY_F10 = 0x79,

        // VKEY_F11 (7A) F11 key
        VKEY_F11 = 0x7A,

        // VKEY_F12 (7B) F12 key
        VKEY_F12 = 0x7B,

        // VKEY_F13 (7C) F13 key
        VKEY_F13 = 0x7C,

        // VKEY_F14 (7D) F14 key
        VKEY_F14 = 0x7D,

        // VKEY_F15 (7E) F15 key
        VKEY_F15 = 0x7E,

        // VKEY_F16 (7F) F16 key
        VKEY_F16 = 0x7F,

        // VKEY_F17 (80H) F17 key
        VKEY_F17 = 0x80,

        // VKEY_F18 (81H) F18 key
        VKEY_F18 = 0x81,

        // VKEY_F19 (82H) F19 key
        VKEY_F19 = 0x82,

        // VKEY_F20 (83H) F20 key
        VKEY_F20 = 0x83,

        // VKEY_F21 (84H) F21 key
        VKEY_F21 = 0x84,

        // VKEY_F22 (85H) F22 key
        VKEY_F22 = 0x85,

        // VKEY_F23 (86H) F23 key
        VKEY_F23 = 0x86,

        // VKEY_F24 (87H) F24 key
        VKEY_F24 = 0x87,

        // VKEY_NUMLOCK (90) NUM LOCK key
        VKEY_NUMLOCK = 0x90,

        // VKEY_SCROLL (91) SCROLL LOCK key
        VKEY_SCROLL = 0x91,

        // VKEY_LSHIFT (A0) Left SHIFT key
        VKEY_LSHIFT = 0xA0,

        // VKEY_RSHIFT (A1) Right SHIFT key
        VKEY_RSHIFT = 0xA1,

        // VKEY_LCONTROL (A2) Left CONTROL key
        VKEY_LCONTROL = 0xA2,

        // VKEY_RCONTROL (A3) Right CONTROL key
        VKEY_RCONTROL = 0xA3,

        // VKEY_LMENU (A4) Left MENU key
        VKEY_LMENU = 0xA4,

        // VKEY_RMENU (A5) Right MENU key
        VKEY_RMENU = 0xA5,

        // VKEY_BROWSER_BACK (A6) Windows 2000/XP: Browser Back key
        VKEY_BROWSER_BACK = 0xA6,

        // VKEY_BROWSER_FORWARD (A7) Windows 2000/XP: Browser Forward key
        VKEY_BROWSER_FORWARD = 0xA7,

        // VKEY_BROWSER_REFRESH (A8) Windows 2000/XP: Browser Refresh key
        VKEY_BROWSER_REFRESH = 0xA8,

        // VKEY_BROWSER_STOP (A9) Windows 2000/XP: Browser Stop key
        VKEY_BROWSER_STOP = 0xA9,

        // VKEY_BROWSER_SEARCH (AA) Windows 2000/XP: Browser Search key
        VKEY_BROWSER_SEARCH = 0xAA,

        // VKEY_BROWSER_FAVORITES (AB) Windows 2000/XP: Browser Favorites key
        VKEY_BROWSER_FAVORITES = 0xAB,

        // VKEY_BROWSER_HOME (AC) Windows 2000/XP: Browser Start and Home key
        VKEY_BROWSER_HOME = 0xAC,

        // VKEY_VOLUME_MUTE (AD) Windows 2000/XP: Volume Mute key
        VKEY_VOLUME_MUTE = 0xAD,

        // VKEY_VOLUME_DOWN (AE) Windows 2000/XP: Volume Down key
        VKEY_VOLUME_DOWN = 0xAE,

        // VKEY_VOLUME_UP (AF) Windows 2000/XP: Volume Up key
        VKEY_VOLUME_UP = 0xAF,

        // VKEY_MEDIA_NEXT_TRACK (B0) Windows 2000/XP: Next Track key
        VKEY_MEDIA_NEXT_TRACK = 0xB0,

        // VKEY_MEDIA_PREV_TRACK (B1) Windows 2000/XP: Previous Track key
        VKEY_MEDIA_PREV_TRACK = 0xB1,

        // VKEY_MEDIA_STOP (B2) Windows 2000/XP: Stop Media key
        VKEY_MEDIA_STOP = 0xB2,

        // VKEY_MEDIA_PLAY_PAUSE (B3) Windows 2000/XP: Play/Pause Media key
        VKEY_MEDIA_PLAY_PAUSE = 0xB3,

        // VKEY_LAUNCH_MAIL (B4) Windows 2000/XP: Start Mail key
        VKEY_MEDIA_LAUNCH_MAIL = 0xB4,

        // VKEY_LAUNCH_MEDIA_SELECT (B5) Windows 2000/XP: Select Media key
        VKEY_MEDIA_LAUNCH_MEDIA_SELECT = 0xB5,

        // VKEY_LAUNCH_APP1 (B6) Windows 2000/XP: Start Application 1 key
        VKEY_MEDIA_LAUNCH_APP1 = 0xB6,

        // VKEY_LAUNCH_APP2 (B7) Windows 2000/XP: Start Application 2 key
        VKEY_MEDIA_LAUNCH_APP2 = 0xB7,

        // VKEY_OEM_1 (BA) Used for miscellaneous characters, it can vary by keyboard. Windows 2000/XP: For the US standard keyboard, the ',:' key
        VKEY_OEM_1 = 0xBA,

        // VKEY_OEM_PLUS (BB) Windows 2000/XP: For any country/region, the '+' key
        VKEY_OEM_PLUS = 0xBB,

        // VKEY_OEM_COMMA (BC) Windows 2000/XP: For any country/region, the ',' key
        VKEY_OEM_COMMA = 0xBC,

        // VKEY_OEM_MINUS (BD) Windows 2000/XP: For any country/region, the '-' key
        VKEY_OEM_MINUS = 0xBD,

        // VKEY_OEM_PERIOD (BE) Windows 2000/XP: For any country/region, the '.' key
        VKEY_OEM_PERIOD = 0xBE,

        // VKEY_OEM_2 (BF) Used for miscellaneous characters, it can vary by keyboard. Windows 2000/XP: For the US standard keyboard, the '/?' key
        VKEY_OEM_2 = 0xBF,

        // VKEY_OEM_3 (C0) Used for miscellaneous characters, it can vary by keyboard. Windows 2000/XP: For the US standard keyboard, the '`~' key
        VKEY_OEM_3 = 0xC0,

        // VKEY_OEM_4 (DB) Used for miscellaneous characters, it can vary by keyboard. Windows 2000/XP: For the US standard keyboard, the '[{' key
        VKEY_OEM_4 = 0xDB,

        // VKEY_OEM_5 (DC) Used for miscellaneous characters, it can vary by keyboard. Windows 2000/XP: For the US standard keyboard, the '\|' key
        VKEY_OEM_5 = 0xDC,

        // VKEY_OEM_6 (DD) Used for miscellaneous characters, it can vary by keyboard. Windows 2000/XP: For the US standard keyboard, the ']}' key
        VKEY_OEM_6 = 0xDD,

        // VKEY_OEM_7 (DE) Used for miscellaneous characters, it can vary by keyboard. Windows 2000/XP: For the US standard keyboard, the 'single-quote/double-quote' key
        VKEY_OEM_7 = 0xDE,

        // VKEY_OEM_8 (DF) Used for miscellaneous characters, it can vary by keyboard.
        VKEY_OEM_8 = 0xDF,

        // VKEY_OEM_102 (E2) Windows 2000/XP: Either the angle bracket key or the backslash key on the RT 102-key keyboard
        VKEY_OEM_102 = 0xE2,

        // VKEY_PROCESSKEY (E5) Windows 95/98/Me, Windows NT 4.0, Windows 2000/XP: IME PROCESS key
        VKEY_PROCESSKEY = 0xE5,

        // VKEY_PACKET (E7) Windows 2000/XP: Used to pass Unicode characters as if they were keystrokes. The VKEY_PACKET key is the low word of a 32-bit Virtual Key value used for non-keyboard input methods. For more information, see Remark in KEYBDINPUT,SendInput, WM_KEYDOWN, and WM_KEYUP
        VKEY_PACKET = 0xE7,

        // VKEY_ATTN (F6) Attn key
        VKEY_ATTN = 0xF6,

        // VKEY_CRSEL (F7) CrSel key
        VKEY_CRSEL = 0xF7,

        // VKEY_EXSEL (F8) ExSel key
        VKEY_EXSEL = 0xF8,

        // VKEY_EREOF (F9) Erase EOF key
        VKEY_EREOF = 0xF9,

        // VKEY_PLAY (FA) Play key
        VKEY_PLAY = 0xFA,

        // VKEY_ZOOM (FB) Zoom key
        VKEY_ZOOM = 0xFB,

        // VKEY_NONAME (FC) Reserved for future use
        VKEY_NONAME = 0xFC,

        // VKEY_PA1 (FD) PA1 key
        VKEY_PA1 = 0xFD,

        // VKEY_OEM_CLEAR (FE) Clear key
        VKEY_OEM_CLEAR = 0xFE,

        VKEY_UNKNOWN = 0
    };

} // namespace WebCore

#endif
