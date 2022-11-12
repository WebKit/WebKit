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
}

namespace WebKit {

enum class GestureWasCancelled : bool { No, Yes };

enum class WebMouseEventButton : int32_t {
    LeftButton = 0,
    MiddleButton,
    RightButton,
    NoButton = -2
};
WebMouseEventButton mouseButton(const WebCore::NavigationAction&);

enum class WebMouseEventSyntheticClickType : uint32_t {
    NoTap,
    OneFingerTap,
    TwoFingerTap
};
WebMouseEventSyntheticClickType syntheticClickType(const WebCore::NavigationAction&);

class WebMouseEvent : public WebEvent {
public:

    WebMouseEvent();

#if PLATFORM(MAC)
    WebMouseEvent(WebEvent&&, WebMouseEventButton, unsigned short buttons, const WebCore::IntPoint& positionInView, const WebCore::IntPoint& globalPosition, float deltaX, float deltaY, float deltaZ, int clickCount, double force, WebMouseEventSyntheticClickType = WebMouseEventSyntheticClickType::NoTap, int eventNumber = -1, int menuType = 0, GestureWasCancelled = GestureWasCancelled::No);
#elif PLATFORM(GTK)
    WebMouseEvent(WebEvent&&, WebMouseEventButton, unsigned short buttons, const WebCore::IntPoint& positionInView, const WebCore::IntPoint& globalPosition, float deltaX, float deltaY, float deltaZ, int clickCount, double force = 0, WebMouseEventSyntheticClickType = WebMouseEventSyntheticClickType::NoTap, WebCore::PlatformMouseEvent::IsTouch m_isTouchEvent = WebCore::PlatformMouseEvent::IsTouch::No, WebCore::PointerID = WebCore::mousePointerID, const String& pointerType = WebCore::mousePointerEventType(), GestureWasCancelled = GestureWasCancelled::No);
#else
    WebMouseEvent(WebEvent&&, WebMouseEventButton, unsigned short buttons, const WebCore::IntPoint& positionInView, const WebCore::IntPoint& globalPosition, float deltaX, float deltaY, float deltaZ, int clickCount, double force = 0, WebMouseEventSyntheticClickType = WebMouseEventSyntheticClickType::NoTap, WebCore::PointerID = WebCore::mousePointerID, const String& pointerType = WebCore::mousePointerEventType(), GestureWasCancelled = GestureWasCancelled::No);
#endif

    WebMouseEventButton button() const { return static_cast<WebMouseEventButton>(m_button); }
    unsigned short buttons() const { return m_buttons; }
    const WebCore::IntPoint& position() const { return m_position; } // Relative to the view.
    const WebCore::IntPoint& globalPosition() const { return m_globalPosition; }
    float deltaX() const { return m_deltaX; }
    float deltaY() const { return m_deltaY; }
    float deltaZ() const { return m_deltaZ; }
    int32_t clickCount() const { return m_clickCount; }
#if PLATFORM(MAC)
    int32_t eventNumber() const { return m_eventNumber; }
    int32_t menuTypeForEvent() const { return m_menuTypeForEvent; }
#elif PLATFORM(GTK)
    WebCore::PlatformMouseEvent::IsTouch isTouchEvent() const { return m_isTouchEvent; }
#endif
    double force() const { return m_force; }
    WebMouseEventSyntheticClickType syntheticClickType() const { return m_syntheticClickType; }
    WebCore::PointerID pointerId() const { return m_pointerId; }
    const String& pointerType() const { return m_pointerType; }
    GestureWasCancelled gestureWasCancelled() const { return m_gestureWasCancelled; }

private:
    static bool isMouseEventType(Type);

    WebMouseEventButton m_button { WebMouseEventButton::NoButton };
    unsigned short m_buttons { 0 };
    WebCore::IntPoint m_position; // Relative to the view.
    WebCore::IntPoint m_globalPosition;
    float m_deltaX { 0 };
    float m_deltaY { 0 };
    float m_deltaZ { 0 };
    int32_t m_clickCount { 0 };
#if PLATFORM(MAC)
    int32_t m_eventNumber { -1 };
    int32_t m_menuTypeForEvent { 0 };
#elif PLATFORM(GTK)
    WebCore::PlatformMouseEvent::IsTouch m_isTouchEvent { WebCore::PlatformMouseEvent::IsTouch::No };
#endif
    double m_force { 0 };
    WebMouseEventSyntheticClickType m_syntheticClickType { WebMouseEventSyntheticClickType::NoTap };
    WebCore::PointerID m_pointerId { WebCore::mousePointerID };
    String m_pointerType { WebCore::mousePointerEventType() };
    GestureWasCancelled m_gestureWasCancelled { GestureWasCancelled::No };
};

} // namespace WebKit
