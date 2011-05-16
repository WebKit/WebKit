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

#include "Arguments.h"
#include "WebCoreArgumentCoders.h"

using namespace WebCore;

namespace WebKit {

WebMouseEvent::WebMouseEvent()
    : WebEvent()
    , m_button(static_cast<uint32_t>(NoButton))
    , m_deltaX(0)
    , m_deltaY(0)
    , m_deltaZ(0)
    , m_clickCount(0)
#if PLATFORM(WIN)
    , m_didActivateWebView(false)
#endif
{
}

WebMouseEvent::WebMouseEvent(Type type, Button button, const IntPoint& position, const IntPoint& globalPosition, float deltaX, float deltaY, float deltaZ, int clickCount, Modifiers modifiers, double timestamp)
    : WebEvent(type, modifiers, timestamp)
    , m_button(button)
    , m_position(position)
    , m_globalPosition(globalPosition)
    , m_deltaX(deltaX)
    , m_deltaY(deltaY)
    , m_deltaZ(deltaZ)
    , m_clickCount(clickCount)
#if PLATFORM(WIN)
    , m_didActivateWebView(false)
#endif
{
    ASSERT(isMouseEventType(type));
}

#if PLATFORM(WIN)
WebMouseEvent::WebMouseEvent(Type type, Button button, const IntPoint& position, const IntPoint& globalPosition, float deltaX, float deltaY, float deltaZ, int clickCount, Modifiers modifiers, double timestamp, bool didActivateWebView)
    : WebEvent(type, modifiers, timestamp)
    , m_button(button)
    , m_position(position)
    , m_globalPosition(globalPosition)
    , m_deltaX(deltaX)
    , m_deltaY(deltaY)
    , m_deltaZ(deltaZ)
    , m_clickCount(clickCount)
    , m_didActivateWebView(didActivateWebView)
{
    ASSERT(isMouseEventType(type));
}
#endif

WebMouseEvent::WebMouseEvent(const WebMouseEvent& event, float pageScaleFactor)
    : WebEvent(event.type(), event.modifiers(), event.timestamp())
    , m_button(event.button())
    , m_position(WebCore::IntPoint(event.position().x() / pageScaleFactor, event.position().y() / pageScaleFactor))
    , m_globalPosition(m_position + (event.globalPosition() - event.position()))
    , m_deltaX(event.deltaX())
    , m_deltaY(event.deltaY())
    , m_deltaZ(event.deltaZ())
    , m_clickCount(event.clickCount())
#if PLATFORM(WIN)
    , m_didActivateWebView(false)
#endif
{
}

void WebMouseEvent::encode(CoreIPC::ArgumentEncoder* encoder) const
{
    WebEvent::encode(encoder);

#if PLATFORM(WIN)
    // Include m_didActivateWebView on Windows.
    encoder->encode(CoreIPC::In(m_button, m_position, m_globalPosition, m_deltaX, m_deltaY, m_deltaZ, m_clickCount, m_didActivateWebView));
#else
    encoder->encode(CoreIPC::In(m_button, m_position, m_globalPosition, m_deltaX, m_deltaY, m_deltaZ, m_clickCount));
#endif
}

bool WebMouseEvent::decode(CoreIPC::ArgumentDecoder* decoder, WebMouseEvent& t)
{
    if (!WebEvent::decode(decoder, t))
        return false;

#if PLATFORM(WIN)
    // Include m_didActivateWebView on Windows.
    return decoder->decode(CoreIPC::Out(t.m_button, t.m_position, t.m_globalPosition, t.m_deltaX, t.m_deltaY, t.m_deltaZ, t.m_clickCount, t.m_didActivateWebView));
#else
    return decoder->decode(CoreIPC::Out(t.m_button, t.m_position, t.m_globalPosition, t.m_deltaX, t.m_deltaY, t.m_deltaZ, t.m_clickCount));
#endif
}

bool WebMouseEvent::isMouseEventType(Type type)
{
    return type == MouseDown || type == MouseUp || type == MouseMove;
}
    
} // namespace WebKit
