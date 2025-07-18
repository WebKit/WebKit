/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "RTCSignalingState.h"

WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN

#include <webrtc/api/media_stream_interface.h>
#include <webrtc/api/make_ref_counted.h>
// See Bug 274508: Disable thread-safety-reference-return warnings in libwebrtc
IGNORE_CLANG_WARNINGS_BEGIN("thread-safety-reference-return")
IGNORE_CLANG_WARNINGS_BEGIN("nullability-completeness")
#include <webrtc/api/peer_connection_interface.h>
IGNORE_CLANG_WARNINGS_END
IGNORE_CLANG_WARNINGS_END

WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END

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

    webrtc::SessionDescription* description() final { return nullptr; }
    const webrtc::SessionDescription* description() const final { return nullptr; }
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
    const webrtc::Candidate& candidate() const final { return m_candidate; }
    bool ToString(std::string* out) const final { *out = m_sdp; return true; }

protected:
    const char* m_sdp;
    const char* m_sdpMid;
    webrtc::Candidate m_candidate;
};

class MockLibWebRTCAudioTrack : public webrtc::AudioTrackInterface {
public:
    explicit MockLibWebRTCAudioTrack(const std::string& id, webrtc::AudioSourceInterface* source)
        : m_id(id)
        , m_source(source) { }

private:
    webrtc::AudioSourceInterface* GetSource() const final { return m_source.get(); }
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
    webrtc::scoped_refptr<webrtc::AudioSourceInterface> m_source;
};

class MockLibWebRTCVideoTrack : public webrtc::VideoTrackInterface {
public:
    explicit MockLibWebRTCVideoTrack(const std::string& id, webrtc::VideoTrackSourceInterface* source)
        : m_id(id)
        , m_source(source) { }

private:
    webrtc::VideoTrackSourceInterface* GetSource() const final { return m_source.get(); }
    void RegisterObserver(webrtc::ObserverInterface*) final { }
    void UnregisterObserver(webrtc::ObserverInterface*) final { }

    std::string kind() const final { return "video"; }
    std::string id() const final { return m_id; }
    bool enabled() const final { return m_enabled; }
    TrackState state() const final { return kLive; }
    bool set_enabled(bool enabled) final { m_enabled = enabled; return true; }

    bool m_enabled { true };
    std::string m_id;
    webrtc::scoped_refptr<webrtc::VideoTrackSourceInterface> m_source;
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

class MockDtmfSender : public webrtc::DtmfSenderInterface {
private:
    void RegisterObserver(webrtc::DtmfSenderObserverInterface*) final { }
    void UnregisterObserver() final { }

    bool CanInsertDtmf() final { return false; }

    std::string tones() const final { return ""; }
    int duration() const final { return 0; }
    int inter_tone_gap() const final { return 50; }
};

class MockRtpSender : public webrtc::RtpSenderInterface {
public:
    explicit MockRtpSender(webrtc::scoped_refptr<webrtc::MediaStreamTrackInterface>&& track)
        : m_track(WTFMove(track))
    {
    }

private:
    bool SetTrack(webrtc::MediaStreamTrackInterface* track) final
    {
        m_track = track;
        return true;
    }
    webrtc::scoped_refptr<webrtc::MediaStreamTrackInterface> track() const final { return m_track; }
    
    uint32_t ssrc() const { return 0; }
    webrtc::MediaType media_type() const { return webrtc::MediaType::VIDEO; }
    std::string id() const { return ""; }
    std::vector<std::string> stream_ids() const { return { }; }
    webrtc::RtpParameters GetParameters() const final { return { }; }
    webrtc::RTCError SetParameters(const webrtc::RtpParameters&) final { return { }; }
    webrtc::scoped_refptr<webrtc::DtmfSenderInterface> GetDtmfSender() const final
    {
        if (!m_dtmfSender)
            m_dtmfSender = webrtc::make_ref_counted<MockDtmfSender>();
        return m_dtmfSender;
    }

    webrtc::scoped_refptr<webrtc::DtlsTransportInterface> dtls_transport() const final { return { }; }
    void SetStreams(const std::vector<std::string>&) final { }
    std::vector<webrtc::RtpEncodingParameters> init_send_encodings() const final { return { }; }
    void SetFrameEncryptor(webrtc::scoped_refptr<webrtc::FrameEncryptorInterface>) final { }
    webrtc::scoped_refptr<webrtc::FrameEncryptorInterface> GetFrameEncryptor() const final { return { }; }

