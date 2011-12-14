/*
 * Copyright 2010, The Android Open Source Project
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
#include <wtf/CurrentTime.h>

#if ENABLE(TOUCH_EVENTS)

namespace WebCore {

// These values should be kept in sync with those defined in the android.view.KeyEvent class from the Android SDK.
enum AndroidMetaKeyState {
    META_SHIFT_ON = 0x01,
    META_ALT_ON = 0x02,
    META_SYM_ON = 0x04
};

PlatformTouchEvent::PlatformTouchEvent(const Vector<int>& ids, const Vector<IntPoint>& windowPoints, TouchEventType type, const Vector<PlatformTouchPoint::State>& states, int metaState)
    : m_type(type)
    , m_metaKey(false)
    , m_timestamp(WTF::currentTime())
{
    m_touchPoints.reserveCapacity(windowPoints.size());
    for (unsigned c = 0; c < windowPoints.size(); c++)
        m_touchPoints.append(PlatformTouchPoint(ids[c], windowPoints[c], states[c]));

    m_altKey = metaState & META_ALT_ON;
    m_shiftKey = metaState & META_SHIFT_ON;
    m_ctrlKey = metaState & META_SYM_ON;
}

}

#endif
