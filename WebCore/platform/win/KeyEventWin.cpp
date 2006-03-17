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
#include "KeyEvent.h"
#include <windows.h>

#define REPEAT_COUNT_MASK           0x0000FFFF
#define NEW_DOWN_STATE_MASK         0x80000000
#define PREVIOUS_DOWN_STATE_MASK    0x40000000
#define ALT_KEY_DOWN_MASK           0x20000000

#define HIGH_BIT_MASK_SHORT         0x8000

namespace WebCore {

KeyEvent::KeyEvent(HWND hWnd, WPARAM wParam, LPARAM lParam)
    : m_text(QChar(wParam))
    , m_unmodifiedText(QChar(wParam))
    , m_keyIdentifier(QString().sprintf("U+%06X", toupper(wParam)))
    , m_isKeyUp(!(lParam & NEW_DOWN_STATE_MASK))
    , m_autoRepeat(lParam & REPEAT_COUNT_MASK)
    , m_WindowsKeyCode(wParam)
    , m_isKeypad(false) // FIXME
    , m_shiftKey(GetAsyncKeyState(VK_SHIFT) & HIGH_BIT_MASK_SHORT)
    , m_ctrlKey(GetAsyncKeyState(VK_CONTROL) & HIGH_BIT_MASK_SHORT)
    , m_altKey(lParam & ALT_KEY_DOWN_MASK)
    , m_metaKey(lParam & ALT_KEY_DOWN_MASK) // FIXME: Is this right?
{
    if (!m_shiftKey)
        m_text = String(QChar(tolower(wParam)));
}   

}
