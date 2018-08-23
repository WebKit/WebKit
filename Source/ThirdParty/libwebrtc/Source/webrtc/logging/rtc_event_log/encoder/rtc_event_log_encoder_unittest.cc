/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <deque>
#include <limits>
#include <string>

#include "absl/memory/memory.h"
#include "logging/rtc_event_log/encoder/rtc_event_log_encoder_legacy.h"
#include "logging/rtc_event_log/events/rtc_event_alr_state.h"
#include "logging/rtc_event_log/events/rtc_event_audio_network_adaptation.h"
#include "logging/rtc_event_log/events/rtc_event_audio_playout.h"
#include "logging/rtc_event_log/events/rtc_event_audio_receive_stream_config.h"
#include "logging/rtc_event_log/events/rtc_event_audio_send_stream_config.h"
#include "logging/rtc_event_log/events/rtc_event_bwe_update_delay_based.h"
#include "logging/rtc_event_log/events/rtc_event_bwe_update_loss_based.h"
#include "logging/rtc_event_log/events/rtc_event_probe_cluster_created.h"
#include "logging/rtc_event_log/events/rtc_event_probe_result_failure.h"
#include "logging/rtc_event_log/events/rtc_event_probe_result_success.h"
#include "logging/rtc_event_log/events/rtc_event_rtcp_packet_incoming.h"
#include "logging/rtc_event_log/events/rtc_event_rtcp_packet_outgoing.h"
#include "logging/rtc_event_log/events/rtc_event_rtp_packet_incoming.h"
#include "logging/rtc_event_log/events/rtc_event_rtp_packet_outgoing.h"
#include "logging/rtc_event_log/events/rtc_event_video_receive_stream_config.h"
#include "logging/rtc_event_log/events/rtc_event_video_send_stream_config.h"
#include "logging/rtc_event_log/rtc_event_log_parser_new.h"
#include "logging/rtc_event_log/rtc_event_log_unittest_helper.h"
#include "modules/audio_coding/audio_network_adaptor/include/audio_network_adaptor_config.h"
#include "modules/remote_bitrate_estimator/include/bwe_defines.h"
#include "modules/rtp_rtcp/source/rtp_header_extensions.h"
#include "rtc_base/random.h"
#include "test/gtest.h"

namespace webrtc {


class RtcEventLogEncoderTest : public testing::TestWithParam<int> {
 protected:
  RtcEventLogEncoderTest()
      : encoder_(new RtcEventLogEncoderLegacy),
        seed_(GetParam()),
        prng_(seed_),
        gen_(seed_ * 880001UL) {}
  ~RtcEventLogEncoderTest() override = default;

  // ANA events have some optional fields, so we want to make sure that we get
  // correct behavior both when all of the values are there, as well as when
  // only some.
  void TestRtcEventAudioNetworkAdaptation(
      std::unique_ptr<AudioEncoderRuntimeConfig> runtime_config);

