/*
 * Copyright (C) 2018 Apple Inc.
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
#include "LibWebRTCRtpSenderBackend.h"

#if ENABLE(WEB_RTC) && USE(LIBWEBRTC)

#include "LibWebRTCPeerConnectionBackend.h"
#include "LibWebRTCUtils.h"
#include "RTCPeerConnection.h"
#include "RTCRtpSender.h"

namespace WebCore {

void LibWebRTCRtpSenderBackend::replaceTrack(RTCRtpSender& sender, RefPtr<MediaStreamTrack>&& track, DOMPromiseDeferred<void>&& promise)
{
    if (!m_peerConnectionBackend) {
        promise.reject(Exception { InvalidStateError, "No WebRTC backend"_s });
        return;
    }

    m_peerConnectionBackend->replaceTrack(sender, WTFMove(track), WTFMove(promise));
}

RTCRtpParameters LibWebRTCRtpSenderBackend::getParameters() const
{
    if (!m_rtcSender)
        return { };

    return toRTCRtpParameters(m_rtcSender->GetParameters());
}

void LibWebRTCRtpSenderBackend::setParameters(const RTCRtpParameters& parameters, DOMPromiseDeferred<void>&& promise)
{
    if (!m_rtcSender) {
        promise.reject(NotSupportedError);
        return;
    }
    auto error = m_rtcSender->SetParameters(fromRTCRtpParameters(parameters));
    if (!error.ok()) {
        promise.reject(Exception { InvalidStateError, error.message() });
        return;
    }
    promise.resolve();
}

} // namespace WebCore

#endif // ENABLE(WEB_RTC) && USE(LIBWEBRTC)
