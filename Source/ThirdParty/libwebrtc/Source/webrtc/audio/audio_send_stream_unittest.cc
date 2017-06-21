/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <string>
#include <utility>
#include <vector>

#include "webrtc/audio/audio_send_stream.h"
#include "webrtc/audio/audio_state.h"
#include "webrtc/audio/conversion.h"
#include "webrtc/base/ptr_util.h"
#include "webrtc/base/task_queue.h"
#include "webrtc/call/fake_rtp_transport_controller_send.h"
#include "webrtc/call/rtp_transport_controller_send_interface.h"
#include "webrtc/logging/rtc_event_log/mock/mock_rtc_event_log.h"
#include "webrtc/modules/audio_mixer/audio_mixer_impl.h"
#include "webrtc/modules/audio_processing/include/mock_audio_processing.h"
#include "webrtc/modules/congestion_controller/include/mock/mock_congestion_observer.h"
#include "webrtc/modules/congestion_controller/include/send_side_congestion_controller.h"
#include "webrtc/modules/pacing/paced_sender.h"
#include "webrtc/modules/rtp_rtcp/mocks/mock_rtcp_rtt_stats.h"
#include "webrtc/modules/rtp_rtcp/mocks/mock_rtp_rtcp.h"
#include "webrtc/test/gtest.h"
#include "webrtc/test/mock_audio_encoder.h"
#include "webrtc/test/mock_audio_encoder_factory.h"
#include "webrtc/test/mock_voe_channel_proxy.h"
#include "webrtc/test/mock_voice_engine.h"
#include "webrtc/voice_engine/transmit_mixer.h"

