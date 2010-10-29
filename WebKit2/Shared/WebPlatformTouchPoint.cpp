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

#if ENABLE(TOUCH_EVENTS)

#include "WebEvent.h"

#include "ArgumentDecoder.h"
#include "ArgumentEncoder.h"

namespace WebKit {

WebPlatformTouchPoint::WebPlatformTouchPoint(unsigned id, TouchPointState state, int screenPosX, int screenPosY, int posX, int posY)
    : m_id(id)
    , m_state(state)
    , m_screenPosX(screenPosX)
    , m_screenPosY(screenPosY)
    , m_posX(posX)
    , m_posY(posY)
{
}

void WebPlatformTouchPoint::encode(CoreIPC::ArgumentEncoder* encoder) const
{
    encoder->encodeBytes(reinterpret_cast<const uint8_t*>(this), sizeof(*this));
}

bool WebPlatformTouchPoint::decode(CoreIPC::ArgumentDecoder* decoder, WebPlatformTouchPoint& t)
{
    return decoder->decodeBytes(reinterpret_cast<uint8_t*>(&t), sizeof(t));
}

} // namespace WebKit

#endif // ENABLE(TOUCH_EVENTS)
