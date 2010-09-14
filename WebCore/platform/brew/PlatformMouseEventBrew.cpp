/*
 * Copyright (C) 2009 Company 100, Inc. All rights reserved.
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
#include "PlatformMouseEvent.h"

#include <AEEEvent.h>
#include <AEEPointerHelpers.h>
#include <AEEStdDef.h>
#include <AEEVCodes.h>

namespace WebCore {

PlatformMouseEvent::PlatformMouseEvent(AEEEvent event, uint16 wParam, uint32 dwParam)
{
    switch (event) {
    case EVT_POINTER_DOWN:
        m_eventType = MouseEventPressed;
        break;
    case EVT_POINTER_UP:
        m_eventType = MouseEventReleased;
        break;
    case EVT_POINTER_MOVE:
    case EVT_POINTER_STALE_MOVE:
        m_eventType = MouseEventMoved;
        break;
    default:
        m_eventType = MouseEventMoved;
        break;
    };

    char* dwParamStr = reinterpret_cast<char*>(dwParam);

    int x, y;
    AEE_POINTER_GET_XY(dwParamStr, &x, &y);
    m_position = IntPoint(x, y);
    // Use IDisplay, so position and global position are the same.
    m_globalPosition = m_position;

    uint32 keyModifiers = AEE_POINTER_GET_KEY_MODIFIERS(dwParamStr);
    m_shiftKey = keyModifiers & (KB_LSHIFT | KB_RSHIFT);
    m_ctrlKey  = keyModifiers & (KB_LCTRL | KB_RCTRL);
    m_altKey   = keyModifiers & (KB_LALT | KB_RALT);
    m_metaKey  = m_altKey;

    // AEE_POINTER_GET_MOUSE_MODIFIERS(dwParamStr) always returns 0,
    // so it is impossible to know which button is pressed or released.
    // Just use LeftButton because Brew MP usually runs on touch device.
    m_button = LeftButton;

    // AEE_POINTER_GET_TIME returns milliseconds
    m_timestamp = AEE_POINTER_GET_TIME(dwParamStr) * 0.001;

    m_clickCount = AEE_POINTER_GET_CLICKCOUNT(dwParamStr);
}

} // namespace WebCore

