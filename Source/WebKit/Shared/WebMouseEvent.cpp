/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
#include "WebMouseEvent.h"

#include "WebCoreArgumentCoders.h"

namespace WebKit {
using namespace WebCore;

WebMouseEvent::WebMouseEvent() = default;

#if PLATFORM(MAC)
WebMouseEvent::WebMouseEvent(WebEvent&& event, WebMouseEventButton button, unsigned short buttons, const IntPoint& positionInView, const IntPoint& globalPosition, float deltaX, float deltaY, float deltaZ, int clickCount, double force, WebMouseEventSyntheticClickType syntheticClickType, int eventNumber, int menuType, GestureWasCancelled gestureWasCancelled)
#elif PLATFORM(GTK)
WebMouseEvent::WebMouseEvent(WebEvent&& event, WebMouseEventButton button, unsigned short buttons, const IntPoint& positionInView, const IntPoint& globalPosition, float deltaX, float deltaY, float deltaZ, int clickCount, double force, WebMouseEventSyntheticClickType syntheticClickType, PlatformMouseEvent::IsTouch isTouchEvent, WebCore::PointerID pointerId, const String& pointerType, GestureWasCancelled gestureWasCancelled)
#else
WebMouseEvent::WebMouseEvent(WebEvent&& event, WebMouseEventButton button, unsigned short buttons, const IntPoint& positionInView, const IntPoint& globalPosition, float deltaX, float deltaY, float deltaZ, int clickCount, double force, WebMouseEventSyntheticClickType syntheticClickType, WebCore::PointerID pointerId, const String& pointerType, GestureWasCancelled gestureWasCancelled)
#endif
    : WebEvent(WTFMove(event))
    , m_button(button)
    , m_buttons(buttons)
    , m_position(positionInView)
    , m_globalPosition(globalPosition)
    , m_deltaX(deltaX)
    , m_deltaY(deltaY)
    , m_deltaZ(deltaZ)
    , m_clickCount(clickCount)
#if PLATFORM(MAC)
    , m_eventNumber(eventNumber)
    , m_menuTypeForEvent(menuType)
#elif PLATFORM(GTK)
    , m_isTouchEvent(isTouchEvent)
#endif
    , m_force(force)
    , m_syntheticClickType(syntheticClickType)
#if !PLATFORM(MAC)
    , m_pointerId(pointerId)
    , m_pointerType(pointerType)
#endif
    , m_gestureWasCancelled(gestureWasCancelled)
{
    ASSERT(isMouseEventType(type()));
}

bool WebMouseEvent::isMouseEventType(Type type)
{
    return type == MouseDown || type == MouseUp || type == MouseMove || type == MouseForceUp || type == MouseForceDown || type == MouseForceChanged;
}
    
} // namespace WebKit
