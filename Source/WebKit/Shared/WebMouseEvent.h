/*
 * Copyright (C) 2010-2020 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies)
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

#pragma once

// FIXME: We should probably move to making the WebCore/PlatformFooEvents trivial classes so that
// we can use them as the event type.

#include "WebEvent.h"
#include <WebCore/IntPoint.h>
#include <WebCore/PlatformMouseEvent.h>
#include <WebCore/PointerEventTypeNames.h>
#include <WebCore/PointerID.h>

namespace WebCore {
class NavigationAction;
enum class SyntheticClickType : uint8_t;
}

namespace WebKit {

enum class GestureWasCancelled : bool { No, Yes };

enum class WebMouseEventButton : int8_t {
    Left,
    Middle,
    Right,
    Back,
    Forward,
    None = -2,
};
WebMouseEventButton NODELETE mouseButton(const WebCore::NavigationAction&);

enum class WebMouseEventSyntheticClickType : uint8_t {
    NoTap,
    OneFingerTap,
    TwoFingerTap
};
WebMouseEventSyntheticClickType NODELETE syntheticClickType(const WebCore::NavigationAction&);
WebCore::SyntheticClickType NODELETE coreSyntheticClickType(WebMouseEventSyntheticClickType);

class WebMouseEvent;

// Field order matches WebEvent.serialization.in. Predicted events are deliberately absent because
// they are not sent over IPC; WebMouseEvent stores them separately.
struct WebMouseEventData {
    WebMouseEventButton button { WebMouseEventButton::None };
    unsigned short buttons { 0 };
    WebCore::DoublePoint position; // Relative to the view.
    WebCore::DoublePoint globalPosition;
    float deltaX { 0 };
    float deltaY { 0 };
    float deltaZ { 0 };
    int32_t clickCount { 0 };
    double force { 0 };
    WebEventInputSource inputSource { WebEventInputSource::UserDriven };
    WebCore::PlatformMouseEvent::CanInitiateDrag canInitiateDrag { WebCore::PlatformMouseEvent::CanInitiateDrag::Yes };
    WebMouseEventSyntheticClickType syntheticClickType { WebMouseEventSyntheticClickType::NoTap };
#if PLATFORM(MAC)
    int32_t eventNumber { -1 };
    int32_t menuTypeForEvent { 0 };
#elif PLATFORM(GTK)
    WebCore::PlatformMouseEvent::IsTouch isTouchEvent { WebCore::PlatformMouseEvent::IsTouch::No };
#endif
#if !PLATFORM(MAC)
    WebCore::PointerID pointerId { WebCore::mousePointerID };
    String pointerType { WebCore::mousePointerEventType() };
#endif
    GestureWasCancelled gestureWasCancelled { GestureWasCancelled::No };
    WebCore::DoublePoint unadjustedMovementDelta;
    Vector<Ref<WebMouseEvent>> coalescedEvents;
};

struct WebMouseEventInit {
    WebEventData event;
    WebMouseEventData mouse;
};

class WebMouseEvent : public WebEvent {
    WTF_MAKE_TZONE_ALLOCATED(WebMouseEvent);
public:
    static Ref<WebMouseEvent> create(WebEventData&&, WebMouseEventData&&);
    static Ref<WebMouseEvent> create(WebMouseEventInit&&);

    // Callers that mutate an event they did not create must copy first, since events are now shared
    // rather than copied by value. Shallow: nothing mutates the coalesced or predicted events.
    Ref<WebMouseEvent> copy() const;

    WebMouseEventButton button() const { return m_data.button; }
    unsigned short buttons() const { return m_data.buttons; }
    const WebCore::DoublePoint& position() const LIFETIME_BOUND { return m_data.position; } // Relative to the view.
    void setPosition(const WebCore::DoublePoint& position) { m_data.position = position; }
    const WebCore::DoublePoint& globalPosition() const LIFETIME_BOUND { return m_data.globalPosition; }
    float deltaX() const { return m_data.deltaX; }
    float deltaY() const { return m_data.deltaY; }
    float deltaZ() const { return m_data.deltaZ; }
    int32_t clickCount() const { return m_data.clickCount; }
#if PLATFORM(MAC)
    int32_t eventNumber() const { return m_data.eventNumber; }
    int32_t menuTypeForEvent() const { return m_data.menuTypeForEvent; }
#elif PLATFORM(GTK)
    WebCore::PlatformMouseEvent::IsTouch isTouchEvent() const { return m_data.isTouchEvent; }
#endif
    double force() const { return m_data.force; }
    WebEventInputSource inputSource() const { return m_data.inputSource; }
    WebCore::PlatformMouseEvent::CanInitiateDrag canInitiateDrag() const { return m_data.canInitiateDrag; }
    WebMouseEventSyntheticClickType syntheticClickType() const { return m_data.syntheticClickType; }
#if PLATFORM(MAC)
    // No constructor on this platform takes these, so the defaults are the only possible values.
    WebCore::PointerID pointerId() const { return WebCore::mousePointerID; }
    const String& pointerType() const LIFETIME_BOUND { return WebCore::mousePointerEventType(); }
#else
    WebCore::PointerID pointerId() const { return m_data.pointerId; }
    const String& pointerType() const LIFETIME_BOUND { return m_data.pointerType; }
#endif
    GestureWasCancelled gestureWasCancelled() const { return m_data.gestureWasCancelled; }
    // Unaccelerated pointer movement
    const WebCore::DoublePoint& unadjustedMovementDelta() const LIFETIME_BOUND { return m_data.unadjustedMovementDelta; }

    void setCoalescedEvents(const Vector<Ref<WebMouseEvent>>& coalescedEvents) { m_data.coalescedEvents = coalescedEvents; }
    const Vector<Ref<WebMouseEvent>>& coalescedEvents() const LIFETIME_BOUND { return m_data.coalescedEvents; }

    void setPredictedEvents(const Vector<Ref<WebMouseEvent>>& predictedEvents) { m_predictedEvents = predictedEvents; }
    const Vector<Ref<WebMouseEvent>>& predictedEvents() const LIFETIME_BOUND { return m_predictedEvents; }

    const WebMouseEventData& mouseData() const LIFETIME_BOUND { return m_data; }

    static bool NODELETE isMouseEventType(WebEventType);

protected:
    WebMouseEvent(WebEventData&&, WebMouseEventData&&);

private:
    WebMouseEventData m_data;
    // Not sent over IPC. See WebMouseEventData.
    Vector<Ref<WebMouseEvent>> m_predictedEvents;
};

} // namespace WebKit

SPECIALIZE_TYPE_TRAITS_BEGIN(WebKit::WebMouseEvent)
static bool isType(const WebKit::WebEvent& event) { return WebKit::WebMouseEvent::isMouseEventType(event.type()); }
SPECIALIZE_TYPE_TRAITS_END()
