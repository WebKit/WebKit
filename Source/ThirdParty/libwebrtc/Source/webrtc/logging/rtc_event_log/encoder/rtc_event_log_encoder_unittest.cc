/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <cmath>
#include <limits>
#include <memory>
#include <string>
#include <utility>

#include "api/rtpparameters.h"  // RtpExtension
#include "logging/rtc_event_log/encoder/rtc_event_log_encoder_legacy.h"
#include "logging/rtc_event_log/events/rtc_event_audio_network_adaptation.h"
#include "logging/rtc_event_log/events/rtc_event_audio_playout.h"
#include "logging/rtc_event_log/events/rtc_event_audio_receive_stream_config.h"
#include "logging/rtc_event_log/events/rtc_event_audio_send_stream_config.h"
#include "logging/rtc_event_log/events/rtc_event_bwe_update_delay_based.h"
#include "logging/rtc_event_log/events/rtc_event_bwe_update_loss_based.h"
#include "logging/rtc_event_log/events/rtc_event_logging_started.h"
#include "logging/rtc_event_log/events/rtc_event_logging_stopped.h"
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
#include "modules/audio_coding/audio_network_adaptor/include/audio_network_adaptor_config.h"
#include "modules/remote_bitrate_estimator/include/bwe_defines.h"
#include "modules/rtp_rtcp/source/rtcp_packet/bye.h"  // Arbitrary RTCP message.
#include "modules/rtp_rtcp/source/rtp_header_extensions.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "modules/rtp_rtcp/source/rtp_packet_to_send.h"
#include "rtc_base/arraysize.h"
#include "rtc_base/numerics/safe_conversions.h"
#include "rtc_base/ptr_util.h"
#include "rtc_base/random.h"
#include "test/gtest.h"

namespace webrtc {

namespace {
struct ExtensionInfo {
  RTPExtensionType type;
  const char* uri;
};

template <typename Extension>
constexpr ExtensionInfo CreateExtensionInfo() {
  return {Extension::kId, Extension::kUri};
}

constexpr ExtensionInfo kExtensions[] = {
    CreateExtensionInfo<TransmissionOffset>(),
    CreateExtensionInfo<AudioLevel>(),
    CreateExtensionInfo<AbsoluteSendTime>(),
    CreateExtensionInfo<VideoOrientation>(),
    CreateExtensionInfo<TransportSequenceNumber>(),
    CreateExtensionInfo<PlayoutDelayLimits>(),
    CreateExtensionInfo<VideoContentTypeExtension>(),
    CreateExtensionInfo<VideoTimingExtension>(),
    CreateExtensionInfo<RtpStreamId>(),
    CreateExtensionInfo<RepairedRtpStreamId>(),
    CreateExtensionInfo<RtpMid>(),
};
}  // namespace

class RtcEventLogEncoderTest : public testing::TestWithParam<int> {
 protected:
  RtcEventLogEncoderTest()
      : encoder_(new RtcEventLogEncoderLegacy), prng_(GetParam()) {}
  ~RtcEventLogEncoderTest() override = default;

  // ANA events have some optional fields, so we want to make sure that we get
  // correct behavior both when all of the values are there, as well as when
  // only some.
  void TestRtcEventAudioNetworkAdaptation(
      std::unique_ptr<AudioEncoderRuntimeConfig> runtime_config);

  // These help prevent code duplication between incoming/outgoing variants.
  void TestRtcEventRtcpPacket(PacketDirection direction);
  void TestRtcEventRtpPacket(PacketDirection direction);

  int RandomInt() {
    // Don't run this on a SNES.
    static_assert(8 * sizeof(int) >= 32, "Don't run this on a SNES.");
    int32_t rand = prng_.Rand(0, std::numeric_limits<int32_t>::max());
    return rtc::saturated_cast<int>(rand);
  }

  int RandomPositiveInt() {
    int32_t rand = prng_.Rand(1, std::numeric_limits<int32_t>::max());
    return rtc::saturated_cast<int>(rand);
  }

  uint32_t RandomSsrc() {
    return prng_.Rand(std::numeric_limits<uint32_t>::max());
  }

