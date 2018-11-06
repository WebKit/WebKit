/*
 * Copyright (C) 2004, 2006, 2007, 2010 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "PlatformKeyboardEvent.h"

#if PLATFORM(MAC)

#import "KeyEventCocoa.h"
#import "Logging.h"
#import "WindowsKeyboardCodes.h"
#import <Carbon/Carbon.h>
#import <wtf/MainThread.h>

namespace WebCore {
using namespace WTF;

int windowsKeyCodeForKeyCode(uint16_t keyCode)
{
    static const int windowsKeyCode[] = {
        /* 0 */ VK_A,
        /* 1 */ VK_S,
        /* 2 */ VK_D,
        /* 3 */ VK_F,
        /* 4 */ VK_H,
        /* 5 */ VK_G,
        /* 6 */ VK_Z,
        /* 7 */ VK_X,
        /* 8 */ VK_C,
        /* 9 */ VK_V,
        /* 0x0A */ VK_OEM_3, // "Section" - key to the left from 1 (ISO Keyboard Only)
        /* 0x0B */ VK_B,
        /* 0x0C */ VK_Q,
        /* 0x0D */ VK_W,
        /* 0x0E */ VK_E,
        /* 0x0F */ VK_R,
        /* 0x10 */ VK_Y,
        /* 0x11 */ VK_T,
        /* 0x12 */ VK_1,
        /* 0x13 */ VK_2,
        /* 0x14 */ VK_3,
        /* 0x15 */ VK_4,
        /* 0x16 */ VK_6,
        /* 0x17 */ VK_5,
        /* 0x18 */ VK_OEM_PLUS, // =+
        /* 0x19 */ VK_9,
        /* 0x1A */ VK_7,
        /* 0x1B */ VK_OEM_MINUS, // -_
        /* 0x1C */ VK_8,
        /* 0x1D */ VK_0,
        /* 0x1E */ VK_OEM_6, // ]}
        /* 0x1F */ VK_O,
        /* 0x20 */ VK_U,
        /* 0x21 */ VK_OEM_4, // {[
        /* 0x22 */ VK_I,
        /* 0x23 */ VK_P,
        /* 0x24 */ VK_RETURN, // Return
        /* 0x25 */ VK_L,
        /* 0x26 */ VK_J,
        /* 0x27 */ VK_OEM_7, // '"
        /* 0x28 */ VK_K,
        /* 0x29 */ VK_OEM_1, // ;:
        /* 0x2A */ VK_OEM_5, // \|
        /* 0x2B */ VK_OEM_COMMA, // ,<
        /* 0x2C */ VK_OEM_2, // /?
        /* 0x2D */ VK_N,
        /* 0x2E */ VK_M,
        /* 0x2F */ VK_OEM_PERIOD, // .>
        /* 0x30 */ VK_TAB,
        /* 0x31 */ VK_SPACE,
        /* 0x32 */ VK_OEM_3, // `~
        /* 0x33 */ VK_BACK, // Backspace
        /* 0x34 */ 0, // n/a
        /* 0x35 */ VK_ESCAPE,
        /* 0x36 */ VK_APPS, // Right Command
        /* 0x37 */ VK_LWIN, // Left Command
        /* 0x38 */ VK_LSHIFT, // Left Shift
        /* 0x39 */ VK_CAPITAL, // Caps Lock
        /* 0x3A */ VK_LMENU, // Left Option
        /* 0x3B */ VK_LCONTROL, // Left Ctrl
        /* 0x3C */ VK_RSHIFT, // Right Shift
        /* 0x3D */ VK_RMENU, // Right Option
        /* 0x3E */ VK_RCONTROL, // Right Ctrl
        /* 0x3F */ 0, // fn
        /* 0x40 */ VK_F17,
        /* 0x41 */ VK_DECIMAL, // Num Pad .
        /* 0x42 */ 0, // n/a
        /* 0x43 */ VK_MULTIPLY, // Num Pad *
        /* 0x44 */ 0, // n/a
        /* 0x45 */ VK_ADD, // Num Pad +
        /* 0x46 */ 0, // n/a
        /* 0x47 */ VK_CLEAR, // Num Pad Clear
        /* 0x48 */ VK_VOLUME_UP,
        /* 0x49 */ VK_VOLUME_DOWN,
        /* 0x4A */ VK_VOLUME_MUTE,
        /* 0x4B */ VK_DIVIDE, // Num Pad /
        /* 0x4C */ VK_RETURN, // Num Pad Enter
        /* 0x4D */ 0, // n/a
        /* 0x4E */ VK_SUBTRACT, // Num Pad -
        /* 0x4F */ VK_F18,
        /* 0x50 */ VK_F19,
        /* 0x51 */ VK_OEM_PLUS, // Num Pad =. There is no such key on common PC keyboards, mapping to normal "+=".
        /* 0x52 */ VK_NUMPAD0,
        /* 0x53 */ VK_NUMPAD1,
        /* 0x54 */ VK_NUMPAD2,
        /* 0x55 */ VK_NUMPAD3,
        /* 0x56 */ VK_NUMPAD4,
        /* 0x57 */ VK_NUMPAD5,
        /* 0x58 */ VK_NUMPAD6,
        /* 0x59 */ VK_NUMPAD7,
        /* 0x5A */ VK_F20,
        /* 0x5B */ VK_NUMPAD8,
        /* 0x5C */ VK_NUMPAD9,
        /* 0x5D */ 0, // Yen (JIS Keyboard Only)
        /* 0x5E */ 0, // Underscore (JIS Keyboard Only)
        /* 0x5F */ 0, // KeypadComma (JIS Keyboard Only)
        /* 0x60 */ VK_F5,
        /* 0x61 */ VK_F6,
        /* 0x62 */ VK_F7,
        /* 0x63 */ VK_F3,
        /* 0x64 */ VK_F8,
        /* 0x65 */ VK_F9,
        /* 0x66 */ 0, // Eisu (JIS Keyboard Only)
        /* 0x67 */ VK_F11,
        /* 0x68 */ 0, // Kana (JIS Keyboard Only)
        /* 0x69 */ VK_F13,
        /* 0x6A */ VK_F16,
        /* 0x6B */ VK_F14,
        /* 0x6C */ 0, // n/a
        /* 0x6D */ VK_F10,
        /* 0x6E */ 0, // n/a (Windows95 key?)
        /* 0x6F */ VK_F12,
        /* 0x70 */ 0, // n/a
        /* 0x71 */ VK_F15,
        /* 0x72 */ VK_INSERT, // Help
        /* 0x73 */ VK_HOME, // Home
        /* 0x74 */ VK_PRIOR, // Page Up
        /* 0x75 */ VK_DELETE, // Forward Delete
        /* 0x76 */ VK_F4,
        /* 0x77 */ VK_END, // End
        /* 0x78 */ VK_F2,
        /* 0x79 */ VK_NEXT, // Page Down
        /* 0x7A */ VK_F1,
        /* 0x7B */ VK_LEFT, // Left Arrow
        /* 0x7C */ VK_RIGHT, // Right Arrow
        /* 0x7D */ VK_DOWN, // Down Arrow
        /* 0x7E */ VK_UP, // Up Arrow
    };
    if (keyCode < WTF_ARRAY_LENGTH(windowsKeyCode))
        return windowsKeyCode[keyCode];
    return 0;
}

