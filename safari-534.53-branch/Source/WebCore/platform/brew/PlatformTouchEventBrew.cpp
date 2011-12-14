/*
 * Copyright 2010, Company 100, Inc. All rights reserved.
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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "PlatformTouchEvent.h"

#include <AEEEvent.h>
#include <AEEPointerHelpers.h>
#include <AEEVCodes.h>

#if ENABLE(TOUCH_EVENTS)

namespace WebCore {

PlatformTouchEvent::PlatformTouchEvent(AEEEvent event, uint16 wParam, uint32 dwParam)
    : m_metaKey(false)
{
    PlatformTouchPoint::State state;

    switch (event) {
    case EVT_POINTER_DOWN:
        m_type = TouchStart;
        state = PlatformTouchPoint::TouchPressed;
        break;
    case EVT_POINTER_UP:
        m_type = TouchEnd;
        state = PlatformTouchPoint::TouchReleased;
        break;
    case EVT_POINTER_MOVE:
    case EVT_POINTER_STALE_MOVE:
        m_type = TouchMove;
        state = PlatformTouchPoint::TouchMoved;
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    char* dwParamStr = reinterpret_cast<char*>(dwParam);
    int x, y;
    AEE_POINTER_GET_XY(dwParamStr, &x, &y);
    IntPoint windowPos = IntPoint(x, y);

    int id = AEE_POINTER_GET_PTRID(dwParamStr);
    m_touchPoints.append(PlatformTouchPoint(id, windowPos, state));

    uint32 keyModifiers = AEE_POINTER_GET_KEY_MODIFIERS(dwParamStr);
    m_altKey   = keyModifiers & (KB_LALT | KB_RALT);
    m_shiftKey = keyModifiers & (KB_LSHIFT | KB_RSHIFT);
    m_ctrlKey  = keyModifiers & (KB_LCTRL | KB_RCTRL);
}

}

#endif
