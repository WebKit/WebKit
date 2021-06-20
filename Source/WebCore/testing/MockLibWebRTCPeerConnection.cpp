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

#include "config.h"
#include "MockLibWebRTCPeerConnection.h"

#if USE(LIBWEBRTC)

#include "LibWebRTCProvider.h"
#include <sstream>
#include <webrtc/pc/media_stream.h>
#include <wtf/Function.h>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/Threading.h>

namespace WebCore {

static inline rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface>& getRealPeerConnectionFactory()
{
    static NeverDestroyed<rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface>> realPeerConnectionFactory;
    return realPeerConnectionFactory;
}

static inline webrtc::PeerConnectionFactoryInterface* realPeerConnectionFactory()
{
    return getRealPeerConnectionFactory().get();
}

void useRealRTCPeerConnectionFactory(LibWebRTCProvider& provider)
{
    auto& factory = getRealPeerConnectionFactory();
    if (!factory)
        return;
    provider.setPeerConnectionFactory(factory.get());
    factory = nullptr;
}

void useMockRTCPeerConnectionFactory(LibWebRTCProvider* provider, const String& testCase)
{
    if (!provider)
        return;

    if (!realPeerConnectionFactory()) {
        auto& factory = getRealPeerConnectionFactory();
        factory = provider->factory();
    }
    provider->setPeerConnectionFactory(MockLibWebRTCPeerConnectionFactory::create(testCase));
}

MockLibWebRTCPeerConnection::~MockLibWebRTCPeerConnection()
{
    // Free senders and receivers in a different thread like an actual peer connection would probably do.
    Thread::create("MockLibWebRTCPeerConnection thread", [transceivers = WTFMove(m_transceivers)] { });
}

std::vector<rtc::scoped_refptr<webrtc::RtpTransceiverInterface>> MockLibWebRTCPeerConnection::GetTransceivers() const
{
    std::vector<rtc::scoped_refptr<webrtc::RtpTransceiverInterface>> transceivers;
    transceivers.reserve(m_transceivers.size());
    for (const auto& transceiver : m_transceivers)
        transceivers.push_back(transceiver);
    return transceivers;
}

class MockLibWebRTCPeerConnectionForIceCandidates : public MockLibWebRTCPeerConnection {
public:
    explicit MockLibWebRTCPeerConnectionForIceCandidates(webrtc::PeerConnectionObserver&, unsigned delayCount = 0);
    virtual ~MockLibWebRTCPeerConnectionForIceCandidates() = default;
private:
    void gotLocalDescription() final;
    void sendCandidates();