    void SetEncoderToPacketizerFrameTransformer(webrtc::scoped_refptr<webrtc::FrameTransformerInterface>) final { }
    void SetEncoderSelector(std::unique_ptr<webrtc::VideoEncoderFactory::EncoderSelectorInterface>) final { }
    webrtc::RTCError GenerateKeyFrame(const std::vector<std::string>&) final { return  { }; }

private:
    webrtc::scoped_refptr<webrtc::MediaStreamTrackInterface> m_track;
    mutable webrtc::scoped_refptr<webrtc::DtmfSenderInterface> m_dtmfSender;
};

class MockRtpReceiver : public webrtc::RtpReceiverInterface {
private:
    webrtc::MediaType media_type() const final { return webrtc::MediaType::VIDEO; }
    std::string id() const { return { }; }
    webrtc::RtpParameters GetParameters() const { return { }; }
    bool SetParameters(const webrtc::RtpParameters&) { return true; }
    void SetObserver(webrtc::RtpReceiverObserverInterface*) { }
    void SetJitterBufferMinimumDelay(std::optional<double>) final { }
    webrtc::scoped_refptr<webrtc::MediaStreamTrackInterface> track() const final
    {
        if (!m_track)
            const_cast<MockRtpReceiver*>(this)->m_track = webrtc::make_ref_counted<MockLibWebRTCVideoTrack>("", nullptr);
        return m_track;
    }

    webrtc::scoped_refptr<webrtc::MediaStreamTrackInterface> m_track;
};

class MockRtpTransceiver : public webrtc::RtpTransceiverInterface {
public:
    MockRtpTransceiver(webrtc::scoped_refptr<webrtc::RtpSenderInterface>&& sender, webrtc::scoped_refptr<webrtc::RtpReceiverInterface>&& receiver)
        : m_sender(WTFMove(sender))
        , m_receiver(WTFMove(receiver))
    {
    }

    webrtc::scoped_refptr<webrtc::RtpSenderInterface> sender() const final { return m_sender; }

private:
    webrtc::MediaType media_type() const final { return m_sender->media_type(); }
    std::optional<std::string> mid() const final { return { }; }
    webrtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver() const final { return m_receiver; }
    bool stopped() const final { return false; }
    webrtc::RtpTransceiverDirection direction() const final { return webrtc::RtpTransceiverDirection::kSendRecv; }
    void SetDirection(webrtc::RtpTransceiverDirection) final { }
    std::optional<webrtc::RtpTransceiverDirection> current_direction() const final { return { }; }
    void StopInternal() final { }
    webrtc::RTCError StopStandard() final { return { }; }
    bool stopping() const final { return true; }
    webrtc::RTCError SetCodecPreferences(webrtc::ArrayView<webrtc::RtpCodecCapability>) final { return { }; };
    std::vector<webrtc::RtpCodecCapability> codec_preferences() const final { return { }; }
    std::vector<webrtc::RtpHeaderExtensionCapability> GetHeaderExtensionsToNegotiate() const final { return { }; }
    std::vector<webrtc::RtpHeaderExtensionCapability> GetNegotiatedHeaderExtensions() const final { return { }; }
    webrtc::RTCError SetHeaderExtensionsToNegotiate(webrtc::ArrayView<const webrtc::RtpHeaderExtensionCapability> ) final { return { }; }

private:
    webrtc::scoped_refptr<webrtc::RtpSenderInterface> m_sender;
    webrtc::scoped_refptr<webrtc::RtpReceiverInterface> m_receiver;
};

class MockLibWebRTCPeerConnection : public webrtc::PeerConnectionInterface {
public:
    ~MockLibWebRTCPeerConnection();

protected:
    explicit MockLibWebRTCPeerConnection(webrtc::PeerConnectionObserver& observer) : m_observer(observer) { }

private:
    webrtc::RTCErrorOr<webrtc::scoped_refptr<webrtc::RtpTransceiverInterface>> AddTransceiver(webrtc::scoped_refptr<webrtc::MediaStreamTrackInterface>) override { return { }; }
    webrtc::RTCErrorOr<webrtc::scoped_refptr<webrtc::RtpTransceiverInterface>> AddTransceiver(webrtc::scoped_refptr<webrtc::MediaStreamTrackInterface>, const webrtc::RtpTransceiverInit&) override { return { }; }
    webrtc::RTCErrorOr<webrtc::scoped_refptr<webrtc::RtpTransceiverInterface>> AddTransceiver(webrtc::MediaType) override { return { }; }
    webrtc::RTCErrorOr<webrtc::scoped_refptr<webrtc::RtpTransceiverInterface>> AddTransceiver(webrtc::MediaType, const webrtc::RtpTransceiverInit&) override { return { }; }

