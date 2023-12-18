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

void LibWebRTCRtpTransformBackend::addOutputCallback(rtc::scoped_refptr<webrtc::TransformedFrameCallback>&& callback, uint32_t ssrc)
{
    Locker locker { m_outputCallbacksLock };
    m_outputCallbacks.insert_or_assign(ssrc, WTFMove(callback));
}

void LibWebRTCRtpTransformBackend::removeOutputCallback(uint32_t ssrc)
{
    Locker locker { m_outputCallbacksLock };
    m_outputCallbacks.erase(ssrc);
}

void LibWebRTCRtpTransformBackend::sendFrameToOutput(std::unique_ptr<webrtc::TransformableFrameInterface>&& rtcFrame)
{
    Locker locker { m_outputCallbacksLock };
    if (m_outputCallbacks.size() == 1) {
        m_outputCallbacks.begin()->second->OnTransformedFrame(WTFMove(rtcFrame));
        return;
    }
    auto iterator = m_outputCallbacks.find(rtcFrame->GetSsrc());
    if (iterator != m_outputCallbacks.end())
        iterator->second->OnTransformedFrame(WTFMove(rtcFrame));
}

void LibWebRTCRtpTransformBackend::processTransformedFrame(RTCRtpTransformableFrame& frame)
{
    if (auto rtcFrame = static_cast<LibWebRTCRtpTransformableFrame&>(frame).takeRTCFrame())
        sendFrameToOutput(WTFMove(rtcFrame));
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
    sendFrameToOutput(WTFMove(rtcFrame));
}

void LibWebRTCRtpTransformBackend::RegisterTransformedFrameCallback(rtc::scoped_refptr<webrtc::TransformedFrameCallback> callback)
{
    addOutputCallback(WTFMove(callback), 0);
}

void LibWebRTCRtpTransformBackend::RegisterTransformedFrameSinkCallback(rtc::scoped_refptr<webrtc::TransformedFrameCallback> callback, uint32_t ssrc)
{
    addOutputCallback(WTFMove(callback), ssrc);
}

void LibWebRTCRtpTransformBackend::UnregisterTransformedFrameCallback()
{
    removeOutputCallback(0);
}

void LibWebRTCRtpTransformBackend::UnregisterTransformedFrameSinkCallback(uint32_t ssrc)
{
    removeOutputCallback(ssrc);
}

} // namespace WebCore

#endif // ENABLE(WEB_RTC) && USE(LIBWEBRTC)
