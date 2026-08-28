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

#import "WebEventFactory.h"
#import "WebGestureEvent.h"
#import <WebCore/IntPoint.h>
#import <WebCore/PlatformEventFactoryMac.h>
#import <wtf/TZoneMallocInlines.h>

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(NativeWebGestureEvent);

static inline std::optional<WebEventType> webEventTypeForPhase(WebEventPhase phase)
{
    switch (phase) {
    case WebEventPhase::Began:
        return WebEventType::GestureStart;
    case WebEventPhase::Changed:
        return WebEventType::GestureChange;
    case WebEventPhase::Ended:
    case WebEventPhase::Cancelled:
        return WebEventType::GestureEnd;
    default:
        break;
    }
    return std::nullopt;
}

static WebCore::IntPoint positionInView(WebCore::FloatPoint locationInWindow, NSView *view)
{
    return WebCore::IntPoint { view ? WebCore::FloatPoint { [view convertPoint:locationInWindow fromView:nil] } : locationInWindow };
}

static NativeWebGestureEvent::Init initForEvent(NSEvent *event)
{
    using Kind = NativeWebGestureEvent::Kind;

    ASSERT(event.type == NSEventTypeMagnify || event.type == NSEventTypeRotate);
    bool isRotation = event.type == NSEventTypeRotate;

    return {
        isRotation ? Kind::Rotation : Kind::Magnification,
        WebEventFactory::phaseForEvent(event),
        WebCore::FloatPoint { event.locationInWindow },
        isRotation ? 0 : static_cast<float>(event.magnification),
        isRotation ? static_cast<float>(event.rotation) : 0,
        MonotonicTime::fromRawSeconds(event.timestamp)
    };
}

RefPtr<NativeWebGestureEvent> NativeWebGestureEvent::create(NSEvent *event, NSView *view)
{
    return create(initForEvent(event), view, event);
}

RefPtr<NativeWebGestureEvent> NativeWebGestureEvent::create(const Init& init, NSView *view)
{
    return create(init, view, nil);
}

RefPtr<NativeWebGestureEvent> NativeWebGestureEvent::create(const Init& init, NSView *view, NSEvent *event)
{
    auto type = webEventTypeForPhase(init.phase);
    if (!type)
        return nullptr;
    return adoptRef(*new NativeWebGestureEvent { *type, init, view, event });
}

NativeWebGestureEvent::NativeWebGestureEvent(WebEventType type, const Init& init, NSView *view, NSEvent *event)
    : WebGestureEvent {
        WebEventData { type, { }, init.timestamp },
        WebGestureEventData {
            .position = positionInView(init.locationInWindow, view),
            .gestureScale = init.gestureScale,
            .gestureRotation = init.gestureRotation,
            .phase = init.phase,
        } }
    , m_allowsNativeZoom(init.allowsNativeZoom)
    , m_kind(init.kind)
    , m_nativeEvent(event)
{
}

} // namespace WebKit

#endif // ENABLE(MAC_GESTURE_EVENTS)
