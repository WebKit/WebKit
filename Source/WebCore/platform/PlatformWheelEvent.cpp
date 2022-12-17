/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "PlatformWheelEvent.h"

#include "Scrollbar.h"
#include <wtf/text/TextStream.h>

#if ENABLE(MAC_GESTURE_EVENTS)
#include "PlatformGestureEventMac.h"
#endif

namespace WebCore {

#if ENABLE(MAC_GESTURE_EVENTS)

PlatformWheelEvent PlatformWheelEvent::createFromGesture(const PlatformGestureEvent& platformGestureEvent, double deltaY)
{
    // This tries to match as much of the behavior of `WebKit::WebEventFactory::createWebWheelEvent` as
    // possible assuming `-[NSEvent hasPreciseScrollingDeltas]` and no `-[NSEvent _scrollCount]`.

    double deltaX = 0;
    double wheelTicksX = 0;
    double wheelTicksY = deltaY / static_cast<float>(Scrollbar::pixelsPerLineStep());
    bool shiftKey = platformGestureEvent.modifiers().contains(PlatformEvent::Modifier::ShiftKey);
    bool ctrlKey = true;
    bool altKey = platformGestureEvent.modifiers().contains(PlatformEvent::Modifier::AltKey);
    bool metaKey = platformGestureEvent.modifiers().contains(PlatformEvent::Modifier::MetaKey);
    PlatformWheelEvent platformWheelEvent(platformGestureEvent.pos(), platformGestureEvent.globalPosition(), deltaX, deltaY, wheelTicksX, wheelTicksY, ScrollByPixelWheelEvent, shiftKey, ctrlKey, altKey, metaKey);

    // PlatformEvent
    platformWheelEvent.m_timestamp = platformGestureEvent.timestamp();

    // PlatformWheelEvent
    platformWheelEvent.m_hasPreciseScrollingDeltas = true;

#if ENABLE(KINETIC_SCROLLING)
    switch (platformGestureEvent.type()) {
    case PlatformEvent::GestureStart:
        platformWheelEvent.m_phase = PlatformWheelEventPhase::Began;
        break;
    case PlatformEvent::GestureChange:
        platformWheelEvent.m_phase = PlatformWheelEventPhase::Changed;
        break;
    case PlatformEvent::GestureEnd:
        platformWheelEvent.m_phase = PlatformWheelEventPhase::Ended;
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }
#endif // ENABLE(KINETIC_SCROLLING)

#if PLATFORM(COCOA)
    platformWheelEvent.m_ioHIDEventTimestamp = platformWheelEvent.m_timestamp;
    platformWheelEvent.m_rawPlatformDelta = platformWheelEvent.m_rawPlatformDelta;
    platformWheelEvent.m_unacceleratedScrollingDeltaY = deltaY;
#endif // PLATFORM(COCOA)

    return platformWheelEvent;
}

#endif // ENABLE(MAC_GESTURE_EVENTS)

TextStream& operator<<(TextStream& ts, PlatformWheelEventPhase phase)
{
    switch (phase) {
    case PlatformWheelEventPhase::None: ts << "none"; break;
#if ENABLE(KINETIC_SCROLLING)
    case PlatformWheelEventPhase::Began: ts << "began"; break;
    case PlatformWheelEventPhase::Stationary: ts << "stationary"; break;
    case PlatformWheelEventPhase::Changed: ts << "changed"; break;
    case PlatformWheelEventPhase::Ended: ts << "ended"; break;
    case PlatformWheelEventPhase::Cancelled: ts << "cancelled"; break;
    case PlatformWheelEventPhase::MayBegin: ts << "mayBegin"; break;
#endif
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, const PlatformWheelEvent& event)
{
    ts << "PlatformWheelEvent " << &event << " at " << event.position() << " deltaX " << event.deltaX() << " deltaY " << event.deltaY();
    ts << " phase \"" << event.phase() << "\" momentum phase \"" << event.momentumPhase() << "\"";
    ts << " velocity " << event.scrollingVelocity();

    return ts;
}

TextStream& operator<<(TextStream& ts, EventHandling steps)
{
    switch (steps) {
    case EventHandling::DispatchedToDOM: ts << "dispatched to DOM"; break;
    case EventHandling::DefaultPrevented: ts << "default prevented"; break;
    case EventHandling::DefaultHandled: ts << "default handled"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, WheelScrollGestureState state)
{
    switch (state) {
    case WheelScrollGestureState::Blocking: ts << "blocking"; break;
    case WheelScrollGestureState::NonBlocking: ts << "non-blocking"; break;
    }
    return ts;
}

} // namespace WebCore
