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
#include <tuple>

#include "absl/memory/memory.h"
#include "logging/rtc_event_log/encoder/rtc_event_log_encoder_legacy.h"
#include "logging/rtc_event_log/encoder/rtc_event_log_encoder_new_format.h"
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
class RtcEventLogEncoderTest
    : public testing::TestWithParam<std::tuple<int, bool, size_t, bool>> {
 protected:
  RtcEventLogEncoderTest()
      : seed_(std::get<0>(GetParam())),
        prng_(seed_),
        new_encoding_(std::get<1>(GetParam())),
        event_count_(std::get<2>(GetParam())),
        force_repeated_fields_(std::get<3>(GetParam())),
        gen_(seed_ * 880001UL),
        verifier_(new_encoding_ ? RtcEventLog::EncodingType::NewFormat
                                : RtcEventLog::EncodingType::Legacy) {
    if (new_encoding_)
      encoder_ = absl::make_unique<RtcEventLogEncoderNewFormat>();
    else
      encoder_ = absl::make_unique<RtcEventLogEncoderLegacy>();
  }
  ~RtcEventLogEncoderTest() override = default;

  // ANA events have some optional fields, so we want to make sure that we get
  // correct behavior both when all of the values are there, as well as when
  // only some.
  void TestRtcEventAudioNetworkAdaptation(
      const std::vector<std::unique_ptr<RtcEventAudioNetworkAdaptation>>&);

  template <typename EventType>
  std::unique_ptr<EventType> NewRtpPacket(
      uint32_t ssrc,
      const RtpHeaderExtensionMap& extension_map);

  template <typename ParsedType>
  const std::vector<ParsedType>* GetRtpPacketsBySsrc(
      const ParsedRtcEventLogNew* parsed_log,
      uint32_t ssrc);

  template <typename EventType, typename ParsedType>
  void TestRtpPackets();

  std::deque<std::unique_ptr<RtcEvent>> history_;
  std::unique_ptr<RtcEventLogEncoder> encoder_;
  ParsedRtcEventLogNew parsed_log_;
  const uint64_t seed_;
  Random prng_;
  const bool new_encoding_;
  const size_t event_count_;
  const bool force_repeated_fields_;
  test::EventGenerator gen_;
  test::EventVerifier verifier_;
};

void RtcEventLogEncoderTest::TestRtcEventAudioNetworkAdaptation(
    const std::vector<std::unique_ptr<RtcEventAudioNetworkAdaptation>>&
        events) {
  ASSERT_TRUE(history_.empty()) << "Function should be called once per test.";

  for (auto& event : events) {
    history_.push_back(event->Copy());
  }

  std::string encoded = encoder_->EncodeBatch(history_.begin(), history_.end());
  ASSERT_TRUE(parsed_log_.ParseString(encoded));
  const auto& ana_configs = parsed_log_.audio_network_adaptation_events();

  ASSERT_EQ(ana_configs.size(), events.size());
  for (size_t i = 0; i < events.size(); ++i) {
    verifier_.VerifyLoggedAudioNetworkAdaptationEvent(*events[i],
                                                      ana_configs[i]);
  }
}

template <>
std::unique_ptr<RtcEventRtpPacketIncoming> RtcEventLogEncoderTest::NewRtpPacket(
    uint32_t ssrc,
    const RtpHeaderExtensionMap& extension_map) {
  return gen_.NewRtpPacketIncoming(ssrc, extension_map, false);
}

template <>
std::unique_ptr<RtcEventRtpPacketOutgoing> RtcEventLogEncoderTest::NewRtpPacket(
    uint32_t ssrc,
    const RtpHeaderExtensionMap& extension_map) {
  return gen_.NewRtpPacketOutgoing(ssrc, extension_map, false);
}

