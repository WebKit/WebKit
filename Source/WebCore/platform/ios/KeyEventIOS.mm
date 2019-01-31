/*
 * Copyright (C) 2004, 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
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

#if PLATFORM(IOS_FAMILY)

#import "KeyEventCocoa.h"
#import "KeyEventCodesIOS.h"
#import "NotImplemented.h"
#import "WebEvent.h"
#import "WindowsKeyboardCodes.h"
#import <pal/spi/cocoa/IOKitSPI.h>
#import <wtf/MainThread.h>

namespace WebCore {

int windowsKeyCodeForKeyCode(uint16_t keyCode)
{
    static const int windowsKeyCode[] = {
        /* 0 */ 0, // n/a
        /* 1 */ 0, // n/a
        /* 2 */ 0, // n/a
        /* 3 */ 0, // n/a
        /* 4 */ VK_A,
        /* 5 */ VK_B,
        /* 6 */ VK_C,
        /* 7 */ VK_D,
        /* 8 */ VK_E,
        /* 9 */ VK_F,
        /* 0x0A */ VK_G,
        /* 0x0B */ VK_H,
        /* 0x0C */ VK_I,
        /* 0x0D */ VK_J,
        /* 0x0E */ VK_K,
        /* 0x0F */ VK_L,
        /* 0x10 */ VK_M,
        /* 0x11 */ VK_N,
        /* 0x12 */ VK_O,
        /* 0x13 */ VK_P,
        /* 0x14 */ VK_Q,
        /* 0x15 */ VK_R,
        /* 0x16 */ VK_S,
        /* 0x17 */ VK_T,
        /* 0x18 */ VK_U,
        /* 0x19 */ VK_V,
        /* 0x1A */ VK_W,
        /* 0x1B */ VK_X,
        /* 0x1C */ VK_Y,
        /* 0x1D */ VK_Z,
        /* 0x1E */ VK_1,
        /* 0x1F */ VK_2,
        /* 0x20 */ VK_3,
        /* 0x21 */ VK_4,
        /* 0x22 */ VK_5,
        /* 0x23 */ VK_6,
        /* 0x24 */ VK_7,
        /* 0x25 */ VK_8,
        /* 0x26 */ VK_9,
        /* 0x27 */ VK_0,
        /* 0x28 */ VK_RETURN, // Return
        /* 0x29 */ VK_ESCAPE,
        /* 0x2A */ VK_BACK, // Backspace
        /* 0x2B */ VK_TAB,
        /* 0x2C */ VK_SPACE,
        /* 0x2D */ VK_OEM_MINUS, // -_
        /* 0x2E */ VK_OEM_PLUS, // =+
        /* 0x2F */ VK_OEM_4, // [{
        /* 0x30 */ VK_OEM_6, // ]}
        /* 0x31 */ VK_OEM_5, // \|
        /* 0x32 */ 0, // kHIDUsage_KeyboardNonUSPound
        /* 0x33 */ VK_OEM_1, // ;:
        /* 0x34 */ VK_OEM_7, // '"
        /* 0x35 */ VK_OEM_3, // `~
        /* 0x36 */ VK_OEM_COMMA, // ,<
        /* 0x37 */ VK_OEM_PERIOD, // .>
        /* 0x38 */ VK_OEM_2, // /?
        /* 0x39 */ VK_CAPITAL, // Caps Lock
        /* 0x3A */ VK_F1,
        /* 0x3B */ VK_F2,
        /* 0x3C */ VK_F3,
        /* 0x3D */ VK_F4,
        /* 0x3E */ VK_F5,
        /* 0x3F */ VK_F6,
        /* 0x40 */ VK_F7,
        /* 0x41 */ VK_F8,
        /* 0x42 */ VK_F9,
        /* 0x43 */ VK_F10,
        /* 0x44 */ VK_F11,
        /* 0x45 */ VK_F12,
        /* 0x46 */ VK_SNAPSHOT, // Print Screen
        /* 0x47 */ VK_SCROLL, // Scroll Lock
        /* 0x48 */ VK_PAUSE,
        /* 0x49 */ VK_INSERT,
        /* 0x4A */ VK_HOME, // Home
        /* 0x4B */ VK_PRIOR, // Page Up
        /* 0x4C */ VK_DELETE, // Forward Delete
        /* 0x4D */ VK_END, // End
        /* 0x4E */ VK_NEXT, // Page Down
        /* 0x4F */ VK_RIGHT, // Right Arrow
        /* 0x50 */ VK_LEFT, // Left Arrow
        /* 0x51 */ VK_DOWN, // Down Arrow
        /* 0x52 */ VK_UP, // Up Arrow
        /* 0x53 */ VK_CLEAR, // Num Pad Clear
        /* 0x54 */ VK_DIVIDE, // Num Pad /
        /* 0x55 */ VK_MULTIPLY, // Num Pad *
        /* 0x56 */ VK_SUBTRACT, // Num Pad -
        /* 0x57 */ VK_ADD, // Num Pad +
        /* 0x58 */ VK_RETURN, // Num Pad Enter
        /* 0x59 */ VK_NUMPAD1,
        /* 0x5A */ VK_NUMPAD2,
        /* 0x5B */ VK_NUMPAD3,
        /* 0x5C */ VK_NUMPAD4,
        /* 0x5D */ VK_NUMPAD5,
        /* 0x5E */ VK_NUMPAD6,
        /* 0x5F */ VK_NUMPAD7,
        /* 0x60 */ VK_NUMPAD8,
        /* 0x61 */ VK_NUMPAD9,
        /* 0x62 */ VK_NUMPAD0,
        /* 0x63 */ VK_DECIMAL, // Num Pad .
        /* 0x64 */ 0, // Non-ANSI \|
        /* 0x65 */ 0, // Application
        /* 0x66 */ 0, // Power
        /* 0x67 */ VK_OEM_PLUS, // Num Pad =. There is no such key on common PC keyboards, mapping to normal "+=".
        /* 0x68 */ VK_F13,
        /* 0x69 */ VK_F14,
        /* 0x6A */ VK_F15,
        /* 0x6B */ VK_F16,
        /* 0x6C */ VK_F17,
        /* 0x6D */ VK_F18,
        /* 0x6E */ VK_F19,
        /* 0x6F */ VK_F20,
        /* 0x70 */ VK_F21,
        /* 0x71 */ VK_F22,
        /* 0x72 */ VK_F23,
        /* 0x73 */ VK_F24,
        /* 0x74 */ VK_EXECUTE,
        /* 0x75 */ VK_INSERT, // Help
        /* 0x76 */ 0, // Menu
        /* 0x77 */ VK_SELECT,
        /* 0x78 */ 0, // Stop
        /* 0x79 */ 0, // Again
        /* 0x7A */ 0, // Undo
        /* 0x7B */ 0, // Cut
        /* 0x7C */ 0, // Copy
        /* 0x7D */ 0, // Paste
        /* 0x7E */ 0, // Find
        /* 0x7F */ VK_VOLUME_MUTE, // Mute
        /* 0x80 */ VK_VOLUME_UP, // Volume Up
        /* 0x81 */ VK_VOLUME_DOWN, // Volume Down
    };
    // Check if key is a modifier.
    switch (keyCode) {
    case kHIDUsage_KeyboardLeftControl:
        return VK_LCONTROL;
    case kHIDUsage_KeyboardLeftShift:
        return VK_LSHIFT;
    case kHIDUsage_KeyboardLeftAlt: // Left Option
        return VK_LMENU;
    case kHIDUsage_KeyboardLeftGUI: // Left Command
        return VK_LWIN;
    case kHIDUsage_KeyboardRightControl:
        return VK_RCONTROL;
    case kHIDUsage_KeyboardRightShift:
        return VK_RSHIFT;
    case kHIDUsage_KeyboardRightAlt: // Right Option
        return VK_RMENU;
    case kHIDUsage_KeyboardRightGUI: // Right Command
        return VK_APPS;
    }
    // Otherwise check all other known keys.
    if (keyCode < WTF_ARRAY_LENGTH(windowsKeyCode))
        return windowsKeyCode[keyCode];
    return 0; // Unknown key
}

