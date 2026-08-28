/*
 * Copyright (C) 2011 Igalia S.L.
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
#include "NativeWebMouseEvent.h"

#include "GtkVersioning.h"
#include "WebEventFactory.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

#if USE(GTK4)
#define constructNativeEvent(event) event
#else
#define constructNativeEvent(event) gdk_event_copy(event)
#endif

WTF_MAKE_TZONE_ALLOCATED_IMPL(NativeWebMouseEvent);

Ref<NativeWebMouseEvent> NativeWebMouseEvent::create(GdkEvent* event, int eventClickCount, std::optional<WebCore::FloatSize> delta)
{
    return adoptRef(*new NativeWebMouseEvent(WebEventFactory::createWebMouseEvent(event, eventClickCount, delta), event));
}

Ref<NativeWebMouseEvent> NativeWebMouseEvent::create(GdkEvent* event, const WebCore::DoublePoint& position, int eventClickCount, std::optional<WebCore::FloatSize> delta)
{
    return adoptRef(*new NativeWebMouseEvent(WebEventFactory::createWebMouseEvent(event, position, eventClickCount, delta), event));
}

Ref<NativeWebMouseEvent> NativeWebMouseEvent::create(const WebCore::DoublePoint& position)
{
    return adoptRef(*new NativeWebMouseEvent(WebEventFactory::createWebMouseEvent(position), nullptr));
}

Ref<NativeWebMouseEvent> NativeWebMouseEvent::create(WebEventType type, WebMouseEventButton button, unsigned short buttons, const WebCore::DoublePoint& position, const WebCore::DoublePoint& globalPosition, int clickCount, OptionSet<WebEventModifier> modifiers, std::optional<WebCore::FloatSize> delta, WebCore::PointerID pointerId, const String& pointerType, WebCore::PlatformMouseEvent::IsTouch isTouchEvent)
{
    return adoptRef(*new NativeWebMouseEvent(WebMouseEventInit {
        { type, modifiers, MonotonicTime::now() },
        {
            .button = button,
            .buttons = buttons,
            .position = position,
            .globalPosition = globalPosition,
            .deltaX = delta.value_or(WebCore::FloatSize()).width(),
            .deltaY = delta.value_or(WebCore::FloatSize()).height(),
            .deltaZ = 0,
            .clickCount = clickCount,
            .force = 0,
            .inputSource = WebEventInputSource::UserDriven,
            .canInitiateDrag = WebCore::PlatformMouseEvent::CanInitiateDrag::Yes,
            .syntheticClickType = WebMouseEventSyntheticClickType::NoTap,
            .isTouchEvent = isTouchEvent,
            .pointerId = pointerId,
            .pointerType = pointerType,
            .gestureWasCancelled = GestureWasCancelled::No,
            .unadjustedMovementDelta = { },
            .coalescedEvents = { },
        }
    }, nullptr));
}

Ref<NativeWebMouseEvent> NativeWebMouseEvent::create(const NativeWebMouseEvent& event)
{
    return adoptRef(*new NativeWebMouseEvent(WebMouseEventInit { event.eventData(), event.mouseData() },
        event.nativeEvent() ? const_cast<GdkEvent*>(event.nativeEvent()) : nullptr));
}

NativeWebMouseEvent::NativeWebMouseEvent(WebMouseEventInit&& init, GdkEvent* event)
    : WebMouseEvent(WTF::move(init.event), WTF::move(init.mouse))
    , m_nativeEvent(event ? constructNativeEvent(event) : nullptr)
{
}

} // namespace WebKit

#undef constructNativeEvent
