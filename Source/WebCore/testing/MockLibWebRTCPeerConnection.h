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

#include "LibWebRTCMacros.h"

ALLOW_UNUSED_PARAMETERS_BEGIN

#include <webrtc/api/mediastreaminterface.h>
#include <webrtc/api/peerconnectioninterface.h>

ALLOW_UNUSED_PARAMETERS_END

#include <wtf/text/WTFString.h>

namespace WebCore {

class LibWebRTCProvider;
class MockRtpSender;

void useMockRTCPeerConnectionFactory(LibWebRTCProvider*, const String&);
void useRealRTCPeerConnectionFactory(LibWebRTCProvider&);

class MockLibWebRTCSessionDescription: public webrtc::SessionDescriptionInterface {
public:
    explicit MockLibWebRTCSessionDescription(std::string&& sdp) : m_sdp(WTFMove(sdp)) { }

private:
    bool ToString(std::string* out) const final { *out = m_sdp; return true; }

    cricket::SessionDescription* description() final { return nullptr; }
    const cricket::SessionDescription* description() const final { return nullptr; }
    std::string session_id() const final { return ""; }
    std::string session_version() const final { return ""; }
    std::string type() const final { return ""; }
    bool AddCandidate(const webrtc::IceCandidateInterface*) final { return true; }
    size_t number_of_mediasections() const final { return 0; }
    const webrtc::IceCandidateCollection* candidates(size_t) const final { return nullptr; }

    std::string m_sdp;
};

class MockLibWebRTCIceCandidate : public webrtc::IceCandidateInterface {
public:
    explicit MockLibWebRTCIceCandidate(const char* sdp, const char* sdpMid)
        : m_sdp(sdp)
        , m_sdpMid(sdpMid) { }

private:
    std::string sdp_mid() const final { return m_sdpMid; }
    int sdp_mline_index() const final { return 0; }
    const cricket::Candidate& candidate() const final { return m_candidate; }
    bool ToString(std::string* out) const final { *out = m_sdp; return true; }

protected:
    const char* m_sdp;
    const char* m_sdpMid;
    cricket::Candidate m_candidate;
};

class MockLibWebRTCAudioTrack : public webrtc::AudioTrackInterface {
public:
    explicit MockLibWebRTCAudioTrack(const std::string& id, webrtc::AudioSourceInterface* source)
        : m_id(id)
        , m_source(source) { }

private:
    webrtc::AudioSourceInterface* GetSource() const final { return m_source; }
    void AddSink(webrtc::AudioTrackSinkInterface* sink) final {
        if (m_source)
            m_source->AddSink(sink);
    }
    void RemoveSink(webrtc::AudioTrackSinkInterface* sink) final {
        if (m_source)
            m_source->RemoveSink(sink);
    }
    void RegisterObserver(webrtc::ObserverInterface*) final { }
    void UnregisterObserver(webrtc::ObserverInterface*) final { }

    std::string kind() const final { return "audio"; }
    std::string id() const final { return m_id; }
    bool enabled() const final { return m_enabled; }
    TrackState state() const final { return kLive; }
    bool set_enabled(bool enabled) final { m_enabled = enabled; return true; }

    bool m_enabled { true };
    std::string m_id;
    rtc::scoped_refptr<webrtc::AudioSourceInterface> m_source;
};

class MockLibWebRTCVideoTrack : public webrtc::VideoTrackInterface {
public:
    explicit MockLibWebRTCVideoTrack(const std::string& id, webrtc::VideoTrackSourceInterface* source)
        : m_id(id)
        , m_source(source) { }

private:
    webrtc::VideoTrackSourceInterface* GetSource() const final { return m_source; }
    void RegisterObserver(webrtc::ObserverInterface*) final { }
    void UnregisterObserver(webrtc::ObserverInterface*) final { }

    std::string kind() const final { return "video"; }
    std::string id() const final { return m_id; }
    bool enabled() const final { return m_enabled; }
    TrackState state() const final { return kLive; }
    bool set_enabled(bool enabled) final { m_enabled = enabled; return true; }

