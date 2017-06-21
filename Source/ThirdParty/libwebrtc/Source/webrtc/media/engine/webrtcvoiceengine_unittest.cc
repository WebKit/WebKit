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

#include "webrtc/api/audio_codecs/builtin_audio_decoder_factory.h"
#include "webrtc/api/audio_codecs/builtin_audio_encoder_factory.h"
#include "webrtc/base/arraysize.h"
#include "webrtc/base/byteorder.h"
#include "webrtc/base/safe_conversions.h"
#include "webrtc/call/call.h"
#include "webrtc/logging/rtc_event_log/rtc_event_log.h"
#include "webrtc/media/base/fakemediaengine.h"
#include "webrtc/media/base/fakenetworkinterface.h"
#include "webrtc/media/base/fakertp.h"
#include "webrtc/media/base/mediaconstants.h"
#include "webrtc/media/engine/fakewebrtccall.h"
#include "webrtc/media/engine/fakewebrtcvoiceengine.h"
#include "webrtc/media/engine/webrtcvoiceengine.h"
#include "webrtc/modules/audio_device/include/mock_audio_device.h"
#include "webrtc/modules/audio_processing/include/mock_audio_processing.h"
#include "webrtc/pc/channel.h"
#include "webrtc/test/field_trial.h"
#include "webrtc/test/gtest.h"
#include "webrtc/test/mock_audio_decoder_factory.h"
#include "webrtc/test/mock_audio_encoder_factory.h"
#include "webrtc/voice_engine/transmit_mixer.h"

using testing::ContainerEq;
using testing::Return;
using testing::StrictMock;

namespace {

constexpr uint32_t kMaxUnsignaledRecvStreams = 4;

const cricket::AudioCodec kPcmuCodec(0, "PCMU", 8000, 64000, 1);
const cricket::AudioCodec kIsacCodec(103, "ISAC", 16000, 32000, 1);
const cricket::AudioCodec kOpusCodec(111, "opus", 48000, 32000, 2);
const cricket::AudioCodec kG722CodecVoE(9, "G722", 16000, 64000, 1);
const cricket::AudioCodec kG722CodecSdp(9, "G722", 8000, 64000, 1);
const cricket::AudioCodec kCn8000Codec(13, "CN", 8000, 0, 1);
const cricket::AudioCodec kCn16000Codec(105, "CN", 16000, 0, 1);
const cricket::AudioCodec
    kTelephoneEventCodec1(106, "telephone-event", 8000, 0, 1);
const cricket::AudioCodec
    kTelephoneEventCodec2(107, "telephone-event", 32000, 0, 1);

const uint32_t kSsrc0 = 0;
const uint32_t kSsrc1 = 1;
const uint32_t kSsrcX = 0x99;
const uint32_t kSsrcY = 0x17;
const uint32_t kSsrcZ = 0x42;
const uint32_t kSsrcW = 0x02;
const uint32_t kSsrcs4[] = { 11, 200, 30, 44 };

constexpr int kRtpHistoryMs = 5000;

class FakeVoEWrapper : public cricket::VoEWrapper {
 public:
  explicit FakeVoEWrapper(cricket::FakeWebRtcVoiceEngine* engine)
      : cricket::VoEWrapper(engine) {
  }
};

class MockTransmitMixer : public webrtc::voe::TransmitMixer {
 public:
  MockTransmitMixer() = default;
  virtual ~MockTransmitMixer() = default;

  MOCK_METHOD1(EnableStereoChannelSwapping, void(bool enable));
};

void AdmSetupExpectations(webrtc::test::MockAudioDeviceModule* adm) {
  RTC_DCHECK(adm);
  EXPECT_CALL(*adm, AddRef()).WillOnce(Return(0));
  EXPECT_CALL(*adm, Release()).WillOnce(Return(0));
#if !defined(WEBRTC_IOS)
  EXPECT_CALL(*adm, Recording()).WillOnce(Return(false));
  EXPECT_CALL(*adm, SetRecordingChannel(webrtc::AudioDeviceModule::
      ChannelType::kChannelBoth)).WillOnce(Return(0));
#if defined(WEBRTC_WIN)
  EXPECT_CALL(*adm, SetRecordingDevice(
      testing::Matcher<webrtc::AudioDeviceModule::WindowsDeviceType>(
          webrtc::AudioDeviceModule::kDefaultCommunicationDevice)))
              .WillOnce(Return(0));
#else
  EXPECT_CALL(*adm, SetRecordingDevice(0)).WillOnce(Return(0));
#endif  // #if defined(WEBRTC_WIN)
  EXPECT_CALL(*adm, InitMicrophone()).WillOnce(Return(0));
  EXPECT_CALL(*adm, StereoRecordingIsAvailable(testing::_)).WillOnce(Return(0));
  EXPECT_CALL(*adm, SetStereoRecording(false)).WillOnce(Return(0));
  EXPECT_CALL(*adm, Playing()).WillOnce(Return(false));
#if defined(WEBRTC_WIN)
  EXPECT_CALL(*adm, SetPlayoutDevice(
      testing::Matcher<webrtc::AudioDeviceModule::WindowsDeviceType>(
          webrtc::AudioDeviceModule::kDefaultCommunicationDevice)))
              .WillOnce(Return(0));
#else
  EXPECT_CALL(*adm, SetPlayoutDevice(0)).WillOnce(Return(0));
#endif  // #if defined(WEBRTC_WIN)
  EXPECT_CALL(*adm, InitSpeaker()).WillOnce(Return(0));
  EXPECT_CALL(*adm, StereoPlayoutIsAvailable(testing::_)).WillOnce(Return(0));
  EXPECT_CALL(*adm, SetStereoPlayout(false)).WillOnce(Return(0));
#endif  // #if !defined(WEBRTC_IOS)
  EXPECT_CALL(*adm, BuiltInAECIsAvailable()).WillOnce(Return(false));
  EXPECT_CALL(*adm, BuiltInAGCIsAvailable()).WillOnce(Return(false));
  EXPECT_CALL(*adm, BuiltInNSIsAvailable()).WillOnce(Return(false));
  EXPECT_CALL(*adm, SetAGC(true)).WillOnce(Return(0));
}
}  // namespace

