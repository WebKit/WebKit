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

#include <String.h>
#include <wtf/text/StringConcatenateNumbers.h>
#include <wtf/HexNumber.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringConcatenate.h>

#include "PlatformKeyboardEvent.h"

#include "NotImplemented.h"
#include "WindowsKeyboardCodes.h"
#include <InterfaceDefs.h>
#include <Message.h>


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

	// FIXME will not work for non-ASCII characters
    return makeString("U+", hex(toASCIIUpper(singleByte)), 4);
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

static String KeyValueForKeyEvent(BString bytes, int keyCode)
{
    switch (bytes.ByteAt(0)) {

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
                    return "ScrollLock";
            }
            break;

        case B_BACKSPACE:
            return "Backspace";
        case B_LEFT_ARROW:
            return "ArrowLeft";
        case B_RIGHT_ARROW:
            return "ArrowRight";
        case B_UP_ARROW:
            return "ArrowUp";
        case B_DOWN_ARROW:
            return "ArrowDown";
        case B_INSERT:
            return "Insert";
        case B_ENTER:
            return "Enter";
        case B_DELETE:
            return "Delete";
        case B_HOME:
            return "Home";
        case B_END:
            return "End";
        case B_PAGE_UP:
            return "PageUp";
        case B_PAGE_DOWN:
            return "PageDown";
        case B_TAB:
            return "Tab";
        case B_SPACE:
            return " "; // (20) SPACEBAR
            
        case B_ESCAPE:
            return "Escape";
            
        default: {
            return bytes;          
        }
    }
    return "Unidentified";
}

static String KeyCodeForKeyEvent(int keyCode)
{
    switch (keyCode) {
        case 0x0001:
            return "Escape";
        case 0x0002:
            return "F1";
        case 0x0003:
            return "F2";
        case 0x0004:
            return "F3";
        case 0x0005:
            return "F4";
        case 0x0006:
            return "F5";
        case 0x0007:
            return "F6";
        case 0x0008:
            return "F7";
        case 0x0009:
            return "F8";
        case 0x000A:
            return "F9";
        case 0x000B:
            return "F10";
        case 0x000C:
            return "F11";
        case 0x000D:
            return "F12";

        case 0x000E:
            return "PrintScreen";
        case 0x000F:
            return "ScrollLock";
        case 0x0010:
            return "Pause";

        case 0x0011:
            return "Backquote";
        case 0x0012:
            return "Digit1";
        case 0x0013:
            return "Digit2";
        case 0x0014:
            return "Digit3";
        case 0x0015:
            return "Digit4";
        case 0x0016:
            return "Digit5";
        case 0x0017:
            return "Digit6";
        case 0x0018:
            return "Digit7";
        case 0x0019:
            return "Digit8";
        case 0x001A:
            return "Digit9";
        case 0x001B:
            return "Digit0";
        case 0x001C:
            return "Minus";
        case 0x001D:
            return "Equal";
        case 0x001E:
            return "Backspace"; // IntYen

        case 0x001F:
            return "Insert";
        case 0x0020:
            return "Home";
        case 0x0021:
            return "PageUp";
            
        case 0x0022:
            return "NumLock";
        case 0x0023:
            return "NumpadDivide";
        case 0x0024:
            return "NumpadMultiply";
        case 0x0025:
            return "NumpadSubtract";

        case 0x0026:
            return "Tab";
        case 0x0027:
            return "KeyQ";
        case 0x0028:
            return "KeyW";
        case 0x0029:
            return "KeyE";
        case 0x002A:
            return "KeyR";
        case 0x002B:
            return "KeyT";
        case 0x002C:
            return "KeyY";
        case 0x002D:
            return "KeyU";
        case 0x002E:
            return "KeyI";
        case 0x002F:
            return "KeyO";
        case 0x0030:
            return "KepP";
        case 0x0031:
            return "BracketLeft";
        case 0x0032:
            return "BracketRight";
        case 0x0033:
            return "Backslash";

        case 0x0034:
            return "Delete";
        case 0x0035:
            return "End";
        case 0x0036:
            return "PageDown";

        case 0x0037:
            return "Numpad7";
        case 0x0038:
            return "Numpad8";
        case 0x0039:
            return "Numpad9";
        case 0x003A:
            return "NumpadAdd";

        case 0x003B:
            return "CapsLock";
        case 0x003C:
            return "KeyA";
        case 0x003D:
            return "KeyS";
        case 0x003E:
            return "KeyD";
        case 0x003F:
            return "KeyF";
        case 0x0040:
            return "KeyG";
        case 0x0041:
            return "KeyH";
        case 0x0042:
            return "KeyJ";
        case 0x0043:
            return "KeyK";
        case 0x0044:
            return "KeyL";
        case 0x0045:
            return "Semicolon";
        case 0x0046:
            return "Quote";
        case 0x0047:
            return "Return";

        case 0x0048:
            return "Numpad4";
        case 0x0049:
            return "Numpad5";
        case 0x004A:
            return "Numpad6";

        case 0x004B:
            return "ShiftLeft";
        case 0x004C:
            return "KeyZ";
        case 0x004D:
            return "KeyX";
        case 0x004E:
            return "KeyC";
        case 0x004F:
            return "KeyV";
        case 0x0050:
            return "KeyB";
        case 0x0051:
            return "KeyN";
        case 0x0052:
            return "KeyM";
        case 0x0053:
            return "Comma";
        case 0x0054:
            return "Period";
        case 0x0055:
            return "Slash";
        case 0x0056:
            return "ShiftRight";

        case 0x0057:
            return "ArrowUp";
        case 0x0058:
            return "Digit1";
        case 0x0059:
            return "Digit2";
        case 0x005A:
            return "Digit3";
        case 0x005B:
            return "NumpadEnter";
        case 0x005C:
            return "ControlLeft";
        case 0x005D:
            return "AltLeft";
        case 0x005E:
            return "Space";
        case 0x005F:
            return "AltRight";
        case 0x0060:
            return "ControlRight";
        case 0x0061:
            return "ArrowLeft";
        case 0x0062:
            return "ArrowDown";
        case 0x0063:
            return "ArrowRight";
        case 0x0064:
            return "Numpad0";
        case 0x0065:
            return "NumpadDecimal";
        case 0x0066:
            return "OSLeft"; // MetaLeft
        case 0x0067:
            return "OSRight";
        case 0x0068:
            return "ContextMenu";
        case 0x0069:
            return "IntlBackslash";
        case 0x006a:
            return "NumPadEqual";
    }
    return "Unidentified";
}

