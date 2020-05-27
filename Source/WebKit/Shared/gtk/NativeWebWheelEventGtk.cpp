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

#include "WebEventFactory.h"
#include <WebCore/GtkVersioning.h>

namespace WebKit {

NativeWebWheelEvent::NativeWebWheelEvent(GdkEvent* event)
    : WebWheelEvent(WebEventFactory::createWebWheelEvent(event))
    , m_nativeEvent(gdk_event_copy(event))
{
}

NativeWebWheelEvent::NativeWebWheelEvent(GdkEvent* event, WebWheelEvent::Phase phase, WebWheelEvent::Phase momentumPhase)
    : WebWheelEvent(WebEventFactory::createWebWheelEvent(event, phase, momentumPhase))
    , m_nativeEvent(gdk_event_copy(event))
{
}

NativeWebWheelEvent::NativeWebWheelEvent(GdkEvent* event, const WebCore::IntPoint& position)
    : WebWheelEvent(WebEventFactory::createWebWheelEvent(event, position, position, WebWheelEvent::Phase::PhaseChanged, WebWheelEvent::Phase::PhaseNone))
    , m_nativeEvent(gdk_event_copy(event))
{
}

NativeWebWheelEvent::NativeWebWheelEvent(const WebCore::IntPoint& position, const WebCore::IntPoint& globalPosition, const WebCore::FloatSize& delta, const WebCore::FloatSize& wheelTicks, WebWheelEvent::Phase phase, WebWheelEvent::Phase momentumPhase)
    : WebWheelEvent(WebEvent::Wheel, position, globalPosition, delta, wheelTicks, phase, momentumPhase, WebWheelEvent::ScrollByPixelWheelEvent, { }, WallTime::now())
{
}

NativeWebWheelEvent::NativeWebWheelEvent(const NativeWebWheelEvent& event)
    : WebWheelEvent(event.type(), event.position(), event.globalPosition(), event.delta(), event.wheelTicks(), event.phase(), event.momentumPhase(), event.granularity(), event.modifiers(), event.timestamp())
    , m_nativeEvent(event.nativeEvent() ? gdk_event_copy(event.nativeEvent()) : nullptr)
{
}

} // namespace WebKit