// This function is only used to map software keyboard events because they lack a key code.
// When !USE(UIKIT_KEYBOARD_ADDITIONS), this function is also used to map hardware keyboard
// keyup events because they lack a key code.
int windowsKeyCodeForCharCode(unichar charCode)
{
    switch (charCode) {
    case 8: case 0x7F: return VK_BACK;
    case 9: return VK_TAB;
    case 0xD: case 3: return VK_RETURN;
    case ' ': return VK_SPACE;

    case '0': case ')': return VK_0;
    case '1': case '!': return VK_1;
    case '2': case '@': return VK_2;
    case '3': case '#': return VK_3;
    case '4': case '$': return VK_4;
    case '5': case '%': return VK_5;
    case '6': case '^': return VK_6;
    case '7': case '&': return VK_7;
    case '8': case '*': return VK_8;
    case '9': case '(': return VK_9;
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

#if !USE(UIKIT_KEYBOARD_ADDITIONS)
    case 0x1B: return VK_ESCAPE; // WebKit generated code for Escape.

    // WebKit uses Unicode PUA codes in the OpenStep reserve range for some special keys.
    case NSUpArrowFunctionKey: return VK_UP;
    case NSDownArrowFunctionKey: return VK_DOWN;
    case NSLeftArrowFunctionKey: return VK_LEFT;
    case NSRightArrowFunctionKey: return VK_RIGHT;
    case NSPageUpFunctionKey: return VK_PRIOR;
    case NSPageDownFunctionKey: return VK_NEXT;
#endif

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

static bool isFunctionKey(UChar charCode)
{
    switch (charCode) {
#if !USE(UIKIT_KEYBOARD_ADDITIONS)
    case 1: // Home
    case 4: // End
    case 5: // FIXME: For some reason WebKitTestRunner generates this code for F14 (why?).
    case 0x7F: // Forward Delete
    case 0x10: // Function key (e.g. F1, F2, ...)
#endif

    // WebKit uses Unicode PUA codes in the OpenStep reserve range for some special keys.
    case NSUpArrowFunctionKey:
    case NSDownArrowFunctionKey:
    case NSLeftArrowFunctionKey:
    case NSRightArrowFunctionKey:
    case NSPageUpFunctionKey:
    case NSPageDownFunctionKey:
    case NSClearLineFunctionKey: // Num Lock / Clear
#if USE(UIKIT_KEYBOARD_ADDITIONS)
    case NSDeleteFunctionKey: // Forward delete
    case NSEndFunctionKey:
    case NSInsertFunctionKey:
    case NSHomeFunctionKey:
#endif
        return true;
    }
#if USE(UIKIT_KEYBOARD_ADDITIONS)
    if (charCode >= NSF1FunctionKey && charCode <= NSF24FunctionKey)
        return true;
#endif
    return false;
}

void PlatformKeyboardEvent::disambiguateKeyDownEvent(Type type, bool backwardCompatibilityMode)
{
    // Can only change type from KeyDown to RawKeyDown or Char, as we lack information for other conversions.
    ASSERT(m_type == KeyDown);
    ASSERT(type == RawKeyDown || type == Char);
    m_type = type;
    if (backwardCompatibilityMode)
        return;

    if (type == PlatformEvent::RawKeyDown) {
        m_text = String();
        m_unmodifiedText = String();
    } else {
        m_keyIdentifier = String();
        m_windowsVirtualKeyCode = 0;
        if (m_text.length() == 1 && isFunctionKey(m_text[0U])) {
            m_text = String();
            m_unmodifiedText = String();
        }
    }
}

OptionSet<PlatformEvent::Modifier> PlatformKeyboardEvent::currentStateOfModifierKeys()
{
    // s_currentModifiers is only set in the WebContent process, not in the UI process.
    if (s_currentModifiers) {
        ASSERT(isMainThread());
        return *s_currentModifiers;
    }

    ::WebEventFlags currentModifiers = [::WebEvent modifierFlags];

    OptionSet<PlatformEvent::Modifier> modifiers;
    if (currentModifiers & ::WebEventFlagMaskShiftKey)
        modifiers.add(PlatformEvent::Modifier::ShiftKey);
    if (currentModifiers & ::WebEventFlagMaskControlKey)
        modifiers.add(PlatformEvent::Modifier::CtrlKey);
    if (currentModifiers & ::WebEventFlagMaskOptionKey)
        modifiers.add(PlatformEvent::Modifier::AltKey);
    if (currentModifiers & ::WebEventFlagMaskCommandKey)
        modifiers.add(PlatformEvent::Modifier::MetaKey);
    if (currentModifiers & ::WebEventFlagMaskLeftCapsLockKey)
        modifiers.add(PlatformEvent::Modifier::CapsLockKey);

    return modifiers;
}

}

#endif // PLATFORM(IOS_FAMILY)
