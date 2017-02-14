/*
 * Copyright (C) 2017 Apple Inc.
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

#include "LibWebRTCProvider.h"
#include "PeerConnectionBackend.h"
#include "RealtimeOutgoingAudioSource.h"
#include "RealtimeOutgoingVideoSource.h"

#include <webrtc/api/jsep.h>
#include <webrtc/api/peerconnectionfactory.h>
#include <webrtc/api/peerconnectioninterface.h>
#include <webrtc/api/rtcstatscollector.h>

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
class MediaStreamTrack;
class RTCSessionDescription;

class LibWebRTCMediaEndpoint : public ThreadSafeRefCounted<LibWebRTCMediaEndpoint>, private webrtc::PeerConnectionObserver {
public:
    static Ref<LibWebRTCMediaEndpoint> create(LibWebRTCPeerConnectionBackend& peerConnection, LibWebRTCProvider& client) { return adoptRef(*new LibWebRTCMediaEndpoint(peerConnection, client)); }
    virtual ~LibWebRTCMediaEndpoint() { }

    webrtc::PeerConnectionInterface& backend() const { ASSERT(m_backend); return *m_backend.get(); }
    void doSetLocalDescription(RTCSessionDescription&);
    void doSetRemoteDescription(RTCSessionDescription&);
    void doCreateOffer();
    void doCreateAnswer();
    void getStats(MediaStreamTrack*, const DeferredPromise&);
    std::unique_ptr<RTCDataChannelHandler> createDataChannel(const String&, const RTCDataChannelInit&);
    bool addIceCandidate(webrtc::IceCandidateInterface& candidate) { return m_backend->AddIceCandidate(&candidate); }

    void stop();
    bool isStopped() const { return !m_backend; }

    RefPtr<RTCSessionDescription> localDescription() const;
    RefPtr<RTCSessionDescription> remoteDescription() const;

private:
    LibWebRTCMediaEndpoint(LibWebRTCPeerConnectionBackend&, LibWebRTCProvider&);

    // webrtc::PeerConnectionObserver API
    void OnSignalingChange(webrtc::PeerConnectionInterface::SignalingState) final;
    void OnAddStream(rtc::scoped_refptr<webrtc::MediaStreamInterface>) final;
    void OnRemoveStream(rtc::scoped_refptr<webrtc::MediaStreamInterface>) final;
    void OnDataChannel(rtc::scoped_refptr<webrtc::DataChannelInterface>) final;
    void OnRenegotiationNeeded() final;
    void OnIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState) final;
    void OnIceGatheringChange(webrtc::PeerConnectionInterface::IceGatheringState) final;
    void OnIceCandidate(const webrtc::IceCandidateInterface*) final;
    void OnIceCandidatesRemoved(const std::vector<cricket::Candidate>&) final;

    void createSessionDescriptionSucceeded(webrtc::SessionDescriptionInterface*);
    void createSessionDescriptionFailed(const std::string&);
    void setLocalSessionDescriptionSucceeded();
    void setLocalSessionDescriptionFailed(const std::string&);
    void setRemoteSessionDescriptionSucceeded();
    void setRemoteSessionDescriptionFailed(const std::string&);
    void addStream(webrtc::MediaStreamInterface&);
    void addDataChannel(rtc::scoped_refptr<webrtc::DataChannelInterface>&&);

    int AddRef() const { ref(); return static_cast<int>(refCount()); }
    int Release() const { deref(); return static_cast<int>(refCount()); }

    class CreateSessionDescriptionObserver final : public webrtc::CreateSessionDescriptionObserver {
    public:
        explicit CreateSessionDescriptionObserver(LibWebRTCMediaEndpoint &endpoint) : m_endpoint(endpoint) { }

        void OnSuccess(webrtc::SessionDescriptionInterface* sessionDescription) final { m_endpoint.createSessionDescriptionSucceeded(sessionDescription); }
        void OnFailure(const std::string& error) final { m_endpoint.createSessionDescriptionFailed(error); }

        int AddRef() const { return m_endpoint.AddRef(); }
        int Release() const { return m_endpoint.Release(); }

    private:
        LibWebRTCMediaEndpoint& m_endpoint;
    };

    class SetLocalSessionDescriptionObserver final : public webrtc::SetSessionDescriptionObserver {
    public:
        explicit SetLocalSessionDescriptionObserver(LibWebRTCMediaEndpoint &endpoint) : m_endpoint(endpoint) { }

        void OnSuccess() final { m_endpoint.setLocalSessionDescriptionSucceeded(); }
        void OnFailure(const std::string& error) final { m_endpoint.setLocalSessionDescriptionFailed(error); }

        int AddRef() const { return m_endpoint.AddRef(); }
        int Release() const { return m_endpoint.Release(); }

    private:
        LibWebRTCMediaEndpoint& m_endpoint;
    };

    class SetRemoteSessionDescriptionObserver final : public webrtc::SetSessionDescriptionObserver {
    public:
        explicit SetRemoteSessionDescriptionObserver(LibWebRTCMediaEndpoint &endpoint) : m_endpoint(endpoint) { }

        void OnSuccess() final { m_endpoint.setRemoteSessionDescriptionSucceeded(); }
        void OnFailure(const std::string& error) final { m_endpoint.setRemoteSessionDescriptionFailed(error); }

        int AddRef() const { return m_endpoint.AddRef(); }
        int Release() const { return m_endpoint.Release(); }

    private:
        LibWebRTCMediaEndpoint& m_endpoint;
    };

    class StatsCollector final : public webrtc::RTCStatsCollectorCallback {
    public:
        static rtc::scoped_refptr<StatsCollector> create(LibWebRTCMediaEndpoint& endpoint, const DeferredPromise& promise, MediaStreamTrack* track) { return new StatsCollector(endpoint, promise, track); }

        int AddRef() const { return m_endpoint.AddRef(); }
        int Release() const { return m_endpoint.Release(); }

    private:
        StatsCollector(LibWebRTCMediaEndpoint&, const DeferredPromise&, MediaStreamTrack*);

        void OnStatsDelivered(const rtc::scoped_refptr<const webrtc::RTCStatsReport>&) final;

        LibWebRTCMediaEndpoint& m_endpoint;
        const DeferredPromise& m_promise;
        String m_id;
    };

    LibWebRTCPeerConnectionBackend& m_peerConnectionBackend;
    rtc::scoped_refptr<webrtc::PeerConnectionInterface> m_backend;

    CreateSessionDescriptionObserver m_createSessionDescriptionObserver;
    SetLocalSessionDescriptionObserver m_setLocalSessionDescriptionObserver;
    SetRemoteSessionDescriptionObserver m_setRemoteSessionDescriptionObserver;

    bool m_isInitiator { false };
};

} // namespace WebCore

#endif // USE(LIBWEBRTC)
