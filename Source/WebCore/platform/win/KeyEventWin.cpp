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
#include <wtf/text/MakeString.h>

#ifndef MAPVK_VSC_TO_VK_EX
#define MAPVK_VSC_TO_VK_EX 3
#endif

namespace WebCore {

static const unsigned short HIGH_BIT_MASK_SHORT = 0x8000;

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
