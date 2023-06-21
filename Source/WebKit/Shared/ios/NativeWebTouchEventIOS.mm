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

#import "UIKitSPI.h"
#import <UIKit/UITouch.h>
#import <WebCore/IntPoint.h>
#import <WebCore/WAKAppKitStubs.h>

namespace WebKit {

#if ENABLE(TOUCH_EVENTS)

static inline WebEventType webEventTypeForUIWebTouchEventType(UIWebTouchEventType type)
{
    switch (type) {
    case UIWebTouchEventTouchBegin:
        return WebEventType::TouchStart;
    case UIWebTouchEventTouchChange:
        return WebEventType::TouchMove;
    case UIWebTouchEventTouchEnd:
        return WebEventType::TouchEnd;
    case UIWebTouchEventTouchCancel:
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

static WebPlatformTouchPoint::TouchType convertTouchType(UIWebTouchPointType touchType)
{
    switch (touchType) {
    case UIWebTouchPointTypeDirect:
        return WebPlatformTouchPoint::TouchType::Direct;
    case UIWebTouchPointTypeStylus:
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

static CGFloat radiusForTouchPoint(const _UIWebTouchPoint& touchPoint)
{
#if ENABLE(FIXED_IOS_TOUCH_POINT_RADIUS)
    return 12.1;
#else
    return touchPoint.majorRadiusInScreenCoordinates;
#endif
}

Vector<WebPlatformTouchPoint> NativeWebTouchEvent::extractWebTouchPoint(const _UIWebTouchEvent* event)
{
    unsigned touchCount = event->touchPointCount;

    Vector<WebPlatformTouchPoint> touchPointList;
    touchPointList.reserveInitialCapacity(touchCount);
    for (unsigned i = 0; i < touchCount; ++i) {
        const _UIWebTouchPoint& touchPoint = event->touchPoints[i];
        unsigned identifier = touchPoint.identifier;
        WebCore::IntPoint location = positionForCGPoint(touchPoint.locationInDocumentCoordinates);
        WebPlatformTouchPoint::State phase = convertTouchPhase(touchPoint.phase);
        WebPlatformTouchPoint platformTouchPoint = WebPlatformTouchPoint(identifier, location, phase);
#if ENABLE(IOS_TOUCH_EVENTS)
        auto radius = radiusForTouchPoint(touchPoint);
        platformTouchPoint.setRadiusX(radius);
        platformTouchPoint.setRadiusY(radius);
        platformTouchPoint.setRotationAngle(0); // Not available in _UIWebTouchEvent yet.
        platformTouchPoint.setForce(touchPoint.force);
        platformTouchPoint.setAltitudeAngle(touchPoint.altitudeAngle);
        platformTouchPoint.setAzimuthAngle(touchPoint.azimuthAngle);
        platformTouchPoint.setTouchType(convertTouchType(touchPoint.touchType));
#endif
        touchPointList.uncheckedAppend(platformTouchPoint);
    }
    return touchPointList;
}

NativeWebTouchEvent::NativeWebTouchEvent(const _UIWebTouchEvent* event, UIKeyModifierFlags flags)
    : WebTouchEvent(
        { webEventTypeForUIWebTouchEventType(event->type), webEventModifierFlags(flags), WallTime::fromRawSeconds(event->timestamp) },
        extractWebTouchPoint(event),
        positionForCGPoint(event->locationInDocumentCoordinates),
        event->isPotentialTap,
        event->inJavaScriptGesture,
        event->scale,
        event->rotation)
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