// Tests that our stub library "works".
TEST(WebRtcVoiceEngineTestStubLibrary, StartupShutdown) {
  StrictMock<webrtc::test::MockAudioDeviceModule> adm;
  AdmSetupExpectations(&adm);
  StrictMock<webrtc::test::MockAudioProcessing> apm;
  EXPECT_CALL(apm, ApplyConfig(testing::_));
  EXPECT_CALL(apm, SetExtraOptions(testing::_));
  EXPECT_CALL(apm, Initialize()).WillOnce(Return(0));
  EXPECT_CALL(apm, DetachAecDump());
  StrictMock<MockTransmitMixer> transmit_mixer;
  EXPECT_CALL(transmit_mixer, EnableStereoChannelSwapping(false));
  cricket::FakeWebRtcVoiceEngine voe(&apm, &transmit_mixer);
  EXPECT_FALSE(voe.IsInited());
  {
    cricket::WebRtcVoiceEngine engine(
        &adm, webrtc::MockAudioEncoderFactory::CreateUnusedFactory(),
        webrtc::MockAudioDecoderFactory::CreateUnusedFactory(), nullptr,
        new FakeVoEWrapper(&voe));
    engine.Init();
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
      : apm_gc_(*apm_.gain_control()), apm_ec_(*apm_.echo_cancellation()),
        apm_ns_(*apm_.noise_suppression()), apm_vd_(*apm_.voice_detection()),
        call_(webrtc::Call::Config(&event_log_)), voe_(&apm_, &transmit_mixer_),
        override_field_trials_(field_trials) {
    // AudioDeviceModule.
    AdmSetupExpectations(&adm_);
    // AudioProcessing.
    EXPECT_CALL(apm_, ApplyConfig(testing::_));
    EXPECT_CALL(apm_, SetExtraOptions(testing::_));
    EXPECT_CALL(apm_, Initialize()).WillOnce(Return(0));
    EXPECT_CALL(apm_, DetachAecDump());
    // Default Options.
    EXPECT_CALL(apm_ec_, Enable(true)).WillOnce(Return(0));
    EXPECT_CALL(apm_ec_, enable_metrics(true)).WillOnce(Return(0));
    EXPECT_CALL(apm_gc_, Enable(true)).WillOnce(Return(0));
    EXPECT_CALL(apm_ns_, Enable(true)).WillOnce(Return(0));
    EXPECT_CALL(apm_vd_, Enable(true)).WillOnce(Return(0));
    EXPECT_CALL(transmit_mixer_, EnableStereoChannelSwapping(false));
    // Init does not overwrite default AGC config.
    EXPECT_CALL(apm_gc_, target_level_dbfs()).WillOnce(Return(1));
    EXPECT_CALL(apm_gc_, compression_gain_db()).WillRepeatedly(Return(5));
    EXPECT_CALL(apm_gc_, is_limiter_enabled()).WillRepeatedly(Return(true));
    EXPECT_CALL(apm_gc_, set_target_level_dbfs(1)).WillOnce(Return(0));
    EXPECT_CALL(apm_gc_, set_compression_gain_db(5)).WillRepeatedly(Return(0));
    EXPECT_CALL(apm_gc_, enable_limiter(true)).WillRepeatedly(Return(0));
    // TODO(kwiberg): We should use mock factories here, but a bunch of
    // the tests here probe the specific set of codecs provided by the builtin
    // factories. Those tests should probably be moved elsewhere.
    auto encoder_factory = webrtc::CreateBuiltinAudioEncoderFactory();
    auto decoder_factory = webrtc::CreateBuiltinAudioDecoderFactory();
    engine_.reset(new cricket::WebRtcVoiceEngine(&adm_, encoder_factory,
                                                 decoder_factory, nullptr,
                                                 new FakeVoEWrapper(&voe_)));
    engine_->Init();
    send_parameters_.codecs.push_back(kPcmuCodec);
    recv_parameters_.codecs.push_back(kPcmuCodec);
    // Default Options.
    EXPECT_TRUE(IsHighPassFilterEnabled());
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
    return AddRecvStream(kSsrcX);
  }

  bool SetupSendStream() {
    if (!SetupChannel()) {
      return false;
    }
    if (!channel_->AddSendStream(cricket::StreamParams::CreateLegacy(kSsrcX))) {
      return false;
    }
    EXPECT_CALL(apm_, set_output_will_be_muted(false));
    return channel_->SetAudioSend(kSsrcX, true, nullptr, &fake_source_);
  }

  bool AddRecvStream(uint32_t ssrc) {
    EXPECT_TRUE(channel_);
    return channel_->AddRecvStream(cricket::StreamParams::CreateLegacy(ssrc));
  }

  void SetupForMultiSendStream() {
    EXPECT_TRUE(SetupSendStream());
    // Remove stream added in Setup.
    EXPECT_TRUE(call_.GetAudioSendStream(kSsrcX));
    EXPECT_TRUE(channel_->RemoveSendStream(kSsrcX));
    // Verify the channel does not exist.
    EXPECT_FALSE(call_.GetAudioSendStream(kSsrcX));
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

  void TestInsertDtmf(uint32_t ssrc, bool caller,
                      const cricket::AudioCodec& codec) {
    EXPECT_TRUE(SetupChannel());
    if (caller) {
      // If this is a caller, local description will be applied and add the
      // send stream.
      EXPECT_TRUE(channel_->AddSendStream(
          cricket::StreamParams::CreateLegacy(kSsrcX)));
    }

    // Test we can only InsertDtmf when the other side supports telephone-event.
    SetSendParameters(send_parameters_);
    SetSend(true);
    EXPECT_FALSE(channel_->CanInsertDtmf());
    EXPECT_FALSE(channel_->InsertDtmf(ssrc, 1, 111));
    send_parameters_.codecs.push_back(codec);
    SetSendParameters(send_parameters_);
    EXPECT_TRUE(channel_->CanInsertDtmf());

    if (!caller) {
      // If this is callee, there's no active send channel yet.
      EXPECT_FALSE(channel_->InsertDtmf(ssrc, 2, 123));
      EXPECT_TRUE(channel_->AddSendStream(
          cricket::StreamParams::CreateLegacy(kSsrcX)));
    }

    // Check we fail if the ssrc is invalid.
    EXPECT_FALSE(channel_->InsertDtmf(-1, 1, 111));

    // Test send.
    cricket::FakeAudioSendStream::TelephoneEvent telephone_event =
        GetSendStream(kSsrcX).GetLatestTelephoneEvent();
    EXPECT_EQ(-1, telephone_event.payload_type);
    EXPECT_TRUE(channel_->InsertDtmf(ssrc, 2, 123));
    telephone_event = GetSendStream(kSsrcX).GetLatestTelephoneEvent();
    EXPECT_EQ(codec.id, telephone_event.payload_type);
    EXPECT_EQ(codec.clockrate, telephone_event.payload_frequency);
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
    EXPECT_EQ(expected_bitrate, GetCodecBitrate(kSsrcX));
  }

  // Sets the per-stream maximum bitrate limit for the specified SSRC.
  bool SetMaxBitrateForStream(int32_t ssrc, int bitrate) {
    webrtc::RtpParameters parameters = channel_->GetRtpSendParameters(ssrc);
    EXPECT_EQ(1UL, parameters.encodings.size());

    parameters.encodings[0].max_bitrate_bps = rtc::Optional<int>(bitrate);
    return channel_->SetRtpSendParameters(ssrc, parameters);
  }

  void SetGlobalMaxBitrate(const cricket::AudioCodec& codec, int bitrate) {
    cricket::AudioSendParameters send_parameters;
    send_parameters.codecs.push_back(codec);
    send_parameters.max_bandwidth_bps = bitrate;
    SetSendParameters(send_parameters);
  }

  void CheckSendCodecBitrate(int32_t ssrc,
                             const char expected_name[],
                             int expected_bitrate) {
    const auto& spec = GetSendStreamConfig(ssrc).send_codec_spec;
    EXPECT_EQ(expected_name, spec->format.name);
    EXPECT_EQ(expected_bitrate, spec->target_bitrate_bps);
  }

  rtc::Optional<int> GetCodecBitrate(int32_t ssrc) {
    return GetSendStreamConfig(ssrc).send_codec_spec->target_bitrate_bps;
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
    EXPECT_TRUE(SetMaxBitrateForStream(kSsrcX, -1));

    // Attempt to set the requested bitrate limits.
    SetGlobalMaxBitrate(codec, global_max);
    EXPECT_EQ(expected_result, SetMaxBitrateForStream(kSsrcX, stream_max));

    // Verify that reading back the parameters gives results
    // consistent with the Set() result.
    webrtc::RtpParameters resulting_parameters =
        channel_->GetRtpSendParameters(kSsrcX);
    EXPECT_EQ(1UL, resulting_parameters.encodings.size());
    EXPECT_EQ(expected_result ? stream_max : -1,
              resulting_parameters.encodings[0].max_bitrate_bps);

    // Verify that the codec settings have the expected bitrate.
    EXPECT_EQ(expected_codec_bitrate, GetCodecBitrate(kSsrcX));
  }

  void SetSendCodecsShouldWorkForBitrates(const char* min_bitrate_kbps,
                                          int expected_min_bitrate_bps,
                                          const char* start_bitrate_kbps,
                                          int expected_start_bitrate_bps,
                                          const char* max_bitrate_kbps,
                                          int expected_max_bitrate_bps) {
    EXPECT_TRUE(SetupSendStream());
    auto& codecs = send_parameters_.codecs;
    codecs.clear();
    codecs.push_back(kOpusCodec);
    codecs[0].params[cricket::kCodecParamMinBitrate] = min_bitrate_kbps;
    codecs[0].params[cricket::kCodecParamStartBitrate] = start_bitrate_kbps;
    codecs[0].params[cricket::kCodecParamMaxBitrate] = max_bitrate_kbps;
    SetSendParameters(send_parameters_);

    EXPECT_EQ(expected_min_bitrate_bps,
              call_.GetConfig().bitrate_config.min_bitrate_bps);
    EXPECT_EQ(expected_start_bitrate_bps,
              call_.GetConfig().bitrate_config.start_bitrate_bps);
    EXPECT_EQ(expected_max_bitrate_bps,
              call_.GetConfig().bitrate_config.max_bitrate_bps);
  }

  void TestSetSendRtpHeaderExtensions(const std::string& ext) {
    EXPECT_TRUE(SetupSendStream());

    // Ensure extensions are off by default.
    EXPECT_EQ(0u, GetSendStreamConfig(kSsrcX).rtp.extensions.size());

    // Ensure unknown extensions won't cause an error.
    send_parameters_.extensions.push_back(
        webrtc::RtpExtension("urn:ietf:params:unknownextention", 1));
    SetSendParameters(send_parameters_);
    EXPECT_EQ(0u, GetSendStreamConfig(kSsrcX).rtp.extensions.size());

    // Ensure extensions stay off with an empty list of headers.
    send_parameters_.extensions.clear();
    SetSendParameters(send_parameters_);
    EXPECT_EQ(0u, GetSendStreamConfig(kSsrcX).rtp.extensions.size());

    // Ensure extension is set properly.
    const int id = 1;
    send_parameters_.extensions.push_back(webrtc::RtpExtension(ext, id));
    SetSendParameters(send_parameters_);
    EXPECT_EQ(1u, GetSendStreamConfig(kSsrcX).rtp.extensions.size());
    EXPECT_EQ(ext, GetSendStreamConfig(kSsrcX).rtp.extensions[0].uri);
    EXPECT_EQ(id, GetSendStreamConfig(kSsrcX).rtp.extensions[0].id);

    // Ensure extension is set properly on new stream.
    EXPECT_TRUE(channel_->AddSendStream(
        cricket::StreamParams::CreateLegacy(kSsrcY)));
    EXPECT_NE(call_.GetAudioSendStream(kSsrcX),
              call_.GetAudioSendStream(kSsrcY));
    EXPECT_EQ(1u, GetSendStreamConfig(kSsrcY).rtp.extensions.size());
    EXPECT_EQ(ext, GetSendStreamConfig(kSsrcY).rtp.extensions[0].uri);
    EXPECT_EQ(id, GetSendStreamConfig(kSsrcY).rtp.extensions[0].id);

    // Ensure all extensions go back off with an empty list.
    send_parameters_.codecs.push_back(kPcmuCodec);
    send_parameters_.extensions.clear();
    SetSendParameters(send_parameters_);
    EXPECT_EQ(0u, GetSendStreamConfig(kSsrcX).rtp.extensions.size());
    EXPECT_EQ(0u, GetSendStreamConfig(kSsrcY).rtp.extensions.size());
  }

  void TestSetRecvRtpHeaderExtensions(const std::string& ext) {
    EXPECT_TRUE(SetupRecvStream());

    // Ensure extensions are off by default.
    EXPECT_EQ(0u, GetRecvStreamConfig(kSsrcX).rtp.extensions.size());

    // Ensure unknown extensions won't cause an error.
    recv_parameters_.extensions.push_back(
        webrtc::RtpExtension("urn:ietf:params:unknownextention", 1));
    EXPECT_TRUE(channel_->SetRecvParameters(recv_parameters_));
    EXPECT_EQ(0u, GetRecvStreamConfig(kSsrcX).rtp.extensions.size());

    // Ensure extensions stay off with an empty list of headers.
    recv_parameters_.extensions.clear();
    EXPECT_TRUE(channel_->SetRecvParameters(recv_parameters_));
    EXPECT_EQ(0u, GetRecvStreamConfig(kSsrcX).rtp.extensions.size());

    // Ensure extension is set properly.
    const int id = 2;
    recv_parameters_.extensions.push_back(webrtc::RtpExtension(ext, id));
    EXPECT_TRUE(channel_->SetRecvParameters(recv_parameters_));
    EXPECT_EQ(1u, GetRecvStreamConfig(kSsrcX).rtp.extensions.size());
    EXPECT_EQ(ext, GetRecvStreamConfig(kSsrcX).rtp.extensions[0].uri);
    EXPECT_EQ(id, GetRecvStreamConfig(kSsrcX).rtp.extensions[0].id);

    // Ensure extension is set properly on new stream.
    EXPECT_TRUE(AddRecvStream(kSsrcY));
    EXPECT_NE(call_.GetAudioReceiveStream(kSsrcX),
              call_.GetAudioReceiveStream(kSsrcY));
    EXPECT_EQ(1u, GetRecvStreamConfig(kSsrcY).rtp.extensions.size());
    EXPECT_EQ(ext, GetRecvStreamConfig(kSsrcY).rtp.extensions[0].uri);
    EXPECT_EQ(id, GetRecvStreamConfig(kSsrcY).rtp.extensions[0].id);

    // Ensure all extensions go back off with an empty list.
    recv_parameters_.extensions.clear();
    EXPECT_TRUE(channel_->SetRecvParameters(recv_parameters_));
    EXPECT_EQ(0u, GetRecvStreamConfig(kSsrcX).rtp.extensions.size());
    EXPECT_EQ(0u, GetRecvStreamConfig(kSsrcY).rtp.extensions.size());
  }

  webrtc::AudioSendStream::Stats GetAudioSendStreamStats() const {
    webrtc::AudioSendStream::Stats stats;
    stats.local_ssrc = 12;
    stats.bytes_sent = 345;
    stats.packets_sent = 678;
    stats.packets_lost = 9012;
    stats.fraction_lost = 34.56f;
    stats.codec_name = "codec_name_send";
    stats.codec_payload_type = rtc::Optional<int>(42);
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
    stats.residual_echo_likelihood_recent_max = 0.6f;
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
    EXPECT_EQ(info.codec_payload_type, stats.codec_payload_type);
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
    EXPECT_EQ(info.residual_echo_likelihood_recent_max,
              stats.residual_echo_likelihood_recent_max);
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
    stats.codec_payload_type = rtc::Optional<int>(42);
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
    EXPECT_EQ(info.codec_payload_type, stats.codec_payload_type);
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
  void VerifyVoiceSendRecvCodecs(const cricket::VoiceMediaInfo& info) const {
    EXPECT_EQ(send_parameters_.codecs.size(), info.send_codecs.size());
    for (const cricket::AudioCodec& codec : send_parameters_.codecs) {
      ASSERT_EQ(info.send_codecs.count(codec.id), 1U);
      EXPECT_EQ(info.send_codecs.find(codec.id)->second,
                codec.ToCodecParameters());
    }
    EXPECT_EQ(recv_parameters_.codecs.size(), info.receive_codecs.size());
    for (const cricket::AudioCodec& codec : recv_parameters_.codecs) {
      ASSERT_EQ(info.receive_codecs.count(codec.id), 1U);
      EXPECT_EQ(info.receive_codecs.find(codec.id)->second,
                codec.ToCodecParameters());
    }
  }

  bool IsHighPassFilterEnabled() {
    return engine_->GetApmConfigForTest().high_pass_filter.enabled;
  }

 protected:
  StrictMock<webrtc::test::MockAudioDeviceModule> adm_;
  StrictMock<webrtc::test::MockAudioProcessing> apm_;
  webrtc::test::MockGainControl& apm_gc_;
  webrtc::test::MockEchoCancellation& apm_ec_;
  webrtc::test::MockNoiseSuppression& apm_ns_;
  webrtc::test::MockVoiceDetection& apm_vd_;
  StrictMock<MockTransmitMixer> transmit_mixer_;
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
      channel_->AddSendStream(cricket::StreamParams::CreateLegacy(kSsrcX)));
  const webrtc::AudioSendStream::Config& config = GetSendStreamConfig(kSsrcX);
  EXPECT_EQ(kSsrcX, config.rtp.ssrc);
  EXPECT_EQ("", config.rtp.c_name);
  EXPECT_EQ(0u, config.rtp.extensions.size());
  EXPECT_EQ(static_cast<cricket::WebRtcVoiceMediaChannel*>(channel_),
            config.send_transport);
}

// Test that we can add a receive stream and that it has the correct defaults.
TEST_F(WebRtcVoiceEngineTestFake, CreateRecvStream) {
  EXPECT_TRUE(SetupChannel());
  EXPECT_TRUE(AddRecvStream(kSsrcX));
  const webrtc::AudioReceiveStream::Config& config =
      GetRecvStreamConfig(kSsrcX);
  EXPECT_EQ(kSsrcX, config.rtp.remote_ssrc);
  EXPECT_EQ(0xFA17FA17, config.rtp.local_ssrc);
  EXPECT_FALSE(config.rtp.transport_cc);
  EXPECT_EQ(0u, config.rtp.extensions.size());
  EXPECT_EQ(static_cast<cricket::WebRtcVoiceMediaChannel*>(channel_),
            config.rtcp_send_transport);
  EXPECT_EQ("", config.sync_group);
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

// Test that we set our inbound codecs properly, including changing PT.
TEST_F(WebRtcVoiceEngineTestFake, SetRecvCodecs) {
  EXPECT_TRUE(SetupChannel());
  cricket::AudioRecvParameters parameters;
  parameters.codecs.push_back(kIsacCodec);
  parameters.codecs.push_back(kPcmuCodec);
  parameters.codecs.push_back(kTelephoneEventCodec1);
  parameters.codecs.push_back(kTelephoneEventCodec2);
  parameters.codecs[0].id = 106;  // collide with existing CN 32k
  parameters.codecs[2].id = 126;
  EXPECT_TRUE(channel_->SetRecvParameters(parameters));
  EXPECT_TRUE(AddRecvStream(kSsrcX));
  EXPECT_THAT(GetRecvStreamConfig(kSsrcX).decoder_map,
              (ContainerEq<std::map<int, webrtc::SdpAudioFormat>>(
                  {{0, {"PCMU", 8000, 1}},
                   {106, {"ISAC", 16000, 1}},
                   {126, {"telephone-event", 8000, 1}},
                   {107, {"telephone-event", 32000, 1}}})));
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
  EXPECT_TRUE(AddRecvStream(kSsrcX));
  EXPECT_THAT(GetRecvStreamConfig(kSsrcX).decoder_map,
              (ContainerEq<std::map<int, webrtc::SdpAudioFormat>>(
                  {{0, {"PCMU", 8000, 1}},
                   {103, {"ISAC", 16000, 1}},
                   {111, {"opus", 48000, 2}}})));
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
  EXPECT_TRUE(AddRecvStream(kSsrcX));
  EXPECT_THAT(GetRecvStreamConfig(kSsrcX).decoder_map,
              (ContainerEq<std::map<int, webrtc::SdpAudioFormat>>(
                  {{0, {"PCMU", 8000, 1}},
                   {103, {"ISAC", 16000, 1}},
                   {111, {"opus", 48000, 2, {{"stereo", "0"}}}}})));
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
  EXPECT_TRUE(AddRecvStream(kSsrcX));
  EXPECT_THAT(GetRecvStreamConfig(kSsrcX).decoder_map,
              (ContainerEq<std::map<int, webrtc::SdpAudioFormat>>(
                  {{0, {"PCMU", 8000, 1}},
                   {103, {"ISAC", 16000, 1}},
                   {111, {"opus", 48000, 2, {{"stereo", "1"}}}}})));
}

// Test that changes to recv codecs are applied to all streams.
TEST_F(WebRtcVoiceEngineTestFake, SetRecvCodecsWithMultipleStreams) {
  EXPECT_TRUE(SetupChannel());
  cricket::AudioRecvParameters parameters;
  parameters.codecs.push_back(kIsacCodec);
  parameters.codecs.push_back(kPcmuCodec);
  parameters.codecs.push_back(kTelephoneEventCodec1);
  parameters.codecs.push_back(kTelephoneEventCodec2);
  parameters.codecs[0].id = 106;  // collide with existing CN 32k
  parameters.codecs[2].id = 126;
  EXPECT_TRUE(channel_->SetRecvParameters(parameters));
  for (const auto& ssrc : {kSsrcX, kSsrcY}) {
    EXPECT_TRUE(AddRecvStream(ssrc));
    EXPECT_THAT(GetRecvStreamConfig(ssrc).decoder_map,
                (ContainerEq<std::map<int, webrtc::SdpAudioFormat>>(
                    {{0, {"PCMU", 8000, 1}},
                     {106, {"ISAC", 16000, 1}},
                     {126, {"telephone-event", 8000, 1}},
                     {107, {"telephone-event", 32000, 1}}})));
  }
}

TEST_F(WebRtcVoiceEngineTestFake, SetRecvCodecsAfterAddingStreams) {
  EXPECT_TRUE(SetupRecvStream());
  cricket::AudioRecvParameters parameters;
  parameters.codecs.push_back(kIsacCodec);
  parameters.codecs[0].id = 106;  // collide with existing CN 32k
  EXPECT_TRUE(channel_->SetRecvParameters(parameters));

  const auto& dm = GetRecvStreamConfig(kSsrcX).decoder_map;
  ASSERT_EQ(1, dm.count(106));
  EXPECT_EQ(webrtc::SdpAudioFormat("isac", 16000, 1), dm.at(106));
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

  // Remapping a payload type to a different codec should fail.
  parameters.codecs[0] = kOpusCodec;
  parameters.codecs[0].id = kIsacCodec.id;
  EXPECT_FALSE(channel_->SetRecvParameters(parameters));
  EXPECT_TRUE(GetRecvStream(kSsrcX).started());
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
  EXPECT_TRUE(GetRecvStream(kSsrcX).started());
}

// Test that we accept adding the same codec with a different payload type.
// See: https://bugs.chromium.org/p/webrtc/issues/detail?id=5847
TEST_F(WebRtcVoiceEngineTestFake, ChangeRecvCodecPayloadType) {
  EXPECT_TRUE(SetupRecvStream());
  cricket::AudioRecvParameters parameters;
  parameters.codecs.push_back(kIsacCodec);
  EXPECT_TRUE(channel_->SetRecvParameters(parameters));

  ++parameters.codecs[0].id;
  EXPECT_TRUE(channel_->SetRecvParameters(parameters));
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

  // opus, default bitrate == 32000 in mono.
  TestMaxSendBandwidth(kOpusCodec, -1, true, 32000);
}

TEST_F(WebRtcVoiceEngineTestFake, SetMaxSendBandwidthMultiRateAsCaller) {
  EXPECT_TRUE(SetupSendStream());

  // ISAC, default bitrate == 32000.
  TestMaxSendBandwidth(kIsacCodec, 16000, true, 16000);
  // Rates above the max (56000) should be capped.
  TestMaxSendBandwidth(kIsacCodec, 100000, true, 32000);

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
      cricket::StreamParams::CreateLegacy(kSsrcX)));

  EXPECT_EQ(kDesiredBitrate, GetCodecBitrate(kSsrcX));
}

// Test that bitrate cannot be set for CBR codecs.
// Bitrate is ignored if it is higher than the fixed bitrate.
// Bitrate less then the fixed bitrate is an error.
TEST_F(WebRtcVoiceEngineTestFake, SetMaxSendBandwidthCbr) {
  EXPECT_TRUE(SetupSendStream());

  // PCMU, default bitrate == 64000.
  SetSendParameters(send_parameters_);
  EXPECT_EQ(64000, GetCodecBitrate(kSsrcX));

  send_parameters_.max_bandwidth_bps = 128000;
  SetSendParameters(send_parameters_);
  EXPECT_EQ(64000, GetCodecBitrate(kSsrcX));

  send_parameters_.max_bandwidth_bps = 128;
  EXPECT_FALSE(channel_->SetSendParameters(send_parameters_));
  EXPECT_EQ(64000, GetCodecBitrate(kSsrcX));
}

// Test that the per-stream bitrate limit and the global
// bitrate limit both apply.
TEST_F(WebRtcVoiceEngineTestFake, SetMaxBitratePerStream) {
  EXPECT_TRUE(SetupSendStream());

  // opus, default bitrate == 32000.
  SetAndExpectMaxBitrate(kOpusCodec, 0, 0, true, 32000);
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
      channel_->GetRtpSendParameters(kSsrcX);
  EXPECT_EQ(0, nonexistent_parameters.encodings.size());

  nonexistent_parameters.encodings.push_back(webrtc::RtpEncodingParameters());
  EXPECT_FALSE(channel_->SetRtpSendParameters(kSsrcX, nonexistent_parameters));
}

TEST_F(WebRtcVoiceEngineTestFake,
       CannotSetRtpSendParametersWithIncorrectNumberOfEncodings) {
  // This test verifies that setting RtpParameters succeeds only if
  // the structure contains exactly one encoding.
  // TODO(skvlad): Update this test when we start supporting setting parameters
  // for each encoding individually.

  EXPECT_TRUE(SetupSendStream());
  webrtc::RtpParameters parameters = channel_->GetRtpSendParameters(kSsrcX);
  // Two or more encodings should result in failure.
  parameters.encodings.push_back(webrtc::RtpEncodingParameters());
  EXPECT_FALSE(channel_->SetRtpSendParameters(kSsrcX, parameters));
  // Zero encodings should also fail.
  parameters.encodings.clear();
  EXPECT_FALSE(channel_->SetRtpSendParameters(kSsrcX, parameters));
}

// Changing the SSRC through RtpParameters is not allowed.
TEST_F(WebRtcVoiceEngineTestFake, CannotSetSsrcInRtpSendParameters) {
  EXPECT_TRUE(SetupSendStream());
  webrtc::RtpParameters parameters = channel_->GetRtpSendParameters(kSsrcX);
  parameters.encodings[0].ssrc = rtc::Optional<uint32_t>(0xdeadbeef);
  EXPECT_FALSE(channel_->SetRtpSendParameters(kSsrcX, parameters));
}

// Test that a stream will not be sending if its encoding is made
// inactive through SetRtpSendParameters.
TEST_F(WebRtcVoiceEngineTestFake, SetRtpParametersEncodingsActive) {
  EXPECT_TRUE(SetupSendStream());
  SetSend(true);
  EXPECT_TRUE(GetSendStream(kSsrcX).IsSending());
  // Get current parameters and change "active" to false.
  webrtc::RtpParameters parameters = channel_->GetRtpSendParameters(kSsrcX);
  ASSERT_EQ(1u, parameters.encodings.size());
  ASSERT_TRUE(parameters.encodings[0].active);
  parameters.encodings[0].active = false;
  EXPECT_TRUE(channel_->SetRtpSendParameters(kSsrcX, parameters));
  EXPECT_FALSE(GetSendStream(kSsrcX).IsSending());

  // Now change it back to active and verify we resume sending.
  parameters.encodings[0].active = true;
  EXPECT_TRUE(channel_->SetRtpSendParameters(kSsrcX, parameters));
  EXPECT_TRUE(GetSendStream(kSsrcX).IsSending());
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
  SetGlobalMaxBitrate(kOpusCodec, 32000);
  EXPECT_TRUE(SetMaxBitrateForStream(kSsrcs4[0], 24000));
  EXPECT_TRUE(SetMaxBitrateForStream(kSsrcs4[1], 48000));
  EXPECT_TRUE(SetMaxBitrateForStream(kSsrcs4[2], -1));

  EXPECT_EQ(24000, GetCodecBitrate(kSsrcs4[0]));
  EXPECT_EQ(32000, GetCodecBitrate(kSsrcs4[1]));
  EXPECT_EQ(32000, GetCodecBitrate(kSsrcs4[2]));

  // Remove the global cap; the streams should switch to their respective
  // maximums (or remain unchanged if there was no other limit on them.)
  SetGlobalMaxBitrate(kOpusCodec, -1);
  EXPECT_EQ(24000, GetCodecBitrate(kSsrcs4[0]));
  EXPECT_EQ(48000, GetCodecBitrate(kSsrcs4[1]));
  EXPECT_EQ(32000, GetCodecBitrate(kSsrcs4[2]));
}

// Test that GetRtpSendParameters returns the currently configured codecs.
TEST_F(WebRtcVoiceEngineTestFake, GetRtpSendParametersCodecs) {
  EXPECT_TRUE(SetupSendStream());
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kIsacCodec);
  parameters.codecs.push_back(kPcmuCodec);
  SetSendParameters(parameters);

  webrtc::RtpParameters rtp_parameters = channel_->GetRtpSendParameters(kSsrcX);
  ASSERT_EQ(2u, rtp_parameters.codecs.size());
  EXPECT_EQ(kIsacCodec.ToCodecParameters(), rtp_parameters.codecs[0]);
  EXPECT_EQ(kPcmuCodec.ToCodecParameters(), rtp_parameters.codecs[1]);
}

