/*
 * Copyright (C) 2018-2026 Apple Inc. All rights reserved.
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
#include "LibWebRTCRefWrappers.h"
#include "RTCRtpReceiverBackend.h"
#include "RealtimeMediaSource.h"
#include <webrtc/api/scoped_refptr.h>
#include <wtf/TZoneMalloc.h>

namespace WebCore {
class Document;
class LibWebRTCRtpReceiverBackend;
class RealtimeMediaSource;

struct LibWebRTCRtpReceiverBackendAndSource {
    UniqueRef<LibWebRTCRtpReceiverBackend> backend;
    Ref<RealtimeMediaSource> source;
};

class LibWebRTCRtpReceiverBackend final : public RTCRtpReceiverBackend, public webrtc::RtpReceiverObserverInterface {
    WTF_MAKE_TZONE_ALLOCATED(LibWebRTCRtpReceiverBackend);
public:
    static LibWebRTCRtpReceiverBackendAndSource create(Document&, Ref<webrtc::RtpReceiverInterface>&&);

    LibWebRTCRtpReceiverBackend(Ref<webrtc::RtpReceiverInterface>&&, RealtimeMediaSource&);
    ~LibWebRTCRtpReceiverBackend();

    webrtc::RtpReceiverInterface& rtcReceiver() { return m_rtcReceiver.get(); }

private:

    // RTCRtpReceiverBackend
    bool isLibWebRTCRtpReceiverBackend() const final { return true; }

    RTCRtpParameters getParameters() final;
    Vector<RTCRtpContributingSource> getContributingSources() const final;
    Vector<RTCRtpSynchronizationSource> getSynchronizationSources() const final;
    Ref<RTCRtpTransformBackend> rtcRtpTransformBackend() final;
    std::unique_ptr<RTCDtlsTransportBackend> dtlsTransportBackend() final;
    void setJitterBufferTarget(std::optional<double>) final;

    // webrtc::RtpReceiverObserverInterface
    void OnFirstPacketReceived(webrtc::MediaType) final { }
    void OnFirstPacketReceivedAfterReceptiveChange(webrtc::MediaType) final;

    double webrtcToWallTimeOffset() const;

    const Ref<webrtc::RtpReceiverInterface> m_rtcReceiver;
    const ThreadSafeWeakPtr<RealtimeMediaSource> m_source;
    const RefPtr<RTCRtpTransformBackend> m_transformBackend;
    mutable std::optional<double> m_webrtcToWallTimeOffset;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::LibWebRTCRtpReceiverBackend)
    static bool isType(const WebCore::RTCRtpReceiverBackend& backend) { return backend.isLibWebRTCRtpReceiverBackend(); }
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(WEB_RTC) && USE(LIBWEBRTC)
