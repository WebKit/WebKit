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

#include "WebEvent.h"

#include "ArgumentDecoder.h"
#include "ArgumentEncoder.h"

using namespace WebCore;

namespace WebKit {

WebMouseEvent::WebMouseEvent(Type type, Button button, const IntPoint& position, const IntPoint& globalPosition, float deltaX, float deltaY, float deltaZ, int clickCount, Modifiers modifiers, double timestamp)
    : WebEvent(type, modifiers, timestamp)
    , m_button(button)
    , m_position(position)
    , m_globalPosition(globalPosition)
    , m_deltaX(deltaX)
    , m_deltaY(deltaY)
    , m_deltaZ(deltaZ)
    , m_clickCount(clickCount)
{
    ASSERT(isMouseEventType(type));
}

void WebMouseEvent::encode(CoreIPC::ArgumentEncoder* encoder) const
{
    encoder->encodeBytes(reinterpret_cast<const uint8_t*>(this), sizeof(*this));
}

bool WebMouseEvent::decode(CoreIPC::ArgumentDecoder* decoder, WebMouseEvent& t)
{
    return decoder->decodeBytes(reinterpret_cast<uint8_t*>(&t), sizeof(t));
}

bool WebMouseEvent::isMouseEventType(Type type)
{
    return type == MouseDown || type == MouseUp || type == MouseMove;
}
    
} // namespace WebKit
