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

#include "WebEventFactory.h"
#include <WebCore/GtkVersioning.h>
#include <gdk/gdk.h>

namespace WebKit {

#if USE(GTK4)
#define constructNativeEvent(event) event
#else
#define constructNativeEvent(event) gdk_event_copy(event)
#endif

NativeWebMouseEvent::NativeWebMouseEvent(GdkEvent* event, int eventClickCount, std::optional<WebCore::FloatSize> delta)
    : WebMouseEvent(WebEventFactory::createWebMouseEvent(event, eventClickCount, delta))
    , m_nativeEvent(constructNativeEvent(event))
{
}

NativeWebMouseEvent::NativeWebMouseEvent(GdkEvent* event, const WebCore::IntPoint& position, int eventClickCount, std::optional<WebCore::FloatSize> delta)
    : WebMouseEvent(WebEventFactory::createWebMouseEvent(event, position, position, eventClickCount, delta))
    , m_nativeEvent(constructNativeEvent(event))
{
}

NativeWebMouseEvent::NativeWebMouseEvent(const WebCore::IntPoint& position)
    : WebMouseEvent(WebEventFactory::createWebMouseEvent(position))
{
}

NativeWebMouseEvent::NativeWebMouseEvent(Type type, WebMouseEventButton button, unsigned short buttons, const WebCore::IntPoint& position, const WebCore::IntPoint& globalPosition, int clickCount, OptionSet<WebEventModifier> modifiers, std::optional<WebCore::FloatSize> delta, WebCore::PointerID pointerId, const String& pointerType, WebCore::PlatformMouseEvent::IsTouch isTouchEvent)
    : WebMouseEvent(WebEvent(type, modifiers, WallTime::now()), button, buttons, position, globalPosition, delta.value_or(WebCore::FloatSize()).width(), delta.value_or(WebCore::FloatSize()).height(), 0, clickCount, 0, WebMouseEventSyntheticClickType::NoTap, isTouchEvent, pointerId, pointerType)
{
}

NativeWebMouseEvent::NativeWebMouseEvent(const NativeWebMouseEvent& event)
    : WebMouseEvent(WebEvent(event.type(), event.modifiers(), event.timestamp()), event.button(), event.buttons(), event.position(), event.globalPosition(), event.deltaX(), event.deltaY(), event.deltaZ(), event.clickCount(), 0, WebMouseEventSyntheticClickType::NoTap, event.isTouchEvent(), event.pointerId(), event.pointerType())
    , m_nativeEvent(event.nativeEvent() ? constructNativeEvent(const_cast<GdkEvent*>(event.nativeEvent())) : nullptr)
{
}

} // namespace WebKit

#undef constructNativeEvent
