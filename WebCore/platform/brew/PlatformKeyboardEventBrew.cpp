/*
 * Copyright (C) 2010 Company 100, Inc. All rights reserved.
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

#include <AEEEvent.h>
#include <AEEIKeysMapping.h>
#include <AEEKeysMapping.bid>
#include <AEEStdDef.h>
#include <AEEVCodes.h>

#include <wtf/brew/RefPtrBrew.h>
#include <wtf/brew/ShellBrew.h>

namespace WebCore {

static String keyIdentifierForBrewKeyCode(uint16 keyCode)
{
    switch (keyCode) {
    case AVK_LALT:
    case AVK_RALT:
        return "Alt";
    case AVK_LCTRL:
    case AVK_RCTRL:
        return "Control";
    case AVK_LSHIFT:
    case AVK_RSHIFT:
        return "Shift";
    case AVK_CAPLK:
        return "CapsLock";
    case AVK_FUNCTION1:
        return "F1";
    case AVK_FUNCTION2:
        return "F2";
    case AVK_FUNCTION3:
        return "F3";
    case AVK_FUNCTION4:
        return "F4";
    case AVK_FUNCTION5:
        return "F5";
    case AVK_FUNCTION6:
        return "F6";
    case AVK_FUNCTION7:
        return "F7";
    case AVK_FUNCTION8:
        return "F8";
    case AVK_FUNCTION9:
        return "F9";
    case AVK_FUNCTION10:
        return "F10";
    case AVK_FUNCTION11:
        return "F11";
    case AVK_FUNCTION12:
        return "F12";
    case AVK_PRSCRN:
        return "PrintScreen";
    case AVK_LEFT:
        return "Left";
    case AVK_RIGHT:
        return "Right";
    case AVK_UP:
        return "Up";
    case AVK_DOWN:
        return "Down";
    case AVK_TXINSERT:
        return "Insert";
    case AVK_ENTER:
        return "Enter";
    case AVK_TXHOME:
        return "Home";
    case AVK_TXDELETE:
        // Standard says that DEL becomes U+007F.
        return "U+007F";
    case AVK_TXEND:
        return "End";
    case AVK_TXPGUP:
        return "PageUp";
    case AVK_TXPGDOWN:
        return "PageDown";
    case AVK_FUNCTION:
        return "U+0009";
    default:
        return String::format("U+%04X", toASCIIUpper(keyCode));
    }
}

static int windowsKeyCodeForKeyEvent(uint16 code)
{
    switch (code) {
    case AVK_CLR:
        return VK_BACK; // (08) BACKSPACE key
    case AVK_ENTER:
        return VK_RETURN; // (0D) Return key
    case AVK_SPACE:
        return VK_SPACE; // (20) SPACEBAR
    case AVK_TXPGUP:
        return VK_PRIOR; // (21) PAGE UP key
    case AVK_TXPGDOWN:
        return VK_NEXT; // (22) PAGE DOWN key
    case AVK_TXEND:
        return VK_END; // (23) END key
    case AVK_TXHOME:
        return VK_HOME; // (24) HOME key
    case AVK_LEFT:
        return VK_LEFT; // (25) LEFT ARROW key
    case AVK_UP:
        return VK_UP; // (26) UP ARROW key
    case AVK_RIGHT:
        return VK_RIGHT; // (27) RIGHT ARROW key
    case AVK_DOWN:
        return VK_DOWN; // (28) DOWN ARROW key
    case AVK_TXINSERT:
        return VK_INSERT; // (2D) INS key
    case AVK_TXDELETE:
        return VK_DELETE; // (2E) DEL key
    case AVK_FUNCTION:
        return VK_TAB; // (09) TAB key
    default:
        return 0;
    }
}

static inline String singleCharacterString(UChar c)
{
    UChar text;

    // Some key codes are not mapped to Unicode characters. Convert them to Unicode characters here.
    switch (c) {
    case AVK_0:
        text = VK_0;
        break;
    case AVK_1:
        text = VK_1;
        break;
    case AVK_2:
        text = VK_2;
        break;
    case AVK_3:
        text = VK_3;
        break;
    case AVK_4:
        text = VK_4;
        break;
    case AVK_5:
        text = VK_5;
        break;
    case AVK_6:
        text = VK_6;
        break;
    case AVK_7:
        text = VK_7;
        break;
    case AVK_8:
        text = VK_8;
        break;
    case AVK_9:
        text = VK_9;
        break;
    case AVK_STAR:
        text = '*';
        break;
    case AVK_POUND:
        text = '#';
        break;
    case AVK_FUNCTION1:
        text = '=';
        break;
    case AVK_FUNCTION2:
        text = '/';
        break;
    case AVK_FUNCTION3:
        text = '_';
        break;
    case AVK_PUNC1:
        text = ',';
        break;
    case AVK_PUNC2:
        text = '.';
        break;
    case AVK_SPACE:
        text = VK_SPACE;
        break;
    default:
        text = c;
        break;
    }

    return String(&text, 1);
}

PlatformKeyboardEvent::PlatformKeyboardEvent(AEEEvent event, uint16 code, uint32 modifiers, Type type)
    : m_type(type)
    , m_isKeypad(false)
    , m_metaKey(false)
    , m_windowsVirtualKeyCode((type == RawKeyDown || type == KeyUp) ? windowsKeyCodeForKeyEvent(code) : 0)
{
    if ((m_type == Char) && modifiers) {
        RefPtr<IKeysMapping> keysMapping = createRefPtrInstance<IKeysMapping>(AEECLSID_KeysMapping);
        int result = IKeysMapping_GetMapping(keysMapping.get(), code, modifiers, reinterpret_cast<AECHAR*>(&code));
        if (result == AEE_SUCCESS) // Reset the modifier when key code is successfully mapped.
            modifiers = 0;
    }

    m_text = (type == Char) ? singleCharacterString(code) : String();
    m_unmodifiedText = (type == Char) ? singleCharacterString(code) : String();
    m_keyIdentifier = (type == Char) ? String() : keyIdentifierForBrewKeyCode(code);
    m_nativeVirtualKeyCode = code;
    m_autoRepeat = modifiers & KB_AUTOREPEAT;
    m_shiftKey = modifiers & (KB_LSHIFT | KB_RSHIFT);
    m_ctrlKey = modifiers & (KB_LCTRL | KB_RCTRL);
    m_altKey = modifiers & (KB_LALT | KB_RALT);
}

void PlatformKeyboardEvent::disambiguateKeyDownEvent(Type type, bool)
{
    // No KeyDown events on BREW to disambiguate.
    ASSERT_NOT_REACHED();
}

bool PlatformKeyboardEvent::currentCapsLockState()
{
    notImplemented();
    return false;
}

void PlatformKeyboardEvent::getCurrentModifierState(bool& shiftKey, bool& ctrlKey, bool& altKey, bool& metaKey)
{
    notImplemented();
    shiftKey = false;
    ctrlKey = false;
    altKey = false;
    metaKey = false;
}

} // namespace WebCore