  std::vector<RtpExtension> RandomRtpExtensions() {
    RTC_DCHECK(arraysize(kExtensions) >= 2);
    size_t id_1 = prng_.Rand(0u, arraysize(kExtensions) - 1);
    size_t id_2 = prng_.Rand(0u, arraysize(kExtensions) - 2);
    if (id_2 == id_1)
      id_2 = arraysize(kExtensions) - 1;
    return std::vector<RtpExtension>{
        RtpExtension(kExtensions[id_1].uri, kExtensions[id_1].type),
        RtpExtension(kExtensions[id_2].uri, kExtensions[id_2].type)};
  }

  int RandomBitrate() { return RandomInt(); }

  // TODO(eladalon): Once we have more than once possible encoder, parameterize
  // encoder selection.
  std::unique_ptr<RtcEventLogEncoder> encoder_;
  ParsedRtcEventLog parsed_log_;
  Random prng_;
};

void RtcEventLogEncoderTest::TestRtcEventAudioNetworkAdaptation(
    std::unique_ptr<AudioEncoderRuntimeConfig> runtime_config) {
  auto original_runtime_config = *runtime_config;
  auto event = rtc::MakeUnique<RtcEventAudioNetworkAdaptation>(
      std::move(runtime_config));
  const int64_t timestamp_us = event->timestamp_us_;

  ASSERT_TRUE(parsed_log_.ParseString(encoder_->Encode(*event)));
  ASSERT_EQ(parsed_log_.GetNumberOfEvents(), 1u);
  ASSERT_EQ(parsed_log_.GetEventType(0),
            ParsedRtcEventLog::AUDIO_NETWORK_ADAPTATION_EVENT);

  AudioEncoderRuntimeConfig parsed_runtime_config;
  parsed_log_.GetAudioNetworkAdaptation(0, &parsed_runtime_config);

  EXPECT_EQ(parsed_log_.GetTimestamp(0), timestamp_us);
  EXPECT_EQ(parsed_runtime_config, original_runtime_config);
}

TEST_P(RtcEventLogEncoderTest, RtcEventAudioNetworkAdaptationBitrate) {
  auto runtime_config = rtc::MakeUnique<AudioEncoderRuntimeConfig>();
  const int bitrate_bps = RandomBitrate();
  runtime_config->bitrate_bps = bitrate_bps;
  TestRtcEventAudioNetworkAdaptation(std::move(runtime_config));
}

TEST_P(RtcEventLogEncoderTest, RtcEventAudioNetworkAdaptationFrameLength) {
  auto runtime_config = rtc::MakeUnique<AudioEncoderRuntimeConfig>();
  const int frame_length_ms = prng_.Rand(1, 1000);
  runtime_config->frame_length_ms = frame_length_ms;
  TestRtcEventAudioNetworkAdaptation(std::move(runtime_config));
}

TEST_P(RtcEventLogEncoderTest, RtcEventAudioNetworkAdaptationPacketLoss) {
  // To simplify the test, we just check powers of two.
  const float plr = std::pow(0.5f, prng_.Rand(1, 8));
  auto runtime_config = rtc::MakeUnique<AudioEncoderRuntimeConfig>();
  runtime_config->uplink_packet_loss_fraction = plr;
  TestRtcEventAudioNetworkAdaptation(std::move(runtime_config));
}

TEST_P(RtcEventLogEncoderTest, RtcEventAudioNetworkAdaptationFec) {
  // The test might be trivially passing for one of the two boolean values, so
  // for safety's sake, we test both.
  for (bool fec_enabled : {false, true}) {
    auto runtime_config = rtc::MakeUnique<AudioEncoderRuntimeConfig>();
    runtime_config->enable_fec = fec_enabled;
    TestRtcEventAudioNetworkAdaptation(std::move(runtime_config));
  }
}

TEST_P(RtcEventLogEncoderTest, RtcEventAudioNetworkAdaptationDtx) {
  // The test might be trivially passing for one of the two boolean values, so
  // for safety's sake, we test both.
  for (bool dtx_enabled : {false, true}) {
    auto runtime_config = rtc::MakeUnique<AudioEncoderRuntimeConfig>();
    runtime_config->enable_dtx = dtx_enabled;
    TestRtcEventAudioNetworkAdaptation(std::move(runtime_config));
  }
}

TEST_P(RtcEventLogEncoderTest, RtcEventAudioNetworkAdaptationChannels) {
  // The test might be trivially passing for one of the two possible values, so
  // for safety's sake, we test both.
  for (size_t channels : {1, 2}) {
    auto runtime_config = rtc::MakeUnique<AudioEncoderRuntimeConfig>();
    runtime_config->num_channels = channels;
    TestRtcEventAudioNetworkAdaptation(std::move(runtime_config));
  }
}

TEST_P(RtcEventLogEncoderTest, RtcEventAudioNetworkAdaptationAll) {
  const int bitrate_bps = RandomBitrate();
  const int frame_length_ms = prng_.Rand(1, 1000);
  const float plr = std::pow(0.5f, prng_.Rand(1, 8));
  for (bool fec_enabled : {false, true}) {
    for (bool dtx_enabled : {false, true}) {
      for (size_t channels : {1, 2}) {
        auto runtime_config = rtc::MakeUnique<AudioEncoderRuntimeConfig>();
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
  const uint32_t ssrc = RandomSsrc();
  auto event = rtc::MakeUnique<RtcEventAudioPlayout>(ssrc);
  const int64_t timestamp_us = event->timestamp_us_;

  ASSERT_TRUE(parsed_log_.ParseString(encoder_->Encode(*event)));
  ASSERT_EQ(parsed_log_.GetNumberOfEvents(), 1u);
  ASSERT_EQ(parsed_log_.GetEventType(0),
            ParsedRtcEventLog::AUDIO_PLAYOUT_EVENT);

  uint32_t parsed_ssrc;
  parsed_log_.GetAudioPlayout(0, &parsed_ssrc);

  EXPECT_EQ(parsed_log_.GetTimestamp(0), timestamp_us);
  EXPECT_EQ(parsed_ssrc, ssrc);
}

TEST_P(RtcEventLogEncoderTest, RtcEventAudioReceiveStreamConfig) {
  auto stream_config = rtc::MakeUnique<rtclog::StreamConfig>();
  stream_config->local_ssrc = RandomSsrc();
  stream_config->remote_ssrc = RandomSsrc();
  // TODO(eladalon): Verify that the extensions are used correctly when
  // parsing RTP packets headers. Here and elsewhere.
  std::vector<RtpExtension> extensions = RandomRtpExtensions();
  for (const auto& extension : extensions)
    stream_config->rtp_extensions.push_back(extension);

  auto original_stream_config = *stream_config;

  auto event = rtc::MakeUnique<RtcEventAudioReceiveStreamConfig>(
      std::move(stream_config));
  const int64_t timestamp_us = event->timestamp_us_;

  ASSERT_TRUE(parsed_log_.ParseString(encoder_->Encode(*event)));
  ASSERT_EQ(parsed_log_.GetNumberOfEvents(), 1u);
  ASSERT_EQ(parsed_log_.GetEventType(0),
            ParsedRtcEventLog::AUDIO_RECEIVER_CONFIG_EVENT);

  auto parsed_event = parsed_log_.GetAudioReceiveConfig(0);
  EXPECT_EQ(parsed_log_.GetTimestamp(0), timestamp_us);
  EXPECT_EQ(parsed_event, original_stream_config);
}

TEST_P(RtcEventLogEncoderTest, RtcEventAudioSendStreamConfig) {
  auto stream_config = rtc::MakeUnique<rtclog::StreamConfig>();
  stream_config->local_ssrc = RandomSsrc();
  std::vector<RtpExtension> extensions = RandomRtpExtensions();
  for (const auto& extension : extensions)
    stream_config->rtp_extensions.push_back(extension);

  auto original_stream_config = *stream_config;

  auto event =
      rtc::MakeUnique<RtcEventAudioSendStreamConfig>(std::move(stream_config));
  const int64_t timestamp_us = event->timestamp_us_;

  ASSERT_TRUE(parsed_log_.ParseString(encoder_->Encode(*event)));
  ASSERT_EQ(parsed_log_.GetNumberOfEvents(), 1u);
  ASSERT_EQ(parsed_log_.GetEventType(0),
            ParsedRtcEventLog::AUDIO_SENDER_CONFIG_EVENT);

  auto parsed_event = parsed_log_.GetAudioSendConfig(0);
  EXPECT_EQ(parsed_log_.GetTimestamp(0), timestamp_us);
  EXPECT_EQ(parsed_event, original_stream_config);
}

TEST_P(RtcEventLogEncoderTest, RtcEventBweUpdateDelayBased) {
  const int32_t bitrate_bps = RandomBitrate();
  const BandwidthUsage detector_state = static_cast<BandwidthUsage>(
      prng_.Rand(0, static_cast<int32_t>(BandwidthUsage::kLast) - 1));
  auto event =
      rtc::MakeUnique<RtcEventBweUpdateDelayBased>(bitrate_bps, detector_state);
  const int64_t timestamp_us = event->timestamp_us_;

  ASSERT_TRUE(parsed_log_.ParseString(encoder_->Encode(*event)));
  ASSERT_EQ(parsed_log_.GetNumberOfEvents(), 1u);
  ASSERT_EQ(parsed_log_.GetEventType(0),
            ParsedRtcEventLog::DELAY_BASED_BWE_UPDATE);

  auto parsed_event = parsed_log_.GetDelayBasedBweUpdate(0);

  EXPECT_EQ(parsed_log_.GetTimestamp(0), timestamp_us);
  EXPECT_EQ(parsed_event.bitrate_bps, bitrate_bps);
  EXPECT_EQ(parsed_event.detector_state, detector_state);
}

TEST_P(RtcEventLogEncoderTest, RtcEventBweUpdateLossBased) {
  const int32_t bitrate_bps = RandomBitrate();
  const uint8_t fraction_loss = rtc::dchecked_cast<uint8_t>(
      prng_.Rand(0, std::numeric_limits<uint8_t>::max()));
  const int32_t total_packets = RandomInt();

  auto event = rtc::MakeUnique<RtcEventBweUpdateLossBased>(
      bitrate_bps, fraction_loss, total_packets);
  const int64_t timestamp_us = event->timestamp_us_;

  ASSERT_TRUE(parsed_log_.ParseString(encoder_->Encode(*event)));
  ASSERT_EQ(parsed_log_.GetNumberOfEvents(), 1u);
  ASSERT_EQ(parsed_log_.GetEventType(0),
            ParsedRtcEventLog::LOSS_BASED_BWE_UPDATE);

  int32_t parsed_bitrate_bps;
  uint8_t parsed_fraction_loss;
  int32_t parsed_total_packets;
  parsed_log_.GetLossBasedBweUpdate(
      0, &parsed_bitrate_bps, &parsed_fraction_loss, &parsed_total_packets);

  EXPECT_EQ(parsed_log_.GetTimestamp(0), timestamp_us);
  EXPECT_EQ(parsed_bitrate_bps, bitrate_bps);
  EXPECT_EQ(parsed_fraction_loss, fraction_loss);
  EXPECT_EQ(parsed_total_packets, total_packets);
}

TEST_P(RtcEventLogEncoderTest, RtcEventLoggingStarted) {
  auto event = rtc::MakeUnique<RtcEventLoggingStarted>();
  const int64_t timestamp_us = event->timestamp_us_;

  ASSERT_TRUE(parsed_log_.ParseString(encoder_->Encode(*event)));
  ASSERT_EQ(parsed_log_.GetNumberOfEvents(), 1u);
  ASSERT_EQ(parsed_log_.GetEventType(0), ParsedRtcEventLog::LOG_START);

  EXPECT_EQ(parsed_log_.GetTimestamp(0), timestamp_us);
}

TEST_P(RtcEventLogEncoderTest, RtcEventLoggingStopped) {
  auto event = rtc::MakeUnique<RtcEventLoggingStopped>();
  const int64_t timestamp_us = event->timestamp_us_;

  ASSERT_TRUE(parsed_log_.ParseString(encoder_->Encode(*event)));
  ASSERT_EQ(parsed_log_.GetNumberOfEvents(), 1u);
  ASSERT_EQ(parsed_log_.GetEventType(0), ParsedRtcEventLog::LOG_END);

  EXPECT_EQ(parsed_log_.GetTimestamp(0), timestamp_us);
}

TEST_P(RtcEventLogEncoderTest, RtcEventProbeClusterCreated) {
  const int id = RandomPositiveInt();
  const int bitrate_bps = RandomBitrate();
  const int min_probes = RandomPositiveInt();
  const int min_bytes = RandomPositiveInt();

  auto event = rtc::MakeUnique<RtcEventProbeClusterCreated>(
      id, bitrate_bps, min_probes, min_bytes);
  const int64_t timestamp_us = event->timestamp_us_;

  ASSERT_TRUE(parsed_log_.ParseString(encoder_->Encode(*event)));
  ASSERT_EQ(parsed_log_.GetNumberOfEvents(), 1u);
  ASSERT_EQ(parsed_log_.GetEventType(0),
            ParsedRtcEventLog::BWE_PROBE_CLUSTER_CREATED_EVENT);

  auto parsed_event = parsed_log_.GetBweProbeClusterCreated(0);

  EXPECT_EQ(parsed_log_.GetTimestamp(0), timestamp_us);
  EXPECT_EQ(rtc::dchecked_cast<int>(parsed_event.id), id);
  EXPECT_EQ(rtc::dchecked_cast<int>(parsed_event.bitrate_bps), bitrate_bps);
  EXPECT_EQ(rtc::dchecked_cast<int>(parsed_event.min_packets), min_probes);
  EXPECT_EQ(rtc::dchecked_cast<int>(parsed_event.min_bytes), min_bytes);
}

TEST_P(RtcEventLogEncoderTest, RtcEventProbeResultFailure) {
  const int id = RandomPositiveInt();
  const ProbeFailureReason failure_reason = static_cast<ProbeFailureReason>(
      prng_.Rand(0, static_cast<int32_t>(ProbeFailureReason::kLast) - 1));

  auto event = rtc::MakeUnique<RtcEventProbeResultFailure>(id, failure_reason);
  const int64_t timestamp_us = event->timestamp_us_;

  ASSERT_TRUE(parsed_log_.ParseString(encoder_->Encode(*event)));
  ASSERT_EQ(parsed_log_.GetNumberOfEvents(), 1u);
  ASSERT_EQ(parsed_log_.GetEventType(0),
            ParsedRtcEventLog::BWE_PROBE_RESULT_EVENT);

  auto parsed_event = parsed_log_.GetBweProbeResult(0);

  EXPECT_EQ(parsed_log_.GetTimestamp(0), timestamp_us);
  EXPECT_EQ(rtc::dchecked_cast<int>(parsed_event.id), id);
  ASSERT_FALSE(parsed_event.bitrate_bps);
  ASSERT_TRUE(parsed_event.failure_reason);
  EXPECT_EQ(parsed_event.failure_reason, failure_reason);
}

TEST_P(RtcEventLogEncoderTest, RtcEventProbeResultSuccess) {
  const int id = RandomPositiveInt();
  const int bitrate_bps = RandomBitrate();

  auto event = rtc::MakeUnique<RtcEventProbeResultSuccess>(id, bitrate_bps);
  const int64_t timestamp_us = event->timestamp_us_;

  ASSERT_TRUE(parsed_log_.ParseString(encoder_->Encode(*event)));
  ASSERT_EQ(parsed_log_.GetNumberOfEvents(), 1u);
  ASSERT_EQ(parsed_log_.GetEventType(0),
            ParsedRtcEventLog::BWE_PROBE_RESULT_EVENT);

  auto parsed_event = parsed_log_.GetBweProbeResult(0);

  EXPECT_EQ(parsed_log_.GetTimestamp(0), timestamp_us);
  EXPECT_EQ(rtc::dchecked_cast<int>(parsed_event.id), id);
  ASSERT_TRUE(parsed_event.bitrate_bps);
  EXPECT_EQ(parsed_event.bitrate_bps, bitrate_bps);
  ASSERT_FALSE(parsed_event.failure_reason);
}

void RtcEventLogEncoderTest::TestRtcEventRtcpPacket(PacketDirection direction) {
  rtcp::Bye bye_packet;  // Arbitrarily chosen RTCP packet type.
  bye_packet.SetReason("a man's reach should exceed his grasp");
  auto rtcp_packet = bye_packet.Build();

  std::unique_ptr<RtcEvent> event;
  if (direction == PacketDirection::kIncomingPacket) {
    event = rtc::MakeUnique<RtcEventRtcpPacketIncoming>(rtcp_packet);
  } else {
    event = rtc::MakeUnique<RtcEventRtcpPacketOutgoing>(rtcp_packet);
  }
  const int64_t timestamp_us = event->timestamp_us_;

  ASSERT_TRUE(parsed_log_.ParseString(encoder_->Encode(*event)));
  ASSERT_EQ(parsed_log_.GetNumberOfEvents(), 1u);
  ASSERT_EQ(parsed_log_.GetEventType(0), ParsedRtcEventLog::RTCP_EVENT);

  PacketDirection parsed_direction;
  uint8_t parsed_packet[IP_PACKET_SIZE];  // "Parsed" = after event-encoding.
  size_t parsed_packet_length;
  parsed_log_.GetRtcpPacket(0, &parsed_direction, parsed_packet,
                            &parsed_packet_length);

  EXPECT_EQ(parsed_direction, direction);
  EXPECT_EQ(parsed_log_.GetTimestamp(0), timestamp_us);
  ASSERT_EQ(parsed_packet_length, rtcp_packet.size());
  ASSERT_EQ(memcmp(parsed_packet, rtcp_packet.data(), parsed_packet_length), 0);
}

TEST_P(RtcEventLogEncoderTest, RtcEventRtcpPacketIncoming) {
  TestRtcEventRtcpPacket(PacketDirection::kIncomingPacket);
}

TEST_P(RtcEventLogEncoderTest, RtcEventRtcpPacketOutgoing) {
  TestRtcEventRtcpPacket(PacketDirection::kOutgoingPacket);
}

void RtcEventLogEncoderTest::TestRtcEventRtpPacket(PacketDirection direction) {
  const int probe_cluster_id = RandomPositiveInt();

  std::unique_ptr<RtpPacketReceived> packet_received;
  std::unique_ptr<RtpPacketToSend> packet_to_send;
  RtpPacket* packet;
  if (direction == PacketDirection::kIncomingPacket) {
    packet_received = rtc::MakeUnique<RtpPacketReceived>();
    packet = packet_received.get();
  } else {
    packet_to_send = rtc::MakeUnique<RtpPacketToSend>(nullptr);
    packet = packet_to_send.get();
  }
  packet->SetSsrc(RandomSsrc());
  packet->SetSequenceNumber(static_cast<uint16_t>(RandomInt()));
  packet->SetPayloadSize(prng_.Rand(0u, 1000u));
  // TODO(terelius): Add marker bit, capture timestamp, CSRCs, and header
  // extensions.

  std::unique_ptr<RtcEvent> event;
  if (direction == PacketDirection::kIncomingPacket) {
    event = rtc::MakeUnique<RtcEventRtpPacketIncoming>(*packet_received);
  } else {
    event = rtc::MakeUnique<RtcEventRtpPacketOutgoing>(*packet_to_send,
                                                       probe_cluster_id);
  }
  const int64_t timestamp_us = event->timestamp_us_;

  ASSERT_TRUE(parsed_log_.ParseString(encoder_->Encode(*event)));
  ASSERT_EQ(parsed_log_.GetNumberOfEvents(), 1u);
  ASSERT_EQ(parsed_log_.GetEventType(0), ParsedRtcEventLog::RTP_EVENT);

  PacketDirection parsed_direction;
  uint8_t parsed_rtp_header[IP_PACKET_SIZE];
  size_t parsed_header_length;
  size_t parsed_total_length;
  int parsed_probe_cluster_id;
  parsed_log_.GetRtpHeader(0, &parsed_direction, parsed_rtp_header,
                           &parsed_header_length, &parsed_total_length,
                           &parsed_probe_cluster_id);

  EXPECT_EQ(parsed_log_.GetTimestamp(0), timestamp_us);
  EXPECT_EQ(parsed_direction, direction);
  if (parsed_direction == PacketDirection::kOutgoingPacket) {
    EXPECT_EQ(parsed_probe_cluster_id, probe_cluster_id);
  }
  EXPECT_EQ(memcmp(parsed_rtp_header, packet->data(), parsed_header_length), 0);
  EXPECT_EQ(parsed_header_length, packet->headers_size());
  EXPECT_EQ(parsed_total_length, packet->size());
}

TEST_P(RtcEventLogEncoderTest, RtcEventRtpPacketIncoming) {
  TestRtcEventRtpPacket(PacketDirection::kIncomingPacket);
}

TEST_P(RtcEventLogEncoderTest, RtcEventRtpPacketOutgoing) {
  TestRtcEventRtpPacket(PacketDirection::kOutgoingPacket);
}

TEST_P(RtcEventLogEncoderTest, RtcEventVideoReceiveStreamConfig) {
  auto stream_config = rtc::MakeUnique<rtclog::StreamConfig>();
  stream_config->local_ssrc = RandomSsrc();
  stream_config->remote_ssrc = RandomSsrc();
  stream_config->rtcp_mode = RtcpMode::kCompound;
  stream_config->remb = prng_.Rand<bool>();
  std::vector<RtpExtension> extensions = RandomRtpExtensions();
  for (const auto& extension : extensions)
    stream_config->rtp_extensions.push_back(extension);
  stream_config->codecs.emplace_back("CODEC", 122, 7);

  auto original_stream_config = *stream_config;

  auto event = rtc::MakeUnique<RtcEventVideoReceiveStreamConfig>(
      std::move(stream_config));
  const int64_t timestamp_us = event->timestamp_us_;

  ASSERT_TRUE(parsed_log_.ParseString(encoder_->Encode(*event)));
  ASSERT_EQ(parsed_log_.GetNumberOfEvents(), 1u);
  ASSERT_EQ(parsed_log_.GetEventType(0),
            ParsedRtcEventLog::VIDEO_RECEIVER_CONFIG_EVENT);

  auto parsed_event = parsed_log_.GetVideoReceiveConfig(0);
  EXPECT_EQ(parsed_log_.GetTimestamp(0), timestamp_us);
  EXPECT_EQ(parsed_event, original_stream_config);
}

TEST_P(RtcEventLogEncoderTest, RtcEventVideoSendStreamConfig) {
  auto stream_config = rtc::MakeUnique<rtclog::StreamConfig>();
  stream_config->local_ssrc = RandomSsrc();
  std::vector<RtpExtension> extensions = RandomRtpExtensions();
  for (const auto& extension : extensions)
    stream_config->rtp_extensions.push_back(extension);
  stream_config->codecs.emplace_back("CODEC", 120, 3);

  auto original_stream_config = *stream_config;

  auto event =
      rtc::MakeUnique<RtcEventVideoSendStreamConfig>(std::move(stream_config));
  const int64_t timestamp_us = event->timestamp_us_;

  ASSERT_TRUE(parsed_log_.ParseString(encoder_->Encode(*event)));
  ASSERT_EQ(parsed_log_.GetNumberOfEvents(), 1u);
  ASSERT_EQ(parsed_log_.GetEventType(0),
            ParsedRtcEventLog::VIDEO_SENDER_CONFIG_EVENT);

  auto parsed_event = parsed_log_.GetVideoSendConfig(0)[0];
  EXPECT_EQ(parsed_log_.GetTimestamp(0), timestamp_us);
  EXPECT_EQ(parsed_event, original_stream_config);
}

INSTANTIATE_TEST_CASE_P(RandomSeeds,
                        RtcEventLogEncoderTest,
                        ::testing::Values(1, 2, 3, 4, 5));

}  // namespace webrtc
