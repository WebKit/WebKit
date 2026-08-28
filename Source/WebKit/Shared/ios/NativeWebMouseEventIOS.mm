/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#import "NativeWebMouseEvent.h"

#if PLATFORM(IOS_FAMILY)

#import "WebIOSEventFactory.h"
#import <wtf/TZoneMallocInlines.h>

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(NativeWebMouseEvent);

Ref<NativeWebMouseEvent> NativeWebMouseEvent::create(::WebEvent *event)
{
    return adoptRef(*new NativeWebMouseEvent(WebIOSEventFactory::createWebMouseEvent(event), event));
}

Ref<NativeWebMouseEvent> NativeWebMouseEvent::create(WebEventType type, WebMouseEventButton button, unsigned short buttons, const WebCore::DoublePoint& position, const WebCore::DoublePoint& globalPosition, float deltaX, float deltaY, float deltaZ, int clickCount, OptionSet<WebEventModifier> modifiers, MonotonicTime timestamp, double force, GestureWasCancelled gestureWasCancelled, const String& pointerType)
{
    return adoptRef(*new NativeWebMouseEvent(WebMouseEventInit {
        { type, modifiers, timestamp },
        {
            .button = button,
            .buttons = buttons,
            .position = position,
            .globalPosition = globalPosition,
            .deltaX = deltaX,
            .deltaY = deltaY,
            .deltaZ = deltaZ,
            .clickCount = clickCount,
            .force = force,
            .inputSource = WebEventInputSource::UserDriven,
            .canInitiateDrag = WebCore::PlatformMouseEvent::CanInitiateDrag::Yes,
            .syntheticClickType = WebMouseEventSyntheticClickType::NoTap,
            .pointerId = WebCore::mousePointerID,
            .pointerType = pointerType,
            .gestureWasCancelled = gestureWasCancelled,
            .unadjustedMovementDelta = { deltaX, deltaY },
        }
    }, nil));
}

Ref<NativeWebMouseEvent> NativeWebMouseEvent::create(const NativeWebMouseEvent& otherEvent, const WebCore::DoublePoint& position, const WebCore::DoublePoint& globalPosition, float deltaX, float deltaY, float deltaZ)
{
    return adoptRef(*new NativeWebMouseEvent(WebMouseEventInit {
        { otherEvent.type(), otherEvent.modifiers(), otherEvent.timestamp() },
        {
            .button = otherEvent.button(),
            .buttons = otherEvent.buttons(),
            .position = position,
            .globalPosition = globalPosition,
            .deltaX = deltaX,
            .deltaY = deltaY,
            .deltaZ = deltaZ,
            .clickCount = otherEvent.clickCount(),
            .force = otherEvent.force(),
            .inputSource = otherEvent.inputSource(),
            .canInitiateDrag = otherEvent.canInitiateDrag(),
            .syntheticClickType = otherEvent.syntheticClickType(),
            .pointerId = otherEvent.pointerId(),
            .pointerType = otherEvent.pointerType(),
            .gestureWasCancelled = otherEvent.gestureWasCancelled(),
            .unadjustedMovementDelta = { deltaX, deltaY },
        }
    }, nil));
}

NativeWebMouseEvent::NativeWebMouseEvent(WebMouseEventInit&& init, ::WebEvent *event)
    : WebMouseEvent(WTF::move(init.event), WTF::move(init.mouse))
    , m_nativeEvent(event)
{
}

} // namespace WebKit

#endif // PLATFORM(IOS_FAMILY)
