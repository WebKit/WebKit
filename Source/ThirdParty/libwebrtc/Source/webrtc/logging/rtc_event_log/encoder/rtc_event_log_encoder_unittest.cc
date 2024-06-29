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
#include <memory>
#include <string>
#include <tuple>

#include "api/field_trials_view.h"
#include "logging/rtc_event_log/encoder/rtc_event_log_encoder_legacy.h"
#include "logging/rtc_event_log/encoder/rtc_event_log_encoder_new_format.h"
#include "logging/rtc_event_log/encoder/rtc_event_log_encoder_v3.h"
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
#include "logging/rtc_event_log/rtc_event_log_parser.h"
#include "logging/rtc_event_log/rtc_event_log_unittest_helper.h"
#include "modules/audio_coding/audio_network_adaptor/include/audio_network_adaptor_config.h"
#include "modules/rtp_rtcp/source/rtcp_packet/bye.h"
#include "modules/rtp_rtcp/source/rtp_header_extensions.h"
#include "rtc_base/fake_clock.h"
#include "rtc_base/random.h"
#include "test/explicit_key_value_config.h"
#include "test/gtest.h"

namespace webrtc {

using test::ExplicitKeyValueConfig;

class RtcEventLogEncoderTest
    : public ::testing::TestWithParam<
          std::tuple<int, RtcEventLog::EncodingType, size_t, bool>> {
 protected:
  RtcEventLogEncoderTest()
      : seed_(std::get<0>(GetParam())),
        prng_(seed_),
        encoding_type_(std::get<1>(GetParam())),
        event_count_(std::get<2>(GetParam())),
        force_repeated_fields_(std::get<3>(GetParam())),
        gen_(seed_ * 880001UL),
        verifier_(encoding_type_) {}
  ~RtcEventLogEncoderTest() override = default;

  std::unique_ptr<RtcEventLogEncoder> CreateEncoder(
      const FieldTrialsView& field_trials = ExplicitKeyValueConfig("")) {
    std::unique_ptr<RtcEventLogEncoder> encoder;
    switch (encoding_type_) {
      case RtcEventLog::EncodingType::Legacy:
        encoder = std::make_unique<RtcEventLogEncoderLegacy>();
        break;
      case RtcEventLog::EncodingType::NewFormat:
        encoder = std::make_unique<RtcEventLogEncoderNewFormat>(field_trials);
        break;
      case RtcEventLog::EncodingType::ProtoFree:
        encoder = std::make_unique<RtcEventLogEncoderV3>();
        break;
    }
    encoded_ = encoder->EncodeLogStart(rtc::TimeMillis(), rtc::TimeUTCMillis());
    return encoder;
  }

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
      const ParsedRtcEventLog* parsed_log,
      uint32_t ssrc);

  template <typename EventType, typename ParsedType>
  void TestRtpPackets(RtcEventLogEncoder& encoder);

  std::deque<std::unique_ptr<RtcEvent>> history_;
  ParsedRtcEventLog parsed_log_;
  const uint64_t seed_;
  Random prng_;
  const RtcEventLog::EncodingType encoding_type_;
  const size_t event_count_;
  const bool force_repeated_fields_;
  test::EventGenerator gen_;
  test::EventVerifier verifier_;
  std::string encoded_;
};

