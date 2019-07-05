/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#import "config.h"
#import "MouseEvent.h"

#if ENABLE(TOUCH_EVENTS) && PLATFORM(IOS_FAMILY)

#import "EventNames.h"

namespace WebCore {

static AtomString mouseEventType(PlatformTouchPoint::TouchPhaseType phase)
{
    switch (phase) {
    case PlatformTouchPoint::TouchPhaseBegan:
        return eventNames().mousedownEvent;
    case PlatformTouchPoint::TouchPhaseMoved:
    case PlatformTouchPoint::TouchPhaseStationary:
        return eventNames().mousemoveEvent;
    case PlatformTouchPoint::TouchPhaseEnded:
    case PlatformTouchPoint::TouchPhaseCancelled:
        return eventNames().mouseupEvent;
    }
    ASSERT_NOT_REACHED();
    return nullAtom();
}

Ref<MouseEvent> MouseEvent::create(const PlatformTouchEvent& event, unsigned index, Ref<WindowProxy>&& view, IsCancelable cancelable)
{
    return adoptRef(*new MouseEvent(mouseEventType(event.touchPhaseAtIndex(index)), CanBubble::Yes, cancelable, IsComposed::Yes, event.timestamp().approximateMonotonicTime(), WTFMove(view), 0, event.touchLocationAtIndex(index), event.touchLocationAtIndex(index), { }, event.modifiers(), 0, 0, nullptr, 0, 0, nullptr, IsSimulated::No, IsTrusted::Yes));
}

} // namespace WebCore

#endif // ENABLE(TOUCH_EVENTS) && PLATFORM(IOS_FAMILY)