    unsigned m_delayCount { 0 };
};

MockLibWebRTCPeerConnectionForIceCandidates::MockLibWebRTCPeerConnectionForIceCandidates(webrtc::PeerConnectionObserver& observer, unsigned delayCount)
    : MockLibWebRTCPeerConnection(observer)
    , m_delayCount(delayCount)
{
}

void MockLibWebRTCPeerConnectionForIceCandidates::gotLocalDescription()
{
    AddRef();
    sendCandidates();
}

void MockLibWebRTCPeerConnectionForIceCandidates::sendCandidates()
{
    if (m_delayCount > 0) {
        m_delayCount--;
        callOnMainThread([this] {
            LibWebRTCProvider::callOnWebRTCNetworkThread([this] {
                sendCandidates();
            });
        });
        return;
    }

    // Let's gather candidates
    LibWebRTCProvider::callOnWebRTCSignalingThread([this]() {
        MockLibWebRTCIceCandidate candidate("candidate:2013266431 1 udp 2013266432 192.168.0.100 38838 typ host generation 0", "1");
        m_observer.OnIceCandidate(&candidate);
    });

    LibWebRTCProvider::callOnWebRTCSignalingThread([this]() {
        MockLibWebRTCIceCandidate candidate("candidate:1019216383 1 tcp 1019216384 192.168.0.100 9 typ host tcptype passive generation 0", "1");
        m_observer.OnIceCandidate(&candidate);
        MockLibWebRTCIceCandidate candidateSSLTcp("candidate:1019216384 1 ssltcp 1019216385 192.168.0.100 49888 typ host generation 0", "1");
        m_observer.OnIceCandidate(&candidateSSLTcp);
    });

    LibWebRTCProvider::callOnWebRTCSignalingThread([this]() {
        MockLibWebRTCIceCandidate candidate("candidate:1677722111 1 tcp 1677722112 172.18.0.1 47989 typ srflx raddr 192.168.0.100 rport 47989 generation 0", "1");
        m_observer.OnIceCandidate(&candidate);
    });

    LibWebRTCProvider::callOnWebRTCSignalingThread([this]() {
        m_observer.OnIceGatheringChange(webrtc::PeerConnectionInterface::kIceGatheringComplete);
    });

    Release();
}

class MockLibWebRTCPeerConnectionForIceConnectionState : public MockLibWebRTCPeerConnection {
public:
    explicit MockLibWebRTCPeerConnectionForIceConnectionState(webrtc::PeerConnectionObserver& observer) : MockLibWebRTCPeerConnection(observer) { }
    virtual ~MockLibWebRTCPeerConnectionForIceConnectionState() = default;

private:
    void gotLocalDescription() final;
};

void MockLibWebRTCPeerConnectionForIceConnectionState::gotLocalDescription()
{
    m_observer.OnIceConnectionChange(kIceConnectionChecking);
    m_observer.OnIceConnectionChange(kIceConnectionConnected);
    m_observer.OnIceConnectionChange(kIceConnectionCompleted);
    m_observer.OnIceConnectionChange(kIceConnectionFailed);
    m_observer.OnIceConnectionChange(kIceConnectionDisconnected);
    m_observer.OnIceConnectionChange(kIceConnectionNew);
}

template<typename U> static inline void releaseInNetworkThread(MockLibWebRTCPeerConnection& mock, U& observer)
{
    mock.AddRef();
    observer.AddRef();
    callOnMainThread([&mock, &observer] {
        LibWebRTCProvider::callOnWebRTCNetworkThread([&mock, &observer]() {
            observer.Release();
            mock.Release();
        });
    });
}

class MockLibWebRTCPeerConnectionReleasedInNetworkThreadWhileCreatingOffer : public MockLibWebRTCPeerConnection {
public:
    explicit MockLibWebRTCPeerConnectionReleasedInNetworkThreadWhileCreatingOffer(webrtc::PeerConnectionObserver& observer) : MockLibWebRTCPeerConnection(observer) { }
    virtual ~MockLibWebRTCPeerConnectionReleasedInNetworkThreadWhileCreatingOffer() = default;

private:
    void CreateOffer(webrtc::CreateSessionDescriptionObserver* observer, const webrtc::PeerConnectionInterface::RTCOfferAnswerOptions&) final { releaseInNetworkThread(*this, *observer); }
};

class MockLibWebRTCPeerConnectionReleasedInNetworkThreadWhileGettingStats : public MockLibWebRTCPeerConnection {
public:
    explicit MockLibWebRTCPeerConnectionReleasedInNetworkThreadWhileGettingStats(webrtc::PeerConnectionObserver& observer) : MockLibWebRTCPeerConnection(observer) { }
    virtual ~MockLibWebRTCPeerConnectionReleasedInNetworkThreadWhileGettingStats() = default;

private:
    bool GetStats(webrtc::StatsObserver*, webrtc::MediaStreamTrackInterface*, StatsOutputLevel) final;
};

bool MockLibWebRTCPeerConnectionReleasedInNetworkThreadWhileGettingStats::GetStats(webrtc::StatsObserver* observer, webrtc::MediaStreamTrackInterface*, StatsOutputLevel)
{
    releaseInNetworkThread(*this, *observer);
    return true;
}

class MockLibWebRTCPeerConnectionReleasedInNetworkThreadWhileSettingDescription : public MockLibWebRTCPeerConnection {
public:
    explicit MockLibWebRTCPeerConnectionReleasedInNetworkThreadWhileSettingDescription(webrtc::PeerConnectionObserver& observer) : MockLibWebRTCPeerConnection(observer) { }
    virtual ~MockLibWebRTCPeerConnectionReleasedInNetworkThreadWhileSettingDescription() = default;

private:
    void SetLocalDescription(webrtc::SetSessionDescriptionObserver* observer, webrtc::SessionDescriptionInterface* sessionDescription) final
    {
        std::unique_ptr<webrtc::SessionDescriptionInterface> toBeFreed(sessionDescription);
        releaseInNetworkThread(*this, *observer);
    }
};

MockLibWebRTCPeerConnectionFactory::MockLibWebRTCPeerConnectionFactory(const String& testCase)
    : m_testCase(testCase.isolatedCopy())
{
}

static rtc::scoped_refptr<webrtc::PeerConnectionInterface> createConnection(const String& testCase, webrtc::PeerConnectionObserver& observer)
{
    if (testCase == "ICECandidates")
        return new rtc::RefCountedObject<MockLibWebRTCPeerConnectionForIceCandidates>(observer);

    if (testCase == "ICECandidatesWithDelay")
        return new rtc::RefCountedObject<MockLibWebRTCPeerConnectionForIceCandidates>(observer, 1000);

    if (testCase == "ICEConnectionState")
        return new rtc::RefCountedObject<MockLibWebRTCPeerConnectionForIceConnectionState>(observer);

    if (testCase == "LibWebRTCReleasingWhileCreatingOffer")
        return new rtc::RefCountedObject<MockLibWebRTCPeerConnectionReleasedInNetworkThreadWhileCreatingOffer>(observer);

    if (testCase == "LibWebRTCReleasingWhileGettingStats")
        return new rtc::RefCountedObject<MockLibWebRTCPeerConnectionReleasedInNetworkThreadWhileGettingStats>(observer);

    if (testCase == "LibWebRTCReleasingWhileSettingDescription")
        return new rtc::RefCountedObject<MockLibWebRTCPeerConnectionReleasedInNetworkThreadWhileSettingDescription>(observer);

    return new rtc::RefCountedObject<MockLibWebRTCPeerConnection>(observer);
}

webrtc::RTCErrorOr<rtc::scoped_refptr<webrtc::PeerConnectionInterface>> MockLibWebRTCPeerConnectionFactory::CreatePeerConnectionOrError(const webrtc::PeerConnectionInterface::RTCConfiguration&, webrtc::PeerConnectionDependencies dependencies)
{
    return createConnection(m_testCase, *dependencies.observer);
}

rtc::scoped_refptr<webrtc::VideoTrackInterface> MockLibWebRTCPeerConnectionFactory::CreateVideoTrack(const std::string& id, webrtc::VideoTrackSourceInterface* source)
{
    return new rtc::RefCountedObject<MockLibWebRTCVideoTrack>(id, source);
}

rtc::scoped_refptr<webrtc::AudioTrackInterface> MockLibWebRTCPeerConnectionFactory::CreateAudioTrack(const std::string& id, webrtc::AudioSourceInterface* source)
{
    return new rtc::RefCountedObject<MockLibWebRTCAudioTrack>(id, source);
}

rtc::scoped_refptr<webrtc::MediaStreamInterface> MockLibWebRTCPeerConnectionFactory::CreateLocalMediaStream(const std::string& label)
{
    return new rtc::RefCountedObject<webrtc::MediaStream>(label);
}

void MockLibWebRTCPeerConnection::SetLocalDescription(webrtc::SetSessionDescriptionObserver* observer, webrtc::SessionDescriptionInterface* sessionDescription)
{
    std::unique_ptr<webrtc::SessionDescriptionInterface> toBeFreed(sessionDescription);
    LibWebRTCProvider::callOnWebRTCSignalingThread([this, observer] {
        observer->OnSuccess();
        gotLocalDescription();
    });
}

void MockLibWebRTCPeerConnection::SetRemoteDescription(webrtc::SetSessionDescriptionObserver* observer, webrtc::SessionDescriptionInterface* sessionDescription)
{
    std::unique_ptr<webrtc::SessionDescriptionInterface> toBeFreed(sessionDescription);
    LibWebRTCProvider::callOnWebRTCSignalingThread([observer] {
        observer->OnSuccess();
    });
    ASSERT(sessionDescription);
    if (sessionDescription->type() == "offer") {
        std::string sdp;
        sessionDescription->ToString(&sdp);

        m_isInitiator = false;
        m_isReceivingAudio = sdp.find("m=audio") != std::string::npos;
        m_isReceivingVideo = sdp.find("m=video") != std::string::npos;
    }
}

rtc::scoped_refptr<webrtc::DataChannelInterface> MockLibWebRTCPeerConnection::CreateDataChannel(const std::string& label, const webrtc::DataChannelInit* init)
{
    webrtc::DataChannelInit parameters;
    if (init)
        parameters = *init;
    return new rtc::RefCountedObject<MockLibWebRTCDataChannel>(std::string(label), parameters.ordered, parameters.reliable, parameters.id);
}

webrtc::RTCErrorOr<rtc::scoped_refptr<webrtc::RtpSenderInterface>> MockLibWebRTCPeerConnection::AddTrack(rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> track, const std::vector<std::string>& streamIds)
{
    LibWebRTCProvider::callOnWebRTCSignalingThread([observer = &m_observer] {
        observer->OnRenegotiationNeeded();
    });

    if (!streamIds.empty())
        m_streamLabel = streamIds.front();

    rtc::scoped_refptr<webrtc::RtpSenderInterface> sender = new rtc::RefCountedObject<MockRtpSender>(WTFMove(track));
    rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver = new rtc::RefCountedObject<MockRtpReceiver>();
    rtc::scoped_refptr<MockRtpTransceiver> transceiver = new rtc::RefCountedObject<MockRtpTransceiver>(WTFMove(sender), WTFMove(receiver));

    m_transceivers.append(WTFMove(transceiver));
    return rtc::scoped_refptr<webrtc::RtpSenderInterface>(m_transceivers.last()->sender());
}

bool MockLibWebRTCPeerConnection::RemoveTrack(webrtc::RtpSenderInterface* sender)
{
    LibWebRTCProvider::callOnWebRTCSignalingThread([observer = &m_observer] {
        observer->OnRenegotiationNeeded();
    });
    bool isRemoved = false;
    return m_transceivers.removeFirstMatching([&](auto& transceiver) {
        if (transceiver->sender().get() != sender)
            return false;
        isRemoved = true;
        return true;
    });
}

void MockLibWebRTCPeerConnection::CreateOffer(webrtc::CreateSessionDescriptionObserver* observer, const webrtc::PeerConnectionInterface::RTCOfferAnswerOptions&)
{
    LibWebRTCProvider::callOnWebRTCSignalingThread([this, observer] {
        std::ostringstream sdp;
        sdp <<
            "v=0\r\n"
            "o=- 5667094644266930845 " << m_counter++ << " IN IP4 127.0.0.1\r\n"
            "s=-\r\n"
            "t=0 0\r\n";
        if (m_transceivers.size()) {
            unsigned partCounter = 1;
            sdp << "a=msid-semantic:WMS " << m_streamLabel << "\r\n";
            for (auto& transceiver : m_transceivers) {
                auto track = transceiver->sender()->track();
                if (track->kind() != "audio")
                    continue;
                sdp <<
                    "m=audio 9 UDP/TLS/RTP/SAVPF 111 8 0\r\n"
                    "c=IN IP4 0.0.0.0\r\n"
                    "a=rtcp-mux\r\n"
                    "a=sendrecv\r\n"
                    "a=mid:part" << partCounter++ << "\r\n"
                    "a=rtpmap:111 OPUS/48000/2\r\n"
                    "a=rtpmap:8 PCMA/8000\r\n"
                    "a=rtpmap:0 PCMU/8000\r\n"
                    "a=ssrc:3409173717 cname:/chKzCS9K6KOgL0n\r\n"
                    "a=msid:" << m_streamLabel << " " << track->id() << "\r\n"
                    "a=ice-ufrag:e/B1\r\n"
                    "a=ice-pwd:Yotk3Im3mnyi+1Q38p51MDub\r\n"
                    "a=fingerprint:sha-256 8B:87:09:8A:5D:C2:F3:33:EF:C5:B1:F6:84:3A:3D:D6:A3:E2:9C:17:4C:E7:46:3B:1B:CE:84:98:DD:8E:AF:7B\r\n"
                    "a=setup:actpass\r\n";
            }
            for (auto& transceiver : m_transceivers) {
                auto track = transceiver->sender()->track();
                if (track->kind() != "video")
                    continue;
                sdp <<
                    "m=video 9 UDP/TLS/RTP/SAVPF 103 100 120\r\n"
                    "c=IN IP4 0.0.0.0\r\n"
                    "a=rtcp-mux\r\n"
                    "a=sendrecv\r\n"
                    "a=mid:part" << partCounter++ << "\r\n"
                    "a=rtpmap:103 H264/90000\r\n"
                    "a=rtpmap:100 VP8/90000\r\n"
                    "a=rtpmap:120 RTX/90000\r\n"
                    "a=fmtp:103 packetization-mode=1\r\n"
                    "a=fmtp:120 apt=100;rtx-time=200\r\n"
                    "a=rtcp-fb:100 nack\r\n"
                    "a=rtcp-fb:103 nack pli\r\n"
                    "a=rtcp-fb:100 nack pli\r\n"
                    "a=rtcp-fb:103 ccm fir\r\n"
                    "a=rtcp-fb:100 ccm fir\r\n"
                    "a=ssrc:3409173718 cname:/chKzCS9K6KOgL0n\r\n"
                    "a=msid:" << m_streamLabel << " " << track->id() << "\r\n"
                    "a=ice-ufrag:e/B1\r\n"
                    "a=ice-pwd:Yotk3Im3mnyi+1Q38p51MDub\r\n"
                    "a=fingerprint:sha-256 8B:87:09:8A:5D:C2:F3:33:EF:C5:B1:F6:84:3A:3D:D6:A3:E2:9C:17:4C:E7:46:3B:1B:CE:84:98:DD:8E:AF:7B\r\n"
                    "a=setup:actpass\r\n";
            }
        }
        observer->OnSuccess(new MockLibWebRTCSessionDescription(sdp.str()));
    });
}

