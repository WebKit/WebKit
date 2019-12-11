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
#include "WebEvent.h"

#include "WebCoreArgumentCoders.h"

namespace WebKit {
using namespace WebCore;

WebMouseEvent::WebMouseEvent()
    : WebEvent()
    , m_button(static_cast<uint32_t>(NoButton))
    , m_deltaX(0)
    , m_deltaY(0)
    , m_deltaZ(0)
    , m_clickCount(0)
#if PLATFORM(MAC)
    , m_eventNumber(-1)
    , m_menuTypeForEvent(0)
#endif
{
}

#if PLATFORM(MAC)
WebMouseEvent::WebMouseEvent(Type type, Button button, unsigned short buttons, const IntPoint& position, const IntPoint& globalPosition, float deltaX, float deltaY, float deltaZ, int clickCount, OptionSet<Modifier> modifiers, WallTime timestamp, double force, SyntheticClickType syntheticClickType, int eventNumber, int menuType)
#else
WebMouseEvent::WebMouseEvent(Type type, Button button, unsigned short buttons, const IntPoint& position, const IntPoint& globalPosition, float deltaX, float deltaY, float deltaZ, int clickCount, OptionSet<Modifier> modifiers, WallTime timestamp, double force, SyntheticClickType syntheticClickType)
#endif
    : WebEvent(type, modifiers, timestamp)
    , m_button(button)
    , m_buttons(buttons)
    , m_position(position)
    , m_globalPosition(globalPosition)
    , m_deltaX(deltaX)
    , m_deltaY(deltaY)
    , m_deltaZ(deltaZ)
    , m_clickCount(clickCount)
#if PLATFORM(MAC)
    , m_eventNumber(eventNumber)
    , m_menuTypeForEvent(menuType)
#endif
    , m_force(force)
    , m_syntheticClickType(syntheticClickType)
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
#endif
    encoder << m_force;
    encoder << m_syntheticClickType;
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
#endif
    if (!decoder.decode(result.m_force))
        return false;

    if (!decoder.decode(result.m_syntheticClickType))
        return false;

    return true;
}

bool WebMouseEvent::isMouseEventType(Type type)
{
    return type == MouseDown || type == MouseUp || type == MouseMove || type == MouseForceUp || type == MouseForceDown || type == MouseForceChanged;
}
    
} // namespace WebKit