namespace webrtc {
namespace test {
namespace {

using testing::_;
using testing::Eq;
using testing::Ne;
using testing::Invoke;
using testing::Return;

const int kChannelId = 1;
const uint32_t kSsrc = 1234;
const char* kCName = "foo_name";
const int kAudioLevelId = 2;
const int kTransportSequenceNumberId = 4;
const int kEchoDelayMedian = 254;
const int kEchoDelayStdDev = -3;
const int kEchoReturnLoss = -65;
const int kEchoReturnLossEnhancement = 101;
const float kResidualEchoLikelihood = -1.0f;
const int32_t kSpeechInputLevel = 96;
const CallStatistics kCallStats = {
    1345,  1678,  1901, 1234,  112, 13456, 17890, 1567, -1890, -1123};
const ReportBlock kReportBlock = {456, 780, 123, 567, 890, 132, 143, 13354};
const int kTelephoneEventPayloadType = 123;
const int kTelephoneEventPayloadFrequency = 65432;
const int kTelephoneEventCode = 45;
const int kTelephoneEventDuration = 6789;
const CodecInst kIsacCodec = {103, "isac", 16000, 320, 1, 32000};
constexpr int kIsacPayloadType = 103;
const SdpAudioFormat kIsacFormat = {"isac", 16000, 1};
const SdpAudioFormat kOpusFormat = {"opus", 48000, 2};
const SdpAudioFormat kG722Format = {"g722", 8000, 1};
const AudioCodecSpec kCodecSpecs[] = {
    {kIsacFormat, {16000, 1, 32000, 10000, 32000}},
    {kOpusFormat, {48000, 1, 32000, 6000, 510000}},
    {kG722Format, {16000, 1, 64000}}};

class MockLimitObserver : public BitrateAllocator::LimitObserver {
 public:
  MOCK_METHOD2(OnAllocationLimitsChanged,
               void(uint32_t min_send_bitrate_bps,
                    uint32_t max_padding_bitrate_bps));
};

class MockTransmitMixer : public voe::TransmitMixer {
 public:
  MOCK_CONST_METHOD0(AudioLevelFullRange, int16_t());
};

std::unique_ptr<MockAudioEncoder> SetupAudioEncoderMock(
    int payload_type,
    const SdpAudioFormat& format) {
  for (const auto& spec : kCodecSpecs) {
    if (format == spec.format) {
      std::unique_ptr<MockAudioEncoder> encoder(new MockAudioEncoder);
      ON_CALL(*encoder.get(), SampleRateHz())
          .WillByDefault(Return(spec.info.sample_rate_hz));
      ON_CALL(*encoder.get(), NumChannels())
          .WillByDefault(Return(spec.info.num_channels));
      ON_CALL(*encoder.get(), RtpTimestampRateHz())
          .WillByDefault(Return(spec.format.clockrate_hz));
      return encoder;
    }
  }
  return nullptr;
}

rtc::scoped_refptr<MockAudioEncoderFactory> SetupEncoderFactoryMock() {
  rtc::scoped_refptr<MockAudioEncoderFactory> factory =
      new rtc::RefCountedObject<MockAudioEncoderFactory>();
  ON_CALL(*factory.get(), GetSupportedEncoders())
      .WillByDefault(Return(std::vector<AudioCodecSpec>(
          std::begin(kCodecSpecs), std::end(kCodecSpecs))));
  ON_CALL(*factory.get(), QueryAudioEncoder(_))
      .WillByDefault(Invoke([](const SdpAudioFormat& format) {
        for (const auto& spec : kCodecSpecs) {
          if (format == spec.format) {
            return rtc::Optional<AudioCodecInfo>(spec.info);
          }
        }
        return rtc::Optional<AudioCodecInfo>();
      }));
  ON_CALL(*factory.get(), MakeAudioEncoderMock(_, _, _))
      .WillByDefault(Invoke([](int payload_type, const SdpAudioFormat& format,
                               std::unique_ptr<AudioEncoder>* return_value) {
        *return_value = SetupAudioEncoderMock(payload_type, format);
      }));
  return factory;
}

struct ConfigHelper {
  ConfigHelper(bool audio_bwe_enabled, bool expect_set_encoder_call)
      : stream_config_(nullptr),
        simulated_clock_(123456),
        send_side_cc_(rtc::MakeUnique<SendSideCongestionController>(
            &simulated_clock_,
            nullptr /* observer */,
            &event_log_,
            &packet_router_)),
        fake_transport_(&packet_router_, send_side_cc_.get()),
        bitrate_allocator_(&limit_observer_),
        worker_queue_("ConfigHelper_worker_queue") {
    using testing::Invoke;

    EXPECT_CALL(voice_engine_,
        RegisterVoiceEngineObserver(_)).WillOnce(Return(0));
    EXPECT_CALL(voice_engine_,
        DeRegisterVoiceEngineObserver()).WillOnce(Return(0));
    EXPECT_CALL(voice_engine_, audio_device_module());
    EXPECT_CALL(voice_engine_, audio_processing());
    EXPECT_CALL(voice_engine_, audio_transport());

    AudioState::Config config;
    config.voice_engine = &voice_engine_;
    config.audio_mixer = AudioMixerImpl::Create();
    audio_state_ = AudioState::Create(config);

    SetupDefaultChannelProxy(audio_bwe_enabled);

    EXPECT_CALL(voice_engine_, ChannelProxyFactory(kChannelId))
        .WillOnce(Invoke([this](int channel_id) {
          return channel_proxy_;
        }));

    SetupMockForSetupSendCodec(expect_set_encoder_call);

    // Use ISAC as default codec so as to prevent unnecessary |voice_engine_|
    // calls from the default ctor behavior.
    stream_config_.send_codec_spec =
        rtc::Optional<AudioSendStream::Config::SendCodecSpec>(
            {kIsacPayloadType, kIsacFormat});
    stream_config_.voe_channel_id = kChannelId;
    stream_config_.rtp.ssrc = kSsrc;
    stream_config_.rtp.nack.rtp_history_ms = 200;
    stream_config_.rtp.c_name = kCName;
    stream_config_.rtp.extensions.push_back(
        RtpExtension(RtpExtension::kAudioLevelUri, kAudioLevelId));
    if (audio_bwe_enabled) {
      stream_config_.rtp.extensions.push_back(
          RtpExtension(RtpExtension::kTransportSequenceNumberUri,
                       kTransportSequenceNumberId));
      stream_config_.send_codec_spec->transport_cc_enabled = true;
    }
    stream_config_.encoder_factory = SetupEncoderFactoryMock();
    stream_config_.min_bitrate_bps = 10000;
    stream_config_.max_bitrate_bps = 65000;
  }

