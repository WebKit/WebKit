/*
 * Copyright (C) 2010-2022 Apple Inc. All rights reserved.
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
#include "WebEventConversion.h"

#include "WebKeyboardEvent.h"
#include "WebMouseEvent.h"
#include "WebTouchEvent.h"
#include "WebWheelEvent.h"
#include <WebCore/PlatformMouseEvent.h>

#if ENABLE(MAC_GESTURE_EVENTS)
#include "WebGestureEvent.h"
#endif

namespace WebKit {

WebCore::MouseButton platform(WebMouseEventButton button)
{
    switch (button) {
    case WebMouseEventButton::None:
        return WebCore::MouseButton::None;
    case WebMouseEventButton::Left:
        return WebCore::MouseButton::Left;
    case WebMouseEventButton::Middle:
        return WebCore::MouseButton::Middle;
    case WebMouseEventButton::Right:
        return WebCore::MouseButton::Right;
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

WebMouseEventButton kit(WebCore::MouseButton button)
{
    switch (button) {
    case WebCore::MouseButton::None:
        return WebMouseEventButton::None;
    case WebCore::MouseButton::Left:
        return WebMouseEventButton::Left;
    case WebCore::MouseButton::Middle:
        return WebMouseEventButton::Middle;
    case WebCore::MouseButton::Right:
        return WebMouseEventButton::Right;
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

WebCore::PlatformEvent::Type platform(WebEventType type)
{
    switch (type) {
    // Mouse
    case WebEventType::MouseDown:
        return WebCore::PlatformEvent::Type::MousePressed;
    case WebEventType::MouseUp:
        return WebCore::PlatformEvent::Type::MouseReleased;
    case WebEventType::MouseMove:
        return WebCore::PlatformEvent::Type::MouseMoved;
    case WebEventType::MouseForceChanged:
        return WebCore::PlatformEvent::Type::MouseForceChanged;
    case WebEventType::MouseForceDown:
        return WebCore::PlatformEvent::Type::MouseForceDown;
    case WebEventType::MouseForceUp:
        return WebCore::PlatformEvent::Type::MouseForceUp;

    // Wheel
    case WebEventType::Wheel:
        return WebCore::PlatformEvent::Type::Wheel;

    // Keyboard
    case WebEventType::KeyDown:
        return WebCore::PlatformEvent::Type::KeyDown;
    case WebEventType::KeyUp:
        return WebCore::PlatformEvent::Type::KeyUp;
    case WebEventType::RawKeyDown:
        return WebCore::PlatformEvent::Type::RawKeyDown;
    case WebEventType::Char:
        return WebCore::PlatformEvent::Type::Char;

#if ENABLE(TOUCH_EVENTS)
    // Touch
    case WebEventType::TouchStart:
        return WebCore::PlatformEvent::Type::TouchStart;
    case WebEventType::TouchMove:
        return WebCore::PlatformEvent::Type::TouchMove;
    case WebEventType::TouchEnd:
        return WebCore::PlatformEvent::Type::TouchEnd;
    case WebEventType::TouchCancel:
        return WebCore::PlatformEvent::Type::TouchCancel;
#endif

#if ENABLE(MAC_GESTURE_EVENTS)
    // Gesture
    case WebEventType::GestureStart:
        return WebCore::PlatformEvent::Type::GestureStart;
    case WebEventType::GestureChange:
        return WebCore::PlatformEvent::Type::GestureChange;
    case WebEventType::GestureEnd:
        return WebCore::PlatformEvent::Type::GestureEnd;
#endif

    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

WebEventType kit(WebCore::PlatformEvent::Type type)
{
    switch (type) {
    // Mouse
    case WebCore::PlatformEvent::Type::MousePressed:
        return WebEventType::MouseDown;
    case WebCore::PlatformEvent::Type::MouseReleased:
        return WebEventType::MouseUp;
    case WebCore::PlatformEvent::Type::MouseMoved:
        return WebEventType::MouseMove;
    case WebCore::PlatformEvent::Type::MouseForceChanged:
        return WebEventType::MouseForceChanged;
    case WebCore::PlatformEvent::Type::MouseForceDown:
        return WebEventType::MouseForceDown;
    case WebCore::PlatformEvent::Type::MouseForceUp:
        return WebEventType::MouseForceUp;

    // Wheel
    case WebCore::PlatformEvent::Type::Wheel:
        return WebEventType::Wheel;

    // Keyboard
    case WebCore::PlatformEvent::Type::KeyDown:
        return WebEventType::KeyDown;
    case WebCore::PlatformEvent::Type::KeyUp:
        return WebEventType::KeyUp;
    case WebCore::PlatformEvent::Type::RawKeyDown:
        return WebEventType::RawKeyDown;
    case WebCore::PlatformEvent::Type::Char:
        return WebEventType::Char;

#if ENABLE(TOUCH_EVENTS)
    // Touch
    case WebCore::PlatformEvent::Type::TouchStart:
        return WebEventType::TouchStart;
    case WebCore::PlatformEvent::Type::TouchMove:
        return WebEventType::TouchMove;
    case WebCore::PlatformEvent::Type::TouchEnd:
        return WebEventType::TouchEnd;
    case WebCore::PlatformEvent::Type::TouchCancel:
        return WebEventType::TouchCancel;
#endif

#if ENABLE(MAC_GESTURE_EVENTS)
    // Gesture
    case WebCore::PlatformEvent::Type::GestureStart:
        return WebEventType::GestureStart;
    case WebCore::PlatformEvent::Type::GestureChange:
        return WebEventType::GestureChange;
    case WebCore::PlatformEvent::Type::GestureEnd:
        return WebEventType::GestureEnd;
#endif

    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

OptionSet<WebCore::PlatformEvent::Modifier> platform(OptionSet<WebEventModifier> modifiers)
{
    OptionSet<WebCore::PlatformEvent::Modifier> result;
    if (modifiers.contains(WebEventModifier::ShiftKey))
        result.add(WebCore::PlatformEvent::Modifier::ShiftKey);
    if (modifiers.contains(WebEventModifier::ControlKey))
        result.add(WebCore::PlatformEvent::Modifier::ControlKey);
    if (modifiers.contains(WebEventModifier::AltKey))
        result.add(WebCore::PlatformEvent::Modifier::AltKey);
    if (modifiers.contains(WebEventModifier::MetaKey))
        result.add(WebCore::PlatformEvent::Modifier::MetaKey);
    if (modifiers.contains(WebEventModifier::CapsLockKey))
        result.add(WebCore::PlatformEvent::Modifier::CapsLockKey);
    return result;
}

OptionSet<WebEventModifier> kit(OptionSet<WebCore::PlatformEvent::Modifier> modifiers)
{
    OptionSet<WebEventModifier> result;
    if (modifiers.contains(WebCore::PlatformEvent::Modifier::ShiftKey))
        result.add(WebEventModifier::ShiftKey);
    if (modifiers.contains(WebCore::PlatformEvent::Modifier::ControlKey))
        result.add(WebEventModifier::ControlKey);
    if (modifiers.contains(WebCore::PlatformEvent::Modifier::AltKey))
        result.add(WebEventModifier::AltKey);
    if (modifiers.contains(WebCore::PlatformEvent::Modifier::MetaKey))
        result.add(WebEventModifier::MetaKey);
    if (modifiers.contains(WebCore::PlatformEvent::Modifier::CapsLockKey))
        result.add(WebEventModifier::CapsLockKey);
    return result;
}

static double forceForEvent(const WebMouseEvent& webEvent)
{
    switch (webEvent.type()) {
    case WebEventType::MouseDown:
        return WebCore::ForceAtClick;
    case WebEventType::MouseForceDown:
        return WebCore::ForceAtForceClick;
    case WebEventType::MouseMove:
    case WebEventType::MouseForceChanged:
        return webEvent.force();
    case WebEventType::MouseUp:
    case WebEventType::MouseForceUp:
        return 0;
    default:
        ASSERT_NOT_REACHED();
        return 0;
    }
}

class WebKit2PlatformMouseEvent : public WebCore::PlatformMouseEvent {
public:
    WebKit2PlatformMouseEvent(const WebMouseEvent& webEvent)
    {
        // PlatformEvent
        m_type = platform(webEvent.type());
        m_modifiers = platform(webEvent.modifiers());
        m_timestamp = webEvent.timestamp();
        m_authorizationToken = webEvent.authorizationToken();

        // PlatformMouseEvent
        m_button = platform(webEvent.button());
        m_buttons = webEvent.buttons();

        m_position = webEvent.position();
        m_movementDelta = WebCore::DoublePoint(webEvent.deltaX(), webEvent.deltaY());
        m_unadjustedMovementDelta = webEvent.unadjustedMovementDelta();
        m_globalPosition = webEvent.globalPosition();
        m_clickCount = webEvent.clickCount();
        m_force = forceForEvent(webEvent);
        m_coalescedEvents = WTF::map(webEvent.coalescedEvents(), [&](const auto& event) {
            return platform(event);
        });
        m_predictedEvents = WTF::map(webEvent.predictedEvents(), [&](const auto& event) {
            return platform(event);
        });

#if PLATFORM(MAC)
        m_eventNumber = webEvent.eventNumber();
        m_menuTypeForEvent = webEvent.menuTypeForEvent();
#elif PLATFORM(GTK)
        m_isTouchEvent = webEvent.isTouchEvent();
#elif PLATFORM(WPE)
        m_syntheticClickType = static_cast<WebCore::SyntheticClickType>(webEvent.syntheticClickType());
#endif
        m_modifierFlags = 0;
        if (webEvent.shiftKey())
            m_modifierFlags |= static_cast<unsigned>(WebEventModifier::ShiftKey);
        if (webEvent.controlKey())
            m_modifierFlags |= static_cast<unsigned>(WebEventModifier::ControlKey);
        if (webEvent.altKey())
            m_modifierFlags |= static_cast<unsigned>(WebEventModifier::AltKey);
        if (webEvent.metaKey())
            m_modifierFlags |= static_cast<unsigned>(WebEventModifier::MetaKey);

        m_pointerId = webEvent.pointerId();
        m_pointerType = webEvent.pointerType();
    }
};

WebCore::PlatformMouseEvent platform(const WebMouseEvent& webEvent)
{
    return WebKit2PlatformMouseEvent(webEvent);
}

class WebKit2PlatformWheelEvent : public WebCore::PlatformWheelEvent {
public:
    WebKit2PlatformWheelEvent(const WebWheelEvent& webEvent)
    {
        // PlatformEvent
        m_type = platform(webEvent.type());
        m_modifiers = platform(webEvent.modifiers());
        m_timestamp = webEvent.timestamp();

        // PlatformWheelEvent
        m_position = webEvent.position();
        m_globalPosition = webEvent.globalPosition();
        m_deltaX = webEvent.delta().width();
        m_deltaY = webEvent.delta().height();
        m_wheelTicksX = webEvent.wheelTicks().width();
        m_wheelTicksY = webEvent.wheelTicks().height();
        m_granularity = (webEvent.granularity() == WebWheelEvent::ScrollByPageWheelEvent) ? WebCore::ScrollByPageWheelEvent : WebCore::ScrollByPixelWheelEvent;
        m_directionInvertedFromDevice = webEvent.directionInvertedFromDevice();
#if ENABLE(KINETIC_SCROLLING)
        m_phase = static_cast<WebCore::PlatformWheelEventPhase>(webEvent.phase());
        m_momentumPhase = static_cast<WebCore::PlatformWheelEventPhase>(webEvent.momentumPhase());
#endif
#if PLATFORM(COCOA) || PLATFORM(GTK) || USE(LIBWPE)
        m_hasPreciseScrollingDeltas = webEvent.hasPreciseScrollingDeltas();
#endif
#if PLATFORM(COCOA)
        m_ioHIDEventTimestamp = webEvent.ioHIDEventTimestamp();
        m_rawPlatformDelta = webEvent.rawPlatformDelta();
        m_scrollCount = webEvent.scrollCount();
        m_unacceleratedScrollingDeltaX = webEvent.unacceleratedScrollingDelta().width();
        m_unacceleratedScrollingDeltaY = webEvent.unacceleratedScrollingDelta().height();
#endif
    }
};

WebCore::PlatformWheelEvent platform(const WebWheelEvent& webEvent)
{
    return WebKit2PlatformWheelEvent(webEvent);
}

class WebKit2PlatformKeyboardEvent : public WebCore::PlatformKeyboardEvent {
public:
    WebKit2PlatformKeyboardEvent(const WebKeyboardEvent& webEvent)
    {
        // PlatformEvent
        m_type = platform(webEvent.type());
        m_modifiers = platform(webEvent.modifiers());
        m_timestamp = webEvent.timestamp();

        // PlatformKeyboardEvent
        m_text = webEvent.text();
        m_unmodifiedText = webEvent.unmodifiedText();
        m_key = webEvent.key();
        m_code = webEvent.code();
        m_keyIdentifier = webEvent.keyIdentifier();
        m_windowsVirtualKeyCode = webEvent.windowsVirtualKeyCode();
#if USE(APPKIT) || PLATFORM(IOS_FAMILY) || PLATFORM(GTK) || USE(LIBWPE)
        m_handledByInputMethod = webEvent.handledByInputMethod();
#endif
#if PLATFORM(GTK) || USE(LIBWPE)
        m_preeditUnderlines = webEvent.preeditUnderlines();
        if (auto preeditSelectionRange = webEvent.preeditSelectionRange()) {
            m_preeditSelectionRangeStart = preeditSelectionRange->location;
            m_preeditSelectionRangeLength = preeditSelectionRange->length;
        }
#endif
#if USE(APPKIT) || PLATFORM(GTK)
        m_commands = webEvent.commands();
#endif
        m_autoRepeat = webEvent.isAutoRepeat();
        m_isKeypad = webEvent.isKeypad();
        m_isSystemKey = webEvent.isSystemKey();
        m_authorizationToken = webEvent.authorizationToken();
    }
};

WebCore::PlatformKeyboardEvent platform(const WebKeyboardEvent& webEvent)
{
    return WebKit2PlatformKeyboardEvent(webEvent);
}

#if ENABLE(TOUCH_EVENTS)

#if PLATFORM(IOS_FAMILY)

static WebCore::PlatformTouchPoint::TouchPhaseType touchEventType(const WebPlatformTouchPoint& webTouchPoint)
{
    switch (webTouchPoint.phase()) {
    case WebPlatformTouchPoint::State::Released:
        return WebCore::PlatformTouchPoint::TouchPhaseEnded;
    case WebPlatformTouchPoint::State::Pressed:
        return WebCore::PlatformTouchPoint::TouchPhaseBegan;
    case WebPlatformTouchPoint::State::Moved:
        return WebCore::PlatformTouchPoint::TouchPhaseMoved;
    case WebPlatformTouchPoint::State::Stationary:
        return WebCore::PlatformTouchPoint::TouchPhaseStationary;
    case WebPlatformTouchPoint::State::Cancelled:
        return WebCore::PlatformTouchPoint::TouchPhaseCancelled;
    }
}

static WebCore::PlatformTouchPoint::TouchType webPlatformTouchTypeToPlatform(const WebPlatformTouchPoint::TouchType& webTouchType)
{
    switch (webTouchType) {
    case WebPlatformTouchPoint::TouchType::Direct:
        return WebCore::PlatformTouchPoint::TouchType::Direct;
    case WebPlatformTouchPoint::TouchType::Stylus:
        return WebCore::PlatformTouchPoint::TouchType::Stylus;
    }
}

class WebKit2PlatformTouchPoint : public WebCore::PlatformTouchPoint {
public:
WebKit2PlatformTouchPoint(const WebPlatformTouchPoint& webTouchPoint)
    : PlatformTouchPoint(webTouchPoint.identifier(), DoublePoint(webTouchPoint.locationInRootView()), DoublePoint(webTouchPoint.locationInViewport()), touchEventType(webTouchPoint)
#if ENABLE(IOS_TOUCH_EVENTS)
        , webTouchPoint.radiusX(), webTouchPoint.radiusY(), webTouchPoint.rotationAngle(), webTouchPoint.twist(), webTouchPoint.force(), webTouchPoint.altitudeAngle(), webTouchPoint.azimuthAngle(), webPlatformTouchTypeToPlatform(webTouchPoint.touchType())
#endif
    )
{
}
};

#else

class WebKit2PlatformTouchPoint : public WebCore::PlatformTouchPoint {
public:
    WebKit2PlatformTouchPoint(const WebPlatformTouchPoint& webTouchPoint)
    {
        m_id = webTouchPoint.id();

        switch (webTouchPoint.state()) {
        case WebPlatformTouchPoint::State::Released:
            m_state = PlatformTouchPoint::TouchReleased;
            break;
        case WebPlatformTouchPoint::State::Pressed:
            m_state = PlatformTouchPoint::TouchPressed;
            break;
        case WebPlatformTouchPoint::State::Moved:
            m_state = PlatformTouchPoint::TouchMoved;
            break;
        case WebPlatformTouchPoint::State::Stationary:
            m_state = PlatformTouchPoint::TouchStationary;
            break;
        case WebPlatformTouchPoint::State::Cancelled:
            m_state = PlatformTouchPoint::TouchCancelled;
            break;
        default:
            ASSERT_NOT_REACHED();
        }

        m_screenPos = webTouchPoint.screenPosition();
        m_pos = webTouchPoint.position();
        m_radius = webTouchPoint.radius();
        m_force = webTouchPoint.force();
        m_rotationAngle = webTouchPoint.rotationAngle();
    }
};
#endif // PLATFORM(IOS_FAMILY)

class WebKit2PlatformTouchEvent : public WebCore::PlatformTouchEvent {
public:
    WebKit2PlatformTouchEvent(const WebTouchEvent& webEvent)
    {
        // PlatformEvent
        m_type = platform(webEvent.type());
        m_modifiers = platform(webEvent.modifiers());
        m_timestamp = webEvent.timestamp();

#if PLATFORM(IOS_FAMILY)
        m_touchPoints = WTF::map(webEvent.touchPoints(), [&](auto& touchPoint) -> WebCore::PlatformTouchPoint {
            return WebKit2PlatformTouchPoint(touchPoint);
        });

        m_coalescedEvents = WTF::map(webEvent.coalescedEvents(), [&](auto& event) {
            return platform(event);
        });

        m_predictedEvents = WTF::map(webEvent.predictedEvents(), [&](auto& event) {
            return platform(event);
        });

        m_gestureScale = webEvent.gestureScale();
        m_gestureRotation = webEvent.gestureRotation();
        m_canPreventNativeGestures = webEvent.canPreventNativeGestures();
        m_isGesture = webEvent.isGesture();
        m_isPotentialTap = webEvent.isPotentialTap();
        m_position = webEvent.position();
        m_globalPosition = webEvent.position();
        m_authorizationToken = webEvent.authorizationToken();
#else
        // PlatformTouchEvent
        for (size_t i = 0; i < webEvent.touchPoints().size(); ++i)
            m_touchPoints.append(WebKit2PlatformTouchPoint(webEvent.touchPoints().at(i)));
#endif //PLATFORM(IOS_FAMILY)
    }
};

WebCore::PlatformTouchEvent platform(const WebTouchEvent& webEvent)
{
    return WebKit2PlatformTouchEvent(webEvent);
}
#endif

#if ENABLE(MAC_GESTURE_EVENTS)
class WebKit2PlatformGestureEvent : public WebCore::PlatformGestureEvent {
public:
    WebKit2PlatformGestureEvent(const WebGestureEvent& webEvent)
    {
        m_type = platform(webEvent.type());
        m_modifiers = platform(webEvent.modifiers());
        m_timestamp = webEvent.timestamp();

        m_gestureScale = webEvent.gestureScale();
        m_gestureRotation = webEvent.gestureRotation();
        m_position = webEvent.position();
        m_globalPosition = webEvent.position();
    }
};

WebCore::PlatformGestureEvent platform(const WebGestureEvent& webEvent)
{
    return WebKit2PlatformGestureEvent(webEvent);
}
#endif

#if PLATFORM(GTK) || PLATFORM(WPE) || USE(LIBWPE)
MonotonicTime monotonicTimeForEventTimeInMilliseconds(uint64_t timestamp)
{
    // This function corrects a fixed offset between GTK and WPE timestamps and
    // MonotonicTime, but it also introduces an error. It will break if the
    // aforementioned offset is not constant, which could happen if timestamps
    // are based on anything other than CLOCK_MONOTONIC.
    if (!timestamp)
        return MonotonicTime::now();

    static MonotonicTime firstEventMonotonicTime;
    static uint64_t firstEventTimestamp = 0;
    if (!firstEventTimestamp) {
        firstEventTimestamp = timestamp;
        // The introduced error is the mismatch between the generation of the
        // first timestamp and this call:
        firstEventMonotonicTime = MonotonicTime::now();
    }
    return firstEventMonotonicTime + Seconds::fromMilliseconds(timestamp - firstEventTimestamp);
}
#endif

} // namespace WebKit
