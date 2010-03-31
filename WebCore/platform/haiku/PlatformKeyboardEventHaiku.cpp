/*
 * Copyright (C) 2007 Ryan Leavengood <leavengood@gmail.com>
 * Copyright (C) 2008 Andrea Anzani <andrea.anzani@gmail.com>
 * Copyright (C) 2010 Stephan Aßmus <superstippi@gmx.de>
 *
 * All rights reserved.
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

#include "NotImplemented.h"
#include "WindowsKeyboardCodes.h"
#include <InterfaceDefs.h>
#include <Message.h>
#include <String.h>
#include <wtf/text/CString.h>


namespace WebCore {

static String keyIdentifierForHaikuKeyCode(char singleByte, int keyCode)
{
    switch (singleByte) {
    case B_FUNCTION_KEY:

        switch (keyCode) {
        case B_F1_KEY:
            return "F1";
        case B_F2_KEY:
            return "F2";
        case B_F3_KEY:
            return "F3";
        case B_F4_KEY:
            return "F4";
        case B_F5_KEY:
            return "F5";
        case B_F6_KEY:
            return "F6";
        case B_F7_KEY:
            return "F7";
        case B_F8_KEY:
            return "F8";
        case B_F9_KEY:
            return "F9";
        case B_F10_KEY:
            return "F10";
        case B_F11_KEY:
            return "F11";
        case B_F12_KEY:
            return "F12";
        case B_PRINT_KEY:
            return "Print";
        case B_PAUSE_KEY:
            return "Pause";
        case B_SCROLL_KEY:
            return ""; // FIXME
        }
        break;

    case B_BACKSPACE:
        return "U+0008";
    case B_LEFT_ARROW:
        return "Left";
    case B_RIGHT_ARROW:
        return "Right";
    case B_UP_ARROW:
        return "Up";
    case B_DOWN_ARROW:
        return "Down";
    case B_INSERT:
        return "Insert";
    case B_ENTER:
        return "Enter";
    case B_DELETE:
        return "U+007F";
    case B_HOME:
        return "Home";
    case B_END:
        return "End";
    case B_PAGE_UP:
        return "PageUp";
    case B_PAGE_DOWN:
        return "PageDown";
    case B_TAB:
        return "U+0009";
    }

    return String::format("U+%04X", toASCIIUpper(singleByte));
}

static int windowsKeyCodeForKeyEvent(char singleByte, int keyCode)
{
    switch (singleByte) {
    case B_FUNCTION_KEY:
        switch (keyCode) {
        case B_F1_KEY:
            return VK_F1;
        case B_F2_KEY:
            return VK_F2;
        case B_F3_KEY:
            return VK_F3;
        case B_F4_KEY:
            return VK_F4;
        case B_F5_KEY:
            return VK_F5;
        case B_F6_KEY:
            return VK_F6;
        case B_F7_KEY:
            return VK_F7;
        case B_F8_KEY:
            return VK_F8;
        case B_F9_KEY:
            return VK_F9;
        case B_F10_KEY:
            return VK_F10;
        case B_F11_KEY:
            return VK_F11;
        case B_F12_KEY:
            return VK_F12;
        case B_PRINT_KEY:
            return VK_PRINT;
        case B_PAUSE_KEY:
            return 0; // FIXME
        case B_SCROLL_KEY:
            return 0; // FIXME
        }
        break;

    case B_BACKSPACE:
        return VK_BACK; // (08) BACKSPACE key
    case B_TAB:
        return VK_TAB; // (09) TAB key
    case B_RETURN:
        return VK_RETURN; //(0D) Return key
    case B_ESCAPE:
        return VK_ESCAPE; // (1B) ESC key
    case B_SPACE:
        return VK_SPACE; // (20) SPACEBAR
    case B_PAGE_UP:
        return VK_PRIOR; // (21) PAGE UP key
    case B_PAGE_DOWN:
        return VK_NEXT; // (22) PAGE DOWN key
    case B_END:
        return VK_END; // (23) END key
    case B_HOME:
        return VK_HOME; // (24) HOME key
    case B_LEFT_ARROW:
        return VK_LEFT; // (25) LEFT ARROW key
    case B_UP_ARROW:
        return VK_UP; // (26) UP ARROW key
    case B_RIGHT_ARROW:
        return VK_RIGHT; // (27) RIGHT ARROW key
    case B_DOWN_ARROW:
        return VK_DOWN; // (28) DOWN ARROW key
    case B_INSERT:
        return VK_INSERT; // (2D) INS key
    case B_DELETE:
        return VK_DELETE; // (2E) DEL key

    case '0':
    case ')':
        return VK_0;
    case '1':
    case '!':
        return VK_1;
    case '2':
    case '@':
        return VK_2;
    case '3':
    case '#':
        return VK_3;
    case '4':
    case '$':
        return VK_4;
    case '5':
    case '%':
        return VK_5;
    case '6':
    case '^':
        return VK_6;
    case '7':
    case '&':
        return VK_7;
    case '8':
    case '*':
        return VK_8;
    case '9':
    case '(':
        return VK_9;
    case 'a':
    case 'A':
        return VK_A;
    case 'b':
    case 'B':
        return VK_B;
    case 'c':
    case 'C':
        return VK_C;
    case 'd':
    case 'D':
        return VK_D;
    case 'e':
    case 'E':
        return VK_E;
    case 'f':
    case 'F':
        return VK_F;
    case 'g':
    case 'G':
        return VK_G;
    case 'h':
    case 'H':
        return VK_H;
    case 'i':
    case 'I':
        return VK_I;
    case 'j':
    case 'J':
        return VK_J;
    case 'k':
    case 'K':
        return VK_K;
    case 'l':
    case 'L':
        return VK_L;
    case 'm':
    case 'M':
        return VK_M;
    case 'n':
    case 'N':
        return VK_N;
    case 'o':
    case 'O':
        return VK_O;
    case 'p':
    case 'P':
        return VK_P;
    case 'q':
    case 'Q':
        return VK_Q;
    case 'r':
    case 'R':
        return VK_R;
    case 's':
    case 'S':
        return VK_S;
    case 't':
    case 'T':
        return VK_T;
    case 'u':
    case 'U':
        return VK_U;
    case 'v':
    case 'V':
        return VK_V;
    case 'w':
    case 'W':
        return VK_W;
    case 'x':
    case 'X':
        return VK_X;
    case 'y':
    case 'Y':
        return VK_Y;
    case 'z':
    case 'Z':
        return VK_Z;
    case ';':
    case ':':
        return VK_OEM_1;
    case '+':
    case '=':
        return VK_OEM_PLUS;
    case ',':
    case '<':
        return VK_OEM_COMMA;
    case '-':
    case '_':
        return VK_OEM_MINUS;
    case '.':
    case '>':
        return VK_OEM_PERIOD;
    case '/':
    case '?':
        return VK_OEM_2;
    case '`':
    case '~':
        return VK_OEM_3;
    case '[':
    case '{':
        return VK_OEM_4;
    case '\\':
    case '|':
        return VK_OEM_5;
    case ']':
    case '}':
        return VK_OEM_6;
    case '\'':
    case '"':
        return VK_OEM_7;
    }
    return singleByte;
}

PlatformKeyboardEvent::PlatformKeyboardEvent(BMessage* message)
    : m_autoRepeat(false)
    , m_isKeypad(false)
    , m_shiftKey(false)
    , m_ctrlKey(false)
    , m_altKey(false)
    , m_metaKey(false)
{
    BString bytes = message->FindString("bytes");

    m_nativeVirtualKeyCode = message->FindInt32("key");

    m_text = String::fromUTF8(bytes.String(), bytes.Length());
    m_unmodifiedText = String(bytes.String(), bytes.Length());
    m_keyIdentifier = keyIdentifierForHaikuKeyCode(bytes.ByteAt(0), m_nativeVirtualKeyCode);

    m_windowsVirtualKeyCode = windowsKeyCodeForKeyEvent(bytes.ByteAt(0), m_nativeVirtualKeyCode);

    if (message->what == B_KEY_UP)
        m_type = KeyUp;
    else if (message->what == B_KEY_DOWN)
        m_type = KeyDown;

    int32 modifiers = message->FindInt32("modifiers");
    m_shiftKey = modifiers & B_SHIFT_KEY;
    m_ctrlKey = modifiers & B_COMMAND_KEY;
    m_altKey = modifiers & B_CONTROL_KEY;
    m_metaKey = modifiers & B_OPTION_KEY;
}

void PlatformKeyboardEvent::disambiguateKeyDownEvent(Type type, bool backwardCompatibilityMode)
{
    // Can only change type from KeyDown to RawKeyDown or Char, as we lack information for other conversions.
    ASSERT(m_type == KeyDown);
    m_type = type;

    if (backwardCompatibilityMode)
        return;

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
    return ::modifiers() & B_CAPS_LOCK;
}

void PlatformKeyboardEvent::getCurrentModifierState(bool& shiftKey, bool& ctrlKey, bool& altKey, bool& metaKey)
{
    unit32 modifiers = ::modifiers();
    shiftKey = modifiers & B_SHIFT_KEY;
    ctrlKey = modifiers & B_COMMAND_KEY;
    altKey = modifiers & B_CONTROL_KEY;
    metaKey = modifiers & B_OPTION_KEY;
}

} // namespace WebCore