// Test that GetRtpSendParameters returns an SSRC.
TEST_F(WebRtcVoiceEngineTestFake, GetRtpSendParametersSsrc) {
  EXPECT_TRUE(SetupSendStream());
  webrtc::RtpParameters rtp_parameters = channel_->GetRtpSendParameters(kSsrcX);
  ASSERT_EQ(1u, rtp_parameters.encodings.size());
  EXPECT_EQ(kSsrcX, rtp_parameters.encodings[0].ssrc);
}

// Test that if we set/get parameters multiple times, we get the same results.
TEST_F(WebRtcVoiceEngineTestFake, SetAndGetRtpSendParameters) {
  EXPECT_TRUE(SetupSendStream());
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kIsacCodec);
  parameters.codecs.push_back(kPcmuCodec);
  SetSendParameters(parameters);

  webrtc::RtpParameters initial_params = channel_->GetRtpSendParameters(kSsrcX);

  // We should be able to set the params we just got.
  EXPECT_TRUE(channel_->SetRtpSendParameters(kSsrcX, initial_params));

  // ... And this shouldn't change the params returned by GetRtpSendParameters.
  webrtc::RtpParameters new_params = channel_->GetRtpSendParameters(kSsrcX);
  EXPECT_EQ(initial_params, channel_->GetRtpSendParameters(kSsrcX));
}

// Test that max_bitrate_bps in send stream config gets updated correctly when
// SetRtpSendParameters is called.
TEST_F(WebRtcVoiceEngineTestFake, SetRtpSendParameterUpdatesMaxBitrate) {
  webrtc::test::ScopedFieldTrials override_field_trials(
      "WebRTC-Audio-SendSideBwe/Enabled/");
  EXPECT_TRUE(SetupSendStream());
  cricket::AudioSendParameters send_parameters;
  send_parameters.codecs.push_back(kOpusCodec);
  SetSendParameters(send_parameters);

  webrtc::RtpParameters rtp_parameters = channel_->GetRtpSendParameters(kSsrcX);
  // Expect empty on parameters.encodings[0].max_bitrate_bps;
  EXPECT_FALSE(rtp_parameters.encodings[0].max_bitrate_bps);

  constexpr int kMaxBitrateBps = 6000;
  rtp_parameters.encodings[0].max_bitrate_bps =
      rtc::Optional<int>(kMaxBitrateBps);
  EXPECT_TRUE(channel_->SetRtpSendParameters(kSsrcX, rtp_parameters));

  const int max_bitrate = GetSendStreamConfig(kSsrcX).max_bitrate_bps;
  EXPECT_EQ(max_bitrate, kMaxBitrateBps);
}

// Test that GetRtpReceiveParameters returns the currently configured codecs.
TEST_F(WebRtcVoiceEngineTestFake, GetRtpReceiveParametersCodecs) {
  EXPECT_TRUE(SetupRecvStream());
  cricket::AudioRecvParameters parameters;
  parameters.codecs.push_back(kIsacCodec);
  parameters.codecs.push_back(kPcmuCodec);
  EXPECT_TRUE(channel_->SetRecvParameters(parameters));

  webrtc::RtpParameters rtp_parameters =
      channel_->GetRtpReceiveParameters(kSsrcX);
  ASSERT_EQ(2u, rtp_parameters.codecs.size());
  EXPECT_EQ(kIsacCodec.ToCodecParameters(), rtp_parameters.codecs[0]);
  EXPECT_EQ(kPcmuCodec.ToCodecParameters(), rtp_parameters.codecs[1]);
}

// Test that GetRtpReceiveParameters returns an SSRC.
TEST_F(WebRtcVoiceEngineTestFake, GetRtpReceiveParametersSsrc) {
  EXPECT_TRUE(SetupRecvStream());
  webrtc::RtpParameters rtp_parameters =
      channel_->GetRtpReceiveParameters(kSsrcX);
  ASSERT_EQ(1u, rtp_parameters.encodings.size());
  EXPECT_EQ(kSsrcX, rtp_parameters.encodings[0].ssrc);
}

// Test that if we set/get parameters multiple times, we get the same results.
TEST_F(WebRtcVoiceEngineTestFake, SetAndGetRtpReceiveParameters) {
  EXPECT_TRUE(SetupRecvStream());
  cricket::AudioRecvParameters parameters;
  parameters.codecs.push_back(kIsacCodec);
  parameters.codecs.push_back(kPcmuCodec);
  EXPECT_TRUE(channel_->SetRecvParameters(parameters));

  webrtc::RtpParameters initial_params =
      channel_->GetRtpReceiveParameters(kSsrcX);

  // We should be able to set the params we just got.
  EXPECT_TRUE(channel_->SetRtpReceiveParameters(kSsrcX, initial_params));

  // ... And this shouldn't change the params returned by
  // GetRtpReceiveParameters.
  webrtc::RtpParameters new_params = channel_->GetRtpReceiveParameters(kSsrcX);
  EXPECT_EQ(initial_params, channel_->GetRtpReceiveParameters(kSsrcX));
}

// Test that GetRtpReceiveParameters returns parameters correctly when SSRCs
// aren't signaled. It should return an empty "RtpEncodingParameters" when
// configured to receive an unsignaled stream and no packets have been received
// yet, and start returning the SSRC once a packet has been received.
TEST_F(WebRtcVoiceEngineTestFake, GetRtpReceiveParametersWithUnsignaledSsrc) {
  ASSERT_TRUE(SetupChannel());
  // Call necessary methods to configure receiving a default stream as
  // soon as it arrives.
  cricket::AudioRecvParameters parameters;
  parameters.codecs.push_back(kIsacCodec);
  parameters.codecs.push_back(kPcmuCodec);
  EXPECT_TRUE(channel_->SetRecvParameters(parameters));

  // Call GetRtpReceiveParameters before configured to receive an unsignaled
  // stream. Should return nothing.
  EXPECT_EQ(webrtc::RtpParameters(), channel_->GetRtpReceiveParameters(0));

  // Set a sink for an unsignaled stream.
  std::unique_ptr<FakeAudioSink> fake_sink(new FakeAudioSink());
  // Value of "0" means "unsignaled stream".
  channel_->SetRawAudioSink(0, std::move(fake_sink));

  // Call GetRtpReceiveParameters before the SSRC is known. Value of "0"
  // in this method means "unsignaled stream".
  webrtc::RtpParameters rtp_parameters = channel_->GetRtpReceiveParameters(0);
  ASSERT_EQ(1u, rtp_parameters.encodings.size());
  EXPECT_FALSE(rtp_parameters.encodings[0].ssrc);

  // Receive PCMU packet (SSRC=1).
  DeliverPacket(kPcmuFrame, sizeof(kPcmuFrame));

  // The |ssrc| member should still be unset.
  rtp_parameters = channel_->GetRtpReceiveParameters(0);
  ASSERT_EQ(1u, rtp_parameters.encodings.size());
  EXPECT_FALSE(rtp_parameters.encodings[0].ssrc);
}

// Test that we apply codecs properly.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecs) {
  EXPECT_TRUE(SetupSendStream());
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kIsacCodec);
  parameters.codecs.push_back(kPcmuCodec);
  parameters.codecs.push_back(kCn8000Codec);
  parameters.codecs[0].id = 96;
  parameters.codecs[0].bitrate = 22000;
  SetSendParameters(parameters);
  const auto& send_codec_spec = *GetSendStreamConfig(kSsrcX).send_codec_spec;
  EXPECT_EQ(96, send_codec_spec.payload_type);
  EXPECT_EQ(22000, send_codec_spec.target_bitrate_bps);
  EXPECT_STRCASEEQ("ISAC", send_codec_spec.format.name.c_str());
  EXPECT_NE(send_codec_spec.format.clockrate_hz, 8000);
  EXPECT_EQ(rtc::Optional<int>(), send_codec_spec.cng_payload_type);
  EXPECT_FALSE(channel_->CanInsertDtmf());
}

// Test that WebRtcVoiceEngine reconfigures, rather than recreates its
// AudioSendStream.
TEST_F(WebRtcVoiceEngineTestFake, DontRecreateSendStream) {
  EXPECT_TRUE(SetupSendStream());
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kIsacCodec);
  parameters.codecs.push_back(kPcmuCodec);
  parameters.codecs.push_back(kCn8000Codec);
  parameters.codecs[0].id = 96;
  parameters.codecs[0].bitrate = 48000;
  const int initial_num = call_.GetNumCreatedSendStreams();
  SetSendParameters(parameters);
  EXPECT_EQ(initial_num, call_.GetNumCreatedSendStreams());
  // Calling SetSendCodec again with same codec which is already set.
  // In this case media channel shouldn't send codec to VoE.
  SetSendParameters(parameters);
  EXPECT_EQ(initial_num, call_.GetNumCreatedSendStreams());
}

