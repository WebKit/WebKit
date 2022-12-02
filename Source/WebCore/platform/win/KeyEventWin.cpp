/*
 * Copyright (C) 2006-2022 Apple Inc. All rights reserved.
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

#include "config.h"
#include "PlatformKeyboardEvent.h"

#include "WindowsKeyNames.h"
#include <windows.h>
#include <wtf/ASCIICType.h>
#include <wtf/HexNumber.h>
#include <wtf/NeverDestroyed.h>

#ifndef MAPVK_VSC_TO_VK_EX
#define MAPVK_VSC_TO_VK_EX 3
#endif

namespace WebCore {

static const unsigned short HIGH_BIT_MASK_SHORT = 0x8000;

// FIXME: This is incomplete. We could change this to mirror
// more like what Firefox does, and generate these switch statements
// at build time.
static String keyIdentifierForWindowsKeyCode(unsigned short keyCode)
{
    switch (keyCode) {
        case VK_MENU:
            return "Alt"_s;
        case VK_CONTROL:
            return "Control"_s;
        case VK_SHIFT:
            return "Shift"_s;
        case VK_CAPITAL:
            return "CapsLock"_s;
        case VK_LWIN:
        case VK_RWIN:
            return "Win"_s;
        case VK_CLEAR:
            return "Clear"_s;
        case VK_DOWN:
            return "Down"_s;
        // "End"
        case VK_END:
            return "End"_s;
        // "Enter"
        case VK_RETURN:
            return "Enter"_s;
        case VK_EXECUTE:
            return "Execute"_s;
        case VK_F1:
            return "F1"_s;
        case VK_F2:
            return "F2"_s;
        case VK_F3:
            return "F3"_s;
        case VK_F4:
            return "F4"_s;
        case VK_F5:
            return "F5"_s;
        case VK_F6:
            return "F6"_s;
        case VK_F7:
            return "F7"_s;
        case VK_F8:
            return "F8"_s;
        case VK_F9:
            return "F9"_s;
        case VK_F10:
            return "F10"_s;
        case VK_F11:
            return "F11"_s;
        case VK_F12:
            return "F12"_s;
        case VK_F13:
            return "F13"_s;
        case VK_F14:
            return "F14"_s;
        case VK_F15:
            return "F15"_s;
        case VK_F16:
            return "F16"_s;
        case VK_F17:
            return "F17"_s;
        case VK_F18:
            return "F18"_s;
        case VK_F19:
            return "F19"_s;
        case VK_F20:
            return "F20"_s;
        case VK_F21:
            return "F21"_s;
        case VK_F22:
            return "F22"_s;
        case VK_F23:
            return "F23"_s;
        case VK_F24:
            return "F24"_s;
        case VK_HELP:
            return "Help"_s;
        case VK_HOME:
            return "Home"_s;
        case VK_INSERT:
            return "Insert"_s;
        case VK_LEFT:
            return "Left"_s;
        case VK_NEXT:
            return "PageDown"_s;
        case VK_PRIOR:
            return "PageUp"_s;
        case VK_PAUSE:
            return "Pause"_s;
        case VK_SNAPSHOT:
            return "PrintScreen"_s;
        case VK_RIGHT:
            return "Right"_s;
        case VK_SCROLL:
            return "Scroll"_s;
        case VK_SELECT:
            return "Select"_s;
        case VK_UP:
            return "Up"_s;
        // Standard says that DEL becomes U+007F.
        case VK_DELETE:
            return "U+007F"_s;
        default:
            return makeString("U+", hex(toASCIIUpper(keyCode), 4));
    }
}

static bool isKeypadEvent(WPARAM code, LPARAM keyData, PlatformEvent::Type type)
{
    if (type != PlatformEvent::RawKeyDown && type != PlatformEvent::KeyUp)
        return false;

    switch (code) {
        case VK_NUMLOCK:
        case VK_NUMPAD0:
        case VK_NUMPAD1:
        case VK_NUMPAD2:
        case VK_NUMPAD3:
        case VK_NUMPAD4:
        case VK_NUMPAD5:
        case VK_NUMPAD6:
        case VK_NUMPAD7:
        case VK_NUMPAD8:
        case VK_NUMPAD9:
        case VK_MULTIPLY:
        case VK_ADD:
        case VK_SEPARATOR:
        case VK_SUBTRACT:
        case VK_DECIMAL:
        case VK_DIVIDE:
            return true;
        case VK_RETURN:
            return HIWORD(keyData) & KF_EXTENDED;
        case VK_INSERT:
        case VK_DELETE:
        case VK_PRIOR:
        case VK_NEXT:
        case VK_END:
        case VK_HOME:
        case VK_LEFT:
        case VK_UP:
        case VK_RIGHT:
        case VK_DOWN:
            return !(HIWORD(keyData) & KF_EXTENDED);
        default:
            return false;
    }
}

static int windowsKeycodeWithLocation(WPARAM keycode, LPARAM keyData)
{
    if (keycode != VK_CONTROL && keycode != VK_MENU && keycode != VK_SHIFT)
        return keycode;

    // If we don't need to support Windows XP or older Windows,
    // it might be better to use MapVirtualKeyEx with scancode and
    // extended keycode (i.e. 0xe0 or 0xe1).
    if ((keyData >> 16) & KF_EXTENDED) {
        switch (keycode) {
        case VK_CONTROL:
            return VK_RCONTROL;
        case VK_SHIFT:
            return VK_RSHIFT;
        case VK_MENU:
            return VK_RMENU;
        default:
            break;
        }
    }

    int scancode = (keyData >> 16) & 0xFF;
    int regeneratedVirtualKeyCode = ::MapVirtualKey(scancode, MAPVK_VSC_TO_VK_EX);
    return regeneratedVirtualKeyCode ? regeneratedVirtualKeyCode : keycode;
}

static inline String singleCharacterString(UChar c)
{
    return String(&c, 1);
}

static WindowsKeyNames& windowsKeyNames()
{
    static NeverDestroyed<WindowsKeyNames> keyNames;
    return keyNames;
}

PlatformKeyboardEvent::PlatformKeyboardEvent(HWND, WPARAM code, LPARAM keyData, Type type, bool systemKey)
    : PlatformEvent(type, GetKeyState(VK_SHIFT) & HIGH_BIT_MASK_SHORT, GetKeyState(VK_CONTROL) & HIGH_BIT_MASK_SHORT, GetKeyState(VK_MENU) & HIGH_BIT_MASK_SHORT, false, WallTime::fromRawSeconds(::GetTickCount() * 0.001))
    , m_text((type == PlatformEvent::Char) ? singleCharacterString(code) : String())
    , m_unmodifiedText((type == PlatformEvent::Char) ? singleCharacterString(code) : String())
    , m_key(type == PlatformEvent::Char ? windowsKeyNames().domKeyFromChar(code) : windowsKeyNames().domKeyFromParams(code, keyData))
    , m_code(windowsKeyNames().domCodeFromLParam(keyData))
    , m_keyIdentifier((type == PlatformEvent::Char) ? String() : keyIdentifierForWindowsKeyCode(code))
    , m_windowsVirtualKeyCode((type == RawKeyDown || type == KeyUp) ? windowsKeycodeWithLocation(code, keyData) : 0)
    , m_autoRepeat(HIWORD(keyData) & KF_REPEAT)
    , m_isKeypad(isKeypadEvent(code, keyData, type))
    , m_isSystemKey(systemKey)
{
}

void PlatformKeyboardEvent::disambiguateKeyDownEvent(Type, bool)
{
    // No KeyDown events on Windows to disambiguate.
    ASSERT_NOT_REACHED();
}

OptionSet<PlatformEvent::Modifier> PlatformKeyboardEvent::currentStateOfModifierKeys()
{
    OptionSet<PlatformEvent::Modifier> modifiers;

    if (GetKeyState(VK_SHIFT) & HIGH_BIT_MASK_SHORT)
        modifiers.add(PlatformEvent::Modifier::ShiftKey);
    if (GetKeyState(VK_CONTROL) & HIGH_BIT_MASK_SHORT)
        modifiers.add(PlatformEvent::Modifier::ControlKey);
    if (GetKeyState(VK_MENU) & HIGH_BIT_MASK_SHORT)
        modifiers.add(PlatformEvent::Modifier::AltKey);
    // No meta key.
    if (GetKeyState(VK_CAPITAL) & 1)
        modifiers.add(PlatformEvent::Modifier::CapsLockKey);

    return modifiers;
}

}
