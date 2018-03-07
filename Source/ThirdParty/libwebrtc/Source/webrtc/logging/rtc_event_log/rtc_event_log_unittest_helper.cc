/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "logging/rtc_event_log/rtc_event_log_unittest_helper.h"

#include <string.h>

#include <string>
#include <vector>

#include "modules/audio_coding/audio_network_adaptor/include/audio_network_adaptor.h"
#include "modules/remote_bitrate_estimator/include/bwe_defines.h"
#include "rtc_base/checks.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/testsupport/fileutils.h"

// Files generated at build-time by the protobuf compiler.
#ifdef WEBRTC_ANDROID_PLATFORM_BUILD
#include "external/webrtc/webrtc/logging/rtc_event_log/rtc_event_log.pb.h"
#else
#include "logging/rtc_event_log/rtc_event_log.pb.h"
#endif

namespace webrtc {

namespace {

BandwidthUsage GetRuntimeDetectorState(
    rtclog::DelayBasedBweUpdate::DetectorState detector_state) {
  switch (detector_state) {
    case rtclog::DelayBasedBweUpdate::BWE_NORMAL:
      return BandwidthUsage::kBwNormal;
    case rtclog::DelayBasedBweUpdate::BWE_UNDERUSING:
      return BandwidthUsage::kBwUnderusing;
    case rtclog::DelayBasedBweUpdate::BWE_OVERUSING:
      return BandwidthUsage::kBwOverusing;
  }
  RTC_NOTREACHED();
  return BandwidthUsage::kBwNormal;
}

rtclog::BweProbeResult::ResultType GetProbeResultType(
    ProbeFailureReason failure_reason) {
  switch (failure_reason) {
    case ProbeFailureReason::kInvalidSendReceiveInterval:
      return rtclog::BweProbeResult::INVALID_SEND_RECEIVE_INTERVAL;
    case ProbeFailureReason::kInvalidSendReceiveRatio:
      return rtclog::BweProbeResult::INVALID_SEND_RECEIVE_RATIO;
    case ProbeFailureReason::kTimeout:
      return rtclog::BweProbeResult::TIMEOUT;
    case ProbeFailureReason::kLast:
      RTC_NOTREACHED();
  }
  RTC_NOTREACHED();
  return rtclog::BweProbeResult::SUCCESS;
}
}  // namespace

// Checks that the event has a timestamp, a type and exactly the data field
// corresponding to the type.
::testing::AssertionResult IsValidBasicEvent(const rtclog::Event& event) {
  if (!event.has_timestamp_us()) {
    return ::testing::AssertionFailure() << "Event has no timestamp";
  }
  if (!event.has_type()) {
    return ::testing::AssertionFailure() << "Event has no event type";
  }
  rtclog::Event_EventType type = event.type();
  if ((type == rtclog::Event::RTP_EVENT) != event.has_rtp_packet()) {
    return ::testing::AssertionFailure()
           << "Event of type " << type << " has "
           << (event.has_rtp_packet() ? "" : "no ") << "RTP packet";
  }
  if ((type == rtclog::Event::RTCP_EVENT) != event.has_rtcp_packet()) {
    return ::testing::AssertionFailure()
           << "Event of type " << type << " has "
           << (event.has_rtcp_packet() ? "" : "no ") << "RTCP packet";
  }
  if ((type == rtclog::Event::LOSS_BASED_BWE_UPDATE) !=
      event.has_loss_based_bwe_update()) {
    return ::testing::AssertionFailure()
           << "Event of type " << type << " has "
           << (event.has_loss_based_bwe_update() ? "" : "no ") << "loss update";
  }
  if ((type == rtclog::Event::DELAY_BASED_BWE_UPDATE) !=
      event.has_delay_based_bwe_update()) {
    return ::testing::AssertionFailure()
           << "Event of type " << type << " has "
           << (event.has_delay_based_bwe_update() ? "" : "no ")
           << "delay update";
  }
  if ((type == rtclog::Event::AUDIO_PLAYOUT_EVENT) !=
      event.has_audio_playout_event()) {
    return ::testing::AssertionFailure()
           << "Event of type " << type << " has "
           << (event.has_audio_playout_event() ? "" : "no ")
           << "audio_playout event";
  }
  if ((type == rtclog::Event::VIDEO_RECEIVER_CONFIG_EVENT) !=
      event.has_video_receiver_config()) {
    return ::testing::AssertionFailure()
           << "Event of type " << type << " has "
           << (event.has_video_receiver_config() ? "" : "no ")
           << "receiver config";
  }
  if ((type == rtclog::Event::VIDEO_SENDER_CONFIG_EVENT) !=
      event.has_video_sender_config()) {
    return ::testing::AssertionFailure()
           << "Event of type " << type << " has "
           << (event.has_video_sender_config() ? "" : "no ") << "sender config";
  }
  if ((type == rtclog::Event::AUDIO_RECEIVER_CONFIG_EVENT) !=
      event.has_audio_receiver_config()) {
    return ::testing::AssertionFailure()
           << "Event of type " << type << " has "
           << (event.has_audio_receiver_config() ? "" : "no ")
           << "audio receiver config";
  }
  if ((type == rtclog::Event::AUDIO_SENDER_CONFIG_EVENT) !=
      event.has_audio_sender_config()) {
    return ::testing::AssertionFailure()
           << "Event of type " << type << " has "
           << (event.has_audio_sender_config() ? "" : "no ")
           << "audio sender config";
  }
  if ((type == rtclog::Event::AUDIO_NETWORK_ADAPTATION_EVENT) !=
      event.has_audio_network_adaptation()) {
    return ::testing::AssertionFailure()
           << "Event of type " << type << " has "
           << (event.has_audio_network_adaptation() ? "" : "no ")
           << "audio network adaptation";
  }
  if ((type == rtclog::Event::BWE_PROBE_CLUSTER_CREATED_EVENT) !=
      event.has_probe_cluster()) {
    return ::testing::AssertionFailure()
           << "Event of type " << type << " has "
           << (event.has_probe_cluster() ? "" : "no ") << "bwe probe cluster";
  }
  if ((type == rtclog::Event::BWE_PROBE_RESULT_EVENT) !=
      event.has_probe_result()) {
    return ::testing::AssertionFailure()
           << "Event of type " << type << " has "
           << (event.has_probe_result() ? "" : "no ") << "bwe probe result";
  }
  return ::testing::AssertionSuccess();
}

void VerifyStreamConfigsAreEqual(const rtclog::StreamConfig& config_1,
                                 const rtclog::StreamConfig& config_2) {
  EXPECT_EQ(config_1.remote_ssrc, config_2.remote_ssrc);
  EXPECT_EQ(config_1.local_ssrc, config_2.local_ssrc);
  EXPECT_EQ(config_1.rtx_ssrc, config_2.rtx_ssrc);
  EXPECT_EQ(config_1.rtcp_mode, config_2.rtcp_mode);
  EXPECT_EQ(config_1.remb, config_2.remb);

  ASSERT_EQ(config_1.rtp_extensions.size(), config_2.rtp_extensions.size());
  for (size_t i = 0; i < config_2.rtp_extensions.size(); i++) {
    EXPECT_EQ(config_1.rtp_extensions[i].uri, config_2.rtp_extensions[i].uri);
    EXPECT_EQ(config_1.rtp_extensions[i].id, config_2.rtp_extensions[i].id);
  }
  ASSERT_EQ(config_1.codecs.size(), config_2.codecs.size());
  for (size_t i = 0; i < config_2.codecs.size(); i++) {
    EXPECT_EQ(config_1.codecs[i].payload_name, config_2.codecs[i].payload_name);
    EXPECT_EQ(config_1.codecs[i].payload_type, config_2.codecs[i].payload_type);
    EXPECT_EQ(config_1.codecs[i].rtx_payload_type,
              config_2.codecs[i].rtx_payload_type);
  }
}

void RtcEventLogTestHelper::VerifyVideoReceiveStreamConfig(
    const ParsedRtcEventLog& parsed_log,
    size_t index,
    const rtclog::StreamConfig& config) {
  ASSERT_LT(index, parsed_log.events_.size());
  const rtclog::Event& event = parsed_log.events_[index];
  ASSERT_TRUE(IsValidBasicEvent(event));
  ASSERT_EQ(rtclog::Event::VIDEO_RECEIVER_CONFIG_EVENT, event.type());
  const rtclog::VideoReceiveConfig& receiver_config =
      event.video_receiver_config();
  // Check SSRCs.
  ASSERT_TRUE(receiver_config.has_remote_ssrc());
  EXPECT_EQ(config.remote_ssrc, receiver_config.remote_ssrc());
  ASSERT_TRUE(receiver_config.has_local_ssrc());
  EXPECT_EQ(config.local_ssrc, receiver_config.local_ssrc());
  // Check RTCP settings.
  ASSERT_TRUE(receiver_config.has_rtcp_mode());
  if (config.rtcp_mode == RtcpMode::kCompound) {
    EXPECT_EQ(rtclog::VideoReceiveConfig::RTCP_COMPOUND,
              receiver_config.rtcp_mode());
  } else {
    EXPECT_EQ(rtclog::VideoReceiveConfig::RTCP_REDUCEDSIZE,
              receiver_config.rtcp_mode());
  }
  ASSERT_TRUE(receiver_config.has_remb());
  EXPECT_EQ(config.remb, receiver_config.remb());
  // Check RTX map.
  for (const rtclog::RtxMap& rtx_map : receiver_config.rtx_map()) {
    ASSERT_TRUE(rtx_map.has_payload_type());
    ASSERT_TRUE(rtx_map.has_config());
    const rtclog::RtxConfig& rtx_config = rtx_map.config();
    ASSERT_TRUE(rtx_config.has_rtx_ssrc());
    ASSERT_TRUE(rtx_config.has_rtx_payload_type());

    EXPECT_EQ(config.rtx_ssrc, rtx_config.rtx_ssrc());
    auto codec_found =
        std::find_if(config.codecs.begin(), config.codecs.end(),
                     [&rtx_map](const rtclog::StreamConfig::Codec& codec) {
                       return rtx_map.payload_type() == codec.payload_type;
                     });
    ASSERT_TRUE(codec_found != config.codecs.end());
    EXPECT_EQ(rtx_config.rtx_payload_type(), codec_found->rtx_payload_type);
  }
  // Check header extensions.
  ASSERT_EQ(static_cast<int>(config.rtp_extensions.size()),
            receiver_config.header_extensions_size());
  for (int i = 0; i < receiver_config.header_extensions_size(); i++) {
    ASSERT_TRUE(receiver_config.header_extensions(i).has_name());
    ASSERT_TRUE(receiver_config.header_extensions(i).has_id());
    const std::string& name = receiver_config.header_extensions(i).name();
    int id = receiver_config.header_extensions(i).id();
    EXPECT_EQ(config.rtp_extensions[i].id, id);
    EXPECT_EQ(config.rtp_extensions[i].uri, name);
  }
  // Check decoders.
  ASSERT_EQ(static_cast<int>(config.codecs.size()),
            receiver_config.decoders_size());
  for (int i = 0; i < receiver_config.decoders_size(); i++) {
    ASSERT_TRUE(receiver_config.decoders(i).has_name());
    ASSERT_TRUE(receiver_config.decoders(i).has_payload_type());
    const std::string& decoder_name = receiver_config.decoders(i).name();
    int decoder_type = receiver_config.decoders(i).payload_type();
    EXPECT_EQ(config.codecs[i].payload_name, decoder_name);
    EXPECT_EQ(config.codecs[i].payload_type, decoder_type);
  }

  // Check consistency of the parser.
  rtclog::StreamConfig parsed_config = parsed_log.GetVideoReceiveConfig(index);
  VerifyStreamConfigsAreEqual(config, parsed_config);
}

void RtcEventLogTestHelper::VerifyVideoSendStreamConfig(
    const ParsedRtcEventLog& parsed_log,
    size_t index,
    const rtclog::StreamConfig& config) {
  ASSERT_LT(index, parsed_log.events_.size());
  const rtclog::Event& event = parsed_log.events_[index];
  ASSERT_TRUE(IsValidBasicEvent(event));
  ASSERT_EQ(rtclog::Event::VIDEO_SENDER_CONFIG_EVENT, event.type());
  const rtclog::VideoSendConfig& sender_config = event.video_sender_config();

  EXPECT_EQ(config.local_ssrc, sender_config.ssrcs(0));
  EXPECT_EQ(config.rtx_ssrc, sender_config.rtx_ssrcs(0));

  // Check header extensions.
  ASSERT_EQ(static_cast<int>(config.rtp_extensions.size()),
            sender_config.header_extensions_size());
  for (int i = 0; i < sender_config.header_extensions_size(); i++) {
    ASSERT_TRUE(sender_config.header_extensions(i).has_name());
    ASSERT_TRUE(sender_config.header_extensions(i).has_id());
    const std::string& name = sender_config.header_extensions(i).name();
    int id = sender_config.header_extensions(i).id();
    EXPECT_EQ(config.rtp_extensions[i].id, id);
    EXPECT_EQ(config.rtp_extensions[i].uri, name);
  }
  // Check encoder.
  ASSERT_TRUE(sender_config.has_encoder());
  ASSERT_TRUE(sender_config.encoder().has_name());
  ASSERT_TRUE(sender_config.encoder().has_payload_type());
  EXPECT_EQ(config.codecs[0].payload_name, sender_config.encoder().name());
  EXPECT_EQ(config.codecs[0].payload_type,
            sender_config.encoder().payload_type());

  EXPECT_EQ(config.codecs[0].rtx_payload_type,
            sender_config.rtx_payload_type());

  // Check consistency of the parser.
  std::vector<rtclog::StreamConfig> parsed_configs =
      parsed_log.GetVideoSendConfig(index);
  ASSERT_EQ(1u, parsed_configs.size());
  VerifyStreamConfigsAreEqual(config, parsed_configs[0]);
}

void RtcEventLogTestHelper::VerifyAudioReceiveStreamConfig(
    const ParsedRtcEventLog& parsed_log,
    size_t index,
    const rtclog::StreamConfig& config) {
  ASSERT_LT(index, parsed_log.events_.size());
  const rtclog::Event& event = parsed_log.events_[index];
  ASSERT_TRUE(IsValidBasicEvent(event));
  ASSERT_EQ(rtclog::Event::AUDIO_RECEIVER_CONFIG_EVENT, event.type());
  const rtclog::AudioReceiveConfig& receiver_config =
      event.audio_receiver_config();
  // Check SSRCs.
  ASSERT_TRUE(receiver_config.has_remote_ssrc());
  EXPECT_EQ(config.remote_ssrc, receiver_config.remote_ssrc());
  ASSERT_TRUE(receiver_config.has_local_ssrc());
  EXPECT_EQ(config.local_ssrc, receiver_config.local_ssrc());
  // Check header extensions.
  ASSERT_EQ(static_cast<int>(config.rtp_extensions.size()),
            receiver_config.header_extensions_size());
  for (int i = 0; i < receiver_config.header_extensions_size(); i++) {
    ASSERT_TRUE(receiver_config.header_extensions(i).has_name());
    ASSERT_TRUE(receiver_config.header_extensions(i).has_id());
    const std::string& name = receiver_config.header_extensions(i).name();
    int id = receiver_config.header_extensions(i).id();
    EXPECT_EQ(config.rtp_extensions[i].id, id);
    EXPECT_EQ(config.rtp_extensions[i].uri, name);
  }

  // Check consistency of the parser.
  rtclog::StreamConfig parsed_config = parsed_log.GetAudioReceiveConfig(index);
  EXPECT_EQ(config.remote_ssrc, parsed_config.remote_ssrc);
  EXPECT_EQ(config.local_ssrc, parsed_config.local_ssrc);
  // Check header extensions.
  EXPECT_EQ(config.rtp_extensions.size(), parsed_config.rtp_extensions.size());
  for (size_t i = 0; i < parsed_config.rtp_extensions.size(); i++) {
    EXPECT_EQ(config.rtp_extensions[i].uri,
              parsed_config.rtp_extensions[i].uri);
    EXPECT_EQ(config.rtp_extensions[i].id, parsed_config.rtp_extensions[i].id);
  }
}

void RtcEventLogTestHelper::VerifyAudioSendStreamConfig(
    const ParsedRtcEventLog& parsed_log,
    size_t index,
    const rtclog::StreamConfig& config) {
  ASSERT_LT(index, parsed_log.events_.size());
  const rtclog::Event& event = parsed_log.events_[index];
  ASSERT_TRUE(IsValidBasicEvent(event));
  ASSERT_EQ(rtclog::Event::AUDIO_SENDER_CONFIG_EVENT, event.type());
  const rtclog::AudioSendConfig& sender_config = event.audio_sender_config();
  // Check SSRCs.
  EXPECT_EQ(config.local_ssrc, sender_config.ssrc());
  // Check header extensions.
  ASSERT_EQ(static_cast<int>(config.rtp_extensions.size()),
            sender_config.header_extensions_size());
  for (int i = 0; i < sender_config.header_extensions_size(); i++) {
    ASSERT_TRUE(sender_config.header_extensions(i).has_name());
    ASSERT_TRUE(sender_config.header_extensions(i).has_id());
    const std::string& name = sender_config.header_extensions(i).name();
    int id = sender_config.header_extensions(i).id();
    EXPECT_EQ(config.rtp_extensions[i].id, id);
    EXPECT_EQ(config.rtp_extensions[i].uri, name);
  }

  // Check consistency of the parser.
  rtclog::StreamConfig parsed_config = parsed_log.GetAudioSendConfig(index);
  VerifyStreamConfigsAreEqual(config, parsed_config);
}

void RtcEventLogTestHelper::VerifyIncomingRtpEvent(
    const ParsedRtcEventLog& parsed_log,
    size_t index,
    const RtpPacketReceived& expected_packet) {
  ASSERT_LT(index, parsed_log.events_.size());
  const rtclog::Event& event = parsed_log.events_[index];
  ASSERT_TRUE(IsValidBasicEvent(event));
  ASSERT_EQ(rtclog::Event::RTP_EVENT, event.type());
  const rtclog::RtpPacket& rtp_packet = event.rtp_packet();
  ASSERT_TRUE(rtp_packet.has_incoming());
  EXPECT_TRUE(rtp_packet.incoming());
  ASSERT_TRUE(rtp_packet.has_packet_length());
  EXPECT_EQ(expected_packet.size(), rtp_packet.packet_length());
  size_t header_size = expected_packet.headers_size();
  ASSERT_TRUE(rtp_packet.has_header());
  EXPECT_THAT(testing::make_tuple(expected_packet.data(), header_size),
              testing::ElementsAreArray(rtp_packet.header().data(),
                                        rtp_packet.header().size()));

  // Check consistency of the parser.
  PacketDirection parsed_direction;
  uint8_t parsed_header[1500];
  size_t parsed_header_size, parsed_total_size;
  parsed_log.GetRtpHeader(index, &parsed_direction, parsed_header,
                          &parsed_header_size, &parsed_total_size, nullptr);
  EXPECT_EQ(kIncomingPacket, parsed_direction);
  EXPECT_THAT(testing::make_tuple(expected_packet.data(), header_size),
              testing::ElementsAreArray(parsed_header, parsed_header_size));
  EXPECT_EQ(expected_packet.size(), parsed_total_size);
}

void RtcEventLogTestHelper::VerifyOutgoingRtpEvent(
    const ParsedRtcEventLog& parsed_log,
    size_t index,
    const RtpPacketToSend& expected_packet) {
  ASSERT_LT(index, parsed_log.events_.size());
  const rtclog::Event& event = parsed_log.events_[index];
  ASSERT_TRUE(IsValidBasicEvent(event));
  ASSERT_EQ(rtclog::Event::RTP_EVENT, event.type());
  const rtclog::RtpPacket& rtp_packet = event.rtp_packet();
  ASSERT_TRUE(rtp_packet.has_incoming());
  EXPECT_FALSE(rtp_packet.incoming());
  ASSERT_TRUE(rtp_packet.has_packet_length());
  EXPECT_EQ(expected_packet.size(), rtp_packet.packet_length());
  size_t header_size = expected_packet.headers_size();
  ASSERT_TRUE(rtp_packet.has_header());
  EXPECT_THAT(testing::make_tuple(expected_packet.data(), header_size),
              testing::ElementsAreArray(rtp_packet.header().data(),
                                        rtp_packet.header().size()));

  // Check consistency of the parser.
  PacketDirection parsed_direction;
  uint8_t parsed_header[1500];
  size_t parsed_header_size, parsed_total_size;
  parsed_log.GetRtpHeader(index, &parsed_direction, parsed_header,
                          &parsed_header_size, &parsed_total_size, nullptr);
  EXPECT_EQ(kOutgoingPacket, parsed_direction);
  EXPECT_THAT(testing::make_tuple(expected_packet.data(), header_size),
              testing::ElementsAreArray(parsed_header, parsed_header_size));
  EXPECT_EQ(expected_packet.size(), parsed_total_size);
}

void RtcEventLogTestHelper::VerifyRtcpEvent(const ParsedRtcEventLog& parsed_log,
                                            size_t index,
                                            PacketDirection direction,
                                            const uint8_t* packet,
                                            size_t total_size) {
  ASSERT_LT(index, parsed_log.events_.size());
  const rtclog::Event& event = parsed_log.events_[index];
  ASSERT_TRUE(IsValidBasicEvent(event));
  ASSERT_EQ(rtclog::Event::RTCP_EVENT, event.type());
  const rtclog::RtcpPacket& rtcp_packet = event.rtcp_packet();
  ASSERT_TRUE(rtcp_packet.has_incoming());
  EXPECT_EQ(direction == kIncomingPacket, rtcp_packet.incoming());
  ASSERT_TRUE(rtcp_packet.has_packet_data());
  ASSERT_EQ(total_size, rtcp_packet.packet_data().size());
  for (size_t i = 0; i < total_size; i++) {
    EXPECT_EQ(packet[i], static_cast<uint8_t>(rtcp_packet.packet_data()[i]));
  }

  // Check consistency of the parser.
  PacketDirection parsed_direction;
  uint8_t parsed_packet[1500];
  size_t parsed_total_size;
  parsed_log.GetRtcpPacket(index, &parsed_direction, parsed_packet,
                           &parsed_total_size);
  EXPECT_EQ(direction, parsed_direction);
  ASSERT_EQ(total_size, parsed_total_size);
  EXPECT_EQ(0, std::memcmp(packet, parsed_packet, total_size));
}

void RtcEventLogTestHelper::VerifyPlayoutEvent(
    const ParsedRtcEventLog& parsed_log,
    size_t index,
    uint32_t ssrc) {
  ASSERT_LT(index, parsed_log.events_.size());
  const rtclog::Event& event = parsed_log.events_[index];
  ASSERT_TRUE(IsValidBasicEvent(event));
  ASSERT_EQ(rtclog::Event::AUDIO_PLAYOUT_EVENT, event.type());
  const rtclog::AudioPlayoutEvent& playout_event = event.audio_playout_event();
  ASSERT_TRUE(playout_event.has_local_ssrc());
  EXPECT_EQ(ssrc, playout_event.local_ssrc());

  // Check consistency of the parser.
  uint32_t parsed_ssrc;
  parsed_log.GetAudioPlayout(index, &parsed_ssrc);
  EXPECT_EQ(ssrc, parsed_ssrc);
}

void RtcEventLogTestHelper::VerifyBweLossEvent(
    const ParsedRtcEventLog& parsed_log,
    size_t index,
    int32_t bitrate,
    uint8_t fraction_loss,
    int32_t total_packets) {
  ASSERT_LT(index, parsed_log.events_.size());
  const rtclog::Event& event = parsed_log.events_[index];
  ASSERT_TRUE(IsValidBasicEvent(event));
  ASSERT_EQ(rtclog::Event::LOSS_BASED_BWE_UPDATE, event.type());
  const rtclog::LossBasedBweUpdate& bwe_event = event.loss_based_bwe_update();
  ASSERT_TRUE(bwe_event.has_bitrate_bps());
  EXPECT_EQ(bitrate, bwe_event.bitrate_bps());
  ASSERT_TRUE(bwe_event.has_fraction_loss());
  EXPECT_EQ(fraction_loss, bwe_event.fraction_loss());
  ASSERT_TRUE(bwe_event.has_total_packets());
  EXPECT_EQ(total_packets, bwe_event.total_packets());

  // Check consistency of the parser.
  int32_t parsed_bitrate;
  uint8_t parsed_fraction_loss;
  int32_t parsed_total_packets;
  parsed_log.GetLossBasedBweUpdate(
      index, &parsed_bitrate, &parsed_fraction_loss, &parsed_total_packets);
  EXPECT_EQ(bitrate, parsed_bitrate);
  EXPECT_EQ(fraction_loss, parsed_fraction_loss);
  EXPECT_EQ(total_packets, parsed_total_packets);
}

void RtcEventLogTestHelper::VerifyBweDelayEvent(
    const ParsedRtcEventLog& parsed_log,
    size_t index,
    int32_t bitrate,
    BandwidthUsage detector_state) {
  ASSERT_LT(index, parsed_log.events_.size());
  const rtclog::Event& event = parsed_log.events_[index];
  ASSERT_TRUE(IsValidBasicEvent(event));
  ASSERT_EQ(rtclog::Event::DELAY_BASED_BWE_UPDATE, event.type());
  const rtclog::DelayBasedBweUpdate& bwe_event = event.delay_based_bwe_update();
  ASSERT_TRUE(bwe_event.has_bitrate_bps());
  EXPECT_EQ(bitrate, bwe_event.bitrate_bps());
  ASSERT_TRUE(bwe_event.has_detector_state());
  EXPECT_EQ(detector_state,
            GetRuntimeDetectorState(bwe_event.detector_state()));

  // Check consistency of the parser.
  ParsedRtcEventLog::BweDelayBasedUpdate res =
      parsed_log.GetDelayBasedBweUpdate(index);
  EXPECT_EQ(res.bitrate_bps, bitrate);
  EXPECT_EQ(res.detector_state, detector_state);
}

void RtcEventLogTestHelper::VerifyAudioNetworkAdaptation(
    const ParsedRtcEventLog& parsed_log,
    size_t index,
    const AudioEncoderRuntimeConfig& config) {
  AudioEncoderRuntimeConfig parsed_config;
  parsed_log.GetAudioNetworkAdaptation(index, &parsed_config);
  EXPECT_EQ(config.bitrate_bps, parsed_config.bitrate_bps);
  EXPECT_EQ(config.enable_dtx, parsed_config.enable_dtx);
  EXPECT_EQ(config.enable_fec, parsed_config.enable_fec);
  EXPECT_EQ(config.frame_length_ms, parsed_config.frame_length_ms);
  EXPECT_EQ(config.num_channels, parsed_config.num_channels);
  EXPECT_EQ(config.uplink_packet_loss_fraction,
            parsed_config.uplink_packet_loss_fraction);
}

void RtcEventLogTestHelper::VerifyLogStartEvent(
    const ParsedRtcEventLog& parsed_log,
    size_t index) {
  ASSERT_LT(index, parsed_log.events_.size());
  const rtclog::Event& event = parsed_log.events_[index];
  ASSERT_TRUE(IsValidBasicEvent(event));
  EXPECT_EQ(rtclog::Event::LOG_START, event.type());
}

void RtcEventLogTestHelper::VerifyLogEndEvent(
    const ParsedRtcEventLog& parsed_log,
    size_t index) {
  ASSERT_LT(index, parsed_log.events_.size());
  const rtclog::Event& event = parsed_log.events_[index];
  ASSERT_TRUE(IsValidBasicEvent(event));
  EXPECT_EQ(rtclog::Event::LOG_END, event.type());
}

void RtcEventLogTestHelper::VerifyBweProbeCluster(
    const ParsedRtcEventLog& parsed_log,
    size_t index,
    uint32_t id,
    uint32_t bitrate_bps,
    uint32_t min_probes,
    uint32_t min_bytes) {
  ASSERT_LT(index, parsed_log.events_.size());
  const rtclog::Event& event = parsed_log.events_[index];
  ASSERT_TRUE(IsValidBasicEvent(event));
  EXPECT_EQ(rtclog::Event::BWE_PROBE_CLUSTER_CREATED_EVENT, event.type());

  const rtclog::BweProbeCluster& bwe_event = event.probe_cluster();
  ASSERT_TRUE(bwe_event.has_id());
  EXPECT_EQ(id, bwe_event.id());
  ASSERT_TRUE(bwe_event.has_bitrate_bps());
  EXPECT_EQ(bitrate_bps, bwe_event.bitrate_bps());
  ASSERT_TRUE(bwe_event.has_min_packets());
  EXPECT_EQ(min_probes, bwe_event.min_packets());
  ASSERT_TRUE(bwe_event.has_min_bytes());
  EXPECT_EQ(min_bytes, bwe_event.min_bytes());

  // TODO(philipel): Verify the parser when parsing has been implemented.
}

void RtcEventLogTestHelper::VerifyProbeResultSuccess(
    const ParsedRtcEventLog& parsed_log,
    size_t index,
    uint32_t id,
    uint32_t bitrate_bps) {
  ASSERT_LT(index, parsed_log.events_.size());
  const rtclog::Event& event = parsed_log.events_[index];
  ASSERT_TRUE(IsValidBasicEvent(event));
  EXPECT_EQ(rtclog::Event::BWE_PROBE_RESULT_EVENT, event.type());

  const rtclog::BweProbeResult& bwe_event = event.probe_result();
  ASSERT_TRUE(bwe_event.has_id());
  EXPECT_EQ(id, bwe_event.id());
  ASSERT_TRUE(bwe_event.has_bitrate_bps());
  EXPECT_EQ(bitrate_bps, bwe_event.bitrate_bps());
  ASSERT_TRUE(bwe_event.has_result());
  EXPECT_EQ(rtclog::BweProbeResult::SUCCESS, bwe_event.result());

  // TODO(philipel): Verify the parser when parsing has been implemented.
}

void RtcEventLogTestHelper::VerifyProbeResultFailure(
    const ParsedRtcEventLog& parsed_log,
    size_t index,
    uint32_t id,
    ProbeFailureReason failure_reason) {
  ASSERT_LT(index, parsed_log.events_.size());
  const rtclog::Event& event = parsed_log.events_[index];
  ASSERT_TRUE(IsValidBasicEvent(event));
  EXPECT_EQ(rtclog::Event::BWE_PROBE_RESULT_EVENT, event.type());

  const rtclog::BweProbeResult& bwe_event = event.probe_result();
  ASSERT_TRUE(bwe_event.has_id());
  EXPECT_EQ(id, bwe_event.id());
  ASSERT_TRUE(bwe_event.has_result());
  EXPECT_EQ(GetProbeResultType(failure_reason), bwe_event.result());
  ASSERT_FALSE(bwe_event.has_bitrate_bps());

  // TODO(philipel): Verify the parser when parsing has been implemented.
}

}  // namespace webrtc