    void MockLibWebRTCPeerConnection::CreateAnswer(webrtc::CreateSessionDescriptionObserver* observer, const webrtc::PeerConnectionInterface::RTCOfferAnswerOptions&)
{
    LibWebRTCProvider::callOnWebRTCSignalingThread([this, observer] {
        std::ostringstream sdp;
        sdp <<
            "v=0\r\n"
            "o=- 5667094644266930846 " << m_counter++ << " IN IP4 127.0.0.1\r\n"
            "s=-\r\n"
            "t=0 0\r\n";
        if (m_transceivers.size()) {
            for (auto& transceiver : m_transceivers) {
                auto track = transceiver->sender()->track();
                if (track->kind() != "audio")
                    continue;
                sdp <<
                    "m=audio 9 UDP/TLS/RTP/SAVPF 111 8 0\r\n"
                    "c=IN IP4 0.0.0.0\r\n"
                    "a=rtcp-mux\r\n"
                    "a=recvonly\r\n"
                    "a=mid:part1\r\n"
                    "a=rtpmap:111 OPUS/48000/2\r\n"
                    "a=rtpmap:8 PCMA/8000\r\n"
                    "a=rtpmap:0 PCMU/8000\r\n"
                    "a=ssrc:3409173717 cname:/chKzCS9K6KOgL0m\r\n"
                    "a=ice-ufrag:e/B1\r\n"
                    "a=ice-pwd:Yotk3Im3mnyi+1Q38p51MDub\r\n"
                    "a=fingerprint:sha-256 8B:87:09:8A:5D:C2:F3:33:EF:C5:B1:F6:84:3A:3D:D6:A3:E2:9C:17:4C:E7:46:3B:1B:CE:84:98:DD:8E:AF:7B\r\n"
                    "a=setup:active\r\n";
            }
            for (auto& transceiver : m_transceivers) {
                auto track = transceiver->sender()->track();
                if (track->kind() != "video")
                    continue;
                sdp <<
                    "m=video 9 UDP/TLS/RTP/SAVPF 103 100 120\r\n"
                    "c=IN IP4 0.0.0.0\r\n"
                    "a=rtcp-mux\r\n"
                    "a=recvonly\r\n"
                    "a=mid:part2\r\n"
                    "a=rtpmap:103 H264/90000\r\n"
                    "a=rtpmap:100 VP8/90000\r\n"
                    "a=rtpmap:120 RTX/90000\r\n"
                    "a=fmtp:103 packetization-mode=1\r\n"
                    "a=fmtp:120 apt=100;rtx-time=200\r\n"
                    "a=rtcp-fb:100 nack\r\n"
                    "a=rtcp-fb:103 nack pli\r\n"
                    "a=rtcp-fb:100 nack pli\r\n"
                    "a=rtcp-fb:103 ccm fir\r\n"
                    "a=rtcp-fb:100 ccm fir\r\n"
                    "a=ssrc:3409173718 cname:/chKzCS9K6KOgL0n\r\n"
                    "a=ice-ufrag:e/B1\r\n"
                    "a=ice-pwd:Yotk3Im3mnyi+1Q38p51MDub\r\n"
                    "a=fingerprint:sha-256 8B:87:09:8A:5D:C2:F3:33:EF:C5:B1:F6:84:3A:3D:D6:A3:E2:9C:17:4C:E7:46:3B:1B:CE:84:98:DD:8E:AF:7B\r\n"
                    "a=setup:active\r\n";
            }
        } else if (!m_isInitiator) {
            if (m_isReceivingAudio) {
                sdp <<
                    "m=audio 9 UDP/TLS/RTP/SAVPF 111 8 0\r\n"
                    "c=IN IP4 0.0.0.0\r\n"
                    "a=rtcp-mux\r\n"
                    "a=recvonly\r\n"
                    "a=mid:part1\r\n"
                    "a=rtpmap:111 OPUS/48000/2\r\n"
                    "a=rtpmap:8 PCMA/8000\r\n"
                    "a=rtpmap:0 PCMU/8000\r\n"
                    "a=ssrc:3409173717 cname:/chKzCS9K6KOgL0m\r\n"
                    "a=ice-ufrag:e/B1\r\n"
                    "a=ice-pwd:Yotk3Im3mnyi+1Q38p51MDub\r\n"
                    "a=fingerprint:sha-256 8B:87:09:8A:5D:C2:F3:33:EF:C5:B1:F6:84:3A:3D:D6:A3:E2:9C:17:4C:E7:46:3B:1B:CE:84:98:DD:8E:AF:7B\r\n"
                    "a=setup:active\r\n";
            }
            if (m_isReceivingVideo) {
                sdp <<
                    "m=video 9 UDP/TLS/RTP/SAVPF 103 100 120\r\n"
                    "c=IN IP4 0.0.0.0\r\n"
                    "a=rtcp-mux\r\n"
                    "a=recvonly\r\n"
                    "a=mid:part2\r\n"
                    "a=rtpmap:103 H264/90000\r\n"
                    "a=rtpmap:100 VP8/90000\r\n"
                    "a=rtpmap:120 RTX/90000\r\n"
                    "a=fmtp:103 packetization-mode=1\r\n"
                    "a=fmtp:120 apt=100;rtx-time=200\r\n"
                    "a=rtcp-fb:100 nack\r\n"
                    "a=rtcp-fb:103 nack pli\r\n"
                    "a=rtcp-fb:100 nack pli\r\n"
                    "a=rtcp-fb:103 ccm fir\r\n"
                    "a=rtcp-fb:100 ccm fir\r\n"
                    "a=ssrc:3409173718 cname:/chKzCS9K6KOgL0n\r\n"
                    "a=ice-ufrag:e/B1\r\n"
                    "a=ice-pwd:Yotk3Im3mnyi+1Q38p51MDub\r\n"
                    "a=fingerprint:sha-256 8B:87:09:8A:5D:C2:F3:33:EF:C5:B1:F6:84:3A:3D:D6:A3:E2:9C:17:4C:E7:46:3B:1B:CE:84:98:DD:8E:AF:7B\r\n"
                    "a=setup:active\r\n";
            }
        }
        observer->OnSuccess(new MockLibWebRTCSessionDescription(sdp.str()));
    });
}

} // namespace WebCore

#endif // USE(LIBWEBRTC)
