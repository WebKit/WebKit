/*
 * Copyright (C) 2017-2018 Apple Inc. All rights reserved.
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

#if USE(LIBWEBRTC)

#include "PeerConnectionBackend.h"
#include <wtf/HashMap.h>

namespace webrtc {
class IceCandidateInterface;
}

namespace WebCore {

class LibWebRTCMediaEndpoint;
class LibWebRTCProvider;
class LibWebRTCRtpSenderBackend;
class LibWebRTCRtpTransceiverBackend;
class RTCRtpReceiver;
class RTCRtpReceiverBackend;
class RTCSessionDescription;
class RTCStatsReport;
class RealtimeIncomingAudioSource;
class RealtimeIncomingVideoSource;
class RealtimeMediaSource;
class RealtimeOutgoingAudioSource;
class RealtimeOutgoingVideoSource;

class LibWebRTCPeerConnectionBackend final : public PeerConnectionBackend {
    WTF_MAKE_FAST_ALLOCATED;
public:
    LibWebRTCPeerConnectionBackend(RTCPeerConnection&, LibWebRTCProvider&);
    ~LibWebRTCPeerConnectionBackend();

    void replaceTrack(RTCRtpSender&, RefPtr<MediaStreamTrack>&&, DOMPromiseDeferred<void>&&);

    bool shouldOfferAllowToReceive(const char*) const;

private:
    void doCreateOffer(RTCOfferOptions&&) final;
    void doCreateAnswer(RTCAnswerOptions&&) final;
    void doSetLocalDescription(RTCSessionDescription&) final;
    void doSetRemoteDescription(RTCSessionDescription&) final;
    void doAddIceCandidate(RTCIceCandidate&) final;
    void doStop() final;
    std::unique_ptr<RTCDataChannelHandler> createDataChannelHandler(const String&, const RTCDataChannelInit&) final;
    bool setConfiguration(MediaEndpointConfiguration&&) final;
    void getStats(Ref<DeferredPromise>&&) final;
    void getStats(RTCRtpSender&, Ref<DeferredPromise>&&) final;
    void getStats(RTCRtpReceiver&, Ref<DeferredPromise>&&) final;

    RefPtr<RTCSessionDescription> localDescription() const final;
    RefPtr<RTCSessionDescription> currentLocalDescription() const final;
    RefPtr<RTCSessionDescription> pendingLocalDescription() const final;

    RefPtr<RTCSessionDescription> remoteDescription() const final;
    RefPtr<RTCSessionDescription> currentRemoteDescription() const final;
    RefPtr<RTCSessionDescription> pendingRemoteDescription() const final;

    void emulatePlatformEvent(const String&) final { }
    void applyRotationForOutgoingVideoSources() final;

    friend class LibWebRTCMediaEndpoint;
    friend class LibWebRTCRtpSenderBackend;
    RTCPeerConnection& connection() { return m_peerConnection; }

    void getStatsSucceeded(const DeferredPromise&, Ref<RTCStatsReport>&&);

    ExceptionOr<Ref<RTCRtpSender>> addTrack(MediaStreamTrack&, Vector<String>&&) final;
    void removeTrack(RTCRtpSender&) final;

    ExceptionOr<Ref<RTCRtpTransceiver>> addTransceiver(const String&, const RTCRtpTransceiverInit&) final;
    ExceptionOr<Ref<RTCRtpTransceiver>> addTransceiver(Ref<MediaStreamTrack>&&, const RTCRtpTransceiverInit&) final;
    void setSenderSourceFromTrack(LibWebRTCRtpSenderBackend&, MediaStreamTrack&);

    RTCRtpTransceiver* existingTransceiver(WTF::Function<bool(LibWebRTCRtpTransceiverBackend&)>&&);
    RTCRtpTransceiver& newRemoteTransceiver(std::unique_ptr<LibWebRTCRtpTransceiverBackend>&&, Ref<RealtimeMediaSource>&&);

    void collectTransceivers() final;

    struct VideoReceiver {
        Ref<RTCRtpReceiver> receiver;
        Ref<RealtimeIncomingVideoSource> source;
    };
    struct AudioReceiver {
        Ref<RTCRtpReceiver> receiver;
        Ref<RealtimeIncomingAudioSource> source;
    };
    VideoReceiver videoReceiver(String&& trackId);
    AudioReceiver audioReceiver(String&& trackId);

private:
    bool isLocalDescriptionSet() const final { return m_isLocalDescriptionSet; }

    Ref<RTCRtpTransceiver> completeAddTransceiver(Ref<RTCRtpSender>&&, const RTCRtpTransceiverInit&, const String& trackId, const String& trackKind);

    Ref<RTCRtpReceiver> createReceiver(const String& trackKind, const String& trackId);

    template<typename T>
    ExceptionOr<Ref<RTCRtpTransceiver>> addUnifiedPlanTransceiver(T&& trackOrKind, const RTCRtpTransceiverInit&);

    Ref<RTCRtpReceiver> createReceiverForSource(Ref<RealtimeMediaSource>&&, std::unique_ptr<RTCRtpReceiverBackend>&&);

    Ref<LibWebRTCMediaEndpoint> m_endpoint;
    bool m_isLocalDescriptionSet { false };
    bool m_isRemoteDescriptionSet { false };

    Vector<std::unique_ptr<webrtc::IceCandidateInterface>> m_pendingCandidates;
    Vector<Ref<RTCRtpReceiver>> m_pendingReceivers;
};

} // namespace WebCore

#endif // USE(LIBWEBRTC)
