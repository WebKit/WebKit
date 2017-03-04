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
#include <vector>

#include "webrtc/audio/audio_send_stream.h"
#include "webrtc/audio/audio_state.h"
#include "webrtc/audio/conversion.h"
#include "webrtc/base/task_queue.h"
#include "webrtc/logging/rtc_event_log/mock/mock_rtc_event_log.h"
#include "webrtc/modules/audio_mixer/audio_mixer_impl.h"
#include "webrtc/modules/audio_processing/include/mock_audio_processing.h"
#include "webrtc/modules/congestion_controller/include/congestion_controller.h"
#include "webrtc/modules/congestion_controller/include/mock/mock_congestion_controller.h"
#include "webrtc/modules/pacing/paced_sender.h"
#include "webrtc/modules/remote_bitrate_estimator/include/mock/mock_remote_bitrate_estimator.h"
#include "webrtc/modules/rtp_rtcp/mocks/mock_rtcp_rtt_stats.h"
#include "webrtc/test/gtest.h"
#include "webrtc/test/mock_voe_channel_proxy.h"
#include "webrtc/test/mock_voice_engine.h"
#include "webrtc/voice_engine/transmit_mixer.h"

namespace webrtc {
namespace test {
namespace {

using testing::_;
using testing::Eq;
using testing::Ne;
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

struct ConfigHelper {
  explicit ConfigHelper(bool audio_bwe_enabled)
      : simulated_clock_(123456),
        stream_config_(nullptr),
        congestion_controller_(&simulated_clock_,
                               &bitrate_observer_,
                               &remote_bitrate_observer_,
                               &event_log_,
                               &packet_router_),
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

    SetupMockForSetupSendCodec();

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
      stream_config_.send_codec_spec.transport_cc_enabled = true;
    }
    // Use ISAC as default codec so as to prevent unnecessary |voice_engine_|
    // calls from the default ctor behavior.
    stream_config_.send_codec_spec.codec_inst = kIsacCodec;
    stream_config_.min_bitrate_bps = 10000;
    stream_config_.max_bitrate_bps = 65000;
  }

  AudioSendStream::Config& config() { return stream_config_; }
  rtc::scoped_refptr<AudioState> audio_state() { return audio_state_; }
  MockVoEChannelProxy* channel_proxy() { return channel_proxy_; }
  PacketRouter* packet_router() { return &packet_router_; }
  CongestionController* congestion_controller() {
    return &congestion_controller_;
  }
  BitrateAllocator* bitrate_allocator() { return &bitrate_allocator_; }
  rtc::TaskQueue* worker_queue() { return &worker_queue_; }
  RtcEventLog* event_log() { return &event_log_; }
  MockVoiceEngine* voice_engine() { return &voice_engine_; }

  void SetupDefaultChannelProxy(bool audio_bwe_enabled) {
    using testing::StrEq;
    channel_proxy_ = new testing::StrictMock<MockVoEChannelProxy>();
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
      EXPECT_CALL(*channel_proxy_,
                  RegisterSenderCongestionControlObjects(
                      congestion_controller_.pacer(),
                      congestion_controller_.GetTransportFeedbackObserver(),
                      packet_router(), Ne(nullptr)))
          .Times(1);
    } else {
      EXPECT_CALL(*channel_proxy_,
                  RegisterSenderCongestionControlObjects(
                      congestion_controller_.pacer(),
                      congestion_controller_.GetTransportFeedbackObserver(),
                      packet_router(), Eq(nullptr)))
          .Times(1);
    }
    EXPECT_CALL(*channel_proxy_, ResetCongestionControlObjects()).Times(1);
    EXPECT_CALL(*channel_proxy_, RegisterExternalTransport(nullptr)).Times(1);
    EXPECT_CALL(*channel_proxy_, DeRegisterExternalTransport()).Times(1);
    EXPECT_CALL(*channel_proxy_, SetRtcEventLog(testing::NotNull())).Times(1);
    EXPECT_CALL(*channel_proxy_, SetRtcEventLog(testing::IsNull()))
        .Times(1);  // Destructor resets the event log
    EXPECT_CALL(*channel_proxy_, SetRtcpRttStats(&rtcp_rtt_stats_)).Times(1);
    EXPECT_CALL(*channel_proxy_, SetRtcpRttStats(testing::IsNull()))
        .Times(1);  // Destructor resets the rtt stats.
  }

