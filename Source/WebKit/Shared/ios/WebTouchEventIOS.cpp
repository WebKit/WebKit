/*
 * Copyright (C) 2012-2014 Apple Inc. All rights reserved.
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
#include "WebTouchEvent.h"

#if ENABLE(TOUCH_EVENTS)

#include "ArgumentCoders.h"
#include "WebCoreArgumentCoders.h"

namespace WebKit {

void WebTouchEvent::encode(IPC::Encoder& encoder) const
{
    WebEvent::encode(encoder);

    encoder << m_touchPoints;
    encoder << m_position;
    encoder << m_canPreventNativeGestures;
    encoder << m_isPotentialTap;
    encoder << m_isGesture;
    encoder << m_gestureScale;
    encoder << m_gestureRotation;
}

bool WebTouchEvent::decode(IPC::Decoder& decoder, WebTouchEvent& result)
{
    if (!WebEvent::decode(decoder, result))
        return false;

    if (!decoder.decode(result.m_touchPoints))
        return false;
    if (!decoder.decode(result.m_position))
        return false;
    if (!decoder.decode(result.m_canPreventNativeGestures))
        return false;
    if (!decoder.decode(result.m_isPotentialTap))
        return false;
    if (!decoder.decode(result.m_isGesture))
        return false;
    if (!decoder.decode(result.m_gestureScale))
        return false;
    if (!decoder.decode(result.m_gestureRotation))
        return false;

    return true;
}

} // namespace WebKit

#endif // ENABLE(TOUCH_EVENTS)