    bool m_enabled;
    std::string m_id;
    rtc::scoped_refptr<webrtc::VideoTrackSourceInterface> m_source;
};

class MockLibWebRTCDataChannel : public webrtc::DataChannelInterface {
public:
    MockLibWebRTCDataChannel(std::string&& label, bool ordered, bool reliable, int id)
        : m_label(WTFMove(label))
        , m_ordered(ordered)
        , m_reliable(reliable)
        , m_id(id) { }

private:
    void RegisterObserver(webrtc::DataChannelObserver*) final { }
    void UnregisterObserver() final { }
    std::string label() const final { return m_label; }
    bool reliable() const final { return m_reliable; }
    bool ordered() const final { return m_ordered; }

    int id() const final { return m_id; }
    DataState state() const final { return kConnecting; }
    uint64_t buffered_amount() const final { return 0; }
    void Close() final { }
    bool Send(const webrtc::DataBuffer&) final { return true; }
    uint32_t messages_sent() const final { return 0; }
    uint64_t bytes_sent() const final { return 0; }
    uint32_t messages_received() const final { return 0; }
    uint64_t bytes_received() const final { return 0; }

    std::string m_label;
    bool m_ordered { true };
    bool m_reliable { false };
    int m_id { -1 };
};

class MockRtpSender : public webrtc::RtpSenderInterface {
public:
    explicit MockRtpSender(rtc::scoped_refptr<webrtc::MediaStreamTrackInterface>&& track) : m_track(WTFMove(track)) { }

private:
    bool SetTrack(webrtc::MediaStreamTrackInterface* track) final
    {
        m_track = track;
        return true;
    }
    rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> track() const final { return m_track; }
    
    uint32_t ssrc() const { return 0; }
    cricket::MediaType media_type() const { return cricket::MEDIA_TYPE_VIDEO; }
    std::string id() const { return ""; }
    std::vector<std::string> stream_ids() const { return { }; }
    webrtc::RtpParameters GetParameters() final { return { }; }
    webrtc::RTCError SetParameters(const webrtc::RtpParameters&) final { return { }; }
    rtc::scoped_refptr<webrtc::DtmfSenderInterface> GetDtmfSender() const final { return nullptr; }

private:
    rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> m_track;
};

class MockRtpReceiver : public webrtc::RtpReceiverInterface {
private:
    cricket::MediaType media_type() const final { return cricket::MEDIA_TYPE_VIDEO; }
    std::string id() const { return { }; }
    webrtc::RtpParameters GetParameters() const { return { }; }
    bool SetParameters(const webrtc::RtpParameters&) { return true; }
    void SetObserver(webrtc::RtpReceiverObserverInterface*) { }
    rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> track() const final { return { }; }
};

class MockRtpTransceiver : public webrtc::RtpTransceiverInterface {
public:
    MockRtpTransceiver(rtc::scoped_refptr<webrtc::RtpSenderInterface>&& sender, rtc::scoped_refptr<webrtc::RtpReceiverInterface>&& receiver)
        : m_sender(WTFMove(sender))
        , m_receiver(WTFMove(receiver))
    {
    }

    rtc::scoped_refptr<webrtc::RtpSenderInterface> sender() const final { return m_sender; }

private:
    cricket::MediaType media_type() const final { return m_sender->media_type(); }
    absl::optional<std::string> mid() const final { return { }; }
    rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver() const final { return m_receiver; }
    bool stopped() const final { return false; }
    webrtc::RtpTransceiverDirection direction() const final { return webrtc::RtpTransceiverDirection::kSendRecv; }
    void SetDirection(webrtc::RtpTransceiverDirection) final { }
    absl::optional<webrtc::RtpTransceiverDirection> current_direction() const final { return { }; }
    void Stop() final { }

private:
    rtc::scoped_refptr<webrtc::RtpSenderInterface> m_sender;
    rtc::scoped_refptr<webrtc::RtpReceiverInterface> m_receiver;
};

class MockLibWebRTCPeerConnection : public webrtc::PeerConnectionInterface {
public:
    ~MockLibWebRTCPeerConnection();

protected:
    explicit MockLibWebRTCPeerConnection(webrtc::PeerConnectionObserver& observer) : m_observer(observer) { }

private:
    rtc::scoped_refptr<webrtc::StreamCollectionInterface> local_streams() override { return nullptr; }
    rtc::scoped_refptr<webrtc::StreamCollectionInterface> remote_streams() override { return nullptr; }
    const webrtc::SessionDescriptionInterface* local_description() const override { return nullptr; }
    const webrtc::SessionDescriptionInterface* remote_description() const override { return nullptr; }
    bool AddIceCandidate(const webrtc::IceCandidateInterface*) override { return true; }
    SignalingState signaling_state() override { return kStable; }
    IceConnectionState ice_connection_state() override { return kIceConnectionNew; }
    IceGatheringState ice_gathering_state() override { return kIceGatheringNew; }
    void StopRtcEventLog() override { }
    void Close() override { }

