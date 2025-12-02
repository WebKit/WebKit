/*
 * Copyright (C) 2023 Pengutronix e.K.
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
#include "PointerEvent.h"

#include "PlatformMouseEvent.h"

#if ENABLE(TOUCH_EVENTS)

#include "EventNames.h"
#include "PlatformTouchEvent.h"

namespace WebCore {

static const AtomString& pointerEventType(PlatformTouchPoint::State state)
{
    switch (state) {
    case PlatformTouchPoint::State::TouchPressed:
        return eventNames().pointerdownEvent;
    case PlatformTouchPoint::State::TouchMoved:
        return eventNames().pointermoveEvent;
    case PlatformTouchPoint::State::TouchStationary:
        return eventNames().pointermoveEvent;
    case PlatformTouchPoint::State::TouchReleased:
        return eventNames().pointerupEvent;
    case PlatformTouchPoint::State::TouchCancelled:
        return eventNames().pointercancelEvent;
    case PlatformTouchPoint::State::TouchStateEnd:
        break;
    }
    ASSERT_NOT_REACHED();
    return nullAtom();
}

Ref<PointerEvent> PointerEvent::create(const PlatformTouchEvent& event, const Vector<Ref<PointerEvent>>& coalescedEvents, const Vector<Ref<PointerEvent>>& predictedEvents, unsigned index, bool isPrimary, Ref<WindowProxy>&& view, const DoublePoint& touchDelta)
{
    const auto& type = pointerEventType(event.touchPoints().at(index).state());
    return adoptRef(*new PointerEvent(type, event, coalescedEvents, predictedEvents, typeCanBubble(type), typeIsCancelable(type), index, isPrimary, WTFMove(view), touchDelta));
}

Ref<PointerEvent> PointerEvent::create(const PlatformTouchEvent& event, const Vector<Ref<PointerEvent>>& coalescedEvents, const Vector<Ref<PointerEvent>>& predictedEvents, CanBubble canBubble, IsCancelable isCancelable, unsigned index, bool isPrimary, Ref<WindowProxy>&& view, const DoublePoint& touchDelta)
{
    const auto& type = pointerEventType(event.touchPoints().at(index).state());
    return adoptRef(*new PointerEvent(type, event, coalescedEvents, predictedEvents, canBubble, isCancelable, index, isPrimary, WTFMove(view), touchDelta));
}

Ref<PointerEvent> PointerEvent::create(const AtomString& type, const PlatformTouchEvent& event, const Vector<Ref<PointerEvent>>& coalescedEvents, const Vector<Ref<PointerEvent>>& predictedEvents, unsigned index, bool isPrimary, Ref<WindowProxy>&& view, const DoublePoint& touchDelta)
{
    return adoptRef(*new PointerEvent(type, event, coalescedEvents, predictedEvents, typeCanBubble(type), typeIsCancelable(type), index, isPrimary, WTFMove(view), touchDelta));
}

// According to the PointerEvents spec all active pointer ids have to be unique.
// Libinput on Linux assigns the ids of the touchpoints starting at 0, but
// the ids 0 and 1 are used for the pointer ids of mouse and pen/stylus.
static constexpr unsigned touchMinimumPointerId = WebCore::mousePointerID + 1;

PointerEvent::PointerEvent(const AtomString& type, const PlatformTouchEvent& event, const Vector<Ref<PointerEvent>>& coalescedEvents, const Vector<Ref<PointerEvent>>& predictedEvents, CanBubble canBubble, IsCancelable isCancelable, unsigned index, bool isPrimary, Ref<WindowProxy>&& view, const DoublePoint& touchDelta)
    : MouseEvent(EventInterfaceType::PointerEvent, type, canBubble, isCancelable, typeIsComposed(type), event.timestamp(), WTFMove(view), 0, event.touchPoints().at(index).pos(), event.touchPoints().at(index).pos(), touchDelta.x(), touchDelta.y(), event.modifiers(), buttonForType(type), buttonsForType(type), nullptr, 0, SyntheticClickType::NoTap, { }, { }, IsSimulated::No, IsTrusted::Yes)
    , m_pointerId(touchMinimumPointerId + event.touchPoints().at(index).id())
    , m_width(2 * event.touchPoints().at(index).radius().width())
    , m_height(2 * event.touchPoints().at(index).radius().height())
    , m_pressure(event.touchPoints().at(index).force())
    , m_pointerType(touchPointerEventType())
    , m_isPrimary(isPrimary)
    , m_coalescedEvents(coalescedEvents)
    , m_predictedEvents(predictedEvents)
{
}

} // namespace WebCore

#endif // ENABLE(TOUCH_EVENTS)
