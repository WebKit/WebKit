/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebTouchEvent.h"

#if ENABLE(TOUCH_EVENTS)

#include "ArgumentCoders.h"

namespace WebKit {

#if !PLATFORM(IOS_FAMILY)

WebTouchEvent::WebTouchEvent(WebEvent&& event, Vector<WebPlatformTouchPoint>&& touchPoints, Vector<WebTouchEvent>&& coalescedEvents, Vector<WebTouchEvent>&& predictedEvents)
    : WebEvent(WTFMove(event))
    , m_touchPoints(WTFMove(touchPoints))
    , m_coalescedEvents(WTFMove(coalescedEvents))
    , m_predictedEvents(WTFMove(predictedEvents))
{
    ASSERT(isTouchEventType(type()));
}

bool WebTouchEvent::isTouchEventType(WebEventType type)
{
    return type == WebEventType::TouchStart || type == WebEventType::TouchMove || type == WebEventType::TouchEnd || type == WebEventType::TouchCancel;
}

#endif // !PLATFORM(IOS_FAMILY)

bool WebTouchEvent::allTouchPointsAreReleased() const
{
    for (const auto& touchPoint : touchPoints()) {
        if (touchPoint.state() != WebPlatformTouchPoint::State::Released && touchPoint.state() != WebPlatformTouchPoint::State::Cancelled)
            return false;
    }

    return true;
}

} // namespace WebKit

#endif // ENABLE(TOUCH_EVENTS)