// TODO(ossu): Revisit if these tests need to be here, now that these kinds of
// tests should be available in AudioEncoderOpusTest.

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

// Test that with bitrate=0 and no stereo, bitrate is 32000.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecOpusGood0BitrateNoStereo) {
  EXPECT_TRUE(SetupSendStream());
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kOpusCodec);
  parameters.codecs[0].bitrate = 0;
  SetSendParameters(parameters);
  CheckSendCodecBitrate(kSsrcX, "opus", 32000);
}

// Test that with bitrate=0 and stereo=0, bitrate is 32000.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecOpusGood0Bitrate0Stereo) {
  EXPECT_TRUE(SetupSendStream());
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kOpusCodec);
  parameters.codecs[0].bitrate = 0;
  parameters.codecs[0].params["stereo"] = "0";
  SetSendParameters(parameters);
  CheckSendCodecBitrate(kSsrcX, "opus", 32000);
}

// Test that with bitrate=invalid and stereo=0, bitrate is 32000.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecOpusGoodXBitrate0Stereo) {
  EXPECT_TRUE(SetupSendStream());
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kOpusCodec);
  parameters.codecs[0].params["stereo"] = "0";
  // bitrate that's out of the range between 6000 and 510000 will be clamped.
  parameters.codecs[0].bitrate = 5999;
  SetSendParameters(parameters);
  CheckSendCodecBitrate(kSsrcX, "opus", 6000);

  parameters.codecs[0].bitrate = 510001;
  SetSendParameters(parameters);
  CheckSendCodecBitrate(kSsrcX, "opus", 510000);
}

// Test that with bitrate=0 and stereo=1, bitrate is 64000.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecOpusGood0Bitrate1Stereo) {
  EXPECT_TRUE(SetupSendStream());
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kOpusCodec);
  parameters.codecs[0].bitrate = 0;
  parameters.codecs[0].params["stereo"] = "1";
  SetSendParameters(parameters);
  CheckSendCodecBitrate(kSsrcX, "opus", 64000);
}

// Test that with bitrate=invalid and stereo=1, bitrate is 64000.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecOpusGoodXBitrate1Stereo) {
  EXPECT_TRUE(SetupSendStream());
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kOpusCodec);
  parameters.codecs[0].params["stereo"] = "1";
  // bitrate that's out of the range between 6000 and 510000 will be clamped.
  parameters.codecs[0].bitrate = 5999;
  SetSendParameters(parameters);
  CheckSendCodecBitrate(kSsrcX, "opus", 6000);

  parameters.codecs[0].bitrate = 510001;
  SetSendParameters(parameters);
  CheckSendCodecBitrate(kSsrcX, "opus", 510000);
}

// Test that with bitrate=N and stereo unset, bitrate is N.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecOpusGoodNBitrateNoStereo) {
  EXPECT_TRUE(SetupSendStream());
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kOpusCodec);
  parameters.codecs[0].bitrate = 96000;
  SetSendParameters(parameters);
  const auto& spec = *GetSendStreamConfig(kSsrcX).send_codec_spec;
  EXPECT_EQ(111, spec.payload_type);
  EXPECT_EQ(96000, spec.target_bitrate_bps);
  EXPECT_EQ("opus", spec.format.name);
  EXPECT_EQ(2, spec.format.num_channels);
  EXPECT_EQ(48000, spec.format.clockrate_hz);
}

// Test that with bitrate=N and stereo=0, bitrate is N.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecOpusGoodNBitrate0Stereo) {
  EXPECT_TRUE(SetupSendStream());
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kOpusCodec);
  parameters.codecs[0].bitrate = 30000;
  parameters.codecs[0].params["stereo"] = "0";
  SetSendParameters(parameters);
  CheckSendCodecBitrate(kSsrcX, "opus", 30000);
}

// Test that with bitrate=N and without any parameters, bitrate is N.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecOpusGoodNBitrateNoParameters) {
  EXPECT_TRUE(SetupSendStream());
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kOpusCodec);
  parameters.codecs[0].bitrate = 30000;
  SetSendParameters(parameters);
  CheckSendCodecBitrate(kSsrcX, "opus", 30000);
}

// Test that with bitrate=N and stereo=1, bitrate is N.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecOpusGoodNBitrate1Stereo) {
  EXPECT_TRUE(SetupSendStream());
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kOpusCodec);
  parameters.codecs[0].bitrate = 30000;
  parameters.codecs[0].params["stereo"] = "1";
  SetSendParameters(parameters);
  CheckSendCodecBitrate(kSsrcX, "opus", 30000);
}

TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecsWithBitrates) {
  SetSendCodecsShouldWorkForBitrates("100", 100000, "150", 150000, "200",
                                     200000);
}

TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecsWithHighMaxBitrate) {
  SetSendCodecsShouldWorkForBitrates("", 0, "", -1, "10000", 10000000);
}

TEST_F(WebRtcVoiceEngineTestFake,
       SetSendCodecsWithoutBitratesUsesCorrectDefaults) {
  SetSendCodecsShouldWorkForBitrates("", 0, "", -1, "", -1);
}

TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecsCapsMinAndStartBitrate) {
  SetSendCodecsShouldWorkForBitrates("-1", 0, "-100", -1, "", -1);
}

TEST_F(WebRtcVoiceEngineTestFake,
       SetMaxSendBandwidthForAudioDoesntAffectBwe) {
  SetSendCodecsShouldWorkForBitrates("100", 100000, "150", 150000, "200",
                                     200000);
  send_parameters_.max_bandwidth_bps = 100000;
  SetSendParameters(send_parameters_);
  EXPECT_EQ(100000, call_.GetConfig().bitrate_config.min_bitrate_bps)
      << "Setting max bitrate should keep previous min bitrate.";
  EXPECT_EQ(-1, call_.GetConfig().bitrate_config.start_bitrate_bps)
      << "Setting max bitrate should not reset start bitrate.";
  EXPECT_EQ(200000, call_.GetConfig().bitrate_config.max_bitrate_bps);
}

// Test that we can enable NACK with opus as caller.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecEnableNackAsCaller) {
  EXPECT_TRUE(SetupSendStream());
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kOpusCodec);
  parameters.codecs[0].AddFeedbackParam(
      cricket::FeedbackParam(cricket::kRtcpFbParamNack,
                             cricket::kParamValueEmpty));
  EXPECT_EQ(0, GetSendStreamConfig(kSsrcX).rtp.nack.rtp_history_ms);
  SetSendParameters(parameters);
  EXPECT_EQ(kRtpHistoryMs, GetSendStreamConfig(kSsrcX).rtp.nack.rtp_history_ms);
}

// Test that we can enable NACK with opus as callee.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecEnableNackAsCallee) {
  EXPECT_TRUE(SetupRecvStream());
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kOpusCodec);
  parameters.codecs[0].AddFeedbackParam(
      cricket::FeedbackParam(cricket::kRtcpFbParamNack,
                             cricket::kParamValueEmpty));
  EXPECT_EQ(0, GetRecvStreamConfig(kSsrcX).rtp.nack.rtp_history_ms);
  SetSendParameters(parameters);
  // NACK should be enabled even with no send stream.
  EXPECT_EQ(kRtpHistoryMs, GetRecvStreamConfig(kSsrcX).rtp.nack.rtp_history_ms);

  EXPECT_TRUE(channel_->AddSendStream(
      cricket::StreamParams::CreateLegacy(kSsrcX)));
  EXPECT_EQ(kRtpHistoryMs, GetSendStreamConfig(kSsrcX).rtp.nack.rtp_history_ms);
}

// Test that we can enable NACK on receive streams.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecEnableNackRecvStreams) {
  EXPECT_TRUE(SetupSendStream());
  EXPECT_TRUE(AddRecvStream(kSsrcY));
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kOpusCodec);
  parameters.codecs[0].AddFeedbackParam(
      cricket::FeedbackParam(cricket::kRtcpFbParamNack,
                             cricket::kParamValueEmpty));
  EXPECT_EQ(0, GetSendStreamConfig(kSsrcX).rtp.nack.rtp_history_ms);
  EXPECT_EQ(0, GetRecvStreamConfig(kSsrcY).rtp.nack.rtp_history_ms);
  SetSendParameters(parameters);
  EXPECT_EQ(kRtpHistoryMs, GetSendStreamConfig(kSsrcX).rtp.nack.rtp_history_ms);
  EXPECT_EQ(kRtpHistoryMs, GetRecvStreamConfig(kSsrcY).rtp.nack.rtp_history_ms);
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
  EXPECT_EQ(kRtpHistoryMs, GetSendStreamConfig(kSsrcX).rtp.nack.rtp_history_ms);

  parameters.codecs.clear();
  parameters.codecs.push_back(kOpusCodec);
  SetSendParameters(parameters);
  EXPECT_EQ(0, GetSendStreamConfig(kSsrcX).rtp.nack.rtp_history_ms);
}

// Test that we can disable NACK on receive streams.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecDisableNackRecvStreams) {
  EXPECT_TRUE(SetupSendStream());
  EXPECT_TRUE(AddRecvStream(kSsrcY));
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kOpusCodec);
  parameters.codecs[0].AddFeedbackParam(
      cricket::FeedbackParam(cricket::kRtcpFbParamNack,
                             cricket::kParamValueEmpty));
  SetSendParameters(parameters);
  EXPECT_EQ(kRtpHistoryMs, GetSendStreamConfig(kSsrcX).rtp.nack.rtp_history_ms);
  EXPECT_EQ(kRtpHistoryMs, GetRecvStreamConfig(kSsrcY).rtp.nack.rtp_history_ms);

  parameters.codecs.clear();
  parameters.codecs.push_back(kOpusCodec);
  SetSendParameters(parameters);
  EXPECT_EQ(0, GetSendStreamConfig(kSsrcX).rtp.nack.rtp_history_ms);
  EXPECT_EQ(0, GetRecvStreamConfig(kSsrcY).rtp.nack.rtp_history_ms);
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
  EXPECT_EQ(kRtpHistoryMs, GetSendStreamConfig(kSsrcX).rtp.nack.rtp_history_ms);

  EXPECT_TRUE(AddRecvStream(kSsrcY));
  EXPECT_EQ(kRtpHistoryMs, GetRecvStreamConfig(kSsrcY).rtp.nack.rtp_history_ms);
  EXPECT_TRUE(AddRecvStream(kSsrcZ));
  EXPECT_EQ(kRtpHistoryMs, GetRecvStreamConfig(kSsrcZ).rtp.nack.rtp_history_ms);
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
  EXPECT_TRUE(AddRecvStream(kSsrcX));
  ASSERT_TRUE(call_.GetAudioReceiveStream(kSsrcX) != nullptr);
  EXPECT_FALSE(
      call_.GetAudioReceiveStream(kSsrcX)->GetConfig().rtp.transport_cc);

  send_parameters.codecs = engine_->send_codecs();
  SetSendParameters(send_parameters);
  ASSERT_TRUE(call_.GetAudioReceiveStream(kSsrcX) != nullptr);
  EXPECT_TRUE(
      call_.GetAudioReceiveStream(kSsrcX)->GetConfig().rtp.transport_cc);
}

// Test that we can switch back and forth between Opus and ISAC with CN.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecsIsacOpusSwitching) {
  EXPECT_TRUE(SetupSendStream());

  cricket::AudioSendParameters opus_parameters;
  opus_parameters.codecs.push_back(kOpusCodec);
  SetSendParameters(opus_parameters);
  {
    const auto& spec = *GetSendStreamConfig(kSsrcX).send_codec_spec;
    EXPECT_EQ(111, spec.payload_type);
    EXPECT_STRCASEEQ("opus", spec.format.name.c_str());
  }

  cricket::AudioSendParameters isac_parameters;
  isac_parameters.codecs.push_back(kIsacCodec);
  isac_parameters.codecs.push_back(kCn16000Codec);
  isac_parameters.codecs.push_back(kOpusCodec);
  SetSendParameters(isac_parameters);
  {
    const auto& spec = *GetSendStreamConfig(kSsrcX).send_codec_spec;
    EXPECT_EQ(103, spec.payload_type);
    EXPECT_STRCASEEQ("ISAC", spec.format.name.c_str());
  }

  SetSendParameters(opus_parameters);
  {
    const auto& spec = *GetSendStreamConfig(kSsrcX).send_codec_spec;
    EXPECT_EQ(111, spec.payload_type);
    EXPECT_STRCASEEQ("opus", spec.format.name.c_str());
  }
}

// Test that we handle various ways of specifying bitrate.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecsBitrate) {
  EXPECT_TRUE(SetupSendStream());
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kIsacCodec);  // bitrate == 32000
  SetSendParameters(parameters);
  {
    const auto& spec = *GetSendStreamConfig(kSsrcX).send_codec_spec;
    EXPECT_EQ(103, spec.payload_type);
    EXPECT_STRCASEEQ("ISAC", spec.format.name.c_str());
    EXPECT_EQ(32000, spec.target_bitrate_bps);
  }

  parameters.codecs[0].bitrate = 0;         // bitrate == default
  SetSendParameters(parameters);
  {
    const auto& spec = *GetSendStreamConfig(kSsrcX).send_codec_spec;
    EXPECT_EQ(103, spec.payload_type);
    EXPECT_STRCASEEQ("ISAC", spec.format.name.c_str());
    EXPECT_EQ(32000, spec.target_bitrate_bps);
  }
  parameters.codecs[0].bitrate = 28000;     // bitrate == 28000
  SetSendParameters(parameters);
  {
    const auto& spec = *GetSendStreamConfig(kSsrcX).send_codec_spec;
    EXPECT_EQ(103, spec.payload_type);
    EXPECT_STRCASEEQ("ISAC", spec.format.name.c_str());
    EXPECT_EQ(28000, spec.target_bitrate_bps);
  }

  parameters.codecs[0] = kPcmuCodec;        // bitrate == 64000
  SetSendParameters(parameters);
  {
    const auto& spec = *GetSendStreamConfig(kSsrcX).send_codec_spec;
    EXPECT_EQ(0, spec.payload_type);
    EXPECT_STRCASEEQ("PCMU", spec.format.name.c_str());
    EXPECT_EQ(64000, spec.target_bitrate_bps);
  }

  parameters.codecs[0].bitrate = 0;         // bitrate == default
  SetSendParameters(parameters);
  {
    const auto& spec = *GetSendStreamConfig(kSsrcX).send_codec_spec;
    EXPECT_EQ(0, spec.payload_type);
    EXPECT_STREQ("PCMU", spec.format.name.c_str());
    EXPECT_EQ(64000, spec.target_bitrate_bps);
  }

  parameters.codecs[0] = kOpusCodec;
  parameters.codecs[0].bitrate = 0;         // bitrate == default
  SetSendParameters(parameters);
  {
    const auto& spec = *GetSendStreamConfig(kSsrcX).send_codec_spec;
    EXPECT_EQ(111, spec.payload_type);
    EXPECT_STREQ("opus", spec.format.name.c_str());
    EXPECT_EQ(32000, spec.target_bitrate_bps);
  }
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
  parameters.codecs.push_back(kTelephoneEventCodec1);
  parameters.codecs.push_back(kIsacCodec);
  parameters.codecs.push_back(kPcmuCodec);
  parameters.codecs[0].id = 98;  // DTMF
  parameters.codecs[1].id = 96;
  SetSendParameters(parameters);
  const auto& spec = *GetSendStreamConfig(kSsrcX).send_codec_spec;
  EXPECT_EQ(96, spec.payload_type);
  EXPECT_STRCASEEQ("ISAC", spec.format.name.c_str());
  EXPECT_TRUE(channel_->CanInsertDtmf());
}

