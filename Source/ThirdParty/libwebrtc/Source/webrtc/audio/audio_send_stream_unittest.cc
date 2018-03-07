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

#include "audio/audio_send_stream.h"
#include "audio/audio_state.h"
#include "audio/conversion.h"
#include "call/fake_rtp_transport_controller_send.h"
#include "call/rtp_transport_controller_send_interface.h"
#include "logging/rtc_event_log/mock/mock_rtc_event_log.h"
#include "modules/audio_mixer/audio_mixer_impl.h"
#include "modules/audio_processing/include/audio_processing_statistics.h"
#include "modules/audio_processing/include/mock_audio_processing.h"
#include "modules/congestion_controller/include/mock/mock_congestion_observer.h"
#include "modules/congestion_controller/include/send_side_congestion_controller.h"
#include "modules/pacing/mock/mock_paced_sender.h"
#include "modules/rtp_rtcp/mocks/mock_rtcp_rtt_stats.h"
#include "modules/rtp_rtcp/mocks/mock_rtp_rtcp.h"
#include "rtc_base/ptr_util.h"
#include "rtc_base/task_queue.h"
#include "test/gtest.h"
#include "test/mock_audio_encoder.h"
#include "test/mock_audio_encoder_factory.h"
#include "test/mock_voe_channel_proxy.h"
#include "test/mock_voice_engine.h"
#include "voice_engine/transmit_mixer.h"

