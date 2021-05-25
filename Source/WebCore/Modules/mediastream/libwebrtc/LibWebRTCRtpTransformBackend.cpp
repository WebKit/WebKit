/*
 * Copyright (C) 2020 Apple Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "LibWebRTCRtpTransformBackend.h"

#if ENABLE(WEB_RTC) && USE(LIBWEBRTC)

#include "LibWebRTCRtpTransformableFrame.h"

namespace WebCore {

void LibWebRTCRtpTransformBackend::setInputCallback(Callback&& callback)
{
    Locker locker { m_inputCallbackLock };
    m_inputCallback = WTFMove(callback);
}

void LibWebRTCRtpTransformBackend::clearTransformableFrameCallback()
{
    setInputCallback({ });
}

void LibWebRTCRtpTransformBackend::setOutputCallback(rtc::scoped_refptr<webrtc::TransformedFrameCallback>&& callback)
{
    Locker locker { m_outputCallbackLock };
    m_outputCallback = WTFMove(callback);
}

void LibWebRTCRtpTransformBackend::processTransformedFrame(RTCRtpTransformableFrame& frame)
{
    auto rtcFrame = static_cast<LibWebRTCRtpTransformableFrame&>(frame).takeRTCFrame();
    if (!rtcFrame)
        return;
    Locker locker { m_outputCallbackLock };
    if (m_outputCallback)
        m_outputCallback->OnTransformedFrame(WTFMove(rtcFrame));
}

void LibWebRTCRtpTransformBackend::Transform(std::unique_ptr<webrtc::TransformableFrameInterface> rtcFrame)
{
    {
        Locker locker { m_inputCallbackLock };
        if (m_inputCallback) {
            m_inputCallback(LibWebRTCRtpTransformableFrame::create(WTFMove(rtcFrame), m_mediaType == MediaType::Audio && m_side == Side::Sender));
            return;
        }
    }
    // In case of no input callback, make the transform a no-op.
    Locker locker { m_outputCallbackLock };
    if (m_outputCallback)
        m_outputCallback->OnTransformedFrame(WTFMove(rtcFrame));
}

void LibWebRTCRtpTransformBackend::RegisterTransformedFrameCallback(rtc::scoped_refptr<webrtc::TransformedFrameCallback> callback)
{
    setOutputCallback(WTFMove(callback));
}

void LibWebRTCRtpTransformBackend::RegisterTransformedFrameSinkCallback(rtc::scoped_refptr<webrtc::TransformedFrameCallback> callback, uint32_t /* ssrc */)
{
    RegisterTransformedFrameCallback(WTFMove(callback));
}

void LibWebRTCRtpTransformBackend::UnregisterTransformedFrameCallback()
{
    setOutputCallback(nullptr);
}

void LibWebRTCRtpTransformBackend::UnregisterTransformedFrameSinkCallback(uint32_t /* ssrc */)
{
    UnregisterTransformedFrameCallback();
}

} // namespace WebCore

#endif // ENABLE(WEB_RTC) && USE(LIBWEBRTC)