// Test that payload type range is limited for telephone-event codec.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecsDTMFPayloadTypeOutOfRange) {
  EXPECT_TRUE(SetupSendStream());
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kTelephoneEventCodec2);
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
  const auto& send_codec_spec = *GetSendStreamConfig(kSsrcX).send_codec_spec;
  EXPECT_EQ(96, send_codec_spec.payload_type);
  EXPECT_STRCASEEQ("ISAC", send_codec_spec.format.name.c_str());
  EXPECT_EQ(98, send_codec_spec.cng_payload_type);
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
  parameters.codecs.push_back(kTelephoneEventCodec1);
  parameters.codecs[0].id = 96;
  parameters.codecs[2].id = 97;  // wideband CN
  parameters.codecs[4].id = 98;  // DTMF
  SetSendParameters(parameters);
  const auto& send_codec_spec = *GetSendStreamConfig(kSsrcX).send_codec_spec;
  EXPECT_EQ(96, send_codec_spec.payload_type);
  EXPECT_STRCASEEQ("ISAC", send_codec_spec.format.name.c_str());
  EXPECT_EQ(1, send_codec_spec.format.num_channels);
  EXPECT_EQ(97, send_codec_spec.cng_payload_type);
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
  parameters.codecs.push_back(kTelephoneEventCodec2);
  parameters.codecs[0].id = 96;
  parameters.codecs[2].id = 97;  // wideband CN
  parameters.codecs[4].id = 98;  // DTMF
  SetSendParameters(parameters);
  EXPECT_TRUE(channel_->AddSendStream(
      cricket::StreamParams::CreateLegacy(kSsrcX)));

  const auto& send_codec_spec = *GetSendStreamConfig(kSsrcX).send_codec_spec;
  EXPECT_EQ(96, send_codec_spec.payload_type);
  EXPECT_STRCASEEQ("ISAC", send_codec_spec.format.name.c_str());
  EXPECT_EQ(1, send_codec_spec.format.num_channels);
  EXPECT_EQ(97, send_codec_spec.cng_payload_type);
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
    const auto& send_codec_spec = *GetSendStreamConfig(kSsrcX).send_codec_spec;
    EXPECT_STRCASEEQ("ISAC", send_codec_spec.format.name.c_str());
    EXPECT_EQ(1, send_codec_spec.format.num_channels);
    EXPECT_EQ(97, send_codec_spec.cng_payload_type);
  }
  // Set PCMU(8K) and CN(16K). VAD should not be activated.
  parameters.codecs[0] = kPcmuCodec;
  SetSendParameters(parameters);
  {
    const auto& send_codec_spec = *GetSendStreamConfig(kSsrcX).send_codec_spec;
    EXPECT_STRCASEEQ("PCMU", send_codec_spec.format.name.c_str());
    EXPECT_EQ(rtc::Optional<int>(), send_codec_spec.cng_payload_type);
  }
  // Set PCMU(8K) and CN(8K). VAD should be activated.
  parameters.codecs[1] = kCn8000Codec;
  SetSendParameters(parameters);
  {
    const auto& send_codec_spec = *GetSendStreamConfig(kSsrcX).send_codec_spec;
    EXPECT_STRCASEEQ("PCMU", send_codec_spec.format.name.c_str());
    EXPECT_EQ(1, send_codec_spec.format.num_channels);
    EXPECT_EQ(13, send_codec_spec.cng_payload_type);
  }
  // Set ISAC(16K) and CN(8K). VAD should not be activated.
  parameters.codecs[0] = kIsacCodec;
  SetSendParameters(parameters);
  {
    const auto& send_codec_spec = *GetSendStreamConfig(kSsrcX).send_codec_spec;
    EXPECT_STRCASEEQ("ISAC", send_codec_spec.format.name.c_str());
    EXPECT_EQ(rtc::Optional<int>(), send_codec_spec.cng_payload_type);
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
  parameters.codecs.push_back(kTelephoneEventCodec1);
  parameters.codecs[0].name = "iSaC";
  parameters.codecs[0].id = 96;
  parameters.codecs[2].id = 97;  // wideband CN
  parameters.codecs[4].id = 98;  // DTMF
  SetSendParameters(parameters);
  const auto& send_codec_spec = *GetSendStreamConfig(kSsrcX).send_codec_spec;
  EXPECT_EQ(96, send_codec_spec.payload_type);
  EXPECT_STRCASEEQ("ISAC", send_codec_spec.format.name.c_str());
  EXPECT_EQ(1, send_codec_spec.format.num_channels);
  EXPECT_EQ(97, send_codec_spec.cng_payload_type);
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

// Test support for transport sequence number header extension.
TEST_F(WebRtcVoiceEngineTestFake, SendTransportSequenceNumberHeaderExtensions) {
  TestSetSendRtpHeaderExtensions(
      webrtc::RtpExtension::kTransportSequenceNumberUri);
}
TEST_F(WebRtcVoiceEngineTestFake, RecvTransportSequenceNumberHeaderExtensions) {
  TestSetRecvRtpHeaderExtensions(
      webrtc::RtpExtension::kTransportSequenceNumberUri);
}

// Test that we can create a channel and start sending on it.
TEST_F(WebRtcVoiceEngineTestFake, Send) {
  EXPECT_TRUE(SetupSendStream());
  SetSendParameters(send_parameters_);
  SetSend(true);
  EXPECT_TRUE(GetSendStream(kSsrcX).IsSending());
  SetSend(false);
  EXPECT_FALSE(GetSendStream(kSsrcX).IsSending());
}

// Test that a channel will send if and only if it has a source and is enabled
// for sending.
TEST_F(WebRtcVoiceEngineTestFake, SendStateWithAndWithoutSource) {
  EXPECT_TRUE(SetupSendStream());
  SetSendParameters(send_parameters_);
  SetAudioSend(kSsrcX, true, nullptr);
  SetSend(true);
  EXPECT_FALSE(GetSendStream(kSsrcX).IsSending());
  SetAudioSend(kSsrcX, true, &fake_source_);
  EXPECT_TRUE(GetSendStream(kSsrcX).IsSending());
  SetAudioSend(kSsrcX, true, nullptr);
  EXPECT_FALSE(GetSendStream(kSsrcX).IsSending());
}

// Test that a channel is muted/unmuted.
TEST_F(WebRtcVoiceEngineTestFake, SendStateMuteUnmute) {
  EXPECT_TRUE(SetupSendStream());
  SetSendParameters(send_parameters_);
  EXPECT_FALSE(GetSendStream(kSsrcX).muted());
  SetAudioSend(kSsrcX, true, nullptr);
  EXPECT_FALSE(GetSendStream(kSsrcX).muted());
  SetAudioSend(kSsrcX, false, nullptr);
  EXPECT_TRUE(GetSendStream(kSsrcX).muted());
}

// Test that SetSendParameters() does not alter a stream's send state.
TEST_F(WebRtcVoiceEngineTestFake, SendStateWhenStreamsAreRecreated) {
  EXPECT_TRUE(SetupSendStream());
  EXPECT_FALSE(GetSendStream(kSsrcX).IsSending());

  // Turn on sending.
  SetSend(true);
  EXPECT_TRUE(GetSendStream(kSsrcX).IsSending());

  // Changing RTP header extensions will recreate the AudioSendStream.
  send_parameters_.extensions.push_back(
      webrtc::RtpExtension(webrtc::RtpExtension::kAudioLevelUri, 12));
  SetSendParameters(send_parameters_);
  EXPECT_TRUE(GetSendStream(kSsrcX).IsSending());

  // Turn off sending.
  SetSend(false);
  EXPECT_FALSE(GetSendStream(kSsrcX).IsSending());

  // Changing RTP header extensions will recreate the AudioSendStream.
  send_parameters_.extensions.clear();
  SetSendParameters(send_parameters_);
  EXPECT_FALSE(GetSendStream(kSsrcX).IsSending());
}

// Test that we can create a channel and start playing out on it.
TEST_F(WebRtcVoiceEngineTestFake, Playout) {
  EXPECT_TRUE(SetupRecvStream());
  EXPECT_TRUE(channel_->SetRecvParameters(recv_parameters_));
  channel_->SetPlayout(true);
  EXPECT_TRUE(GetRecvStream(kSsrcX).started());
  channel_->SetPlayout(false);
  EXPECT_FALSE(GetRecvStream(kSsrcX).started());
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
        *call_.GetAudioSendStream(ssrc)->GetConfig().send_codec_spec;
    EXPECT_STRCASEEQ("ISAC", send_codec_spec.format.name.c_str());
    EXPECT_EQ(1, send_codec_spec.format.num_channels);
    EXPECT_EQ(97, send_codec_spec.cng_payload_type);
  }

  // Change to PCMU(8K) and CN(16K).
  parameters.codecs[0] = kPcmuCodec;
  SetSendParameters(parameters);
  for (uint32_t ssrc : kSsrcs4) {
    ASSERT_TRUE(call_.GetAudioSendStream(ssrc) != nullptr);
    const auto& send_codec_spec =
        *call_.GetAudioSendStream(ssrc)->GetConfig().send_codec_spec;
    EXPECT_STRCASEEQ("PCMU", send_codec_spec.format.name.c_str());
    EXPECT_EQ(rtc::Optional<int>(), send_codec_spec.cng_payload_type);
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
  EXPECT_TRUE(AddRecvStream(kSsrcY));

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
    VerifyVoiceSendRecvCodecs(info);

    // We have added one receive stream. We should see empty stats.
    EXPECT_EQ(info.receivers.size(), 1u);
    EXPECT_EQ(info.receivers[0].ssrc(), 0);
  }

  // Remove the kSsrcY stream. No receiver stats.
  {
    cricket::VoiceMediaInfo info;
    EXPECT_TRUE(channel_->RemoveRecvStream(kSsrcY));
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
    VerifyVoiceSendRecvCodecs(info);
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
  EXPECT_TRUE(AddRecvStream(kSsrcY));
  SetSend(true);
  EXPECT_TRUE(GetSendStream(kSsrcX).IsSending());

  // Make sure only the new stream is played out.
  EXPECT_TRUE(GetRecvStream(kSsrcY).started());

  // Adding yet another stream should have stream 2 and 3 enabled for playout.
  EXPECT_TRUE(AddRecvStream(kSsrcZ));
  EXPECT_TRUE(GetRecvStream(kSsrcY).started());
  EXPECT_TRUE(GetRecvStream(kSsrcZ).started());

  // Stop sending.
  SetSend(false);
  EXPECT_FALSE(GetSendStream(kSsrcX).IsSending());

  // Stop playout.
  channel_->SetPlayout(false);
  EXPECT_FALSE(GetRecvStream(kSsrcY).started());
  EXPECT_FALSE(GetRecvStream(kSsrcZ).started());

  // Restart playout and make sure recv streams are played out.
  channel_->SetPlayout(true);
  EXPECT_TRUE(GetRecvStream(kSsrcY).started());
  EXPECT_TRUE(GetRecvStream(kSsrcZ).started());

  // Now remove the recv streams.
  EXPECT_TRUE(channel_->RemoveRecvStream(kSsrcZ));
  EXPECT_TRUE(channel_->RemoveRecvStream(kSsrcY));
}

// Test that we can create a channel configured for Codian bridges,
// and start sending on it.
TEST_F(WebRtcVoiceEngineTestFake, CodianSend) {
  EXPECT_TRUE(SetupSendStream());
  send_parameters_.options.adjust_agc_delta = rtc::Optional<int>(-10);
  EXPECT_CALL(apm_gc_,
              set_target_level_dbfs(11)).Times(2).WillRepeatedly(Return(0));
  SetSendParameters(send_parameters_);
  SetSend(true);
  EXPECT_TRUE(GetSendStream(kSsrcX).IsSending());
  SetSend(false);
  EXPECT_FALSE(GetSendStream(kSsrcX).IsSending());
}

TEST_F(WebRtcVoiceEngineTestFake, TxAgcConfigViaOptions) {
  EXPECT_TRUE(SetupSendStream());
  EXPECT_CALL(adm_,
              BuiltInAGCIsAvailable()).Times(2).WillRepeatedly(Return(false));
  EXPECT_CALL(adm_, SetAGC(true)).Times(2).WillRepeatedly(Return(0));
  EXPECT_CALL(apm_gc_, Enable(true)).Times(2).WillOnce(Return(0));
  send_parameters_.options.tx_agc_target_dbov = rtc::Optional<uint16_t>(3);
  send_parameters_.options.tx_agc_digital_compression_gain =
      rtc::Optional<uint16_t>(9);
  send_parameters_.options.tx_agc_limiter = rtc::Optional<bool>(true);
  send_parameters_.options.auto_gain_control = rtc::Optional<bool>(true);
  EXPECT_CALL(apm_gc_, set_target_level_dbfs(3)).WillOnce(Return(0));
  EXPECT_CALL(apm_gc_, set_compression_gain_db(9)).WillRepeatedly(Return(0));
  EXPECT_CALL(apm_gc_, enable_limiter(true)).WillRepeatedly(Return(0));
  SetSendParameters(send_parameters_);

  // Check interaction with adjust_agc_delta. Both should be respected, for
  // backwards compatibility.
  send_parameters_.options.adjust_agc_delta = rtc::Optional<int>(-10);
  EXPECT_CALL(apm_gc_, set_target_level_dbfs(13)).WillOnce(Return(0));
  SetSendParameters(send_parameters_);
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
            GetAudioNetworkAdaptorConfig(kSsrcX));
}

TEST_F(WebRtcVoiceEngineTestFake, AudioSendResetAudioNetworkAdaptor) {
  EXPECT_TRUE(SetupSendStream());
  send_parameters_.options.audio_network_adaptor = rtc::Optional<bool>(true);
  send_parameters_.options.audio_network_adaptor_config =
      rtc::Optional<std::string>("1234");
  SetSendParameters(send_parameters_);
  EXPECT_EQ(send_parameters_.options.audio_network_adaptor_config,
            GetAudioNetworkAdaptorConfig(kSsrcX));
  cricket::AudioOptions options;
  options.audio_network_adaptor = rtc::Optional<bool>(false);
  SetAudioSend(kSsrcX, true, nullptr, &options);
  EXPECT_EQ(rtc::Optional<std::string>(), GetAudioNetworkAdaptorConfig(kSsrcX));
}

