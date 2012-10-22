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

void WebMouseEvent::encode(CoreIPC::ArgumentEncoder* encoder) const
{
    WebEvent::encode(encoder);

    encoder->encode(m_button);
    encoder->encode(m_position);
    encoder->encode(m_globalPosition);
    encoder->encode(m_deltaX);
    encoder->encode(m_deltaY);
    encoder->encode(m_deltaZ);
    encoder->encode(m_clickCount);

#if PLATFORM(WIN)
    encoder->encode(m_didActivateWebView);
#endif
}

bool WebMouseEvent::decode(CoreIPC::ArgumentDecoder* decoder, WebMouseEvent& result)
{
    if (!WebEvent::decode(decoder, result))
        return false;

    if (!decoder->decode(result.m_button))
        return false;
    if (!decoder->decode(result.m_position))
        return false;
    if (!decoder->decode(result.m_globalPosition))
        return false;
    if (!decoder->decode(result.m_deltaX))
        return false;
    if (!decoder->decode(result.m_deltaY))
        return false;
    if (!decoder->decode(result.m_deltaZ))
        return false;
    if (!decoder->decode(result.m_clickCount))
        return false;
#if PLATFORM(WIN)
    if (!decoder->decode(result.m_didActivateWebView))
        return false;
#endif

    return true;
}

bool WebMouseEvent::isMouseEventType(Type type)
{
    return type == MouseDown || type == MouseUp || type == MouseMove;
}
    
} // namespace WebKit