    bool AddStream(webrtc::MediaStreamInterface*) final { return false; }
    void RemoveStream(webrtc::MediaStreamInterface*) final { }

    std::vector<rtc::scoped_refptr<webrtc::RtpTransceiverInterface>> GetTransceivers() const final;

protected:
    void SetRemoteDescription(webrtc::SetSessionDescriptionObserver*, webrtc::SessionDescriptionInterface*) final;
    void CreateAnswer(webrtc::CreateSessionDescriptionObserver*, const webrtc::PeerConnectionInterface::RTCOfferAnswerOptions&) final;
    rtc::scoped_refptr<webrtc::DataChannelInterface> CreateDataChannel(const std::string&, const webrtc::DataChannelInit*) final;
    webrtc::RTCErrorOr<rtc::scoped_refptr<webrtc::RtpSenderInterface>> AddTrack(rtc::scoped_refptr<webrtc::MediaStreamTrackInterface>, const std::vector<std::string>& streams) final;
    bool RemoveTrack(webrtc::RtpSenderInterface*) final;
    webrtc::RTCError SetBitrate(const BitrateParameters&) final { return { }; }

    void SetLocalDescription(webrtc::SetSessionDescriptionObserver*, webrtc::SessionDescriptionInterface*) override;
    bool GetStats(webrtc::StatsObserver*, webrtc::MediaStreamTrackInterface*, StatsOutputLevel) override { return false; }
    void CreateOffer(webrtc::CreateSessionDescriptionObserver*, const webrtc::PeerConnectionInterface::RTCOfferAnswerOptions&) override;

    virtual void gotLocalDescription() { }

    webrtc::PeerConnectionObserver& m_observer;
    unsigned m_counter { 0 };
    Vector<rtc::scoped_refptr<MockRtpTransceiver>> m_transceivers;
    bool m_isInitiator { true };
    bool m_isReceivingAudio { false };
    bool m_isReceivingVideo { false };
    std::string m_streamLabel;
};

class MockLibWebRTCPeerConnectionFactory : public webrtc::PeerConnectionFactoryInterface {
public:
    static rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> create(const String& testCase) { return new rtc::RefCountedObject<MockLibWebRTCPeerConnectionFactory>(testCase); }

protected:
    explicit MockLibWebRTCPeerConnectionFactory(const String&);

private:
    rtc::scoped_refptr<webrtc::PeerConnectionInterface> CreatePeerConnection(const webrtc::PeerConnectionInterface::RTCConfiguration&, webrtc::PeerConnectionDependencies) final;

    rtc::scoped_refptr<webrtc::MediaStreamInterface> CreateLocalMediaStream(const std::string&) final;

    void SetOptions(const Options&) final { }
    rtc::scoped_refptr<webrtc::AudioSourceInterface> CreateAudioSource(const cricket::AudioOptions&) final { return nullptr; }

    rtc::scoped_refptr<webrtc::VideoTrackSourceInterface> CreateVideoSource(cricket::VideoCapturer*) final { return nullptr; }
    rtc::scoped_refptr<webrtc::VideoTrackSourceInterface> CreateVideoSource(std::unique_ptr<cricket::VideoCapturer>, const webrtc::MediaConstraintsInterface*) final { return nullptr; }

    rtc::scoped_refptr<webrtc::VideoTrackInterface> CreateVideoTrack(const std::string&, webrtc::VideoTrackSourceInterface*) final;
    rtc::scoped_refptr<webrtc::AudioTrackInterface> CreateAudioTrack(const std::string&, webrtc::AudioSourceInterface*) final;

    bool StartAecDump(rtc::PlatformFile, int64_t) final { return false; }
    void StopAecDump() final { }

private:
    String m_testCase;
};

} // namespace WebCore

#endif // USE(LIBWEBRTC)