template <>
const std::vector<LoggedRtpPacketIncoming>*
RtcEventLogEncoderTest::GetRtpPacketsBySsrc(
    const ParsedRtcEventLogNew* parsed_log,
    uint32_t ssrc) {
  const auto& incoming_streams = parsed_log->incoming_rtp_packets_by_ssrc();
  for (const auto& stream : incoming_streams) {
    if (stream.ssrc == ssrc) {
      return &stream.incoming_packets;
    }
  }
  return nullptr;
}

template <>
const std::vector<LoggedRtpPacketOutgoing>*
RtcEventLogEncoderTest::GetRtpPacketsBySsrc(
    const ParsedRtcEventLogNew* parsed_log,
    uint32_t ssrc) {
  const auto& outgoing_streams = parsed_log->outgoing_rtp_packets_by_ssrc();
  for (const auto& stream : outgoing_streams) {
    if (stream.ssrc == ssrc) {
      return &stream.outgoing_packets;
    }
  }
  return nullptr;
}

template <typename EventType, typename ParsedType>
void RtcEventLogEncoderTest::TestRtpPackets() {
  // SSRCs will be randomly assigned out of this small pool, significant only
  // in that it also covers such edge cases as SSRC = 0 and SSRC = 0xffffffff.
  // The pool is intentionally small, so as to produce collisions.
  const std::vector<uint32_t> kSsrcPool = {0x00000000, 0x12345678, 0xabcdef01,
                                           0xffffffff, 0x20171024, 0x19840730,
                                           0x19831230};

  // TODO(terelius): Test extensions for legacy encoding, too.
  RtpHeaderExtensionMap extension_map;
  if (new_encoding_) {
    extension_map = gen_.NewRtpHeaderExtensionMap(true);
  }

  // Simulate |event_count_| RTP packets, with SSRCs assigned randomly
  // out of the small pool above.
  std::map<uint32_t, std::vector<std::unique_ptr<EventType>>> events_by_ssrc;
  for (size_t i = 0; i < event_count_; ++i) {
    const uint32_t ssrc = kSsrcPool[prng_.Rand(kSsrcPool.size() - 1)];
    std::unique_ptr<EventType> event =
        (events_by_ssrc[ssrc].empty() || !force_repeated_fields_)
            ? NewRtpPacket<EventType>(ssrc, extension_map)
            : events_by_ssrc[ssrc][0]->Copy();
    history_.push_back(event->Copy());
    events_by_ssrc[ssrc].emplace_back(std::move(event));
  }

  // Encode and parse.
  std::string encoded = encoder_->EncodeBatch(history_.begin(), history_.end());
  ASSERT_TRUE(parsed_log_.ParseString(encoded));

  // For each SSRC, make sure the RTP packets associated with it to have been
  // correctly encoded and parsed.
  for (auto it = events_by_ssrc.begin(); it != events_by_ssrc.end(); ++it) {
    const uint32_t ssrc = it->first;
    const auto& original_packets = it->second;
    const std::vector<ParsedType>* parsed_rtp_packets =
        GetRtpPacketsBySsrc<ParsedType>(&parsed_log_, ssrc);
    ASSERT_NE(parsed_rtp_packets, nullptr);
    ASSERT_EQ(original_packets.size(), parsed_rtp_packets->size());
    for (size_t i = 0; i < original_packets.size(); ++i) {
      verifier_.VerifyLoggedRtpPacket<EventType, ParsedType>(
          *original_packets[i], (*parsed_rtp_packets)[i]);
    }
  }
}

TEST_P(RtcEventLogEncoderTest, RtcEventAlrState) {
  std::vector<std::unique_ptr<RtcEventAlrState>> events(event_count_);
  for (size_t i = 0; i < event_count_; ++i) {
    events[i] = (i == 0 || !force_repeated_fields_) ? gen_.NewAlrState()
                                                    : events[0]->Copy();
    history_.push_back(events[i]->Copy());
  }

  std::string encoded = encoder_->EncodeBatch(history_.begin(), history_.end());
  ASSERT_TRUE(parsed_log_.ParseString(encoded));
  const auto& alr_state_events = parsed_log_.alr_state_events();

  ASSERT_EQ(alr_state_events.size(), event_count_);
  for (size_t i = 0; i < event_count_; ++i) {
    verifier_.VerifyLoggedAlrStateEvent(*events[i], alr_state_events[i]);
  }
}

