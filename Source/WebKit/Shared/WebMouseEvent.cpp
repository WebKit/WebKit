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
WebMouseEvent::WebMouseEvent(Type type, Button button, unsigned short buttons, const IntPoint& positionInView, const IntPoint& globalPosition, float deltaX, float deltaY, float deltaZ, int clickCount, OptionSet<Modifier> modifiers, WallTime timestamp, double force, SyntheticClickType syntheticClickType, int eventNumber, int menuType, GestureWasCancelled gestureWasCancelled)
#elif PLATFORM(GTK)
WebMouseEvent::WebMouseEvent(Type type, Button button, unsigned short buttons, const IntPoint& positionInView, const IntPoint& globalPosition, float deltaX, float deltaY, float deltaZ, int clickCount, OptionSet<Modifier> modifiers, WallTime timestamp, double force, SyntheticClickType syntheticClickType, PlatformMouseEvent::IsTouch isTouchEvent, WebCore::PointerID pointerId, const String& pointerType, GestureWasCancelled gestureWasCancelled)
#else
WebMouseEvent::WebMouseEvent(Type type, Button button, unsigned short buttons, const IntPoint& positionInView, const IntPoint& globalPosition, float deltaX, float deltaY, float deltaZ, int clickCount, OptionSet<Modifier> modifiers, WallTime timestamp, double force, SyntheticClickType syntheticClickType, WebCore::PointerID pointerId, const String& pointerType, GestureWasCancelled gestureWasCancelled)
#endif
    : WebEvent(type, modifiers, timestamp)
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
    ASSERT(isMouseEventType(type));
}

void WebMouseEvent::encode(IPC::Encoder& encoder) const
{
    WebEvent::encode(encoder);

    encoder << m_button;
    encoder << m_buttons;
    encoder << m_position;
    encoder << m_globalPosition;
    encoder << m_deltaX;
    encoder << m_deltaY;
    encoder << m_deltaZ;
    encoder << m_clickCount;
#if PLATFORM(MAC)
    encoder << m_eventNumber;
    encoder << m_menuTypeForEvent;
#elif PLATFORM(GTK)
    encoder << m_isTouchEvent;
#endif
    encoder << m_force;
    encoder << m_syntheticClickType;
    encoder << m_pointerId;
    encoder << m_pointerType;
    encoder << m_gestureWasCancelled;
}

bool WebMouseEvent::decode(IPC::Decoder& decoder, WebMouseEvent& result)
{
    if (!WebEvent::decode(decoder, result))
        return false;

    if (!decoder.decode(result.m_button))
        return false;
    if (!decoder.decode(result.m_buttons))
        return false;
    if (!decoder.decode(result.m_position))
        return false;
    if (!decoder.decode(result.m_globalPosition))
        return false;
    if (!decoder.decode(result.m_deltaX))
        return false;
    if (!decoder.decode(result.m_deltaY))
        return false;
    if (!decoder.decode(result.m_deltaZ))
        return false;
    if (!decoder.decode(result.m_clickCount))
        return false;
#if PLATFORM(MAC)
    if (!decoder.decode(result.m_eventNumber))
        return false;
    if (!decoder.decode(result.m_menuTypeForEvent))
        return false;
#elif PLATFORM(GTK)
    if (!decoder.decode(result.m_isTouchEvent))
        return false;
#endif
    if (!decoder.decode(result.m_force))
        return false;

    if (!decoder.decode(result.m_syntheticClickType))
        return false;
    if (!decoder.decode(result.m_pointerId))
        return false;
    if (!decoder.decode(result.m_pointerType))
        return false;
    if (!decoder.decode(result.m_gestureWasCancelled))
        return false;

    return true;
}

bool WebMouseEvent::isMouseEventType(Type type)
{
    return type == MouseDown || type == MouseUp || type == MouseMove || type == MouseForceUp || type == MouseForceDown || type == MouseForceChanged;
}
    
} // namespace WebKit