  void SetupMockForSetupSendCodec() {
    EXPECT_CALL(*channel_proxy_, SetVADStatus(false))
        .WillOnce(Return(true));
    EXPECT_CALL(*channel_proxy_, SetCodecFECStatus(false))
        .WillOnce(Return(true));
    EXPECT_CALL(*channel_proxy_, DisableAudioNetworkAdaptor());
    // Let |GetSendCodec| return false for the first time to indicate that no
    // send codec has been set.
    EXPECT_CALL(*channel_proxy_, GetSendCodec(_)).WillOnce(Return(false));
    EXPECT_CALL(*channel_proxy_, SetSendCodec(_)).WillOnce(Return(true));
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
    EXPECT_CALL(*channel_proxy_, GetSendCodec(_))
        .WillRepeatedly(DoAll(SetArgPointee<0>(kIsacCodec), Return(true)));
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
  SimulatedClock simulated_clock_;
  testing::StrictMock<MockVoiceEngine> voice_engine_;
  rtc::scoped_refptr<AudioState> audio_state_;
  AudioSendStream::Config stream_config_;
  testing::StrictMock<MockVoEChannelProxy>* channel_proxy_ = nullptr;
  testing::NiceMock<MockCongestionObserver> bitrate_observer_;
  testing::NiceMock<MockRemoteBitrateObserver> remote_bitrate_observer_;
  MockAudioProcessing audio_processing_;
  MockTransmitMixer transmit_mixer_;
  AudioProcessing::AudioProcessingStatistics audio_processing_stats_;
  PacketRouter packet_router_;
  CongestionController congestion_controller_;
  MockRtcEventLog event_log_;
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
  config.send_codec_spec.nack_enabled = true;
  config.send_codec_spec.transport_cc_enabled = false;
  config.send_codec_spec.enable_codec_fec = true;
  config.send_codec_spec.enable_opus_dtx = false;
  config.send_codec_spec.opus_max_playback_rate = 32000;
  config.send_codec_spec.cng_payload_type = 42;
  config.send_codec_spec.cng_plfreq = 56;
  config.send_codec_spec.min_ptime_ms = 20;
  config.send_codec_spec.max_ptime_ms = 60;
  config.send_codec_spec.codec_inst = kIsacCodec;
  config.rtp.extensions.push_back(
      RtpExtension(RtpExtension::kAudioLevelUri, kAudioLevelId));
  EXPECT_EQ(
      "{rtp: {ssrc: 1234, extensions: [{uri: "
      "urn:ietf:params:rtp-hdrext:ssrc-audio-level, id: 2}], nack: "
      "{rtp_history_ms: 0}, c_name: foo_name}, send_transport: null, "
      "voe_channel_id: 1, min_bitrate_bps: 12000, max_bitrate_bps: 34000, "
      "send_codec_spec: {nack_enabled: true, transport_cc_enabled: false, "
      "enable_codec_fec: true, enable_opus_dtx: false, opus_max_playback_rate: "
      "32000, cng_payload_type: 42, cng_plfreq: 56, min_ptime: 20, max_ptime: "
      "60, codec_inst: {pltype: 103, plname: \"isac\", plfreq: 16000, pacsize: "
      "320, channels: 1, rate: 32000}}}",
      config.ToString());
}

TEST(AudioSendStreamTest, ConstructDestruct) {
  ConfigHelper helper(false);
  internal::AudioSendStream send_stream(
      helper.config(), helper.audio_state(), helper.worker_queue(),
      helper.packet_router(), helper.congestion_controller(),
      helper.bitrate_allocator(), helper.event_log(), helper.rtcp_rtt_stats());
}

TEST(AudioSendStreamTest, SendTelephoneEvent) {
  ConfigHelper helper(false);
  internal::AudioSendStream send_stream(
      helper.config(), helper.audio_state(), helper.worker_queue(),
      helper.packet_router(), helper.congestion_controller(),
      helper.bitrate_allocator(), helper.event_log(), helper.rtcp_rtt_stats());
  helper.SetupMockForSendTelephoneEvent();
  EXPECT_TRUE(send_stream.SendTelephoneEvent(kTelephoneEventPayloadType,
      kTelephoneEventPayloadFrequency, kTelephoneEventCode,
      kTelephoneEventDuration));
}

TEST(AudioSendStreamTest, SetMuted) {
  ConfigHelper helper(false);
  internal::AudioSendStream send_stream(
      helper.config(), helper.audio_state(), helper.worker_queue(),
      helper.packet_router(), helper.congestion_controller(),
      helper.bitrate_allocator(), helper.event_log(), helper.rtcp_rtt_stats());
  EXPECT_CALL(*helper.channel_proxy(), SetInputMute(true));
  send_stream.SetMuted(true);
}

TEST(AudioSendStreamTest, AudioBweCorrectObjectsOnChannelProxy) {
  ConfigHelper helper(true);
  internal::AudioSendStream send_stream(
      helper.config(), helper.audio_state(), helper.worker_queue(),
      helper.packet_router(), helper.congestion_controller(),
      helper.bitrate_allocator(), helper.event_log(), helper.rtcp_rtt_stats());
}

TEST(AudioSendStreamTest, NoAudioBweCorrectObjectsOnChannelProxy) {
  ConfigHelper helper(false);
  internal::AudioSendStream send_stream(
      helper.config(), helper.audio_state(), helper.worker_queue(),
      helper.packet_router(), helper.congestion_controller(),
      helper.bitrate_allocator(), helper.event_log(), helper.rtcp_rtt_stats());
}

TEST(AudioSendStreamTest, GetStats) {
  ConfigHelper helper(false);
  internal::AudioSendStream send_stream(
      helper.config(), helper.audio_state(), helper.worker_queue(),
      helper.packet_router(), helper.congestion_controller(),
      helper.bitrate_allocator(), helper.event_log(), helper.rtcp_rtt_stats());
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
  ConfigHelper helper(false);
  internal::AudioSendStream send_stream(
      helper.config(), helper.audio_state(), helper.worker_queue(),
      helper.packet_router(), helper.congestion_controller(),
      helper.bitrate_allocator(), helper.event_log(), helper.rtcp_rtt_stats());
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

TEST(AudioSendStreamTest, SendCodecAppliesConfigParams) {
  ConfigHelper helper(false);
  auto stream_config = helper.config();
  const CodecInst kOpusCodec = {111, "opus", 48000, 960, 2, 64000};
  stream_config.send_codec_spec.codec_inst = kOpusCodec;
  stream_config.send_codec_spec.enable_codec_fec = true;
  stream_config.send_codec_spec.enable_opus_dtx = true;
  stream_config.send_codec_spec.opus_max_playback_rate = 12345;
  stream_config.send_codec_spec.cng_plfreq = 16000;
  stream_config.send_codec_spec.cng_payload_type = 105;
  stream_config.send_codec_spec.min_ptime_ms = 10;
  stream_config.send_codec_spec.max_ptime_ms = 60;
  stream_config.audio_network_adaptor_config =
      rtc::Optional<std::string>("abced");
  EXPECT_CALL(*helper.channel_proxy(), SetCodecFECStatus(true))
      .WillOnce(Return(true));
  EXPECT_CALL(
      *helper.channel_proxy(),
      SetOpusDtx(stream_config.send_codec_spec.enable_opus_dtx))
      .WillOnce(Return(true));
  EXPECT_CALL(
      *helper.channel_proxy(),
      SetOpusMaxPlaybackRate(
          stream_config.send_codec_spec.opus_max_playback_rate))
      .WillOnce(Return(true));
  EXPECT_CALL(*helper.channel_proxy(),
              SetSendCNPayloadType(
                  stream_config.send_codec_spec.cng_payload_type,
                  webrtc::kFreq16000Hz))
      .WillOnce(Return(true));
  EXPECT_CALL(
      *helper.channel_proxy(),
      SetReceiverFrameLengthRange(stream_config.send_codec_spec.min_ptime_ms,
                                  stream_config.send_codec_spec.max_ptime_ms));
  EXPECT_CALL(
      *helper.channel_proxy(),
      EnableAudioNetworkAdaptor(*stream_config.audio_network_adaptor_config));
  internal::AudioSendStream send_stream(
      stream_config, helper.audio_state(), helper.worker_queue(),
      helper.packet_router(), helper.congestion_controller(),
      helper.bitrate_allocator(), helper.event_log(), helper.rtcp_rtt_stats());
}

// VAD is applied when codec is mono and the CNG frequency matches the codec
// sample rate.
TEST(AudioSendStreamTest, SendCodecCanApplyVad) {
  ConfigHelper helper(false);
  auto stream_config = helper.config();
  const CodecInst kG722Codec = {9, "g722", 8000, 160, 1, 16000};
  stream_config.send_codec_spec.codec_inst = kG722Codec;
  stream_config.send_codec_spec.cng_plfreq = 8000;
  stream_config.send_codec_spec.cng_payload_type = 105;
  EXPECT_CALL(*helper.channel_proxy(), SetVADStatus(true))
      .WillOnce(Return(true));
  internal::AudioSendStream send_stream(
      stream_config, helper.audio_state(), helper.worker_queue(),
      helper.packet_router(), helper.congestion_controller(),
      helper.bitrate_allocator(), helper.event_log(), helper.rtcp_rtt_stats());
}

TEST(AudioSendStreamTest, DoesNotPassHigherBitrateThanMaxBitrate) {
  ConfigHelper helper(false);
  internal::AudioSendStream send_stream(
      helper.config(), helper.audio_state(), helper.worker_queue(),
      helper.packet_router(), helper.congestion_controller(),
      helper.bitrate_allocator(), helper.event_log(), helper.rtcp_rtt_stats());
  EXPECT_CALL(*helper.channel_proxy(),
              SetBitrate(helper.config().max_bitrate_bps, _));
  send_stream.OnBitrateUpdated(helper.config().max_bitrate_bps + 5000, 0.0, 50,
                               6000);
}

TEST(AudioSendStreamTest, ProbingIntervalOnBitrateUpdated) {
  ConfigHelper helper(false);
  internal::AudioSendStream send_stream(
      helper.config(), helper.audio_state(), helper.worker_queue(),
      helper.packet_router(), helper.congestion_controller(),
      helper.bitrate_allocator(), helper.event_log(), helper.rtcp_rtt_stats());
  EXPECT_CALL(*helper.channel_proxy(), SetBitrate(_, 5000));
  send_stream.OnBitrateUpdated(50000, 0.0, 50, 5000);
}

}  // namespace test
}  // namespace webrtc
