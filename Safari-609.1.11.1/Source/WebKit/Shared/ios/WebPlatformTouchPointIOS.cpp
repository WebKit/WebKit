/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#if ENABLE(TOUCH_EVENTS)

#include "WebCoreArgumentCoders.h"

namespace WebKit {
using namespace WebCore;

void WebPlatformTouchPoint::encode(IPC::Encoder& encoder) const
{
    encoder << m_identifier;
    encoder << m_location;
    encoder << m_phase;
#if ENABLE(IOS_TOUCH_EVENTS)
    encoder << m_radiusX;
    encoder << m_radiusY;
    encoder << m_rotationAngle;
    encoder << m_force;
    encoder << m_altitudeAngle;
    encoder << m_azimuthAngle;
    encoder << m_touchType;
#endif
}

Optional<WebPlatformTouchPoint> WebPlatformTouchPoint::decode(IPC::Decoder& decoder)
{
    WebPlatformTouchPoint result;
    if (!decoder.decode(result.m_identifier))
        return WTF::nullopt;
    if (!decoder.decode(result.m_location))
        return WTF::nullopt;
    if (!decoder.decode(result.m_phase))
        return WTF::nullopt;
#if ENABLE(IOS_TOUCH_EVENTS)
    if (!decoder.decode(result.m_radiusX))
        return WTF::nullopt;
    if (!decoder.decode(result.m_radiusY))
        return WTF::nullopt;
    if (!decoder.decode(result.m_rotationAngle))
        return WTF::nullopt;
    if (!decoder.decode(result.m_force))
        return WTF::nullopt;
    if (!decoder.decode(result.m_altitudeAngle))
        return WTF::nullopt;
    if (!decoder.decode(result.m_azimuthAngle))
        return WTF::nullopt;
    if (!decoder.decode(result.m_touchType))
        return WTF::nullopt;
#endif
    return WTFMove(result);
}

} // namespace WebKit

#endif // ENABLE(TOUCH_EVENTS)
