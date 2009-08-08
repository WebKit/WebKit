/*
 * Copyright (C) 2007 Ryan Leavengood <leavengood@gmail.com>
 * Copyright (C) 2008 Andrea Anzani <andrea.anzani@gmail.com>
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

#include "KeyboardCodes.h"
#include "NotImplemented.h"
#include <InterfaceDefs.h>
#include <Message.h>
#include <String.h>


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
        return "U+0009";
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

    return String::format("U+%04X", toASCIIUpper(keyCode));
}

static int windowsKeyCodeForKeyEvent(char singleByte)
{
    switch (singleByte) {
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
    case 0x2e:
    default:
        return 0;
    }
}

PlatformKeyboardEvent::PlatformKeyboardEvent(BMessage* message)
    : m_autoRepeat(false)
    , m_ctrlKey(false)
    , m_altKey(false)
    , m_metaKey(false)
    , m_isKeypad(false)
    , m_shiftKey(false)
{
    BString bytes = message->FindString("bytes");

    m_text = String::fromUTF8(bytes.String(), bytes.Length());
    m_unmodifiedText = String(bytes.String(), bytes.Length());
    m_keyIdentifier = keyIdentifierForHaikuKeyCode(bytes.ByteAt(0), message->FindInt32("key"));

    if (message->what == B_KEY_UP)
        m_type = KeyUp;
    else if (message->what == B_KEY_DOWN)
        m_type = KeyDown;

    m_windowsVirtualKeyCode = windowsKeyCodeForKeyEvent(bytes.ByteAt(0));
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
    notImplemented();
    return false;
}

} // namespace WebCore

