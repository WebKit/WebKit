/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "audio/audio_send_stream.h"

#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "api/task_queue/default_task_queue_factory.h"
#include "api/test/mock_frame_encryptor.h"
#include "audio/audio_state.h"
#include "audio/conversion.h"
#include "audio/mock_voe_channel_proxy.h"
#include "call/test/mock_rtp_transport_controller_send.h"
#include "logging/rtc_event_log/mock/mock_rtc_event_log.h"
#include "modules/audio_device/include/mock_audio_device.h"
#include "modules/audio_mixer/audio_mixer_impl.h"
#include "modules/audio_mixer/sine_wave_generator.h"
#include "modules/audio_processing/include/audio_processing_statistics.h"
#include "modules/audio_processing/include/mock_audio_processing.h"
#include "modules/rtp_rtcp/mocks/mock_rtcp_bandwidth_observer.h"
#include "modules/rtp_rtcp/mocks/mock_rtp_rtcp.h"
#include "rtc_base/task_queue_for_test.h"
#include "system_wrappers/include/clock.h"
#include "test/field_trial.h"
#include "test/gtest.h"
#include "test/mock_audio_encoder.h"
#include "test/mock_audio_encoder_factory.h"

namespace webrtc {
namespace test {
namespace {

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Eq;
using ::testing::Field;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::Ne;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::StrEq;

static const float kTolerance = 0.0001f;

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
const CallSendStatistics kCallStats = {112, 12, 13456, 17890};
const ReportBlock kReportBlock = {456, 780, 123, 567, 890, 132, 143, 13354};
const int kTelephoneEventPayloadType = 123;
const int kTelephoneEventPayloadFrequency = 65432;
const int kTelephoneEventCode = 45;
const int kTelephoneEventDuration = 6789;
constexpr int kIsacPayloadType = 103;
const SdpAudioFormat kIsacFormat = {"isac", 16000, 1};
const SdpAudioFormat kOpusFormat = {"opus", 48000, 2};
const SdpAudioFormat kG722Format = {"g722", 8000, 1};
const AudioCodecSpec kCodecSpecs[] = {
    {kIsacFormat, {16000, 1, 32000, 10000, 32000}},
    {kOpusFormat, {48000, 1, 32000, 6000, 510000}},
    {kG722Format, {16000, 1, 64000}}};

// TODO(dklee): This mirrors calculation in audio_send_stream.cc, which
// should be made more precise in the future. This can be changed when that
// logic is more accurate.
const DataSize kOverheadPerPacket = DataSize::Bytes(20 + 8 + 10 + 12);
const TimeDelta kMinFrameLength = TimeDelta::Millis(20);
const TimeDelta kMaxFrameLength = TimeDelta::Millis(120);
const DataRate kMinOverheadRate = kOverheadPerPacket / kMaxFrameLength;
const DataRate kMaxOverheadRate = kOverheadPerPacket / kMinFrameLength;

class MockLimitObserver : public BitrateAllocator::LimitObserver {
 public:
  MOCK_METHOD(void,
              OnAllocationLimitsChanged,
              (BitrateAllocationLimits),
              (override));
};

std::unique_ptr<MockAudioEncoder> SetupAudioEncoderMock(
    int payload_type,
    const SdpAudioFormat& format) {
  for (const auto& spec : kCodecSpecs) {
    if (format == spec.format) {
      std::unique_ptr<MockAudioEncoder> encoder(
          new ::testing::NiceMock<MockAudioEncoder>());
      ON_CALL(*encoder.get(), SampleRateHz())
          .WillByDefault(Return(spec.info.sample_rate_hz));
      ON_CALL(*encoder.get(), NumChannels())
          .WillByDefault(Return(spec.info.num_channels));
      ON_CALL(*encoder.get(), RtpTimestampRateHz())
          .WillByDefault(Return(spec.format.clockrate_hz));
      ON_CALL(*encoder.get(), GetFrameLengthRange())
          .WillByDefault(Return(absl::optional<std::pair<TimeDelta, TimeDelta>>{
              {TimeDelta::Millis(20), TimeDelta::Millis(120)}}));
      return encoder;
    }
  }
  return nullptr;
}

rtc::scoped_refptr<MockAudioEncoderFactory> SetupEncoderFactoryMock() {
  rtc::scoped_refptr<MockAudioEncoderFactory> factory =
      rtc::make_ref_counted<MockAudioEncoderFactory>();
  ON_CALL(*factory.get(), GetSupportedEncoders())
      .WillByDefault(Return(std::vector<AudioCodecSpec>(
          std::begin(kCodecSpecs), std::end(kCodecSpecs))));
  ON_CALL(*factory.get(), QueryAudioEncoder(_))
      .WillByDefault(Invoke(
          [](const SdpAudioFormat& format) -> absl::optional<AudioCodecInfo> {
            for (const auto& spec : kCodecSpecs) {
              if (format == spec.format) {
                return spec.info;
              }
            }
            return absl::nullopt;
          }));
  ON_CALL(*factory.get(), MakeAudioEncoderMock(_, _, _, _))
      .WillByDefault(Invoke([](int payload_type, const SdpAudioFormat& format,
                               absl::optional<AudioCodecPairId> codec_pair_id,
                               std::unique_ptr<AudioEncoder>* return_value) {
        *return_value = SetupAudioEncoderMock(payload_type, format);
      }));
  return factory;
}

struct ConfigHelper {
  ConfigHelper(bool audio_bwe_enabled,
               bool expect_set_encoder_call,
               bool use_null_audio_processing)
      : clock_(1000000),
        task_queue_factory_(CreateDefaultTaskQueueFactory()),
        stream_config_(/*send_transport=*/nullptr),
        audio_processing_(
            use_null_audio_processing
                ? nullptr
                : rtc::make_ref_counted<NiceMock<MockAudioProcessing>>()),
        bitrate_allocator_(&limit_observer_),
        worker_queue_(task_queue_factory_->CreateTaskQueue(
            "ConfigHelper_worker_queue",
            TaskQueueFactory::Priority::NORMAL)),
        audio_encoder_(nullptr) {
    using ::testing::Invoke;

    AudioState::Config config;
    config.audio_mixer = AudioMixerImpl::Create();
    config.audio_processing = audio_processing_;
    config.audio_device_module = rtc::make_ref_counted<MockAudioDeviceModule>();
    audio_state_ = AudioState::Create(config);

    SetupDefaultChannelSend(audio_bwe_enabled);
    SetupMockForSetupSendCodec(expect_set_encoder_call);
    SetupMockForCallEncoder();

    // Use ISAC as default codec so as to prevent unnecessary |channel_proxy_|
    // calls from the default ctor behavior.
    stream_config_.send_codec_spec =
        AudioSendStream::Config::SendCodecSpec(kIsacPayloadType, kIsacFormat);
    stream_config_.rtp.ssrc = kSsrc;
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

  std::unique_ptr<internal::AudioSendStream> CreateAudioSendStream() {
    EXPECT_CALL(rtp_transport_, GetWorkerQueue())
        .WillRepeatedly(Return(&worker_queue_));
    return std::unique_ptr<internal::AudioSendStream>(
        new internal::AudioSendStream(
            Clock::GetRealTimeClock(), stream_config_, audio_state_,
            task_queue_factory_.get(), &rtp_transport_, &bitrate_allocator_,
            &event_log_, absl::nullopt,
            std::unique_ptr<voe::ChannelSendInterface>(channel_send_)));
  }

  AudioSendStream::Config& config() { return stream_config_; }
  MockAudioEncoderFactory& mock_encoder_factory() {
    return *static_cast<MockAudioEncoderFactory*>(
        stream_config_.encoder_factory.get());
  }
  MockRtpRtcpInterface* rtp_rtcp() { return &rtp_rtcp_; }
  MockChannelSend* channel_send() { return channel_send_; }
  RtpTransportControllerSendInterface* transport() { return &rtp_transport_; }

  static void AddBweToConfig(AudioSendStream::Config* config) {
    config->rtp.extensions.push_back(RtpExtension(
        RtpExtension::kTransportSequenceNumberUri, kTransportSequenceNumberId));
    config->send_codec_spec->transport_cc_enabled = true;
  }

  void SetupDefaultChannelSend(bool audio_bwe_enabled) {
    EXPECT_TRUE(channel_send_ == nullptr);
    channel_send_ = new ::testing::StrictMock<MockChannelSend>();
    EXPECT_CALL(*channel_send_, GetRtpRtcp()).WillRepeatedly(Invoke([this]() {
      return &this->rtp_rtcp_;
    }));
    EXPECT_CALL(rtp_rtcp_, SSRC).WillRepeatedly(Return(kSsrc));
    EXPECT_CALL(*channel_send_, SetRTCP_CNAME(StrEq(kCName))).Times(1);
    EXPECT_CALL(*channel_send_, SetFrameEncryptor(_)).Times(1);
    EXPECT_CALL(*channel_send_, SetEncoderToPacketizerFrameTransformer(_))
        .Times(1);
    EXPECT_CALL(rtp_rtcp_, SetExtmapAllowMixed(false)).Times(1);
    EXPECT_CALL(*channel_send_,
                SetSendAudioLevelIndicationStatus(true, kAudioLevelId))
        .Times(1);
    EXPECT_CALL(rtp_transport_, GetBandwidthObserver())
        .WillRepeatedly(Return(&bandwidth_observer_));
    if (audio_bwe_enabled) {
      EXPECT_CALL(rtp_rtcp_,
                  RegisterRtpHeaderExtension(TransportSequenceNumber::kUri,
                                             kTransportSequenceNumberId))
          .Times(1);
      EXPECT_CALL(*channel_send_,
                  RegisterSenderCongestionControlObjects(
                      &rtp_transport_, Eq(&bandwidth_observer_)))
          .Times(1);
    } else {
      EXPECT_CALL(*channel_send_, RegisterSenderCongestionControlObjects(
                                      &rtp_transport_, Eq(nullptr)))
          .Times(1);
    }
    EXPECT_CALL(*channel_send_, ResetSenderCongestionControlObjects()).Times(1);
    EXPECT_CALL(rtp_rtcp_, SetRid(std::string())).Times(1);
  }

  void SetupMockForSetupSendCodec(bool expect_set_encoder_call) {
    if (expect_set_encoder_call) {
      EXPECT_CALL(*channel_send_, SetEncoder)
          .WillOnce(
              [this](int payload_type, std::unique_ptr<AudioEncoder> encoder) {
                this->audio_encoder_ = std::move(encoder);
                return true;
              });
    }
  }

  void SetupMockForCallEncoder() {
    // Let ModifyEncoder to invoke mock audio encoder.
    EXPECT_CALL(*channel_send_, CallEncoder(_))
        .WillRepeatedly(
            [this](rtc::FunctionView<void(AudioEncoder*)> modifier) {
              if (this->audio_encoder_)
                modifier(this->audio_encoder_.get());
            });
  }

  void SetupMockForSendTelephoneEvent() {
    EXPECT_TRUE(channel_send_);
    EXPECT_CALL(*channel_send_, SetSendTelephoneEventPayloadType(
                                    kTelephoneEventPayloadType,
                                    kTelephoneEventPayloadFrequency));
    EXPECT_CALL(
        *channel_send_,
        SendTelephoneEventOutband(kTelephoneEventCode, kTelephoneEventDuration))
        .WillOnce(Return(true));
  }

  void SetupMockForGetStats(bool use_null_audio_processing) {
    using ::testing::DoAll;
    using ::testing::SetArgPointee;
    using ::testing::SetArgReferee;

    std::vector<ReportBlock> report_blocks;
    webrtc::ReportBlock block = kReportBlock;
    report_blocks.push_back(block);  // Has wrong SSRC.
    block.source_SSRC = kSsrc;
    report_blocks.push_back(block);  // Correct block.
    block.fraction_lost = 0;
    report_blocks.push_back(block);  // Duplicate SSRC, bad fraction_lost.

    EXPECT_TRUE(channel_send_);
    EXPECT_CALL(*channel_send_, GetRTCPStatistics())
        .WillRepeatedly(Return(kCallStats));
    EXPECT_CALL(*channel_send_, GetRemoteRTCPReportBlocks())
        .WillRepeatedly(Return(report_blocks));
    EXPECT_CALL(*channel_send_, GetANAStatistics())
        .WillRepeatedly(Return(ANAStats()));
    EXPECT_CALL(*channel_send_, GetBitrate()).WillRepeatedly(Return(0));

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
    if (!use_null_audio_processing) {
      ASSERT_TRUE(audio_processing_);
      EXPECT_CALL(*audio_processing_, GetStatistics(true))
          .WillRepeatedly(Return(audio_processing_stats_));
    }
  }

  TaskQueueForTest* worker() { return &worker_queue_; }

 private:
  SimulatedClock clock_;
  std::unique_ptr<TaskQueueFactory> task_queue_factory_;
  rtc::scoped_refptr<AudioState> audio_state_;
  AudioSendStream::Config stream_config_;
  ::testing::StrictMock<MockChannelSend>* channel_send_ = nullptr;
  rtc::scoped_refptr<MockAudioProcessing> audio_processing_;
  AudioProcessingStats audio_processing_stats_;
  ::testing::StrictMock<MockRtcpBandwidthObserver> bandwidth_observer_;
  ::testing::NiceMock<MockRtcEventLog> event_log_;
  ::testing::NiceMock<MockRtpTransportControllerSend> rtp_transport_;
  ::testing::NiceMock<MockRtpRtcpInterface> rtp_rtcp_;
  ::testing::NiceMock<MockLimitObserver> limit_observer_;
  BitrateAllocator bitrate_allocator_;
  // |worker_queue| is defined last to ensure all pending tasks are cancelled
  // and deleted before any other members.
  TaskQueueForTest worker_queue_;
  std::unique_ptr<AudioEncoder> audio_encoder_;
};

// The audio level ranges linearly [0,32767].
std::unique_ptr<AudioFrame> CreateAudioFrame1kHzSineWave(int16_t audio_level,
                                                         int duration_ms,
                                                         int sample_rate_hz,
                                                         size_t num_channels) {
  size_t samples_per_channel = sample_rate_hz / (1000 / duration_ms);
  std::vector<int16_t> audio_data(samples_per_channel * num_channels, 0);
  std::unique_ptr<AudioFrame> audio_frame = std::make_unique<AudioFrame>();
  audio_frame->UpdateFrame(0 /* RTP timestamp */, &audio_data[0],
                           samples_per_channel, sample_rate_hz,
                           AudioFrame::SpeechType::kNormalSpeech,
                           AudioFrame::VADActivity::kVadUnknown, num_channels);
  SineWaveGenerator wave_generator(1000.0, audio_level);
  wave_generator.GenerateNextFrame(audio_frame.get());
  return audio_frame;
}

}  // namespace

TEST(AudioSendStreamTest, ConfigToString) {
  AudioSendStream::Config config(/*send_transport=*/nullptr);
  config.rtp.ssrc = kSsrc;
  config.rtp.c_name = kCName;
  config.min_bitrate_bps = 12000;
  config.max_bitrate_bps = 34000;
  config.has_dscp = true;
  config.send_codec_spec =
      AudioSendStream::Config::SendCodecSpec(kIsacPayloadType, kIsacFormat);
  config.send_codec_spec->nack_enabled = true;
  config.send_codec_spec->transport_cc_enabled = false;
  config.send_codec_spec->cng_payload_type = 42;
  config.send_codec_spec->red_payload_type = 43;
  config.encoder_factory = MockAudioEncoderFactory::CreateUnusedFactory();
  config.rtp.extmap_allow_mixed = true;
  config.rtp.extensions.push_back(
      RtpExtension(RtpExtension::kAudioLevelUri, kAudioLevelId));
  config.rtcp_report_interval_ms = 2500;
  EXPECT_EQ(
      "{rtp: {ssrc: 1234, extmap-allow-mixed: true, extensions: [{uri: "
      "urn:ietf:params:rtp-hdrext:ssrc-audio-level, id: 2}], "
      "c_name: foo_name}, rtcp_report_interval_ms: 2500, "
      "send_transport: null, "
      "min_bitrate_bps: 12000, max_bitrate_bps: 34000, has "
      "audio_network_adaptor_config: false, has_dscp: true, "
      "send_codec_spec: {nack_enabled: true, transport_cc_enabled: false, "
      "cng_payload_type: 42, red_payload_type: 43, payload_type: 103, "
      "format: {name: isac, clockrate_hz: 16000, num_channels: 1, "
      "parameters: {}}}}",
      config.ToString());
}

TEST(AudioSendStreamTest, ConstructDestruct) {
  for (bool use_null_audio_processing : {false, true}) {
    ConfigHelper helper(false, true, use_null_audio_processing);
    auto send_stream = helper.CreateAudioSendStream();
  }
}

TEST(AudioSendStreamTest, SendTelephoneEvent) {
  for (bool use_null_audio_processing : {false, true}) {
    ConfigHelper helper(false, true, use_null_audio_processing);
    auto send_stream = helper.CreateAudioSendStream();
    helper.SetupMockForSendTelephoneEvent();
    EXPECT_TRUE(send_stream->SendTelephoneEvent(
        kTelephoneEventPayloadType, kTelephoneEventPayloadFrequency,
        kTelephoneEventCode, kTelephoneEventDuration));
  }
}

TEST(AudioSendStreamTest, SetMuted) {
  for (bool use_null_audio_processing : {false, true}) {
    ConfigHelper helper(false, true, use_null_audio_processing);
    auto send_stream = helper.CreateAudioSendStream();
    EXPECT_CALL(*helper.channel_send(), SetInputMute(true));
    send_stream->SetMuted(true);
  }
}

TEST(AudioSendStreamTest, AudioBweCorrectObjectsOnChannelProxy) {
  for (bool use_null_audio_processing : {false, true}) {
    ConfigHelper helper(true, true, use_null_audio_processing);
    auto send_stream = helper.CreateAudioSendStream();
  }
}

TEST(AudioSendStreamTest, NoAudioBweCorrectObjectsOnChannelProxy) {
  for (bool use_null_audio_processing : {false, true}) {
    ConfigHelper helper(false, true, use_null_audio_processing);
    auto send_stream = helper.CreateAudioSendStream();
  }
}

TEST(AudioSendStreamTest, GetStats) {
  for (bool use_null_audio_processing : {false, true}) {
    ConfigHelper helper(false, true, use_null_audio_processing);
    auto send_stream = helper.CreateAudioSendStream();
    helper.SetupMockForGetStats(use_null_audio_processing);
    AudioSendStream::Stats stats = send_stream->GetStats(true);
    EXPECT_EQ(kSsrc, stats.local_ssrc);
    EXPECT_EQ(kCallStats.payload_bytes_sent, stats.payload_bytes_sent);
    EXPECT_EQ(kCallStats.header_and_padding_bytes_sent,
              stats.header_and_padding_bytes_sent);
    EXPECT_EQ(kCallStats.packetsSent, stats.packets_sent);
    EXPECT_EQ(kReportBlock.cumulative_num_packets_lost, stats.packets_lost);
    EXPECT_EQ(Q8ToFloat(kReportBlock.fraction_lost), stats.fraction_lost);
    EXPECT_EQ(kIsacFormat.name, stats.codec_name);
    EXPECT_EQ(static_cast<int32_t>(kReportBlock.interarrival_jitter /
                                   (kIsacFormat.clockrate_hz / 1000)),
              stats.jitter_ms);
    EXPECT_EQ(kCallStats.rttMs, stats.rtt_ms);
    EXPECT_EQ(0, stats.audio_level);
    EXPECT_EQ(0, stats.total_input_energy);
    EXPECT_EQ(0, stats.total_input_duration);

    if (!use_null_audio_processing) {
      EXPECT_EQ(kEchoDelayMedian, stats.apm_statistics.delay_median_ms);
      EXPECT_EQ(kEchoDelayStdDev,
                stats.apm_statistics.delay_standard_deviation_ms);
      EXPECT_EQ(kEchoReturnLoss, stats.apm_statistics.echo_return_loss);
      EXPECT_EQ(kEchoReturnLossEnhancement,
                stats.apm_statistics.echo_return_loss_enhancement);
      EXPECT_EQ(kDivergentFilterFraction,
                stats.apm_statistics.divergent_filter_fraction);
      EXPECT_EQ(kResidualEchoLikelihood,
                stats.apm_statistics.residual_echo_likelihood);
      EXPECT_EQ(kResidualEchoLikelihoodMax,
                stats.apm_statistics.residual_echo_likelihood_recent_max);
      EXPECT_FALSE(stats.typing_noise_detected);
    }
  }
}

TEST(AudioSendStreamTest, GetStatsAudioLevel) {
  for (bool use_null_audio_processing : {false, true}) {
    ConfigHelper helper(false, true, use_null_audio_processing);
    auto send_stream = helper.CreateAudioSendStream();
    helper.SetupMockForGetStats(use_null_audio_processing);
    EXPECT_CALL(*helper.channel_send(), ProcessAndEncodeAudio)
        .Times(AnyNumber());

    constexpr int kSampleRateHz = 48000;
    constexpr size_t kNumChannels = 1;

    constexpr int16_t kSilentAudioLevel = 0;
    constexpr int16_t kMaxAudioLevel = 32767;  // Audio level is [0,32767].
    constexpr int kAudioFrameDurationMs = 10;

    // Process 10 audio frames (100 ms) of silence. After this, on the next
    // (11-th) frame, the audio level will be updated with the maximum audio
    // level of the first 11 frames. See AudioLevel.
    for (size_t i = 0; i < 10; ++i) {
      send_stream->SendAudioData(
          CreateAudioFrame1kHzSineWave(kSilentAudioLevel, kAudioFrameDurationMs,
                                       kSampleRateHz, kNumChannels));
    }
    AudioSendStream::Stats stats = send_stream->GetStats();
    EXPECT_EQ(kSilentAudioLevel, stats.audio_level);
    EXPECT_NEAR(0.0f, stats.total_input_energy, kTolerance);
    EXPECT_NEAR(0.1f, stats.total_input_duration,
                kTolerance);  // 100 ms = 0.1 s

    // Process 10 audio frames (100 ms) of maximum audio level.
    // Note that AudioLevel updates the audio level every 11th frame, processing
    // 10 frames above was needed to see a non-zero audio level here.
    for (size_t i = 0; i < 10; ++i) {
      send_stream->SendAudioData(CreateAudioFrame1kHzSineWave(
          kMaxAudioLevel, kAudioFrameDurationMs, kSampleRateHz, kNumChannels));
    }
    stats = send_stream->GetStats();
    EXPECT_EQ(kMaxAudioLevel, stats.audio_level);
    // Energy increases by energy*duration, where energy is audio level in
    // [0,1].
    EXPECT_NEAR(0.1f, stats.total_input_energy, kTolerance);  // 0.1 s of max
    EXPECT_NEAR(0.2f, stats.total_input_duration,
                kTolerance);  // 200 ms = 0.2 s
  }
}

TEST(AudioSendStreamTest, SendCodecAppliesAudioNetworkAdaptor) {
  for (bool use_null_audio_processing : {false, true}) {
    ConfigHelper helper(true, true, use_null_audio_processing);
    helper.config().send_codec_spec =
        AudioSendStream::Config::SendCodecSpec(0, kOpusFormat);
    const std::string kAnaConfigString = "abcde";
    const std::string kAnaReconfigString = "12345";

    helper.config().audio_network_adaptor_config = kAnaConfigString;

    EXPECT_CALL(helper.mock_encoder_factory(), MakeAudioEncoderMock(_, _, _, _))
        .WillOnce(Invoke([&kAnaConfigString, &kAnaReconfigString](
                             int payload_type, const SdpAudioFormat& format,
                             absl::optional<AudioCodecPairId> codec_pair_id,
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

    auto send_stream = helper.CreateAudioSendStream();

    auto stream_config = helper.config();
    stream_config.audio_network_adaptor_config = kAnaReconfigString;

    send_stream->Reconfigure(stream_config);
  }
}

TEST(AudioSendStreamTest, AudioNetworkAdaptorReceivesOverhead) {
  for (bool use_null_audio_processing : {false, true}) {
    ConfigHelper helper(true, true, use_null_audio_processing);
    helper.config().send_codec_spec =
        AudioSendStream::Config::SendCodecSpec(0, kOpusFormat);
    const std::string kAnaConfigString = "abcde";

    EXPECT_CALL(helper.mock_encoder_factory(), MakeAudioEncoderMock(_, _, _, _))
        .WillOnce(Invoke(
            [&kAnaConfigString](int payload_type, const SdpAudioFormat& format,
                                absl::optional<AudioCodecPairId> codec_pair_id,
                                std::unique_ptr<AudioEncoder>* return_value) {
              auto mock_encoder = SetupAudioEncoderMock(payload_type, format);
              InSequence s;
              EXPECT_CALL(
                  *mock_encoder,
                  OnReceivedOverhead(Eq(kOverheadPerPacket.bytes<size_t>())))
                  .Times(2);
              EXPECT_CALL(*mock_encoder,
                          EnableAudioNetworkAdaptor(StrEq(kAnaConfigString), _))
                  .WillOnce(Return(true));
              // Note: Overhead is received AFTER ANA has been enabled.
              EXPECT_CALL(
                  *mock_encoder,
                  OnReceivedOverhead(Eq(kOverheadPerPacket.bytes<size_t>())))
                  .WillOnce(Return());
              *return_value = std::move(mock_encoder);
            }));
    EXPECT_CALL(*helper.rtp_rtcp(), ExpectedPerPacketOverhead)
        .WillRepeatedly(Return(kOverheadPerPacket.bytes<size_t>()));

    auto send_stream = helper.CreateAudioSendStream();

    auto stream_config = helper.config();
    stream_config.audio_network_adaptor_config = kAnaConfigString;

    send_stream->Reconfigure(stream_config);
  }
}

// VAD is applied when codec is mono and the CNG frequency matches the codec
// clock rate.
TEST(AudioSendStreamTest, SendCodecCanApplyVad) {
  for (bool use_null_audio_processing : {false, true}) {
    ConfigHelper helper(false, false, use_null_audio_processing);
    helper.config().send_codec_spec =
        AudioSendStream::Config::SendCodecSpec(9, kG722Format);
    helper.config().send_codec_spec->cng_payload_type = 105;
    std::unique_ptr<AudioEncoder> stolen_encoder;
    EXPECT_CALL(*helper.channel_send(), SetEncoder)
        .WillOnce([&stolen_encoder](int payload_type,
                                    std::unique_ptr<AudioEncoder> encoder) {
          stolen_encoder = std::move(encoder);
          return true;
        });
    EXPECT_CALL(*helper.channel_send(), RegisterCngPayloadType(105, 8000));

    auto send_stream = helper.CreateAudioSendStream();

    // We cannot truly determine if the encoder created is an AudioEncoderCng.
    // It is the only reasonable implementation that will return something from
    // ReclaimContainedEncoders, though.
    ASSERT_TRUE(stolen_encoder);
    EXPECT_FALSE(stolen_encoder->ReclaimContainedEncoders().empty());
  }
}

TEST(AudioSendStreamTest, DoesNotPassHigherBitrateThanMaxBitrate) {
  for (bool use_null_audio_processing : {false, true}) {
    ConfigHelper helper(false, true, use_null_audio_processing);
    auto send_stream = helper.CreateAudioSendStream();
    EXPECT_CALL(
        *helper.channel_send(),
        OnBitrateAllocation(
            Field(&BitrateAllocationUpdate::target_bitrate,
                  Eq(DataRate::BitsPerSec(helper.config().max_bitrate_bps)))));
    BitrateAllocationUpdate update;
    update.target_bitrate =
        DataRate::BitsPerSec(helper.config().max_bitrate_bps + 5000);
    update.packet_loss_ratio = 0;
    update.round_trip_time = TimeDelta::Millis(50);
    update.bwe_period = TimeDelta::Millis(6000);
    helper.worker()->SendTask([&] { send_stream->OnBitrateUpdated(update); },
                              RTC_FROM_HERE);
  }
}

TEST(AudioSendStreamTest, SSBweTargetInRangeRespected) {
  for (bool use_null_audio_processing : {false, true}) {
    ConfigHelper helper(true, true, use_null_audio_processing);
    auto send_stream = helper.CreateAudioSendStream();
    EXPECT_CALL(
        *helper.channel_send(),
        OnBitrateAllocation(Field(
            &BitrateAllocationUpdate::target_bitrate,
            Eq(DataRate::BitsPerSec(helper.config().max_bitrate_bps - 5000)))));
    BitrateAllocationUpdate update;
    update.target_bitrate =
        DataRate::BitsPerSec(helper.config().max_bitrate_bps - 5000);
    helper.worker()->SendTask([&] { send_stream->OnBitrateUpdated(update); },
                              RTC_FROM_HERE);
  }
}

TEST(AudioSendStreamTest, SSBweFieldTrialMinRespected) {
  ScopedFieldTrials field_trials(
      "WebRTC-Audio-Allocation/min:6kbps,max:64kbps/");
  for (bool use_null_audio_processing : {false, true}) {
    ConfigHelper helper(true, true, use_null_audio_processing);
    auto send_stream = helper.CreateAudioSendStream();
    EXPECT_CALL(
        *helper.channel_send(),
        OnBitrateAllocation(Field(&BitrateAllocationUpdate::target_bitrate,
                                  Eq(DataRate::KilobitsPerSec(6)))));
    BitrateAllocationUpdate update;
    update.target_bitrate = DataRate::KilobitsPerSec(1);
    helper.worker()->SendTask([&] { send_stream->OnBitrateUpdated(update); },
                              RTC_FROM_HERE);
  }
}

TEST(AudioSendStreamTest, SSBweFieldTrialMaxRespected) {
  ScopedFieldTrials field_trials(
      "WebRTC-Audio-Allocation/min:6kbps,max:64kbps/");
  for (bool use_null_audio_processing : {false, true}) {
    ConfigHelper helper(true, true, use_null_audio_processing);
    auto send_stream = helper.CreateAudioSendStream();
    EXPECT_CALL(
        *helper.channel_send(),
        OnBitrateAllocation(Field(&BitrateAllocationUpdate::target_bitrate,
                                  Eq(DataRate::KilobitsPerSec(64)))));
    BitrateAllocationUpdate update;
    update.target_bitrate = DataRate::KilobitsPerSec(128);
    helper.worker()->SendTask([&] { send_stream->OnBitrateUpdated(update); },
                              RTC_FROM_HERE);
  }
}

TEST(AudioSendStreamTest, SSBweWithOverhead) {
  ScopedFieldTrials field_trials(
      "WebRTC-Audio-LegacyOverhead/Disabled/");
  for (bool use_null_audio_processing : {false, true}) {
    ConfigHelper helper(true, true, use_null_audio_processing);
    EXPECT_CALL(*helper.rtp_rtcp(), ExpectedPerPacketOverhead)
        .WillRepeatedly(Return(kOverheadPerPacket.bytes<size_t>()));
    auto send_stream = helper.CreateAudioSendStream();
    const DataRate bitrate =
        DataRate::BitsPerSec(helper.config().max_bitrate_bps) +
        kMaxOverheadRate;
    EXPECT_CALL(*helper.channel_send(),
                OnBitrateAllocation(Field(
                    &BitrateAllocationUpdate::target_bitrate, Eq(bitrate))));
    BitrateAllocationUpdate update;
    update.target_bitrate = bitrate;
    helper.worker()->SendTask([&] { send_stream->OnBitrateUpdated(update); },
                              RTC_FROM_HERE);
  }
}

TEST(AudioSendStreamTest, SSBweWithOverheadMinRespected) {
  ScopedFieldTrials field_trials(
      "WebRTC-Audio-LegacyOverhead/Disabled/"
      "WebRTC-Audio-Allocation/min:6kbps,max:64kbps/");
  for (bool use_null_audio_processing : {false, true}) {
    ConfigHelper helper(true, true, use_null_audio_processing);
    EXPECT_CALL(*helper.rtp_rtcp(), ExpectedPerPacketOverhead)
        .WillRepeatedly(Return(kOverheadPerPacket.bytes<size_t>()));
    auto send_stream = helper.CreateAudioSendStream();
    const DataRate bitrate = DataRate::KilobitsPerSec(6) + kMinOverheadRate;
    EXPECT_CALL(*helper.channel_send(),
                OnBitrateAllocation(Field(
                    &BitrateAllocationUpdate::target_bitrate, Eq(bitrate))));
    BitrateAllocationUpdate update;
    update.target_bitrate = DataRate::KilobitsPerSec(1);
    helper.worker()->SendTask([&] { send_stream->OnBitrateUpdated(update); },
                              RTC_FROM_HERE);
  }
}

TEST(AudioSendStreamTest, SSBweWithOverheadMaxRespected) {
  ScopedFieldTrials field_trials(
      "WebRTC-Audio-LegacyOverhead/Disabled/"
      "WebRTC-Audio-Allocation/min:6kbps,max:64kbps/");
  for (bool use_null_audio_processing : {false, true}) {
    ConfigHelper helper(true, true, use_null_audio_processing);
    EXPECT_CALL(*helper.rtp_rtcp(), ExpectedPerPacketOverhead)
        .WillRepeatedly(Return(kOverheadPerPacket.bytes<size_t>()));
    auto send_stream = helper.CreateAudioSendStream();
    const DataRate bitrate = DataRate::KilobitsPerSec(64) + kMaxOverheadRate;
    EXPECT_CALL(*helper.channel_send(),
                OnBitrateAllocation(Field(
                    &BitrateAllocationUpdate::target_bitrate, Eq(bitrate))));
    BitrateAllocationUpdate update;
    update.target_bitrate = DataRate::KilobitsPerSec(128);
    helper.worker()->SendTask([&] { send_stream->OnBitrateUpdated(update); },
                              RTC_FROM_HERE);
  }
}

TEST(AudioSendStreamTest, ProbingIntervalOnBitrateUpdated) {
  for (bool use_null_audio_processing : {false, true}) {
    ConfigHelper helper(false, true, use_null_audio_processing);
    auto send_stream = helper.CreateAudioSendStream();

    EXPECT_CALL(*helper.channel_send(),
                OnBitrateAllocation(Field(&BitrateAllocationUpdate::bwe_period,
                                          Eq(TimeDelta::Millis(5000)))));
    BitrateAllocationUpdate update;
    update.target_bitrate =
        DataRate::BitsPerSec(helper.config().max_bitrate_bps + 5000);
    update.packet_loss_ratio = 0;
    update.round_trip_time = TimeDelta::Millis(50);
    update.bwe_period = TimeDelta::Millis(5000);
    helper.worker()->SendTask([&] { send_stream->OnBitrateUpdated(update); },
                              RTC_FROM_HERE);
  }
}

// Test that AudioSendStream doesn't recreate the encoder unnecessarily.
TEST(AudioSendStreamTest, DontRecreateEncoder) {
  for (bool use_null_audio_processing : {false, true}) {
    ConfigHelper helper(false, false, use_null_audio_processing);
    // WillOnce is (currently) the default used by ConfigHelper if asked to set
    // an expectation for SetEncoder. Since this behavior is essential for this
    // test to be correct, it's instead set-up manually here. Otherwise a simple
    // change to ConfigHelper (say to WillRepeatedly) would silently make this
    // test useless.
    EXPECT_CALL(*helper.channel_send(), SetEncoder).WillOnce(Return());

    EXPECT_CALL(*helper.channel_send(), RegisterCngPayloadType(105, 8000));

    helper.config().send_codec_spec =
        AudioSendStream::Config::SendCodecSpec(9, kG722Format);
    helper.config().send_codec_spec->cng_payload_type = 105;
    auto send_stream = helper.CreateAudioSendStream();
    send_stream->Reconfigure(helper.config());
  }
}

TEST(AudioSendStreamTest, ReconfigureTransportCcResetsFirst) {
  for (bool use_null_audio_processing : {false, true}) {
    ConfigHelper helper(false, true, use_null_audio_processing);
    auto send_stream = helper.CreateAudioSendStream();
    auto new_config = helper.config();
    ConfigHelper::AddBweToConfig(&new_config);

    EXPECT_CALL(*helper.rtp_rtcp(),
                RegisterRtpHeaderExtension(TransportSequenceNumber::kUri,
                                           kTransportSequenceNumberId))
        .Times(1);
    {
      ::testing::InSequence seq;
      EXPECT_CALL(*helper.channel_send(), ResetSenderCongestionControlObjects())
          .Times(1);
      EXPECT_CALL(*helper.channel_send(),
                  RegisterSenderCongestionControlObjects(helper.transport(),
                                                         Ne(nullptr)))
          .Times(1);
    }

    send_stream->Reconfigure(new_config);
  }
}

TEST(AudioSendStreamTest, OnTransportOverheadChanged) {
  for (bool use_null_audio_processing : {false, true}) {
    ConfigHelper helper(false, true, use_null_audio_processing);
    auto send_stream = helper.CreateAudioSendStream();
    auto new_config = helper.config();

    // CallEncoder will be called on overhead change.
    EXPECT_CALL(*helper.channel_send(), CallEncoder);

    const size_t transport_overhead_per_packet_bytes = 333;
    send_stream->SetTransportOverhead(transport_overhead_per_packet_bytes);

    EXPECT_EQ(transport_overhead_per_packet_bytes,
              send_stream->TestOnlyGetPerPacketOverheadBytes());
  }
}

TEST(AudioSendStreamTest, DoesntCallEncoderWhenOverheadUnchanged) {
  for (bool use_null_audio_processing : {false, true}) {
    ConfigHelper helper(false, true, use_null_audio_processing);
    auto send_stream = helper.CreateAudioSendStream();
    auto new_config = helper.config();

    // CallEncoder will be called on overhead change.
    EXPECT_CALL(*helper.channel_send(), CallEncoder);
    const size_t transport_overhead_per_packet_bytes = 333;
    send_stream->SetTransportOverhead(transport_overhead_per_packet_bytes);

    // Set the same overhead again, CallEncoder should not be called again.
    EXPECT_CALL(*helper.channel_send(), CallEncoder).Times(0);
    send_stream->SetTransportOverhead(transport_overhead_per_packet_bytes);

    // New overhead, call CallEncoder again
    EXPECT_CALL(*helper.channel_send(), CallEncoder);
    send_stream->SetTransportOverhead(transport_overhead_per_packet_bytes + 1);
  }
}

TEST(AudioSendStreamTest, AudioOverheadChanged) {
  for (bool use_null_audio_processing : {false, true}) {
    ConfigHelper helper(false, true, use_null_audio_processing);
    const size_t audio_overhead_per_packet_bytes = 555;
    EXPECT_CALL(*helper.rtp_rtcp(), ExpectedPerPacketOverhead)
        .WillRepeatedly(Return(audio_overhead_per_packet_bytes));
    auto send_stream = helper.CreateAudioSendStream();
    auto new_config = helper.config();

    BitrateAllocationUpdate update;
    update.target_bitrate =
        DataRate::BitsPerSec(helper.config().max_bitrate_bps) +
        kMaxOverheadRate;
    EXPECT_CALL(*helper.channel_send(), OnBitrateAllocation);
    helper.worker()->SendTask([&] { send_stream->OnBitrateUpdated(update); },
                              RTC_FROM_HERE);

    EXPECT_EQ(audio_overhead_per_packet_bytes,
              send_stream->TestOnlyGetPerPacketOverheadBytes());

    EXPECT_CALL(*helper.rtp_rtcp(), ExpectedPerPacketOverhead)
        .WillRepeatedly(Return(audio_overhead_per_packet_bytes + 20));
    EXPECT_CALL(*helper.channel_send(), OnBitrateAllocation);
    helper.worker()->SendTask([&] { send_stream->OnBitrateUpdated(update); },
                              RTC_FROM_HERE);

    EXPECT_EQ(audio_overhead_per_packet_bytes + 20,
              send_stream->TestOnlyGetPerPacketOverheadBytes());
  }
}

TEST(AudioSendStreamTest, OnAudioAndTransportOverheadChanged) {
  for (bool use_null_audio_processing : {false, true}) {
    ConfigHelper helper(false, true, use_null_audio_processing);
    const size_t audio_overhead_per_packet_bytes = 555;
    EXPECT_CALL(*helper.rtp_rtcp(), ExpectedPerPacketOverhead)
        .WillRepeatedly(Return(audio_overhead_per_packet_bytes));
    auto send_stream = helper.CreateAudioSendStream();
    auto new_config = helper.config();

    const size_t transport_overhead_per_packet_bytes = 333;
    send_stream->SetTransportOverhead(transport_overhead_per_packet_bytes);

    BitrateAllocationUpdate update;
    update.target_bitrate =
        DataRate::BitsPerSec(helper.config().max_bitrate_bps) +
        kMaxOverheadRate;
    EXPECT_CALL(*helper.channel_send(), OnBitrateAllocation);
    helper.worker()->SendTask([&] { send_stream->OnBitrateUpdated(update); },
                              RTC_FROM_HERE);

    EXPECT_EQ(
        transport_overhead_per_packet_bytes + audio_overhead_per_packet_bytes,
        send_stream->TestOnlyGetPerPacketOverheadBytes());
  }
}

// Validates that reconfiguring the AudioSendStream with a Frame encryptor
// correctly reconfigures on the object without crashing.
TEST(AudioSendStreamTest, ReconfigureWithFrameEncryptor) {
  for (bool use_null_audio_processing : {false, true}) {
    ConfigHelper helper(false, true, use_null_audio_processing);
    auto send_stream = helper.CreateAudioSendStream();
    auto new_config = helper.config();

    rtc::scoped_refptr<FrameEncryptorInterface> mock_frame_encryptor_0(
        rtc::make_ref_counted<MockFrameEncryptor>());
    new_config.frame_encryptor = mock_frame_encryptor_0;
    EXPECT_CALL(*helper.channel_send(), SetFrameEncryptor(Ne(nullptr)))
        .Times(1);
    send_stream->Reconfigure(new_config);

    // Not updating the frame encryptor shouldn't force it to reconfigure.
    EXPECT_CALL(*helper.channel_send(), SetFrameEncryptor(_)).Times(0);
    send_stream->Reconfigure(new_config);

    // Updating frame encryptor to a new object should force a call to the
    // proxy.
    rtc::scoped_refptr<FrameEncryptorInterface> mock_frame_encryptor_1(
        rtc::make_ref_counted<MockFrameEncryptor>());
    new_config.frame_encryptor = mock_frame_encryptor_1;
    new_config.crypto_options.sframe.require_frame_encryption = true;
    EXPECT_CALL(*helper.channel_send(), SetFrameEncryptor(Ne(nullptr)))
        .Times(1);
    send_stream->Reconfigure(new_config);
  }
}
}  // namespace test
}  // namespace webrtc