  AudioSendStream::Config& config() { return stream_config_; }
  MockAudioEncoderFactory& mock_encoder_factory() {
    return *static_cast<MockAudioEncoderFactory*>(
        stream_config_.encoder_factory.get());
  }
  rtc::scoped_refptr<AudioState> audio_state() { return audio_state_; }
  MockVoEChannelProxy* channel_proxy() { return channel_proxy_; }
  RtpTransportControllerSendInterface* transport() { return &fake_transport_; }
  BitrateAllocator* bitrate_allocator() { return &bitrate_allocator_; }
  rtc::TaskQueue* worker_queue() { return &worker_queue_; }
  RtcEventLog* event_log() { return &event_log_; }
  MockVoiceEngine* voice_engine() { return &voice_engine_; }

  void SetupDefaultChannelProxy(bool audio_bwe_enabled) {
    using testing::StrEq;
    channel_proxy_ = new testing::StrictMock<MockVoEChannelProxy>();
    EXPECT_CALL(*channel_proxy_, GetRtpRtcp(_, _))
        .WillRepeatedly(Invoke(
            [this](RtpRtcp** rtp_rtcp_module, RtpReceiver** rtp_receiver) {
              *rtp_rtcp_module = &this->rtp_rtcp_;
              *rtp_receiver = nullptr;  // Not deemed necessary for tests yet.
            }));
    EXPECT_CALL(*channel_proxy_, SetRTCPStatus(true)).Times(1);
    EXPECT_CALL(*channel_proxy_, SetLocalSSRC(kSsrc)).Times(1);
    EXPECT_CALL(*channel_proxy_, SetRTCP_CNAME(StrEq(kCName))).Times(1);
    EXPECT_CALL(*channel_proxy_, SetNACKStatus(true, 10)).Times(1);
    EXPECT_CALL(*channel_proxy_,
                SetSendAudioLevelIndicationStatus(true, kAudioLevelId))
        .Times(1);
    if (audio_bwe_enabled) {
      EXPECT_CALL(*channel_proxy_,
                  EnableSendTransportSequenceNumber(kTransportSequenceNumberId))
          .Times(1);
      EXPECT_CALL(*channel_proxy_, RegisterSenderCongestionControlObjects(
                                       &fake_transport_, Ne(nullptr)))
          .Times(1);
    } else {
      EXPECT_CALL(*channel_proxy_, RegisterSenderCongestionControlObjects(
                                       &fake_transport_, Eq(nullptr)))
          .Times(1);
    }
    EXPECT_CALL(*channel_proxy_, SetBitrate(_, _))
        .Times(1);
    EXPECT_CALL(*channel_proxy_, ResetSenderCongestionControlObjects())
        .Times(1);
    EXPECT_CALL(*channel_proxy_, RegisterExternalTransport(nullptr)).Times(1);
    EXPECT_CALL(*channel_proxy_, DeRegisterExternalTransport()).Times(1);
    EXPECT_CALL(*channel_proxy_, SetRtcEventLog(testing::NotNull())).Times(1);
    EXPECT_CALL(*channel_proxy_, SetRtcEventLog(testing::IsNull()))
        .Times(1);  // Destructor resets the event log
    EXPECT_CALL(*channel_proxy_, SetRtcpRttStats(&rtcp_rtt_stats_)).Times(1);
    EXPECT_CALL(*channel_proxy_, SetRtcpRttStats(testing::IsNull()))
        .Times(1);  // Destructor resets the rtt stats.
  }

  void SetupMockForSetupSendCodec(bool expect_set_encoder_call) {
    if (expect_set_encoder_call) {
      EXPECT_CALL(*channel_proxy_, SetEncoderForMock(_, _))
          .WillOnce(Return(true));
    }
  }

