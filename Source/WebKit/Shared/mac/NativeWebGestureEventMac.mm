/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#import "NativeWebGestureEvent.h"

#if ENABLE(MAC_GESTURE_EVENTS)

#import "WebEvent.h"
#import "WebGestureEvent.h"
#import <WebCore/IntPoint.h>
#import <WebCore/PlatformEventFactoryMac.h>

namespace WebKit {

static inline WebEvent::Type webEventTypeForNSEvent(NSEvent *event)
{
    switch (event.phase) {
    case NSEventPhaseBegan:
        return WebEvent::GestureStart;
    case NSEventPhaseChanged:
        return WebEvent::GestureChange;
    case NSEventPhaseEnded:
    case NSEventPhaseCancelled:
        return WebEvent::GestureEnd;
    default:
        break;
    }
    return WebEvent::Type::NoType;
}

static NSPoint pointForEvent(NSEvent *event, NSView *windowView)
{
    NSPoint location = [event locationInWindow];
    if (windowView)
        location = [windowView convertPoint:location fromView:nil];
    return location;
}

NativeWebGestureEvent::NativeWebGestureEvent(NSEvent *event, NSView *view)
    : WebGestureEvent(
        webEventTypeForNSEvent(event),
        OptionSet<WebEvent::Modifier> { },
        WebCore::eventTimeStampSince1970(event),
        WebCore::IntPoint(pointForEvent(event, view)),
        event.type == NSEventTypeMagnify ? event.magnification : 0,
        event.type == NSEventTypeRotate ? event.rotation : 0)
    , m_nativeEvent(event)
{
}

} // namespace WebKit

#endif // ENABLE(MAC_GESTURE_EVENTS)