  std::deque<std::unique_ptr<RtcEvent>> history_;
  // TODO(eladalon): Once we have more than one possible encoder, parameterize
  // encoder selection.
  std::unique_ptr<RtcEventLogEncoder> encoder_;
  ParsedRtcEventLogNew parsed_log_;
  const uint64_t seed_;
  Random prng_;
  test::EventGenerator gen_;
};

TEST_P(RtcEventLogEncoderTest, RtcEventAlrState) {
  std::unique_ptr<RtcEventAlrState> event = gen_.NewAlrState();
  history_.push_back(event->Copy());

  std::string encoded = encoder_->EncodeBatch(history_.begin(), history_.end());
  ASSERT_TRUE(parsed_log_.ParseString(encoded));
  const auto& alr_state_events = parsed_log_.alr_state_events();

  ASSERT_EQ(alr_state_events.size(), 1u);
  EXPECT_TRUE(test::VerifyLoggedAlrStateEvent(*event, alr_state_events[0]));
}

void RtcEventLogEncoderTest::TestRtcEventAudioNetworkAdaptation(
    std::unique_ptr<AudioEncoderRuntimeConfig> runtime_config) {
  // This function is called repeatedly. Clear state between calls.
  history_.clear();
  auto original_runtime_config = *runtime_config;
  auto event = absl::make_unique<RtcEventAudioNetworkAdaptation>(
      std::move(runtime_config));
  const int64_t timestamp_us = event->timestamp_us_;
  history_.push_back(std::move(event));

  std::string encoded = encoder_->EncodeBatch(history_.begin(), history_.end());
  ASSERT_TRUE(parsed_log_.ParseString(encoded));
  const auto& ana_configs = parsed_log_.audio_network_adaptation_events();
  ASSERT_EQ(ana_configs.size(), 1u);

  EXPECT_EQ(ana_configs[0].timestamp_us, timestamp_us);
  EXPECT_EQ(ana_configs[0].config, original_runtime_config);
}

TEST_P(RtcEventLogEncoderTest, RtcEventAudioNetworkAdaptationBitrate) {
  auto runtime_config = absl::make_unique<AudioEncoderRuntimeConfig>();
  const int bitrate_bps = rtc::checked_cast<int>(
      prng_.Rand(0, std::numeric_limits<int32_t>::max()));
  runtime_config->bitrate_bps = bitrate_bps;
  TestRtcEventAudioNetworkAdaptation(std::move(runtime_config));
}

TEST_P(RtcEventLogEncoderTest, RtcEventAudioNetworkAdaptationFrameLength) {
  auto runtime_config = absl::make_unique<AudioEncoderRuntimeConfig>();
  const int frame_length_ms = prng_.Rand(1, 1000);
  runtime_config->frame_length_ms = frame_length_ms;
  TestRtcEventAudioNetworkAdaptation(std::move(runtime_config));
}

TEST_P(RtcEventLogEncoderTest, RtcEventAudioNetworkAdaptationPacketLoss) {
  // To simplify the test, we just check powers of two.
  const float plr = std::pow(0.5f, prng_.Rand(1, 8));
  auto runtime_config = absl::make_unique<AudioEncoderRuntimeConfig>();
  runtime_config->uplink_packet_loss_fraction = plr;
  TestRtcEventAudioNetworkAdaptation(std::move(runtime_config));
}

TEST_P(RtcEventLogEncoderTest, RtcEventAudioNetworkAdaptationFec) {
  // The test might be trivially passing for one of the two boolean values, so
  // for safety's sake, we test both.
  for (bool fec_enabled : {false, true}) {
    auto runtime_config = absl::make_unique<AudioEncoderRuntimeConfig>();
    runtime_config->enable_fec = fec_enabled;
    TestRtcEventAudioNetworkAdaptation(std::move(runtime_config));
  }
}

TEST_P(RtcEventLogEncoderTest, RtcEventAudioNetworkAdaptationDtx) {
  // The test might be trivially passing for one of the two boolean values, so
  // for safety's sake, we test both.
  for (bool dtx_enabled : {false, true}) {
    auto runtime_config = absl::make_unique<AudioEncoderRuntimeConfig>();
    runtime_config->enable_dtx = dtx_enabled;
    TestRtcEventAudioNetworkAdaptation(std::move(runtime_config));
  }
}

TEST_P(RtcEventLogEncoderTest, RtcEventAudioNetworkAdaptationChannels) {
  // The test might be trivially passing for one of the two possible values, so
  // for safety's sake, we test both.
  for (size_t channels : {1, 2}) {
    auto runtime_config = absl::make_unique<AudioEncoderRuntimeConfig>();
    runtime_config->num_channels = channels;
    TestRtcEventAudioNetworkAdaptation(std::move(runtime_config));
  }
}

TEST_P(RtcEventLogEncoderTest, RtcEventAudioNetworkAdaptationAll) {
  const int bitrate_bps = rtc::checked_cast<int>(
      prng_.Rand(0, std::numeric_limits<int32_t>::max()));
  const int frame_length_ms = prng_.Rand(1, 1000);
  const float plr = std::pow(0.5f, prng_.Rand(1, 8));
  for (bool fec_enabled : {false, true}) {
    for (bool dtx_enabled : {false, true}) {
      for (size_t channels : {1, 2}) {
        auto runtime_config = absl::make_unique<AudioEncoderRuntimeConfig>();
        runtime_config->bitrate_bps = bitrate_bps;
        runtime_config->frame_length_ms = frame_length_ms;
        runtime_config->uplink_packet_loss_fraction = plr;
        runtime_config->enable_fec = fec_enabled;
        runtime_config->enable_dtx = dtx_enabled;
        runtime_config->num_channels = channels;

        TestRtcEventAudioNetworkAdaptation(std::move(runtime_config));
      }
    }
  }
}

TEST_P(RtcEventLogEncoderTest, RtcEventAudioPlayout) {
  uint32_t ssrc = prng_.Rand<uint32_t>();
  std::unique_ptr<RtcEventAudioPlayout> event = gen_.NewAudioPlayout(ssrc);
  history_.push_back(event->Copy());

  std::string encoded = encoder_->EncodeBatch(history_.begin(), history_.end());
  ASSERT_TRUE(parsed_log_.ParseString(encoded));

  const auto& playout_events = parsed_log_.audio_playout_events();
  ASSERT_EQ(playout_events.size(), 1u);
  const auto playout_stream = playout_events.find(ssrc);
  ASSERT_TRUE(playout_stream != playout_events.end());
  ASSERT_EQ(playout_stream->second.size(), 1u);
  LoggedAudioPlayoutEvent playout_event = playout_stream->second[0];
  EXPECT_TRUE(test::VerifyLoggedAudioPlayoutEvent(*event, playout_event));
}

TEST_P(RtcEventLogEncoderTest, RtcEventAudioReceiveStreamConfig) {
  uint32_t ssrc = prng_.Rand<uint32_t>();
  RtpHeaderExtensionMap extensions = gen_.NewRtpHeaderExtensionMap();
  std::unique_ptr<RtcEventAudioReceiveStreamConfig> event =
      gen_.NewAudioReceiveStreamConfig(ssrc, extensions);
  history_.push_back(event->Copy());

  std::string encoded = encoder_->EncodeBatch(history_.begin(), history_.end());
  ASSERT_TRUE(parsed_log_.ParseString(encoded));
  const auto& audio_recv_configs = parsed_log_.audio_recv_configs();

  ASSERT_EQ(audio_recv_configs.size(), 1u);
  EXPECT_TRUE(test::VerifyLoggedAudioRecvConfig(*event, audio_recv_configs[0]));
}

TEST_P(RtcEventLogEncoderTest, RtcEventAudioSendStreamConfig) {
  uint32_t ssrc = prng_.Rand<uint32_t>();
  RtpHeaderExtensionMap extensions = gen_.NewRtpHeaderExtensionMap();
  std::unique_ptr<RtcEventAudioSendStreamConfig> event =
      gen_.NewAudioSendStreamConfig(ssrc, extensions);
  history_.push_back(event->Copy());

  std::string encoded = encoder_->EncodeBatch(history_.begin(), history_.end());
  ASSERT_TRUE(parsed_log_.ParseString(encoded));
  const auto& audio_send_configs = parsed_log_.audio_send_configs();

  ASSERT_EQ(audio_send_configs.size(), 1u);
  EXPECT_TRUE(test::VerifyLoggedAudioSendConfig(*event, audio_send_configs[0]));
}

TEST_P(RtcEventLogEncoderTest, RtcEventBweUpdateDelayBased) {
  std::unique_ptr<RtcEventBweUpdateDelayBased> event =
      gen_.NewBweUpdateDelayBased();
  history_.push_back(event->Copy());

  std::string encoded = encoder_->EncodeBatch(history_.begin(), history_.end());
  ASSERT_TRUE(parsed_log_.ParseString(encoded));
  const auto& bwe_delay_updates = parsed_log_.bwe_delay_updates();

  ASSERT_EQ(bwe_delay_updates.size(), 1u);
  EXPECT_TRUE(
      test::VerifyLoggedBweDelayBasedUpdate(*event, bwe_delay_updates[0]));
}

TEST_P(RtcEventLogEncoderTest, RtcEventBweUpdateLossBased) {
  std::unique_ptr<RtcEventBweUpdateLossBased> event =
      gen_.NewBweUpdateLossBased();
  history_.push_back(event->Copy());

  std::string encoded = encoder_->EncodeBatch(history_.begin(), history_.end());
  ASSERT_TRUE(parsed_log_.ParseString(encoded));
  const auto& bwe_loss_updates = parsed_log_.bwe_loss_updates();

  ASSERT_EQ(bwe_loss_updates.size(), 1u);
  EXPECT_TRUE(
      test::VerifyLoggedBweLossBasedUpdate(*event, bwe_loss_updates[0]));
}

TEST_P(RtcEventLogEncoderTest, RtcEventIceCandidatePairConfig) {
  std::unique_ptr<RtcEventIceCandidatePairConfig> event =
      gen_.NewIceCandidatePairConfig();
  history_.push_back(event->Copy());

  std::string encoded = encoder_->EncodeBatch(history_.begin(), history_.end());
  ASSERT_TRUE(parsed_log_.ParseString(encoded));
  const auto& ice_candidate_pair_configs =
      parsed_log_.ice_candidate_pair_configs();

  ASSERT_EQ(ice_candidate_pair_configs.size(), 1u);
  EXPECT_TRUE(test::VerifyLoggedIceCandidatePairConfig(
      *event, ice_candidate_pair_configs[0]));
}

TEST_P(RtcEventLogEncoderTest, RtcEventIceCandidatePair) {
  std::unique_ptr<RtcEventIceCandidatePair> event = gen_.NewIceCandidatePair();
  history_.push_back(event->Copy());

  std::string encoded = encoder_->EncodeBatch(history_.begin(), history_.end());
  ASSERT_TRUE(parsed_log_.ParseString(encoded));
  const auto& ice_candidate_pair_events =
      parsed_log_.ice_candidate_pair_events();

  ASSERT_EQ(ice_candidate_pair_events.size(), 1u);
  EXPECT_TRUE(test::VerifyLoggedIceCandidatePairEvent(
      *event, ice_candidate_pair_events[0]));
}

TEST_P(RtcEventLogEncoderTest, RtcEventLoggingStarted) {
  const int64_t timestamp_us = rtc::TimeMicros();

  ASSERT_TRUE(parsed_log_.ParseString(encoder_->EncodeLogStart(timestamp_us)));
  const auto& start_log_events = parsed_log_.start_log_events();

  ASSERT_EQ(start_log_events.size(), 1u);
  EXPECT_EQ(start_log_events[0].timestamp_us, timestamp_us);
}

TEST_P(RtcEventLogEncoderTest, RtcEventLoggingStopped) {
  const int64_t timestamp_us = rtc::TimeMicros();

  ASSERT_TRUE(parsed_log_.ParseString(encoder_->EncodeLogEnd(timestamp_us)));
  const auto& stop_log_events = parsed_log_.stop_log_events();

  ASSERT_EQ(stop_log_events.size(), 1u);
  EXPECT_EQ(stop_log_events[0].timestamp_us, timestamp_us);
}

TEST_P(RtcEventLogEncoderTest, RtcEventProbeClusterCreated) {
  std::unique_ptr<RtcEventProbeClusterCreated> event =
      gen_.NewProbeClusterCreated();
  history_.push_back(event->Copy());

  std::string encoded = encoder_->EncodeBatch(history_.begin(), history_.end());
  ASSERT_TRUE(parsed_log_.ParseString(encoded));
  const auto& bwe_probe_cluster_created_events =
      parsed_log_.bwe_probe_cluster_created_events();

  ASSERT_EQ(bwe_probe_cluster_created_events.size(), 1u);
  EXPECT_TRUE(test::VerifyLoggedBweProbeClusterCreatedEvent(
      *event, bwe_probe_cluster_created_events[0]));
}

TEST_P(RtcEventLogEncoderTest, RtcEventProbeResultFailure) {
  std::unique_ptr<RtcEventProbeResultFailure> event =
      gen_.NewProbeResultFailure();
  history_.push_back(event->Copy());

  std::string encoded = encoder_->EncodeBatch(history_.begin(), history_.end());
  ASSERT_TRUE(parsed_log_.ParseString(encoded));
  const auto& bwe_probe_failure_events = parsed_log_.bwe_probe_failure_events();

  ASSERT_EQ(bwe_probe_failure_events.size(), 1u);
  EXPECT_TRUE(test::VerifyLoggedBweProbeFailureEvent(
      *event, bwe_probe_failure_events[0]));
}

TEST_P(RtcEventLogEncoderTest, RtcEventProbeResultSuccess) {
  std::unique_ptr<RtcEventProbeResultSuccess> event =
      gen_.NewProbeResultSuccess();
  history_.push_back(event->Copy());

  std::string encoded = encoder_->EncodeBatch(history_.begin(), history_.end());
  ASSERT_TRUE(parsed_log_.ParseString(encoded));
  const auto& bwe_probe_success_events = parsed_log_.bwe_probe_success_events();

  ASSERT_EQ(bwe_probe_success_events.size(), 1u);
  EXPECT_TRUE(test::VerifyLoggedBweProbeSuccessEvent(
      *event, bwe_probe_success_events[0]));
}

TEST_P(RtcEventLogEncoderTest, RtcEventRtcpPacketIncoming) {
  std::unique_ptr<RtcEventRtcpPacketIncoming> event =
      gen_.NewRtcpPacketIncoming();
  history_.push_back(event->Copy());

  std::string encoded = encoder_->EncodeBatch(history_.begin(), history_.end());
  ASSERT_TRUE(parsed_log_.ParseString(encoded));
  const auto& incoming_rtcp_packets = parsed_log_.incoming_rtcp_packets();

  ASSERT_EQ(incoming_rtcp_packets.size(), 1u);
  EXPECT_TRUE(
      test::VerifyLoggedRtcpPacketIncoming(*event, incoming_rtcp_packets[0]));
}

TEST_P(RtcEventLogEncoderTest, RtcEventRtcpPacketOutgoing) {
  std::unique_ptr<RtcEventRtcpPacketOutgoing> event =
      gen_.NewRtcpPacketOutgoing();
  history_.push_back(event->Copy());

  std::string encoded = encoder_->EncodeBatch(history_.begin(), history_.end());
  ASSERT_TRUE(parsed_log_.ParseString(encoded));
  const auto& outgoing_rtcp_packets = parsed_log_.outgoing_rtcp_packets();

  ASSERT_EQ(outgoing_rtcp_packets.size(), 1u);
  EXPECT_TRUE(
      test::VerifyLoggedRtcpPacketOutgoing(*event, outgoing_rtcp_packets[0]));
}

TEST_P(RtcEventLogEncoderTest, RtcEventRtpPacketIncoming) {
  uint32_t ssrc = prng_.Rand<uint32_t>();
  RtpHeaderExtensionMap extension_map;  // TODO(terelius): Test extensions too.
  std::unique_ptr<RtcEventRtpPacketIncoming> event =
      gen_.NewRtpPacketIncoming(ssrc, extension_map);
  history_.push_back(event->Copy());

  std::string encoded = encoder_->EncodeBatch(history_.begin(), history_.end());
  ASSERT_TRUE(parsed_log_.ParseString(encoded));
  const auto& incoming_rtp_packets_by_ssrc =
      parsed_log_.incoming_rtp_packets_by_ssrc();

  ASSERT_EQ(incoming_rtp_packets_by_ssrc.size(), 1u);
  const auto& stream = incoming_rtp_packets_by_ssrc[0];
  EXPECT_EQ(stream.ssrc, ssrc);
  ASSERT_EQ(stream.incoming_packets.size(), 1u);
  EXPECT_TRUE(
      test::VerifyLoggedRtpPacketIncoming(*event, stream.incoming_packets[0]));
}

TEST_P(RtcEventLogEncoderTest, RtcEventRtpPacketOutgoing) {
  uint32_t ssrc = prng_.Rand<uint32_t>();
  RtpHeaderExtensionMap extension_map;  // TODO(terelius): Test extensions too.
  std::unique_ptr<RtcEventRtpPacketOutgoing> event =
      gen_.NewRtpPacketOutgoing(ssrc, extension_map);
  history_.push_back(event->Copy());

  std::string encoded = encoder_->EncodeBatch(history_.begin(), history_.end());
  ASSERT_TRUE(parsed_log_.ParseString(encoded));
  const auto& outgoing_rtp_packets_by_ssrc =
      parsed_log_.outgoing_rtp_packets_by_ssrc();

  ASSERT_EQ(outgoing_rtp_packets_by_ssrc.size(), 1u);
  const auto& stream = outgoing_rtp_packets_by_ssrc[0];
  EXPECT_EQ(stream.ssrc, ssrc);
  ASSERT_EQ(stream.outgoing_packets.size(), 1u);
  EXPECT_TRUE(
      test::VerifyLoggedRtpPacketOutgoing(*event, stream.outgoing_packets[0]));
}

TEST_P(RtcEventLogEncoderTest, RtcEventVideoReceiveStreamConfig) {
  uint32_t ssrc = prng_.Rand<uint32_t>();
  RtpHeaderExtensionMap extensions = gen_.NewRtpHeaderExtensionMap();
  std::unique_ptr<RtcEventVideoReceiveStreamConfig> event =
      gen_.NewVideoReceiveStreamConfig(ssrc, extensions);
  history_.push_back(event->Copy());

  std::string encoded = encoder_->EncodeBatch(history_.begin(), history_.end());
  ASSERT_TRUE(parsed_log_.ParseString(encoded));
  const auto& video_recv_configs = parsed_log_.video_recv_configs();

  ASSERT_EQ(video_recv_configs.size(), 1u);
  EXPECT_TRUE(test::VerifyLoggedVideoRecvConfig(*event, video_recv_configs[0]));
}

TEST_P(RtcEventLogEncoderTest, RtcEventVideoSendStreamConfig) {
  uint32_t ssrc = prng_.Rand<uint32_t>();
  RtpHeaderExtensionMap extensions = gen_.NewRtpHeaderExtensionMap();
  std::unique_ptr<RtcEventVideoSendStreamConfig> event =
      gen_.NewVideoSendStreamConfig(ssrc, extensions);
  history_.push_back(event->Copy());

  std::string encoded = encoder_->EncodeBatch(history_.begin(), history_.end());
  ASSERT_TRUE(parsed_log_.ParseString(encoded));
  const auto& video_send_configs = parsed_log_.video_send_configs();

  ASSERT_EQ(video_send_configs.size(), 1u);
  EXPECT_TRUE(test::VerifyLoggedVideoSendConfig(*event, video_send_configs[0]));
}

INSTANTIATE_TEST_CASE_P(RandomSeeds,
                        RtcEventLogEncoderTest,
                        ::testing::Values(1, 2, 3, 4, 5));

}  // namespace webrtc
