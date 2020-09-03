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

#include "LibWebRTCObservers.h"
#include "LibWebRTCProvider.h"
#include "LibWebRTCRtpSenderBackend.h"
#include "RTCRtpReceiver.h"
#include <Timer.h>

ALLOW_UNUSED_PARAMETERS_BEGIN

#include <webrtc/api/jsep.h>
#include <webrtc/api/peer_connection_interface.h>
#include <webrtc/pc/peer_connection_factory.h>
#include <webrtc/pc/rtc_stats_collector.h>

ALLOW_UNUSED_PARAMETERS_END

#include <wtf/LoggerHelper.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace webrtc {
class CreateSessionDescriptionObserver;
class DataChannelInterface;
class IceCandidateInterface;
class MediaStreamInterface;
class PeerConnectionObserver;
class SessionDescriptionInterface;
class SetSessionDescriptionObserver;
}

namespace WebCore {
class LibWebRTCProvider;
class LibWebRTCPeerConnectionBackend;
class LibWebRTCRtpReceiverBackend;
class LibWebRTCRtpTransceiverBackend;
class LibWebRTCStatsCollector;
class MediaStreamTrack;
class RTCSessionDescription;

class LibWebRTCMediaEndpoint
    : public ThreadSafeRefCounted<LibWebRTCMediaEndpoint, WTF::DestructionThread::Main>
    , private webrtc::PeerConnectionObserver
    , private webrtc::RTCStatsCollectorCallback
#if !RELEASE_LOG_DISABLED
    , private LoggerHelper
#endif
{
public:
    static Ref<LibWebRTCMediaEndpoint> create(LibWebRTCPeerConnectionBackend& peerConnection, LibWebRTCProvider& client) { return adoptRef(*new LibWebRTCMediaEndpoint(peerConnection, client)); }
    virtual ~LibWebRTCMediaEndpoint() = default;

    void restartIce();
    bool setConfiguration(LibWebRTCProvider&, webrtc::PeerConnectionInterface::RTCConfiguration&&);

    webrtc::PeerConnectionInterface& backend() const { ASSERT(m_backend); return *m_backend.get(); }
    void doSetLocalDescription(const RTCSessionDescription*);
    void doSetRemoteDescription(const RTCSessionDescription&);
    void doCreateOffer(const RTCOfferOptions&);
    void doCreateAnswer();
    void getStats(Ref<DeferredPromise>&&);
    void getStats(webrtc::RtpReceiverInterface&, Ref<DeferredPromise>&&);
    void getStats(webrtc::RtpSenderInterface&, Ref<DeferredPromise>&&);
    std::unique_ptr<RTCDataChannelHandler> createDataChannel(const String&, const RTCDataChannelInit&);
    bool addIceCandidate(webrtc::IceCandidateInterface& candidate) { return m_backend->AddIceCandidate(&candidate); }

    void close();
    void stop();
    bool isStopped() const { return !m_backend; }

    RefPtr<RTCSessionDescription> localDescription() const;
    RefPtr<RTCSessionDescription> remoteDescription() const;
    RefPtr<RTCSessionDescription> currentLocalDescription() const;
    RefPtr<RTCSessionDescription> currentRemoteDescription() const;
    RefPtr<RTCSessionDescription> pendingLocalDescription() const;
    RefPtr<RTCSessionDescription> pendingRemoteDescription() const;

    bool addTrack(LibWebRTCRtpSenderBackend&, MediaStreamTrack&, const Vector<String>&);
    void removeTrack(LibWebRTCRtpSenderBackend&);

    struct Backends {
        std::unique_ptr<LibWebRTCRtpSenderBackend> senderBackend;
        std::unique_ptr<LibWebRTCRtpReceiverBackend> receiverBackend;
        std::unique_ptr<LibWebRTCRtpTransceiverBackend> transceiverBackend;
    };
    Optional<Backends> addTransceiver(const String& trackKind, const RTCRtpTransceiverInit&);
    Optional<Backends> addTransceiver(MediaStreamTrack&, const RTCRtpTransceiverInit&);
    std::unique_ptr<LibWebRTCRtpTransceiverBackend> transceiverBackendFromSender(LibWebRTCRtpSenderBackend&);

    void setSenderSourceFromTrack(LibWebRTCRtpSenderBackend&, MediaStreamTrack&);
    void collectTransceivers();

    void suspend();
    void resume();

private:
    LibWebRTCMediaEndpoint(LibWebRTCPeerConnectionBackend&, LibWebRTCProvider&);

    // webrtc::PeerConnectionObserver API
    void OnSignalingChange(webrtc::PeerConnectionInterface::SignalingState) final;
    void OnDataChannel(rtc::scoped_refptr<webrtc::DataChannelInterface>) final;
    void OnTrack(rtc::scoped_refptr<webrtc::RtpTransceiverInterface>) final;
    void OnRemoveTrack(rtc::scoped_refptr<webrtc::RtpReceiverInterface>) final;

    void OnRenegotiationNeeded() final;
    void OnIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState) final;
    void OnIceGatheringChange(webrtc::PeerConnectionInterface::IceGatheringState) final;
    void OnIceCandidate(const webrtc::IceCandidateInterface*) final;
    void OnIceCandidatesRemoved(const std::vector<cricket::Candidate>&) final;