void RtcEventLogEncoderTest::TestRtcEventAudioNetworkAdaptation(
    const std::vector<std::unique_ptr<RtcEventAudioNetworkAdaptation>>&
        events) {
  ASSERT_TRUE(history_.empty()) << "Function should be called once per test.";
  std::unique_ptr<RtcEventLogEncoder> encoder = CreateEncoder();

  for (auto& event : events) {
    history_.push_back(event->Copy());
  }

  encoded_ += encoder->EncodeBatch(history_.begin(), history_.end());
  ASSERT_TRUE(parsed_log_.ParseString(encoded_).ok());
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
RtcEventLogEncoderTest::GetRtpPacketsBySsrc(const ParsedRtcEventLog* parsed_log,
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
RtcEventLogEncoderTest::GetRtpPacketsBySsrc(const ParsedRtcEventLog* parsed_log,
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
void RtcEventLogEncoderTest::TestRtpPackets(RtcEventLogEncoder& encoder) {
  // SSRCs will be randomly assigned out of this small pool, significant only
  // in that it also covers such edge cases as SSRC = 0 and SSRC = 0xffffffff.
  // The pool is intentionally small, so as to produce collisions.
  const std::vector<uint32_t> kSsrcPool = {0x00000000, 0x12345678, 0xabcdef01,
                                           0xffffffff, 0x20171024, 0x19840730,
                                           0x19831230};

  // TODO(terelius): Test extensions for legacy encoding, too.
  RtpHeaderExtensionMap extension_map;
  if (encoding_type_ != RtcEventLog::EncodingType::Legacy) {
    extension_map = gen_.NewRtpHeaderExtensionMap(true);
  }

  // Simulate `event_count_` RTP packets, with SSRCs assigned randomly
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
  encoded_ += encoder.EncodeBatch(history_.begin(), history_.end());
  ASSERT_TRUE(parsed_log_.ParseString(encoded_).ok());

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
  std::unique_ptr<RtcEventLogEncoder> encoder = CreateEncoder();
  std::vector<std::unique_ptr<RtcEventAlrState>> events(event_count_);
  for (size_t i = 0; i < event_count_; ++i) {
    events[i] = (i == 0 || !force_repeated_fields_) ? gen_.NewAlrState()
                                                    : events[0]->Copy();
    history_.push_back(events[i]->Copy());
  }

  encoded_ += encoder->EncodeBatch(history_.begin(), history_.end());
  ASSERT_TRUE(parsed_log_.ParseString(encoded_).ok());
  const auto& alr_state_events = parsed_log_.alr_state_events();

  ASSERT_EQ(alr_state_events.size(), event_count_);
  for (size_t i = 0; i < event_count_; ++i) {
    verifier_.VerifyLoggedAlrStateEvent(*events[i], alr_state_events[i]);
  }
}

TEST_P(RtcEventLogEncoderTest, RtcEventRouteChange) {
  if (encoding_type_ == RtcEventLog::EncodingType::Legacy) {
    return;
  }
  std::unique_ptr<RtcEventLogEncoder> encoder = CreateEncoder();
  std::vector<std::unique_ptr<RtcEventRouteChange>> events(event_count_);
  for (size_t i = 0; i < event_count_; ++i) {
    events[i] = (i == 0 || !force_repeated_fields_) ? gen_.NewRouteChange()
                                                    : events[0]->Copy();
    history_.push_back(events[i]->Copy());
  }

  encoded_ += encoder->EncodeBatch(history_.begin(), history_.end());
  ASSERT_TRUE(parsed_log_.ParseString(encoded_).ok());
  const auto& route_change_events = parsed_log_.route_change_events();

  ASSERT_EQ(route_change_events.size(), event_count_);
  for (size_t i = 0; i < event_count_; ++i) {
    verifier_.VerifyLoggedRouteChangeEvent(*events[i], route_change_events[i]);
  }
}

TEST_P(RtcEventLogEncoderTest, RtcEventRemoteEstimate) {
  std::unique_ptr<RtcEventLogEncoder> encoder = CreateEncoder();
  std::vector<std::unique_ptr<RtcEventRemoteEstimate>> events(event_count_);
  for (size_t i = 0; i < event_count_; ++i) {
    events[i] = (i == 0 || !force_repeated_fields_)
                    ? gen_.NewRemoteEstimate()
                    : std::make_unique<RtcEventRemoteEstimate>(*events[0]);
    history_.push_back(std::make_unique<RtcEventRemoteEstimate>(*events[i]));
  }

  encoded_ += encoder->EncodeBatch(history_.begin(), history_.end());
  ASSERT_TRUE(parsed_log_.ParseString(encoded_).ok());
  const auto& parsed_events = parsed_log_.remote_estimate_events();

  ASSERT_EQ(parsed_events.size(), event_count_);
  for (size_t i = 0; i < event_count_; ++i) {
    verifier_.VerifyLoggedRemoteEstimateEvent(*events[i], parsed_events[i]);
  }
}

TEST_P(RtcEventLogEncoderTest, RtcEventAudioNetworkAdaptationBitrate) {
  std::vector<std::unique_ptr<RtcEventAudioNetworkAdaptation>> events(
      event_count_);
  for (size_t i = 0; i < event_count_; ++i) {
    if (i == 0 || !force_repeated_fields_) {
      auto runtime_config = std::make_unique<AudioEncoderRuntimeConfig>();
      const int bitrate_bps = rtc::checked_cast<int>(
          prng_.Rand(0, std::numeric_limits<int32_t>::max()));
      runtime_config->bitrate_bps = bitrate_bps;
      events[i] = std::make_unique<RtcEventAudioNetworkAdaptation>(
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
      auto runtime_config = std::make_unique<AudioEncoderRuntimeConfig>();
      const int frame_length_ms = prng_.Rand(1, 1000);
      runtime_config->frame_length_ms = frame_length_ms;
      events[i] = std::make_unique<RtcEventAudioNetworkAdaptation>(
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
      auto runtime_config = std::make_unique<AudioEncoderRuntimeConfig>();
      runtime_config->uplink_packet_loss_fraction = plr;
      events[i] = std::make_unique<RtcEventAudioNetworkAdaptation>(
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
      auto runtime_config = std::make_unique<AudioEncoderRuntimeConfig>();
      runtime_config->enable_fec = prng_.Rand<bool>();
      events[i] = std::make_unique<RtcEventAudioNetworkAdaptation>(
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
      auto runtime_config = std::make_unique<AudioEncoderRuntimeConfig>();
      runtime_config->enable_dtx = prng_.Rand<bool>();
      events[i] = std::make_unique<RtcEventAudioNetworkAdaptation>(
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
      auto runtime_config = std::make_unique<AudioEncoderRuntimeConfig>();
      runtime_config->num_channels = prng_.Rand(1, 2);
      events[i] = std::make_unique<RtcEventAudioNetworkAdaptation>(
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
      auto runtime_config = std::make_unique<AudioEncoderRuntimeConfig>();
      runtime_config->bitrate_bps = rtc::checked_cast<int>(
          prng_.Rand(0, std::numeric_limits<int32_t>::max()));
      runtime_config->frame_length_ms = prng_.Rand(1, 1000);
      runtime_config->uplink_packet_loss_fraction =
          std::pow(0.5f, prng_.Rand(1, 8));
      runtime_config->enable_fec = prng_.Rand<bool>();
      runtime_config->enable_dtx = prng_.Rand<bool>();
      runtime_config->num_channels = prng_.Rand(1, 2);
      events[i] = std::make_unique<RtcEventAudioNetworkAdaptation>(
          std::move(runtime_config));
    } else {
      events[i] = events[0]->Copy();
    }
  }
  TestRtcEventAudioNetworkAdaptation(events);
}

TEST_P(RtcEventLogEncoderTest, RtcEventAudioPlayout) {
  std::unique_ptr<RtcEventLogEncoder> encoder = CreateEncoder();
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

  encoded_ += encoder->EncodeBatch(history_.begin(), history_.end());
  ASSERT_TRUE(parsed_log_.ParseString(encoded_).ok());

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

TEST_P(RtcEventLogEncoderTest, RtcEventNetEqSetMinimumDelayDecoded) {
  std::unique_ptr<RtcEventLogEncoder> encoder = CreateEncoder();
  // SSRCs will be randomly assigned out of this small pool, significant only
  // in that it also covers such edge cases as SSRC = 0 and SSRC = 0xffffffff.
  // The pool is intentionally small, so as to produce collisions.
  const std::vector<uint32_t> kSsrcPool = {0x00000000, 0x12345678, 0xabcdef01,
                                           0xffffffff, 0x20171024, 0x19840730,
                                           0x19831230};
  std::map<uint32_t, std::vector<std::unique_ptr<RtcEventNetEqSetMinimumDelay>>>
      original_events_by_ssrc;
  for (size_t i = 0; i < event_count_; ++i) {
    const uint32_t ssrc = kSsrcPool[prng_.Rand(kSsrcPool.size() - 1)];
    std::unique_ptr<RtcEventNetEqSetMinimumDelay> event =
        (original_events_by_ssrc[ssrc].empty() || !force_repeated_fields_)
            ? gen_.NewNetEqSetMinimumDelay(ssrc)
            : original_events_by_ssrc[ssrc][0]->Copy();
    history_.push_back(event->Copy());
    original_events_by_ssrc[ssrc].push_back(std::move(event));
  }

  encoded_ += encoder->EncodeBatch(history_.begin(), history_.end());
  ASSERT_TRUE(parsed_log_.ParseString(encoded_).ok());

  const auto& parsed_neteq_set_minimum_delay_events_by_ssrc =
      parsed_log_.neteq_set_minimum_delay_events();

  if (encoding_type_ == RtcEventLog::EncodingType::Legacy) {
    ASSERT_EQ(parsed_neteq_set_minimum_delay_events_by_ssrc.size(), 0u);
    return;
  }

  // Same number of distinct SSRCs.
  ASSERT_EQ(parsed_neteq_set_minimum_delay_events_by_ssrc.size(),
            original_events_by_ssrc.size());

  for (auto& original_event_it : original_events_by_ssrc) {
    const uint32_t ssrc = original_event_it.first;
    const auto& original_neteq_set_minimum_delay_events =
        original_event_it.second;

    const auto& parsed_event_it =
        parsed_neteq_set_minimum_delay_events_by_ssrc.find(ssrc);
    ASSERT_TRUE(parsed_event_it !=
                parsed_neteq_set_minimum_delay_events_by_ssrc.end());
    const auto& parsed_neteq_set_minimum_delay_events = parsed_event_it->second;

    // Same number playout events for the SSRC under examination.
    ASSERT_EQ(original_neteq_set_minimum_delay_events.size(),
              parsed_neteq_set_minimum_delay_events.size());

    for (size_t i = 0; i < original_neteq_set_minimum_delay_events.size();
         ++i) {
      verifier_.VerifyLoggedNetEqSetMinimumDelay(
          *original_neteq_set_minimum_delay_events[i],
          parsed_neteq_set_minimum_delay_events[i]);
    }
  }
}

// TODO(eladalon/terelius): Test with multiple events in the batch.
TEST_P(RtcEventLogEncoderTest, RtcEventAudioReceiveStreamConfig) {
  std::unique_ptr<RtcEventLogEncoder> encoder = CreateEncoder();
  uint32_t ssrc = prng_.Rand<uint32_t>();
  RtpHeaderExtensionMap extensions = gen_.NewRtpHeaderExtensionMap();
  std::unique_ptr<RtcEventAudioReceiveStreamConfig> event =
      gen_.NewAudioReceiveStreamConfig(ssrc, extensions);
  history_.push_back(event->Copy());

  encoded_ += encoder->EncodeBatch(history_.begin(), history_.end());
  ASSERT_TRUE(parsed_log_.ParseString(encoded_).ok());
  const auto& audio_recv_configs = parsed_log_.audio_recv_configs();

  ASSERT_EQ(audio_recv_configs.size(), 1u);
  verifier_.VerifyLoggedAudioRecvConfig(*event, audio_recv_configs[0]);
}

// TODO(eladalon/terelius): Test with multiple events in the batch.
TEST_P(RtcEventLogEncoderTest, RtcEventAudioSendStreamConfig) {
  std::unique_ptr<RtcEventLogEncoder> encoder = CreateEncoder();
  uint32_t ssrc = prng_.Rand<uint32_t>();
  RtpHeaderExtensionMap extensions = gen_.NewRtpHeaderExtensionMap();
  std::unique_ptr<RtcEventAudioSendStreamConfig> event =
      gen_.NewAudioSendStreamConfig(ssrc, extensions);
  history_.push_back(event->Copy());

  encoded_ += encoder->EncodeBatch(history_.begin(), history_.end());
  ASSERT_TRUE(parsed_log_.ParseString(encoded_).ok());
  const auto& audio_send_configs = parsed_log_.audio_send_configs();

  ASSERT_EQ(audio_send_configs.size(), 1u);
  verifier_.VerifyLoggedAudioSendConfig(*event, audio_send_configs[0]);
}

TEST_P(RtcEventLogEncoderTest, RtcEventBweUpdateDelayBased) {
  std::unique_ptr<RtcEventLogEncoder> encoder = CreateEncoder();
  std::vector<std::unique_ptr<RtcEventBweUpdateDelayBased>> events(
      event_count_);
  for (size_t i = 0; i < event_count_; ++i) {
    events[i] = (i == 0 || !force_repeated_fields_)
                    ? gen_.NewBweUpdateDelayBased()
                    : events[0]->Copy();
    history_.push_back(events[i]->Copy());
  }

  encoded_ += encoder->EncodeBatch(history_.begin(), history_.end());
  ASSERT_TRUE(parsed_log_.ParseString(encoded_).ok());

  const auto& bwe_delay_updates = parsed_log_.bwe_delay_updates();
  ASSERT_EQ(bwe_delay_updates.size(), event_count_);

  for (size_t i = 0; i < event_count_; ++i) {
    verifier_.VerifyLoggedBweDelayBasedUpdate(*events[i], bwe_delay_updates[i]);
  }
}

TEST_P(RtcEventLogEncoderTest, RtcEventBweUpdateLossBased) {
  std::unique_ptr<RtcEventLogEncoder> encoder = CreateEncoder();
  std::vector<std::unique_ptr<RtcEventBweUpdateLossBased>> events(event_count_);
  for (size_t i = 0; i < event_count_; ++i) {
    events[i] = (i == 0 || !force_repeated_fields_)
                    ? gen_.NewBweUpdateLossBased()
                    : events[0]->Copy();
    history_.push_back(events[i]->Copy());
  }

  encoded_ += encoder->EncodeBatch(history_.begin(), history_.end());
  ASSERT_TRUE(parsed_log_.ParseString(encoded_).ok());

  const auto& bwe_loss_updates = parsed_log_.bwe_loss_updates();
  ASSERT_EQ(bwe_loss_updates.size(), event_count_);

  for (size_t i = 0; i < event_count_; ++i) {
    verifier_.VerifyLoggedBweLossBasedUpdate(*events[i], bwe_loss_updates[i]);
  }
}

TEST_P(RtcEventLogEncoderTest, RtcEventGenericPacketReceived) {
  if (encoding_type_ == RtcEventLog::EncodingType::Legacy) {
    return;
  }
  std::unique_ptr<RtcEventLogEncoder> encoder = CreateEncoder();
  std::vector<std::unique_ptr<RtcEventGenericPacketReceived>> events(
      event_count_);
  for (size_t i = 0; i < event_count_; ++i) {
    events[i] = (i == 0 || !force_repeated_fields_)
                    ? gen_.NewGenericPacketReceived()
                    : events[0]->Copy();
    history_.push_back(events[i]->Copy());
  }

  encoded_ += encoder->EncodeBatch(history_.begin(), history_.end());
  ASSERT_TRUE(parsed_log_.ParseString(encoded_).ok());

  const auto& packets_received = parsed_log_.generic_packets_received();
  ASSERT_EQ(packets_received.size(), event_count_);

  for (size_t i = 0; i < event_count_; ++i) {
    verifier_.VerifyLoggedGenericPacketReceived(*events[i],
                                                packets_received[i]);
  }
}

TEST_P(RtcEventLogEncoderTest, RtcEventGenericPacketSent) {
  if (encoding_type_ == RtcEventLog::EncodingType::Legacy) {
    return;
  }
  std::unique_ptr<RtcEventLogEncoder> encoder = CreateEncoder();
  std::vector<std::unique_ptr<RtcEventGenericPacketSent>> events(event_count_);
  for (size_t i = 0; i < event_count_; ++i) {
    events[i] = (i == 0 || !force_repeated_fields_)
                    ? gen_.NewGenericPacketSent()
                    : events[0]->Copy();
    history_.push_back(events[i]->Copy());
  }

  encoded_ += encoder->EncodeBatch(history_.begin(), history_.end());
  ASSERT_TRUE(parsed_log_.ParseString(encoded_).ok());

  const auto& packets_sent = parsed_log_.generic_packets_sent();
  ASSERT_EQ(packets_sent.size(), event_count_);

  for (size_t i = 0; i < event_count_; ++i) {
    verifier_.VerifyLoggedGenericPacketSent(*events[i], packets_sent[i]);
  }
}

TEST_P(RtcEventLogEncoderTest, RtcEventGenericAcksReceived) {
  if (encoding_type_ == RtcEventLog::EncodingType::Legacy) {
    return;
  }
  std::unique_ptr<RtcEventLogEncoder> encoder = CreateEncoder();
  std::vector<std::unique_ptr<RtcEventGenericAckReceived>> events(event_count_);
  for (size_t i = 0; i < event_count_; ++i) {
    events[i] = (i == 0 || !force_repeated_fields_)
                    ? gen_.NewGenericAckReceived()
                    : events[0]->Copy();
    history_.push_back(events[i]->Copy());
  }

  encoded_ += encoder->EncodeBatch(history_.begin(), history_.end());
  ASSERT_TRUE(parsed_log_.ParseString(encoded_).ok());

  const auto& decoded_events = parsed_log_.generic_acks_received();
  ASSERT_EQ(decoded_events.size(), event_count_);

  for (size_t i = 0; i < event_count_; ++i) {
    verifier_.VerifyLoggedGenericAckReceived(*events[i], decoded_events[i]);
  }
}

TEST_P(RtcEventLogEncoderTest, RtcEventDtlsTransportState) {
  std::unique_ptr<RtcEventLogEncoder> encoder = CreateEncoder();
  std::vector<std::unique_ptr<RtcEventDtlsTransportState>> events(event_count_);
  for (size_t i = 0; i < event_count_; ++i) {
    events[i] = (i == 0 || !force_repeated_fields_)
                    ? gen_.NewDtlsTransportState()
                    : events[0]->Copy();
    history_.push_back(events[i]->Copy());
  }

  encoded_ += encoder->EncodeBatch(history_.begin(), history_.end());
  ASSERT_TRUE(parsed_log_.ParseString(encoded_).ok());

  const auto& dtls_transport_states = parsed_log_.dtls_transport_states();
  if (encoding_type_ == RtcEventLog::EncodingType::Legacy) {
    ASSERT_EQ(dtls_transport_states.size(), 0u);
    return;
  }

  ASSERT_EQ(dtls_transport_states.size(), event_count_);
  for (size_t i = 0; i < event_count_; ++i) {
    verifier_.VerifyLoggedDtlsTransportState(*events[i],
                                             dtls_transport_states[i]);
  }
}

TEST_P(RtcEventLogEncoderTest, RtcEventDtlsWritableState) {
  std::unique_ptr<RtcEventLogEncoder> encoder = CreateEncoder();
  std::vector<std::unique_ptr<RtcEventDtlsWritableState>> events(event_count_);
  for (size_t i = 0; i < event_count_; ++i) {
    events[i] = (i == 0 || !force_repeated_fields_)
                    ? gen_.NewDtlsWritableState()
                    : events[0]->Copy();
    history_.push_back(events[i]->Copy());
  }

  encoded_ += encoder->EncodeBatch(history_.begin(), history_.end());
  ASSERT_TRUE(parsed_log_.ParseString(encoded_).ok());

  const auto& dtls_writable_states = parsed_log_.dtls_writable_states();
  if (encoding_type_ == RtcEventLog::EncodingType::Legacy) {
    ASSERT_EQ(dtls_writable_states.size(), 0u);
    return;
  }

  ASSERT_EQ(dtls_writable_states.size(), event_count_);

  for (size_t i = 0; i < event_count_; ++i) {
    verifier_.VerifyLoggedDtlsWritableState(*events[i],
                                            dtls_writable_states[i]);
  }
}

TEST_P(RtcEventLogEncoderTest, RtcEventFrameDecoded) {
  std::unique_ptr<RtcEventLogEncoder> encoder = CreateEncoder();
  // SSRCs will be randomly assigned out of this small pool, significant only
  // in that it also covers such edge cases as SSRC = 0 and SSRC = 0xffffffff.
  // The pool is intentionally small, so as to produce collisions.
  const std::vector<uint32_t> kSsrcPool = {0x00000000, 0x12345678, 0xabcdef01,
                                           0xffffffff, 0x20171024, 0x19840730,
                                           0x19831230};

  std::map<uint32_t, std::vector<std::unique_ptr<RtcEventFrameDecoded>>>
      original_events_by_ssrc;
  for (size_t i = 0; i < event_count_; ++i) {
    const uint32_t ssrc = kSsrcPool[prng_.Rand(kSsrcPool.size() - 1)];
    std::unique_ptr<RtcEventFrameDecoded> event =
        (original_events_by_ssrc[ssrc].empty() || !force_repeated_fields_)
            ? gen_.NewFrameDecodedEvent(ssrc)
            : original_events_by_ssrc[ssrc][0]->Copy();
    history_.push_back(event->Copy());
    original_events_by_ssrc[ssrc].push_back(std::move(event));
  }

  encoded_ += encoder->EncodeBatch(history_.begin(), history_.end());
  auto status = parsed_log_.ParseString(encoded_);
  if (!status.ok())
    RTC_LOG(LS_ERROR) << status.message();
  ASSERT_TRUE(status.ok());

  const auto& decoded_frames_by_ssrc = parsed_log_.decoded_frames();
  if (encoding_type_ == RtcEventLog::EncodingType::Legacy) {
    ASSERT_EQ(decoded_frames_by_ssrc.size(), 0u);
    return;
  }

  // Same number of distinct SSRCs.
  ASSERT_EQ(decoded_frames_by_ssrc.size(), original_events_by_ssrc.size());

  for (const auto& original_event_it : original_events_by_ssrc) {
    const uint32_t ssrc = original_event_it.first;
    const std::vector<std::unique_ptr<RtcEventFrameDecoded>>& original_frames =
        original_event_it.second;

    const auto& parsed_event_it = decoded_frames_by_ssrc.find(ssrc);
    ASSERT_TRUE(parsed_event_it != decoded_frames_by_ssrc.end());
    const std::vector<LoggedFrameDecoded>& parsed_frames =
        parsed_event_it->second;

    // Same number events for the SSRC under examination.
    ASSERT_EQ(original_frames.size(), parsed_frames.size());

    for (size_t i = 0; i < original_frames.size(); ++i) {
      verifier_.VerifyLoggedFrameDecoded(*original_frames[i], parsed_frames[i]);
    }
  }
}

// TODO(eladalon/terelius): Test with multiple events in the batch.
TEST_P(RtcEventLogEncoderTest, RtcEventIceCandidatePairConfig) {
  std::unique_ptr<RtcEventLogEncoder> encoder = CreateEncoder();
  std::unique_ptr<RtcEventIceCandidatePairConfig> event =
      gen_.NewIceCandidatePairConfig();
  history_.push_back(event->Copy());

  encoded_ += encoder->EncodeBatch(history_.begin(), history_.end());
  ASSERT_TRUE(parsed_log_.ParseString(encoded_).ok());
  const auto& ice_candidate_pair_configs =
      parsed_log_.ice_candidate_pair_configs();

  ASSERT_EQ(ice_candidate_pair_configs.size(), 1u);
  verifier_.VerifyLoggedIceCandidatePairConfig(*event,
                                               ice_candidate_pair_configs[0]);
}

// TODO(eladalon/terelius): Test with multiple events in the batch.
TEST_P(RtcEventLogEncoderTest, RtcEventIceCandidatePair) {
  std::unique_ptr<RtcEventLogEncoder> encoder = CreateEncoder();
  std::unique_ptr<RtcEventIceCandidatePair> event = gen_.NewIceCandidatePair();
  history_.push_back(event->Copy());

  encoded_ += encoder->EncodeBatch(history_.begin(), history_.end());
  ASSERT_TRUE(parsed_log_.ParseString(encoded_).ok());
  const auto& ice_candidate_pair_events =
      parsed_log_.ice_candidate_pair_events();

  ASSERT_EQ(ice_candidate_pair_events.size(), 1u);
  verifier_.VerifyLoggedIceCandidatePairEvent(*event,
                                              ice_candidate_pair_events[0]);
}

TEST_P(RtcEventLogEncoderTest, RtcEventLoggingStarted) {
  std::unique_ptr<RtcEventLogEncoder> encoder = CreateEncoder();
  const int64_t timestamp_ms = prng_.Rand(1'000'000'000);
  const int64_t utc_time_ms = prng_.Rand(1'000'000'000);

  // Overwrite the previously encoded LogStart event.
  encoded_ = encoder->EncodeLogStart(timestamp_ms * 1000, utc_time_ms * 1000);
  ASSERT_TRUE(parsed_log_.ParseString(encoded_).ok());
  const auto& start_log_events = parsed_log_.start_log_events();

  ASSERT_EQ(start_log_events.size(), 1u);
  verifier_.VerifyLoggedStartEvent(timestamp_ms * 1000, utc_time_ms * 1000,
                                   start_log_events[0]);
}

TEST_P(RtcEventLogEncoderTest, RtcEventLoggingStopped) {
  std::unique_ptr<RtcEventLogEncoder> encoder = CreateEncoder();
  const int64_t start_timestamp_ms = prng_.Rand(1'000'000'000);
  const int64_t start_utc_time_ms = prng_.Rand(1'000'000'000);

  // Overwrite the previously encoded LogStart event.
  encoded_ = encoder->EncodeLogStart(start_timestamp_ms * 1000,
                                     start_utc_time_ms * 1000);

  const int64_t stop_timestamp_ms =
      prng_.Rand(start_timestamp_ms, 2'000'000'000);
  encoded_ += encoder->EncodeLogEnd(stop_timestamp_ms * 1000);
  ASSERT_TRUE(parsed_log_.ParseString(encoded_).ok());
  const auto& stop_log_events = parsed_log_.stop_log_events();

  ASSERT_EQ(stop_log_events.size(), 1u);
  verifier_.VerifyLoggedStopEvent(stop_timestamp_ms * 1000, stop_log_events[0]);
}

// TODO(eladalon/terelius): Test with multiple events in the batch.
TEST_P(RtcEventLogEncoderTest, RtcEventProbeClusterCreated) {
  std::unique_ptr<RtcEventLogEncoder> encoder = CreateEncoder();
  std::unique_ptr<RtcEventProbeClusterCreated> event =
      gen_.NewProbeClusterCreated();
  history_.push_back(event->Copy());

  encoded_ += encoder->EncodeBatch(history_.begin(), history_.end());
  ASSERT_TRUE(parsed_log_.ParseString(encoded_).ok());
  const auto& bwe_probe_cluster_created_events =
      parsed_log_.bwe_probe_cluster_created_events();

  ASSERT_EQ(bwe_probe_cluster_created_events.size(), 1u);
  verifier_.VerifyLoggedBweProbeClusterCreatedEvent(
      *event, bwe_probe_cluster_created_events[0]);
}

// TODO(eladalon/terelius): Test with multiple events in the batch.
TEST_P(RtcEventLogEncoderTest, RtcEventProbeResultFailure) {
  std::unique_ptr<RtcEventLogEncoder> encoder = CreateEncoder();
  std::unique_ptr<RtcEventProbeResultFailure> event =
      gen_.NewProbeResultFailure();
  history_.push_back(event->Copy());

  encoded_ += encoder->EncodeBatch(history_.begin(), history_.end());
  ASSERT_TRUE(parsed_log_.ParseString(encoded_).ok());
  const auto& bwe_probe_failure_events = parsed_log_.bwe_probe_failure_events();

  ASSERT_EQ(bwe_probe_failure_events.size(), 1u);
  verifier_.VerifyLoggedBweProbeFailureEvent(*event,
                                             bwe_probe_failure_events[0]);
}

// TODO(eladalon/terelius): Test with multiple events in the batch.
TEST_P(RtcEventLogEncoderTest, RtcEventProbeResultSuccess) {
  std::unique_ptr<RtcEventLogEncoder> encoder = CreateEncoder();
  std::unique_ptr<RtcEventProbeResultSuccess> event =
      gen_.NewProbeResultSuccess();
  history_.push_back(event->Copy());

  encoded_ += encoder->EncodeBatch(history_.begin(), history_.end());
  ASSERT_TRUE(parsed_log_.ParseString(encoded_).ok());
  const auto& bwe_probe_success_events = parsed_log_.bwe_probe_success_events();

  ASSERT_EQ(bwe_probe_success_events.size(), 1u);
  verifier_.VerifyLoggedBweProbeSuccessEvent(*event,
                                             bwe_probe_success_events[0]);
}

TEST_P(RtcEventLogEncoderTest, RtcEventRtcpPacketIncoming) {
  if (force_repeated_fields_) {
    // RTCP packets maybe delivered twice (once for audio and once for video).
    // As a work around, we're removing duplicates in the parser.
    return;
  }
  std::unique_ptr<RtcEventLogEncoder> encoder = CreateEncoder();

  std::vector<std::unique_ptr<RtcEventRtcpPacketIncoming>> events(event_count_);
  for (size_t i = 0; i < event_count_; ++i) {
    events[i] = (i == 0 || !force_repeated_fields_)
                    ? gen_.NewRtcpPacketIncoming()
                    : events[0]->Copy();
    history_.push_back(events[i]->Copy());
  }

  encoded_ += encoder->EncodeBatch(history_.begin(), history_.end());
  ASSERT_TRUE(parsed_log_.ParseString(encoded_).ok());

  const auto& incoming_rtcp_packets = parsed_log_.incoming_rtcp_packets();
  ASSERT_EQ(incoming_rtcp_packets.size(), event_count_);

  for (size_t i = 0; i < event_count_; ++i) {
    verifier_.VerifyLoggedRtcpPacketIncoming(*events[i],
                                             incoming_rtcp_packets[i]);
  }
}

TEST_P(RtcEventLogEncoderTest, RtcEventRtcpPacketOutgoing) {
  std::unique_ptr<RtcEventLogEncoder> encoder = CreateEncoder();
  std::vector<std::unique_ptr<RtcEventRtcpPacketOutgoing>> events(event_count_);
  for (size_t i = 0; i < event_count_; ++i) {
    events[i] = (i == 0 || !force_repeated_fields_)
                    ? gen_.NewRtcpPacketOutgoing()
                    : events[0]->Copy();
    history_.push_back(events[i]->Copy());
  }

  encoded_ += encoder->EncodeBatch(history_.begin(), history_.end());
  ASSERT_TRUE(parsed_log_.ParseString(encoded_).ok());

  const auto& outgoing_rtcp_packets = parsed_log_.outgoing_rtcp_packets();
  ASSERT_EQ(outgoing_rtcp_packets.size(), event_count_);

  for (size_t i = 0; i < event_count_; ++i) {
    verifier_.VerifyLoggedRtcpPacketOutgoing(*events[i],
                                             outgoing_rtcp_packets[i]);
  }
}

TEST_P(RtcEventLogEncoderTest, RtcEventRtcpReceiverReport) {
  if (force_repeated_fields_) {
    return;
  }

  rtc::ScopedFakeClock fake_clock;
  fake_clock.SetTime(Timestamp::Millis(prng_.Rand<uint32_t>()));

  std::unique_ptr<RtcEventLogEncoder> encoder = CreateEncoder();

  for (auto direction : {kIncomingPacket, kOutgoingPacket}) {
    std::vector<rtcp::ReceiverReport> events(event_count_);
    std::vector<int64_t> timestamps_ms(event_count_);
    for (size_t i = 0; i < event_count_; ++i) {
      timestamps_ms[i] = rtc::TimeMillis();
      events[i] = gen_.NewReceiverReport();
      rtc::Buffer buffer = events[i].Build();
      if (direction == kIncomingPacket) {
        history_.push_back(
            std::make_unique<RtcEventRtcpPacketIncoming>(buffer));
      } else {
        history_.push_back(
            std::make_unique<RtcEventRtcpPacketOutgoing>(buffer));
      }
      fake_clock.AdvanceTime(TimeDelta::Millis(prng_.Rand(0, 1000)));
    }

    encoded_ += encoder->EncodeBatch(history_.begin(), history_.end());
    ASSERT_TRUE(parsed_log_.ParseString(encoded_).ok());

    const auto& receiver_reports = parsed_log_.receiver_reports(direction);
    ASSERT_EQ(receiver_reports.size(), event_count_);

    for (size_t i = 0; i < event_count_; ++i) {
      verifier_.VerifyLoggedReceiverReport(timestamps_ms[i], events[i],
                                           receiver_reports[i]);
    }
  }
}

TEST_P(RtcEventLogEncoderTest, RtcEventRtcpSenderReport) {
  if (force_repeated_fields_) {
    return;
  }

  rtc::ScopedFakeClock fake_clock;
  fake_clock.SetTime(Timestamp::Millis(prng_.Rand<uint32_t>()));

  std::unique_ptr<RtcEventLogEncoder> encoder = CreateEncoder();

  for (auto direction : {kIncomingPacket, kOutgoingPacket}) {
    std::vector<rtcp::SenderReport> events(event_count_);
    std::vector<int64_t> timestamps_ms(event_count_);
    for (size_t i = 0; i < event_count_; ++i) {
      timestamps_ms[i] = rtc::TimeMillis();
      events[i] = gen_.NewSenderReport();
      rtc::Buffer buffer = events[i].Build();
      if (direction == kIncomingPacket) {
        history_.push_back(
            std::make_unique<RtcEventRtcpPacketIncoming>(buffer));
      } else {
        history_.push_back(
            std::make_unique<RtcEventRtcpPacketOutgoing>(buffer));
      }
      fake_clock.AdvanceTime(TimeDelta::Millis(prng_.Rand(0, 1000)));
    }

    encoded_ += encoder->EncodeBatch(history_.begin(), history_.end());
    ASSERT_TRUE(parsed_log_.ParseString(encoded_).ok());

    const auto& sender_reports = parsed_log_.sender_reports(direction);
    ASSERT_EQ(sender_reports.size(), event_count_);

    for (size_t i = 0; i < event_count_; ++i) {
      verifier_.VerifyLoggedSenderReport(timestamps_ms[i], events[i],
                                         sender_reports[i]);
    }
  }
}

TEST_P(RtcEventLogEncoderTest, RtcEventRtcpExtendedReports) {
  if (force_repeated_fields_) {
    return;
  }

  rtc::ScopedFakeClock fake_clock;
  fake_clock.SetTime(Timestamp::Millis(prng_.Rand<uint32_t>()));

  std::unique_ptr<RtcEventLogEncoder> encoder = CreateEncoder();

  for (auto direction : {kIncomingPacket, kOutgoingPacket}) {
    std::vector<rtcp::ExtendedReports> events(event_count_);
    std::vector<int64_t> timestamps_ms(event_count_);
    for (size_t i = 0; i < event_count_; ++i) {
      timestamps_ms[i] = rtc::TimeMillis();
      events[i] = gen_.NewExtendedReports();
      rtc::Buffer buffer = events[i].Build();
      if (direction == kIncomingPacket) {
        history_.push_back(
            std::make_unique<RtcEventRtcpPacketIncoming>(buffer));
      } else {
        history_.push_back(
            std::make_unique<RtcEventRtcpPacketOutgoing>(buffer));
      }
      fake_clock.AdvanceTime(TimeDelta::Millis(prng_.Rand(0, 1000)));
    }

    encoded_ += encoder->EncodeBatch(history_.begin(), history_.end());
    ASSERT_TRUE(parsed_log_.ParseString(encoded_).ok());

    const auto& extended_reports = parsed_log_.extended_reports(direction);
    ASSERT_EQ(extended_reports.size(), event_count_);

    for (size_t i = 0; i < event_count_; ++i) {
      verifier_.VerifyLoggedExtendedReports(timestamps_ms[i], events[i],
                                            extended_reports[i]);
    }
  }
}

TEST_P(RtcEventLogEncoderTest, RtcEventRtcpFir) {
  if (force_repeated_fields_) {
    return;
  }

  rtc::ScopedFakeClock fake_clock;
  fake_clock.SetTime(Timestamp::Millis(prng_.Rand<uint32_t>()));

  std::unique_ptr<RtcEventLogEncoder> encoder = CreateEncoder();

  for (auto direction : {kIncomingPacket, kOutgoingPacket}) {
    std::vector<rtcp::Fir> events(event_count_);
    std::vector<int64_t> timestamps_ms(event_count_);
    for (size_t i = 0; i < event_count_; ++i) {
      timestamps_ms[i] = rtc::TimeMillis();
      events[i] = gen_.NewFir();
      rtc::Buffer buffer = events[i].Build();
      if (direction == kIncomingPacket) {
        history_.push_back(
            std::make_unique<RtcEventRtcpPacketIncoming>(buffer));
      } else {
        history_.push_back(
            std::make_unique<RtcEventRtcpPacketOutgoing>(buffer));
      }
      fake_clock.AdvanceTime(TimeDelta::Millis(prng_.Rand(0, 1000)));
    }

    encoded_ += encoder->EncodeBatch(history_.begin(), history_.end());
    ASSERT_TRUE(parsed_log_.ParseString(encoded_).ok());

    const auto& firs = parsed_log_.firs(direction);
    ASSERT_EQ(firs.size(), event_count_);

    for (size_t i = 0; i < event_count_; ++i) {
      verifier_.VerifyLoggedFir(timestamps_ms[i], events[i], firs[i]);
    }
  }
}

TEST_P(RtcEventLogEncoderTest, RtcEventRtcpPli) {
  if (force_repeated_fields_) {
    return;
  }

  rtc::ScopedFakeClock fake_clock;
  fake_clock.SetTime(Timestamp::Millis(prng_.Rand<uint32_t>()));

  std::unique_ptr<RtcEventLogEncoder> encoder = CreateEncoder();

  for (auto direction : {kIncomingPacket, kOutgoingPacket}) {
    std::vector<rtcp::Pli> events(event_count_);
    std::vector<int64_t> timestamps_ms(event_count_);
    for (size_t i = 0; i < event_count_; ++i) {
      timestamps_ms[i] = rtc::TimeMillis();
      events[i] = gen_.NewPli();
      rtc::Buffer buffer = events[i].Build();
      if (direction == kIncomingPacket) {
        history_.push_back(
            std::make_unique<RtcEventRtcpPacketIncoming>(buffer));
      } else {
        history_.push_back(
            std::make_unique<RtcEventRtcpPacketOutgoing>(buffer));
      }
      fake_clock.AdvanceTime(TimeDelta::Millis(prng_.Rand(0, 1000)));
    }

    encoded_ += encoder->EncodeBatch(history_.begin(), history_.end());
    ASSERT_TRUE(parsed_log_.ParseString(encoded_).ok());

    const auto& plis = parsed_log_.plis(direction);
    ASSERT_EQ(plis.size(), event_count_);

    for (size_t i = 0; i < event_count_; ++i) {
      verifier_.VerifyLoggedPli(timestamps_ms[i], events[i], plis[i]);
    }
  }
}

TEST_P(RtcEventLogEncoderTest, RtcEventRtcpBye) {
  if (force_repeated_fields_) {
    return;
  }

  rtc::ScopedFakeClock fake_clock;
  fake_clock.SetTime(Timestamp::Millis(prng_.Rand<uint32_t>()));

  std::unique_ptr<RtcEventLogEncoder> encoder = CreateEncoder();

  for (auto direction : {kIncomingPacket, kOutgoingPacket}) {
    std::vector<rtcp::Bye> events(event_count_);
    std::vector<int64_t> timestamps_ms(event_count_);
    for (size_t i = 0; i < event_count_; ++i) {
      timestamps_ms[i] = rtc::TimeMillis();
      events[i] = gen_.NewBye();
      rtc::Buffer buffer = events[i].Build();
      if (direction == kIncomingPacket) {
        history_.push_back(
            std::make_unique<RtcEventRtcpPacketIncoming>(buffer));
      } else {
        history_.push_back(
            std::make_unique<RtcEventRtcpPacketOutgoing>(buffer));
      }
      fake_clock.AdvanceTime(TimeDelta::Millis(prng_.Rand(0, 1000)));
    }

    encoded_ += encoder->EncodeBatch(history_.begin(), history_.end());
    ASSERT_TRUE(parsed_log_.ParseString(encoded_).ok());

    const auto& byes = parsed_log_.byes(direction);
    ASSERT_EQ(byes.size(), event_count_);

    for (size_t i = 0; i < event_count_; ++i) {
      verifier_.VerifyLoggedBye(timestamps_ms[i], events[i], byes[i]);
    }
  }
}

TEST_P(RtcEventLogEncoderTest, RtcEventRtcpNack) {
  if (force_repeated_fields_) {
    return;
  }

  rtc::ScopedFakeClock fake_clock;
  fake_clock.SetTime(Timestamp::Millis(prng_.Rand<uint32_t>()));

  std::unique_ptr<RtcEventLogEncoder> encoder = CreateEncoder();

  for (auto direction : {kIncomingPacket, kOutgoingPacket}) {
    std::vector<rtcp::Nack> events(event_count_);
    std::vector<int64_t> timestamps_ms(event_count_);
    for (size_t i = 0; i < event_count_; ++i) {
      timestamps_ms[i] = rtc::TimeMillis();
      events[i] = gen_.NewNack();
      rtc::Buffer buffer = events[i].Build();
      if (direction == kIncomingPacket) {
        history_.push_back(
            std::make_unique<RtcEventRtcpPacketIncoming>(buffer));
      } else {
        history_.push_back(
            std::make_unique<RtcEventRtcpPacketOutgoing>(buffer));
      }
      fake_clock.AdvanceTime(TimeDelta::Millis(prng_.Rand(0, 1000)));
    }

    encoded_ += encoder->EncodeBatch(history_.begin(), history_.end());
    ASSERT_TRUE(parsed_log_.ParseString(encoded_).ok());

    const auto& nacks = parsed_log_.nacks(direction);
    ASSERT_EQ(nacks.size(), event_count_);

    for (size_t i = 0; i < event_count_; ++i) {
      verifier_.VerifyLoggedNack(timestamps_ms[i], events[i], nacks[i]);
    }
  }
}

TEST_P(RtcEventLogEncoderTest, RtcEventRtcpRemb) {
  if (force_repeated_fields_) {
    return;
  }

  rtc::ScopedFakeClock fake_clock;
  fake_clock.SetTime(Timestamp::Millis(prng_.Rand<uint32_t>()));

  std::unique_ptr<RtcEventLogEncoder> encoder = CreateEncoder();

  for (auto direction : {kIncomingPacket, kOutgoingPacket}) {
    std::vector<rtcp::Remb> events(event_count_);
    std::vector<int64_t> timestamps_ms(event_count_);
    for (size_t i = 0; i < event_count_; ++i) {
      timestamps_ms[i] = rtc::TimeMillis();
      events[i] = gen_.NewRemb();
      rtc::Buffer buffer = events[i].Build();
      if (direction == kIncomingPacket) {
        history_.push_back(
            std::make_unique<RtcEventRtcpPacketIncoming>(buffer));
      } else {
        history_.push_back(
            std::make_unique<RtcEventRtcpPacketOutgoing>(buffer));
      }
      fake_clock.AdvanceTime(TimeDelta::Millis(prng_.Rand(0, 1000)));
    }

    encoded_ += encoder->EncodeBatch(history_.begin(), history_.end());
    ASSERT_TRUE(parsed_log_.ParseString(encoded_).ok());

    const auto& rembs = parsed_log_.rembs(direction);
    ASSERT_EQ(rembs.size(), event_count_);

    for (size_t i = 0; i < event_count_; ++i) {
      verifier_.VerifyLoggedRemb(timestamps_ms[i], events[i], rembs[i]);
    }
  }
}

TEST_P(RtcEventLogEncoderTest, RtcEventRtcpTransportFeedback) {
  if (force_repeated_fields_) {
    return;
  }

  rtc::ScopedFakeClock fake_clock;
  fake_clock.SetTime(Timestamp::Millis(prng_.Rand<uint32_t>()));

  std::unique_ptr<RtcEventLogEncoder> encoder = CreateEncoder();

  for (auto direction : {kIncomingPacket, kOutgoingPacket}) {
    std::vector<rtcp::TransportFeedback> events;
    events.reserve(event_count_);
    std::vector<int64_t> timestamps_ms(event_count_);
    for (size_t i = 0; i < event_count_; ++i) {
      timestamps_ms[i] = rtc::TimeMillis();
      events.emplace_back(gen_.NewTransportFeedback());
      rtc::Buffer buffer = events[i].Build();
      if (direction == kIncomingPacket) {
        history_.push_back(
            std::make_unique<RtcEventRtcpPacketIncoming>(buffer));
      } else {
        history_.push_back(
            std::make_unique<RtcEventRtcpPacketOutgoing>(buffer));
      }
      fake_clock.AdvanceTime(TimeDelta::Millis(prng_.Rand(0, 1000)));
    }

    encoded_ += encoder->EncodeBatch(history_.begin(), history_.end());
    ASSERT_TRUE(parsed_log_.ParseString(encoded_).ok());

    const auto& transport_feedbacks =
        parsed_log_.transport_feedbacks(direction);
    ASSERT_EQ(transport_feedbacks.size(), event_count_);

    for (size_t i = 0; i < event_count_; ++i) {
      verifier_.VerifyLoggedTransportFeedback(timestamps_ms[i], events[i],
                                              transport_feedbacks[i]);
    }
  }
}

TEST_P(RtcEventLogEncoderTest, RtcEventRtcpLossNotification) {
  if (force_repeated_fields_) {
    return;
  }

  rtc::ScopedFakeClock fake_clock;
  fake_clock.SetTime(Timestamp::Millis(prng_.Rand<uint32_t>()));

  std::unique_ptr<RtcEventLogEncoder> encoder = CreateEncoder();

  for (auto direction : {kIncomingPacket, kOutgoingPacket}) {
    std::vector<rtcp::LossNotification> events;
    events.reserve(event_count_);
    std::vector<int64_t> timestamps_ms(event_count_);
    for (size_t i = 0; i < event_count_; ++i) {
      timestamps_ms[i] = rtc::TimeMillis();
      events.emplace_back(gen_.NewLossNotification());
      rtc::Buffer buffer = events[i].Build();
      if (direction == kIncomingPacket) {
        history_.push_back(
            std::make_unique<RtcEventRtcpPacketIncoming>(buffer));
      } else {
        history_.push_back(
            std::make_unique<RtcEventRtcpPacketOutgoing>(buffer));
      }
      fake_clock.AdvanceTime(TimeDelta::Millis(prng_.Rand(0, 1000)));
    }

    encoded_ += encoder->EncodeBatch(history_.begin(), history_.end());
    ASSERT_TRUE(parsed_log_.ParseString(encoded_).ok());

    const auto& loss_notifications = parsed_log_.loss_notifications(direction);
    ASSERT_EQ(loss_notifications.size(), event_count_);

    for (size_t i = 0; i < event_count_; ++i) {
      verifier_.VerifyLoggedLossNotification(timestamps_ms[i], events[i],
                                             loss_notifications[i]);
    }
  }
}

TEST_P(RtcEventLogEncoderTest, RtcEventRtpPacketIncoming) {
  std::unique_ptr<RtcEventLogEncoder> encoder = CreateEncoder();
  TestRtpPackets<RtcEventRtpPacketIncoming, LoggedRtpPacketIncoming>(*encoder);
}

TEST_P(RtcEventLogEncoderTest, RtcEventRtpPacketOutgoing) {
  std::unique_ptr<RtcEventLogEncoder> encoder = CreateEncoder();
  TestRtpPackets<RtcEventRtpPacketOutgoing, LoggedRtpPacketOutgoing>(*encoder);
}

TEST_P(RtcEventLogEncoderTest,
       RtcEventRtpPacketIncomingNoDependencyDescriptor) {
  ExplicitKeyValueConfig no_dd(
      "WebRTC-RtcEventLogEncodeDependencyDescriptor/Disabled/");
  std::unique_ptr<RtcEventLogEncoder> encoder = CreateEncoder(no_dd);
  verifier_.ExpectDependencyDescriptorExtensionIsSet(false);
  TestRtpPackets<RtcEventRtpPacketIncoming, LoggedRtpPacketIncoming>(*encoder);
}

TEST_P(RtcEventLogEncoderTest,
       RtcEventRtpPacketOutgoingNoDependencyDescriptor) {
  ExplicitKeyValueConfig no_dd(
      "WebRTC-RtcEventLogEncodeDependencyDescriptor/Disabled/");
  std::unique_ptr<RtcEventLogEncoder> encoder = CreateEncoder(no_dd);
  verifier_.ExpectDependencyDescriptorExtensionIsSet(false);
  TestRtpPackets<RtcEventRtpPacketOutgoing, LoggedRtpPacketOutgoing>(*encoder);
}

// TODO(eladalon/terelius): Test with multiple events in the batch.
TEST_P(RtcEventLogEncoderTest, RtcEventVideoReceiveStreamConfig) {
  std::unique_ptr<RtcEventLogEncoder> encoder = CreateEncoder();
  uint32_t ssrc = prng_.Rand<uint32_t>();
  RtpHeaderExtensionMap extensions = gen_.NewRtpHeaderExtensionMap();
  std::unique_ptr<RtcEventVideoReceiveStreamConfig> event =
      gen_.NewVideoReceiveStreamConfig(ssrc, extensions);
  history_.push_back(event->Copy());

  encoded_ += encoder->EncodeBatch(history_.begin(), history_.end());
  ASSERT_TRUE(parsed_log_.ParseString(encoded_).ok());
  const auto& video_recv_configs = parsed_log_.video_recv_configs();

  ASSERT_EQ(video_recv_configs.size(), 1u);
  verifier_.VerifyLoggedVideoRecvConfig(*event, video_recv_configs[0]);
}

// TODO(eladalon/terelius): Test with multiple events in the batch.
TEST_P(RtcEventLogEncoderTest, RtcEventVideoSendStreamConfig) {
  std::unique_ptr<RtcEventLogEncoder> encoder = CreateEncoder();
  uint32_t ssrc = prng_.Rand<uint32_t>();
  RtpHeaderExtensionMap extensions = gen_.NewRtpHeaderExtensionMap();
  std::unique_ptr<RtcEventVideoSendStreamConfig> event =
      gen_.NewVideoSendStreamConfig(ssrc, extensions);
  history_.push_back(event->Copy());

  encoded_ += encoder->EncodeBatch(history_.begin(), history_.end());
  ASSERT_TRUE(parsed_log_.ParseString(encoded_).ok());
  const auto& video_send_configs = parsed_log_.video_send_configs();

  ASSERT_EQ(video_send_configs.size(), 1u);
  verifier_.VerifyLoggedVideoSendConfig(*event, video_send_configs[0]);
}

INSTANTIATE_TEST_SUITE_P(
    RandomSeeds,
    RtcEventLogEncoderTest,
    ::testing::Combine(/* Random seed*: */ ::testing::Values(1, 2, 3, 4, 5),
                       /* Encoding: */
                       ::testing::Values(RtcEventLog::EncodingType::Legacy,
                                         RtcEventLog::EncodingType::NewFormat),
                       /* Event count: */ ::testing::Values(1, 2, 10, 100),
                       /* Repeated fields: */ ::testing::Bool()));

class RtcEventLogEncoderSimpleTest
    : public ::testing::TestWithParam<RtcEventLog::EncodingType> {
 protected:
  RtcEventLogEncoderSimpleTest() : encoding_type_(GetParam()) {
    switch (encoding_type_) {
      case RtcEventLog::EncodingType::Legacy:
        encoder_ = std::make_unique<RtcEventLogEncoderLegacy>();
        break;
      case RtcEventLog::EncodingType::NewFormat:
        encoder_ = std::make_unique<RtcEventLogEncoderNewFormat>(
            ExplicitKeyValueConfig(""));
        break;
      case RtcEventLog::EncodingType::ProtoFree:
        encoder_ = std::make_unique<RtcEventLogEncoderV3>();
        break;
    }
    encoded_ =
        encoder_->EncodeLogStart(rtc::TimeMillis(), rtc::TimeUTCMillis());
  }
  ~RtcEventLogEncoderSimpleTest() override = default;

  std::deque<std::unique_ptr<RtcEvent>> history_;
  std::unique_ptr<RtcEventLogEncoder> encoder_;
  ParsedRtcEventLog parsed_log_;
  const RtcEventLog::EncodingType encoding_type_;
  std::string encoded_;
};

TEST_P(RtcEventLogEncoderSimpleTest, RtcEventLargeCompoundRtcpPacketIncoming) {
  // Create a compound packet containing multiple Bye messages.
  rtc::Buffer packet;
  size_t index = 0;
  for (int i = 0; i < 8; i++) {
    rtcp::Bye bye;
    std::string reason(255, 'a');  // Add some arbitrary data.
    bye.SetReason(reason);
    bye.SetSenderSsrc(0x12345678);
    packet.SetSize(packet.size() + bye.BlockLength());
    bool created =
        bye.Create(packet.data(), &index, packet.capacity(), nullptr);
    ASSERT_TRUE(created);
    ASSERT_EQ(index, packet.size());
  }

  EXPECT_GT(packet.size(), static_cast<size_t>(IP_PACKET_SIZE));
  auto event = std::make_unique<RtcEventRtcpPacketIncoming>(packet);
  history_.push_back(event->Copy());
  encoded_ += encoder_->EncodeBatch(history_.begin(), history_.end());

  ParsedRtcEventLog::ParseStatus status = parsed_log_.ParseString(encoded_);
  ASSERT_TRUE(status.ok()) << status.message();

  const auto& incoming_rtcp_packets = parsed_log_.incoming_rtcp_packets();
  ASSERT_EQ(incoming_rtcp_packets.size(), 1u);
  ASSERT_EQ(incoming_rtcp_packets[0].rtcp.raw_data.size(), packet.size());
  EXPECT_EQ(memcmp(incoming_rtcp_packets[0].rtcp.raw_data.data(), packet.data(),
                   packet.size()),
            0);
}

INSTANTIATE_TEST_SUITE_P(
    LargeCompoundRtcp,
    RtcEventLogEncoderSimpleTest,
    ::testing::Values(RtcEventLog::EncodingType::Legacy,
                      RtcEventLog::EncodingType::NewFormat));

}  // namespace webrtc
