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
#include <gdk/gdk.h>

namespace WebKit {

NativeWebMouseEvent::NativeWebMouseEvent(GdkEvent* event, int eventClickCount, Optional<WebCore::IntPoint> delta)
    : WebMouseEvent(WebEventFactory::createWebMouseEvent(event, eventClickCount, delta))
    , m_nativeEvent(gdk_event_copy(event))
{
}

NativeWebMouseEvent::NativeWebMouseEvent(GdkEvent* event, const WebCore::IntPoint& position, int eventClickCount, Optional<WebCore::IntPoint> delta)
    : WebMouseEvent(WebEventFactory::createWebMouseEvent(event, position, position, eventClickCount, delta))
    , m_nativeEvent(gdk_event_copy(event))
{
}

NativeWebMouseEvent::NativeWebMouseEvent(const WebCore::IntPoint& position)
    : WebMouseEvent(WebEventFactory::createWebMouseEvent(position))
{
}

NativeWebMouseEvent::NativeWebMouseEvent(Type type, Button button, unsigned short buttons, const WebCore::IntPoint& position, const WebCore::IntPoint& globalPosition, int clickCount, OptionSet<Modifier> modifiers)
    : WebMouseEvent(type, button, buttons, position, globalPosition, 0, 0, 0, clickCount, modifiers, WallTime::now())
{
}

NativeWebMouseEvent::NativeWebMouseEvent(const NativeWebMouseEvent& event)
    : WebMouseEvent(event.type(), event.button(), event.buttons(), event.position(), event.globalPosition(), event.deltaX(), event.deltaY(), event.deltaZ(), event.clickCount(), event.modifiers(), event.timestamp())
    , m_nativeEvent(event.nativeEvent() ? gdk_event_copy(const_cast<GdkEvent*>(event.nativeEvent())) : nullptr)
{
}

} // namespace WebKit
