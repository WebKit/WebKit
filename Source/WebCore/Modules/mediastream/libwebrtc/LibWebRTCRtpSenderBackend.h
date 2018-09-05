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

#pragma once

#if ENABLE(WEB_RTC)

#include "LibWebRTCMacros.h"
#include "RTCRtpSenderBackend.h"
#include <wtf/WeakPtr.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

#include <webrtc/api/rtpsenderinterface.h>
#include <webrtc/rtc_base/scoped_ref_ptr.h>

#pragma GCC diagnostic pop

namespace WebCore {

class LibWebRTCPeerConnectionBackend;

class LibWebRTCRtpSenderBackend final : public RTCRtpSenderBackend {
public:
    explicit LibWebRTCRtpSenderBackend(LibWebRTCPeerConnectionBackend& backend, rtc::scoped_refptr<webrtc::RtpSenderInterface>&& rtcSender)
        : m_peerConnectionBackend(makeWeakPtr(&backend))
        , m_rtcSender(WTFMove(rtcSender))
    {
    }

    void setRTCSender(rtc::scoped_refptr<webrtc::RtpSenderInterface>&& rtcSender) { m_rtcSender = WTFMove(rtcSender); }
    webrtc::RtpSenderInterface* rtcSender() { return m_rtcSender.get(); }

private:
    void replaceTrack(RTCRtpSender&, RefPtr<MediaStreamTrack>&&, DOMPromiseDeferred<void>&&) final;
    RTCRtpParameters getParameters() const final;

    WeakPtr<LibWebRTCPeerConnectionBackend> m_peerConnectionBackend;
    rtc::scoped_refptr<webrtc::RtpSenderInterface> m_rtcSender;
};

} // namespace WebCore

#endif // ENABLE(WEB_RTC)