TEST_P(RtcEventLogEncoderTest, RtcEventAudioNetworkAdaptationBitrate) {
  std::vector<std::unique_ptr<RtcEventAudioNetworkAdaptation>> events(
      event_count_);
  for (size_t i = 0; i < event_count_; ++i) {
    if (i == 0 || !force_repeated_fields_) {
      auto runtime_config = absl::make_unique<AudioEncoderRuntimeConfig>();
      const int bitrate_bps = rtc::checked_cast<int>(
          prng_.Rand(0, std::numeric_limits<int32_t>::max()));
      runtime_config->bitrate_bps = bitrate_bps;
      events[i] = absl::make_unique<RtcEventAudioNetworkAdaptation>(
          std::move(runtime_config));
    } else {
      events[i] = events[0]->Copy();
    }
  }
  TestRtcEventAudioNetworkAdaptation(events);
}

TEST_P(RtcEventLogEncoderTest, RtcEventAudioNetworkAdaptationFrameLength) {
  std::vector<std::unique_ptr<RtcEventAudioNetworkAdaptation>> events(
      event_count_);
  for (size_t i = 0; i < event_count_; ++i) {
    if (i == 0 || !force_repeated_fields_) {
      auto runtime_config = absl::make_unique<AudioEncoderRuntimeConfig>();
      const int frame_length_ms = prng_.Rand(1, 1000);
      runtime_config->frame_length_ms = frame_length_ms;
      events[i] = absl::make_unique<RtcEventAudioNetworkAdaptation>(
          std::move(runtime_config));
    } else {
      events[i] = events[0]->Copy();
    }
  }
  TestRtcEventAudioNetworkAdaptation(events);
}

TEST_P(RtcEventLogEncoderTest, RtcEventAudioNetworkAdaptationPacketLoss) {
  std::vector<std::unique_ptr<RtcEventAudioNetworkAdaptation>> events(
      event_count_);
  for (size_t i = 0; i < event_count_; ++i) {
    if (i == 0 || !force_repeated_fields_) {
      // To simplify the test, we just check powers of two.
      const float plr = std::pow(0.5f, prng_.Rand(1, 8));
      auto runtime_config = absl::make_unique<AudioEncoderRuntimeConfig>();
      runtime_config->uplink_packet_loss_fraction = plr;
      events[i] = absl::make_unique<RtcEventAudioNetworkAdaptation>(
          std::move(runtime_config));
    } else {
      events[i] = events[0]->Copy();
    }
  }
  TestRtcEventAudioNetworkAdaptation(events);
}

TEST_P(RtcEventLogEncoderTest, RtcEventAudioNetworkAdaptationFec) {
  std::vector<std::unique_ptr<RtcEventAudioNetworkAdaptation>> events(
      event_count_);
  for (size_t i = 0; i < event_count_; ++i) {
    if (i == 0 || !force_repeated_fields_) {
      auto runtime_config = absl::make_unique<AudioEncoderRuntimeConfig>();
      runtime_config->enable_fec = prng_.Rand<bool>();
      events[i] = absl::make_unique<RtcEventAudioNetworkAdaptation>(
          std::move(runtime_config));
    } else {
      events[i] = events[0]->Copy();
    }
  }
  TestRtcEventAudioNetworkAdaptation(events);
}

