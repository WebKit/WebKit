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
#include "LibWebRTCRtpReceiverTransformBackend.h"

#if ENABLE(WEB_RTC) && USE(LIBWEBRTC)

#include "LibWebRTCRtpTransformableFrame.h"

namespace WebCore {

static inline LibWebRTCRtpReceiverTransformBackend::MediaType mediaTypeFromReceiver(const webrtc::RtpReceiverInterface& receiver)
{
    return receiver.media_type() == cricket::MEDIA_TYPE_AUDIO ? RTCRtpTransformBackend::MediaType::Audio : RTCRtpTransformBackend::MediaType::Video;
}

LibWebRTCRtpReceiverTransformBackend::LibWebRTCRtpReceiverTransformBackend(rtc::scoped_refptr<webrtc::RtpReceiverInterface> rtcReceiver)
    : LibWebRTCRtpTransformBackend(mediaTypeFromReceiver(*rtcReceiver), Side::Receiver)
    , m_rtcReceiver(WTFMove(rtcReceiver))
{
}

LibWebRTCRtpReceiverTransformBackend::~LibWebRTCRtpReceiverTransformBackend()
{
}

void LibWebRTCRtpReceiverTransformBackend::setTransformableFrameCallback(Callback&& callback)
{
    setInputCallback(WTFMove(callback));
    if (m_isRegistered)
        return;

    m_isRegistered = true;
    m_rtcReceiver->SetDepacketizerToDecoderFrameTransformer(this);
}

void LibWebRTCRtpReceiverTransformBackend::requestKeyFrame()
{
    ASSERT(mediaType() == MediaType::Video);
    m_rtcReceiver->GenerateKeyFrame();
}

} // namespace WebCore

#endif // ENABLE(WEB_RTC) && USE(LIBWEBRTC)