    void createSessionDescriptionSucceeded(std::unique_ptr<webrtc::SessionDescriptionInterface>&&);
    void createSessionDescriptionFailed(ExceptionCode, const char*);
    void setLocalSessionDescriptionSucceeded();
    void setLocalSessionDescriptionFailed(ExceptionCode, const char*);
    void setRemoteSessionDescriptionSucceeded();
    void setRemoteSessionDescriptionFailed(ExceptionCode, const char*);
    void newTransceiver(rtc::scoped_refptr<webrtc::RtpTransceiverInterface>&&);
    void removeRemoteTrack(rtc::scoped_refptr<webrtc::RtpReceiverInterface>&&);

    void addPendingTrackEvent(Ref<RTCRtpReceiver>&&, MediaStreamTrack&, const std::vector<rtc::scoped_refptr<webrtc::MediaStreamInterface>>&, RefPtr<RTCRtpTransceiver>&&);

    template<typename T>
    Optional<Backends> createTransceiverBackends(T&&, const RTCRtpTransceiverInit&, LibWebRTCRtpSenderBackend::Source&&);

    void OnStatsDelivered(const rtc::scoped_refptr<const webrtc::RTCStatsReport>&) final;
    void gatherStatsForLogging();
    void startLoggingStats();
    void stopLoggingStats();

    rtc::scoped_refptr<LibWebRTCStatsCollector> createStatsCollector(Ref<DeferredPromise>&&);

    MediaStream& mediaStreamFromRTCStream(webrtc::MediaStreamInterface&);

    void AddRef() const { ref(); }
    rtc::RefCountReleaseStatus Release() const
    {
        auto result = refCount() - 1;
        deref();
        return result ? rtc::RefCountReleaseStatus::kOtherRefsRemained
        : rtc::RefCountReleaseStatus::kDroppedLastRef;
    }

    std::pair<LibWebRTCRtpSenderBackend::Source, rtc::scoped_refptr<webrtc::MediaStreamTrackInterface>> createSourceAndRTCTrack(MediaStreamTrack&);
    RefPtr<RealtimeMediaSource> sourceFromNewReceiver(webrtc::RtpReceiverInterface&);

#if !RELEASE_LOG_DISABLED
    const Logger& logger() const final { return m_logger.get(); }
    const void* logIdentifier() const final { return m_logIdentifier; }
    const char* logClassName() const final { return "LibWebRTCMediaEndpoint"; }
    WTFLogChannel& logChannel() const final;

    Seconds statsLogInterval(int64_t) const;
#endif

    LibWebRTCPeerConnectionBackend& m_peerConnectionBackend;
    webrtc::PeerConnectionFactoryInterface& m_peerConnectionFactory;
    rtc::scoped_refptr<webrtc::PeerConnectionInterface> m_backend;

    friend CreateSessionDescriptionObserver<LibWebRTCMediaEndpoint>;
    friend SetLocalSessionDescriptionObserver<LibWebRTCMediaEndpoint>;
    friend SetRemoteSessionDescriptionObserver<LibWebRTCMediaEndpoint>;

    CreateSessionDescriptionObserver<LibWebRTCMediaEndpoint> m_createSessionDescriptionObserver;
    SetLocalSessionDescriptionObserver<LibWebRTCMediaEndpoint> m_setLocalSessionDescriptionObserver;
    SetRemoteSessionDescriptionObserver<LibWebRTCMediaEndpoint> m_setRemoteSessionDescriptionObserver;

    HashMap<String, RefPtr<MediaStream>> m_remoteStreamsById;
    HashMap<MediaStreamTrack*, Vector<String>> m_remoteStreamsFromRemoteTrack;

    bool m_isInitiator { false };
    Timer m_statsLogTimer;

    HashMap<String, rtc::scoped_refptr<webrtc::MediaStreamInterface>> m_localStreams;

    std::unique_ptr<LibWebRTCProvider::SuspendableSocketFactory> m_rtcSocketFactory;
#if !RELEASE_LOG_DISABLED
    int64_t m_statsFirstDeliveredTimestamp { 0 };
    Ref<const Logger> m_logger;
    const void* m_logIdentifier;
#endif
};

} // namespace WebCore

#endif // USE(LIBWEBRTC)