    webrtc::scoped_refptr<webrtc::RtpSenderInterface> CreateSender(const std::string&, const std::string&) override  { return { }; }
    std::vector<webrtc::scoped_refptr<webrtc::RtpSenderInterface>> GetSenders() const override { return { }; }
    std::vector<webrtc::scoped_refptr<webrtc::RtpReceiverInterface>> GetReceivers() const override { return { }; }
    void GetStats(webrtc::RTCStatsCollectorCallback*) override { }
    void GetStats(webrtc::scoped_refptr<webrtc::RtpSenderInterface>, webrtc::scoped_refptr<webrtc::RTCStatsCollectorCallback>) override { }
    void GetStats(webrtc::scoped_refptr<webrtc::RtpReceiverInterface>, webrtc::scoped_refptr<webrtc::RTCStatsCollectorCallback>) override { }
    const webrtc::SessionDescriptionInterface* current_local_description() const override { return nullptr; }
    const webrtc::SessionDescriptionInterface* current_remote_description() const override { return nullptr; }
    const webrtc::SessionDescriptionInterface* pending_local_description() const override { return nullptr; }
    const webrtc::SessionDescriptionInterface* pending_remote_description() const override { return nullptr; }

    void RestartIce() override { }
    webrtc::PeerConnectionInterface::RTCConfiguration GetConfiguration() override { return { }; }
    IceConnectionState standardized_ice_connection_state() override { return kIceConnectionNew; }
    webrtc::scoped_refptr<webrtc::StreamCollectionInterface> local_streams() override { return nullptr; }
    webrtc::scoped_refptr<webrtc::StreamCollectionInterface> remote_streams() override { return nullptr; }
    const webrtc::SessionDescriptionInterface* local_description() const override { return nullptr; }
    const webrtc::SessionDescriptionInterface* remote_description() const override { return nullptr; }
    bool AddIceCandidate(const webrtc::IceCandidateInterface*) override { return true; }
    void AddIceCandidate(std::unique_ptr<webrtc::IceCandidateInterface>, std::function<void(webrtc::RTCError)> callback) override { callback({ }); }
    SignalingState signaling_state() override;
    IceConnectionState ice_connection_state() override { return kIceConnectionNew; }
    IceGatheringState ice_gathering_state() override { return kIceGatheringNew; }
    void StopRtcEventLog() override { }
    void Close() override { }

    bool AddStream(webrtc::MediaStreamInterface*) final { return false; }
    void RemoveStream(webrtc::MediaStreamInterface*) final { }

    std::vector<webrtc::scoped_refptr<webrtc::RtpTransceiverInterface>> GetTransceivers() const final;

    bool ShouldFireNegotiationNeededEvent(uint32_t) final { return false; }
    void ReconfigureBandwidthEstimation(const webrtc::BandwidthEstimationSettings&) final { }
    void SetAudioPlayout(bool) final { }
    void SetAudioRecording(bool) final { }
    std::optional<bool> can_trickle_ice_candidates() final { return { }; }
    void AddAdaptationResource(webrtc::scoped_refptr<webrtc::Resource>) final { }
    webrtc::Thread* signaling_thread() const final { return nullptr; }
    webrtc::NetworkControllerInterface* GetNetworkController() final { return nullptr; }

protected:
    void SetRemoteDescription(webrtc::SetSessionDescriptionObserver*, webrtc::SessionDescriptionInterface*) final { ASSERT_NOT_REACHED(); }
    void SetRemoteDescription(std::unique_ptr<webrtc::SessionDescriptionInterface>, webrtc::scoped_refptr<webrtc::SetRemoteDescriptionObserverInterface>) override;
    bool RemoveIceCandidates(const std::vector<webrtc::Candidate>&) override { return true; }
    webrtc::scoped_refptr<webrtc::DtlsTransportInterface> LookupDtlsTransportByMid(const std::string&) override { return { }; }
    webrtc::scoped_refptr<webrtc::SctpTransportInterface> GetSctpTransport() const override { return { }; }
    webrtc::PeerConnectionInterface::PeerConnectionState peer_connection_state() override { return PeerConnectionState::kNew; }
    bool StartRtcEventLog(std::unique_ptr<webrtc::RtcEventLogOutput>, int64_t) override { return true; }
    bool StartRtcEventLog(std::unique_ptr<webrtc::RtcEventLogOutput>) override { return true; }

