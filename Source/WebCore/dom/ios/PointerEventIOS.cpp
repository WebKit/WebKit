/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#import "PointerEvent.h"

#if ENABLE(POINTER_EVENTS) && ENABLE(TOUCH_EVENTS) && PLATFORM(IOS_FAMILY)

#import "EventNames.h"

namespace WebCore {

static AtomicString eventType(PlatformTouchPoint::TouchPhaseType phase)
{
    switch (phase) {
    case PlatformTouchPoint::TouchPhaseBegan:
        return eventNames().pointerdownEvent;
    case PlatformTouchPoint::TouchPhaseMoved:
        return eventNames().pointermoveEvent;
    case PlatformTouchPoint::TouchPhaseStationary:
        return eventNames().pointermoveEvent;
    case PlatformTouchPoint::TouchPhaseEnded:
        return eventNames().pointerupEvent;
    case PlatformTouchPoint::TouchPhaseCancelled:
        return eventNames().pointercancelEvent;
    }
    ASSERT_NOT_REACHED();
    return nullAtom();
}

static PointerEvent::IsCancelable phaseIsCancelable(PlatformTouchPoint::TouchPhaseType phase)
{
    if (phase == PlatformTouchPoint::TouchPhaseCancelled)
        return PointerEvent::IsCancelable::No;
    return PointerEvent::IsCancelable::Yes;
}

Ref<PointerEvent> PointerEvent::create(const PlatformTouchEvent& event, unsigned index, bool isPrimary, Ref<WindowProxy>&& view)
{
    auto phase = event.touchPhaseAtIndex(index);
    return adoptRef(*new PointerEvent(eventType(phase), event, phaseIsCancelable(phase), index, isPrimary, WTFMove(view)));
}

PointerEvent::PointerEvent(const AtomicString& type, const PlatformTouchEvent& event, IsCancelable isCancelable, unsigned index, bool isPrimary, Ref<WindowProxy>&& view)
    : MouseEvent(type, CanBubble::Yes, isCancelable, IsComposed::Yes, event.timestamp().approximateMonotonicTime(), WTFMove(view), 0, event.touchLocationAtIndex(index), event.touchLocationAtIndex(index), { }, event.modifiers(), 0, 0, nullptr, 0, 0, nullptr, IsSimulated::No, IsTrusted::Yes)
    , m_pointerId(event.touchIdentifierAtIndex(index))
    , m_width(2 * event.radiusXAtIndex(index))
    , m_height(2 * event.radiusYAtIndex(index))
    , m_pointerType(event.touchTypeAtIndex(index) == PlatformTouchPoint::TouchType::Stylus ? "pen"_s : "touch"_s)
    , m_isPrimary(isPrimary)
{
}

} // namespace WebCore

#endif // ENABLE(POINTER_EVENTS) && ENABLE(TOUCH_EVENTS) && PLATFORM(IOS_FAMILY)