TEST_P(RtcEventLogEncoderTest, RtcEventAudioNetworkAdaptationDtx) {
  std::vector<std::unique_ptr<RtcEventAudioNetworkAdaptation>> events(
      event_count_);
  for (size_t i = 0; i < event_count_; ++i) {
    if (i == 0 || !force_repeated_fields_) {
      auto runtime_config = absl::make_unique<AudioEncoderRuntimeConfig>();
      runtime_config->enable_dtx = prng_.Rand<bool>();
      events[i] = absl::make_unique<RtcEventAudioNetworkAdaptation>(
          std::move(runtime_config));
    } else {
      events[i] = events[0]->Copy();
    }
  }
  TestRtcEventAudioNetworkAdaptation(events);
}

TEST_P(RtcEventLogEncoderTest, RtcEventAudioNetworkAdaptationChannels) {
  std::vector<std::unique_ptr<RtcEventAudioNetworkAdaptation>> events(
      event_count_);
  for (size_t i = 0; i < event_count_; ++i) {
    if (i == 0 || !force_repeated_fields_) {
      auto runtime_config = absl::make_unique<AudioEncoderRuntimeConfig>();
      runtime_config->num_channels = prng_.Rand(1, 2);
      events[i] = absl::make_unique<RtcEventAudioNetworkAdaptation>(
          std::move(runtime_config));
    } else {
      events[i] = events[0]->Copy();
    }
  }
  TestRtcEventAudioNetworkAdaptation(events);
}

TEST_P(RtcEventLogEncoderTest, RtcEventAudioNetworkAdaptationAll) {
  std::vector<std::unique_ptr<RtcEventAudioNetworkAdaptation>> events(
      event_count_);
  for (size_t i = 0; i < event_count_; ++i) {
    if (i == 0 || !force_repeated_fields_) {
      auto runtime_config = absl::make_unique<AudioEncoderRuntimeConfig>();
      runtime_config->bitrate_bps = rtc::checked_cast<int>(
          prng_.Rand(0, std::numeric_limits<int32_t>::max()));
      runtime_config->frame_length_ms = prng_.Rand(1, 1000);
      runtime_config->uplink_packet_loss_fraction =
          std::pow(0.5f, prng_.Rand(1, 8));
      runtime_config->enable_fec = prng_.Rand<bool>();
      runtime_config->enable_dtx = prng_.Rand<bool>();
      runtime_config->num_channels = prng_.Rand(1, 2);
      events[i] = absl::make_unique<RtcEventAudioNetworkAdaptation>(
          std::move(runtime_config));
    } else {
      events[i] = events[0]->Copy();
    }
  }
  TestRtcEventAudioNetworkAdaptation(events);
}

TEST_P(RtcEventLogEncoderTest, RtcEventAudioPlayout) {
  // SSRCs will be randomly assigned out of this small pool, significant only
  // in that it also covers such edge cases as SSRC = 0 and SSRC = 0xffffffff.
  // The pool is intentionally small, so as to produce collisions.
  const std::vector<uint32_t> kSsrcPool = {0x00000000, 0x12345678, 0xabcdef01,
                                           0xffffffff, 0x20171024, 0x19840730,
                                           0x19831230};

  std::map<uint32_t, std::vector<std::unique_ptr<RtcEventAudioPlayout>>>
      original_events_by_ssrc;
  for (size_t i = 0; i < event_count_; ++i) {
    const uint32_t ssrc = kSsrcPool[prng_.Rand(kSsrcPool.size() - 1)];
    std::unique_ptr<RtcEventAudioPlayout> event =
        (original_events_by_ssrc[ssrc].empty() || !force_repeated_fields_)
            ? gen_.NewAudioPlayout(ssrc)
            : original_events_by_ssrc[ssrc][0]->Copy();
    history_.push_back(event->Copy());
    original_events_by_ssrc[ssrc].push_back(std::move(event));
  }

  std::string encoded = encoder_->EncodeBatch(history_.begin(), history_.end());
  ASSERT_TRUE(parsed_log_.ParseString(encoded));

  const auto& parsed_playout_events_by_ssrc =
      parsed_log_.audio_playout_events();

  // Same number of distinct SSRCs.
  ASSERT_EQ(parsed_playout_events_by_ssrc.size(),
            original_events_by_ssrc.size());

  for (auto& original_event_it : original_events_by_ssrc) {
    const uint32_t ssrc = original_event_it.first;
    const auto& original_playout_events = original_event_it.second;

    const auto& parsed_event_it = parsed_playout_events_by_ssrc.find(ssrc);
    ASSERT_TRUE(parsed_event_it != parsed_playout_events_by_ssrc.end());
    const auto& parsed_playout_events = parsed_event_it->second;

    // Same number playout events for the SSRC under examination.
    ASSERT_EQ(original_playout_events.size(), parsed_playout_events.size());

    for (size_t i = 0; i < original_playout_events.size(); ++i) {
      verifier_.VerifyLoggedAudioPlayoutEvent(*original_playout_events[i],
                                              parsed_playout_events[i]);
    }
  }
}

