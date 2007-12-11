/*
 * Copyright (C) 2007 Kevin Ollivier <kevino@theolliviers.com>
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

#include "config.h"
#include "PlatformKeyboardEvent.h"

#include "KeyboardCodes.h"
#include "NotImplemented.h"

#include <wx/defs.h>
#include <wx/event.h>

namespace WebCore {

static String keyIdentifierForWxKeyCode(int keyCode)
{
    switch (keyCode) {
        case WXK_MENU:
        case WXK_ALT:
            return "Alt";
        case WXK_CLEAR:
            return "Clear";
        case WXK_DOWN:
            return "Down";
        case WXK_END:
            return "End";
        case WXK_RETURN:
            return "Enter";
        case WXK_EXECUTE:
            return "Execute";
        case WXK_F1:
            return "F1";
        case WXK_F2:
            return "F2";
        case WXK_F3:
            return "F3";
        case WXK_F4:
            return "F4";
        case WXK_F5:
            return "F5";
        case WXK_F6:
            return "F6";
        case WXK_F7:
            return "F7";
        case WXK_F8:
            return "F8";
        case WXK_F9:
            return "F9";
        case WXK_F10:
            return "F10";
        case WXK_F11:
            return "F11";
        case WXK_F12:
            return "F12";
        case WXK_F13:
            return "F13";
        case WXK_F14:
            return "F14";
        case WXK_F15:
            return "F15";
        case WXK_F16:
            return "F16";
        case WXK_F17:
            return "F17";
        case WXK_F18:
            return "F18";
        case WXK_F19:
            return "F19";
        case WXK_F20:
            return "F20";
        case WXK_F21:
            return "F21";
        case WXK_F22:
            return "F22";
        case WXK_F23:
            return "F23";
        case WXK_F24:
            return "F24";
        case WXK_HELP:
            return "Help";
        case WXK_HOME:
            return "Home";
        case WXK_INSERT:
            return "Insert";
        case WXK_LEFT:
            return "Left";
        case WXK_PAGEDOWN:
            return "PageDown";
        case WXK_PAGEUP:
            return "PageUp";
        case WXK_PAUSE:
            return "Pause";
        case WXK_PRINT:
            return "PrintScreen";
        case WXK_RIGHT:
            return "Right";
        case WXK_SELECT:
            return "Select";
        case WXK_UP:
            return "Up";
            // Standard says that DEL becomes U+007F.
        case WXK_DELETE:
            return "U+007F";
        default:
            return String::format("U+%04X", toupper(keyCode));
    }
}

static int windowsKeyCodeForKeyEvent(unsigned int keycode)
{
    switch (keycode) {
        /* FIXME: Need to supply a bool in this func, to determine wheter the event comes from the keypad
        */
        case WXK_NUMPAD0:
            return VK_NUMPAD0;// (60) Numeric keypad 0 key
        case WXK_NUMPAD1:
            return VK_NUMPAD1;// (61) Numeric keypad 1 key
        case WXK_NUMPAD2:
            return  VK_NUMPAD2; // (62) Numeric keypad 2 key
        case WXK_NUMPAD3:
            return VK_NUMPAD3; // (63) Numeric keypad 3 key
        case WXK_NUMPAD4:
            return VK_NUMPAD4; // (64) Numeric keypad 4 key
        case WXK_NUMPAD5:
            return VK_NUMPAD5; //(65) Numeric keypad 5 key
        case WXK_NUMPAD6:
            return VK_NUMPAD6; // (66) Numeric keypad 6 key
        case WXK_NUMPAD7:
            return VK_NUMPAD7; // (67) Numeric keypad 7 key
        case WXK_NUMPAD8:
            return VK_NUMPAD8; // (68) Numeric keypad 8 key
        case WXK_NUMPAD9:
            return VK_NUMPAD9; // (69) Numeric keypad 9 key
        case WXK_NUMPAD_MULTIPLY:
            return VK_MULTIPLY; // (6A) Multiply key
        case WXK_NUMPAD_ADD:
            return VK_ADD; // (6B) Add key
        case WXK_NUMPAD_SUBTRACT:
            return VK_SUBTRACT; // (6D) Subtract key
        case WXK_NUMPAD_DECIMAL:
            return VK_DECIMAL; // (6E) Decimal key
        case WXK_DIVIDE:
            return VK_DIVIDE; // (6F) Divide key

        
        case WXK_BACK:
            return VK_BACK; // (08) BACKSPACE key
        case WXK_TAB:
            return VK_TAB; // (09) TAB key
        case WXK_CLEAR:
            return VK_CLEAR; // (0C) CLEAR key
        case WXK_RETURN:
            return VK_RETURN; //(0D) Return key
        case WXK_SHIFT:
            return VK_SHIFT; // (10) SHIFT key
        case WXK_CONTROL:
            return VK_CONTROL; // (11) CTRL key
        case WXK_MENU:
        case WXK_ALT:
            return VK_MENU; // (12) ALT key

        case WXK_PAUSE:
            return VK_PAUSE; // (13) PAUSE key
        case WXK_CAPITAL:
            return VK_CAPITAL; // (14) CAPS LOCK key
        /*
        case WXK_Kana_Lock:
        case WXK_Kana_Shift:
            return VK_KANA; // (15) Input Method Editor (IME) Kana mode
        case WXK_Hangul:
            return VK_HANGUL; // VK_HANGUL (15) IME Hangul mode
            // VK_JUNJA (17) IME Junja mode
            // VK_FINAL (18) IME final mode
        case WXK_Hangul_Hanja:
            return VK_HANJA; // (19) IME Hanja mode
        case WXK_Kanji:
            return VK_KANJI; // (19) IME Kanji mode
        */
        case WXK_ESCAPE:
            return VK_ESCAPE; // (1B) ESC key
            // VK_CONVERT (1C) IME convert
            // VK_NONCONVERT (1D) IME nonconvert
            // VK_ACCEPT (1E) IME accept
            // VK_MODECHANGE (1F) IME mode change request
        case WXK_SPACE:
            return VK_SPACE; // (20) SPACEBAR
        case WXK_PAGEUP:
            return VK_PRIOR; // (21) PAGE UP key
        case WXK_PAGEDOWN:
            return VK_NEXT; // (22) PAGE DOWN key
        case WXK_END:
            return VK_END; // (23) END key
        case WXK_HOME:
            return VK_HOME; // (24) HOME key
        case WXK_LEFT:
            return VK_LEFT; // (25) LEFT ARROW key
        case WXK_UP:
            return VK_UP; // (26) UP ARROW key
        case WXK_RIGHT:
            return VK_RIGHT; // (27) RIGHT ARROW key
        case WXK_DOWN:
            return VK_DOWN; // (28) DOWN ARROW key
        case WXK_SELECT:
            return VK_SELECT; // (29) SELECT key
        case WXK_PRINT:
            return VK_PRINT; // (2A) PRINT key
        case WXK_EXECUTE:
            return VK_EXECUTE;// (2B) EXECUTE key
            //dunno on this
            //case WXK_PrintScreen:
            //      return VK_SNAPSHOT; // (2C) PRINT SCREEN key
        case WXK_INSERT:
            return VK_INSERT; // (2D) INS key
        case WXK_DELETE:
            return VK_DELETE; // (2E) DEL key
        case WXK_HELP:
            return VK_HELP; // (2F) HELP key
        case '0':
            return VK_0;    //  (30) 0) key
        case '1':
            return VK_1; //  (31) 1 ! key
        case '2':
            return VK_2; //  (32) 2 & key
        case '3':
            return VK_3; //case '3': case '#';
        case '4':  //  (34) 4 key '$';
            return VK_4;
        case '5':
            return VK_5; //  (35) 5 key  '%'
        case '6':
            return VK_6; //  (36) 6 key  '^'
        case '7':
            return VK_7; //  (37) 7 key  case '&'
        case '8':
            return VK_8; //  (38) 8 key  '*'
        case '9':
            return VK_9; //  (39) 9 key '('
        case 'A':
            return VK_A; //  (41) A key case 'a': case 'A': return 0x41;
        case 'B':
            return VK_B; //  (42) B key case 'b': case 'B': return 0x42;
        case 'C':
            return VK_C; //  (43) C key case 'c': case 'C': return 0x43;
        case 'D':
            return VK_D; //  (44) D key case 'd': case 'D': return 0x44;
        case 'E':
            return VK_E; //  (45) E key case 'e': case 'E': return 0x45;
        case 'F':
            return VK_F; //  (46) F key case 'f': case 'F': return 0x46;
        case 'G':
            return VK_G; //  (47) G key case 'g': case 'G': return 0x47;
        case 'H':
            return VK_H; //  (48) H key case 'h': case 'H': return 0x48;
        case 'I':
            return VK_I; //  (49) I key case 'i': case 'I': return 0x49;
        case 'J':
            return VK_J; //  (4A) J key case 'j': case 'J': return 0x4A;
        case 'K':
            return VK_K; //  (4B) K key case 'k': case 'K': return 0x4B;
        case 'L':
            return VK_L; //  (4C) L key case 'l': case 'L': return 0x4C;
        case 'M':
            return VK_M; //  (4D) M key case 'm': case 'M': return 0x4D;
        case 'N':
            return VK_N; //  (4E) N key case 'n': case 'N': return 0x4E;
        case 'O':
            return VK_O; //  (4F) O key case 'o': case 'O': return 0x4F;
        case 'P':
            return VK_P; //  (50) P key case 'p': case 'P': return 0x50;
        case 'Q':
            return VK_Q; //  (51) Q key case 'q': case 'Q': return 0x51;
        case 'R':
            return VK_R; //  (52) R key case 'r': case 'R': return 0x52;
        case 'S':
            return VK_S; //  (53) S key case 's': case 'S': return 0x53;
        case 'T':
            return VK_T; //  (54) T key case 't': case 'T': return 0x54;
        case 'U':
            return VK_U; //  (55) U key case 'u': case 'U': return 0x55;
        case 'V':
            return VK_V; //  (56) V key case 'v': case 'V': return 0x56;
        case 'W':
            return VK_W; //  (57) W key case 'w': case 'W': return 0x57;
        case 'X':
            return VK_X; //  (58) X key case 'x': case 'X': return 0x58;
        case 'Y':
            return VK_Y; //  (59) Y key case 'y': case 'Y': return 0x59;
        case 'Z':
            return VK_Z; //  (5A) Z key case 'z': case 'Z': return 0x5A;
        case WXK_WINDOWS_LEFT:
            return VK_LWIN; // (5B) Left Windows key (Microsoft Natural keyboard)

        case WXK_NUMLOCK:
            return VK_NUMLOCK; // (90) NUM LOCK key

        case WXK_SCROLL:
            return VK_SCROLL; // (91) SCROLL LOCK key

        default:
            return 0;
    }
}

