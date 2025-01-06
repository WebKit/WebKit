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

#if ENABLE(TOUCH_EVENTS) && PLATFORM(IOS_FAMILY)

#import "EventNames.h"
#import "PlatformMouseEvent.h"

namespace WebCore {

static const AtomString& pointerEventType(PlatformTouchPoint::TouchPhaseType phase)
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

Ref<PointerEvent> PointerEvent::create(const PlatformTouchEvent& event, const Vector<Ref<PointerEvent>>& coalescedEvents, const Vector<Ref<PointerEvent>>& predictedEvents, unsigned index, bool isPrimary, Ref<WindowProxy>&& view, const IntPoint& touchDelta)
{
    const auto& type = pointerEventType(event.touchPhaseAtIndex(index));

    return adoptRef(*new PointerEvent(type, event, coalescedEvents, predictedEvents, typeCanBubble(type), typeIsCancelable(type), index, isPrimary, WTFMove(view), touchDelta));
}

Ref<PointerEvent> PointerEvent::create(const PlatformTouchEvent& event, const Vector<Ref<PointerEvent>>& coalescedEvents, const Vector<Ref<PointerEvent>>& predictedEvents, CanBubble canBubble, IsCancelable isCancelable, unsigned index, bool isPrimary, Ref<WindowProxy>&& view, const IntPoint& touchDelta)
{
    const auto& type = pointerEventType(event.touchPhaseAtIndex(index));

    return adoptRef(*new PointerEvent(type, event, coalescedEvents, predictedEvents, canBubble, isCancelable, index, isPrimary, WTFMove(view), touchDelta));
}

Ref<PointerEvent> PointerEvent::create(const AtomString& type, const PlatformTouchEvent& event, const Vector<Ref<PointerEvent>>& coalescedEvents, const Vector<Ref<PointerEvent>>& predictedEvents, unsigned index, bool isPrimary, Ref<WindowProxy>&& view, const IntPoint& touchDelta)
{
    return adoptRef(*new PointerEvent(type, event, coalescedEvents, predictedEvents, typeCanBubble(type), typeIsCancelable(type), index, isPrimary, WTFMove(view), touchDelta));
}

PointerEvent::PointerEvent(const AtomString& type, const PlatformTouchEvent& event, const Vector<Ref<PointerEvent>>& coalescedEvents, const Vector<Ref<PointerEvent>>& predictedEvents, CanBubble canBubble, IsCancelable isCancelable, unsigned index, bool isPrimary, Ref<WindowProxy>&& view, const IntPoint& touchDelta)
    : MouseEvent(EventInterfaceType::PointerEvent, type, canBubble, isCancelable, typeIsComposed(type), event.timestamp().approximateMonotonicTime(), WTFMove(view), 0,
        event.touchLocationInRootViewAtIndex(index), event.touchLocationInRootViewAtIndex(index), touchDelta.x(), touchDelta.y(), event.modifiers(), buttonForType(type), buttonsForType(type), nullptr, 0, SyntheticClickType::NoTap, { }, { }, IsSimulated::No, IsTrusted::Yes)
    , m_pointerId(event.touchIdentifierAtIndex(index))
    , m_width(2 * event.radiusXAtIndex(index))
    , m_height(2 * event.radiusYAtIndex(index))
    , m_pressure(event.forceAtIndex(index))
    , m_pointerType(event.touchTypeAtIndex(index) == PlatformTouchPoint::TouchType::Stylus ? penPointerEventType() : touchPointerEventType())
    , m_isPrimary(isPrimary)
    , m_coalescedEvents(coalescedEvents)
    , m_predictedEvents(predictedEvents)
{
    m_azimuthAngle = event.azimuthAngleAtIndex(index);
    m_altitudeAngle = m_pointerType == penPointerEventType() ? event.altitudeAngleAtIndex(index) : piOverTwoDouble;
    auto tilt = tiltFromAngle(m_altitudeAngle, m_azimuthAngle);
    m_tiltX = tilt.tiltX;
    m_tiltY = tilt.tiltY;
}

} // namespace WebCore

#endif // ENABLE(TOUCH_EVENTS) && PLATFORM(IOS_FAMILY)
