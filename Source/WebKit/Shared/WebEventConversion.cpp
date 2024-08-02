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

#if ENABLE(MAC_GESTURE_EVENTS)
#include "WebGestureEvent.h"
#endif

namespace WebKit {

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

class WebKit2PlatformMouseEvent : public WebCore::PlatformMouseEvent {
public:
    WebKit2PlatformMouseEvent(const WebMouseEvent& webEvent)
    {
        // PlatformEvent
        switch (webEvent.type()) {
        case WebEventType::MouseDown:
            m_type = WebCore::PlatformEvent::Type::MousePressed;
            m_force = WebCore::ForceAtClick;
            break;
        case WebEventType::MouseUp:
            m_type = WebCore::PlatformEvent::Type::MouseReleased;
            m_force = 0;
            break;
        case WebEventType::MouseMove:
            m_type = WebCore::PlatformEvent::Type::MouseMoved;
            m_force = webEvent.force();
            break;
        case WebEventType::MouseForceChanged:
            m_type = WebCore::PlatformEvent::Type::MouseForceChanged;
            m_force = webEvent.force();
            break;
        case WebEventType::MouseForceDown:
            m_type = WebCore::PlatformEvent::Type::MouseForceDown;
            m_force = WebCore::ForceAtForceClick;
            break;
        case WebEventType::MouseForceUp:
            m_type = WebCore::PlatformEvent::Type::MouseForceUp;
            m_force = 0;
            break;
        default:
            ASSERT_NOT_REACHED();
        }

        m_modifiers = platform(webEvent.modifiers());

        m_timestamp = webEvent.timestamp();
        m_authorizationToken = webEvent.authorizationToken();

        // PlatformMouseEvent
        switch (webEvent.button()) {
        case WebMouseEventButton::None:
            m_button = WebCore::MouseButton::None;
            break;
        case WebMouseEventButton::Left:
            m_button = WebCore::MouseButton::Left;
            break;
        case WebMouseEventButton::Middle:
            m_button = WebCore::MouseButton::Middle;
            break;
        case WebMouseEventButton::Right:
            m_button = WebCore::MouseButton::Right;
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }

        m_buttons = webEvent.buttons();

        m_position = webEvent.position();
        m_movementDelta = WebCore::IntPoint(webEvent.deltaX(), webEvent.deltaY());
        m_unadjustedMovementDelta = webEvent.unadjustedMovementDelta();
        m_globalPosition = webEvent.globalPosition();
        m_clickCount = webEvent.clickCount();
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
        m_type = PlatformEvent::Type::Wheel;

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
        switch (webEvent.type()) {
        case WebEventType::KeyDown:
            m_type = WebCore::PlatformEvent::Type::KeyDown;
            break;
        case WebEventType::KeyUp:
            m_type = WebCore::PlatformEvent::Type::KeyUp;
            break;
        case WebEventType::RawKeyDown:
            m_type = WebCore::PlatformEvent::Type::RawKeyDown;
            break;
        case WebEventType::Char:
            m_type = WebCore::PlatformEvent::Type::Char;
            break;
        default:
            ASSERT_NOT_REACHED();
        }

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
    : PlatformTouchPoint(webTouchPoint.identifier(), webTouchPoint.location(), touchEventType(webTouchPoint)
#if ENABLE(IOS_TOUCH_EVENTS)
        , webTouchPoint.radiusX(), webTouchPoint.radiusY(), webTouchPoint.rotationAngle(), webTouchPoint.force(), webTouchPoint.altitudeAngle(), webTouchPoint.azimuthAngle(), webPlatformTouchTypeToPlatform(webTouchPoint.touchType())
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
        m_radiusX = webTouchPoint.radius().width();
        m_radiusY = webTouchPoint.radius().height();
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
        switch (webEvent.type()) {
        case WebEventType::TouchStart:
            m_type = WebCore::PlatformEvent::Type::TouchStart;
            break;
        case WebEventType::TouchMove:
            m_type = WebCore::PlatformEvent::Type::TouchMove;
            break;
        case WebEventType::TouchEnd:
            m_type = WebCore::PlatformEvent::Type::TouchEnd;
            break;
        case WebEventType::TouchCancel:
            m_type = WebCore::PlatformEvent::Type::TouchCancel;
            break;
        default:
            ASSERT_NOT_REACHED();
        }

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
        switch (webEvent.type()) {
        case WebEventType::GestureStart:
            m_type = WebCore::PlatformEvent::Type::GestureStart;
            break;
        case WebEventType::GestureChange:
            m_type = WebCore::PlatformEvent::Type::GestureChange;
            break;
        case WebEventType::GestureEnd:
            m_type = WebCore::PlatformEvent::Type::GestureEnd;
            break;
        default:
            ASSERT_NOT_REACHED();
        }

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

} // namespace WebKit
