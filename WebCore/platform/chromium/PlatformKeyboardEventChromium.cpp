/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
 * Copyright (C) 2008, 2009 Google Inc.
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

#if PLATFORM(WIN_OS)
#include <windows.h>
#elif PLATFORM(DARWIN)
#import <Carbon/Carbon.h>
#else
#include "NotImplemented.h"
#endif

namespace WebCore {

void PlatformKeyboardEvent::disambiguateKeyDownEvent(Type type, bool backwardCompatibilityMode)
{
#if PLATFORM(WIN_OS)
    // No KeyDown events on Windows to disambiguate.
    ASSERT_NOT_REACHED();
#else
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
#if PLATFORM(DARWIN)
        if (m_text.length() == 1 && (m_text[0U] >= 0xF700 && m_text[0U] <= 0xF7FF)) {
            // According to NSEvents.h, OpenStep reserves the range 0xF700-0xF8FF for function keys. However, some actual private use characters
            // happen to be in this range, e.g. the Apple logo (Option+Shift+K).
            // 0xF7FF is an arbitrary cut-off.
            m_text = String();
            m_unmodifiedText = String();
        }
#endif
    }
#endif
}

bool PlatformKeyboardEvent::currentCapsLockState()
{
#if PLATFORM(WIN_OS)
    // FIXME: Does this even work inside the sandbox?
    return GetKeyState(VK_CAPITAL) & 1;
#elif PLATFORM(DARWIN)
    return GetCurrentKeyModifiers() & alphaLock;
#else
    notImplemented();
    return false;
#endif
}

} // namespace WebCore
