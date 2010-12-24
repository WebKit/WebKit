/*
 * Copyright (C) 2009-2010 Samsung Electronics
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
#include "PlatformTouchEvent.h"

#include "ewk_frame.h"

#if ENABLE(TOUCH_EVENTS)

namespace WebCore {

PlatformTouchEvent::PlatformTouchEvent(Eina_List* points, const IntPoint pos, TouchEventType type, int metaState)
    : m_type(type)
    , m_ctrlKey(false)
    , m_altKey(false)
    , m_shiftKey(false)
    , m_metaKey(false)
{
    void* point;

    EINA_LIST_FREE(points, point) {
        Ewk_Touch_Point* p = static_cast<Ewk_Touch_Point*>(point);
        IntPoint pnt = IntPoint(p->x - pos.x(), p->y - pos.y());

        PlatformTouchPoint::State state = PlatformTouchPoint::TouchPressed;
        switch (p->state) {
        case EWK_TOUCH_POINT_PRESSED:
            state = PlatformTouchPoint::TouchPressed;
            break;
        case EWK_TOUCH_POINT_RELEASED:
            state = PlatformTouchPoint::TouchReleased;
            break;
        case EWK_TOUCH_POINT_MOVED:
            state = PlatformTouchPoint::TouchMoved;
            break;
        case EWK_TOUCH_POINT_CANCELLED:
            state = PlatformTouchPoint::TouchCancelled;
            break;
        }

        m_touchPoints.append(PlatformTouchPoint(p->id, pnt, state));
    }

    // FIXME: We don't support metaState for now.
}

}

#endif
