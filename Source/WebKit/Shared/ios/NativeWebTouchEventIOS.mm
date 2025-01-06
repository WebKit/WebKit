/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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
#import "NativeWebTouchEvent.h"

#if PLATFORM(IOS_FAMILY)

#import "WKTouchEventsGestureRecognizer.h"
#import <WebCore/IntPoint.h>
#import <WebCore/WAKAppKitStubs.h>

namespace WebKit {

#if ENABLE(TOUCH_EVENTS)

static inline WebEventType webEventTypeForWKTouchEventType(WKTouchEventType type)
{
    switch (type) {
    case WKTouchEventType::Begin:
        return WebEventType::TouchStart;
    case WKTouchEventType::Change:
        return WebEventType::TouchMove;
    case WKTouchEventType::End:
        return WebEventType::TouchEnd;
    case WKTouchEventType::Cancel:
        return WebEventType::TouchCancel;
    }
}

static WebPlatformTouchPoint::State convertTouchPhase(UITouchPhase touchPhase)
{
    switch (touchPhase) {
    case UITouchPhaseBegan:
        return WebPlatformTouchPoint::State::Pressed;
    case UITouchPhaseMoved:
        return WebPlatformTouchPoint::State::Moved;
    case UITouchPhaseStationary:
        return WebPlatformTouchPoint::State::Stationary;
    case UITouchPhaseEnded:
        return WebPlatformTouchPoint::State::Released;
    case UITouchPhaseCancelled:
        return WebPlatformTouchPoint::State::Cancelled;
    default:
        ASSERT_NOT_REACHED();
        return WebPlatformTouchPoint::State::Stationary;
    }
}

static WebPlatformTouchPoint::TouchType convertTouchType(WKTouchPointType touchType)
{
    switch (touchType) {
    case WKTouchPointType::Direct:
        return WebPlatformTouchPoint::TouchType::Direct;
    case WKTouchPointType::Stylus:
        return WebPlatformTouchPoint::TouchType::Stylus;
    default:
        ASSERT_NOT_REACHED();
        return WebPlatformTouchPoint::TouchType::Direct;
    }
}

static inline WebCore::IntPoint positionForCGPoint(CGPoint position)
{
    return WebCore::IntPoint(position);
}

static CGFloat radiusForTouchPoint(const WKTouchPoint& touchPoint)
{
#if ENABLE(FIXED_IOS_TOUCH_POINT_RADIUS)
    return 12.1;
#else
    return touchPoint.majorRadiusInWindowCoordinates;
#endif
}

Vector<WebPlatformTouchPoint> NativeWebTouchEvent::extractWebTouchPoints(const WKTouchEvent& event)
{
    return event.touchPoints.map([](auto& touchPoint) {
        unsigned identifier = touchPoint.identifier;
        auto locationInRootView = positionForCGPoint(touchPoint.locationInRootViewCoordinates);
        auto locationInViewport = positionForCGPoint(touchPoint.locationInViewport);
        WebPlatformTouchPoint::State phase = convertTouchPhase(touchPoint.phase);
        WebPlatformTouchPoint platformTouchPoint = WebPlatformTouchPoint(identifier, locationInRootView, locationInViewport, phase);
#if ENABLE(IOS_TOUCH_EVENTS)
        auto radius = radiusForTouchPoint(touchPoint);
        platformTouchPoint.setRadiusX(radius);
        platformTouchPoint.setRadiusY(radius);
        // FIXME (259068): Add support for Touch.rotationAngle.
        platformTouchPoint.setRotationAngle(0);
        platformTouchPoint.setForce(touchPoint.force);
        platformTouchPoint.setAltitudeAngle(touchPoint.altitudeAngle);
        platformTouchPoint.setAzimuthAngle(touchPoint.azimuthAngle);
        platformTouchPoint.setTouchType(convertTouchType(touchPoint.touchType));
#endif
        return platformTouchPoint;
    });
}

Vector<WebTouchEvent> NativeWebTouchEvent::extractCoalescedWebTouchEvents(const WKTouchEvent& event, UIKeyModifierFlags flags)
{
    return event.coalescedEvents.map([&](auto& event) -> WebTouchEvent {
        return NativeWebTouchEvent { event, flags };
    });
}

Vector<WebTouchEvent> NativeWebTouchEvent::extractPredictedWebTouchEvents(const WKTouchEvent& event, UIKeyModifierFlags flags)
{
    return event.predictedEvents.map([&](auto& event) -> WebTouchEvent {
        return NativeWebTouchEvent { event, flags };
    });
}

NativeWebTouchEvent::NativeWebTouchEvent(const WKTouchEvent& event, UIKeyModifierFlags flags)
    : WebTouchEvent(
        { webEventTypeForWKTouchEventType(event.type), webEventModifierFlags(flags), WallTime::fromRawSeconds(event.timestamp) },
        extractWebTouchPoints(event),
        extractCoalescedWebTouchEvents(event, flags),
        extractPredictedWebTouchEvents(event, flags),
        positionForCGPoint(event.locationInRootViewCoordinates),
        event.isPotentialTap,
        event.inJavaScriptGesture,
        event.scale,
        event.rotation)
{
}

#endif // ENABLE(TOUCH_EVENTS)

OptionSet<WebEventModifier> webEventModifierFlags(UIKeyModifierFlags flags)
{
    OptionSet<WebEventModifier> modifiers;
    if (flags & UIKeyModifierShift)
        modifiers.add(WebEventModifier::ShiftKey);
    if (flags & UIKeyModifierControl)
        modifiers.add(WebEventModifier::ControlKey);
    if (flags & UIKeyModifierAlternate)
        modifiers.add(WebEventModifier::AltKey);
    if (flags & UIKeyModifierCommand)
        modifiers.add(WebEventModifier::MetaKey);
    if (flags & UIKeyModifierAlphaShift)
        modifiers.add(WebEventModifier::CapsLockKey);
    return modifiers;
}

} // namespace WebKit

#endif // PLATFORM(IOS_FAMILY)
