/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#if ENABLE(WEB_RTC) && USE(LIBWEBRTC)

#include "LibWebRTCMacros.h"
#include "LibWebRTCPeerConnectionBackend.h"
#include "LibWebRTCRefWrappers.h"
#include "RTCRtpSenderBackend.h"
#include "RealtimeOutgoingAudioSource.h"
#include "RealtimeOutgoingVideoSource.h"
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <webrtc/api/rtp_sender_interface.h>
#include <webrtc/api/scoped_refptr.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakPtr.h>

namespace WebCore {
class LibWebRTCRtpSenderBackend;
}

namespace WTF {
template<typename T> struct IsDeprecatedWeakRefSmartPointerException;
template<> struct IsDeprecatedWeakRefSmartPointerException<WebCore::LibWebRTCRtpSenderBackend> : std::true_type { };
}

namespace WebCore {

class LibWebRTCPeerConnectionBackend;

class LibWebRTCRtpSenderBackend final : public RTCRtpSenderBackend, public CanMakeWeakPtr<LibWebRTCRtpSenderBackend> {
    WTF_MAKE_TZONE_ALLOCATED(LibWebRTCRtpSenderBackend);
public:
    using Source = Variant<std::nullptr_t, Ref<RealtimeOutgoingAudioSource>, Ref<RealtimeOutgoingVideoSource>>;
    LibWebRTCRtpSenderBackend(LibWebRTCPeerConnectionBackend&, RefPtr<webrtc::RtpSenderInterface>&&, Source&&);
    LibWebRTCRtpSenderBackend(LibWebRTCPeerConnectionBackend&, RefPtr<webrtc::RtpSenderInterface>&&);
    ~LibWebRTCRtpSenderBackend();

    void setRTCSender(RefPtr<webrtc::RtpSenderInterface>&& rtcSender) { m_rtcSender = WTFMove(rtcSender); }
    webrtc::RtpSenderInterface* rtcSender() { return m_rtcSender.get(); }
    RefPtr<webrtc::RtpSenderInterface> protectedRTCSender() { return m_rtcSender; }

    RealtimeOutgoingVideoSource* videoSource();
    void clearSource() { setSource(nullptr); }
    void setSource(Source&&);
    void takeSource(LibWebRTCRtpSenderBackend&);

private:
    bool replaceTrack(RTCRtpSender&, MediaStreamTrack*) final;
    RTCRtpSendParameters getParameters() const final;
    void setParameters(const RTCRtpSendParameters&, DOMPromiseDeferred<void>&&) final;
    std::unique_ptr<RTCDTMFSenderBackend> createDTMFBackend() final;
    Ref<RTCRtpTransformBackend> rtcRtpTransformBackend() final;
    std::unique_ptr<RTCDtlsTransportBackend> dtlsTransportBackend() final;
    void setMediaStreamIds(const FixedVector<String>&) final;

    void startSource();
    void stopSource();
    bool hasSource() const;

    RefPtr<LibWebRTCPeerConnectionBackend> protectedPeerConnectionBackend() const;

    WeakPtr<LibWebRTCPeerConnectionBackend> m_peerConnectionBackend;
    RefPtr<webrtc::RtpSenderInterface> m_rtcSender;
    Source m_source;
    const RefPtr<RTCRtpTransformBackend> m_transformBackend;
    mutable std::optional<webrtc::RtpParameters> m_currentParameters;
};

} // namespace WebCore

#endif // ENABLE(WEB_RTC) && USE(LIBWEBRTC)
