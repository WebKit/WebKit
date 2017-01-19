/*
 *  Copyright (c) 2008 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>

#include "webrtc/pc/channel.h"
#include "webrtc/base/arraysize.h"
#include "webrtc/base/byteorder.h"
#include "webrtc/base/gunit.h"
#include "webrtc/call.h"
#include "webrtc/p2p/base/faketransportcontroller.h"
#include "webrtc/test/field_trial.h"
#include "webrtc/logging/rtc_event_log/rtc_event_log.h"
#include "webrtc/media/base/fakemediaengine.h"
#include "webrtc/media/base/fakenetworkinterface.h"
#include "webrtc/media/base/fakertp.h"
#include "webrtc/media/base/mediaconstants.h"
#include "webrtc/media/engine/fakewebrtccall.h"
#include "webrtc/media/engine/fakewebrtcvoiceengine.h"
#include "webrtc/media/engine/webrtcvoiceengine.h"
#include "webrtc/modules/audio_coding/codecs/builtin_audio_decoder_factory.h"
#include "webrtc/modules/audio_coding/codecs/mock/mock_audio_decoder_factory.h"
#include "webrtc/modules/audio_device/include/mock_audio_device.h"
#include "webrtc/modules/audio_processing/include/mock_audio_processing.h"

using testing::Return;
using testing::StrictMock;

namespace {

const cricket::AudioCodec kPcmuCodec(0, "PCMU", 8000, 64000, 1);
const cricket::AudioCodec kIsacCodec(103, "ISAC", 16000, 32000, 1);
const cricket::AudioCodec kOpusCodec(111, "opus", 48000, 64000, 2);
const cricket::AudioCodec kG722CodecVoE(9, "G722", 16000, 64000, 1);
const cricket::AudioCodec kG722CodecSdp(9, "G722", 8000, 64000, 1);
const cricket::AudioCodec kCn8000Codec(13, "CN", 8000, 0, 1);
const cricket::AudioCodec kCn16000Codec(105, "CN", 16000, 0, 1);
const cricket::AudioCodec kTelephoneEventCodec(106,
                                               "telephone-event",
                                               8000,
                                               0,
                                               1);
const uint32_t kSsrc1 = 0x99;
const uint32_t kSsrc2 = 2;
const uint32_t kSsrc3 = 3;
const uint32_t kSsrc4 = 0x42;
const uint32_t kSsrcs4[] = { 1, 2, 3, 4 };

constexpr int kRtpHistoryMs = 5000;

class FakeVoEWrapper : public cricket::VoEWrapper {
 public:
  explicit FakeVoEWrapper(cricket::FakeWebRtcVoiceEngine* engine)
      : cricket::VoEWrapper(engine,  // processing
                            engine,  // base
                            engine,  // codec
                            engine,  // hw
                            engine) {  // volume
  }
};

}  // namespace

// Tests that our stub library "works".
TEST(WebRtcVoiceEngineTestStubLibrary, StartupShutdown) {
  StrictMock<webrtc::test::MockAudioDeviceModule> adm;
  EXPECT_CALL(adm, AddRef()).WillOnce(Return(0));
  EXPECT_CALL(adm, Release()).WillOnce(Return(0));
  EXPECT_CALL(adm, BuiltInAECIsAvailable()).WillOnce(Return(false));
  EXPECT_CALL(adm, BuiltInAGCIsAvailable()).WillOnce(Return(false));
  EXPECT_CALL(adm, BuiltInNSIsAvailable()).WillOnce(Return(false));
  StrictMock<webrtc::test::MockAudioProcessing> apm;
  EXPECT_CALL(apm, ApplyConfig(testing::_));
  EXPECT_CALL(apm, SetExtraOptions(testing::_));
  EXPECT_CALL(apm, Initialize()).WillOnce(Return(0));
  cricket::FakeWebRtcVoiceEngine voe(&apm);
  EXPECT_FALSE(voe.IsInited());
  {
    cricket::WebRtcVoiceEngine engine(
        &adm, webrtc::MockAudioDecoderFactory::CreateUnusedFactory(),
        new FakeVoEWrapper(&voe));
    EXPECT_TRUE(voe.IsInited());
  }
  EXPECT_FALSE(voe.IsInited());
}

class FakeAudioSink : public webrtc::AudioSinkInterface {
 public:
  void OnData(const Data& audio) override {}
};

class FakeAudioSource : public cricket::AudioSource {
  void SetSink(Sink* sink) override {}
};

class WebRtcVoiceEngineTestFake : public testing::Test {
 public:
  WebRtcVoiceEngineTestFake() : WebRtcVoiceEngineTestFake("") {}

  explicit WebRtcVoiceEngineTestFake(const char* field_trials)
      : call_(webrtc::Call::Config(&event_log_)), voe_(&apm_),
        override_field_trials_(field_trials) {
    auto factory = webrtc::MockAudioDecoderFactory::CreateUnusedFactory();
    EXPECT_CALL(adm_, AddRef()).WillOnce(Return(0));
    EXPECT_CALL(adm_, Release()).WillOnce(Return(0));
    EXPECT_CALL(adm_, BuiltInAECIsAvailable()).WillOnce(Return(false));
    EXPECT_CALL(adm_, BuiltInAGCIsAvailable()).WillOnce(Return(false));
    EXPECT_CALL(adm_, BuiltInNSIsAvailable()).WillOnce(Return(false));
    EXPECT_CALL(apm_, ApplyConfig(testing::_));
    EXPECT_CALL(apm_, SetExtraOptions(testing::_));
    EXPECT_CALL(apm_, Initialize()).WillOnce(Return(0));
    engine_.reset(new cricket::WebRtcVoiceEngine(&adm_, factory,
                                                 new FakeVoEWrapper(&voe_)));
    send_parameters_.codecs.push_back(kPcmuCodec);
    recv_parameters_.codecs.push_back(kPcmuCodec);
  }

  bool SetupChannel() {
    EXPECT_CALL(apm_, ApplyConfig(testing::_));
    EXPECT_CALL(apm_, SetExtraOptions(testing::_));
    channel_ = engine_->CreateChannel(&call_, cricket::MediaConfig(),
                                      cricket::AudioOptions());
    return (channel_ != nullptr);
  }

  bool SetupRecvStream() {
    if (!SetupChannel()) {
      return false;
    }
    return AddRecvStream(kSsrc1);
  }

  bool SetupSendStream() {
    if (!SetupChannel()) {
      return false;
    }
    if (!channel_->AddSendStream(cricket::StreamParams::CreateLegacy(kSsrc1))) {
      return false;
    }
    EXPECT_CALL(apm_, set_output_will_be_muted(false));
    return channel_->SetAudioSend(kSsrc1, true, nullptr, &fake_source_);
  }

  bool AddRecvStream(uint32_t ssrc) {
    EXPECT_TRUE(channel_);
    return channel_->AddRecvStream(cricket::StreamParams::CreateLegacy(ssrc));
  }

  void SetupForMultiSendStream() {
    EXPECT_TRUE(SetupSendStream());
    // Remove stream added in Setup.
    EXPECT_TRUE(call_.GetAudioSendStream(kSsrc1));
    EXPECT_TRUE(channel_->RemoveSendStream(kSsrc1));
    // Verify the channel does not exist.
    EXPECT_FALSE(call_.GetAudioSendStream(kSsrc1));
  }

  void DeliverPacket(const void* data, int len) {
    rtc::CopyOnWriteBuffer packet(reinterpret_cast<const uint8_t*>(data), len);
    channel_->OnPacketReceived(&packet, rtc::PacketTime());
  }

  void TearDown() override {
    delete channel_;
  }

  const cricket::FakeAudioSendStream& GetSendStream(uint32_t ssrc) {
    const auto* send_stream = call_.GetAudioSendStream(ssrc);
    EXPECT_TRUE(send_stream);
    return *send_stream;
  }

  const cricket::FakeAudioReceiveStream& GetRecvStream(uint32_t ssrc) {
    const auto* recv_stream = call_.GetAudioReceiveStream(ssrc);
    EXPECT_TRUE(recv_stream);
    return *recv_stream;
  }

  const webrtc::AudioSendStream::Config& GetSendStreamConfig(uint32_t ssrc) {
    return GetSendStream(ssrc).GetConfig();
  }

  const webrtc::AudioReceiveStream::Config& GetRecvStreamConfig(uint32_t ssrc) {
    return GetRecvStream(ssrc).GetConfig();
  }

  void SetSend(bool enable) {
    ASSERT_TRUE(channel_);
    if (enable) {
      EXPECT_CALL(adm_, RecordingIsInitialized()).WillOnce(Return(false));
      EXPECT_CALL(adm_, Recording()).WillOnce(Return(false));
      EXPECT_CALL(adm_, InitRecording()).WillOnce(Return(0));
      EXPECT_CALL(apm_, ApplyConfig(testing::_));
      EXPECT_CALL(apm_, SetExtraOptions(testing::_));
    }
    channel_->SetSend(enable);
  }

  void SetSendParameters(const cricket::AudioSendParameters& params) {
    EXPECT_CALL(apm_, ApplyConfig(testing::_));
    EXPECT_CALL(apm_, SetExtraOptions(testing::_));
    ASSERT_TRUE(channel_);
    EXPECT_TRUE(channel_->SetSendParameters(params));
  }

  void SetAudioSend(uint32_t ssrc, bool enable, cricket::AudioSource* source,
                    const cricket::AudioOptions* options = nullptr) {
    EXPECT_CALL(apm_, set_output_will_be_muted(!enable));
    ASSERT_TRUE(channel_);
    if (enable && options) {
      EXPECT_CALL(apm_, ApplyConfig(testing::_));
      EXPECT_CALL(apm_, SetExtraOptions(testing::_));
    }
    EXPECT_TRUE(channel_->SetAudioSend(ssrc, enable, options, source));
  }

  void TestInsertDtmf(uint32_t ssrc, bool caller) {
    EXPECT_TRUE(SetupChannel());
    if (caller) {
      // If this is a caller, local description will be applied and add the
      // send stream.
      EXPECT_TRUE(channel_->AddSendStream(
          cricket::StreamParams::CreateLegacy(kSsrc1)));
    }

    // Test we can only InsertDtmf when the other side supports telephone-event.
    SetSendParameters(send_parameters_);
    SetSend(true);
    EXPECT_FALSE(channel_->CanInsertDtmf());
    EXPECT_FALSE(channel_->InsertDtmf(ssrc, 1, 111));
    send_parameters_.codecs.push_back(kTelephoneEventCodec);
    SetSendParameters(send_parameters_);
    EXPECT_TRUE(channel_->CanInsertDtmf());

    if (!caller) {
      // If this is callee, there's no active send channel yet.
      EXPECT_FALSE(channel_->InsertDtmf(ssrc, 2, 123));
      EXPECT_TRUE(channel_->AddSendStream(
          cricket::StreamParams::CreateLegacy(kSsrc1)));
    }

    // Check we fail if the ssrc is invalid.
    EXPECT_FALSE(channel_->InsertDtmf(-1, 1, 111));

    // Test send.
    cricket::FakeAudioSendStream::TelephoneEvent telephone_event =
        GetSendStream(kSsrc1).GetLatestTelephoneEvent();
    EXPECT_EQ(-1, telephone_event.payload_type);
    EXPECT_TRUE(channel_->InsertDtmf(ssrc, 2, 123));
    telephone_event = GetSendStream(kSsrc1).GetLatestTelephoneEvent();
    EXPECT_EQ(kTelephoneEventCodec.id, telephone_event.payload_type);
    EXPECT_EQ(2, telephone_event.event_code);
    EXPECT_EQ(123, telephone_event.duration_ms);
  }

  // Test that send bandwidth is set correctly.
  // |codec| is the codec under test.
  // |max_bitrate| is a parameter to set to SetMaxSendBandwidth().
  // |expected_result| is the expected result from SetMaxSendBandwidth().
  // |expected_bitrate| is the expected audio bitrate afterward.
  void TestMaxSendBandwidth(const cricket::AudioCodec& codec,
                            int max_bitrate,
                            bool expected_result,
                            int expected_bitrate) {
    cricket::AudioSendParameters parameters;
    parameters.codecs.push_back(codec);
    parameters.max_bandwidth_bps = max_bitrate;
    if (expected_result) {
      SetSendParameters(parameters);
    } else {
      EXPECT_FALSE(channel_->SetSendParameters(parameters));
    }
    EXPECT_EQ(expected_bitrate, GetCodecBitrate(kSsrc1));
  }

  // Sets the per-stream maximum bitrate limit for the specified SSRC.
  bool SetMaxBitrateForStream(int32_t ssrc, int bitrate) {
    webrtc::RtpParameters parameters = channel_->GetRtpSendParameters(ssrc);
    EXPECT_EQ(1UL, parameters.encodings.size());

    parameters.encodings[0].max_bitrate_bps = bitrate;
    return channel_->SetRtpSendParameters(ssrc, parameters);
  }

  void SetGlobalMaxBitrate(const cricket::AudioCodec& codec, int bitrate) {
    cricket::AudioSendParameters send_parameters;
    send_parameters.codecs.push_back(codec);
    send_parameters.max_bandwidth_bps = bitrate;
    SetSendParameters(send_parameters);
  }

  void CheckSendCodec(int32_t ssrc,
                      const char expected_name[],
                      int expected_channels,
                      int expected_bitrate) {
    const auto& codec = GetSendStreamConfig(ssrc).send_codec_spec.codec_inst;
    EXPECT_STREQ(expected_name, codec.plname);
    EXPECT_EQ(expected_channels, codec.channels);
    EXPECT_EQ(expected_bitrate, codec.rate);
  }

  int GetOpusMaxPlaybackRate(int32_t ssrc) {
    return GetSendStreamConfig(ssrc).send_codec_spec.opus_max_playback_rate;
  }

  bool GetOpusDtx(int32_t ssrc) {
    return GetSendStreamConfig(ssrc).send_codec_spec.enable_opus_dtx;
  }

  bool GetCodecFec(int32_t ssrc) {
    return GetSendStreamConfig(ssrc).send_codec_spec.enable_codec_fec;
  }

  int GetCodecBitrate(int32_t ssrc) {
    return GetSendStreamConfig(ssrc).send_codec_spec.codec_inst.rate;
  }

  int GetCodecPacSize(int32_t ssrc) {
    return GetSendStreamConfig(ssrc).send_codec_spec.codec_inst.pacsize;
  }

  const rtc::Optional<std::string>& GetAudioNetworkAdaptorConfig(int32_t ssrc) {
    return GetSendStreamConfig(ssrc).audio_network_adaptor_config;
  }

  void SetAndExpectMaxBitrate(const cricket::AudioCodec& codec,
                              int global_max,
                              int stream_max,
                              bool expected_result,
                              int expected_codec_bitrate) {
    // Clear the bitrate limit from the previous test case.
    EXPECT_TRUE(SetMaxBitrateForStream(kSsrc1, -1));

    // Attempt to set the requested bitrate limits.
    SetGlobalMaxBitrate(codec, global_max);
    EXPECT_EQ(expected_result, SetMaxBitrateForStream(kSsrc1, stream_max));

    // Verify that reading back the parameters gives results
    // consistent with the Set() result.
    webrtc::RtpParameters resulting_parameters =
        channel_->GetRtpSendParameters(kSsrc1);
    EXPECT_EQ(1UL, resulting_parameters.encodings.size());
    EXPECT_EQ(expected_result ? stream_max : -1,
              resulting_parameters.encodings[0].max_bitrate_bps);

    // Verify that the codec settings have the expected bitrate.
    EXPECT_EQ(expected_codec_bitrate, GetCodecBitrate(kSsrc1));
  }

  void TestSetSendRtpHeaderExtensions(const std::string& ext) {
    EXPECT_TRUE(SetupSendStream());

    // Ensure extensions are off by default.
    EXPECT_EQ(0u, GetSendStreamConfig(kSsrc1).rtp.extensions.size());

    // Ensure unknown extensions won't cause an error.
    send_parameters_.extensions.push_back(
        webrtc::RtpExtension("urn:ietf:params:unknownextention", 1));
    SetSendParameters(send_parameters_);
    EXPECT_EQ(0u, GetSendStreamConfig(kSsrc1).rtp.extensions.size());

    // Ensure extensions stay off with an empty list of headers.
    send_parameters_.extensions.clear();
    SetSendParameters(send_parameters_);
    EXPECT_EQ(0u, GetSendStreamConfig(kSsrc1).rtp.extensions.size());

    // Ensure extension is set properly.
    const int id = 1;
    send_parameters_.extensions.push_back(webrtc::RtpExtension(ext, id));
    SetSendParameters(send_parameters_);
    EXPECT_EQ(1u, GetSendStreamConfig(kSsrc1).rtp.extensions.size());
    EXPECT_EQ(ext, GetSendStreamConfig(kSsrc1).rtp.extensions[0].uri);
    EXPECT_EQ(id, GetSendStreamConfig(kSsrc1).rtp.extensions[0].id);

    // Ensure extension is set properly on new stream.
    EXPECT_TRUE(channel_->AddSendStream(
        cricket::StreamParams::CreateLegacy(kSsrc2)));
    EXPECT_NE(call_.GetAudioSendStream(kSsrc1),
              call_.GetAudioSendStream(kSsrc2));
    EXPECT_EQ(1u, GetSendStreamConfig(kSsrc2).rtp.extensions.size());
    EXPECT_EQ(ext, GetSendStreamConfig(kSsrc2).rtp.extensions[0].uri);
    EXPECT_EQ(id, GetSendStreamConfig(kSsrc2).rtp.extensions[0].id);

    // Ensure all extensions go back off with an empty list.
    send_parameters_.codecs.push_back(kPcmuCodec);
    send_parameters_.extensions.clear();
    SetSendParameters(send_parameters_);
    EXPECT_EQ(0u, GetSendStreamConfig(kSsrc1).rtp.extensions.size());
    EXPECT_EQ(0u, GetSendStreamConfig(kSsrc2).rtp.extensions.size());
  }

  void TestSetRecvRtpHeaderExtensions(const std::string& ext) {
    EXPECT_TRUE(SetupRecvStream());

    // Ensure extensions are off by default.
    EXPECT_EQ(0u, GetRecvStreamConfig(kSsrc1).rtp.extensions.size());

    // Ensure unknown extensions won't cause an error.
    recv_parameters_.extensions.push_back(
        webrtc::RtpExtension("urn:ietf:params:unknownextention", 1));
    EXPECT_TRUE(channel_->SetRecvParameters(recv_parameters_));
    EXPECT_EQ(0u, GetRecvStreamConfig(kSsrc1).rtp.extensions.size());

    // Ensure extensions stay off with an empty list of headers.
    recv_parameters_.extensions.clear();
    EXPECT_TRUE(channel_->SetRecvParameters(recv_parameters_));
    EXPECT_EQ(0u, GetRecvStreamConfig(kSsrc1).rtp.extensions.size());

    // Ensure extension is set properly.
    const int id = 2;
    recv_parameters_.extensions.push_back(webrtc::RtpExtension(ext, id));
    EXPECT_TRUE(channel_->SetRecvParameters(recv_parameters_));
    EXPECT_EQ(1u, GetRecvStreamConfig(kSsrc1).rtp.extensions.size());
    EXPECT_EQ(ext, GetRecvStreamConfig(kSsrc1).rtp.extensions[0].uri);
    EXPECT_EQ(id, GetRecvStreamConfig(kSsrc1).rtp.extensions[0].id);

    // Ensure extension is set properly on new stream.
    EXPECT_TRUE(AddRecvStream(kSsrc2));
    EXPECT_NE(call_.GetAudioReceiveStream(kSsrc1),
              call_.GetAudioReceiveStream(kSsrc2));
    EXPECT_EQ(1u, GetRecvStreamConfig(kSsrc2).rtp.extensions.size());
    EXPECT_EQ(ext, GetRecvStreamConfig(kSsrc2).rtp.extensions[0].uri);
    EXPECT_EQ(id, GetRecvStreamConfig(kSsrc2).rtp.extensions[0].id);

    // Ensure all extensions go back off with an empty list.
    recv_parameters_.extensions.clear();
    EXPECT_TRUE(channel_->SetRecvParameters(recv_parameters_));
    EXPECT_EQ(0u, GetRecvStreamConfig(kSsrc1).rtp.extensions.size());
    EXPECT_EQ(0u, GetRecvStreamConfig(kSsrc2).rtp.extensions.size());
  }

  webrtc::AudioSendStream::Stats GetAudioSendStreamStats() const {
    webrtc::AudioSendStream::Stats stats;
    stats.local_ssrc = 12;
    stats.bytes_sent = 345;
    stats.packets_sent = 678;
    stats.packets_lost = 9012;
    stats.fraction_lost = 34.56f;
    stats.codec_name = "codec_name_send";
    stats.ext_seqnum = 789;
    stats.jitter_ms = 12;
    stats.rtt_ms = 345;
    stats.audio_level = 678;
    stats.aec_quality_min = 9.01f;
    stats.echo_delay_median_ms = 234;
    stats.echo_delay_std_ms = 567;
    stats.echo_return_loss = 890;
    stats.echo_return_loss_enhancement = 1234;
    stats.residual_echo_likelihood = 0.432f;
    stats.typing_noise_detected = true;
    return stats;
  }
  void SetAudioSendStreamStats() {
    for (auto* s : call_.GetAudioSendStreams()) {
      s->SetStats(GetAudioSendStreamStats());
    }
  }
  void VerifyVoiceSenderInfo(const cricket::VoiceSenderInfo& info,
                             bool is_sending) {
    const auto stats = GetAudioSendStreamStats();
    EXPECT_EQ(info.ssrc(), stats.local_ssrc);
    EXPECT_EQ(info.bytes_sent, stats.bytes_sent);
    EXPECT_EQ(info.packets_sent, stats.packets_sent);
    EXPECT_EQ(info.packets_lost, stats.packets_lost);
    EXPECT_EQ(info.fraction_lost, stats.fraction_lost);
    EXPECT_EQ(info.codec_name, stats.codec_name);
    EXPECT_EQ(info.ext_seqnum, stats.ext_seqnum);
    EXPECT_EQ(info.jitter_ms, stats.jitter_ms);
    EXPECT_EQ(info.rtt_ms, stats.rtt_ms);
    EXPECT_EQ(info.audio_level, stats.audio_level);
    EXPECT_EQ(info.aec_quality_min, stats.aec_quality_min);
    EXPECT_EQ(info.echo_delay_median_ms, stats.echo_delay_median_ms);
    EXPECT_EQ(info.echo_delay_std_ms, stats.echo_delay_std_ms);
    EXPECT_EQ(info.echo_return_loss, stats.echo_return_loss);
    EXPECT_EQ(info.echo_return_loss_enhancement,
              stats.echo_return_loss_enhancement);
    EXPECT_EQ(info.residual_echo_likelihood, stats.residual_echo_likelihood);
    EXPECT_EQ(info.typing_noise_detected,
              stats.typing_noise_detected && is_sending);
  }

  webrtc::AudioReceiveStream::Stats GetAudioReceiveStreamStats() const {
    webrtc::AudioReceiveStream::Stats stats;
    stats.remote_ssrc = 123;
    stats.bytes_rcvd = 456;
    stats.packets_rcvd = 768;
    stats.packets_lost = 101;
    stats.fraction_lost = 23.45f;
    stats.codec_name = "codec_name_recv";
    stats.ext_seqnum = 678;
    stats.jitter_ms = 901;
    stats.jitter_buffer_ms = 234;
    stats.jitter_buffer_preferred_ms = 567;
    stats.delay_estimate_ms = 890;
    stats.audio_level = 1234;
    stats.expand_rate = 5.67f;
    stats.speech_expand_rate = 8.90f;
    stats.secondary_decoded_rate = 1.23f;
    stats.accelerate_rate = 4.56f;
    stats.preemptive_expand_rate = 7.89f;
    stats.decoding_calls_to_silence_generator = 12;
    stats.decoding_calls_to_neteq = 345;
    stats.decoding_normal = 67890;
    stats.decoding_plc = 1234;
    stats.decoding_cng = 5678;
    stats.decoding_plc_cng = 9012;
    stats.decoding_muted_output = 3456;
    stats.capture_start_ntp_time_ms = 7890;
    return stats;
  }
  void SetAudioReceiveStreamStats() {
    for (auto* s : call_.GetAudioReceiveStreams()) {
      s->SetStats(GetAudioReceiveStreamStats());
    }
  }
  void VerifyVoiceReceiverInfo(const cricket::VoiceReceiverInfo& info) {
    const auto stats = GetAudioReceiveStreamStats();
    EXPECT_EQ(info.ssrc(), stats.remote_ssrc);
    EXPECT_EQ(info.bytes_rcvd, stats.bytes_rcvd);
    EXPECT_EQ(info.packets_rcvd, stats.packets_rcvd);
    EXPECT_EQ(info.packets_lost, stats.packets_lost);
    EXPECT_EQ(info.fraction_lost, stats.fraction_lost);
    EXPECT_EQ(info.codec_name, stats.codec_name);
    EXPECT_EQ(info.ext_seqnum, stats.ext_seqnum);
    EXPECT_EQ(info.jitter_ms, stats.jitter_ms);
    EXPECT_EQ(info.jitter_buffer_ms, stats.jitter_buffer_ms);
    EXPECT_EQ(info.jitter_buffer_preferred_ms,
              stats.jitter_buffer_preferred_ms);
    EXPECT_EQ(info.delay_estimate_ms, stats.delay_estimate_ms);
    EXPECT_EQ(info.audio_level, stats.audio_level);
    EXPECT_EQ(info.expand_rate, stats.expand_rate);
    EXPECT_EQ(info.speech_expand_rate, stats.speech_expand_rate);
    EXPECT_EQ(info.secondary_decoded_rate, stats.secondary_decoded_rate);
    EXPECT_EQ(info.accelerate_rate, stats.accelerate_rate);
    EXPECT_EQ(info.preemptive_expand_rate, stats.preemptive_expand_rate);
    EXPECT_EQ(info.decoding_calls_to_silence_generator,
              stats.decoding_calls_to_silence_generator);
    EXPECT_EQ(info.decoding_calls_to_neteq, stats.decoding_calls_to_neteq);
    EXPECT_EQ(info.decoding_normal, stats.decoding_normal);
    EXPECT_EQ(info.decoding_plc, stats.decoding_plc);
    EXPECT_EQ(info.decoding_cng, stats.decoding_cng);
    EXPECT_EQ(info.decoding_plc_cng, stats.decoding_plc_cng);
    EXPECT_EQ(info.decoding_muted_output, stats.decoding_muted_output);
    EXPECT_EQ(info.capture_start_ntp_time_ms, stats.capture_start_ntp_time_ms);
  }

 protected:
  StrictMock<webrtc::test::MockAudioDeviceModule> adm_;
  StrictMock<webrtc::test::MockAudioProcessing> apm_;
  webrtc::RtcEventLogNullImpl event_log_;
  cricket::FakeCall call_;
  cricket::FakeWebRtcVoiceEngine voe_;
  std::unique_ptr<cricket::WebRtcVoiceEngine> engine_;
  cricket::VoiceMediaChannel* channel_ = nullptr;
  cricket::AudioSendParameters send_parameters_;
  cricket::AudioRecvParameters recv_parameters_;
  FakeAudioSource fake_source_;
 private:
  webrtc::test::ScopedFieldTrials override_field_trials_;
};

// Tests that we can create and destroy a channel.
TEST_F(WebRtcVoiceEngineTestFake, CreateChannel) {
  EXPECT_TRUE(SetupChannel());
}

// Test that we can add a send stream and that it has the correct defaults.
TEST_F(WebRtcVoiceEngineTestFake, CreateSendStream) {
  EXPECT_TRUE(SetupChannel());
  EXPECT_TRUE(
      channel_->AddSendStream(cricket::StreamParams::CreateLegacy(kSsrc1)));
  const webrtc::AudioSendStream::Config& config = GetSendStreamConfig(kSsrc1);
  EXPECT_EQ(kSsrc1, config.rtp.ssrc);
  EXPECT_EQ("", config.rtp.c_name);
  EXPECT_EQ(0u, config.rtp.extensions.size());
  EXPECT_EQ(static_cast<cricket::WebRtcVoiceMediaChannel*>(channel_),
            config.send_transport);
}

// Test that we can add a receive stream and that it has the correct defaults.
TEST_F(WebRtcVoiceEngineTestFake, CreateRecvStream) {
  EXPECT_TRUE(SetupChannel());
  EXPECT_TRUE(AddRecvStream(kSsrc1));
  const webrtc::AudioReceiveStream::Config& config =
      GetRecvStreamConfig(kSsrc1);
  EXPECT_EQ(kSsrc1, config.rtp.remote_ssrc);
  EXPECT_EQ(0xFA17FA17, config.rtp.local_ssrc);
  EXPECT_FALSE(config.rtp.transport_cc);
  EXPECT_EQ(0u, config.rtp.extensions.size());
  EXPECT_EQ(static_cast<cricket::WebRtcVoiceMediaChannel*>(channel_),
            config.rtcp_send_transport);
  EXPECT_EQ("", config.sync_group);
}

// Tests that the list of supported codecs is created properly and ordered
// correctly (such that opus appears first).
// TODO(ossu): This test should move into a separate builtin audio codecs
// module.
TEST_F(WebRtcVoiceEngineTestFake, CodecOrder) {
  const std::vector<cricket::AudioCodec>& codecs = engine_->send_codecs();
  ASSERT_FALSE(codecs.empty());
  EXPECT_STRCASEEQ("opus", codecs[0].name.c_str());
  EXPECT_EQ(48000, codecs[0].clockrate);
  EXPECT_EQ(2, codecs[0].channels);
  EXPECT_EQ(64000, codecs[0].bitrate);
}

TEST_F(WebRtcVoiceEngineTestFake, OpusSupportsTransportCc) {
  const std::vector<cricket::AudioCodec>& codecs = engine_->send_codecs();
  bool opus_found = false;
  for (cricket::AudioCodec codec : codecs) {
    if (codec.name == "opus") {
      EXPECT_TRUE(HasTransportCc(codec));
      opus_found = true;
    }
  }
  EXPECT_TRUE(opus_found);
}

// Tests that we can find codecs by name or id, and that we interpret the
// clockrate and bitrate fields properly.
TEST_F(WebRtcVoiceEngineTestFake, FindCodec) {
  cricket::AudioCodec codec;
  webrtc::CodecInst codec_inst;
  // Find PCMU with explicit clockrate and bitrate.
  EXPECT_TRUE(cricket::WebRtcVoiceEngine::ToCodecInst(kPcmuCodec, &codec_inst));
  // Find ISAC with explicit clockrate and 0 bitrate.
  EXPECT_TRUE(cricket::WebRtcVoiceEngine::ToCodecInst(kIsacCodec, &codec_inst));
  // Find telephone-event with explicit clockrate and 0 bitrate.
  EXPECT_TRUE(cricket::WebRtcVoiceEngine::ToCodecInst(kTelephoneEventCodec,
                                                      &codec_inst));
  // Find ISAC with a different payload id.
  codec = kIsacCodec;
  codec.id = 127;
  EXPECT_TRUE(cricket::WebRtcVoiceEngine::ToCodecInst(codec, &codec_inst));
  EXPECT_EQ(codec.id, codec_inst.pltype);
  // Find PCMU with a 0 clockrate.
  codec = kPcmuCodec;
  codec.clockrate = 0;
  EXPECT_TRUE(cricket::WebRtcVoiceEngine::ToCodecInst(codec, &codec_inst));
  EXPECT_EQ(codec.id, codec_inst.pltype);
  EXPECT_EQ(8000, codec_inst.plfreq);
  // Find PCMU with a 0 bitrate.
  codec = kPcmuCodec;
  codec.bitrate = 0;
  EXPECT_TRUE(cricket::WebRtcVoiceEngine::ToCodecInst(codec, &codec_inst));
  EXPECT_EQ(codec.id, codec_inst.pltype);
  EXPECT_EQ(64000, codec_inst.rate);
  // Find ISAC with an explicit bitrate.
  codec = kIsacCodec;
  codec.bitrate = 32000;
  EXPECT_TRUE(cricket::WebRtcVoiceEngine::ToCodecInst(codec, &codec_inst));
  EXPECT_EQ(codec.id, codec_inst.pltype);
  EXPECT_EQ(32000, codec_inst.rate);
}

// Test that we set our inbound codecs properly, including changing PT.
TEST_F(WebRtcVoiceEngineTestFake, SetRecvCodecs) {
  EXPECT_TRUE(SetupChannel());
  cricket::AudioRecvParameters parameters;
  parameters.codecs.push_back(kIsacCodec);
  parameters.codecs.push_back(kPcmuCodec);
  parameters.codecs.push_back(kTelephoneEventCodec);
  parameters.codecs[0].id = 106;  // collide with existing telephone-event
  parameters.codecs[2].id = 126;
  EXPECT_TRUE(channel_->SetRecvParameters(parameters));
  EXPECT_TRUE(AddRecvStream(kSsrc1));
  int channel_num = voe_.GetLastChannel();
  webrtc::CodecInst gcodec;
  rtc::strcpyn(gcodec.plname, arraysize(gcodec.plname), "ISAC");
  gcodec.plfreq = 16000;
  gcodec.channels = 1;
  EXPECT_EQ(0, voe_.GetRecPayloadType(channel_num, gcodec));
  EXPECT_EQ(106, gcodec.pltype);
  EXPECT_STREQ("ISAC", gcodec.plname);
  rtc::strcpyn(gcodec.plname, arraysize(gcodec.plname), "telephone-event");
  gcodec.plfreq = 8000;
  EXPECT_EQ(0, voe_.GetRecPayloadType(channel_num, gcodec));
  EXPECT_EQ(126, gcodec.pltype);
  EXPECT_STREQ("telephone-event", gcodec.plname);
}

// Test that we fail to set an unknown inbound codec.
TEST_F(WebRtcVoiceEngineTestFake, SetRecvCodecsUnsupportedCodec) {
  EXPECT_TRUE(SetupChannel());
  cricket::AudioRecvParameters parameters;
  parameters.codecs.push_back(kIsacCodec);
  parameters.codecs.push_back(cricket::AudioCodec(127, "XYZ", 32000, 0, 1));
  EXPECT_FALSE(channel_->SetRecvParameters(parameters));
}

// Test that we fail if we have duplicate types in the inbound list.
TEST_F(WebRtcVoiceEngineTestFake, SetRecvCodecsDuplicatePayloadType) {
  EXPECT_TRUE(SetupChannel());
  cricket::AudioRecvParameters parameters;
  parameters.codecs.push_back(kIsacCodec);
  parameters.codecs.push_back(kCn16000Codec);
  parameters.codecs[1].id = kIsacCodec.id;
  EXPECT_FALSE(channel_->SetRecvParameters(parameters));
}

// Test that we can decode OPUS without stereo parameters.
TEST_F(WebRtcVoiceEngineTestFake, SetRecvCodecsWithOpusNoStereo) {
  EXPECT_TRUE(SetupChannel());
  cricket::AudioRecvParameters parameters;
  parameters.codecs.push_back(kIsacCodec);
  parameters.codecs.push_back(kPcmuCodec);
  parameters.codecs.push_back(kOpusCodec);
  EXPECT_TRUE(channel_->SetRecvParameters(parameters));
  EXPECT_TRUE(AddRecvStream(kSsrc1));
  int channel_num = voe_.GetLastChannel();
  webrtc::CodecInst opus;
  cricket::WebRtcVoiceEngine::ToCodecInst(kOpusCodec, &opus);
  // Even without stereo parameters, recv codecs still specify channels = 2.
  EXPECT_EQ(2, opus.channels);
  EXPECT_EQ(111, opus.pltype);
  EXPECT_STREQ("opus", opus.plname);
  opus.pltype = 0;
  EXPECT_EQ(0, voe_.GetRecPayloadType(channel_num, opus));
  EXPECT_EQ(111, opus.pltype);
}

// Test that we can decode OPUS with stereo = 0.
TEST_F(WebRtcVoiceEngineTestFake, SetRecvCodecsWithOpus0Stereo) {
  EXPECT_TRUE(SetupChannel());
  cricket::AudioRecvParameters parameters;
  parameters.codecs.push_back(kIsacCodec);
  parameters.codecs.push_back(kPcmuCodec);
  parameters.codecs.push_back(kOpusCodec);
  parameters.codecs[2].params["stereo"] = "0";
  EXPECT_TRUE(channel_->SetRecvParameters(parameters));
  EXPECT_TRUE(AddRecvStream(kSsrc1));
  int channel_num2 = voe_.GetLastChannel();
  webrtc::CodecInst opus;
  cricket::WebRtcVoiceEngine::ToCodecInst(kOpusCodec, &opus);
  // Even when stereo is off, recv codecs still specify channels = 2.
  EXPECT_EQ(2, opus.channels);
  EXPECT_EQ(111, opus.pltype);
  EXPECT_STREQ("opus", opus.plname);
  opus.pltype = 0;
  EXPECT_EQ(0, voe_.GetRecPayloadType(channel_num2, opus));
  EXPECT_EQ(111, opus.pltype);
}

// Test that we can decode OPUS with stereo = 1.
TEST_F(WebRtcVoiceEngineTestFake, SetRecvCodecsWithOpus1Stereo) {
  EXPECT_TRUE(SetupChannel());
  cricket::AudioRecvParameters parameters;
  parameters.codecs.push_back(kIsacCodec);
  parameters.codecs.push_back(kPcmuCodec);
  parameters.codecs.push_back(kOpusCodec);
  parameters.codecs[2].params["stereo"] = "1";
  EXPECT_TRUE(channel_->SetRecvParameters(parameters));
  EXPECT_TRUE(AddRecvStream(kSsrc1));
  int channel_num2 = voe_.GetLastChannel();
  webrtc::CodecInst opus;
  cricket::WebRtcVoiceEngine::ToCodecInst(kOpusCodec, &opus);
  EXPECT_EQ(2, opus.channels);
  EXPECT_EQ(111, opus.pltype);
  EXPECT_STREQ("opus", opus.plname);
  opus.pltype = 0;
  EXPECT_EQ(0, voe_.GetRecPayloadType(channel_num2, opus));
  EXPECT_EQ(111, opus.pltype);
}

// Test that changes to recv codecs are applied to all streams.
TEST_F(WebRtcVoiceEngineTestFake, SetRecvCodecsWithMultipleStreams) {
  EXPECT_TRUE(SetupChannel());
  cricket::AudioRecvParameters parameters;
  parameters.codecs.push_back(kIsacCodec);
  parameters.codecs.push_back(kPcmuCodec);
  parameters.codecs.push_back(kTelephoneEventCodec);
  parameters.codecs[0].id = 106;  // collide with existing telephone-event
  parameters.codecs[2].id = 126;
  EXPECT_TRUE(channel_->SetRecvParameters(parameters));
  EXPECT_TRUE(AddRecvStream(kSsrc1));
  int channel_num2 = voe_.GetLastChannel();
  webrtc::CodecInst gcodec;
  rtc::strcpyn(gcodec.plname, arraysize(gcodec.plname), "ISAC");
  gcodec.plfreq = 16000;
  gcodec.channels = 1;
  EXPECT_EQ(0, voe_.GetRecPayloadType(channel_num2, gcodec));
  EXPECT_EQ(106, gcodec.pltype);
  EXPECT_STREQ("ISAC", gcodec.plname);
  rtc::strcpyn(gcodec.plname, arraysize(gcodec.plname), "telephone-event");
  gcodec.plfreq = 8000;
  gcodec.channels = 1;
  EXPECT_EQ(0, voe_.GetRecPayloadType(channel_num2, gcodec));
  EXPECT_EQ(126, gcodec.pltype);
  EXPECT_STREQ("telephone-event", gcodec.plname);
}

TEST_F(WebRtcVoiceEngineTestFake, SetRecvCodecsAfterAddingStreams) {
  EXPECT_TRUE(SetupRecvStream());
  cricket::AudioRecvParameters parameters;
  parameters.codecs.push_back(kIsacCodec);
  parameters.codecs[0].id = 106;  // collide with existing telephone-event
  EXPECT_TRUE(channel_->SetRecvParameters(parameters));

  int channel_num2 = voe_.GetLastChannel();
  webrtc::CodecInst gcodec;
  rtc::strcpyn(gcodec.plname, arraysize(gcodec.plname), "ISAC");
  gcodec.plfreq = 16000;
  gcodec.channels = 1;
  EXPECT_EQ(0, voe_.GetRecPayloadType(channel_num2, gcodec));
  EXPECT_EQ(106, gcodec.pltype);
  EXPECT_STREQ("ISAC", gcodec.plname);
}

// Test that we can apply the same set of codecs again while playing.
TEST_F(WebRtcVoiceEngineTestFake, SetRecvCodecsWhilePlaying) {
  EXPECT_TRUE(SetupRecvStream());
  cricket::AudioRecvParameters parameters;
  parameters.codecs.push_back(kIsacCodec);
  parameters.codecs.push_back(kCn16000Codec);
  EXPECT_TRUE(channel_->SetRecvParameters(parameters));
  channel_->SetPlayout(true);
  EXPECT_TRUE(channel_->SetRecvParameters(parameters));

  // Changing the payload type of a codec should fail.
  parameters.codecs[0].id = 127;
  EXPECT_FALSE(channel_->SetRecvParameters(parameters));
  EXPECT_TRUE(GetRecvStream(kSsrc1).started());
}

// Test that we can add a codec while playing.
TEST_F(WebRtcVoiceEngineTestFake, AddRecvCodecsWhilePlaying) {
  EXPECT_TRUE(SetupRecvStream());
  cricket::AudioRecvParameters parameters;
  parameters.codecs.push_back(kIsacCodec);
  parameters.codecs.push_back(kCn16000Codec);
  EXPECT_TRUE(channel_->SetRecvParameters(parameters));
  channel_->SetPlayout(true);

  parameters.codecs.push_back(kOpusCodec);
  EXPECT_TRUE(channel_->SetRecvParameters(parameters));
  EXPECT_TRUE(GetRecvStream(kSsrc1).started());
  webrtc::CodecInst gcodec;
  EXPECT_TRUE(cricket::WebRtcVoiceEngine::ToCodecInst(kOpusCodec, &gcodec));
  EXPECT_EQ(kOpusCodec.id, gcodec.pltype);
}

TEST_F(WebRtcVoiceEngineTestFake, SetSendBandwidthAuto) {
  EXPECT_TRUE(SetupSendStream());

  // Test that when autobw is enabled, bitrate is kept as the default
  // value. autobw is enabled for the following tests because the target
  // bitrate is <= 0.

  // ISAC, default bitrate == 32000.
  TestMaxSendBandwidth(kIsacCodec, 0, true, 32000);

  // PCMU, default bitrate == 64000.
  TestMaxSendBandwidth(kPcmuCodec, -1, true, 64000);

  // opus, default bitrate == 64000.
  TestMaxSendBandwidth(kOpusCodec, -1, true, 64000);
}

TEST_F(WebRtcVoiceEngineTestFake, SetMaxSendBandwidthMultiRateAsCaller) {
  EXPECT_TRUE(SetupSendStream());

  // Test that the bitrate of a multi-rate codec is always the maximum.

  // ISAC, default bitrate == 32000.
  TestMaxSendBandwidth(kIsacCodec, 40000, true, 40000);
  TestMaxSendBandwidth(kIsacCodec, 16000, true, 16000);
  // Rates above the max (56000) should be capped.
  TestMaxSendBandwidth(kIsacCodec, 100000, true, 56000);

  // opus, default bitrate == 64000.
  TestMaxSendBandwidth(kOpusCodec, 96000, true, 96000);
  TestMaxSendBandwidth(kOpusCodec, 48000, true, 48000);
  // Rates above the max (510000) should be capped.
  TestMaxSendBandwidth(kOpusCodec, 600000, true, 510000);
}

TEST_F(WebRtcVoiceEngineTestFake, SetMaxSendBandwidthFixedRateAsCaller) {
  EXPECT_TRUE(SetupSendStream());

  // Test that we can only set a maximum bitrate for a fixed-rate codec
  // if it's bigger than the fixed rate.

  // PCMU, fixed bitrate == 64000.
  TestMaxSendBandwidth(kPcmuCodec, 0, true, 64000);
  TestMaxSendBandwidth(kPcmuCodec, 1, false, 64000);
  TestMaxSendBandwidth(kPcmuCodec, 128000, true, 64000);
  TestMaxSendBandwidth(kPcmuCodec, 32000, false, 64000);
  TestMaxSendBandwidth(kPcmuCodec, 64000, true, 64000);
  TestMaxSendBandwidth(kPcmuCodec, 63999, false, 64000);
  TestMaxSendBandwidth(kPcmuCodec, 64001, true, 64000);
}

TEST_F(WebRtcVoiceEngineTestFake, SetMaxSendBandwidthMultiRateAsCallee) {
  EXPECT_TRUE(SetupChannel());
  const int kDesiredBitrate = 128000;
  cricket::AudioSendParameters parameters;
  parameters.codecs = engine_->send_codecs();
  parameters.max_bandwidth_bps = kDesiredBitrate;
  SetSendParameters(parameters);

  EXPECT_TRUE(channel_->AddSendStream(
      cricket::StreamParams::CreateLegacy(kSsrc1)));

  EXPECT_EQ(kDesiredBitrate, GetCodecBitrate(kSsrc1));
}

// Test that bitrate cannot be set for CBR codecs.
// Bitrate is ignored if it is higher than the fixed bitrate.
// Bitrate less then the fixed bitrate is an error.
TEST_F(WebRtcVoiceEngineTestFake, SetMaxSendBandwidthCbr) {
  EXPECT_TRUE(SetupSendStream());

  // PCMU, default bitrate == 64000.
  SetSendParameters(send_parameters_);
  EXPECT_EQ(64000, GetCodecBitrate(kSsrc1));

  send_parameters_.max_bandwidth_bps = 128000;
  SetSendParameters(send_parameters_);
  EXPECT_EQ(64000, GetCodecBitrate(kSsrc1));

  send_parameters_.max_bandwidth_bps = 128;
  EXPECT_FALSE(channel_->SetSendParameters(send_parameters_));
  EXPECT_EQ(64000, GetCodecBitrate(kSsrc1));
}

// Test that the per-stream bitrate limit and the global
// bitrate limit both apply.
TEST_F(WebRtcVoiceEngineTestFake, SetMaxBitratePerStream) {
  EXPECT_TRUE(SetupSendStream());

  // opus, default bitrate == 64000.
  SetAndExpectMaxBitrate(kOpusCodec, 0, 0, true, 64000);
  SetAndExpectMaxBitrate(kOpusCodec, 48000, 0, true, 48000);
  SetAndExpectMaxBitrate(kOpusCodec, 48000, 64000, true, 48000);
  SetAndExpectMaxBitrate(kOpusCodec, 64000, 48000, true, 48000);

  // CBR codecs allow both maximums to exceed the bitrate.
  SetAndExpectMaxBitrate(kPcmuCodec, 0, 0, true, 64000);
  SetAndExpectMaxBitrate(kPcmuCodec, 64001, 0, true, 64000);
  SetAndExpectMaxBitrate(kPcmuCodec, 0, 64001, true, 64000);
  SetAndExpectMaxBitrate(kPcmuCodec, 64001, 64001, true, 64000);

  // CBR codecs don't allow per stream maximums to be too low.
  SetAndExpectMaxBitrate(kPcmuCodec, 0, 63999, false, 64000);
  SetAndExpectMaxBitrate(kPcmuCodec, 64001, 63999, false, 64000);
}

// Test that an attempt to set RtpParameters for a stream that does not exist
// fails.
TEST_F(WebRtcVoiceEngineTestFake, CannotSetMaxBitrateForNonexistentStream) {
  EXPECT_TRUE(SetupChannel());
  webrtc::RtpParameters nonexistent_parameters =
      channel_->GetRtpSendParameters(kSsrc1);
  EXPECT_EQ(0, nonexistent_parameters.encodings.size());

  nonexistent_parameters.encodings.push_back(webrtc::RtpEncodingParameters());
  EXPECT_FALSE(channel_->SetRtpSendParameters(kSsrc1, nonexistent_parameters));
}

TEST_F(WebRtcVoiceEngineTestFake,
       CannotSetRtpSendParametersWithIncorrectNumberOfEncodings) {
  // This test verifies that setting RtpParameters succeeds only if
  // the structure contains exactly one encoding.
  // TODO(skvlad): Update this test when we start supporting setting parameters
  // for each encoding individually.

  EXPECT_TRUE(SetupSendStream());
  // Setting RtpParameters with no encoding is expected to fail.
  webrtc::RtpParameters parameters;
  EXPECT_FALSE(channel_->SetRtpSendParameters(kSsrc1, parameters));
  // Setting RtpParameters with exactly one encoding should succeed.
  parameters.encodings.push_back(webrtc::RtpEncodingParameters());
  EXPECT_TRUE(channel_->SetRtpSendParameters(kSsrc1, parameters));
  // Two or more encodings should result in failure.
  parameters.encodings.push_back(webrtc::RtpEncodingParameters());
  EXPECT_FALSE(channel_->SetRtpSendParameters(kSsrc1, parameters));
}

// Test that a stream will not be sending if its encoding is made
// inactive through SetRtpSendParameters.
TEST_F(WebRtcVoiceEngineTestFake, SetRtpParametersEncodingsActive) {
  EXPECT_TRUE(SetupSendStream());
  SetSend(true);
  EXPECT_TRUE(GetSendStream(kSsrc1).IsSending());
  // Get current parameters and change "active" to false.
  webrtc::RtpParameters parameters = channel_->GetRtpSendParameters(kSsrc1);
  ASSERT_EQ(1u, parameters.encodings.size());
  ASSERT_TRUE(parameters.encodings[0].active);
  parameters.encodings[0].active = false;
  EXPECT_TRUE(channel_->SetRtpSendParameters(kSsrc1, parameters));
  EXPECT_FALSE(GetSendStream(kSsrc1).IsSending());

  // Now change it back to active and verify we resume sending.
  parameters.encodings[0].active = true;
  EXPECT_TRUE(channel_->SetRtpSendParameters(kSsrc1, parameters));
  EXPECT_TRUE(GetSendStream(kSsrc1).IsSending());
}

// Test that SetRtpSendParameters configures the correct encoding channel for
// each SSRC.
TEST_F(WebRtcVoiceEngineTestFake, RtpParametersArePerStream) {
  SetupForMultiSendStream();
  // Create send streams.
  for (uint32_t ssrc : kSsrcs4) {
    EXPECT_TRUE(
        channel_->AddSendStream(cricket::StreamParams::CreateLegacy(ssrc)));
  }
  // Configure one stream to be limited by the stream config, another to be
  // limited by the global max, and the third one with no per-stream limit
  // (still subject to the global limit).
  SetGlobalMaxBitrate(kOpusCodec, 64000);
  EXPECT_TRUE(SetMaxBitrateForStream(kSsrcs4[0], 48000));
  EXPECT_TRUE(SetMaxBitrateForStream(kSsrcs4[1], 96000));
  EXPECT_TRUE(SetMaxBitrateForStream(kSsrcs4[2], -1));

  EXPECT_EQ(48000, GetCodecBitrate(kSsrcs4[0]));
  EXPECT_EQ(64000, GetCodecBitrate(kSsrcs4[1]));
  EXPECT_EQ(64000, GetCodecBitrate(kSsrcs4[2]));

  // Remove the global cap; the streams should switch to their respective
  // maximums (or remain unchanged if there was no other limit on them.)
  SetGlobalMaxBitrate(kOpusCodec, -1);
  EXPECT_EQ(48000, GetCodecBitrate(kSsrcs4[0]));
  EXPECT_EQ(96000, GetCodecBitrate(kSsrcs4[1]));
  EXPECT_EQ(64000, GetCodecBitrate(kSsrcs4[2]));
}

// Test that GetRtpSendParameters returns the currently configured codecs.
TEST_F(WebRtcVoiceEngineTestFake, GetRtpSendParametersCodecs) {
  EXPECT_TRUE(SetupSendStream());
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kIsacCodec);
  parameters.codecs.push_back(kPcmuCodec);
  SetSendParameters(parameters);

  webrtc::RtpParameters rtp_parameters = channel_->GetRtpSendParameters(kSsrc1);
  ASSERT_EQ(2u, rtp_parameters.codecs.size());
  EXPECT_EQ(kIsacCodec.ToCodecParameters(), rtp_parameters.codecs[0]);
  EXPECT_EQ(kPcmuCodec.ToCodecParameters(), rtp_parameters.codecs[1]);
}

// Test that if we set/get parameters multiple times, we get the same results.
TEST_F(WebRtcVoiceEngineTestFake, SetAndGetRtpSendParameters) {
  EXPECT_TRUE(SetupSendStream());
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kIsacCodec);
  parameters.codecs.push_back(kPcmuCodec);
  SetSendParameters(parameters);

  webrtc::RtpParameters initial_params = channel_->GetRtpSendParameters(kSsrc1);

  // We should be able to set the params we just got.
  EXPECT_TRUE(channel_->SetRtpSendParameters(kSsrc1, initial_params));

  // ... And this shouldn't change the params returned by GetRtpSendParameters.
  webrtc::RtpParameters new_params = channel_->GetRtpSendParameters(kSsrc1);
  EXPECT_EQ(initial_params, channel_->GetRtpSendParameters(kSsrc1));
}

// Test that GetRtpReceiveParameters returns the currently configured codecs.
TEST_F(WebRtcVoiceEngineTestFake, GetRtpReceiveParametersCodecs) {
  EXPECT_TRUE(SetupRecvStream());
  cricket::AudioRecvParameters parameters;
  parameters.codecs.push_back(kIsacCodec);
  parameters.codecs.push_back(kPcmuCodec);
  EXPECT_TRUE(channel_->SetRecvParameters(parameters));

  webrtc::RtpParameters rtp_parameters =
      channel_->GetRtpReceiveParameters(kSsrc1);
  ASSERT_EQ(2u, rtp_parameters.codecs.size());
  EXPECT_EQ(kIsacCodec.ToCodecParameters(), rtp_parameters.codecs[0]);
  EXPECT_EQ(kPcmuCodec.ToCodecParameters(), rtp_parameters.codecs[1]);
}

// Test that if we set/get parameters multiple times, we get the same results.
TEST_F(WebRtcVoiceEngineTestFake, SetAndGetRtpReceiveParameters) {
  EXPECT_TRUE(SetupRecvStream());
  cricket::AudioRecvParameters parameters;
  parameters.codecs.push_back(kIsacCodec);
  parameters.codecs.push_back(kPcmuCodec);
  EXPECT_TRUE(channel_->SetRecvParameters(parameters));

  webrtc::RtpParameters initial_params =
      channel_->GetRtpReceiveParameters(kSsrc1);

  // We should be able to set the params we just got.
  EXPECT_TRUE(channel_->SetRtpReceiveParameters(kSsrc1, initial_params));

  // ... And this shouldn't change the params returned by
  // GetRtpReceiveParameters.
  webrtc::RtpParameters new_params = channel_->GetRtpReceiveParameters(kSsrc1);
  EXPECT_EQ(initial_params, channel_->GetRtpReceiveParameters(kSsrc1));
}

// Test that we apply codecs properly.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecs) {
  EXPECT_TRUE(SetupSendStream());
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kIsacCodec);
  parameters.codecs.push_back(kPcmuCodec);
  parameters.codecs.push_back(kCn8000Codec);
  parameters.codecs[0].id = 96;
  parameters.codecs[0].bitrate = 48000;
  const int initial_num = call_.GetNumCreatedSendStreams();
  SetSendParameters(parameters);
  EXPECT_EQ(initial_num + 1, call_.GetNumCreatedSendStreams());
  const auto& send_codec_spec = GetSendStreamConfig(kSsrc1).send_codec_spec;
  EXPECT_EQ(96, send_codec_spec.codec_inst.pltype);
  EXPECT_EQ(48000, send_codec_spec.codec_inst.rate);
  EXPECT_STREQ("ISAC", send_codec_spec.codec_inst.plname);
  EXPECT_NE(send_codec_spec.codec_inst.plfreq, send_codec_spec.cng_plfreq);
  EXPECT_EQ(13, send_codec_spec.cng_payload_type);
  EXPECT_EQ(webrtc::kFreq8000Hz, send_codec_spec.cng_plfreq);
  EXPECT_FALSE(channel_->CanInsertDtmf());
}

// Test that VoE Channel doesn't call SetSendCodec again if same codec is tried
// to apply.
TEST_F(WebRtcVoiceEngineTestFake, DontResetSetSendCodec) {
  EXPECT_TRUE(SetupSendStream());
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kIsacCodec);
  parameters.codecs.push_back(kPcmuCodec);
  parameters.codecs.push_back(kCn8000Codec);
  parameters.codecs[0].id = 96;
  parameters.codecs[0].bitrate = 48000;
  const int initial_num = call_.GetNumCreatedSendStreams();
  SetSendParameters(parameters);
  EXPECT_EQ(initial_num + 1, call_.GetNumCreatedSendStreams());
  // Calling SetSendCodec again with same codec which is already set.
  // In this case media channel shouldn't send codec to VoE.
  SetSendParameters(parameters);
  EXPECT_EQ(initial_num + 1, call_.GetNumCreatedSendStreams());
}

// Verify that G722 is set with 16000 samples per second to WebRTC.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecG722) {
  EXPECT_TRUE(SetupSendStream());
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kG722CodecSdp);
  SetSendParameters(parameters);
  const auto& gcodec = GetSendStreamConfig(kSsrc1).send_codec_spec.codec_inst;
  EXPECT_STREQ("G722", gcodec.plname);
  EXPECT_EQ(1, gcodec.channels);
  EXPECT_EQ(16000, gcodec.plfreq);
}

// Test that if clockrate is not 48000 for opus, we fail.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecOpusBadClockrate) {
  EXPECT_TRUE(SetupSendStream());
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kOpusCodec);
  parameters.codecs[0].bitrate = 0;
  parameters.codecs[0].clockrate = 50000;
  EXPECT_FALSE(channel_->SetSendParameters(parameters));
}

// Test that if channels=0 for opus, we fail.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecOpusBad0ChannelsNoStereo) {
  EXPECT_TRUE(SetupSendStream());
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kOpusCodec);
  parameters.codecs[0].bitrate = 0;
  parameters.codecs[0].channels = 0;
  EXPECT_FALSE(channel_->SetSendParameters(parameters));
}

// Test that if channels=0 for opus, we fail.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecOpusBad0Channels1Stereo) {
  EXPECT_TRUE(SetupSendStream());
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kOpusCodec);
  parameters.codecs[0].bitrate = 0;
  parameters.codecs[0].channels = 0;
  parameters.codecs[0].params["stereo"] = "1";
  EXPECT_FALSE(channel_->SetSendParameters(parameters));
}

// Test that if channel is 1 for opus and there's no stereo, we fail.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecOpus1ChannelNoStereo) {
  EXPECT_TRUE(SetupSendStream());
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kOpusCodec);
  parameters.codecs[0].bitrate = 0;
  parameters.codecs[0].channels = 1;
  EXPECT_FALSE(channel_->SetSendParameters(parameters));
}

// Test that if channel is 1 for opus and stereo=0, we fail.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecOpusBad1Channel0Stereo) {
  EXPECT_TRUE(SetupSendStream());
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kOpusCodec);
  parameters.codecs[0].bitrate = 0;
  parameters.codecs[0].channels = 1;
  parameters.codecs[0].params["stereo"] = "0";
  EXPECT_FALSE(channel_->SetSendParameters(parameters));
}

// Test that if channel is 1 for opus and stereo=1, we fail.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecOpusBad1Channel1Stereo) {
  EXPECT_TRUE(SetupSendStream());
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kOpusCodec);
  parameters.codecs[0].bitrate = 0;
  parameters.codecs[0].channels = 1;
  parameters.codecs[0].params["stereo"] = "1";
  EXPECT_FALSE(channel_->SetSendParameters(parameters));
}

// Test that with bitrate=0 and no stereo,
// channels and bitrate are 1 and 32000.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecOpusGood0BitrateNoStereo) {
  EXPECT_TRUE(SetupSendStream());
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kOpusCodec);
  parameters.codecs[0].bitrate = 0;
  SetSendParameters(parameters);
  CheckSendCodec(kSsrc1, "opus", 1, 32000);
}

// Test that with bitrate=0 and stereo=0,
// channels and bitrate are 1 and 32000.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecOpusGood0Bitrate0Stereo) {
  EXPECT_TRUE(SetupSendStream());
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kOpusCodec);
  parameters.codecs[0].bitrate = 0;
  parameters.codecs[0].params["stereo"] = "0";
  SetSendParameters(parameters);
  CheckSendCodec(kSsrc1, "opus", 1, 32000);
}

// Test that with bitrate=invalid and stereo=0,
// channels and bitrate are 1 and 32000.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecOpusGoodXBitrate0Stereo) {
  EXPECT_TRUE(SetupSendStream());
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kOpusCodec);
  parameters.codecs[0].params["stereo"] = "0";
  // bitrate that's out of the range between 6000 and 510000 will be clamped.
  parameters.codecs[0].bitrate = 5999;
  SetSendParameters(parameters);
  CheckSendCodec(kSsrc1, "opus", 1, 6000);

  parameters.codecs[0].bitrate = 510001;
  SetSendParameters(parameters);
  CheckSendCodec(kSsrc1, "opus", 1, 510000);
}

// Test that with bitrate=0 and stereo=1,
// channels and bitrate are 2 and 64000.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecOpusGood0Bitrate1Stereo) {
  EXPECT_TRUE(SetupSendStream());
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kOpusCodec);
  parameters.codecs[0].bitrate = 0;
  parameters.codecs[0].params["stereo"] = "1";
  SetSendParameters(parameters);
  CheckSendCodec(kSsrc1, "opus", 2, 64000);
}

// Test that with bitrate=invalid and stereo=1,
// channels and bitrate are 2 and 64000.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecOpusGoodXBitrate1Stereo) {
  EXPECT_TRUE(SetupSendStream());
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kOpusCodec);
  parameters.codecs[0].params["stereo"] = "1";
  // bitrate that's out of the range between 6000 and 510000 will be clamped.
  parameters.codecs[0].bitrate = 5999;
  SetSendParameters(parameters);
  CheckSendCodec(kSsrc1, "opus", 2, 6000);

  parameters.codecs[0].bitrate = 510001;
  SetSendParameters(parameters);
  CheckSendCodec(kSsrc1, "opus", 2, 510000);
}

// Test that with bitrate=N and stereo unset,
// channels and bitrate are 1 and N.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecOpusGoodNBitrateNoStereo) {
  EXPECT_TRUE(SetupSendStream());
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kOpusCodec);
  parameters.codecs[0].bitrate = 96000;
  SetSendParameters(parameters);
  const auto& gcodec = GetSendStreamConfig(kSsrc1).send_codec_spec.codec_inst;
  EXPECT_EQ(111, gcodec.pltype);
  EXPECT_EQ(96000, gcodec.rate);
  EXPECT_STREQ("opus", gcodec.plname);
  EXPECT_EQ(1, gcodec.channels);
  EXPECT_EQ(48000, gcodec.plfreq);
}

// Test that with bitrate=N and stereo=0,
// channels and bitrate are 1 and N.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecOpusGoodNBitrate0Stereo) {
  EXPECT_TRUE(SetupSendStream());
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kOpusCodec);
  parameters.codecs[0].bitrate = 30000;
  parameters.codecs[0].params["stereo"] = "0";
  SetSendParameters(parameters);
  CheckSendCodec(kSsrc1, "opus", 1, 30000);
}

// Test that with bitrate=N and without any parameters,
// channels and bitrate are 1 and N.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecOpusGoodNBitrateNoParameters) {
  EXPECT_TRUE(SetupSendStream());
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kOpusCodec);
  parameters.codecs[0].bitrate = 30000;
  SetSendParameters(parameters);
  CheckSendCodec(kSsrc1, "opus", 1, 30000);
}

// Test that with bitrate=N and stereo=1,
// channels and bitrate are 2 and N.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecOpusGoodNBitrate1Stereo) {
  EXPECT_TRUE(SetupSendStream());
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kOpusCodec);
  parameters.codecs[0].bitrate = 30000;
  parameters.codecs[0].params["stereo"] = "1";
  SetSendParameters(parameters);
  CheckSendCodec(kSsrc1, "opus", 2, 30000);
}

// Test that bitrate will be overridden by the "maxaveragebitrate" parameter.
// Also test that the "maxaveragebitrate" can't be set to values outside the
// range of 6000 and 510000
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecOpusMaxAverageBitrate) {
  EXPECT_TRUE(SetupSendStream());
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kOpusCodec);
  parameters.codecs[0].bitrate = 30000;
  // Ignore if less than 6000.
  parameters.codecs[0].params["maxaveragebitrate"] = "5999";
  SetSendParameters(parameters);
  EXPECT_EQ(6000, GetCodecBitrate(kSsrc1));

  // Ignore if larger than 510000.
  parameters.codecs[0].params["maxaveragebitrate"] = "510001";
  SetSendParameters(parameters);
  EXPECT_EQ(510000, GetCodecBitrate(kSsrc1));

  parameters.codecs[0].params["maxaveragebitrate"] = "200000";
  SetSendParameters(parameters);
  EXPECT_EQ(200000, GetCodecBitrate(kSsrc1));
}

// Test that we can enable NACK with opus as caller.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecEnableNackAsCaller) {
  EXPECT_TRUE(SetupSendStream());
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kOpusCodec);
  parameters.codecs[0].AddFeedbackParam(
      cricket::FeedbackParam(cricket::kRtcpFbParamNack,
                             cricket::kParamValueEmpty));
  EXPECT_EQ(0, GetSendStreamConfig(kSsrc1).rtp.nack.rtp_history_ms);
  SetSendParameters(parameters);
  EXPECT_EQ(kRtpHistoryMs, GetSendStreamConfig(kSsrc1).rtp.nack.rtp_history_ms);
}

// Test that we can enable NACK with opus as callee.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecEnableNackAsCallee) {
  EXPECT_TRUE(SetupRecvStream());
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kOpusCodec);
  parameters.codecs[0].AddFeedbackParam(
      cricket::FeedbackParam(cricket::kRtcpFbParamNack,
                             cricket::kParamValueEmpty));
  EXPECT_EQ(0, GetRecvStreamConfig(kSsrc1).rtp.nack.rtp_history_ms);
  SetSendParameters(parameters);
  // NACK should be enabled even with no send stream.
  EXPECT_EQ(kRtpHistoryMs, GetRecvStreamConfig(kSsrc1).rtp.nack.rtp_history_ms);

  EXPECT_TRUE(channel_->AddSendStream(
      cricket::StreamParams::CreateLegacy(kSsrc1)));
  EXPECT_EQ(kRtpHistoryMs, GetSendStreamConfig(kSsrc1).rtp.nack.rtp_history_ms);
}

// Test that we can enable NACK on receive streams.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecEnableNackRecvStreams) {
  EXPECT_TRUE(SetupSendStream());
  EXPECT_TRUE(AddRecvStream(kSsrc2));
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kOpusCodec);
  parameters.codecs[0].AddFeedbackParam(
      cricket::FeedbackParam(cricket::kRtcpFbParamNack,
                             cricket::kParamValueEmpty));
  EXPECT_EQ(0, GetSendStreamConfig(kSsrc1).rtp.nack.rtp_history_ms);
  EXPECT_EQ(0, GetRecvStreamConfig(kSsrc2).rtp.nack.rtp_history_ms);
  SetSendParameters(parameters);
  EXPECT_EQ(kRtpHistoryMs, GetSendStreamConfig(kSsrc1).rtp.nack.rtp_history_ms);
  EXPECT_EQ(kRtpHistoryMs, GetRecvStreamConfig(kSsrc2).rtp.nack.rtp_history_ms);
}

// Test that we can disable NACK.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecDisableNack) {
  EXPECT_TRUE(SetupSendStream());
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kOpusCodec);
  parameters.codecs[0].AddFeedbackParam(
      cricket::FeedbackParam(cricket::kRtcpFbParamNack,
                             cricket::kParamValueEmpty));
  SetSendParameters(parameters);
  EXPECT_EQ(kRtpHistoryMs, GetSendStreamConfig(kSsrc1).rtp.nack.rtp_history_ms);

  parameters.codecs.clear();
  parameters.codecs.push_back(kOpusCodec);
  SetSendParameters(parameters);
  EXPECT_EQ(0, GetSendStreamConfig(kSsrc1).rtp.nack.rtp_history_ms);
}

// Test that we can disable NACK on receive streams.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecDisableNackRecvStreams) {
  EXPECT_TRUE(SetupSendStream());
  EXPECT_TRUE(AddRecvStream(kSsrc2));
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kOpusCodec);
  parameters.codecs[0].AddFeedbackParam(
      cricket::FeedbackParam(cricket::kRtcpFbParamNack,
                             cricket::kParamValueEmpty));
  SetSendParameters(parameters);
  EXPECT_EQ(kRtpHistoryMs, GetSendStreamConfig(kSsrc1).rtp.nack.rtp_history_ms);
  EXPECT_EQ(kRtpHistoryMs, GetRecvStreamConfig(kSsrc2).rtp.nack.rtp_history_ms);

  parameters.codecs.clear();
  parameters.codecs.push_back(kOpusCodec);
  SetSendParameters(parameters);
  EXPECT_EQ(0, GetSendStreamConfig(kSsrc1).rtp.nack.rtp_history_ms);
  EXPECT_EQ(0, GetRecvStreamConfig(kSsrc2).rtp.nack.rtp_history_ms);
}

// Test that NACK is enabled on a new receive stream.
TEST_F(WebRtcVoiceEngineTestFake, AddRecvStreamEnableNack) {
  EXPECT_TRUE(SetupSendStream());
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kIsacCodec);
  parameters.codecs.push_back(kCn16000Codec);
  parameters.codecs[0].AddFeedbackParam(
      cricket::FeedbackParam(cricket::kRtcpFbParamNack,
                             cricket::kParamValueEmpty));
  SetSendParameters(parameters);
  EXPECT_EQ(kRtpHistoryMs, GetSendStreamConfig(kSsrc1).rtp.nack.rtp_history_ms);

  EXPECT_TRUE(AddRecvStream(kSsrc2));
  EXPECT_EQ(kRtpHistoryMs, GetRecvStreamConfig(kSsrc2).rtp.nack.rtp_history_ms);
  EXPECT_TRUE(AddRecvStream(kSsrc3));
  EXPECT_EQ(kRtpHistoryMs, GetRecvStreamConfig(kSsrc3).rtp.nack.rtp_history_ms);
}

// Test that without useinbandfec, Opus FEC is off.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecNoOpusFec) {
  EXPECT_TRUE(SetupSendStream());
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kOpusCodec);
  SetSendParameters(parameters);
  EXPECT_FALSE(GetCodecFec(kSsrc1));
}

// Test that with useinbandfec=0, Opus FEC is off.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecOpusDisableFec) {
  EXPECT_TRUE(SetupSendStream());
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kOpusCodec);
  parameters.codecs[0].bitrate = 0;
  parameters.codecs[0].params["useinbandfec"] = "0";
  SetSendParameters(parameters);
  CheckSendCodec(kSsrc1, "opus", 1, 32000);
}

// Test that with useinbandfec=1, Opus FEC is on.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecOpusEnableFec) {
  EXPECT_TRUE(SetupSendStream());
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kOpusCodec);
  parameters.codecs[0].bitrate = 0;
  parameters.codecs[0].params["useinbandfec"] = "1";
  SetSendParameters(parameters);
  EXPECT_TRUE(GetCodecFec(kSsrc1));
  CheckSendCodec(kSsrc1, "opus", 1, 32000);
}

// Test that with useinbandfec=1, stereo=1, Opus FEC is on.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecOpusEnableFecStereo) {
  EXPECT_TRUE(SetupSendStream());
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kOpusCodec);
  parameters.codecs[0].bitrate = 0;
  parameters.codecs[0].params["stereo"] = "1";
  parameters.codecs[0].params["useinbandfec"] = "1";
  SetSendParameters(parameters);
  EXPECT_TRUE(GetCodecFec(kSsrc1));
  CheckSendCodec(kSsrc1, "opus", 2, 64000);
}

// Test that with non-Opus, codec FEC is off.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecIsacNoFec) {
  EXPECT_TRUE(SetupSendStream());
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kIsacCodec);
  SetSendParameters(parameters);
  EXPECT_FALSE(GetCodecFec(kSsrc1));
}

// Test the with non-Opus, even if useinbandfec=1, FEC is off.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecIsacWithParamNoFec) {
  EXPECT_TRUE(SetupSendStream());
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kIsacCodec);
  parameters.codecs[0].params["useinbandfec"] = "1";
  SetSendParameters(parameters);
  EXPECT_FALSE(GetCodecFec(kSsrc1));
}

// Test that Opus FEC status can be changed.
TEST_F(WebRtcVoiceEngineTestFake, ChangeOpusFecStatus) {
  EXPECT_TRUE(SetupSendStream());
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kOpusCodec);
  SetSendParameters(parameters);
  EXPECT_FALSE(GetCodecFec(kSsrc1));

  parameters.codecs[0].params["useinbandfec"] = "1";
  SetSendParameters(parameters);
  EXPECT_TRUE(GetCodecFec(kSsrc1));
}

TEST_F(WebRtcVoiceEngineTestFake, TransportCcCanBeEnabledAndDisabled) {
  EXPECT_TRUE(SetupChannel());
  cricket::AudioSendParameters send_parameters;
  send_parameters.codecs.push_back(kOpusCodec);
  EXPECT_TRUE(send_parameters.codecs[0].feedback_params.params().empty());
  SetSendParameters(send_parameters);

  cricket::AudioRecvParameters recv_parameters;
  recv_parameters.codecs.push_back(kIsacCodec);
  EXPECT_TRUE(channel_->SetRecvParameters(recv_parameters));
  EXPECT_TRUE(AddRecvStream(kSsrc1));
  ASSERT_TRUE(call_.GetAudioReceiveStream(kSsrc1) != nullptr);
  EXPECT_FALSE(
      call_.GetAudioReceiveStream(kSsrc1)->GetConfig().rtp.transport_cc);

  send_parameters.codecs = engine_->send_codecs();
  SetSendParameters(send_parameters);
  ASSERT_TRUE(call_.GetAudioReceiveStream(kSsrc1) != nullptr);
  EXPECT_TRUE(
      call_.GetAudioReceiveStream(kSsrc1)->GetConfig().rtp.transport_cc);
}

// Test maxplaybackrate <= 8000 triggers Opus narrow band mode.
TEST_F(WebRtcVoiceEngineTestFake, SetOpusMaxPlaybackRateNb) {
  EXPECT_TRUE(SetupSendStream());
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kOpusCodec);
  parameters.codecs[0].bitrate = 0;
  parameters.codecs[0].SetParam(cricket::kCodecParamMaxPlaybackRate, 8000);
  SetSendParameters(parameters);
  EXPECT_EQ(8000, GetOpusMaxPlaybackRate(kSsrc1));
  EXPECT_EQ(12000, GetCodecBitrate(kSsrc1));

  parameters.codecs[0].SetParam(cricket::kCodecParamStereo, "1");
  SetSendParameters(parameters);
  EXPECT_EQ(24000, GetCodecBitrate(kSsrc1));
}

// Test 8000 < maxplaybackrate <= 12000 triggers Opus medium band mode.
TEST_F(WebRtcVoiceEngineTestFake, SetOpusMaxPlaybackRateMb) {
  EXPECT_TRUE(SetupSendStream());
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kOpusCodec);
  parameters.codecs[0].bitrate = 0;
  parameters.codecs[0].SetParam(cricket::kCodecParamMaxPlaybackRate, 8001);
  SetSendParameters(parameters);
  EXPECT_EQ(8001, GetOpusMaxPlaybackRate(kSsrc1));
  EXPECT_EQ(20000, GetCodecBitrate(kSsrc1));

  parameters.codecs[0].SetParam(cricket::kCodecParamStereo, "1");
  SetSendParameters(parameters);
  EXPECT_EQ(40000, GetCodecBitrate(kSsrc1));
}

// Test 12000 < maxplaybackrate <= 16000 triggers Opus wide band mode.
TEST_F(WebRtcVoiceEngineTestFake, SetOpusMaxPlaybackRateWb) {
  EXPECT_TRUE(SetupSendStream());
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kOpusCodec);
  parameters.codecs[0].bitrate = 0;
  parameters.codecs[0].SetParam(cricket::kCodecParamMaxPlaybackRate, 12001);
  SetSendParameters(parameters);
  EXPECT_EQ(12001, GetOpusMaxPlaybackRate(kSsrc1));
  EXPECT_EQ(20000, GetCodecBitrate(kSsrc1));

  parameters.codecs[0].SetParam(cricket::kCodecParamStereo, "1");
  SetSendParameters(parameters);
  EXPECT_EQ(40000, GetCodecBitrate(kSsrc1));
}

// Test 16000 < maxplaybackrate <= 24000 triggers Opus super wide band mode.
TEST_F(WebRtcVoiceEngineTestFake, SetOpusMaxPlaybackRateSwb) {
  EXPECT_TRUE(SetupSendStream());
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kOpusCodec);
  parameters.codecs[0].bitrate = 0;
  parameters.codecs[0].SetParam(cricket::kCodecParamMaxPlaybackRate, 16001);
  SetSendParameters(parameters);
  EXPECT_EQ(16001, GetOpusMaxPlaybackRate(kSsrc1));
  EXPECT_EQ(32000, GetCodecBitrate(kSsrc1));

  parameters.codecs[0].SetParam(cricket::kCodecParamStereo, "1");
  SetSendParameters(parameters);
  EXPECT_EQ(64000, GetCodecBitrate(kSsrc1));
}

// Test 24000 < maxplaybackrate triggers Opus full band mode.
TEST_F(WebRtcVoiceEngineTestFake, SetOpusMaxPlaybackRateFb) {
  EXPECT_TRUE(SetupSendStream());
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kOpusCodec);
  parameters.codecs[0].bitrate = 0;
  parameters.codecs[0].SetParam(cricket::kCodecParamMaxPlaybackRate, 24001);
  SetSendParameters(parameters);
  EXPECT_EQ(24001, GetOpusMaxPlaybackRate(kSsrc1));
  EXPECT_EQ(32000, GetCodecBitrate(kSsrc1));

  parameters.codecs[0].SetParam(cricket::kCodecParamStereo, "1");
  SetSendParameters(parameters);
  EXPECT_EQ(64000, GetCodecBitrate(kSsrc1));
}

// Test Opus that without maxplaybackrate, default playback rate is used.
TEST_F(WebRtcVoiceEngineTestFake, DefaultOpusMaxPlaybackRate) {
  EXPECT_TRUE(SetupSendStream());
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kOpusCodec);
  SetSendParameters(parameters);
  EXPECT_EQ(48000, GetOpusMaxPlaybackRate(kSsrc1));
}

// Test the with non-Opus, maxplaybackrate has no effect.
TEST_F(WebRtcVoiceEngineTestFake, SetNonOpusMaxPlaybackRate) {
  EXPECT_TRUE(SetupSendStream());
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kIsacCodec);
  parameters.codecs[0].SetParam(cricket::kCodecParamMaxPlaybackRate, 32000);
  SetSendParameters(parameters);
  EXPECT_EQ(0, GetOpusMaxPlaybackRate(kSsrc1));
}

// Test maxplaybackrate can be set on two streams.
TEST_F(WebRtcVoiceEngineTestFake, SetOpusMaxPlaybackRateOnTwoStreams) {
  EXPECT_TRUE(SetupSendStream());
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kOpusCodec);
  SetSendParameters(parameters);
  EXPECT_EQ(48000, GetOpusMaxPlaybackRate(kSsrc1));

  parameters.codecs[0].SetParam(cricket::kCodecParamMaxPlaybackRate, 8000);
  SetSendParameters(parameters);
  EXPECT_EQ(8000, GetOpusMaxPlaybackRate(kSsrc1));

  channel_->AddSendStream(cricket::StreamParams::CreateLegacy(kSsrc2));
  EXPECT_EQ(8000, GetOpusMaxPlaybackRate(kSsrc2));
}

// Test that with usedtx=0, Opus DTX is off.
TEST_F(WebRtcVoiceEngineTestFake, DisableOpusDtxOnOpus) {
  EXPECT_TRUE(SetupSendStream());
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kOpusCodec);
  parameters.codecs[0].params["usedtx"] = "0";
  SetSendParameters(parameters);
  EXPECT_FALSE(GetOpusDtx(kSsrc1));
}

// Test that with usedtx=1, Opus DTX is on.
TEST_F(WebRtcVoiceEngineTestFake, EnableOpusDtxOnOpus) {
  EXPECT_TRUE(SetupSendStream());
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kOpusCodec);
  parameters.codecs[0].params["usedtx"] = "1";
  SetSendParameters(parameters);
  EXPECT_TRUE(GetOpusDtx(kSsrc1));
}

// Test that usedtx=1 works with stereo Opus.
TEST_F(WebRtcVoiceEngineTestFake, EnableOpusDtxOnOpusStereo) {
  EXPECT_TRUE(SetupSendStream());
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kOpusCodec);
  parameters.codecs[0].params["usedtx"] = "1";
  parameters.codecs[0].params["stereo"] = "1";
  SetSendParameters(parameters);
  EXPECT_TRUE(GetOpusDtx(kSsrc1));
}

// Test that usedtx=1 does not work with non Opus.
TEST_F(WebRtcVoiceEngineTestFake, CannotEnableOpusDtxOnNonOpus) {
  EXPECT_TRUE(SetupSendStream());
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kIsacCodec);
  parameters.codecs[0].params["usedtx"] = "1";
  SetSendParameters(parameters);
  EXPECT_FALSE(GetOpusDtx(kSsrc1));
}

// Test that we can switch back and forth between Opus and ISAC with CN.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecsIsacOpusSwitching) {
  EXPECT_TRUE(SetupSendStream());

  cricket::AudioSendParameters opus_parameters;
  opus_parameters.codecs.push_back(kOpusCodec);
  SetSendParameters(opus_parameters);
  {
    const auto& gcodec = GetSendStreamConfig(kSsrc1).send_codec_spec.codec_inst;
    EXPECT_EQ(111, gcodec.pltype);
    EXPECT_STREQ("opus", gcodec.plname);
  }

  cricket::AudioSendParameters isac_parameters;
  isac_parameters.codecs.push_back(kIsacCodec);
  isac_parameters.codecs.push_back(kCn16000Codec);
  isac_parameters.codecs.push_back(kOpusCodec);
  SetSendParameters(isac_parameters);
  {
    const auto& gcodec = GetSendStreamConfig(kSsrc1).send_codec_spec.codec_inst;
    EXPECT_EQ(103, gcodec.pltype);
    EXPECT_STREQ("ISAC", gcodec.plname);
  }

  SetSendParameters(opus_parameters);
  {
    const auto& gcodec = GetSendStreamConfig(kSsrc1).send_codec_spec.codec_inst;
    EXPECT_EQ(111, gcodec.pltype);
    EXPECT_STREQ("opus", gcodec.plname);
  }
}

// Test that we handle various ways of specifying bitrate.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecsBitrate) {
  EXPECT_TRUE(SetupSendStream());
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kIsacCodec);  // bitrate == 32000
  SetSendParameters(parameters);
  {
    const auto& gcodec = GetSendStreamConfig(kSsrc1).send_codec_spec.codec_inst;
    EXPECT_EQ(103, gcodec.pltype);
    EXPECT_STREQ("ISAC", gcodec.plname);
    EXPECT_EQ(32000, gcodec.rate);
  }

  parameters.codecs[0].bitrate = 0;         // bitrate == default
  SetSendParameters(parameters);
  {
    const auto& gcodec = GetSendStreamConfig(kSsrc1).send_codec_spec.codec_inst;
    EXPECT_EQ(103, gcodec.pltype);
    EXPECT_STREQ("ISAC", gcodec.plname);
    EXPECT_EQ(-1, gcodec.rate);
  }
  parameters.codecs[0].bitrate = 28000;     // bitrate == 28000
  SetSendParameters(parameters);
  {
    const auto& gcodec = GetSendStreamConfig(kSsrc1).send_codec_spec.codec_inst;
    EXPECT_EQ(103, gcodec.pltype);
    EXPECT_STREQ("ISAC", gcodec.plname);
    EXPECT_EQ(28000, gcodec.rate);
  }

  parameters.codecs[0] = kPcmuCodec;        // bitrate == 64000
  SetSendParameters(parameters);
  {
    const auto& gcodec = GetSendStreamConfig(kSsrc1).send_codec_spec.codec_inst;
    EXPECT_EQ(0, gcodec.pltype);
    EXPECT_STREQ("PCMU", gcodec.plname);
    EXPECT_EQ(64000, gcodec.rate);
  }

  parameters.codecs[0].bitrate = 0;         // bitrate == default
  SetSendParameters(parameters);
  {
    const auto& gcodec = GetSendStreamConfig(kSsrc1).send_codec_spec.codec_inst;
    EXPECT_EQ(0, gcodec.pltype);
    EXPECT_STREQ("PCMU", gcodec.plname);
    EXPECT_EQ(64000, gcodec.rate);
  }

  parameters.codecs[0] = kOpusCodec;
  parameters.codecs[0].bitrate = 0;         // bitrate == default
  SetSendParameters(parameters);
  {
    const auto& gcodec = GetSendStreamConfig(kSsrc1).send_codec_spec.codec_inst;
    EXPECT_EQ(111, gcodec.pltype);
    EXPECT_STREQ("opus", gcodec.plname);
    EXPECT_EQ(32000, gcodec.rate);
  }
}

// Test that we could set packet size specified in kCodecParamPTime.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecsPTimeAsPacketSize) {
  EXPECT_TRUE(SetupSendStream());
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kOpusCodec);
  parameters.codecs[0].SetParam(cricket::kCodecParamPTime, 40); // Within range.
  SetSendParameters(parameters);
  EXPECT_EQ(1920, GetCodecPacSize(kSsrc1));  // Opus gets 40ms.

  parameters.codecs[0].SetParam(cricket::kCodecParamPTime, 5); // Below range.
  SetSendParameters(parameters);
  EXPECT_EQ(480, GetCodecPacSize(kSsrc1));  // Opus gets 10ms.

  parameters.codecs[0].SetParam(cricket::kCodecParamPTime, 80); // Beyond range.
  SetSendParameters(parameters);
  EXPECT_EQ(2880, GetCodecPacSize(kSsrc1));  // Opus gets 60ms.

  parameters.codecs[0] = kIsacCodec;  // Also try Isac, with unsupported size.
  parameters.codecs[0].SetParam(cricket::kCodecParamPTime, 40); // Within range.
  SetSendParameters(parameters);
  EXPECT_EQ(480, GetCodecPacSize(
                     kSsrc1));  // Isac gets 30ms as the next smallest value.

  parameters.codecs[0] = kG722CodecSdp;  // Try G722 @8kHz as negotiated in SDP.
  parameters.codecs[0].SetParam(cricket::kCodecParamPTime, 40);
  SetSendParameters(parameters);
  EXPECT_EQ(640, GetCodecPacSize(
                     kSsrc1));  // G722 gets 40ms @16kHz as defined in VoE.
}

// Test that we fail if no codecs are specified.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecsNoCodecs) {
  EXPECT_TRUE(SetupSendStream());
  cricket::AudioSendParameters parameters;
  EXPECT_FALSE(channel_->SetSendParameters(parameters));
}

// Test that we can set send codecs even with telephone-event codec as the first
// one on the list.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecsDTMFOnTop) {
  EXPECT_TRUE(SetupSendStream());
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kTelephoneEventCodec);
  parameters.codecs.push_back(kIsacCodec);
  parameters.codecs.push_back(kPcmuCodec);
  parameters.codecs[0].id = 98;  // DTMF
  parameters.codecs[1].id = 96;
  SetSendParameters(parameters);
  const auto& gcodec = GetSendStreamConfig(kSsrc1).send_codec_spec.codec_inst;
  EXPECT_EQ(96, gcodec.pltype);
  EXPECT_STREQ("ISAC", gcodec.plname);
  EXPECT_TRUE(channel_->CanInsertDtmf());
}

// Test that payload type range is limited for telephone-event codec.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecsDTMFPayloadTypeOutOfRange) {
  EXPECT_TRUE(SetupSendStream());
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kTelephoneEventCodec);
  parameters.codecs.push_back(kIsacCodec);
  parameters.codecs[0].id = 0;  // DTMF
  parameters.codecs[1].id = 96;
  SetSendParameters(parameters);
  EXPECT_TRUE(channel_->CanInsertDtmf());
  parameters.codecs[0].id = 128;  // DTMF
  EXPECT_FALSE(channel_->SetSendParameters(parameters));
  EXPECT_FALSE(channel_->CanInsertDtmf());
  parameters.codecs[0].id = 127;
  SetSendParameters(parameters);
  EXPECT_TRUE(channel_->CanInsertDtmf());
  parameters.codecs[0].id = -1;  // DTMF
  EXPECT_FALSE(channel_->SetSendParameters(parameters));
  EXPECT_FALSE(channel_->CanInsertDtmf());
}

// Test that we can set send codecs even with CN codec as the first
// one on the list.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecsCNOnTop) {
  EXPECT_TRUE(SetupSendStream());
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kCn16000Codec);
  parameters.codecs.push_back(kIsacCodec);
  parameters.codecs.push_back(kPcmuCodec);
  parameters.codecs[0].id = 98;  // wideband CN
  parameters.codecs[1].id = 96;
  SetSendParameters(parameters);
  const auto& send_codec_spec = GetSendStreamConfig(kSsrc1).send_codec_spec;
  EXPECT_EQ(96, send_codec_spec.codec_inst.pltype);
  EXPECT_STREQ("ISAC", send_codec_spec.codec_inst.plname);
  EXPECT_EQ(98, send_codec_spec.cng_payload_type);
  EXPECT_EQ(webrtc::kFreq16000Hz, send_codec_spec.cng_plfreq);
}

// Test that we set VAD and DTMF types correctly as caller.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecsCNandDTMFAsCaller) {
  EXPECT_TRUE(SetupSendStream());
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kIsacCodec);
  parameters.codecs.push_back(kPcmuCodec);
  // TODO(juberti): cn 32000
  parameters.codecs.push_back(kCn16000Codec);
  parameters.codecs.push_back(kCn8000Codec);
  parameters.codecs.push_back(kTelephoneEventCodec);
  parameters.codecs[0].id = 96;
  parameters.codecs[2].id = 97;  // wideband CN
  parameters.codecs[4].id = 98;  // DTMF
  SetSendParameters(parameters);
  const auto& send_codec_spec = GetSendStreamConfig(kSsrc1).send_codec_spec;
  EXPECT_EQ(96, send_codec_spec.codec_inst.pltype);
  EXPECT_STREQ("ISAC", send_codec_spec.codec_inst.plname);
  EXPECT_EQ(1, send_codec_spec.codec_inst.channels);
  EXPECT_EQ(send_codec_spec.codec_inst.plfreq, send_codec_spec.cng_plfreq);
  EXPECT_EQ(97, send_codec_spec.cng_payload_type);
  EXPECT_EQ(webrtc::kFreq16000Hz, send_codec_spec.cng_plfreq);
  EXPECT_TRUE(channel_->CanInsertDtmf());
}

// Test that we set VAD and DTMF types correctly as callee.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecsCNandDTMFAsCallee) {
  EXPECT_TRUE(SetupChannel());
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kIsacCodec);
  parameters.codecs.push_back(kPcmuCodec);
  // TODO(juberti): cn 32000
  parameters.codecs.push_back(kCn16000Codec);
  parameters.codecs.push_back(kCn8000Codec);
  parameters.codecs.push_back(kTelephoneEventCodec);
  parameters.codecs[0].id = 96;
  parameters.codecs[2].id = 97;  // wideband CN
  parameters.codecs[4].id = 98;  // DTMF
  SetSendParameters(parameters);
  EXPECT_TRUE(channel_->AddSendStream(
      cricket::StreamParams::CreateLegacy(kSsrc1)));

  const auto& send_codec_spec = GetSendStreamConfig(kSsrc1).send_codec_spec;
  EXPECT_EQ(96, send_codec_spec.codec_inst.pltype);
  EXPECT_STREQ("ISAC", send_codec_spec.codec_inst.plname);
  EXPECT_EQ(1, send_codec_spec.codec_inst.channels);
  EXPECT_EQ(send_codec_spec.codec_inst.plfreq, send_codec_spec.cng_plfreq);
  EXPECT_EQ(97, send_codec_spec.cng_payload_type);
  EXPECT_EQ(webrtc::kFreq16000Hz, send_codec_spec.cng_plfreq);
  EXPECT_TRUE(channel_->CanInsertDtmf());
}

// Test that we only apply VAD if we have a CN codec that matches the
// send codec clockrate.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecsCNNoMatch) {
  EXPECT_TRUE(SetupSendStream());
  cricket::AudioSendParameters parameters;
  // Set ISAC(16K) and CN(16K). VAD should be activated.
  parameters.codecs.push_back(kIsacCodec);
  parameters.codecs.push_back(kCn16000Codec);
  parameters.codecs[1].id = 97;
  SetSendParameters(parameters);
  {
    const auto& send_codec_spec = GetSendStreamConfig(kSsrc1).send_codec_spec;
    EXPECT_STREQ("ISAC", send_codec_spec.codec_inst.plname);
    EXPECT_EQ(1, send_codec_spec.codec_inst.channels);
    EXPECT_EQ(send_codec_spec.codec_inst.plfreq, send_codec_spec.cng_plfreq);
    EXPECT_EQ(97, send_codec_spec.cng_payload_type);
    EXPECT_EQ(webrtc::kFreq16000Hz, send_codec_spec.cng_plfreq);
  }
  // Set PCMU(8K) and CN(16K). VAD should not be activated.
  parameters.codecs[0] = kPcmuCodec;
  SetSendParameters(parameters);
  {
    const auto& send_codec_spec = GetSendStreamConfig(kSsrc1).send_codec_spec;
    EXPECT_STREQ("PCMU", send_codec_spec.codec_inst.plname);
    EXPECT_NE(send_codec_spec.codec_inst.plfreq, send_codec_spec.cng_plfreq);
  }
  // Set PCMU(8K) and CN(8K). VAD should be activated.
  parameters.codecs[1] = kCn8000Codec;
  SetSendParameters(parameters);
  {
    const auto& send_codec_spec = GetSendStreamConfig(kSsrc1).send_codec_spec;
    EXPECT_STREQ("PCMU", send_codec_spec.codec_inst.plname);
    EXPECT_EQ(1, send_codec_spec.codec_inst.channels);
    EXPECT_EQ(send_codec_spec.codec_inst.plfreq, send_codec_spec.cng_plfreq);
    EXPECT_EQ(13, send_codec_spec.cng_payload_type);
    EXPECT_EQ(webrtc::kFreq8000Hz, send_codec_spec.cng_plfreq);
  }
  // Set ISAC(16K) and CN(8K). VAD should not be activated.
  parameters.codecs[0] = kIsacCodec;
  SetSendParameters(parameters);
  {
    const auto& send_codec_spec = GetSendStreamConfig(kSsrc1).send_codec_spec;
    EXPECT_STREQ("ISAC", send_codec_spec.codec_inst.plname);
    EXPECT_NE(send_codec_spec.codec_inst.plfreq, send_codec_spec.cng_plfreq);
  }
}

// Test that we perform case-insensitive matching of codec names.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecsCaseInsensitive) {
  EXPECT_TRUE(SetupSendStream());
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kIsacCodec);
  parameters.codecs.push_back(kPcmuCodec);
  parameters.codecs.push_back(kCn16000Codec);
  parameters.codecs.push_back(kCn8000Codec);
  parameters.codecs.push_back(kTelephoneEventCodec);
  parameters.codecs[0].name = "iSaC";
  parameters.codecs[0].id = 96;
  parameters.codecs[2].id = 97;  // wideband CN
  parameters.codecs[4].id = 98;  // DTMF
  SetSendParameters(parameters);
  const auto& send_codec_spec = GetSendStreamConfig(kSsrc1).send_codec_spec;
  EXPECT_EQ(96, send_codec_spec.codec_inst.pltype);
  EXPECT_STREQ("ISAC", send_codec_spec.codec_inst.plname);
  EXPECT_EQ(1, send_codec_spec.codec_inst.channels);
  EXPECT_EQ(send_codec_spec.codec_inst.plfreq, send_codec_spec.cng_plfreq);
  EXPECT_EQ(97, send_codec_spec.cng_payload_type);
  EXPECT_EQ(webrtc::kFreq16000Hz, send_codec_spec.cng_plfreq);
  EXPECT_TRUE(channel_->CanInsertDtmf());
}

class WebRtcVoiceEngineWithSendSideBweTest : public WebRtcVoiceEngineTestFake {
 public:
  WebRtcVoiceEngineWithSendSideBweTest()
      : WebRtcVoiceEngineTestFake("WebRTC-Audio-SendSideBwe/Enabled/") {}
};

TEST_F(WebRtcVoiceEngineWithSendSideBweTest,
       SupportsTransportSequenceNumberHeaderExtension) {
  cricket::RtpCapabilities capabilities = engine_->GetCapabilities();
  ASSERT_FALSE(capabilities.header_extensions.empty());
  for (const webrtc::RtpExtension& extension : capabilities.header_extensions) {
    if (extension.uri == webrtc::RtpExtension::kTransportSequenceNumberUri) {
      EXPECT_EQ(webrtc::RtpExtension::kTransportSequenceNumberDefaultId,
                extension.id);
      return;
    }
  }
  FAIL() << "Transport sequence number extension not in header-extension list.";
}

// Test support for audio level header extension.
TEST_F(WebRtcVoiceEngineTestFake, SendAudioLevelHeaderExtensions) {
  TestSetSendRtpHeaderExtensions(webrtc::RtpExtension::kAudioLevelUri);
}
TEST_F(WebRtcVoiceEngineTestFake, RecvAudioLevelHeaderExtensions) {
  TestSetRecvRtpHeaderExtensions(webrtc::RtpExtension::kAudioLevelUri);
}

// Test support for absolute send time header extension.
TEST_F(WebRtcVoiceEngineTestFake, SendAbsoluteSendTimeHeaderExtensions) {
  TestSetSendRtpHeaderExtensions(webrtc::RtpExtension::kAbsSendTimeUri);
}
TEST_F(WebRtcVoiceEngineTestFake, RecvAbsoluteSendTimeHeaderExtensions) {
  TestSetRecvRtpHeaderExtensions(webrtc::RtpExtension::kAbsSendTimeUri);
}

// Test that we can create a channel and start sending on it.
TEST_F(WebRtcVoiceEngineTestFake, Send) {
  EXPECT_TRUE(SetupSendStream());
  SetSendParameters(send_parameters_);
  SetSend(true);
  EXPECT_TRUE(GetSendStream(kSsrc1).IsSending());
  SetSend(false);
  EXPECT_FALSE(GetSendStream(kSsrc1).IsSending());
}

// Test that a channel will send if and only if it has a source and is enabled
// for sending.
TEST_F(WebRtcVoiceEngineTestFake, SendStateWithAndWithoutSource) {
  EXPECT_TRUE(SetupSendStream());
  SetSendParameters(send_parameters_);
  SetAudioSend(kSsrc1, true, nullptr);
  SetSend(true);
  EXPECT_FALSE(GetSendStream(kSsrc1).IsSending());
  SetAudioSend(kSsrc1, true, &fake_source_);
  EXPECT_TRUE(GetSendStream(kSsrc1).IsSending());
  SetAudioSend(kSsrc1, true, nullptr);
  EXPECT_FALSE(GetSendStream(kSsrc1).IsSending());
}

// Test that a channel is muted/unmuted.
TEST_F(WebRtcVoiceEngineTestFake, SendStateMuteUnmute) {
  EXPECT_TRUE(SetupSendStream());
  SetSendParameters(send_parameters_);
  EXPECT_FALSE(GetSendStream(kSsrc1).muted());
  SetAudioSend(kSsrc1, true, nullptr);
  EXPECT_FALSE(GetSendStream(kSsrc1).muted());
  SetAudioSend(kSsrc1, false, nullptr);
  EXPECT_TRUE(GetSendStream(kSsrc1).muted());
}

// Test that SetSendParameters() does not alter a stream's send state.
TEST_F(WebRtcVoiceEngineTestFake, SendStateWhenStreamsAreRecreated) {
  EXPECT_TRUE(SetupSendStream());
  EXPECT_FALSE(GetSendStream(kSsrc1).IsSending());

  // Turn on sending.
  SetSend(true);
  EXPECT_TRUE(GetSendStream(kSsrc1).IsSending());

  // Changing RTP header extensions will recreate the AudioSendStream.
  send_parameters_.extensions.push_back(
      webrtc::RtpExtension(webrtc::RtpExtension::kAudioLevelUri, 12));
  SetSendParameters(send_parameters_);
  EXPECT_TRUE(GetSendStream(kSsrc1).IsSending());

  // Turn off sending.
  SetSend(false);
  EXPECT_FALSE(GetSendStream(kSsrc1).IsSending());

  // Changing RTP header extensions will recreate the AudioSendStream.
  send_parameters_.extensions.clear();
  SetSendParameters(send_parameters_);
  EXPECT_FALSE(GetSendStream(kSsrc1).IsSending());
}

// Test that we can create a channel and start playing out on it.
TEST_F(WebRtcVoiceEngineTestFake, Playout) {
  EXPECT_TRUE(SetupRecvStream());
  EXPECT_TRUE(channel_->SetRecvParameters(recv_parameters_));
  channel_->SetPlayout(true);
  EXPECT_TRUE(GetRecvStream(kSsrc1).started());
  channel_->SetPlayout(false);
  EXPECT_FALSE(GetRecvStream(kSsrc1).started());
}

// Test that we can add and remove send streams.
TEST_F(WebRtcVoiceEngineTestFake, CreateAndDeleteMultipleSendStreams) {
  SetupForMultiSendStream();

  // Set the global state for sending.
  SetSend(true);

  for (uint32_t ssrc : kSsrcs4) {
    EXPECT_TRUE(channel_->AddSendStream(
        cricket::StreamParams::CreateLegacy(ssrc)));
    SetAudioSend(ssrc, true, &fake_source_);
    // Verify that we are in a sending state for all the created streams.
    EXPECT_TRUE(GetSendStream(ssrc).IsSending());
  }
  EXPECT_EQ(arraysize(kSsrcs4), call_.GetAudioSendStreams().size());

  // Delete the send streams.
  for (uint32_t ssrc : kSsrcs4) {
    EXPECT_TRUE(channel_->RemoveSendStream(ssrc));
    EXPECT_FALSE(call_.GetAudioSendStream(ssrc));
    EXPECT_FALSE(channel_->RemoveSendStream(ssrc));
  }
  EXPECT_EQ(0u, call_.GetAudioSendStreams().size());
}

// Test SetSendCodecs correctly configure the codecs in all send streams.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecsWithMultipleSendStreams) {
  SetupForMultiSendStream();

  // Create send streams.
  for (uint32_t ssrc : kSsrcs4) {
    EXPECT_TRUE(channel_->AddSendStream(
        cricket::StreamParams::CreateLegacy(ssrc)));
  }

  cricket::AudioSendParameters parameters;
  // Set ISAC(16K) and CN(16K). VAD should be activated.
  parameters.codecs.push_back(kIsacCodec);
  parameters.codecs.push_back(kCn16000Codec);
  parameters.codecs[1].id = 97;
  SetSendParameters(parameters);

  // Verify ISAC and VAD are corrected configured on all send channels.
  for (uint32_t ssrc : kSsrcs4) {
    ASSERT_TRUE(call_.GetAudioSendStream(ssrc) != nullptr);
    const auto& send_codec_spec =
        call_.GetAudioSendStream(ssrc)->GetConfig().send_codec_spec;
    EXPECT_STREQ("ISAC", send_codec_spec.codec_inst.plname);
    EXPECT_EQ(send_codec_spec.codec_inst.plfreq, send_codec_spec.cng_plfreq);
    EXPECT_EQ(1, send_codec_spec.codec_inst.channels);
    EXPECT_EQ(97, send_codec_spec.cng_payload_type);
    EXPECT_EQ(webrtc::kFreq16000Hz, send_codec_spec.cng_plfreq);
  }

  // Change to PCMU(8K) and CN(16K).
  parameters.codecs[0] = kPcmuCodec;
  SetSendParameters(parameters);
  for (uint32_t ssrc : kSsrcs4) {
    ASSERT_TRUE(call_.GetAudioSendStream(ssrc) != nullptr);
    const auto& send_codec_spec =
        call_.GetAudioSendStream(ssrc)->GetConfig().send_codec_spec;
    EXPECT_STREQ("PCMU", send_codec_spec.codec_inst.plname);
    EXPECT_NE(send_codec_spec.codec_inst.plfreq, send_codec_spec.cng_plfreq);
    EXPECT_EQ(97, send_codec_spec.cng_payload_type);
    EXPECT_EQ(webrtc::kFreq16000Hz, send_codec_spec.cng_plfreq);
  }
}

// Test we can SetSend on all send streams correctly.
TEST_F(WebRtcVoiceEngineTestFake, SetSendWithMultipleSendStreams) {
  SetupForMultiSendStream();

  // Create the send channels and they should be a "not sending" date.
  for (uint32_t ssrc : kSsrcs4) {
    EXPECT_TRUE(channel_->AddSendStream(
        cricket::StreamParams::CreateLegacy(ssrc)));
    SetAudioSend(ssrc, true, &fake_source_);
    EXPECT_FALSE(GetSendStream(ssrc).IsSending());
  }

  // Set the global state for starting sending.
  SetSend(true);
  for (uint32_t ssrc : kSsrcs4) {
    // Verify that we are in a sending state for all the send streams.
    EXPECT_TRUE(GetSendStream(ssrc).IsSending());
  }

  // Set the global state for stopping sending.
  SetSend(false);
  for (uint32_t ssrc : kSsrcs4) {
    // Verify that we are in a stop state for all the send streams.
    EXPECT_FALSE(GetSendStream(ssrc).IsSending());
  }
}

// Test we can set the correct statistics on all send streams.
TEST_F(WebRtcVoiceEngineTestFake, GetStatsWithMultipleSendStreams) {
  SetupForMultiSendStream();

  // Create send streams.
  for (uint32_t ssrc : kSsrcs4) {
    EXPECT_TRUE(channel_->AddSendStream(
        cricket::StreamParams::CreateLegacy(ssrc)));
  }

  // Create a receive stream to check that none of the send streams end up in
  // the receive stream stats.
  EXPECT_TRUE(AddRecvStream(kSsrc2));

  // We need send codec to be set to get all stats.
  SetSendParameters(send_parameters_);
  EXPECT_TRUE(channel_->SetRecvParameters(recv_parameters_));
  SetAudioSendStreamStats();

  // Check stats for the added streams.
  {
    cricket::VoiceMediaInfo info;
    EXPECT_EQ(true, channel_->GetStats(&info));

    // We have added 4 send streams. We should see empty stats for all.
    EXPECT_EQ(static_cast<size_t>(arraysize(kSsrcs4)), info.senders.size());
    for (const auto& sender : info.senders) {
      VerifyVoiceSenderInfo(sender, false);
    }

    // We have added one receive stream. We should see empty stats.
    EXPECT_EQ(info.receivers.size(), 1u);
    EXPECT_EQ(info.receivers[0].ssrc(), 0);
  }

  // Remove the kSsrc2 stream. No receiver stats.
  {
    cricket::VoiceMediaInfo info;
    EXPECT_TRUE(channel_->RemoveRecvStream(kSsrc2));
    EXPECT_EQ(true, channel_->GetStats(&info));
    EXPECT_EQ(static_cast<size_t>(arraysize(kSsrcs4)), info.senders.size());
    EXPECT_EQ(0u, info.receivers.size());
  }

  // Deliver a new packet - a default receive stream should be created and we
  // should see stats again.
  {
    cricket::VoiceMediaInfo info;
    DeliverPacket(kPcmuFrame, sizeof(kPcmuFrame));
    SetAudioReceiveStreamStats();
    EXPECT_EQ(true, channel_->GetStats(&info));
    EXPECT_EQ(static_cast<size_t>(arraysize(kSsrcs4)), info.senders.size());
    EXPECT_EQ(1u, info.receivers.size());
    VerifyVoiceReceiverInfo(info.receivers[0]);
  }
}

// Test that we can add and remove receive streams, and do proper send/playout.
// We can receive on multiple streams while sending one stream.
TEST_F(WebRtcVoiceEngineTestFake, PlayoutWithMultipleStreams) {
  EXPECT_TRUE(SetupSendStream());

  // Start playout without a receive stream.
  SetSendParameters(send_parameters_);
  channel_->SetPlayout(true);

  // Adding another stream should enable playout on the new stream only.
  EXPECT_TRUE(AddRecvStream(kSsrc2));
  SetSend(true);
  EXPECT_TRUE(GetSendStream(kSsrc1).IsSending());

  // Make sure only the new stream is played out.
  EXPECT_TRUE(GetRecvStream(kSsrc2).started());

  // Adding yet another stream should have stream 2 and 3 enabled for playout.
  EXPECT_TRUE(AddRecvStream(kSsrc3));
  EXPECT_TRUE(GetRecvStream(kSsrc2).started());
  EXPECT_TRUE(GetRecvStream(kSsrc3).started());

  // Stop sending.
  SetSend(false);
  EXPECT_FALSE(GetSendStream(kSsrc1).IsSending());

  // Stop playout.
  channel_->SetPlayout(false);
  EXPECT_FALSE(GetRecvStream(kSsrc2).started());
  EXPECT_FALSE(GetRecvStream(kSsrc3).started());

  // Restart playout and make sure recv streams are played out.
  channel_->SetPlayout(true);
  EXPECT_TRUE(GetRecvStream(kSsrc2).started());
  EXPECT_TRUE(GetRecvStream(kSsrc3).started());

  // Now remove the recv streams.
  EXPECT_TRUE(channel_->RemoveRecvStream(3));
  EXPECT_TRUE(channel_->RemoveRecvStream(2));
}

// Test that we can create a channel configured for Codian bridges,
// and start sending on it.
TEST_F(WebRtcVoiceEngineTestFake, CodianSend) {
  EXPECT_TRUE(SetupSendStream());
  cricket::AudioOptions options_adjust_agc;
  options_adjust_agc.adjust_agc_delta = rtc::Optional<int>(-10);
  webrtc::AgcConfig agc_config;
  EXPECT_EQ(0, voe_.GetAgcConfig(agc_config));
  EXPECT_EQ(0, agc_config.targetLeveldBOv);
  send_parameters_.options = options_adjust_agc;
  SetSendParameters(send_parameters_);
  SetSend(true);
  EXPECT_TRUE(GetSendStream(kSsrc1).IsSending());
  EXPECT_EQ(0, voe_.GetAgcConfig(agc_config));
  EXPECT_EQ(agc_config.targetLeveldBOv, 10);  // level was attenuated
  SetSend(false);
  EXPECT_FALSE(GetSendStream(kSsrc1).IsSending());
  EXPECT_EQ(0, voe_.GetAgcConfig(agc_config));
}

TEST_F(WebRtcVoiceEngineTestFake, TxAgcConfigViaOptions) {
  EXPECT_TRUE(SetupSendStream());
  EXPECT_CALL(adm_,
              BuiltInAGCIsAvailable()).Times(2).WillRepeatedly(Return(false));
  webrtc::AgcConfig agc_config;
  EXPECT_EQ(0, voe_.GetAgcConfig(agc_config));
  EXPECT_EQ(0, agc_config.targetLeveldBOv);
  send_parameters_.options.tx_agc_target_dbov = rtc::Optional<uint16_t>(3);
  send_parameters_.options.tx_agc_digital_compression_gain =
      rtc::Optional<uint16_t>(9);
  send_parameters_.options.tx_agc_limiter = rtc::Optional<bool>(true);
  send_parameters_.options.auto_gain_control = rtc::Optional<bool>(true);
  SetSendParameters(send_parameters_);
  EXPECT_EQ(0, voe_.GetAgcConfig(agc_config));
  EXPECT_EQ(3, agc_config.targetLeveldBOv);
  EXPECT_EQ(9, agc_config.digitalCompressionGaindB);
  EXPECT_TRUE(agc_config.limiterEnable);

  // Check interaction with adjust_agc_delta. Both should be respected, for
  // backwards compatibility.
  send_parameters_.options.adjust_agc_delta = rtc::Optional<int>(-10);
  SetSendParameters(send_parameters_);
  EXPECT_EQ(0, voe_.GetAgcConfig(agc_config));
  EXPECT_EQ(13, agc_config.targetLeveldBOv);
}

TEST_F(WebRtcVoiceEngineTestFake, SampleRatesViaOptions) {
  EXPECT_TRUE(SetupSendStream());
  EXPECT_CALL(adm_, SetRecordingSampleRate(48000)).WillOnce(Return(0));
  EXPECT_CALL(adm_, SetPlayoutSampleRate(44100)).WillOnce(Return(0));
  send_parameters_.options.recording_sample_rate =
      rtc::Optional<uint32_t>(48000);
  send_parameters_.options.playout_sample_rate = rtc::Optional<uint32_t>(44100);
  SetSendParameters(send_parameters_);
}

TEST_F(WebRtcVoiceEngineTestFake, SetAudioNetworkAdaptorViaOptions) {
  EXPECT_TRUE(SetupSendStream());
  send_parameters_.options.audio_network_adaptor = rtc::Optional<bool>(true);
  send_parameters_.options.audio_network_adaptor_config =
      rtc::Optional<std::string>("1234");
  SetSendParameters(send_parameters_);
  EXPECT_EQ(send_parameters_.options.audio_network_adaptor_config,
            GetAudioNetworkAdaptorConfig(kSsrc1));
}

TEST_F(WebRtcVoiceEngineTestFake, AudioSendResetAudioNetworkAdaptor) {
  EXPECT_TRUE(SetupSendStream());
  send_parameters_.options.audio_network_adaptor = rtc::Optional<bool>(true);
  send_parameters_.options.audio_network_adaptor_config =
      rtc::Optional<std::string>("1234");
  SetSendParameters(send_parameters_);
  EXPECT_EQ(send_parameters_.options.audio_network_adaptor_config,
            GetAudioNetworkAdaptorConfig(kSsrc1));
  const int initial_num = call_.GetNumCreatedSendStreams();
  cricket::AudioOptions options;
  options.audio_network_adaptor = rtc::Optional<bool>(false);
  SetAudioSend(kSsrc1, true, nullptr, &options);
  // AudioSendStream expected to be recreated.
  EXPECT_EQ(initial_num + 1, call_.GetNumCreatedSendStreams());
  EXPECT_EQ(rtc::Optional<std::string>(), GetAudioNetworkAdaptorConfig(kSsrc1));
}

TEST_F(WebRtcVoiceEngineTestFake, AudioNetworkAdaptorNotGetOverridden) {
  EXPECT_TRUE(SetupSendStream());
  send_parameters_.options.audio_network_adaptor = rtc::Optional<bool>(true);
  send_parameters_.options.audio_network_adaptor_config =
      rtc::Optional<std::string>("1234");
  SetSendParameters(send_parameters_);
  EXPECT_EQ(send_parameters_.options.audio_network_adaptor_config,
            GetAudioNetworkAdaptorConfig(kSsrc1));
  const int initial_num = call_.GetNumCreatedSendStreams();
  cricket::AudioOptions options;
  options.audio_network_adaptor = rtc::Optional<bool>();
  // Unvalued |options.audio_network_adaptor|.should not reset audio network
  // adaptor.
  SetAudioSend(kSsrc1, true, nullptr, &options);
  // AudioSendStream not expected to be recreated.
  EXPECT_EQ(initial_num, call_.GetNumCreatedSendStreams());
  EXPECT_EQ(send_parameters_.options.audio_network_adaptor_config,
            GetAudioNetworkAdaptorConfig(kSsrc1));
}

// Test that we can set the outgoing SSRC properly.
// SSRC is set in SetupSendStream() by calling AddSendStream.
TEST_F(WebRtcVoiceEngineTestFake, SetSendSsrc) {
  EXPECT_TRUE(SetupSendStream());
  EXPECT_TRUE(call_.GetAudioSendStream(kSsrc1));
}

TEST_F(WebRtcVoiceEngineTestFake, GetStats) {
  // Setup. We need send codec to be set to get all stats.
  EXPECT_TRUE(SetupSendStream());
  // SetupSendStream adds a send stream with kSsrc1, so the receive
  // stream has to use a different SSRC.
  EXPECT_TRUE(AddRecvStream(kSsrc2));
  SetSendParameters(send_parameters_);
  EXPECT_TRUE(channel_->SetRecvParameters(recv_parameters_));
  SetAudioSendStreamStats();

  // Check stats for the added streams.
  {
    cricket::VoiceMediaInfo info;
    EXPECT_EQ(true, channel_->GetStats(&info));

    // We have added one send stream. We should see the stats we've set.
    EXPECT_EQ(1u, info.senders.size());
    VerifyVoiceSenderInfo(info.senders[0], false);
    // We have added one receive stream. We should see empty stats.
    EXPECT_EQ(info.receivers.size(), 1u);
    EXPECT_EQ(info.receivers[0].ssrc(), 0);
  }

  // Start sending - this affects some reported stats.
  {
    cricket::VoiceMediaInfo info;
    SetSend(true);
    EXPECT_EQ(true, channel_->GetStats(&info));
    VerifyVoiceSenderInfo(info.senders[0], true);
  }

  // Remove the kSsrc2 stream. No receiver stats.
  {
    cricket::VoiceMediaInfo info;
    EXPECT_TRUE(channel_->RemoveRecvStream(kSsrc2));
    EXPECT_EQ(true, channel_->GetStats(&info));
    EXPECT_EQ(1u, info.senders.size());
    EXPECT_EQ(0u, info.receivers.size());
  }

  // Deliver a new packet - a default receive stream should be created and we
  // should see stats again.
  {
    cricket::VoiceMediaInfo info;
    DeliverPacket(kPcmuFrame, sizeof(kPcmuFrame));
    SetAudioReceiveStreamStats();
    EXPECT_EQ(true, channel_->GetStats(&info));
    EXPECT_EQ(1u, info.senders.size());
    EXPECT_EQ(1u, info.receivers.size());
    VerifyVoiceReceiverInfo(info.receivers[0]);
  }
}

// Test that we can set the outgoing SSRC properly with multiple streams.
// SSRC is set in SetupSendStream() by calling AddSendStream.
TEST_F(WebRtcVoiceEngineTestFake, SetSendSsrcWithMultipleStreams) {
  EXPECT_TRUE(SetupSendStream());
  EXPECT_TRUE(call_.GetAudioSendStream(kSsrc1));
  EXPECT_TRUE(AddRecvStream(kSsrc2));
  EXPECT_EQ(kSsrc1, GetRecvStreamConfig(kSsrc2).rtp.local_ssrc);
}

// Test that the local SSRC is the same on sending and receiving channels if the
// receive channel is created before the send channel.
TEST_F(WebRtcVoiceEngineTestFake, SetSendSsrcAfterCreatingReceiveChannel) {
  EXPECT_TRUE(SetupChannel());
  EXPECT_TRUE(AddRecvStream(kSsrc2));
  EXPECT_TRUE(channel_->AddSendStream(
      cricket::StreamParams::CreateLegacy(kSsrc1)));
  EXPECT_TRUE(call_.GetAudioSendStream(kSsrc1));
  EXPECT_EQ(kSsrc1, GetRecvStreamConfig(kSsrc2).rtp.local_ssrc);
}

// Test that we can properly receive packets.
TEST_F(WebRtcVoiceEngineTestFake, Recv) {
  EXPECT_TRUE(SetupChannel());
  EXPECT_TRUE(AddRecvStream(1));
  DeliverPacket(kPcmuFrame, sizeof(kPcmuFrame));

  EXPECT_TRUE(GetRecvStream(1).VerifyLastPacket(kPcmuFrame,
                                                sizeof(kPcmuFrame)));
}

// Test that we can properly receive packets on multiple streams.
TEST_F(WebRtcVoiceEngineTestFake, RecvWithMultipleStreams) {
  EXPECT_TRUE(SetupChannel());
  const uint32_t ssrc1 = 1;
  const uint32_t ssrc2 = 2;
  const uint32_t ssrc3 = 3;
  EXPECT_TRUE(AddRecvStream(ssrc1));
  EXPECT_TRUE(AddRecvStream(ssrc2));
  EXPECT_TRUE(AddRecvStream(ssrc3));
  // Create packets with the right SSRCs.
  unsigned char packets[4][sizeof(kPcmuFrame)];
  for (size_t i = 0; i < arraysize(packets); ++i) {
    memcpy(packets[i], kPcmuFrame, sizeof(kPcmuFrame));
    rtc::SetBE32(packets[i] + 8, static_cast<uint32_t>(i));
  }

  const cricket::FakeAudioReceiveStream& s1 = GetRecvStream(ssrc1);
  const cricket::FakeAudioReceiveStream& s2 = GetRecvStream(ssrc2);
  const cricket::FakeAudioReceiveStream& s3 = GetRecvStream(ssrc3);

  EXPECT_EQ(s1.received_packets(), 0);
  EXPECT_EQ(s2.received_packets(), 0);
  EXPECT_EQ(s3.received_packets(), 0);

  DeliverPacket(packets[0], sizeof(packets[0]));
  EXPECT_EQ(s1.received_packets(), 0);
  EXPECT_EQ(s2.received_packets(), 0);
  EXPECT_EQ(s3.received_packets(), 0);

  DeliverPacket(packets[1], sizeof(packets[1]));
  EXPECT_EQ(s1.received_packets(), 1);
  EXPECT_TRUE(s1.VerifyLastPacket(packets[1], sizeof(packets[1])));
  EXPECT_EQ(s2.received_packets(), 0);
  EXPECT_EQ(s3.received_packets(), 0);

  DeliverPacket(packets[2], sizeof(packets[2]));
  EXPECT_EQ(s1.received_packets(), 1);
  EXPECT_EQ(s2.received_packets(), 1);
  EXPECT_TRUE(s2.VerifyLastPacket(packets[2], sizeof(packets[2])));
  EXPECT_EQ(s3.received_packets(), 0);

  DeliverPacket(packets[3], sizeof(packets[3]));
  EXPECT_EQ(s1.received_packets(), 1);
  EXPECT_EQ(s2.received_packets(), 1);
  EXPECT_EQ(s3.received_packets(), 1);
  EXPECT_TRUE(s3.VerifyLastPacket(packets[3], sizeof(packets[3])));

  EXPECT_TRUE(channel_->RemoveRecvStream(ssrc3));
  EXPECT_TRUE(channel_->RemoveRecvStream(ssrc2));
  EXPECT_TRUE(channel_->RemoveRecvStream(ssrc1));
}

// Test that receiving on an unsignalled stream works (default channel will be
// created).
TEST_F(WebRtcVoiceEngineTestFake, RecvUnsignalled) {
  EXPECT_TRUE(SetupChannel());
  EXPECT_EQ(0, call_.GetAudioReceiveStreams().size());

  DeliverPacket(kPcmuFrame, sizeof(kPcmuFrame));

  EXPECT_EQ(1, call_.GetAudioReceiveStreams().size());
  EXPECT_TRUE(GetRecvStream(1).VerifyLastPacket(kPcmuFrame,
                                                sizeof(kPcmuFrame)));
}

// Test that receiving on an unsignalled stream works (default channel will be
// created), and that packets will be forwarded to the default channel
// regardless of their SSRCs.
TEST_F(WebRtcVoiceEngineTestFake, RecvUnsignalledWithSsrcSwitch) {
  EXPECT_TRUE(SetupChannel());
  unsigned char packet[sizeof(kPcmuFrame)];
  memcpy(packet, kPcmuFrame, sizeof(kPcmuFrame));

  // Note that ssrc = 0 is not supported.
  uint32_t ssrc = 1;
  for (; ssrc < 10; ++ssrc) {
    rtc::SetBE32(&packet[8], ssrc);
    DeliverPacket(packet, sizeof(packet));

    // Verify we only have one default stream.
    EXPECT_EQ(1, call_.GetAudioReceiveStreams().size());
    EXPECT_EQ(1, GetRecvStream(ssrc).received_packets());
    EXPECT_TRUE(GetRecvStream(ssrc).VerifyLastPacket(packet, sizeof(packet)));
  }

  // Sending the same ssrc again should not create a new stream.
  --ssrc;
  DeliverPacket(packet, sizeof(packet));
  EXPECT_EQ(1, call_.GetAudioReceiveStreams().size());
  EXPECT_EQ(2, GetRecvStream(ssrc).received_packets());
  EXPECT_TRUE(GetRecvStream(ssrc).VerifyLastPacket(packet, sizeof(packet)));
}

// Test that a default channel is created even after a signalled stream has been
// added, and that this stream will get any packets for unknown SSRCs.
TEST_F(WebRtcVoiceEngineTestFake, RecvUnsignalledAfterSignalled) {
  EXPECT_TRUE(SetupChannel());
  unsigned char packet[sizeof(kPcmuFrame)];
  memcpy(packet, kPcmuFrame, sizeof(kPcmuFrame));

  // Add a known stream, send packet and verify we got it.
  const uint32_t signaled_ssrc = 1;
  rtc::SetBE32(&packet[8], signaled_ssrc);
  EXPECT_TRUE(AddRecvStream(signaled_ssrc));
  DeliverPacket(packet, sizeof(packet));
  EXPECT_TRUE(GetRecvStream(signaled_ssrc).VerifyLastPacket(
      packet, sizeof(packet)));

  // Note that the first unknown SSRC cannot be 0, because we only support
  // creating receive streams for SSRC!=0.
  const uint32_t unsignaled_ssrc = 7011;
  rtc::SetBE32(&packet[8], unsignaled_ssrc);
  DeliverPacket(packet, sizeof(packet));
  EXPECT_TRUE(GetRecvStream(unsignaled_ssrc).VerifyLastPacket(
      packet, sizeof(packet)));
  EXPECT_EQ(2, call_.GetAudioReceiveStreams().size());

  DeliverPacket(packet, sizeof(packet));
  EXPECT_EQ(2, GetRecvStream(unsignaled_ssrc).received_packets());

  rtc::SetBE32(&packet[8], signaled_ssrc);
  DeliverPacket(packet, sizeof(packet));
  EXPECT_EQ(2, GetRecvStream(signaled_ssrc).received_packets());
  EXPECT_EQ(2, call_.GetAudioReceiveStreams().size());
}

// Test that we properly handle failures to add a receive stream.
TEST_F(WebRtcVoiceEngineTestFake, AddRecvStreamFail) {
  EXPECT_TRUE(SetupChannel());
  voe_.set_fail_create_channel(true);
  EXPECT_FALSE(AddRecvStream(2));
}

// Test that we properly handle failures to add a send stream.
TEST_F(WebRtcVoiceEngineTestFake, AddSendStreamFail) {
  EXPECT_TRUE(SetupChannel());
  voe_.set_fail_create_channel(true);
  EXPECT_FALSE(channel_->AddSendStream(cricket::StreamParams::CreateLegacy(2)));
}

// Test that AddRecvStream creates new stream.
TEST_F(WebRtcVoiceEngineTestFake, AddRecvStream) {
  EXPECT_TRUE(SetupRecvStream());
  int channel_num = voe_.GetLastChannel();
  EXPECT_TRUE(AddRecvStream(1));
  EXPECT_NE(channel_num, voe_.GetLastChannel());
}

// Test that after adding a recv stream, we do not decode more codecs than
// those previously passed into SetRecvCodecs.
TEST_F(WebRtcVoiceEngineTestFake, AddRecvStreamUnsupportedCodec) {
  EXPECT_TRUE(SetupSendStream());
  cricket::AudioRecvParameters parameters;
  parameters.codecs.push_back(kIsacCodec);
  parameters.codecs.push_back(kPcmuCodec);
  EXPECT_TRUE(channel_->SetRecvParameters(parameters));
  EXPECT_TRUE(AddRecvStream(kSsrc1));
  int channel_num2 = voe_.GetLastChannel();
  webrtc::CodecInst gcodec;
  rtc::strcpyn(gcodec.plname, arraysize(gcodec.plname), "opus");
  gcodec.plfreq = 48000;
  gcodec.channels = 2;
  EXPECT_EQ(-1, voe_.GetRecPayloadType(channel_num2, gcodec));
}

// Test that we properly clean up any streams that were added, even if
// not explicitly removed.
TEST_F(WebRtcVoiceEngineTestFake, StreamCleanup) {
  EXPECT_TRUE(SetupSendStream());
  SetSendParameters(send_parameters_);
  EXPECT_TRUE(AddRecvStream(1));
  EXPECT_TRUE(AddRecvStream(2));
  EXPECT_EQ(3, voe_.GetNumChannels());  // default channel + 2 added
  delete channel_;
  channel_ = NULL;
  EXPECT_EQ(0, voe_.GetNumChannels());
}

TEST_F(WebRtcVoiceEngineTestFake, TestAddRecvStreamFailWithZeroSsrc) {
  EXPECT_TRUE(SetupSendStream());
  EXPECT_FALSE(AddRecvStream(0));
}

TEST_F(WebRtcVoiceEngineTestFake, TestNoLeakingWhenAddRecvStreamFail) {
  EXPECT_TRUE(SetupChannel());
  EXPECT_TRUE(AddRecvStream(1));
  // Manually delete channel to simulate a failure.
  int channel = voe_.GetLastChannel();
  EXPECT_EQ(0, voe_.DeleteChannel(channel));
  // Add recv stream 2 should work.
  EXPECT_TRUE(AddRecvStream(2));
  int new_channel = voe_.GetLastChannel();
  EXPECT_NE(channel, new_channel);
  // The last created channel is deleted too.
  EXPECT_EQ(0, voe_.DeleteChannel(new_channel));
}

// Test the InsertDtmf on default send stream as caller.
TEST_F(WebRtcVoiceEngineTestFake, InsertDtmfOnDefaultSendStreamAsCaller) {
  TestInsertDtmf(0, true);
}

// Test the InsertDtmf on default send stream as callee
TEST_F(WebRtcVoiceEngineTestFake, InsertDtmfOnDefaultSendStreamAsCallee) {
  TestInsertDtmf(0, false);
}

// Test the InsertDtmf on specified send stream as caller.
TEST_F(WebRtcVoiceEngineTestFake, InsertDtmfOnSendStreamAsCaller) {
  TestInsertDtmf(kSsrc1, true);
}

// Test the InsertDtmf on specified send stream as callee.
TEST_F(WebRtcVoiceEngineTestFake, InsertDtmfOnSendStreamAsCallee) {
  TestInsertDtmf(kSsrc1, false);
}

TEST_F(WebRtcVoiceEngineTestFake, SetAudioOptions) {
  EXPECT_TRUE(SetupSendStream());
  EXPECT_CALL(adm_,
              BuiltInAECIsAvailable()).Times(9).WillRepeatedly(Return(false));
  EXPECT_CALL(adm_,
              BuiltInAGCIsAvailable()).Times(4).WillRepeatedly(Return(false));
  EXPECT_CALL(adm_,
              BuiltInNSIsAvailable()).Times(2).WillRepeatedly(Return(false));
  bool ec_enabled;
  webrtc::EcModes ec_mode;
  webrtc::AecmModes aecm_mode;
  bool cng_enabled;
  bool agc_enabled;
  webrtc::AgcModes agc_mode;
  webrtc::AgcConfig agc_config;
  bool ns_enabled;
  webrtc::NsModes ns_mode;
  bool highpass_filter_enabled;
  bool stereo_swapping_enabled;
  bool typing_detection_enabled;
  voe_.GetEcStatus(ec_enabled, ec_mode);
  voe_.GetAecmMode(aecm_mode, cng_enabled);
  voe_.GetAgcStatus(agc_enabled, agc_mode);
  voe_.GetAgcConfig(agc_config);
  voe_.GetNsStatus(ns_enabled, ns_mode);
  highpass_filter_enabled = voe_.IsHighPassFilterEnabled();
  stereo_swapping_enabled = voe_.IsStereoChannelSwappingEnabled();
  voe_.GetTypingDetectionStatus(typing_detection_enabled);
  EXPECT_TRUE(ec_enabled);
  EXPECT_TRUE(voe_.ec_metrics_enabled());
  EXPECT_FALSE(cng_enabled);
  EXPECT_TRUE(agc_enabled);
  EXPECT_EQ(0, agc_config.targetLeveldBOv);
  EXPECT_TRUE(ns_enabled);
  EXPECT_TRUE(highpass_filter_enabled);
  EXPECT_FALSE(stereo_swapping_enabled);
  EXPECT_TRUE(typing_detection_enabled);
  EXPECT_EQ(ec_mode, webrtc::kEcConference);
  EXPECT_EQ(ns_mode, webrtc::kNsHighSuppression);
  EXPECT_EQ(50, voe_.GetNetEqCapacity());
  EXPECT_FALSE(voe_.GetNetEqFastAccelerate());

  // Nothing set in AudioOptions, so everything should be as default.
  send_parameters_.options = cricket::AudioOptions();
  SetSendParameters(send_parameters_);
  voe_.GetEcStatus(ec_enabled, ec_mode);
  voe_.GetAecmMode(aecm_mode, cng_enabled);
  voe_.GetAgcStatus(agc_enabled, agc_mode);
  voe_.GetAgcConfig(agc_config);
  voe_.GetNsStatus(ns_enabled, ns_mode);
  highpass_filter_enabled = voe_.IsHighPassFilterEnabled();
  stereo_swapping_enabled = voe_.IsStereoChannelSwappingEnabled();
  voe_.GetTypingDetectionStatus(typing_detection_enabled);
  EXPECT_TRUE(ec_enabled);
  EXPECT_TRUE(voe_.ec_metrics_enabled());
  EXPECT_FALSE(cng_enabled);
  EXPECT_TRUE(agc_enabled);
  EXPECT_EQ(0, agc_config.targetLeveldBOv);
  EXPECT_TRUE(ns_enabled);
  EXPECT_TRUE(highpass_filter_enabled);
  EXPECT_FALSE(stereo_swapping_enabled);
  EXPECT_TRUE(typing_detection_enabled);
  EXPECT_EQ(ec_mode, webrtc::kEcConference);
  EXPECT_EQ(ns_mode, webrtc::kNsHighSuppression);
  EXPECT_EQ(50, voe_.GetNetEqCapacity());
  EXPECT_FALSE(voe_.GetNetEqFastAccelerate());

  // Turn echo cancellation off
  send_parameters_.options.echo_cancellation = rtc::Optional<bool>(false);
  SetSendParameters(send_parameters_);
  voe_.GetEcStatus(ec_enabled, ec_mode);
  EXPECT_FALSE(ec_enabled);

  // Turn echo cancellation back on, with settings, and make sure
  // nothing else changed.
  send_parameters_.options.echo_cancellation = rtc::Optional<bool>(true);
  SetSendParameters(send_parameters_);
  voe_.GetEcStatus(ec_enabled, ec_mode);
  voe_.GetAecmMode(aecm_mode, cng_enabled);
  voe_.GetAgcStatus(agc_enabled, agc_mode);
  voe_.GetAgcConfig(agc_config);
  voe_.GetNsStatus(ns_enabled, ns_mode);
  highpass_filter_enabled = voe_.IsHighPassFilterEnabled();
  stereo_swapping_enabled = voe_.IsStereoChannelSwappingEnabled();
  voe_.GetTypingDetectionStatus(typing_detection_enabled);
  EXPECT_TRUE(ec_enabled);
  EXPECT_TRUE(voe_.ec_metrics_enabled());
  EXPECT_TRUE(agc_enabled);
  EXPECT_EQ(0, agc_config.targetLeveldBOv);
  EXPECT_TRUE(ns_enabled);
  EXPECT_TRUE(highpass_filter_enabled);
  EXPECT_FALSE(stereo_swapping_enabled);
  EXPECT_TRUE(typing_detection_enabled);
  EXPECT_EQ(ec_mode, webrtc::kEcConference);
  EXPECT_EQ(ns_mode, webrtc::kNsHighSuppression);

  // Turn on delay agnostic aec and make sure nothing change w.r.t. echo
  // control.
  send_parameters_.options.delay_agnostic_aec = rtc::Optional<bool>(true);
  SetSendParameters(send_parameters_);
  voe_.GetEcStatus(ec_enabled, ec_mode);
  voe_.GetAecmMode(aecm_mode, cng_enabled);
  EXPECT_TRUE(ec_enabled);
  EXPECT_TRUE(voe_.ec_metrics_enabled());
  EXPECT_EQ(ec_mode, webrtc::kEcConference);

  // Turn off echo cancellation and delay agnostic aec.
  send_parameters_.options.delay_agnostic_aec = rtc::Optional<bool>(false);
  send_parameters_.options.extended_filter_aec = rtc::Optional<bool>(false);
  send_parameters_.options.echo_cancellation = rtc::Optional<bool>(false);
  SetSendParameters(send_parameters_);
  voe_.GetEcStatus(ec_enabled, ec_mode);
  EXPECT_FALSE(ec_enabled);
  // Turning delay agnostic aec back on should also turn on echo cancellation.
  send_parameters_.options.delay_agnostic_aec = rtc::Optional<bool>(true);
  SetSendParameters(send_parameters_);
  voe_.GetEcStatus(ec_enabled, ec_mode);
  EXPECT_TRUE(ec_enabled);
  EXPECT_TRUE(voe_.ec_metrics_enabled());
  EXPECT_EQ(ec_mode, webrtc::kEcConference);

  // Turn off AGC
  send_parameters_.options.auto_gain_control = rtc::Optional<bool>(false);
  SetSendParameters(send_parameters_);
  voe_.GetAgcStatus(agc_enabled, agc_mode);
  EXPECT_FALSE(agc_enabled);

  // Turn AGC back on
  send_parameters_.options.auto_gain_control = rtc::Optional<bool>(true);
  send_parameters_.options.adjust_agc_delta = rtc::Optional<int>();
  SetSendParameters(send_parameters_);
  voe_.GetAgcStatus(agc_enabled, agc_mode);
  EXPECT_TRUE(agc_enabled);
  voe_.GetAgcConfig(agc_config);
  EXPECT_EQ(0, agc_config.targetLeveldBOv);

  // Turn off other options (and stereo swapping on).
  send_parameters_.options.noise_suppression = rtc::Optional<bool>(false);
  send_parameters_.options.highpass_filter = rtc::Optional<bool>(false);
  send_parameters_.options.typing_detection = rtc::Optional<bool>(false);
  send_parameters_.options.stereo_swapping = rtc::Optional<bool>(true);
  SetSendParameters(send_parameters_);
  voe_.GetNsStatus(ns_enabled, ns_mode);
  highpass_filter_enabled = voe_.IsHighPassFilterEnabled();
  stereo_swapping_enabled = voe_.IsStereoChannelSwappingEnabled();
  voe_.GetTypingDetectionStatus(typing_detection_enabled);
  EXPECT_FALSE(ns_enabled);
  EXPECT_FALSE(highpass_filter_enabled);
  EXPECT_FALSE(typing_detection_enabled);
  EXPECT_TRUE(stereo_swapping_enabled);

  // Set options again to ensure it has no impact.
  SetSendParameters(send_parameters_);
  voe_.GetEcStatus(ec_enabled, ec_mode);
  voe_.GetNsStatus(ns_enabled, ns_mode);
  EXPECT_TRUE(ec_enabled);
  EXPECT_EQ(webrtc::kEcConference, ec_mode);
  EXPECT_FALSE(ns_enabled);
  EXPECT_EQ(webrtc::kNsHighSuppression, ns_mode);
}

TEST_F(WebRtcVoiceEngineTestFake, DefaultOptions) {
  EXPECT_TRUE(SetupSendStream());

  bool ec_enabled;
  webrtc::EcModes ec_mode;
  bool agc_enabled;
  webrtc::AgcModes agc_mode;
  bool ns_enabled;
  webrtc::NsModes ns_mode;
  bool highpass_filter_enabled;
  bool stereo_swapping_enabled;
  bool typing_detection_enabled;

  voe_.GetEcStatus(ec_enabled, ec_mode);
  voe_.GetAgcStatus(agc_enabled, agc_mode);
  voe_.GetNsStatus(ns_enabled, ns_mode);
  highpass_filter_enabled = voe_.IsHighPassFilterEnabled();
  stereo_swapping_enabled = voe_.IsStereoChannelSwappingEnabled();
  voe_.GetTypingDetectionStatus(typing_detection_enabled);
  EXPECT_TRUE(ec_enabled);
  EXPECT_TRUE(agc_enabled);
  EXPECT_TRUE(ns_enabled);
  EXPECT_TRUE(highpass_filter_enabled);
  EXPECT_TRUE(typing_detection_enabled);
  EXPECT_FALSE(stereo_swapping_enabled);
}

TEST_F(WebRtcVoiceEngineTestFake, InitDoesNotOverwriteDefaultAgcConfig) {
  webrtc::AgcConfig set_config = {0};
  set_config.targetLeveldBOv = 3;
  set_config.digitalCompressionGaindB = 9;
  set_config.limiterEnable = true;
  EXPECT_EQ(0, voe_.SetAgcConfig(set_config));

  webrtc::AgcConfig config = {0};
  EXPECT_EQ(0, voe_.GetAgcConfig(config));
  EXPECT_EQ(set_config.targetLeveldBOv, config.targetLeveldBOv);
  EXPECT_EQ(set_config.digitalCompressionGaindB,
            config.digitalCompressionGaindB);
  EXPECT_EQ(set_config.limiterEnable, config.limiterEnable);
}

TEST_F(WebRtcVoiceEngineTestFake, SetOptionOverridesViaChannels) {
  EXPECT_TRUE(SetupSendStream());
  EXPECT_CALL(adm_,
              BuiltInAECIsAvailable()).Times(9).WillRepeatedly(Return(false));
  EXPECT_CALL(adm_,
              BuiltInAGCIsAvailable()).Times(9).WillRepeatedly(Return(false));
  EXPECT_CALL(adm_,
              BuiltInNSIsAvailable()).Times(9).WillRepeatedly(Return(false));
  EXPECT_CALL(adm_,
              RecordingIsInitialized()).Times(2).WillRepeatedly(Return(false));
  EXPECT_CALL(adm_, Recording()).Times(2).WillRepeatedly(Return(false));
  EXPECT_CALL(adm_, InitRecording()).Times(2).WillRepeatedly(Return(0));
  EXPECT_CALL(apm_, ApplyConfig(testing::_)).Times(10);
  EXPECT_CALL(apm_, SetExtraOptions(testing::_)).Times(10);

  std::unique_ptr<cricket::WebRtcVoiceMediaChannel> channel1(
      static_cast<cricket::WebRtcVoiceMediaChannel*>(engine_->CreateChannel(
          &call_, cricket::MediaConfig(), cricket::AudioOptions())));
  std::unique_ptr<cricket::WebRtcVoiceMediaChannel> channel2(
      static_cast<cricket::WebRtcVoiceMediaChannel*>(engine_->CreateChannel(
          &call_, cricket::MediaConfig(), cricket::AudioOptions())));

  // Have to add a stream to make SetSend work.
  cricket::StreamParams stream1;
  stream1.ssrcs.push_back(1);
  channel1->AddSendStream(stream1);
  cricket::StreamParams stream2;
  stream2.ssrcs.push_back(2);
  channel2->AddSendStream(stream2);

  // AEC and AGC and NS
  cricket::AudioSendParameters parameters_options_all = send_parameters_;
  parameters_options_all.options.echo_cancellation = rtc::Optional<bool>(true);
  parameters_options_all.options.auto_gain_control = rtc::Optional<bool>(true);
  parameters_options_all.options.noise_suppression = rtc::Optional<bool>(true);
  EXPECT_TRUE(channel1->SetSendParameters(parameters_options_all));
  EXPECT_EQ(parameters_options_all.options, channel1->options());
  EXPECT_TRUE(channel2->SetSendParameters(parameters_options_all));
  EXPECT_EQ(parameters_options_all.options, channel2->options());

  // unset NS
  cricket::AudioSendParameters parameters_options_no_ns = send_parameters_;
  parameters_options_no_ns.options.noise_suppression =
      rtc::Optional<bool>(false);
  EXPECT_TRUE(channel1->SetSendParameters(parameters_options_no_ns));
  cricket::AudioOptions expected_options = parameters_options_all.options;
  expected_options.echo_cancellation = rtc::Optional<bool>(true);
  expected_options.auto_gain_control = rtc::Optional<bool>(true);
  expected_options.noise_suppression = rtc::Optional<bool>(false);
  EXPECT_EQ(expected_options, channel1->options());

  // unset AGC
  cricket::AudioSendParameters parameters_options_no_agc = send_parameters_;
  parameters_options_no_agc.options.auto_gain_control =
      rtc::Optional<bool>(false);
  EXPECT_TRUE(channel2->SetSendParameters(parameters_options_no_agc));
  expected_options.echo_cancellation = rtc::Optional<bool>(true);
  expected_options.auto_gain_control = rtc::Optional<bool>(false);
  expected_options.noise_suppression = rtc::Optional<bool>(true);
  EXPECT_EQ(expected_options, channel2->options());

  EXPECT_TRUE(channel_->SetSendParameters(parameters_options_all));
  bool ec_enabled;
  webrtc::EcModes ec_mode;
  bool agc_enabled;
  webrtc::AgcModes agc_mode;
  bool ns_enabled;
  webrtc::NsModes ns_mode;
  voe_.GetEcStatus(ec_enabled, ec_mode);
  voe_.GetAgcStatus(agc_enabled, agc_mode);
  voe_.GetNsStatus(ns_enabled, ns_mode);
  EXPECT_TRUE(ec_enabled);
  EXPECT_TRUE(agc_enabled);
  EXPECT_TRUE(ns_enabled);

  channel1->SetSend(true);
  voe_.GetEcStatus(ec_enabled, ec_mode);
  voe_.GetAgcStatus(agc_enabled, agc_mode);
  voe_.GetNsStatus(ns_enabled, ns_mode);
  EXPECT_TRUE(ec_enabled);
  EXPECT_TRUE(agc_enabled);
  EXPECT_FALSE(ns_enabled);

  channel2->SetSend(true);
  voe_.GetEcStatus(ec_enabled, ec_mode);
  voe_.GetAgcStatus(agc_enabled, agc_mode);
  voe_.GetNsStatus(ns_enabled, ns_mode);
  EXPECT_TRUE(ec_enabled);
  EXPECT_FALSE(agc_enabled);
  EXPECT_TRUE(ns_enabled);

  // Make sure settings take effect while we are sending.
  EXPECT_TRUE(channel_->SetSendParameters(parameters_options_all));
  EXPECT_CALL(apm_, ApplyConfig(testing::_));
  EXPECT_CALL(apm_, SetExtraOptions(testing::_));
  cricket::AudioSendParameters parameters_options_no_agc_nor_ns =
      send_parameters_;
  parameters_options_no_agc_nor_ns.options.auto_gain_control =
      rtc::Optional<bool>(false);
  parameters_options_no_agc_nor_ns.options.noise_suppression =
      rtc::Optional<bool>(false);
  channel2->SetSend(true);
  EXPECT_TRUE(channel2->SetSendParameters(parameters_options_no_agc_nor_ns));
  expected_options.echo_cancellation = rtc::Optional<bool>(true);
  expected_options.auto_gain_control = rtc::Optional<bool>(false);
  expected_options.noise_suppression = rtc::Optional<bool>(false);
  EXPECT_EQ(expected_options, channel2->options());
  voe_.GetEcStatus(ec_enabled, ec_mode);
  voe_.GetAgcStatus(agc_enabled, agc_mode);
  voe_.GetNsStatus(ns_enabled, ns_mode);
  EXPECT_TRUE(ec_enabled);
  EXPECT_FALSE(agc_enabled);
  EXPECT_FALSE(ns_enabled);
}

// This test verifies DSCP settings are properly applied on voice media channel.
TEST_F(WebRtcVoiceEngineTestFake, TestSetDscpOptions) {
  EXPECT_TRUE(SetupSendStream());
  cricket::FakeNetworkInterface network_interface;
  cricket::MediaConfig config;
  std::unique_ptr<cricket::VoiceMediaChannel> channel;

  EXPECT_CALL(apm_, ApplyConfig(testing::_)).Times(3);
  EXPECT_CALL(apm_, SetExtraOptions(testing::_)).Times(3);

  channel.reset(
      engine_->CreateChannel(&call_, config, cricket::AudioOptions()));
  channel->SetInterface(&network_interface);
  // Default value when DSCP is disabled should be DSCP_DEFAULT.
  EXPECT_EQ(rtc::DSCP_DEFAULT, network_interface.dscp());

  config.enable_dscp = true;
  channel.reset(
      engine_->CreateChannel(&call_, config, cricket::AudioOptions()));
  channel->SetInterface(&network_interface);
  EXPECT_EQ(rtc::DSCP_EF, network_interface.dscp());

  // Verify that setting the option to false resets the
  // DiffServCodePoint.
  config.enable_dscp = false;
  channel.reset(
      engine_->CreateChannel(&call_, config, cricket::AudioOptions()));
  channel->SetInterface(&network_interface);
  // Default value when DSCP is disabled should be DSCP_DEFAULT.
  EXPECT_EQ(rtc::DSCP_DEFAULT, network_interface.dscp());

  channel->SetInterface(nullptr);
}

TEST_F(WebRtcVoiceEngineTestFake, TestGetReceiveChannelId) {
  EXPECT_TRUE(SetupChannel());
  cricket::WebRtcVoiceMediaChannel* media_channel =
        static_cast<cricket::WebRtcVoiceMediaChannel*>(channel_);
  EXPECT_EQ(-1, media_channel->GetReceiveChannelId(0));
  EXPECT_TRUE(AddRecvStream(kSsrc1));
  int channel_id = voe_.GetLastChannel();
  EXPECT_EQ(channel_id, media_channel->GetReceiveChannelId(kSsrc1));
  EXPECT_EQ(-1, media_channel->GetReceiveChannelId(kSsrc2));
  EXPECT_TRUE(AddRecvStream(kSsrc2));
  int channel_id2 = voe_.GetLastChannel();
  EXPECT_EQ(channel_id2, media_channel->GetReceiveChannelId(kSsrc2));
}

TEST_F(WebRtcVoiceEngineTestFake, TestGetSendChannelId) {
  EXPECT_TRUE(SetupChannel());
  cricket::WebRtcVoiceMediaChannel* media_channel =
        static_cast<cricket::WebRtcVoiceMediaChannel*>(channel_);
  EXPECT_EQ(-1, media_channel->GetSendChannelId(0));
  EXPECT_TRUE(channel_->AddSendStream(
      cricket::StreamParams::CreateLegacy(kSsrc1)));
  int channel_id = voe_.GetLastChannel();
  EXPECT_EQ(channel_id, media_channel->GetSendChannelId(kSsrc1));
  EXPECT_EQ(-1, media_channel->GetSendChannelId(kSsrc2));
  EXPECT_TRUE(channel_->AddSendStream(
      cricket::StreamParams::CreateLegacy(kSsrc2)));
  int channel_id2 = voe_.GetLastChannel();
  EXPECT_EQ(channel_id2, media_channel->GetSendChannelId(kSsrc2));
}

TEST_F(WebRtcVoiceEngineTestFake, SetOutputVolume) {
  EXPECT_TRUE(SetupChannel());
  EXPECT_FALSE(channel_->SetOutputVolume(kSsrc2, 0.5));
  cricket::StreamParams stream;
  stream.ssrcs.push_back(kSsrc2);
  EXPECT_TRUE(channel_->AddRecvStream(stream));
  EXPECT_DOUBLE_EQ(1, GetRecvStream(kSsrc2).gain());
  EXPECT_TRUE(channel_->SetOutputVolume(kSsrc2, 3));
  EXPECT_DOUBLE_EQ(3, GetRecvStream(kSsrc2).gain());
}

TEST_F(WebRtcVoiceEngineTestFake, SetOutputVolumeDefaultRecvStream) {
  EXPECT_TRUE(SetupChannel());
  EXPECT_TRUE(channel_->SetOutputVolume(0, 2));
  DeliverPacket(kPcmuFrame, sizeof(kPcmuFrame));
  EXPECT_DOUBLE_EQ(2, GetRecvStream(1).gain());
  EXPECT_TRUE(channel_->SetOutputVolume(0, 3));
  EXPECT_DOUBLE_EQ(3, GetRecvStream(1).gain());
  EXPECT_TRUE(channel_->SetOutputVolume(1, 4));
  EXPECT_DOUBLE_EQ(4, GetRecvStream(1).gain());
}

TEST_F(WebRtcVoiceEngineTestFake, SetsSyncGroupFromSyncLabel) {
  const uint32_t kAudioSsrc = 123;
  const std::string kSyncLabel = "AvSyncLabel";

  EXPECT_TRUE(SetupSendStream());
  cricket::StreamParams sp = cricket::StreamParams::CreateLegacy(kAudioSsrc);
  sp.sync_label = kSyncLabel;
  // Creating two channels to make sure that sync label is set properly for both
  // the default voice channel and following ones.
  EXPECT_TRUE(channel_->AddRecvStream(sp));
  sp.ssrcs[0] += 1;
  EXPECT_TRUE(channel_->AddRecvStream(sp));

  ASSERT_EQ(2, call_.GetAudioReceiveStreams().size());
  EXPECT_EQ(kSyncLabel,
            call_.GetAudioReceiveStream(kAudioSsrc)->GetConfig().sync_group)
      << "SyncGroup should be set based on sync_label";
  EXPECT_EQ(kSyncLabel,
            call_.GetAudioReceiveStream(kAudioSsrc + 1)->GetConfig().sync_group)
      << "SyncGroup should be set based on sync_label";
}

// TODO(solenberg): Remove, once recv streams are configured through Call.
//                  (This is then covered by TestSetRecvRtpHeaderExtensions.)
TEST_F(WebRtcVoiceEngineTestFake, ConfiguresAudioReceiveStreamRtpExtensions) {
  // Test that setting the header extensions results in the expected state
  // changes on an associated Call.
  std::vector<uint32_t> ssrcs;
  ssrcs.push_back(223);
  ssrcs.push_back(224);

  EXPECT_TRUE(SetupSendStream());
  SetSendParameters(send_parameters_);
  for (uint32_t ssrc : ssrcs) {
    EXPECT_TRUE(channel_->AddRecvStream(
        cricket::StreamParams::CreateLegacy(ssrc)));
  }

  EXPECT_EQ(2, call_.GetAudioReceiveStreams().size());
  for (uint32_t ssrc : ssrcs) {
    const auto* s = call_.GetAudioReceiveStream(ssrc);
    EXPECT_NE(nullptr, s);
    EXPECT_EQ(0, s->GetConfig().rtp.extensions.size());
  }

  // Set up receive extensions.
  cricket::RtpCapabilities capabilities = engine_->GetCapabilities();
  cricket::AudioRecvParameters recv_parameters;
  recv_parameters.extensions = capabilities.header_extensions;
  channel_->SetRecvParameters(recv_parameters);
  EXPECT_EQ(2, call_.GetAudioReceiveStreams().size());
  for (uint32_t ssrc : ssrcs) {
    const auto* s = call_.GetAudioReceiveStream(ssrc);
    EXPECT_NE(nullptr, s);
    const auto& s_exts = s->GetConfig().rtp.extensions;
    EXPECT_EQ(capabilities.header_extensions.size(), s_exts.size());
    for (const auto& e_ext : capabilities.header_extensions) {
      for (const auto& s_ext : s_exts) {
        if (e_ext.id == s_ext.id) {
          EXPECT_EQ(e_ext.uri, s_ext.uri);
        }
      }
    }
  }

  // Disable receive extensions.
  channel_->SetRecvParameters(cricket::AudioRecvParameters());
  for (uint32_t ssrc : ssrcs) {
    const auto* s = call_.GetAudioReceiveStream(ssrc);
    EXPECT_NE(nullptr, s);
    EXPECT_EQ(0, s->GetConfig().rtp.extensions.size());
  }
}

TEST_F(WebRtcVoiceEngineTestFake, DeliverAudioPacket_Call) {
  // Test that packets are forwarded to the Call when configured accordingly.
  const uint32_t kAudioSsrc = 1;
  rtc::CopyOnWriteBuffer kPcmuPacket(kPcmuFrame, sizeof(kPcmuFrame));
  static const unsigned char kRtcp[] = {
    0x80, 0xc9, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02,
    0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
  };
  rtc::CopyOnWriteBuffer kRtcpPacket(kRtcp, sizeof(kRtcp));

  EXPECT_TRUE(SetupSendStream());
  cricket::WebRtcVoiceMediaChannel* media_channel =
      static_cast<cricket::WebRtcVoiceMediaChannel*>(channel_);
  SetSendParameters(send_parameters_);
  EXPECT_TRUE(media_channel->AddRecvStream(
      cricket::StreamParams::CreateLegacy(kAudioSsrc)));

  EXPECT_EQ(1, call_.GetAudioReceiveStreams().size());
  const cricket::FakeAudioReceiveStream* s =
      call_.GetAudioReceiveStream(kAudioSsrc);
  EXPECT_EQ(0, s->received_packets());
  channel_->OnPacketReceived(&kPcmuPacket, rtc::PacketTime());
  EXPECT_EQ(1, s->received_packets());
  channel_->OnRtcpReceived(&kRtcpPacket, rtc::PacketTime());
  EXPECT_EQ(2, s->received_packets());
}

// All receive channels should be associated with the first send channel,
// since they do not send RTCP SR.
TEST_F(WebRtcVoiceEngineTestFake, AssociateFirstSendChannel_SendCreatedFirst) {
  EXPECT_TRUE(SetupSendStream());
  EXPECT_TRUE(AddRecvStream(kSsrc2));
  EXPECT_EQ(kSsrc1, GetRecvStreamConfig(kSsrc2).rtp.local_ssrc);
  EXPECT_TRUE(channel_->AddSendStream(
      cricket::StreamParams::CreateLegacy(kSsrc3)));
  EXPECT_EQ(kSsrc1, GetRecvStreamConfig(kSsrc2).rtp.local_ssrc);
  EXPECT_TRUE(AddRecvStream(kSsrc4));
  EXPECT_EQ(kSsrc1, GetRecvStreamConfig(kSsrc4).rtp.local_ssrc);
}

TEST_F(WebRtcVoiceEngineTestFake, AssociateFirstSendChannel_RecvCreatedFirst) {
  EXPECT_TRUE(SetupRecvStream());
  EXPECT_EQ(0xFA17FA17u, GetRecvStreamConfig(kSsrc1).rtp.local_ssrc);
  EXPECT_TRUE(channel_->AddSendStream(
      cricket::StreamParams::CreateLegacy(kSsrc2)));
  EXPECT_EQ(kSsrc2, GetRecvStreamConfig(kSsrc1).rtp.local_ssrc);
  EXPECT_TRUE(AddRecvStream(kSsrc3));
  EXPECT_EQ(kSsrc2, GetRecvStreamConfig(kSsrc3).rtp.local_ssrc);
  EXPECT_TRUE(channel_->AddSendStream(
      cricket::StreamParams::CreateLegacy(kSsrc4)));
  EXPECT_EQ(kSsrc2, GetRecvStreamConfig(kSsrc1).rtp.local_ssrc);
  EXPECT_EQ(kSsrc2, GetRecvStreamConfig(kSsrc3).rtp.local_ssrc);
}

TEST_F(WebRtcVoiceEngineTestFake, SetRawAudioSink) {
  EXPECT_TRUE(SetupChannel());
  std::unique_ptr<FakeAudioSink> fake_sink_1(new FakeAudioSink());
  std::unique_ptr<FakeAudioSink> fake_sink_2(new FakeAudioSink());

  // Setting the sink before a recv stream exists should do nothing.
  channel_->SetRawAudioSink(kSsrc1, std::move(fake_sink_1));
  EXPECT_TRUE(AddRecvStream(kSsrc1));
  EXPECT_EQ(nullptr, GetRecvStream(kSsrc1).sink());

  // Now try actually setting the sink.
  channel_->SetRawAudioSink(kSsrc1, std::move(fake_sink_2));
  EXPECT_NE(nullptr, GetRecvStream(kSsrc1).sink());

  // Now try resetting it.
  channel_->SetRawAudioSink(kSsrc1, nullptr);
  EXPECT_EQ(nullptr, GetRecvStream(kSsrc1).sink());
}

TEST_F(WebRtcVoiceEngineTestFake, SetRawAudioSinkDefaultRecvStream) {
  EXPECT_TRUE(SetupChannel());
  std::unique_ptr<FakeAudioSink> fake_sink_1(new FakeAudioSink());
  std::unique_ptr<FakeAudioSink> fake_sink_2(new FakeAudioSink());

  // Should be able to set a default sink even when no stream exists.
  channel_->SetRawAudioSink(0, std::move(fake_sink_1));

  // Create default channel and ensure it's assigned the default sink.
  DeliverPacket(kPcmuFrame, sizeof(kPcmuFrame));
  EXPECT_NE(nullptr, GetRecvStream(0x01).sink());

  // Try resetting the default sink.
  channel_->SetRawAudioSink(0, nullptr);
  EXPECT_EQ(nullptr, GetRecvStream(0x01).sink());

  // Try setting the default sink while the default stream exists.
  channel_->SetRawAudioSink(0, std::move(fake_sink_2));
  EXPECT_NE(nullptr, GetRecvStream(0x01).sink());

  // If we remove and add a default stream, it should get the same sink.
  EXPECT_TRUE(channel_->RemoveRecvStream(0x01));
  DeliverPacket(kPcmuFrame, sizeof(kPcmuFrame));
  EXPECT_NE(nullptr, GetRecvStream(0x01).sink());
}

// Test that, just like the video channel, the voice channel communicates the
// network state to the call.
TEST_F(WebRtcVoiceEngineTestFake, OnReadyToSendSignalsNetworkState) {
  EXPECT_TRUE(SetupChannel());

  EXPECT_EQ(webrtc::kNetworkUp,
            call_.GetNetworkState(webrtc::MediaType::AUDIO));
  EXPECT_EQ(webrtc::kNetworkUp,
            call_.GetNetworkState(webrtc::MediaType::VIDEO));

  channel_->OnReadyToSend(false);
  EXPECT_EQ(webrtc::kNetworkDown,
            call_.GetNetworkState(webrtc::MediaType::AUDIO));
  EXPECT_EQ(webrtc::kNetworkUp,
            call_.GetNetworkState(webrtc::MediaType::VIDEO));

  channel_->OnReadyToSend(true);
  EXPECT_EQ(webrtc::kNetworkUp,
            call_.GetNetworkState(webrtc::MediaType::AUDIO));
  EXPECT_EQ(webrtc::kNetworkUp,
            call_.GetNetworkState(webrtc::MediaType::VIDEO));
}

// Test that playout is still started after changing parameters
TEST_F(WebRtcVoiceEngineTestFake, PreservePlayoutWhenRecreateRecvStream) {
  SetupRecvStream();
  channel_->SetPlayout(true);
  EXPECT_TRUE(GetRecvStream(kSsrc1).started());

  // Changing RTP header extensions will recreate the AudioReceiveStream.
  cricket::AudioRecvParameters parameters;
  parameters.extensions.push_back(
      webrtc::RtpExtension(webrtc::RtpExtension::kAudioLevelUri, 12));
  channel_->SetRecvParameters(parameters);

  EXPECT_TRUE(GetRecvStream(kSsrc1).started());
}

// Tests that the library initializes and shuts down properly.
TEST(WebRtcVoiceEngineTest, StartupShutdown) {
  // If the VoiceEngine wants to gather available codecs early, that's fine but
  // we never want it to create a decoder at this stage.
  cricket::WebRtcVoiceEngine engine(
      nullptr, webrtc::MockAudioDecoderFactory::CreateUnusedFactory());
  webrtc::RtcEventLogNullImpl event_log;
  std::unique_ptr<webrtc::Call> call(
      webrtc::Call::Create(webrtc::Call::Config(&event_log)));
  cricket::VoiceMediaChannel* channel = engine.CreateChannel(
      call.get(), cricket::MediaConfig(), cricket::AudioOptions());
  EXPECT_TRUE(channel != nullptr);
  delete channel;
}

// Tests that reference counting on the external ADM is correct.
TEST(WebRtcVoiceEngineTest, StartupShutdownWithExternalADM) {
  testing::NiceMock<webrtc::test::MockAudioDeviceModule> adm;
  EXPECT_CALL(adm, AddRef()).Times(3).WillRepeatedly(Return(0));
  EXPECT_CALL(adm, Release()).Times(3).WillRepeatedly(Return(0));
  {
    cricket::WebRtcVoiceEngine engine(
        &adm, webrtc::MockAudioDecoderFactory::CreateUnusedFactory());
    webrtc::RtcEventLogNullImpl event_log;
    std::unique_ptr<webrtc::Call> call(
        webrtc::Call::Create(webrtc::Call::Config(&event_log)));
    cricket::VoiceMediaChannel* channel = engine.CreateChannel(
        call.get(), cricket::MediaConfig(), cricket::AudioOptions());
    EXPECT_TRUE(channel != nullptr);
    delete channel;
  }
}

// Tests that the library is configured with the codecs we want.
TEST(WebRtcVoiceEngineTest, HasCorrectCodecs) {
  // TODO(ossu): These tests should move into a future "builtin audio codecs"
  // module.

  // Check codecs by name.
  EXPECT_TRUE(cricket::WebRtcVoiceEngine::ToCodecInst(
      cricket::AudioCodec(96, "OPUS", 48000, 0, 2), nullptr));
  EXPECT_TRUE(cricket::WebRtcVoiceEngine::ToCodecInst(
      cricket::AudioCodec(96, "ISAC", 16000, 0, 1), nullptr));
  EXPECT_TRUE(cricket::WebRtcVoiceEngine::ToCodecInst(
      cricket::AudioCodec(96, "ISAC", 32000, 0, 1), nullptr));
  // Check that name matching is case-insensitive.
  EXPECT_TRUE(cricket::WebRtcVoiceEngine::ToCodecInst(
      cricket::AudioCodec(96, "ILBC", 8000, 0, 1), nullptr));
  EXPECT_TRUE(cricket::WebRtcVoiceEngine::ToCodecInst(
      cricket::AudioCodec(96, "iLBC", 8000, 0, 1), nullptr));
  EXPECT_TRUE(cricket::WebRtcVoiceEngine::ToCodecInst(
      cricket::AudioCodec(96, "CN", 32000, 0, 1), nullptr));
  EXPECT_TRUE(cricket::WebRtcVoiceEngine::ToCodecInst(
      cricket::AudioCodec(96, "CN", 16000, 0, 1), nullptr));
  EXPECT_TRUE(cricket::WebRtcVoiceEngine::ToCodecInst(
      cricket::AudioCodec(96, "telephone-event", 8000, 0, 1), nullptr));
  // Check codecs with an id by id.
  EXPECT_TRUE(cricket::WebRtcVoiceEngine::ToCodecInst(
      cricket::AudioCodec(0, "", 8000, 0, 1), nullptr));  // PCMU
  EXPECT_TRUE(cricket::WebRtcVoiceEngine::ToCodecInst(
      cricket::AudioCodec(8, "", 8000, 0, 1), nullptr));  // PCMA
  EXPECT_TRUE(cricket::WebRtcVoiceEngine::ToCodecInst(
      cricket::AudioCodec(9, "", 8000, 0, 1), nullptr));  // G722
  EXPECT_TRUE(cricket::WebRtcVoiceEngine::ToCodecInst(
      cricket::AudioCodec(13, "", 8000, 0, 1), nullptr));  // CN
  // Check sample/bitrate matching.
  EXPECT_TRUE(cricket::WebRtcVoiceEngine::ToCodecInst(
      cricket::AudioCodec(0, "PCMU", 8000, 64000, 1), nullptr));
  // Check that bad codecs fail.
  EXPECT_FALSE(cricket::WebRtcVoiceEngine::ToCodecInst(
      cricket::AudioCodec(99, "ABCD", 0, 0, 1), nullptr));
  EXPECT_FALSE(cricket::WebRtcVoiceEngine::ToCodecInst(
      cricket::AudioCodec(88, "", 0, 0, 1), nullptr));
  EXPECT_FALSE(cricket::WebRtcVoiceEngine::ToCodecInst(
      cricket::AudioCodec(0, "", 0, 0, 2), nullptr));
  EXPECT_FALSE(cricket::WebRtcVoiceEngine::ToCodecInst(
      cricket::AudioCodec(0, "", 5000, 0, 1), nullptr));
  EXPECT_FALSE(cricket::WebRtcVoiceEngine::ToCodecInst(
      cricket::AudioCodec(0, "", 0, 5000, 1), nullptr));

  // Verify the payload id of common audio codecs, including CN, ISAC, and G722.
  // TODO(ossu): Why are the payload types of codecs with non-static payload
  // type assignments checked here? It shouldn't really matter.
  cricket::WebRtcVoiceEngine engine(
      nullptr, webrtc::MockAudioDecoderFactory::CreateUnusedFactory());
  for (std::vector<cricket::AudioCodec>::const_iterator it =
           engine.send_codecs().begin();
       it != engine.send_codecs().end(); ++it) {
    if (it->name == "CN" && it->clockrate == 16000) {
      EXPECT_EQ(105, it->id);
    } else if (it->name == "CN" && it->clockrate == 32000) {
      EXPECT_EQ(106, it->id);
    } else if (it->name == "ISAC" && it->clockrate == 16000) {
      EXPECT_EQ(103, it->id);
    } else if (it->name == "ISAC" && it->clockrate == 32000) {
      EXPECT_EQ(104, it->id);
    } else if (it->name == "G722" && it->clockrate == 8000) {
      EXPECT_EQ(9, it->id);
    } else if (it->name == "telephone-event") {
      EXPECT_EQ(126, it->id);
    } else if (it->name == "opus") {
      EXPECT_EQ(111, it->id);
      ASSERT_TRUE(it->params.find("minptime") != it->params.end());
      EXPECT_EQ("10", it->params.find("minptime")->second);
      ASSERT_TRUE(it->params.find("useinbandfec") != it->params.end());
      EXPECT_EQ("1", it->params.find("useinbandfec")->second);
    }
  }
}

// Tests that VoE supports at least 32 channels
TEST(WebRtcVoiceEngineTest, Has32Channels) {
  cricket::WebRtcVoiceEngine engine(
      nullptr, webrtc::MockAudioDecoderFactory::CreateUnusedFactory());
  webrtc::RtcEventLogNullImpl event_log;
  std::unique_ptr<webrtc::Call> call(
      webrtc::Call::Create(webrtc::Call::Config(&event_log)));

  cricket::VoiceMediaChannel* channels[32];
  int num_channels = 0;
  while (num_channels < arraysize(channels)) {
    cricket::VoiceMediaChannel* channel = engine.CreateChannel(
        call.get(), cricket::MediaConfig(), cricket::AudioOptions());
    if (!channel)
      break;
    channels[num_channels++] = channel;
  }

  int expected = arraysize(channels);
  EXPECT_EQ(expected, num_channels);

  while (num_channels > 0) {
    delete channels[--num_channels];
  }
}

// Test that we set our preferred codecs properly.
TEST(WebRtcVoiceEngineTest, SetRecvCodecs) {
  // TODO(ossu): I'm not sure of the intent of this test. It's either:
  // - Check that our builtin codecs are usable by Channel.
  // - The codecs provided by the engine is usable by Channel.
  // It does not check that the codecs in the RecvParameters are actually
  // what we sent in - though it's probably reasonable to expect so, if
  // SetRecvParameters returns true.
  // I think it will become clear once audio decoder injection is completed.
  cricket::WebRtcVoiceEngine engine(
      nullptr, webrtc::CreateBuiltinAudioDecoderFactory());
  webrtc::RtcEventLogNullImpl event_log;
  std::unique_ptr<webrtc::Call> call(
      webrtc::Call::Create(webrtc::Call::Config(&event_log)));
  cricket::WebRtcVoiceMediaChannel channel(&engine, cricket::MediaConfig(),
                                           cricket::AudioOptions(), call.get());
  cricket::AudioRecvParameters parameters;
  parameters.codecs = engine.recv_codecs();
  EXPECT_TRUE(channel.SetRecvParameters(parameters));
}
