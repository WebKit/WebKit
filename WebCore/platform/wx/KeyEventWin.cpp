/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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
#include <windows.h>

#define REPEAT_COUNT_MASK           0x0000FFFF
#define NEW_RELEASE_STATE_MASK      0x80000000
#define PREVIOUS_DOWN_STATE_MASK    0x40000000
#define ALT_KEY_DOWN_MASK           0x20000000

#define HIGH_BIT_MASK_SHORT         0x8000

namespace WebCore {

// FIXME: This is incomplete.  We should change this to mirror
// more like what Firefox does, and generate these switch statements
// at build time.
static String keyIdentifierForWindowsKeyCode(short keyCode)
{
    switch (keyCode) {
        case VK_MENU:
            return "Alt";
        case VK_CLEAR:
            return "Clear";
        case VK_DOWN:
            return "Down";
        // "End"
        case VK_END:
            return "End";
        // "Enter"
        case VK_RETURN:
            return "Enter";
        case VK_EXECUTE:
            return "Execute";
        case VK_F1:
            return "F1";
        case VK_F2:
            return "F2";
        case VK_F3:
            return "F3";
        case VK_F4:
            return "F4";
        case VK_F5:
            return "F5";
        case VK_F6:
            return "F6";
        case VK_F7:
            return "F7";
        case VK_F8:
            return "F8";
        case VK_F9:
            return "F9";
        case VK_F10:
            return "F11";
        case VK_F12:
            return "F12";
        case VK_F13:
            return "F13";
        case VK_F14:
            return "F14";
        case VK_F15:
            return "F15";
        case VK_F16:
            return "F16";
        case VK_F17:
            return "F17";
        case VK_F18:
            return "F18";
        case VK_F19:
            return "F19";
        case VK_F20:
            return "F20";
        case VK_F21:
            return "F21";
        case VK_F22:
            return "F22";
        case VK_F23:
            return "F23";
        case VK_F24:
            return "F24";
        case VK_HELP:
            return "Help";
        case VK_HOME:
            return "Home";
        case VK_INSERT:
            return "Insert";
        case VK_LEFT:
            return "Left";
        case VK_NEXT:
            return "PageDown";
        case VK_PRIOR:
            return "PageUp";
        case VK_PAUSE:
            return "Pause";
        case VK_SNAPSHOT:
            return "PrintScreen";
        case VK_RIGHT:
            return "Right";
        case VK_SCROLL:
            return "Scroll";
        case VK_SELECT:
            return "Select";
        case VK_UP:
            return "Up";
        // Standard says that DEL becomes U+007F.
        case VK_DELETE:
            return "U+007F";
        default:
            return String::sprintf("U+%04X", toupper(keyCode));
    }
}

static String singleCharacterString(UChar c) { return String(&c, 1); }

PlatformKeyboardEvent::PlatformKeyboardEvent(HWND hWnd, WPARAM wParam, LPARAM lParam)
    : m_text(singleCharacterString(wParam))
    , m_unmodifiedText(singleCharacterString(wParam))
    , m_keyIdentifier(keyIdentifierForWindowsKeyCode(wParam))
    , m_isKeyUp((lParam & NEW_RELEASE_STATE_MASK))
    , m_autoRepeat(lParam & REPEAT_COUNT_MASK)
    , m_WindowsKeyCode(wParam)
    , m_isKeypad(false) // FIXME
    , m_shiftKey(GetAsyncKeyState(VK_SHIFT) & HIGH_BIT_MASK_SHORT)
    , m_ctrlKey(GetAsyncKeyState(VK_CONTROL) & HIGH_BIT_MASK_SHORT)
    , m_altKey(lParam & ALT_KEY_DOWN_MASK)
    , m_metaKey(lParam & ALT_KEY_DOWN_MASK) // FIXME: Is this right?
    , m_isModifierKeyPress(false)
{
    if (!m_shiftKey)
        m_text = String(singleCharacterString(tolower(wParam)));
}

}