// TODO(eladalon/terelius): Test with multiple events in the batch.
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
  verifier_.VerifyLoggedAudioRecvConfig(*event, audio_recv_configs[0]);
}

// TODO(eladalon/terelius): Test with multiple events in the batch.
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
  verifier_.VerifyLoggedAudioSendConfig(*event, audio_send_configs[0]);
}

TEST_P(RtcEventLogEncoderTest, RtcEventBweUpdateDelayBased) {
  std::vector<std::unique_ptr<RtcEventBweUpdateDelayBased>> events(
      event_count_);
  for (size_t i = 0; i < event_count_; ++i) {
    events[i] = (i == 0 || !force_repeated_fields_)
                    ? gen_.NewBweUpdateDelayBased()
                    : events[0]->Copy();
    history_.push_back(events[i]->Copy());
  }

  std::string encoded = encoder_->EncodeBatch(history_.begin(), history_.end());
  ASSERT_TRUE(parsed_log_.ParseString(encoded));

  const auto& bwe_delay_updates = parsed_log_.bwe_delay_updates();
  ASSERT_EQ(bwe_delay_updates.size(), event_count_);

  for (size_t i = 0; i < event_count_; ++i) {
    verifier_.VerifyLoggedBweDelayBasedUpdate(*events[i], bwe_delay_updates[i]);
  }
}

TEST_P(RtcEventLogEncoderTest, RtcEventBweUpdateLossBased) {
  std::vector<std::unique_ptr<RtcEventBweUpdateLossBased>> events(event_count_);
  for (size_t i = 0; i < event_count_; ++i) {
    events[i] = (i == 0 || !force_repeated_fields_)
                    ? gen_.NewBweUpdateLossBased()
                    : events[0]->Copy();
    history_.push_back(events[i]->Copy());
  }

  std::string encoded = encoder_->EncodeBatch(history_.begin(), history_.end());
  ASSERT_TRUE(parsed_log_.ParseString(encoded));

  const auto& bwe_loss_updates = parsed_log_.bwe_loss_updates();
  ASSERT_EQ(bwe_loss_updates.size(), event_count_);

  for (size_t i = 0; i < event_count_; ++i) {
    verifier_.VerifyLoggedBweLossBasedUpdate(*events[i], bwe_loss_updates[i]);
  }
}

// TODO(eladalon/terelius): Test with multiple events in the batch.
TEST_P(RtcEventLogEncoderTest, RtcEventIceCandidatePairConfig) {
  std::unique_ptr<RtcEventIceCandidatePairConfig> event =
      gen_.NewIceCandidatePairConfig();
  history_.push_back(event->Copy());

  std::string encoded = encoder_->EncodeBatch(history_.begin(), history_.end());
  ASSERT_TRUE(parsed_log_.ParseString(encoded));
  const auto& ice_candidate_pair_configs =
      parsed_log_.ice_candidate_pair_configs();

  ASSERT_EQ(ice_candidate_pair_configs.size(), 1u);
  verifier_.VerifyLoggedIceCandidatePairConfig(*event,
                                               ice_candidate_pair_configs[0]);
}