TEST_F(WebRtcVoiceEngineTestFake, AudioNetworkAdaptorNotGetOverridden) {
  EXPECT_TRUE(SetupSendStream());
  send_parameters_.options.audio_network_adaptor = rtc::Optional<bool>(true);
  send_parameters_.options.audio_network_adaptor_config =
      rtc::Optional<std::string>("1234");
  SetSendParameters(send_parameters_);
  EXPECT_EQ(send_parameters_.options.audio_network_adaptor_config,
            GetAudioNetworkAdaptorConfig(kSsrcX));
  const int initial_num = call_.GetNumCreatedSendStreams();
  cricket::AudioOptions options;
  options.audio_network_adaptor = rtc::Optional<bool>();
  // Unvalued |options.audio_network_adaptor|.should not reset audio network
  // adaptor.
  SetAudioSend(kSsrcX, true, nullptr, &options);
  // AudioSendStream not expected to be recreated.
  EXPECT_EQ(initial_num, call_.GetNumCreatedSendStreams());
  EXPECT_EQ(send_parameters_.options.audio_network_adaptor_config,
            GetAudioNetworkAdaptorConfig(kSsrcX));
}

class WebRtcVoiceEngineWithSendSideBweWithOverheadTest
    : public WebRtcVoiceEngineTestFake {
 public:
  WebRtcVoiceEngineWithSendSideBweWithOverheadTest()
      : WebRtcVoiceEngineTestFake(
            "WebRTC-Audio-SendSideBwe/Enabled/WebRTC-SendSideBwe-WithOverhead/"
            "Enabled/") {}
};

TEST_F(WebRtcVoiceEngineWithSendSideBweWithOverheadTest, MinAndMaxBitrate) {
  EXPECT_TRUE(SetupSendStream());
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kOpusCodec);
  SetSendParameters(parameters);
  const int initial_num = call_.GetNumCreatedSendStreams();
  EXPECT_EQ(initial_num, call_.GetNumCreatedSendStreams());

  // OverheadPerPacket = Ipv4(20B) + UDP(8B) + SRTP(10B) + RTP(12)
  constexpr int kOverheadPerPacket = 20 + 8 + 10 + 12;
  constexpr int kOpusMaxPtimeMs = WEBRTC_OPUS_SUPPORT_120MS_PTIME ? 120 : 60;
  constexpr int kMinOverheadBps =
      kOverheadPerPacket * 8 * 1000 / kOpusMaxPtimeMs;

  constexpr int kOpusMinBitrateBps = 6000;
  EXPECT_EQ(kOpusMinBitrateBps + kMinOverheadBps,
            GetSendStreamConfig(kSsrcX).min_bitrate_bps);
  constexpr int kOpusBitrateFbBps = 32000;
  EXPECT_EQ(kOpusBitrateFbBps + kMinOverheadBps,
            GetSendStreamConfig(kSsrcX).max_bitrate_bps);

  parameters.options.audio_network_adaptor = rtc::Optional<bool>(true);
  parameters.options.audio_network_adaptor_config =
      rtc::Optional<std::string>("1234");
  SetSendParameters(parameters);

  constexpr int kMinOverheadWithAnaBps =
      kOverheadPerPacket * 8 * 1000 / kOpusMaxPtimeMs;

  EXPECT_EQ(kOpusMinBitrateBps + kMinOverheadWithAnaBps,
            GetSendStreamConfig(kSsrcX).min_bitrate_bps);

  EXPECT_EQ(kOpusBitrateFbBps + kMinOverheadWithAnaBps,
            GetSendStreamConfig(kSsrcX).max_bitrate_bps);
}

// This test is similar to
// WebRtcVoiceEngineTestFake.SetRtpSendParameterUpdatesMaxBitrate but with an
// additional field trial.
TEST_F(WebRtcVoiceEngineWithSendSideBweWithOverheadTest,
       SetRtpSendParameterUpdatesMaxBitrate) {
  EXPECT_TRUE(SetupSendStream());
  cricket::AudioSendParameters send_parameters;
  send_parameters.codecs.push_back(kOpusCodec);
  SetSendParameters(send_parameters);

  webrtc::RtpParameters rtp_parameters = channel_->GetRtpSendParameters(kSsrcX);
  // Expect empty on parameters.encodings[0].max_bitrate_bps;
  EXPECT_FALSE(rtp_parameters.encodings[0].max_bitrate_bps);

  constexpr int kMaxBitrateBps = 6000;
  rtp_parameters.encodings[0].max_bitrate_bps =
      rtc::Optional<int>(kMaxBitrateBps);
  EXPECT_TRUE(channel_->SetRtpSendParameters(kSsrcX, rtp_parameters));

  const int max_bitrate = GetSendStreamConfig(kSsrcX).max_bitrate_bps;
#if WEBRTC_OPUS_SUPPORT_120MS_PTIME
  constexpr int kMinOverhead = 3333;
#else
  constexpr int kMinOverhead = 6666;
#endif
  EXPECT_EQ(max_bitrate, kMaxBitrateBps + kMinOverhead);
}

// Test that we can set the outgoing SSRC properly.
// SSRC is set in SetupSendStream() by calling AddSendStream.
TEST_F(WebRtcVoiceEngineTestFake, SetSendSsrc) {
  EXPECT_TRUE(SetupSendStream());
  EXPECT_TRUE(call_.GetAudioSendStream(kSsrcX));
}

TEST_F(WebRtcVoiceEngineTestFake, GetStats) {
  // Setup. We need send codec to be set to get all stats.
  EXPECT_TRUE(SetupSendStream());
  // SetupSendStream adds a send stream with kSsrcX, so the receive
  // stream has to use a different SSRC.
  EXPECT_TRUE(AddRecvStream(kSsrcY));
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
    VerifyVoiceSendRecvCodecs(info);
  }

  // Remove the kSsrcY stream. No receiver stats.
  {
    cricket::VoiceMediaInfo info;
    EXPECT_TRUE(channel_->RemoveRecvStream(kSsrcY));
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
    VerifyVoiceSendRecvCodecs(info);
  }
}

// Test that we can set the outgoing SSRC properly with multiple streams.
// SSRC is set in SetupSendStream() by calling AddSendStream.
TEST_F(WebRtcVoiceEngineTestFake, SetSendSsrcWithMultipleStreams) {
  EXPECT_TRUE(SetupSendStream());
  EXPECT_TRUE(call_.GetAudioSendStream(kSsrcX));
  EXPECT_TRUE(AddRecvStream(kSsrcY));
  EXPECT_EQ(kSsrcX, GetRecvStreamConfig(kSsrcY).rtp.local_ssrc);
}

