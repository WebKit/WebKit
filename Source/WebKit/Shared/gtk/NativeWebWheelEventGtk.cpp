/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
#include "NativeWebWheelEvent.h"

#include "GtkVersioning.h"
#include "WebEventFactory.h"
#include <wtf/TZoneMallocInlines.h>

#if USE(GTK4)
#define constructNativeEvent(event) event
#else
#define constructNativeEvent(event) gdk_event_copy(event)
#endif

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(NativeWebWheelEvent);

Ref<NativeWebWheelEvent> NativeWebWheelEvent::create(GdkEvent* event, const WebCore::IntPoint& position, const WebCore::IntPoint& globalPosition, const WebCore::FloatSize& delta, const WebCore::FloatSize& wheelTicks, WebWheelEvent::Phase phase, WebWheelEvent::Phase momentumPhase, bool hasPreciseDeltas)
{
    return adoptRef(*new NativeWebWheelEvent(WebEventFactory::createWebWheelEvent(event, position, globalPosition, delta, wheelTicks, phase, momentumPhase, hasPreciseDeltas), event));
}

Ref<NativeWebWheelEvent> NativeWebWheelEvent::create(const NativeWebWheelEvent& event)
{
    return adoptRef(*new NativeWebWheelEvent(WebWheelEventInit { event.eventData(), event.wheelData() },
        event.nativeEvent() ? const_cast<GdkEvent*>(event.nativeEvent()) : nullptr));
}

NativeWebWheelEvent::NativeWebWheelEvent(WebWheelEventInit&& init, GdkEvent* event)
    : WebWheelEvent(WTF::move(init.event), WTF::move(init.wheel))
    , m_nativeEvent(event ? constructNativeEvent(event) : nullptr)
{
}

} // namespace WebKit

#undef constructNativeEvent