namespace webrtc {
namespace test {
namespace {

using testing::_;
using testing::Eq;
using testing::Ne;
using testing::Invoke;
using testing::Return;
using testing::StrEq;

const int kChannelId = 1;
const uint32_t kSsrc = 1234;
const char* kCName = "foo_name";
const int kAudioLevelId = 2;
const int kTransportSequenceNumberId = 4;
const int32_t kEchoDelayMedian = 254;
const int32_t kEchoDelayStdDev = -3;
const double kDivergentFilterFraction = 0.2f;
const double kEchoReturnLoss = -65;
const double kEchoReturnLossEnhancement = 101;
const double kResidualEchoLikelihood = -1.0f;
const double kResidualEchoLikelihoodMax = 23.0f;
const int32_t kSpeechInputLevel = 96;
const double kTotalInputEnergy = 0.25;
const double kTotalInputDuration = 0.5;
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
  MOCK_CONST_METHOD0(GetTotalInputEnergy, double());
  MOCK_CONST_METHOD0(GetTotalInputDuration, double());
  MOCK_CONST_METHOD0(typing_noise_detected, bool());
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
      .WillByDefault(Invoke(
          [](const SdpAudioFormat& format) -> rtc::Optional<AudioCodecInfo> {
            for (const auto& spec : kCodecSpecs) {
              if (format == spec.format) {
                return spec.info;
              }
            }
            return rtc::nullopt;
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
        audio_processing_(new rtc::RefCountedObject<MockAudioProcessing>()),
        simulated_clock_(123456),
        send_side_cc_(rtc::MakeUnique<SendSideCongestionController>(
            &simulated_clock_,
            nullptr /* observer */,
            &event_log_,
            &pacer_)),
        fake_transport_(&packet_router_, &pacer_, send_side_cc_.get()),
        bitrate_allocator_(&limit_observer_),
        worker_queue_("ConfigHelper_worker_queue"),
        audio_encoder_(nullptr) {
    using testing::Invoke;

    EXPECT_CALL(voice_engine_, audio_transport());

    AudioState::Config config;
    config.voice_engine = &voice_engine_;
    config.audio_mixer = AudioMixerImpl::Create();
    config.audio_processing = audio_processing_;
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
        AudioSendStream::Config::SendCodecSpec(kIsacPayloadType, kIsacFormat);
    stream_config_.voe_channel_id = kChannelId;
    stream_config_.rtp.ssrc = kSsrc;
    stream_config_.rtp.nack.rtp_history_ms = 200;
    stream_config_.rtp.c_name = kCName;
    stream_config_.rtp.extensions.push_back(
        RtpExtension(RtpExtension::kAudioLevelUri, kAudioLevelId));
    if (audio_bwe_enabled) {
      AddBweToConfig(&stream_config_);
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

  static void AddBweToConfig(AudioSendStream::Config* config) {
    config->rtp.extensions.push_back(
        RtpExtension(RtpExtension::kTransportSequenceNumberUri,
                     kTransportSequenceNumberId));
    config->send_codec_spec->transport_cc_enabled = true;
  }

  void SetupDefaultChannelProxy(bool audio_bwe_enabled) {
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
    EXPECT_CALL(*channel_proxy_, RegisterTransport(nullptr)).Times(2);
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
          .WillOnce(Invoke(
              [this](int payload_type, std::unique_ptr<AudioEncoder>* encoder) {
                this->audio_encoder_ = std::move(*encoder);
                return true;
              }));
    }
  }

  void SetupMockForModifyEncoder() {
    // Let ModifyEncoder to invoke mock audio encoder.
    EXPECT_CALL(*channel_proxy_, ModifyEncoder(_))
        .WillRepeatedly(Invoke(
            [this](rtc::FunctionView<void(std::unique_ptr<AudioEncoder>*)>
                       modifier) {
              if (this->audio_encoder_)
                modifier(&this->audio_encoder_);
            }));
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
    EXPECT_CALL(*channel_proxy_, GetANAStatistics())
        .WillRepeatedly(Return(ANAStats()));
    EXPECT_CALL(voice_engine_, transmit_mixer())
        .WillRepeatedly(Return(&transmit_mixer_));

    EXPECT_CALL(transmit_mixer_, AudioLevelFullRange())
        .WillRepeatedly(Return(kSpeechInputLevel));
    EXPECT_CALL(transmit_mixer_, GetTotalInputEnergy())
        .WillRepeatedly(Return(kTotalInputEnergy));
    EXPECT_CALL(transmit_mixer_, GetTotalInputDuration())
        .WillRepeatedly(Return(kTotalInputDuration));
    EXPECT_CALL(transmit_mixer_, typing_noise_detected())
        .WillRepeatedly(Return(true));

    audio_processing_stats_.echo_return_loss = kEchoReturnLoss;
    audio_processing_stats_.echo_return_loss_enhancement =
        kEchoReturnLossEnhancement;
    audio_processing_stats_.delay_median_ms = kEchoDelayMedian;
    audio_processing_stats_.delay_standard_deviation_ms = kEchoDelayStdDev;
    audio_processing_stats_.divergent_filter_fraction =
        kDivergentFilterFraction;
    audio_processing_stats_.residual_echo_likelihood = kResidualEchoLikelihood;
    audio_processing_stats_.residual_echo_likelihood_recent_max =
        kResidualEchoLikelihoodMax;

    EXPECT_CALL(*audio_processing_, GetStatistics(true))
        .WillRepeatedly(Return(audio_processing_stats_));
  }

 private:
  testing::StrictMock<MockVoiceEngine> voice_engine_;
  rtc::scoped_refptr<AudioState> audio_state_;
  AudioSendStream::Config stream_config_;
  testing::StrictMock<MockVoEChannelProxy>* channel_proxy_ = nullptr;
  rtc::scoped_refptr<MockAudioProcessing> audio_processing_;
  MockTransmitMixer transmit_mixer_;
  AudioProcessingStats audio_processing_stats_;
  SimulatedClock simulated_clock_;
  PacketRouter packet_router_;
  testing::NiceMock<MockPacedSender> pacer_;
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
  std::unique_ptr<AudioEncoder> audio_encoder_;
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
      AudioSendStream::Config::SendCodecSpec(kIsacPayloadType, kIsacFormat);
  config.send_codec_spec->nack_enabled = true;
  config.send_codec_spec->transport_cc_enabled = false;
  config.send_codec_spec->cng_payload_type = 42;
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
      helper.rtcp_rtt_stats(), rtc::nullopt);
}

TEST(AudioSendStreamTest, SendTelephoneEvent) {
  ConfigHelper helper(false, true);
  internal::AudioSendStream send_stream(
      helper.config(), helper.audio_state(), helper.worker_queue(),
      helper.transport(), helper.bitrate_allocator(), helper.event_log(),
      helper.rtcp_rtt_stats(), rtc::nullopt);
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
      helper.rtcp_rtt_stats(), rtc::nullopt);
  EXPECT_CALL(*helper.channel_proxy(), SetInputMute(true));
  send_stream.SetMuted(true);
}

TEST(AudioSendStreamTest, AudioBweCorrectObjectsOnChannelProxy) {
  ConfigHelper helper(true, true);
  internal::AudioSendStream send_stream(
      helper.config(), helper.audio_state(), helper.worker_queue(),
      helper.transport(), helper.bitrate_allocator(), helper.event_log(),
      helper.rtcp_rtt_stats(), rtc::nullopt);
}

TEST(AudioSendStreamTest, NoAudioBweCorrectObjectsOnChannelProxy) {
  ConfigHelper helper(false, true);
  internal::AudioSendStream send_stream(
      helper.config(), helper.audio_state(), helper.worker_queue(),
      helper.transport(), helper.bitrate_allocator(), helper.event_log(),
      helper.rtcp_rtt_stats(), rtc::nullopt);
}

TEST(AudioSendStreamTest, GetStats) {
  ConfigHelper helper(false, true);
  internal::AudioSendStream send_stream(
      helper.config(), helper.audio_state(), helper.worker_queue(),
      helper.transport(), helper.bitrate_allocator(), helper.event_log(),
      helper.rtcp_rtt_stats(), rtc::nullopt);
  helper.SetupMockForGetStats();
  AudioSendStream::Stats stats = send_stream.GetStats(true);
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
  EXPECT_EQ(kTotalInputEnergy, stats.total_input_energy);
  EXPECT_EQ(kTotalInputDuration, stats.total_input_duration);
  EXPECT_EQ(kEchoDelayMedian, stats.apm_statistics.delay_median_ms);
  EXPECT_EQ(kEchoDelayStdDev, stats.apm_statistics.delay_standard_deviation_ms);
  EXPECT_EQ(kEchoReturnLoss, stats.apm_statistics.echo_return_loss);
  EXPECT_EQ(kEchoReturnLossEnhancement,
            stats.apm_statistics.echo_return_loss_enhancement);
  EXPECT_EQ(kDivergentFilterFraction,
            stats.apm_statistics.divergent_filter_fraction);
  EXPECT_EQ(kResidualEchoLikelihood,
            stats.apm_statistics.residual_echo_likelihood);
  EXPECT_EQ(kResidualEchoLikelihoodMax,
            stats.apm_statistics.residual_echo_likelihood_recent_max);
  EXPECT_TRUE(stats.typing_noise_detected);
}

TEST(AudioSendStreamTest, SendCodecAppliesAudioNetworkAdaptor) {
  ConfigHelper helper(false, true);
  auto stream_config = helper.config();
  stream_config.send_codec_spec =
      AudioSendStream::Config::SendCodecSpec(0, kOpusFormat);
  const std::string kAnaConfigString = "abcde";
  const std::string kAnaReconfigString = "12345";

  stream_config.audio_network_adaptor_config = kAnaConfigString;

  EXPECT_CALL(helper.mock_encoder_factory(), MakeAudioEncoderMock(_, _, _))
      .WillOnce(Invoke([&kAnaConfigString, &kAnaReconfigString](
                           int payload_type, const SdpAudioFormat& format,
                           std::unique_ptr<AudioEncoder>* return_value) {
        auto mock_encoder = SetupAudioEncoderMock(payload_type, format);
        EXPECT_CALL(*mock_encoder,
                    EnableAudioNetworkAdaptor(StrEq(kAnaConfigString), _))
            .WillOnce(Return(true));
        EXPECT_CALL(*mock_encoder,
                    EnableAudioNetworkAdaptor(StrEq(kAnaReconfigString), _))
            .WillOnce(Return(true));
        *return_value = std::move(mock_encoder);
      }));

  internal::AudioSendStream send_stream(
      stream_config, helper.audio_state(), helper.worker_queue(),
      helper.transport(), helper.bitrate_allocator(), helper.event_log(),
      helper.rtcp_rtt_stats(), rtc::nullopt);

  stream_config.audio_network_adaptor_config = kAnaReconfigString;

  helper.SetupMockForModifyEncoder();
  send_stream.Reconfigure(stream_config);
}

// VAD is applied when codec is mono and the CNG frequency matches the codec
// clock rate.
TEST(AudioSendStreamTest, SendCodecCanApplyVad) {
  ConfigHelper helper(false, false);
  auto stream_config = helper.config();
  stream_config.send_codec_spec =
      AudioSendStream::Config::SendCodecSpec(9, kG722Format);
  stream_config.send_codec_spec->cng_payload_type = 105;
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
      helper.rtcp_rtt_stats(), rtc::nullopt);

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
      helper.rtcp_rtt_stats(), rtc::nullopt);
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
      helper.rtcp_rtt_stats(), rtc::nullopt);
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
      AudioSendStream::Config::SendCodecSpec(9, kG722Format);
  stream_config.send_codec_spec->cng_payload_type = 105;
  internal::AudioSendStream send_stream(
      stream_config, helper.audio_state(), helper.worker_queue(),
      helper.transport(), helper.bitrate_allocator(), helper.event_log(),
      helper.rtcp_rtt_stats(), rtc::nullopt);
  send_stream.Reconfigure(stream_config);
}

TEST(AudioSendStreamTest, ReconfigureTransportCcResetsFirst) {
  ConfigHelper helper(false, true);
  internal::AudioSendStream send_stream(
      helper.config(), helper.audio_state(), helper.worker_queue(),
      helper.transport(), helper.bitrate_allocator(), helper.event_log(),
      helper.rtcp_rtt_stats(), rtc::nullopt);
  auto new_config = helper.config();
  ConfigHelper::AddBweToConfig(&new_config);
  EXPECT_CALL(*helper.channel_proxy(),
              EnableSendTransportSequenceNumber(kTransportSequenceNumberId))
      .Times(1);
  {
    ::testing::InSequence seq;
    EXPECT_CALL(*helper.channel_proxy(), ResetSenderCongestionControlObjects())
        .Times(1);
    EXPECT_CALL(*helper.channel_proxy(), RegisterSenderCongestionControlObjects(
        helper.transport(), Ne(nullptr)))
        .Times(1);
  }
  send_stream.Reconfigure(new_config);
}


}  // namespace test
}  // namespace webrtc