// TODO(eladalon/terelius): Test with multiple events in the batch.
TEST_P(RtcEventLogEncoderTest, RtcEventIceCandidatePair) {
  std::unique_ptr<RtcEventIceCandidatePair> event = gen_.NewIceCandidatePair();
  history_.push_back(event->Copy());

  std::string encoded = encoder_->EncodeBatch(history_.begin(), history_.end());
  ASSERT_TRUE(parsed_log_.ParseString(encoded));
  const auto& ice_candidate_pair_events =
      parsed_log_.ice_candidate_pair_events();

  ASSERT_EQ(ice_candidate_pair_events.size(), 1u);
  verifier_.VerifyLoggedIceCandidatePairEvent(*event,
                                              ice_candidate_pair_events[0]);
}

TEST_P(RtcEventLogEncoderTest, RtcEventLoggingStarted) {
  const int64_t timestamp_us = rtc::TimeMicros();
  const int64_t utc_time_us = rtc::TimeUTCMicros();

  ASSERT_TRUE(parsed_log_.ParseString(
      encoder_->EncodeLogStart(timestamp_us, utc_time_us)));
  const auto& start_log_events = parsed_log_.start_log_events();

  ASSERT_EQ(start_log_events.size(), 1u);
  verifier_.VerifyLoggedStartEvent(timestamp_us, utc_time_us,
                                   start_log_events[0]);
}

TEST_P(RtcEventLogEncoderTest, RtcEventLoggingStopped) {
  const int64_t timestamp_us = rtc::TimeMicros();

  ASSERT_TRUE(parsed_log_.ParseString(encoder_->EncodeLogEnd(timestamp_us)));
  const auto& stop_log_events = parsed_log_.stop_log_events();

  ASSERT_EQ(stop_log_events.size(), 1u);
  verifier_.VerifyLoggedStopEvent(timestamp_us, stop_log_events[0]);
}

// TODO(eladalon/terelius): Test with multiple events in the batch.
TEST_P(RtcEventLogEncoderTest, RtcEventProbeClusterCreated) {
  std::unique_ptr<RtcEventProbeClusterCreated> event =
      gen_.NewProbeClusterCreated();
  history_.push_back(event->Copy());

  std::string encoded = encoder_->EncodeBatch(history_.begin(), history_.end());
  ASSERT_TRUE(parsed_log_.ParseString(encoded));
  const auto& bwe_probe_cluster_created_events =
      parsed_log_.bwe_probe_cluster_created_events();

  ASSERT_EQ(bwe_probe_cluster_created_events.size(), 1u);
  verifier_.VerifyLoggedBweProbeClusterCreatedEvent(
      *event, bwe_probe_cluster_created_events[0]);
}

// TODO(eladalon/terelius): Test with multiple events in the batch.
TEST_P(RtcEventLogEncoderTest, RtcEventProbeResultFailure) {
  std::unique_ptr<RtcEventProbeResultFailure> event =
      gen_.NewProbeResultFailure();
  history_.push_back(event->Copy());

  std::string encoded = encoder_->EncodeBatch(history_.begin(), history_.end());
  ASSERT_TRUE(parsed_log_.ParseString(encoded));
  const auto& bwe_probe_failure_events = parsed_log_.bwe_probe_failure_events();

  ASSERT_EQ(bwe_probe_failure_events.size(), 1u);
  verifier_.VerifyLoggedBweProbeFailureEvent(*event,
                                             bwe_probe_failure_events[0]);
}