  RtcpRttStats* rtcp_rtt_stats() { return &rtcp_rtt_stats_; }

  void SetupMockForSendTelephoneEvent() {
    EXPECT_TRUE(channel_proxy_);
    EXPECT_CALL(*channel_proxy_,
        SetSendTelephoneEventPayloadType(kTelephoneEventPayloadType,
                                         kTelephoneEventPayloadFrequency))
            .WillOnce(Return(true));
    EXPECT_CALL(*channel_proxy_,
        SendTelephoneEventOutband(kTelephoneEventCode, kTelephoneEventDuration))
            .WillOnce(Return(true));
  }

  void SetupMockForGetStats() {
    using testing::DoAll;
    using testing::SetArgPointee;
    using testing::SetArgReferee;

    std::vector<ReportBlock> report_blocks;
    webrtc::ReportBlock block = kReportBlock;
    report_blocks.push_back(block);  // Has wrong SSRC.
    block.source_SSRC = kSsrc;
    report_blocks.push_back(block);  // Correct block.
    block.fraction_lost = 0;
    report_blocks.push_back(block);  // Duplicate SSRC, bad fraction_lost.

    EXPECT_TRUE(channel_proxy_);
    EXPECT_CALL(*channel_proxy_, GetRTCPStatistics())
        .WillRepeatedly(Return(kCallStats));
    EXPECT_CALL(*channel_proxy_, GetRemoteRTCPReportBlocks())
        .WillRepeatedly(Return(report_blocks));
    EXPECT_CALL(voice_engine_, transmit_mixer())
        .WillRepeatedly(Return(&transmit_mixer_));
    EXPECT_CALL(voice_engine_, audio_processing())
        .WillRepeatedly(Return(&audio_processing_));

    EXPECT_CALL(transmit_mixer_, AudioLevelFullRange())
        .WillRepeatedly(Return(kSpeechInputLevel));

    // We have to set the instantaneous value, the average, min and max. We only
    // care about the instantaneous value, so we set all to the same value.
    audio_processing_stats_.echo_return_loss.Set(
        kEchoReturnLoss, kEchoReturnLoss, kEchoReturnLoss, kEchoReturnLoss);
    audio_processing_stats_.echo_return_loss_enhancement.Set(
        kEchoReturnLossEnhancement, kEchoReturnLossEnhancement,
        kEchoReturnLossEnhancement, kEchoReturnLossEnhancement);
    audio_processing_stats_.delay_median = kEchoDelayMedian;
    audio_processing_stats_.delay_standard_deviation = kEchoDelayStdDev;

    EXPECT_CALL(audio_processing_, GetStatistics())
        .WillRepeatedly(Return(audio_processing_stats_));
  }