int windowsKeyCodeForCharCode(unichar charCode)
{
    switch (charCode) {
    case 'a': case 'A': return VK_A;
    case 'b': case 'B': return VK_B;
    case 'c': case 'C': return VK_C;
    case 'd': case 'D': return VK_D;
    case 'e': case 'E': return VK_E;
    case 'f': case 'F': return VK_F;
    case 'g': case 'G': return VK_G;
    case 'h': case 'H': return VK_H;
    case 'i': case 'I': return VK_I;
    case 'j': case 'J': return VK_J;
    case 'k': case 'K': return VK_K;
    case 'l': case 'L': return VK_L;
    case 'm': case 'M': return VK_M;
    case 'n': case 'N': return VK_N;
    case 'o': case 'O': return VK_O;
    case 'p': case 'P': return VK_P;
    case 'q': case 'Q': return VK_Q;
    case 'r': case 'R': return VK_R;
    case 's': case 'S': return VK_S;
    case 't': case 'T': return VK_T;
    case 'u': case 'U': return VK_U;
    case 'v': case 'V': return VK_V;
    case 'w': case 'W': return VK_W;
    case 'x': case 'X': return VK_X;
    case 'y': case 'Y': return VK_Y;
    case 'z': case 'Z': return VK_Z;

    // AppKit generates Unicode PUA character codes for some function keys; using these when key code is not known.
    case NSPauseFunctionKey: return VK_PAUSE;
    case NSSelectFunctionKey: return VK_SELECT;
    case NSPrintFunctionKey: return VK_PRINT;
    case NSExecuteFunctionKey: return VK_EXECUTE;
    case NSPrintScreenFunctionKey: return VK_SNAPSHOT;
    case NSInsertFunctionKey: return VK_INSERT;
    case NSF21FunctionKey: return VK_F21;
    case NSF22FunctionKey: return VK_F22;
    case NSF23FunctionKey: return VK_F23;
    case NSF24FunctionKey: return VK_F24;
    case NSScrollLockFunctionKey: return VK_SCROLL;

    // This is for U.S. keyboard mapping, and doesn't necessarily make sense for different keyboard layouts.
    // For example, '"' on Windows Russian layout is VK_2, not VK_OEM_7.
    case ';': case ':': return VK_OEM_1;
    case '=': case '+': return VK_OEM_PLUS;
    case ',': case '<': return VK_OEM_COMMA;
    case '-': case '_': return VK_OEM_MINUS;
    case '.': case '>': return VK_OEM_PERIOD;
    case '/': case '?': return VK_OEM_2;
    case '`': case '~': return VK_OEM_3;
    case '[': case '{': return VK_OEM_4;
    case '\\': case '|': return VK_OEM_5;
    case ']': case '}': return VK_OEM_6;
    case '\'': case '"': return VK_OEM_7;
    }
    return 0;
}

