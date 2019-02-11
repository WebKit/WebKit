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

#if PLATFORM(IOS_FAMILY) && ENABLE(TOUCH_EVENTS)

#import "UIKitSPI.h"
#import "WebEvent.h"
#import <UIKit/UITouch.h>
#import <WebCore/IntPoint.h>
#import <WebCore/WAKAppKitStubs.h>

namespace WebKit {

static inline WebEvent::Type webEventTypeForUIWebTouchEventType(UIWebTouchEventType type)
{
    switch (type) {
    case UIWebTouchEventTouchBegin:
        return WebEvent::TouchStart;
    case UIWebTouchEventTouchChange:
        return WebEvent::TouchMove;
    case UIWebTouchEventTouchEnd:
        return WebEvent::TouchEnd;
    case UIWebTouchEventTouchCancel:
        return WebEvent::TouchCancel;
    }
}

static WebPlatformTouchPoint::TouchPointState convertTouchPhase(UITouchPhase touchPhase)
{
    switch (touchPhase) {
    case UITouchPhaseBegan:
        return WebPlatformTouchPoint::TouchPressed;
    case UITouchPhaseMoved:
        return WebPlatformTouchPoint::TouchMoved;
    case UITouchPhaseStationary:
        return WebPlatformTouchPoint::TouchStationary;
    case UITouchPhaseEnded:
        return WebPlatformTouchPoint::TouchReleased;
    case UITouchPhaseCancelled:
        return WebPlatformTouchPoint::TouchCancelled;
    default:
        ASSERT_NOT_REACHED();
        return WebPlatformTouchPoint::TouchStationary;
    }
}

#if defined UI_WEB_TOUCH_EVENT_HAS_STYLUS_DATA && UI_WEB_TOUCH_EVENT_HAS_STYLUS_DATA
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
#endif

static inline WebCore::IntPoint positionForCGPoint(CGPoint position)
{
    return WebCore::IntPoint(position);
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
        WebPlatformTouchPoint::TouchPointState phase = convertTouchPhase(touchPoint.phase);
        WebPlatformTouchPoint platformTouchPoint = WebPlatformTouchPoint(identifier, location, phase);
#if ENABLE(IOS_TOUCH_EVENTS)
        platformTouchPoint.setRadiusX(touchPoint.majorRadiusInScreenCoordinates);
        platformTouchPoint.setRadiusY(touchPoint.majorRadiusInScreenCoordinates);
        platformTouchPoint.setRotationAngle(0); // Not available in _UIWebTouchEvent yet.
        platformTouchPoint.setForce(touchPoint.force);
#if defined UI_WEB_TOUCH_EVENT_HAS_STYLUS_DATA && UI_WEB_TOUCH_EVENT_HAS_STYLUS_DATA
        platformTouchPoint.setAltitudeAngle(touchPoint.altitudeAngle);
        platformTouchPoint.setAzimuthAngle(touchPoint.azimuthAngle);
        platformTouchPoint.setTouchType(convertTouchType(touchPoint.touchType));
#endif
#endif
        touchPointList.uncheckedAppend(platformTouchPoint);
    }
    return touchPointList;
}

NativeWebTouchEvent::NativeWebTouchEvent(const _UIWebTouchEvent* event)
    : WebTouchEvent(
        webEventTypeForUIWebTouchEventType(event->type),
        OptionSet<Modifier> { },
        WallTime::fromRawSeconds(event->timestamp),
        extractWebTouchPoint(event),
        positionForCGPoint(event->locationInDocumentCoordinates),
#if defined UI_WEB_TOUCH_EVENT_HAS_IS_POTENTIAL_TAP && UI_WEB_TOUCH_EVENT_HAS_IS_POTENTIAL_TAP
        event->isPotentialTap,
#else
        true,
#endif
        event->inJavaScriptGesture,
        event->scale,
        event->rotation)
{
}

} // namespace WebKit

#endif // PLATFORM(IOS_FAMILY) && ENABLE(TOUCH_EVENTS)