PlatformKeyboardEvent::PlatformKeyboardEvent(wxKeyEvent& event)
{
    if (event.GetEventType() == wxEVT_KEY_UP)
        m_type = KeyUp;
    else if (event.GetEventType() == wxEVT_KEY_DOWN)
        m_type = KeyDown;
    else if (event.GetEventType() == wxEVT_CHAR)
        m_type = Char;
    else
        ASSERT_NOT_REACHED();
    if (m_type != Char)
        m_keyIdentifier = keyIdentifierForWxKeyCode(event.GetKeyCode());
    else {
        m_text = wxString(event.GetUnicodeKey());
        m_unmodifiedText = m_text;
    }
    m_autoRepeat = false; // FIXME: not correct.
    m_windowsVirtualKeyCode = windowsKeyCodeForKeyEvent(event.GetKeyCode());
    m_isKeypad = (event.GetKeyCode() >= WXK_NUMPAD_SPACE) && (event.GetKeyCode() <= WXK_NUMPAD_DIVIDE);
    m_shiftKey = event.ShiftDown();
    m_ctrlKey = event.CmdDown();
    m_altKey = event.AltDown();
    m_metaKey = event.MetaDown();
}

void PlatformKeyboardEvent::disambiguateKeyDownEvent(Type type, bool)
{
    // Can only change type from KeyDown to RawKeyDown or Char, as we lack information for other conversions.
    ASSERT(m_type == KeyDown);
    m_type = type;
    if (type == RawKeyDown) {
        m_text = String();
        m_unmodifiedText = String();
    } else {
        m_keyIdentifier = String();
        m_windowsVirtualKeyCode = 0;
    }
}

bool PlatformKeyboardEvent::currentCapsLockState()
{
    return wxGetKeyState(WXK_CAPITAL);
}

}