void PlatformKeyboardEvent::disambiguateKeyDownEvent(Type type, bool backwardCompatibilityMode)
{
    // Can only change type from KeyDown to RawKeyDown or Char, as we lack information for other conversions.
    ASSERT(m_type == KeyDown);
    ASSERT(type == RawKeyDown || type == Char);
    m_type = type;
    if (backwardCompatibilityMode)
        return;

    if (type == RawKeyDown) {
        m_text = String();
        m_unmodifiedText = String();
    } else {
        m_keyIdentifier = String();
        m_windowsVirtualKeyCode = 0;
        if (m_text.length() == 1 && (m_text[0U] >= 0xF700 && m_text[0U] <= 0xF7FF)) {
            // According to NSEvents.h, OpenStep reserves the range 0xF700-0xF8FF for function keys. However, some actual private use characters
            // happen to be in this range, e.g. the Apple logo (Option+Shift+K).
            // 0xF7FF is an arbitrary cut-off.
            m_text = String();
            m_unmodifiedText = String();
        }
    }
}

bool PlatformKeyboardEvent::currentCapsLockState()
{
    auto currentModifiers = currentStateOfModifierKeys();
    return currentModifiers.contains(PlatformEvent::Modifier::CapsLockKey);
}

void PlatformKeyboardEvent::getCurrentModifierState(bool& shiftKey, bool& ctrlKey, bool& altKey, bool& metaKey)
{
    auto currentModifiers = currentStateOfModifierKeys();
    shiftKey = currentModifiers.contains(PlatformEvent::Modifier::ShiftKey);
    ctrlKey = currentModifiers.contains(PlatformEvent::Modifier::CtrlKey);
    altKey = currentModifiers.contains(PlatformEvent::Modifier::AltKey);
    metaKey = currentModifiers.contains(PlatformEvent::Modifier::MetaKey);
}

OptionSet<PlatformEvent::Modifier> PlatformKeyboardEvent::currentStateOfModifierKeys()
{
#if ENABLE(WEBPROCESS_WINDOWSERVER_BLOCKING)
    // s_currentModifiers is only set in the WebContent process, not in the UI process.
    if (s_currentModifiers) {
        ASSERT(isMainThread());
        return *s_currentModifiers;
    }
#endif
    UInt32 currentModifiers = GetCurrentKeyModifiers();

    OptionSet<PlatformEvent::Modifier> modifiers;
    if (currentModifiers & ::shiftKey)
        modifiers.add(PlatformEvent::Modifier::ShiftKey);
    if (currentModifiers & ::controlKey)
        modifiers.add(PlatformEvent::Modifier::CtrlKey);
    if (currentModifiers & ::optionKey)
        modifiers.add(PlatformEvent::Modifier::AltKey);
    if (currentModifiers & ::cmdKey)
        modifiers.add(PlatformEvent::Modifier::MetaKey);
    if (currentModifiers & ::alphaLock)
        modifiers.add(PlatformEvent::Modifier::CapsLockKey);

    return modifiers;
}

} // namespace WebCore

#endif // PLATFORM(MAC)