 private:
  testing::StrictMock<MockVoiceEngine> voice_engine_;
  rtc::scoped_refptr<AudioState> audio_state_;
  AudioSendStream::Config stream_config_;
  testing::StrictMock<MockVoEChannelProxy>* channel_proxy_ = nullptr;
  MockAudioProcessing audio_processing_;
  MockTransmitMixer transmit_mixer_;
  AudioProcessing::AudioProcessingStatistics audio_processing_stats_;
  SimulatedClock simulated_clock_;
  PacketRouter packet_router_;
  std::unique_ptr<SendSideCongestionController> send_side_cc_;
  FakeRtpTransportControllerSend fake_transport_;
  MockRtcEventLog event_log_;
  MockRtpRtcp rtp_rtcp_;
  MockRtcpRttStats rtcp_rtt_stats_;
  testing::NiceMock<MockLimitObserver> limit_observer_;
  BitrateAllocator bitrate_allocator_;
  // |worker_queue| is defined last to ensure all pending tasks are cancelled
  // and deleted before any other members.
  rtc::TaskQueue worker_queue_;
};
}  // namespace

TEST(AudioSendStreamTest, ConfigToString) {
  AudioSendStream::Config config(nullptr);
  config.rtp.ssrc = kSsrc;
  config.rtp.c_name = kCName;
  config.voe_channel_id = kChannelId;
  config.min_bitrate_bps = 12000;
  config.max_bitrate_bps = 34000;
  config.send_codec_spec =
      rtc::Optional<AudioSendStream::Config::SendCodecSpec>(
          {kIsacPayloadType, kIsacFormat});
  config.send_codec_spec->nack_enabled = true;
  config.send_codec_spec->transport_cc_enabled = false;
  config.send_codec_spec->cng_payload_type = rtc::Optional<int>(42);
  config.encoder_factory = MockAudioEncoderFactory::CreateUnusedFactory();
  config.rtp.extensions.push_back(
      RtpExtension(RtpExtension::kAudioLevelUri, kAudioLevelId));
  EXPECT_EQ(
      "{rtp: {ssrc: 1234, extensions: [{uri: "
      "urn:ietf:params:rtp-hdrext:ssrc-audio-level, id: 2}], nack: "
      "{rtp_history_ms: 0}, c_name: foo_name}, send_transport: null, "
      "voe_channel_id: 1, min_bitrate_bps: 12000, max_bitrate_bps: 34000, "
      "send_codec_spec: {nack_enabled: true, transport_cc_enabled: false, "
      "cng_payload_type: 42, payload_type: 103, "
      "format: {name: isac, clockrate_hz: 16000, num_channels: 1, "
      "parameters: {}}}}",
      config.ToString());
}

TEST(AudioSendStreamTest, ConstructDestruct) {
  ConfigHelper helper(false, true);
  internal::AudioSendStream send_stream(
      helper.config(), helper.audio_state(), helper.worker_queue(),
      helper.transport(), helper.bitrate_allocator(), helper.event_log(),
      helper.rtcp_rtt_stats(), rtc::Optional<RtpState>());
}

TEST(AudioSendStreamTest, SendTelephoneEvent) {
  ConfigHelper helper(false, true);
  internal::AudioSendStream send_stream(
      helper.config(), helper.audio_state(), helper.worker_queue(),
      helper.transport(), helper.bitrate_allocator(), helper.event_log(),
      helper.rtcp_rtt_stats(), rtc::Optional<RtpState>());
  helper.SetupMockForSendTelephoneEvent();
  EXPECT_TRUE(send_stream.SendTelephoneEvent(kTelephoneEventPayloadType,
      kTelephoneEventPayloadFrequency, kTelephoneEventCode,
      kTelephoneEventDuration));
}

TEST(AudioSendStreamTest, SetMuted) {
  ConfigHelper helper(false, true);
  internal::AudioSendStream send_stream(
      helper.config(), helper.audio_state(), helper.worker_queue(),
      helper.transport(), helper.bitrate_allocator(), helper.event_log(),
      helper.rtcp_rtt_stats(), rtc::Optional<RtpState>());
  EXPECT_CALL(*helper.channel_proxy(), SetInputMute(true));
  send_stream.SetMuted(true);
}

TEST(AudioSendStreamTest, AudioBweCorrectObjectsOnChannelProxy) {
  ConfigHelper helper(true, true);
  internal::AudioSendStream send_stream(
      helper.config(), helper.audio_state(), helper.worker_queue(),
      helper.transport(), helper.bitrate_allocator(), helper.event_log(),
      helper.rtcp_rtt_stats(), rtc::Optional<RtpState>());
}

TEST(AudioSendStreamTest, NoAudioBweCorrectObjectsOnChannelProxy) {
  ConfigHelper helper(false, true);
  internal::AudioSendStream send_stream(
      helper.config(), helper.audio_state(), helper.worker_queue(),
      helper.transport(), helper.bitrate_allocator(), helper.event_log(),
      helper.rtcp_rtt_stats(), rtc::Optional<RtpState>());
}

TEST(AudioSendStreamTest, GetStats) {
  ConfigHelper helper(false, true);
  internal::AudioSendStream send_stream(
      helper.config(), helper.audio_state(), helper.worker_queue(),
      helper.transport(), helper.bitrate_allocator(), helper.event_log(),
      helper.rtcp_rtt_stats(), rtc::Optional<RtpState>());
  helper.SetupMockForGetStats();
  AudioSendStream::Stats stats = send_stream.GetStats();
  EXPECT_EQ(kSsrc, stats.local_ssrc);
  EXPECT_EQ(static_cast<int64_t>(kCallStats.bytesSent), stats.bytes_sent);
  EXPECT_EQ(kCallStats.packetsSent, stats.packets_sent);
  EXPECT_EQ(static_cast<int32_t>(kReportBlock.cumulative_num_packets_lost),
            stats.packets_lost);
  EXPECT_EQ(Q8ToFloat(kReportBlock.fraction_lost), stats.fraction_lost);
  EXPECT_EQ(std::string(kIsacCodec.plname), stats.codec_name);
  EXPECT_EQ(static_cast<int32_t>(kReportBlock.extended_highest_sequence_number),
            stats.ext_seqnum);
  EXPECT_EQ(static_cast<int32_t>(kReportBlock.interarrival_jitter /
                                 (kIsacCodec.plfreq / 1000)),
            stats.jitter_ms);
  EXPECT_EQ(kCallStats.rttMs, stats.rtt_ms);
  EXPECT_EQ(static_cast<int32_t>(kSpeechInputLevel), stats.audio_level);
  EXPECT_EQ(-1, stats.aec_quality_min);
  EXPECT_EQ(kEchoDelayMedian, stats.echo_delay_median_ms);
  EXPECT_EQ(kEchoDelayStdDev, stats.echo_delay_std_ms);
  EXPECT_EQ(kEchoReturnLoss, stats.echo_return_loss);
  EXPECT_EQ(kEchoReturnLossEnhancement, stats.echo_return_loss_enhancement);
  EXPECT_EQ(kResidualEchoLikelihood, stats.residual_echo_likelihood);
  EXPECT_FALSE(stats.typing_noise_detected);
}

TEST(AudioSendStreamTest, GetStatsTypingNoiseDetected) {
  ConfigHelper helper(false, true);
  internal::AudioSendStream send_stream(
      helper.config(), helper.audio_state(), helper.worker_queue(),
      helper.transport(), helper.bitrate_allocator(), helper.event_log(),
      helper.rtcp_rtt_stats(), rtc::Optional<RtpState>());
  helper.SetupMockForGetStats();
  EXPECT_FALSE(send_stream.GetStats().typing_noise_detected);

  internal::AudioState* internal_audio_state =
      static_cast<internal::AudioState*>(helper.audio_state().get());
  VoiceEngineObserver* voe_observer =
      static_cast<VoiceEngineObserver*>(internal_audio_state);
  voe_observer->CallbackOnError(-1, VE_TYPING_NOISE_WARNING);
  EXPECT_TRUE(send_stream.GetStats().typing_noise_detected);
  voe_observer->CallbackOnError(-1, VE_TYPING_NOISE_OFF_WARNING);
  EXPECT_FALSE(send_stream.GetStats().typing_noise_detected);
}

TEST(AudioSendStreamTest, SendCodecAppliesNetworkAdaptor) {
  ConfigHelper helper(false, true);
  auto stream_config = helper.config();
  stream_config.send_codec_spec =
      rtc::Optional<AudioSendStream::Config::SendCodecSpec>({0, kOpusFormat});
  stream_config.audio_network_adaptor_config =
      rtc::Optional<std::string>("abced");

  EXPECT_CALL(helper.mock_encoder_factory(), MakeAudioEncoderMock(_, _, _))
      .WillOnce(Invoke([](int payload_type, const SdpAudioFormat& format,
                          std::unique_ptr<AudioEncoder>* return_value) {
        auto mock_encoder = SetupAudioEncoderMock(payload_type, format);
        EXPECT_CALL(*mock_encoder.get(), EnableAudioNetworkAdaptor(_, _))
            .WillOnce(Return(true));
        *return_value = std::move(mock_encoder);
      }));

  internal::AudioSendStream send_stream(
      stream_config, helper.audio_state(), helper.worker_queue(),
      helper.transport(), helper.bitrate_allocator(), helper.event_log(),
      helper.rtcp_rtt_stats(), rtc::Optional<RtpState>());
}

// VAD is applied when codec is mono and the CNG frequency matches the codec
// clock rate.
TEST(AudioSendStreamTest, SendCodecCanApplyVad) {
  ConfigHelper helper(false, false);
  auto stream_config = helper.config();
  stream_config.send_codec_spec =
      rtc::Optional<AudioSendStream::Config::SendCodecSpec>({9, kG722Format});
  stream_config.send_codec_spec->cng_payload_type = rtc::Optional<int>(105);
  using ::testing::Invoke;
  std::unique_ptr<AudioEncoder> stolen_encoder;
  EXPECT_CALL(*helper.channel_proxy(), SetEncoderForMock(_, _))
      .WillOnce(
          Invoke([&stolen_encoder](int payload_type,
                                   std::unique_ptr<AudioEncoder>* encoder) {
            stolen_encoder = std::move(*encoder);
            return true;
          }));

  internal::AudioSendStream send_stream(
      stream_config, helper.audio_state(), helper.worker_queue(),
      helper.transport(), helper.bitrate_allocator(), helper.event_log(),
      helper.rtcp_rtt_stats(), rtc::Optional<RtpState>());

  // We cannot truly determine if the encoder created is an AudioEncoderCng.  It
  // is the only reasonable implementation that will return something from
  // ReclaimContainedEncoders, though.
  ASSERT_TRUE(stolen_encoder);
  EXPECT_FALSE(stolen_encoder->ReclaimContainedEncoders().empty());
}

TEST(AudioSendStreamTest, DoesNotPassHigherBitrateThanMaxBitrate) {
  ConfigHelper helper(false, true);
  internal::AudioSendStream send_stream(
      helper.config(), helper.audio_state(), helper.worker_queue(),
      helper.transport(), helper.bitrate_allocator(), helper.event_log(),
      helper.rtcp_rtt_stats(), rtc::Optional<RtpState>());
  EXPECT_CALL(*helper.channel_proxy(),
              SetBitrate(helper.config().max_bitrate_bps, _));
  send_stream.OnBitrateUpdated(helper.config().max_bitrate_bps + 5000, 0.0, 50,
                               6000);
}

TEST(AudioSendStreamTest, ProbingIntervalOnBitrateUpdated) {
  ConfigHelper helper(false, true);
  internal::AudioSendStream send_stream(
      helper.config(), helper.audio_state(), helper.worker_queue(),
      helper.transport(), helper.bitrate_allocator(), helper.event_log(),
      helper.rtcp_rtt_stats(), rtc::Optional<RtpState>());
  EXPECT_CALL(*helper.channel_proxy(), SetBitrate(_, 5000));
  send_stream.OnBitrateUpdated(50000, 0.0, 50, 5000);
}

// Test that AudioSendStream doesn't recreate the encoder unnecessarily.
TEST(AudioSendStreamTest, DontRecreateEncoder) {
  ConfigHelper helper(false, false);
  // WillOnce is (currently) the default used by ConfigHelper if asked to set an
  // expectation for SetEncoder. Since this behavior is essential for this test
  // to be correct, it's instead set-up manually here. Otherwise a simple change
  // to ConfigHelper (say to WillRepeatedly) would silently make this test
  // useless.
  EXPECT_CALL(*helper.channel_proxy(), SetEncoderForMock(_, _))
      .WillOnce(Return(true));

  auto stream_config = helper.config();
  stream_config.send_codec_spec =
      rtc::Optional<AudioSendStream::Config::SendCodecSpec>({9, kG722Format});
  stream_config.send_codec_spec->cng_payload_type = rtc::Optional<int>(105);
  internal::AudioSendStream send_stream(
      stream_config, helper.audio_state(), helper.worker_queue(),
      helper.transport(), helper.bitrate_allocator(), helper.event_log(),
      helper.rtcp_rtt_stats(), rtc::Optional<RtpState>());
  send_stream.Reconfigure(stream_config);
}

}  // namespace test
}  // namespace webrtc
