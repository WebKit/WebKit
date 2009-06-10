/*
 * Copyright 2007, The Android Open Source Project
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
 * Copyright (C) 2006 Michael Emmel mike.emmel@gmail.com
 * Copyright (C) 2007 Holger Hans Peter Freyther
 * Copyright (C) 2008 Collabora, Ltd.  All rights reserved.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
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
#include <ui/KeycodeLabels.h>

namespace WebCore {

// compare to same function in gdk/KeyEventGdk.cpp
static int windowsKeyCodeForKeyEvent(unsigned int keyCode)
{
    // Does not provide all key codes, and does not handle all keys.
    switch(keyCode) {
        case kKeyCodeDel:
            return VK_BACK;
        case kKeyCodeTab:
            return VK_TAB;
        case kKeyCodeClear:
            return VK_CLEAR;
        case kKeyCodeDpadCenter:
        case kKeyCodeNewline:
            return VK_RETURN;
        case kKeyCodeShiftLeft:
        case kKeyCodeShiftRight:
            return VK_SHIFT;
        // back will serve as escape, although we probably do not have access to it
        case kKeyCodeBack:
            return VK_ESCAPE;
        case kKeyCodeSpace:
            return VK_SPACE;
        case kKeyCodeHome:
            return VK_HOME;
        case kKeyCodeDpadLeft:
            return VK_LEFT;
        case kKeyCodeDpadUp:
            return VK_UP;
        case kKeyCodeDpadRight:
            return VK_RIGHT;
        case kKeyCodeDpadDown:
            return VK_DOWN;
        case kKeyCode0:
            return VK_0;
        case kKeyCode1:
            return VK_1;
        case kKeyCode2:
            return VK_2;
        case kKeyCode3:
            return VK_3;
        case kKeyCode4:
            return VK_4;
        case kKeyCode5:
            return VK_5;
        case kKeyCode6:
            return VK_6;
        case kKeyCode7:
            return VK_7;
        case kKeyCode8:
            return VK_8;
        case kKeyCode9:
            return VK_9;
        case kKeyCodeA:
            return VK_A;
        case kKeyCodeB:
            return VK_B;
        case kKeyCodeC:
            return VK_C;
        case kKeyCodeD:
            return VK_D;
        case kKeyCodeE:
            return VK_E;
        case kKeyCodeF:
            return VK_F;
        case kKeyCodeG:
            return VK_G;
        case kKeyCodeH:
            return VK_H;
        case kKeyCodeI:
            return VK_I;
        case kKeyCodeJ:
            return VK_J;
        case kKeyCodeK:
            return VK_K;
        case kKeyCodeL:
            return VK_L;
        case kKeyCodeM:
            return VK_M;
        case kKeyCodeN:
            return VK_N;
        case kKeyCodeO:
            return VK_O;
        case kKeyCodeP:
            return VK_P;
        case kKeyCodeQ:
            return VK_Q;
        case kKeyCodeR:
            return VK_R;
        case kKeyCodeS:
            return VK_S;
        case kKeyCodeT:
            return VK_T;
        case kKeyCodeU:
            return VK_U;
        case kKeyCodeV:
            return VK_V;
        case kKeyCodeW:
            return VK_W;
        case kKeyCodeX:
            return VK_X;
        case kKeyCodeY:
            return VK_Y;
        case kKeyCodeZ:
            return VK_Z;
        // colon
        case kKeyCodeSemicolon:
            return VK_OEM_1;
        case kKeyCodeComma:
            return VK_OEM_COMMA;
        case kKeyCodeMinus:
            return VK_OEM_MINUS;
        case kKeyCodeEquals:
            return VK_OEM_PLUS;
        case kKeyCodePeriod:
            return VK_OEM_PERIOD;
        case kKeyCodeSlash:
            return VK_OEM_2;
        // maybe not the right choice
        case kKeyCodeLeftBracket:
            return VK_OEM_4;
        case kKeyCodeBackslash:
            return VK_OEM_5;
        case kKeyCodeRightBracket:
            return VK_OEM_6;
        default:
            return 0;
    }
}

static String keyIdentifierForAndroidKeyCode(int keyCode)
{
    // Does not return all of the same key identifiers, and
    // does not handle all the keys.
    switch (keyCode) {
        case kKeyCodeClear:
            return "Clear";
        case kKeyCodeNewline:
        case kKeyCodeDpadCenter:
            return "Enter";
        case kKeyCodeHome:
            return "Home";
        case kKeyCodeDpadDown:
            return "Down";
        case kKeyCodeDpadLeft:
            return "Left";
        case kKeyCodeDpadRight:
            return "Right";
        case kKeyCodeDpadUp:
            return "Up";
        // Standard says that DEL becomes U+00007F.
        case kKeyCodeDel:
            return "U+00007F";
        default:
            char upper[16];
            sprintf(upper, "U+%06X", windowsKeyCodeForKeyEvent(keyCode));
            return String(upper);
    }
}

static inline String singleCharacterString(UChar32 c) 
{
    if (!c)
        return String();
    if (c > 0xffff) {
        UChar lead = U16_LEAD(c);
        UChar trail = U16_TRAIL(c);
        UChar utf16[2] = {lead, trail};
        return String(utf16, 2);
    }
    UChar n = (UChar)c;
    return String(&n, 1);
}

PlatformKeyboardEvent::PlatformKeyboardEvent(int keyCode, UChar32 unichar,
                                             Type type, int repeatCount,
                                             ModifierKey mods)
    : m_type(type)
    , m_text(singleCharacterString(unichar))
    , m_unmodifiedText(singleCharacterString(unichar))
    , m_keyIdentifier(keyIdentifierForAndroidKeyCode(keyCode))
    , m_autoRepeat(repeatCount > 0)
    , m_windowsVirtualKeyCode(windowsKeyCodeForKeyEvent(keyCode))
    , m_nativeVirtualKeyCode(keyCode)
    , m_isKeypad(false)
    , m_shiftKey((mods & ShiftKey))
    , m_ctrlKey((mods & CtrlKey))
    , m_altKey((mods & AltKey))
    , m_metaKey((mods & MetaKey))
    // added for android
    , m_repeatCount(repeatCount)
    , m_unichar(unichar)
{
    // Copied from the mac port
    if (m_windowsVirtualKeyCode == '\r') {
        m_text = "\r";
        m_unmodifiedText = "\r";
    }

    if (m_text == "\x7F")
        m_text = "\x8";
    if (m_unmodifiedText == "\x7F")
        m_unmodifiedText = "\x8";

    if (m_windowsVirtualKeyCode == 9) {
        m_text = "\x9";
        m_unmodifiedText = "\x9";
    }
}

bool PlatformKeyboardEvent::currentCapsLockState()
{
    notImplemented();
    return false;
}

void PlatformKeyboardEvent::disambiguateKeyDownEvent(Type type, bool backwardCompatibilityMode)
{
    // Copied with modification from the mac port.
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
    }
}

} // namespace WebCore
