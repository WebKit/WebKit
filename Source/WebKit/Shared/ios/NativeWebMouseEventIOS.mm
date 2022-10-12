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

namespace WebKit {

NativeWebMouseEvent::NativeWebMouseEvent(::WebEvent *event)
    : WebMouseEvent(WebIOSEventFactory::createWebMouseEvent(event))
    , m_nativeEvent(event)
{
}

NativeWebMouseEvent::NativeWebMouseEvent(Type type, WebMouseEventButton button, unsigned short buttons, const WebCore::IntPoint& position, const WebCore::IntPoint& globalPosition, float deltaX, float deltaY, float deltaZ, int clickCount, OptionSet<WebEventModifier> modifiers, WallTime timestamp, double force, GestureWasCancelled gestureWasCancelled, const String& pointerType)
    : WebMouseEvent({ type, modifiers, timestamp }, button, buttons, position, globalPosition, deltaX, deltaY, deltaZ, clickCount, force, WebMouseEventSyntheticClickType::NoTap, WebCore::mousePointerID, pointerType, gestureWasCancelled)
{
}

NativeWebMouseEvent::NativeWebMouseEvent(const NativeWebMouseEvent& otherEvent, const WebCore::IntPoint& position, const WebCore::IntPoint& globalPosition, float deltaX, float deltaY, float deltaZ)
    : WebMouseEvent({ otherEvent.type(), otherEvent.modifiers(), otherEvent.timestamp() }, otherEvent.button(), otherEvent.buttons(), position, globalPosition, deltaX, deltaY, deltaZ, otherEvent.clickCount(), otherEvent.force(), otherEvent.syntheticClickType(), otherEvent.pointerId(), otherEvent.pointerType(), otherEvent.gestureWasCancelled())
{
}

} // namespace WebKit

#endif // PLATFORM(IOS_FAMILY)