// Test that the local SSRC is the same on sending and receiving channels if the
// receive channel is created before the send channel.
TEST_F(WebRtcVoiceEngineTestFake, SetSendSsrcAfterCreatingReceiveChannel) {
  EXPECT_TRUE(SetupChannel());
  EXPECT_TRUE(AddRecvStream(kSsrcY));
  EXPECT_TRUE(channel_->AddSendStream(
      cricket::StreamParams::CreateLegacy(kSsrcX)));
  EXPECT_TRUE(call_.GetAudioSendStream(kSsrcX));
  EXPECT_EQ(kSsrcX, GetRecvStreamConfig(kSsrcY).rtp.local_ssrc);
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

// Test that receiving on an unsignaled stream works (a stream is created).
TEST_F(WebRtcVoiceEngineTestFake, RecvUnsignaled) {
  EXPECT_TRUE(SetupChannel());
  EXPECT_EQ(0, call_.GetAudioReceiveStreams().size());

  DeliverPacket(kPcmuFrame, sizeof(kPcmuFrame));

  EXPECT_EQ(1, call_.GetAudioReceiveStreams().size());
  EXPECT_TRUE(GetRecvStream(kSsrc1).VerifyLastPacket(kPcmuFrame,
                                                     sizeof(kPcmuFrame)));
}

// Test that receiving N unsignaled stream works (streams will be created), and
// that packets are forwarded to them all.
TEST_F(WebRtcVoiceEngineTestFake, RecvMultipleUnsignaled) {
  EXPECT_TRUE(SetupChannel());
  unsigned char packet[sizeof(kPcmuFrame)];
  memcpy(packet, kPcmuFrame, sizeof(kPcmuFrame));

  // Note that SSRC = 0 is not supported.
  for (uint32_t ssrc = 1; ssrc < (1 + kMaxUnsignaledRecvStreams); ++ssrc) {
    rtc::SetBE32(&packet[8], ssrc);
    DeliverPacket(packet, sizeof(packet));

    // Verify we have one new stream for each loop iteration.
    EXPECT_EQ(ssrc, call_.GetAudioReceiveStreams().size());
    EXPECT_EQ(1, GetRecvStream(ssrc).received_packets());
    EXPECT_TRUE(GetRecvStream(ssrc).VerifyLastPacket(packet, sizeof(packet)));
  }

  // Sending on the same SSRCs again should not create new streams.
  for (uint32_t ssrc = 1; ssrc < (1 + kMaxUnsignaledRecvStreams); ++ssrc) {
    rtc::SetBE32(&packet[8], ssrc);
    DeliverPacket(packet, sizeof(packet));

    EXPECT_EQ(kMaxUnsignaledRecvStreams, call_.GetAudioReceiveStreams().size());
    EXPECT_EQ(2, GetRecvStream(ssrc).received_packets());
    EXPECT_TRUE(GetRecvStream(ssrc).VerifyLastPacket(packet, sizeof(packet)));
  }

  // Send on another SSRC, the oldest unsignaled stream (SSRC=1) is replaced.
  constexpr uint32_t kAnotherSsrc = 667;
  rtc::SetBE32(&packet[8], kAnotherSsrc);
  DeliverPacket(packet, sizeof(packet));

  const auto& streams = call_.GetAudioReceiveStreams();
  EXPECT_EQ(kMaxUnsignaledRecvStreams, streams.size());
  size_t i = 0;
  for (uint32_t ssrc = 2; ssrc < (1 + kMaxUnsignaledRecvStreams); ++ssrc, ++i) {
    EXPECT_EQ(ssrc, streams[i]->GetConfig().rtp.remote_ssrc);
    EXPECT_EQ(2, streams[i]->received_packets());
  }
  EXPECT_EQ(kAnotherSsrc, streams[i]->GetConfig().rtp.remote_ssrc);
  EXPECT_EQ(1, streams[i]->received_packets());
  // Sanity check that we've checked all streams.
  EXPECT_EQ(kMaxUnsignaledRecvStreams, (i + 1));
}

// Test that a default channel is created even after a signaled stream has been
// added, and that this stream will get any packets for unknown SSRCs.
TEST_F(WebRtcVoiceEngineTestFake, RecvUnsignaledAfterSignaled) {
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
  EXPECT_EQ(1, call_.GetAudioReceiveStreams().size());

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

// Two tests to verify that adding a receive stream with the same SSRC as a
// previously added unsignaled stream will only recreate underlying stream
// objects if the stream parameters have changed.
TEST_F(WebRtcVoiceEngineTestFake, AddRecvStreamAfterUnsignaled_NoRecreate) {
  EXPECT_TRUE(SetupChannel());

  // Spawn unsignaled stream with SSRC=1.
  DeliverPacket(kPcmuFrame, sizeof(kPcmuFrame));
  EXPECT_EQ(1, call_.GetAudioReceiveStreams().size());
  EXPECT_TRUE(GetRecvStream(1).VerifyLastPacket(kPcmuFrame,
                                                sizeof(kPcmuFrame)));

  // Verify that the underlying stream object in Call is not recreated when a
  // stream with SSRC=1 is added.
  const auto& streams = call_.GetAudioReceiveStreams();
  EXPECT_EQ(1, streams.size());
  int audio_receive_stream_id = streams.front()->id();
  EXPECT_TRUE(AddRecvStream(1));
  EXPECT_EQ(1, streams.size());
  EXPECT_EQ(audio_receive_stream_id, streams.front()->id());
}

TEST_F(WebRtcVoiceEngineTestFake, AddRecvStreamAfterUnsignaled_Recreate) {
  EXPECT_TRUE(SetupChannel());

  // Spawn unsignaled stream with SSRC=1.
  DeliverPacket(kPcmuFrame, sizeof(kPcmuFrame));
  EXPECT_EQ(1, call_.GetAudioReceiveStreams().size());
  EXPECT_TRUE(GetRecvStream(1).VerifyLastPacket(kPcmuFrame,
                                                sizeof(kPcmuFrame)));

  // Verify that the underlying stream object in Call *is* recreated when a
  // stream with SSRC=1 is added, and which has changed stream parameters.
  const auto& streams = call_.GetAudioReceiveStreams();
  EXPECT_EQ(1, streams.size());
  int audio_receive_stream_id = streams.front()->id();
  cricket::StreamParams stream_params;
  stream_params.ssrcs.push_back(1);
  stream_params.sync_label = "sync_label";
  EXPECT_TRUE(channel_->AddRecvStream(stream_params));
  EXPECT_EQ(1, streams.size());
  EXPECT_NE(audio_receive_stream_id, streams.front()->id());
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
  EXPECT_TRUE(AddRecvStream(kSsrcX));
  EXPECT_THAT(GetRecvStreamConfig(kSsrcX).decoder_map,
              (ContainerEq<std::map<int, webrtc::SdpAudioFormat>>(
                  {{0, {"PCMU", 8000, 1}}, {103, {"ISAC", 16000, 1}}})));
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
  TestInsertDtmf(0, true, kTelephoneEventCodec1);
}

// Test the InsertDtmf on default send stream as callee
TEST_F(WebRtcVoiceEngineTestFake, InsertDtmfOnDefaultSendStreamAsCallee) {
  TestInsertDtmf(0, false, kTelephoneEventCodec2);
}

// Test the InsertDtmf on specified send stream as caller.
TEST_F(WebRtcVoiceEngineTestFake, InsertDtmfOnSendStreamAsCaller) {
  TestInsertDtmf(kSsrcX, true, kTelephoneEventCodec2);
}

// Test the InsertDtmf on specified send stream as callee.
TEST_F(WebRtcVoiceEngineTestFake, InsertDtmfOnSendStreamAsCallee) {
  TestInsertDtmf(kSsrcX, false, kTelephoneEventCodec1);
}

TEST_F(WebRtcVoiceEngineTestFake, SetAudioOptions) {
  EXPECT_TRUE(SetupSendStream());
  EXPECT_CALL(adm_,
              BuiltInAECIsAvailable()).Times(9).WillRepeatedly(Return(false));
  EXPECT_CALL(adm_,
              BuiltInAGCIsAvailable()).Times(4).WillRepeatedly(Return(false));
  EXPECT_CALL(adm_,
              BuiltInNSIsAvailable()).Times(2).WillRepeatedly(Return(false));

  EXPECT_EQ(50, voe_.GetNetEqCapacity());
  EXPECT_FALSE(voe_.GetNetEqFastAccelerate());

  // Nothing set in AudioOptions, so everything should be as default.
  send_parameters_.options = cricket::AudioOptions();
  SetSendParameters(send_parameters_);
  EXPECT_TRUE(IsHighPassFilterEnabled());
  EXPECT_EQ(50, voe_.GetNetEqCapacity());
  EXPECT_FALSE(voe_.GetNetEqFastAccelerate());

  // Turn echo cancellation off
  EXPECT_CALL(apm_ec_, Enable(false)).WillOnce(Return(0));
  EXPECT_CALL(apm_ec_, enable_metrics(false)).WillOnce(Return(0));
  send_parameters_.options.echo_cancellation = rtc::Optional<bool>(false);
  SetSendParameters(send_parameters_);

  // Turn echo cancellation back on, with settings, and make sure
  // nothing else changed.
  EXPECT_CALL(apm_ec_, Enable(true)).WillOnce(Return(0));
  EXPECT_CALL(apm_ec_, enable_metrics(true)).WillOnce(Return(0));
  send_parameters_.options.echo_cancellation = rtc::Optional<bool>(true);
  SetSendParameters(send_parameters_);

  // Turn on delay agnostic aec and make sure nothing change w.r.t. echo
  // control.
  EXPECT_CALL(apm_ec_, Enable(true)).WillOnce(Return(0));
  EXPECT_CALL(apm_ec_, enable_metrics(true)).WillOnce(Return(0));
  send_parameters_.options.delay_agnostic_aec = rtc::Optional<bool>(true);
  SetSendParameters(send_parameters_);

  // Turn off echo cancellation and delay agnostic aec.
  EXPECT_CALL(apm_ec_, Enable(false)).WillOnce(Return(0));
  EXPECT_CALL(apm_ec_, enable_metrics(false)).WillOnce(Return(0));
  send_parameters_.options.delay_agnostic_aec = rtc::Optional<bool>(false);
  send_parameters_.options.extended_filter_aec = rtc::Optional<bool>(false);
  send_parameters_.options.echo_cancellation = rtc::Optional<bool>(false);
  SetSendParameters(send_parameters_);

  // Turning delay agnostic aec back on should also turn on echo cancellation.
  EXPECT_CALL(apm_ec_, Enable(true)).WillOnce(Return(0));
  EXPECT_CALL(apm_ec_, enable_metrics(true)).WillOnce(Return(0));
  send_parameters_.options.delay_agnostic_aec = rtc::Optional<bool>(true);
  SetSendParameters(send_parameters_);

  // Turn off AGC
  EXPECT_CALL(adm_, SetAGC(false)).WillOnce(Return(0));
  EXPECT_CALL(apm_ec_, Enable(true)).WillOnce(Return(0));
  EXPECT_CALL(apm_ec_, enable_metrics(true)).WillOnce(Return(0));
  EXPECT_CALL(apm_gc_, Enable(false)).WillOnce(Return(0));
  send_parameters_.options.auto_gain_control = rtc::Optional<bool>(false);
  SetSendParameters(send_parameters_);

  // Turn AGC back on
  EXPECT_CALL(adm_, SetAGC(true)).WillOnce(Return(0));
  EXPECT_CALL(apm_ec_, Enable(true)).WillOnce(Return(0));
  EXPECT_CALL(apm_ec_, enable_metrics(true)).WillOnce(Return(0));
  EXPECT_CALL(apm_gc_, Enable(true)).WillOnce(Return(0));
  send_parameters_.options.auto_gain_control = rtc::Optional<bool>(true);
  send_parameters_.options.adjust_agc_delta = rtc::Optional<int>();
  SetSendParameters(send_parameters_);

  // Turn off other options (and stereo swapping on).
  EXPECT_CALL(adm_, SetAGC(true)).WillOnce(Return(0));
  EXPECT_CALL(apm_ec_, Enable(true)).WillOnce(Return(0));
  EXPECT_CALL(apm_ec_, enable_metrics(true)).WillOnce(Return(0));
  EXPECT_CALL(apm_gc_, Enable(true)).WillOnce(Return(0));
  EXPECT_CALL(apm_ns_, Enable(false)).WillOnce(Return(0));
  EXPECT_CALL(apm_vd_, Enable(false)).WillOnce(Return(0));
  EXPECT_CALL(transmit_mixer_, EnableStereoChannelSwapping(true));
  send_parameters_.options.noise_suppression = rtc::Optional<bool>(false);
  send_parameters_.options.highpass_filter = rtc::Optional<bool>(false);
  send_parameters_.options.typing_detection = rtc::Optional<bool>(false);
  send_parameters_.options.stereo_swapping = rtc::Optional<bool>(true);
  SetSendParameters(send_parameters_);
  EXPECT_FALSE(IsHighPassFilterEnabled());

  // Set options again to ensure it has no impact.
  EXPECT_CALL(adm_, SetAGC(true)).WillOnce(Return(0));
  EXPECT_CALL(apm_ec_, Enable(true)).WillOnce(Return(0));
  EXPECT_CALL(apm_ec_, enable_metrics(true)).WillOnce(Return(0));
  EXPECT_CALL(apm_gc_, Enable(true)).WillOnce(Return(0));
  EXPECT_CALL(apm_ns_, Enable(false)).WillOnce(Return(0));
  EXPECT_CALL(apm_vd_, Enable(false)).WillOnce(Return(0));
  EXPECT_CALL(transmit_mixer_, EnableStereoChannelSwapping(true));
  SetSendParameters(send_parameters_);
}

TEST_F(WebRtcVoiceEngineTestFake, SetOptionOverridesViaChannels) {
  EXPECT_TRUE(SetupSendStream());
  EXPECT_CALL(adm_,
              BuiltInAECIsAvailable()).Times(8).WillRepeatedly(Return(false));
  EXPECT_CALL(adm_,
              BuiltInAGCIsAvailable()).Times(8).WillRepeatedly(Return(false));
  EXPECT_CALL(adm_,
              BuiltInNSIsAvailable()).Times(8).WillRepeatedly(Return(false));
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
  EXPECT_CALL(adm_, SetAGC(true)).Times(2).WillRepeatedly(Return(0));
  EXPECT_CALL(apm_ec_, Enable(true)).Times(2).WillRepeatedly(Return(0));
  EXPECT_CALL(apm_ec_, enable_metrics(true)).Times(2).WillRepeatedly(Return(0));
  EXPECT_CALL(apm_gc_, Enable(true)).Times(2).WillRepeatedly(Return(0));
  EXPECT_CALL(apm_ns_, Enable(true)).Times(2).WillRepeatedly(Return(0));
  EXPECT_TRUE(channel1->SetSendParameters(parameters_options_all));
  EXPECT_EQ(parameters_options_all.options, channel1->options());
  EXPECT_TRUE(channel2->SetSendParameters(parameters_options_all));
  EXPECT_EQ(parameters_options_all.options, channel2->options());

  // unset NS
  cricket::AudioSendParameters parameters_options_no_ns = send_parameters_;
  parameters_options_no_ns.options.noise_suppression =
      rtc::Optional<bool>(false);
  EXPECT_CALL(adm_, SetAGC(true)).WillOnce(Return(0));
  EXPECT_CALL(apm_ec_, Enable(true)).WillOnce(Return(0));
  EXPECT_CALL(apm_ec_, enable_metrics(true)).WillOnce(Return(0));
  EXPECT_CALL(apm_gc_, Enable(true)).WillOnce(Return(0));
  EXPECT_CALL(apm_ns_, Enable(false)).WillOnce(Return(0));
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
  EXPECT_CALL(adm_, SetAGC(false)).WillOnce(Return(0));
  EXPECT_CALL(apm_ec_, Enable(true)).WillOnce(Return(0));
  EXPECT_CALL(apm_ec_, enable_metrics(true)).WillOnce(Return(0));
  EXPECT_CALL(apm_gc_, Enable(false)).WillOnce(Return(0));
  EXPECT_CALL(apm_ns_, Enable(true)).WillOnce(Return(0));
  EXPECT_TRUE(channel2->SetSendParameters(parameters_options_no_agc));
  expected_options.echo_cancellation = rtc::Optional<bool>(true);
  expected_options.auto_gain_control = rtc::Optional<bool>(false);
  expected_options.noise_suppression = rtc::Optional<bool>(true);
  EXPECT_EQ(expected_options, channel2->options());

  EXPECT_CALL(adm_, SetAGC(true)).WillOnce(Return(0));
  EXPECT_CALL(apm_ec_, Enable(true)).WillOnce(Return(0));
  EXPECT_CALL(apm_ec_, enable_metrics(true)).WillOnce(Return(0));
  EXPECT_CALL(apm_gc_, Enable(true)).WillOnce(Return(0));
  EXPECT_CALL(apm_ns_, Enable(true)).WillOnce(Return(0));
  EXPECT_TRUE(channel_->SetSendParameters(parameters_options_all));

  EXPECT_CALL(adm_, SetAGC(true)).WillOnce(Return(0));
  EXPECT_CALL(apm_ec_, Enable(true)).WillOnce(Return(0));
  EXPECT_CALL(apm_ec_, enable_metrics(true)).WillOnce(Return(0));
  EXPECT_CALL(apm_gc_, Enable(true)).WillOnce(Return(0));
  EXPECT_CALL(apm_ns_, Enable(false)).WillOnce(Return(0));
  channel1->SetSend(true);

  EXPECT_CALL(adm_, SetAGC(false)).WillOnce(Return(0));
  EXPECT_CALL(apm_ec_, Enable(true)).WillOnce(Return(0));
  EXPECT_CALL(apm_ec_, enable_metrics(true)).WillOnce(Return(0));
  EXPECT_CALL(apm_gc_, Enable(false)).WillOnce(Return(0));
  EXPECT_CALL(apm_ns_, Enable(true)).WillOnce(Return(0));
  channel2->SetSend(true);

  // Make sure settings take effect while we are sending.
  cricket::AudioSendParameters parameters_options_no_agc_nor_ns =
      send_parameters_;
  parameters_options_no_agc_nor_ns.options.auto_gain_control =
      rtc::Optional<bool>(false);
  parameters_options_no_agc_nor_ns.options.noise_suppression =
      rtc::Optional<bool>(false);
  EXPECT_CALL(adm_, SetAGC(false)).WillOnce(Return(0));
  EXPECT_CALL(apm_ec_, Enable(true)).WillOnce(Return(0));
  EXPECT_CALL(apm_ec_, enable_metrics(true)).WillOnce(Return(0));
  EXPECT_CALL(apm_gc_, Enable(false)).WillOnce(Return(0));
  EXPECT_CALL(apm_ns_, Enable(false)).WillOnce(Return(0));
  EXPECT_TRUE(channel2->SetSendParameters(parameters_options_no_agc_nor_ns));
  expected_options.echo_cancellation = rtc::Optional<bool>(true);
  expected_options.auto_gain_control = rtc::Optional<bool>(false);
  expected_options.noise_suppression = rtc::Optional<bool>(false);
  EXPECT_EQ(expected_options, channel2->options());
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
  EXPECT_TRUE(AddRecvStream(kSsrcX));
  int channel_id = voe_.GetLastChannel();
  EXPECT_EQ(channel_id, media_channel->GetReceiveChannelId(kSsrcX));
  EXPECT_EQ(-1, media_channel->GetReceiveChannelId(kSsrcY));
  EXPECT_TRUE(AddRecvStream(kSsrcY));
  int channel_id2 = voe_.GetLastChannel();
  EXPECT_EQ(channel_id2, media_channel->GetReceiveChannelId(kSsrcY));
}

TEST_F(WebRtcVoiceEngineTestFake, TestGetSendChannelId) {
  EXPECT_TRUE(SetupChannel());
  cricket::WebRtcVoiceMediaChannel* media_channel =
        static_cast<cricket::WebRtcVoiceMediaChannel*>(channel_);
  EXPECT_EQ(-1, media_channel->GetSendChannelId(0));
  EXPECT_TRUE(channel_->AddSendStream(
      cricket::StreamParams::CreateLegacy(kSsrcX)));
  int channel_id = voe_.GetLastChannel();
  EXPECT_EQ(channel_id, media_channel->GetSendChannelId(kSsrcX));
  EXPECT_EQ(-1, media_channel->GetSendChannelId(kSsrcY));
  EXPECT_TRUE(channel_->AddSendStream(
      cricket::StreamParams::CreateLegacy(kSsrcY)));
  int channel_id2 = voe_.GetLastChannel();
  EXPECT_EQ(channel_id2, media_channel->GetSendChannelId(kSsrcY));
}

TEST_F(WebRtcVoiceEngineTestFake, SetOutputVolume) {
  EXPECT_TRUE(SetupChannel());
  EXPECT_FALSE(channel_->SetOutputVolume(kSsrcY, 0.5));
  cricket::StreamParams stream;
  stream.ssrcs.push_back(kSsrcY);
  EXPECT_TRUE(channel_->AddRecvStream(stream));
  EXPECT_DOUBLE_EQ(1, GetRecvStream(kSsrcY).gain());
  EXPECT_TRUE(channel_->SetOutputVolume(kSsrcY, 3));
  EXPECT_DOUBLE_EQ(3, GetRecvStream(kSsrcY).gain());
}

TEST_F(WebRtcVoiceEngineTestFake, SetOutputVolumeUnsignaledRecvStream) {
  EXPECT_TRUE(SetupChannel());

  // Spawn an unsignaled stream by sending a packet - gain should be 1.
  DeliverPacket(kPcmuFrame, sizeof(kPcmuFrame));
  EXPECT_DOUBLE_EQ(1, GetRecvStream(kSsrc1).gain());

  // Should remember the volume "2" which will be set on new unsignaled streams,
  // and also set the gain to 2 on existing unsignaled streams.
  EXPECT_TRUE(channel_->SetOutputVolume(kSsrc0, 2));
  EXPECT_DOUBLE_EQ(2, GetRecvStream(kSsrc1).gain());

  // Spawn an unsignaled stream by sending a packet - gain should be 2.
  unsigned char pcmuFrame2[sizeof(kPcmuFrame)];
  memcpy(pcmuFrame2, kPcmuFrame, sizeof(kPcmuFrame));
  rtc::SetBE32(&pcmuFrame2[8], kSsrcX);
  DeliverPacket(pcmuFrame2, sizeof(pcmuFrame2));
  EXPECT_DOUBLE_EQ(2, GetRecvStream(kSsrcX).gain());

  // Setting gain with SSRC=0 should affect all unsignaled streams.
  EXPECT_TRUE(channel_->SetOutputVolume(kSsrc0, 3));
  if (kMaxUnsignaledRecvStreams > 1) {
    EXPECT_DOUBLE_EQ(3, GetRecvStream(kSsrc1).gain());
  }
  EXPECT_DOUBLE_EQ(3, GetRecvStream(kSsrcX).gain());

  // Setting gain on an individual stream affects only that.
  EXPECT_TRUE(channel_->SetOutputVolume(kSsrcX, 4));
  if (kMaxUnsignaledRecvStreams > 1) {
    EXPECT_DOUBLE_EQ(3, GetRecvStream(kSsrc1).gain());
  }
  EXPECT_DOUBLE_EQ(4, GetRecvStream(kSsrcX).gain());
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
  EXPECT_TRUE(AddRecvStream(kSsrcY));
  EXPECT_EQ(kSsrcX, GetRecvStreamConfig(kSsrcY).rtp.local_ssrc);
  EXPECT_TRUE(channel_->AddSendStream(
      cricket::StreamParams::CreateLegacy(kSsrcZ)));
  EXPECT_EQ(kSsrcX, GetRecvStreamConfig(kSsrcY).rtp.local_ssrc);
  EXPECT_TRUE(AddRecvStream(kSsrcW));
  EXPECT_EQ(kSsrcX, GetRecvStreamConfig(kSsrcW).rtp.local_ssrc);
}

TEST_F(WebRtcVoiceEngineTestFake, AssociateFirstSendChannel_RecvCreatedFirst) {
  EXPECT_TRUE(SetupRecvStream());
  EXPECT_EQ(0xFA17FA17u, GetRecvStreamConfig(kSsrcX).rtp.local_ssrc);
  EXPECT_TRUE(channel_->AddSendStream(
      cricket::StreamParams::CreateLegacy(kSsrcY)));
  EXPECT_EQ(kSsrcY, GetRecvStreamConfig(kSsrcX).rtp.local_ssrc);
  EXPECT_TRUE(AddRecvStream(kSsrcZ));
  EXPECT_EQ(kSsrcY, GetRecvStreamConfig(kSsrcZ).rtp.local_ssrc);
  EXPECT_TRUE(channel_->AddSendStream(
      cricket::StreamParams::CreateLegacy(kSsrcW)));
  EXPECT_EQ(kSsrcY, GetRecvStreamConfig(kSsrcX).rtp.local_ssrc);
  EXPECT_EQ(kSsrcY, GetRecvStreamConfig(kSsrcZ).rtp.local_ssrc);
}

TEST_F(WebRtcVoiceEngineTestFake, SetRawAudioSink) {
  EXPECT_TRUE(SetupChannel());
  std::unique_ptr<FakeAudioSink> fake_sink_1(new FakeAudioSink());
  std::unique_ptr<FakeAudioSink> fake_sink_2(new FakeAudioSink());

  // Setting the sink before a recv stream exists should do nothing.
  channel_->SetRawAudioSink(kSsrcX, std::move(fake_sink_1));
  EXPECT_TRUE(AddRecvStream(kSsrcX));
  EXPECT_EQ(nullptr, GetRecvStream(kSsrcX).sink());

  // Now try actually setting the sink.
  channel_->SetRawAudioSink(kSsrcX, std::move(fake_sink_2));
  EXPECT_NE(nullptr, GetRecvStream(kSsrcX).sink());

  // Now try resetting it.
  channel_->SetRawAudioSink(kSsrcX, nullptr);
  EXPECT_EQ(nullptr, GetRecvStream(kSsrcX).sink());
}

TEST_F(WebRtcVoiceEngineTestFake, SetRawAudioSinkUnsignaledRecvStream) {
  EXPECT_TRUE(SetupChannel());
  std::unique_ptr<FakeAudioSink> fake_sink_1(new FakeAudioSink());
  std::unique_ptr<FakeAudioSink> fake_sink_2(new FakeAudioSink());
  std::unique_ptr<FakeAudioSink> fake_sink_3(new FakeAudioSink());
  std::unique_ptr<FakeAudioSink> fake_sink_4(new FakeAudioSink());

  // Should be able to set a default sink even when no stream exists.
  channel_->SetRawAudioSink(0, std::move(fake_sink_1));

  // Spawn an unsignaled stream by sending a packet - it should be assigned the
  // default sink.
  DeliverPacket(kPcmuFrame, sizeof(kPcmuFrame));
  EXPECT_NE(nullptr, GetRecvStream(kSsrc1).sink());

  // Try resetting the default sink.
  channel_->SetRawAudioSink(kSsrc0, nullptr);
  EXPECT_EQ(nullptr, GetRecvStream(kSsrc1).sink());

  // Try setting the default sink while the default stream exists.
  channel_->SetRawAudioSink(kSsrc0, std::move(fake_sink_2));
  EXPECT_NE(nullptr, GetRecvStream(kSsrc1).sink());

  // If we remove and add a default stream, it should get the same sink.
  EXPECT_TRUE(channel_->RemoveRecvStream(kSsrc1));
  DeliverPacket(kPcmuFrame, sizeof(kPcmuFrame));
  EXPECT_NE(nullptr, GetRecvStream(kSsrc1).sink());

  // Spawn another unsignaled stream - it should be assigned the default sink
  // and the previous unsignaled stream should lose it.
  unsigned char pcmuFrame2[sizeof(kPcmuFrame)];
  memcpy(pcmuFrame2, kPcmuFrame, sizeof(kPcmuFrame));
  rtc::SetBE32(&pcmuFrame2[8], kSsrcX);
  DeliverPacket(pcmuFrame2, sizeof(pcmuFrame2));
  if (kMaxUnsignaledRecvStreams > 1) {
    EXPECT_EQ(nullptr, GetRecvStream(kSsrc1).sink());
  }
  EXPECT_NE(nullptr, GetRecvStream(kSsrcX).sink());

  // Reset the default sink - the second unsignaled stream should lose it.
  channel_->SetRawAudioSink(kSsrc0, nullptr);
  if (kMaxUnsignaledRecvStreams > 1) {
    EXPECT_EQ(nullptr, GetRecvStream(kSsrc1).sink());
  }
  EXPECT_EQ(nullptr, GetRecvStream(kSsrcX).sink());

  // Try setting the default sink while two streams exists.
  channel_->SetRawAudioSink(kSsrc0, std::move(fake_sink_3));
  if (kMaxUnsignaledRecvStreams > 1) {
    EXPECT_EQ(nullptr, GetRecvStream(kSsrc1).sink());
  }
  EXPECT_NE(nullptr, GetRecvStream(kSsrcX).sink());

  // Try setting the sink for the first unsignaled stream using its known SSRC.
  channel_->SetRawAudioSink(kSsrc1, std::move(fake_sink_4));
  if (kMaxUnsignaledRecvStreams > 1) {
    EXPECT_NE(nullptr, GetRecvStream(kSsrc1).sink());
  }
  EXPECT_NE(nullptr, GetRecvStream(kSsrcX).sink());
  if (kMaxUnsignaledRecvStreams > 1) {
    EXPECT_NE(GetRecvStream(kSsrc1).sink(), GetRecvStream(kSsrcX).sink());
  }
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
  EXPECT_TRUE(GetRecvStream(kSsrcX).started());

  // Changing RTP header extensions will recreate the AudioReceiveStream.
  cricket::AudioRecvParameters parameters;
  parameters.extensions.push_back(
      webrtc::RtpExtension(webrtc::RtpExtension::kAudioLevelUri, 12));
  channel_->SetRecvParameters(parameters);

  EXPECT_TRUE(GetRecvStream(kSsrcX).started());
}

// Tests that the library initializes and shuts down properly.
TEST(WebRtcVoiceEngineTest, StartupShutdown) {
  // If the VoiceEngine wants to gather available codecs early, that's fine but
  // we never want it to create a decoder at this stage.
  cricket::WebRtcVoiceEngine engine(
      nullptr, webrtc::MockAudioEncoderFactory::CreateUnusedFactory(),
      webrtc::MockAudioDecoderFactory::CreateUnusedFactory(), nullptr);
  engine.Init();
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
  // Return 100ms just in case this function gets called.  If we don't,
  // we could enter a tight loop since the mock would return 0.
  EXPECT_CALL(adm, TimeUntilNextProcess()).WillRepeatedly(Return(100));
  {
    cricket::WebRtcVoiceEngine engine(
        &adm, webrtc::MockAudioEncoderFactory::CreateUnusedFactory(),
        webrtc::MockAudioDecoderFactory::CreateUnusedFactory(), nullptr);
    engine.Init();
    webrtc::RtcEventLogNullImpl event_log;
    std::unique_ptr<webrtc::Call> call(
        webrtc::Call::Create(webrtc::Call::Config(&event_log)));
    cricket::VoiceMediaChannel* channel = engine.CreateChannel(
        call.get(), cricket::MediaConfig(), cricket::AudioOptions());
    EXPECT_TRUE(channel != nullptr);
    delete channel;
  }
}

// Verify the payload id of common audio codecs, including CN, ISAC, and G722.
TEST(WebRtcVoiceEngineTest, HasCorrectPayloadTypeMapping) {
  // TODO(ossu): Why are the payload types of codecs with non-static payload
  // type assignments checked here? It shouldn't really matter.
  cricket::WebRtcVoiceEngine engine(
      nullptr, webrtc::MockAudioEncoderFactory::CreateUnusedFactory(),
      webrtc::MockAudioDecoderFactory::CreateUnusedFactory(), nullptr);
  engine.Init();
  for (const cricket::AudioCodec& codec : engine.send_codecs()) {
    auto is_codec = [&codec](const char* name, int clockrate = 0) {
      return STR_CASE_CMP(codec.name.c_str(), name) == 0 &&
             (clockrate == 0 || codec.clockrate == clockrate);
    };
    if (is_codec("CN", 16000)) {
      EXPECT_EQ(105, codec.id);
    } else if (is_codec("CN", 32000)) {
      EXPECT_EQ(106, codec.id);
    } else if (is_codec("ISAC", 16000)) {
      EXPECT_EQ(103, codec.id);
    } else if (is_codec("ISAC", 32000)) {
      EXPECT_EQ(104, codec.id);
    } else if (is_codec("G722", 8000)) {
      EXPECT_EQ(9, codec.id);
    } else if (is_codec("telephone-event", 8000)) {
      EXPECT_EQ(126, codec.id);
    // TODO(solenberg): 16k, 32k, 48k DTMF should be dynamically assigned.
    // Remove these checks once both send and receive side assigns payload types
    // dynamically.
    } else if (is_codec("telephone-event", 16000)) {
      EXPECT_EQ(113, codec.id);
    } else if (is_codec("telephone-event", 32000)) {
      EXPECT_EQ(112, codec.id);
    } else if (is_codec("telephone-event", 48000)) {
      EXPECT_EQ(110, codec.id);
    } else if (is_codec("opus")) {
      EXPECT_EQ(111, codec.id);
      ASSERT_TRUE(codec.params.find("minptime") != codec.params.end());
      EXPECT_EQ("10", codec.params.find("minptime")->second);
      ASSERT_TRUE(codec.params.find("useinbandfec") != codec.params.end());
      EXPECT_EQ("1", codec.params.find("useinbandfec")->second);
    }
  }
}

// Tests that VoE supports at least 32 channels
TEST(WebRtcVoiceEngineTest, Has32Channels) {
  cricket::WebRtcVoiceEngine engine(
      nullptr, webrtc::MockAudioEncoderFactory::CreateUnusedFactory(),
      webrtc::MockAudioDecoderFactory::CreateUnusedFactory(), nullptr);
  engine.Init();
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
      nullptr, webrtc::MockAudioEncoderFactory::CreateUnusedFactory(),
      webrtc::CreateBuiltinAudioDecoderFactory(), nullptr);
  engine.Init();
  webrtc::RtcEventLogNullImpl event_log;
  std::unique_ptr<webrtc::Call> call(
      webrtc::Call::Create(webrtc::Call::Config(&event_log)));
  cricket::WebRtcVoiceMediaChannel channel(&engine, cricket::MediaConfig(),
                                           cricket::AudioOptions(), call.get());
  cricket::AudioRecvParameters parameters;
  parameters.codecs = engine.recv_codecs();
  EXPECT_TRUE(channel.SetRecvParameters(parameters));
}

TEST(WebRtcVoiceEngineTest, CollectRecvCodecs) {
  std::vector<webrtc::AudioCodecSpec> specs;
  webrtc::AudioCodecSpec spec1{{"codec1", 48000, 2, {{"param1", "value1"}}},
                               {48000, 2, 16000, 10000, 20000}};
  spec1.info.allow_comfort_noise = false;
  spec1.info.supports_network_adaption = true;
  specs.push_back(spec1);
  webrtc::AudioCodecSpec spec2{{"codec2", 32000, 1}, {32000, 1, 32000}};
  spec2.info.allow_comfort_noise = false;
  specs.push_back(spec2);
  specs.push_back(webrtc::AudioCodecSpec{
      {"codec3", 16000, 1, {{"param1", "value1b"}, {"param2", "value2"}}},
      {16000, 1, 13300}});
  specs.push_back(
      webrtc::AudioCodecSpec{{"codec4", 8000, 1}, {8000, 1, 64000}});
  specs.push_back(
      webrtc::AudioCodecSpec{{"codec5", 8000, 2}, {8000, 1, 64000}});

  rtc::scoped_refptr<webrtc::MockAudioEncoderFactory> unused_encoder_factory =
      webrtc::MockAudioEncoderFactory::CreateUnusedFactory();
  rtc::scoped_refptr<webrtc::MockAudioDecoderFactory> mock_decoder_factory =
      new rtc::RefCountedObject<webrtc::MockAudioDecoderFactory>;
  EXPECT_CALL(*mock_decoder_factory.get(), GetSupportedDecoders())
      .WillOnce(Return(specs));

  cricket::WebRtcVoiceEngine engine(nullptr, unused_encoder_factory,
                                    mock_decoder_factory, nullptr);
  engine.Init();
  auto codecs = engine.recv_codecs();
  EXPECT_EQ(11, codecs.size());

  // Rather than just ASSERTing that there are enough codecs, ensure that we can
  // check the actual values safely, to provide better test results.
  auto get_codec =
      [&codecs](size_t index) -> const cricket::AudioCodec& {
        static const cricket::AudioCodec missing_codec(0, "<missing>", 0, 0, 0);
        if (codecs.size() > index)
          return codecs[index];
        return missing_codec;
      };

  // Ensure the general codecs are generated first and in order.
  for (size_t i = 0; i != specs.size(); ++i) {
    EXPECT_EQ(specs[i].format.name, get_codec(i).name);
    EXPECT_EQ(specs[i].format.clockrate_hz, get_codec(i).clockrate);
    EXPECT_EQ(specs[i].format.num_channels, get_codec(i).channels);
    EXPECT_EQ(specs[i].format.parameters, get_codec(i).params);
  }

  // Find the index of a codec, or -1 if not found, so that we can easily check
  // supplementary codecs are ordered after the general codecs.
  auto find_codec =
      [&codecs](const webrtc::SdpAudioFormat& format) -> int {
        for (size_t i = 0; i != codecs.size(); ++i) {
          const cricket::AudioCodec& codec = codecs[i];
          if (STR_CASE_CMP(codec.name.c_str(), format.name.c_str()) == 0 &&
              codec.clockrate == format.clockrate_hz &&
              codec.channels == format.num_channels) {
            return rtc::checked_cast<int>(i);
          }
        }
        return -1;
      };

  // Ensure all supplementary codecs are generated last. Their internal ordering
  // is not important.
  // Without this cast, the comparison turned unsigned and, thus, failed for -1.
  const int num_specs = static_cast<int>(specs.size());
  EXPECT_GE(find_codec({"cn", 8000, 1}), num_specs);
  EXPECT_GE(find_codec({"cn", 16000, 1}), num_specs);
  EXPECT_EQ(find_codec({"cn", 32000, 1}), -1);
  EXPECT_GE(find_codec({"telephone-event", 8000, 1}), num_specs);
  EXPECT_GE(find_codec({"telephone-event", 16000, 1}), num_specs);
  EXPECT_GE(find_codec({"telephone-event", 32000, 1}), num_specs);
  EXPECT_GE(find_codec({"telephone-event", 48000, 1}), num_specs);
}