// TODO(eladalon/terelius): Test with multiple events in the batch.
TEST_P(RtcEventLogEncoderTest, RtcEventProbeResultSuccess) {
  std::unique_ptr<RtcEventProbeResultSuccess> event =
      gen_.NewProbeResultSuccess();
  history_.push_back(event->Copy());

  std::string encoded = encoder_->EncodeBatch(history_.begin(), history_.end());
  ASSERT_TRUE(parsed_log_.ParseString(encoded));
  const auto& bwe_probe_success_events = parsed_log_.bwe_probe_success_events();

  ASSERT_EQ(bwe_probe_success_events.size(), 1u);
  verifier_.VerifyLoggedBweProbeSuccessEvent(*event,
                                             bwe_probe_success_events[0]);
}

TEST_P(RtcEventLogEncoderTest, RtcEventRtcpPacketIncoming) {
  if (!new_encoding_ && force_repeated_fields_) {
    // The old encoding does not work with duplicated packets. Since the legacy
    // encoding is being phased out, we will not fix this.
    return;
  }

  std::vector<std::unique_ptr<RtcEventRtcpPacketIncoming>> events(event_count_);
  for (size_t i = 0; i < event_count_; ++i) {
    events[i] = (i == 0 || !force_repeated_fields_)
                    ? gen_.NewRtcpPacketIncoming()
                    : events[0]->Copy();
    history_.push_back(events[i]->Copy());
  }

  std::string encoded = encoder_->EncodeBatch(history_.begin(), history_.end());
  ASSERT_TRUE(parsed_log_.ParseString(encoded));

  const auto& incoming_rtcp_packets = parsed_log_.incoming_rtcp_packets();
  ASSERT_EQ(incoming_rtcp_packets.size(), event_count_);

  for (size_t i = 0; i < event_count_; ++i) {
    verifier_.VerifyLoggedRtcpPacketIncoming(*events[i],
                                             incoming_rtcp_packets[i]);
  }
}

TEST_P(RtcEventLogEncoderTest, RtcEventRtcpPacketOutgoing) {
  std::vector<std::unique_ptr<RtcEventRtcpPacketOutgoing>> events(event_count_);
  for (size_t i = 0; i < event_count_; ++i) {
    events[i] = (i == 0 || !force_repeated_fields_)
                    ? gen_.NewRtcpPacketOutgoing()
                    : events[0]->Copy();
    history_.push_back(events[i]->Copy());
  }

  std::string encoded = encoder_->EncodeBatch(history_.begin(), history_.end());
  ASSERT_TRUE(parsed_log_.ParseString(encoded));

  const auto& outgoing_rtcp_packets = parsed_log_.outgoing_rtcp_packets();
  ASSERT_EQ(outgoing_rtcp_packets.size(), event_count_);

  for (size_t i = 0; i < event_count_; ++i) {
    verifier_.VerifyLoggedRtcpPacketOutgoing(*events[i],
                                             outgoing_rtcp_packets[i]);
  }
}

TEST_P(RtcEventLogEncoderTest, RtcEventRtpPacketIncoming) {
  TestRtpPackets<RtcEventRtpPacketIncoming, LoggedRtpPacketIncoming>();
}

TEST_P(RtcEventLogEncoderTest, RtcEventRtpPacketOutgoing) {
  TestRtpPackets<RtcEventRtpPacketOutgoing, LoggedRtpPacketOutgoing>();
}

// TODO(eladalon/terelius): Test with multiple events in the batch.
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
  verifier_.VerifyLoggedVideoRecvConfig(*event, video_recv_configs[0]);
}

// TODO(eladalon/terelius): Test with multiple events in the batch.
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
  verifier_.VerifyLoggedVideoSendConfig(*event, video_send_configs[0]);
}

INSTANTIATE_TEST_CASE_P(
    RandomSeeds,
    RtcEventLogEncoderTest,
    ::testing::Combine(/* Random seed*: */ ::testing::Values(1, 2, 3, 4, 5),
                       /* Encoding: */ ::testing::Bool(),
                       /* Event count: */ ::testing::Values(1, 2, 10, 100),
                       /* Repeated fields: */ ::testing::Bool()));

}  // namespace webrtc
