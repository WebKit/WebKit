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

namespace WebKit {    

WebWheelEvent::WebWheelEvent(Type type, int x, int y, int globalX, int globalY, float deltaX, float deltaY, float wheelTicksX, float wheelTicksY, Granularity granularity, Modifiers modifiers, double timestamp)
    : WebEvent(type, modifiers, timestamp)
    , m_positionX(x)
    , m_positionY(y)
    , m_globalPositionX(globalX)
    , m_globalPositionY(globalY)
    , m_deltaX(deltaX)
    , m_deltaY(deltaY)
    , m_wheelTicksX(wheelTicksX)
    , m_wheelTicksY(wheelTicksY)
    , m_granularity(granularity)
{
    ASSERT(isWheelEventType(type));
}

void WebWheelEvent::encode(CoreIPC::ArgumentEncoder* encoder) const
{
    encoder->encodeBytes(reinterpret_cast<const uint8_t*>(this), sizeof(*this));
}

bool WebWheelEvent::decode(CoreIPC::ArgumentDecoder* decoder, WebWheelEvent& t)
{
    return decoder->decodeBytes(reinterpret_cast<uint8_t*>(&t), sizeof(t));
}

bool WebWheelEvent::isWheelEventType(Type type)
{
    return type == Wheel;
}

} // namespace WebKit