PlatformKeyboardEvent::PlatformKeyboardEvent(BMessage* message)
    : m_autoRepeat(false)
    , m_isKeypad(false)
{
    BString bytes = message->FindString("bytes");

    int32 nativeVirtualKeyCode = message->FindInt32("key");

    m_text = String::fromUTF8(bytes.String(), bytes.Length());
    m_unmodifiedText = String(bytes.String(), bytes.Length());
    m_keyIdentifier = keyIdentifierForHaikuKeyCode(bytes.ByteAt(0), nativeVirtualKeyCode);

    m_windowsVirtualKeyCode = windowsKeyCodeForKeyEvent(bytes.ByteAt(0), nativeVirtualKeyCode);
	// TODO m_key should also do something for modifier keys, which cannot be
	// extracted from "bytes"
    m_key = KeyValueForKeyEvent(bytes, nativeVirtualKeyCode);
    m_code = KeyCodeForKeyEvent(nativeVirtualKeyCode);

    if (message->what == B_KEY_UP)
        m_type = KeyUp;
    else if (message->what == B_KEY_DOWN)
        m_type = KeyDown;

    int32 modifiers = message->FindInt32("modifiers");
    if (modifiers & B_SHIFT_KEY)
        m_modifiers.add(PlatformEvent::Modifier::ShiftKey);
    if (modifiers & B_COMMAND_KEY)
        m_modifiers.add(PlatformEvent::Modifier::ControlKey);
    if (modifiers & B_CONTROL_KEY)
        m_modifiers.add(PlatformEvent::Modifier::AltKey);
    if (modifiers & B_OPTION_KEY)
        m_modifiers.add(PlatformEvent::Modifier::MetaKey);
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
    // FIXME: copied on Qt, check this
    notImplemented();
    shiftKey = false;
    ctrlKey = false;
    altKey = false;
    metaKey = false;
}

} // namespace WebCore