    void CreateAnswer(webrtc::CreateSessionDescriptionObserver*, const webrtc::PeerConnectionInterface::RTCOfferAnswerOptions&) final;
    webrtc::RTCErrorOr<webrtc::scoped_refptr<webrtc::DataChannelInterface>> CreateDataChannelOrError(const std::string&, const webrtc::DataChannelInit*) final;
    webrtc::RTCErrorOr<webrtc::scoped_refptr<webrtc::RtpSenderInterface>> AddTrack(webrtc::scoped_refptr<webrtc::MediaStreamTrackInterface>, const std::vector<std::string>& streams) final;
    webrtc::RTCErrorOr<webrtc::scoped_refptr<webrtc::RtpSenderInterface>> AddTrack(webrtc::scoped_refptr<webrtc::MediaStreamTrackInterface> track, const std::vector<std::string>& streams, const std::vector<webrtc::RtpEncodingParameters>&) final { return AddTrack(track, streams); }
    void SetDataChannelEventObserver(std::unique_ptr<webrtc::DataChannelEventObserverInterface>) final { }

    webrtc::RTCError RemoveTrackOrError(webrtc::scoped_refptr<webrtc::RtpSenderInterface>) final;

    webrtc::RTCError SetBitrate(const webrtc::BitrateSettings&) final { return { }; }

    void SetLocalDescription(webrtc::SetSessionDescriptionObserver*, webrtc::SessionDescriptionInterface*) final { ASSERT_NOT_REACHED(); };
    void SetLocalDescription(std::unique_ptr<webrtc::SessionDescriptionInterface>, webrtc::scoped_refptr<webrtc::SetLocalDescriptionObserverInterface>) override;
    bool GetStats(webrtc::StatsObserver*, webrtc::MediaStreamTrackInterface*, StatsOutputLevel) override { return false; }
    void CreateOffer(webrtc::CreateSessionDescriptionObserver*, const webrtc::PeerConnectionInterface::RTCOfferAnswerOptions&) override;

    webrtc::RTCError SetConfiguration(const webrtc::PeerConnectionInterface::RTCConfiguration&) final { return { }; };

    virtual void gotLocalDescription() { }

    webrtc::PeerConnectionObserver& m_observer;
    unsigned m_counter { 0 };
    Vector<webrtc::scoped_refptr<MockRtpTransceiver>> m_transceivers;
    bool m_isInitiator { true };
    bool m_isReceivingAudio { false };
    bool m_isReceivingVideo { false };
    std::string m_streamLabel;
    RTCSignalingState m_signalingState { RTCSignalingState::Stable };
};

class MockLibWebRTCPeerConnectionFactory : public webrtc::PeerConnectionFactoryInterface {
public:
    static webrtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> create(const String& testCase) { return webrtc::make_ref_counted<MockLibWebRTCPeerConnectionFactory>(testCase); }

protected:
    explicit MockLibWebRTCPeerConnectionFactory(const String&);

private:
    webrtc::RTCErrorOr<webrtc::scoped_refptr<webrtc::PeerConnectionInterface>> CreatePeerConnectionOrError(const webrtc::PeerConnectionInterface::RTCConfiguration&, webrtc::PeerConnectionDependencies) final;

    webrtc::scoped_refptr<webrtc::MediaStreamInterface> CreateLocalMediaStream(const std::string&) final;

    void SetOptions(const Options&) final { }
    webrtc::scoped_refptr<webrtc::AudioSourceInterface> CreateAudioSource(const webrtc::AudioOptions&) final { return nullptr; }

    webrtc::scoped_refptr<webrtc::VideoTrackInterface> CreateVideoTrack(webrtc::scoped_refptr<webrtc::VideoTrackSourceInterface>, absl::string_view) final;
    webrtc::scoped_refptr<webrtc::AudioTrackInterface> CreateAudioTrack(const std::string&, webrtc::AudioSourceInterface*) final;
    webrtc::RtpCapabilities GetRtpReceiverCapabilities(webrtc::MediaType) const final { return { }; }
    webrtc::RtpCapabilities GetRtpSenderCapabilities(webrtc::MediaType) const final { return { }; }

    void StopAecDump() final { }

private:
    String m_testCase;
};

} // namespace WebCore

#endif // USE(LIBWEBRTC)
