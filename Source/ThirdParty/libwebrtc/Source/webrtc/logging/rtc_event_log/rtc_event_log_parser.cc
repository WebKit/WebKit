/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/logging/rtc_event_log/rtc_event_log_parser.h"

#include <stdint.h>
#include <string.h>

#include <algorithm>
#include <fstream>
#include <istream>
#include <utility>

#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/call/call.h"
#include "webrtc/logging/rtc_event_log/rtc_event_log.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp_defines.h"

namespace webrtc {

namespace {
MediaType GetRuntimeMediaType(rtclog::MediaType media_type) {
  switch (media_type) {
    case rtclog::MediaType::ANY:
      return MediaType::ANY;
    case rtclog::MediaType::AUDIO:
      return MediaType::AUDIO;
    case rtclog::MediaType::VIDEO:
      return MediaType::VIDEO;
    case rtclog::MediaType::DATA:
      return MediaType::DATA;
  }
  RTC_NOTREACHED();
  return MediaType::ANY;
}

RtcpMode GetRuntimeRtcpMode(rtclog::VideoReceiveConfig::RtcpMode rtcp_mode) {
  switch (rtcp_mode) {
    case rtclog::VideoReceiveConfig::RTCP_COMPOUND:
      return RtcpMode::kCompound;
    case rtclog::VideoReceiveConfig::RTCP_REDUCEDSIZE:
      return RtcpMode::kReducedSize;
  }
  RTC_NOTREACHED();
  return RtcpMode::kOff;
}

ParsedRtcEventLog::EventType GetRuntimeEventType(
    rtclog::Event::EventType event_type) {
  switch (event_type) {
    case rtclog::Event::UNKNOWN_EVENT:
      return ParsedRtcEventLog::EventType::UNKNOWN_EVENT;
    case rtclog::Event::LOG_START:
      return ParsedRtcEventLog::EventType::LOG_START;
    case rtclog::Event::LOG_END:
      return ParsedRtcEventLog::EventType::LOG_END;
    case rtclog::Event::RTP_EVENT:
      return ParsedRtcEventLog::EventType::RTP_EVENT;
    case rtclog::Event::RTCP_EVENT:
      return ParsedRtcEventLog::EventType::RTCP_EVENT;
    case rtclog::Event::AUDIO_PLAYOUT_EVENT:
      return ParsedRtcEventLog::EventType::AUDIO_PLAYOUT_EVENT;
    case rtclog::Event::LOSS_BASED_BWE_UPDATE:
      return ParsedRtcEventLog::EventType::LOSS_BASED_BWE_UPDATE;
    case rtclog::Event::DELAY_BASED_BWE_UPDATE:
      return ParsedRtcEventLog::EventType::DELAY_BASED_BWE_UPDATE;
    case rtclog::Event::VIDEO_RECEIVER_CONFIG_EVENT:
      return ParsedRtcEventLog::EventType::VIDEO_RECEIVER_CONFIG_EVENT;
    case rtclog::Event::VIDEO_SENDER_CONFIG_EVENT:
      return ParsedRtcEventLog::EventType::VIDEO_SENDER_CONFIG_EVENT;
    case rtclog::Event::AUDIO_RECEIVER_CONFIG_EVENT:
      return ParsedRtcEventLog::EventType::AUDIO_RECEIVER_CONFIG_EVENT;
    case rtclog::Event::AUDIO_SENDER_CONFIG_EVENT:
      return ParsedRtcEventLog::EventType::AUDIO_SENDER_CONFIG_EVENT;
    case rtclog::Event::AUDIO_NETWORK_ADAPTATION_EVENT:
      return ParsedRtcEventLog::EventType::AUDIO_NETWORK_ADAPTATION_EVENT;
    case rtclog::Event::BWE_PROBE_CLUSTER_CREATED_EVENT:
      return ParsedRtcEventLog::EventType::BWE_PROBE_CLUSTER_CREATED_EVENT;
    case rtclog::Event::BWE_PROBE_RESULT_EVENT:
      return ParsedRtcEventLog::EventType::BWE_PROBE_RESULT_EVENT;
  }
  RTC_NOTREACHED();
  return ParsedRtcEventLog::EventType::UNKNOWN_EVENT;
}

BandwidthUsage GetRuntimeDetectorState(
    rtclog::DelayBasedBweUpdate::DetectorState detector_state) {
  switch (detector_state) {
    case rtclog::DelayBasedBweUpdate::BWE_NORMAL:
      return kBwNormal;
    case rtclog::DelayBasedBweUpdate::BWE_UNDERUSING:
      return kBwUnderusing;
    case rtclog::DelayBasedBweUpdate::BWE_OVERUSING:
      return kBwOverusing;
  }
  RTC_NOTREACHED();
  return kBwNormal;
}

std::pair<uint64_t, bool> ParseVarInt(std::istream& stream) {
  uint64_t varint = 0;
  for (size_t bytes_read = 0; bytes_read < 10; ++bytes_read) {
    // The most significant bit of each byte is 0 if it is the last byte in
    // the varint and 1 otherwise. Thus, we take the 7 least significant bits
    // of each byte and shift them 7 bits for each byte read previously to get
    // the (unsigned) integer.
    int byte = stream.get();
    if (stream.eof()) {
      return std::make_pair(varint, false);
    }
    RTC_DCHECK(0 <= byte && byte <= 255);
    varint |= static_cast<uint64_t>(byte & 0x7F) << (7 * bytes_read);
    if ((byte & 0x80) == 0) {
      return std::make_pair(varint, true);
    }
  }
  return std::make_pair(varint, false);
}

void GetHeaderExtensions(
    std::vector<RtpExtension>* header_extensions,
    const google::protobuf::RepeatedPtrField<rtclog::RtpHeaderExtension>&
        proto_header_extensions) {
  header_extensions->clear();
  for (auto& p : proto_header_extensions) {
    RTC_CHECK(p.has_name());
    RTC_CHECK(p.has_id());
    const std::string& name = p.name();
    int id = p.id();
    header_extensions->push_back(RtpExtension(name, id));
  }
}

}  // namespace

bool ParsedRtcEventLog::ParseFile(const std::string& filename) {
  std::ifstream file(filename, std::ios_base::in | std::ios_base::binary);
  if (!file.good() || !file.is_open()) {
    LOG(LS_WARNING) << "Could not open file for reading.";
    return false;
  }

  return ParseStream(file);
}

bool ParsedRtcEventLog::ParseString(const std::string& s) {
  std::istringstream stream(s, std::ios_base::in | std::ios_base::binary);
  return ParseStream(stream);
}

bool ParsedRtcEventLog::ParseStream(std::istream& stream) {
  events_.clear();
  const size_t kMaxEventSize = (1u << 16) - 1;
  std::vector<char> tmp_buffer(kMaxEventSize);
  uint64_t tag;
  uint64_t message_length;
  bool success;

  RTC_DCHECK(stream.good());

  while (1) {
    // Check whether we have reached end of file.
    stream.peek();
    if (stream.eof()) {
      return true;
    }

    // Read the next message tag. The tag number is defined as
    // (fieldnumber << 3) | wire_type. In our case, the field number is
    // supposed to be 1 and the wire type for an length-delimited field is 2.
    const uint64_t kExpectedTag = (1 << 3) | 2;
    std::tie(tag, success) = ParseVarInt(stream);
    if (!success) {
      LOG(LS_WARNING) << "Missing field tag from beginning of protobuf event.";
      return false;
    } else if (tag != kExpectedTag) {
      LOG(LS_WARNING) << "Unexpected field tag at beginning of protobuf event.";
      return false;
    }

    // Read the length field.
    std::tie(message_length, success) = ParseVarInt(stream);
    if (!success) {
      LOG(LS_WARNING) << "Missing message length after protobuf field tag.";
      return false;
    } else if (message_length > kMaxEventSize) {
      LOG(LS_WARNING) << "Protobuf message length is too large.";
      return false;
    }

    // Read the next protobuf event to a temporary char buffer.
    stream.read(tmp_buffer.data(), message_length);
    if (stream.gcount() != static_cast<int>(message_length)) {
      LOG(LS_WARNING) << "Failed to read protobuf message from file.";
      return false;
    }

    // Parse the protobuf event from the buffer.
    rtclog::Event event;
    if (!event.ParseFromArray(tmp_buffer.data(), message_length)) {
      LOG(LS_WARNING) << "Failed to parse protobuf message.";
      return false;
    }
    events_.push_back(event);
  }
}

size_t ParsedRtcEventLog::GetNumberOfEvents() const {
  return events_.size();
}

int64_t ParsedRtcEventLog::GetTimestamp(size_t index) const {
  RTC_CHECK_LT(index, GetNumberOfEvents());
  const rtclog::Event& event = events_[index];
  RTC_CHECK(event.has_timestamp_us());
  return event.timestamp_us();
}

ParsedRtcEventLog::EventType ParsedRtcEventLog::GetEventType(
    size_t index) const {
  RTC_CHECK_LT(index, GetNumberOfEvents());
  const rtclog::Event& event = events_[index];
  RTC_CHECK(event.has_type());
  return GetRuntimeEventType(event.type());
}

// The header must have space for at least IP_PACKET_SIZE bytes.
void ParsedRtcEventLog::GetRtpHeader(size_t index,
                                     PacketDirection* incoming,
                                     MediaType* media_type,
                                     uint8_t* header,
                                     size_t* header_length,
                                     size_t* total_length) const {
  RTC_CHECK_LT(index, GetNumberOfEvents());
  const rtclog::Event& event = events_[index];
  RTC_CHECK(event.has_type());
  RTC_CHECK_EQ(event.type(), rtclog::Event::RTP_EVENT);
  RTC_CHECK(event.has_rtp_packet());
  const rtclog::RtpPacket& rtp_packet = event.rtp_packet();
  // Get direction of packet.
  RTC_CHECK(rtp_packet.has_incoming());
  if (incoming != nullptr) {
    *incoming = rtp_packet.incoming() ? kIncomingPacket : kOutgoingPacket;
  }
  // Get media type.
  RTC_CHECK(rtp_packet.has_type());
  if (media_type != nullptr) {
    *media_type = GetRuntimeMediaType(rtp_packet.type());
  }
  // Get packet length.
  RTC_CHECK(rtp_packet.has_packet_length());
  if (total_length != nullptr) {
    *total_length = rtp_packet.packet_length();
  }
  // Get header length.
  RTC_CHECK(rtp_packet.has_header());
  if (header_length != nullptr) {
    *header_length = rtp_packet.header().size();
  }
  // Get header contents.
  if (header != nullptr) {
    const size_t kMinRtpHeaderSize = 12;
    RTC_CHECK_GE(rtp_packet.header().size(), kMinRtpHeaderSize);
    RTC_CHECK_LE(rtp_packet.header().size(),
                 static_cast<size_t>(IP_PACKET_SIZE));
    memcpy(header, rtp_packet.header().data(), rtp_packet.header().size());
  }
}

// The packet must have space for at least IP_PACKET_SIZE bytes.
void ParsedRtcEventLog::GetRtcpPacket(size_t index,
                                      PacketDirection* incoming,
                                      MediaType* media_type,
                                      uint8_t* packet,
                                      size_t* length) const {
  RTC_CHECK_LT(index, GetNumberOfEvents());
  const rtclog::Event& event = events_[index];
  RTC_CHECK(event.has_type());
  RTC_CHECK_EQ(event.type(), rtclog::Event::RTCP_EVENT);
  RTC_CHECK(event.has_rtcp_packet());
  const rtclog::RtcpPacket& rtcp_packet = event.rtcp_packet();
  // Get direction of packet.
  RTC_CHECK(rtcp_packet.has_incoming());
  if (incoming != nullptr) {
    *incoming = rtcp_packet.incoming() ? kIncomingPacket : kOutgoingPacket;
  }
  // Get media type.
  RTC_CHECK(rtcp_packet.has_type());
  if (media_type != nullptr) {
    *media_type = GetRuntimeMediaType(rtcp_packet.type());
  }
  // Get packet length.
  RTC_CHECK(rtcp_packet.has_packet_data());
  if (length != nullptr) {
    *length = rtcp_packet.packet_data().size();
  }
  // Get packet contents.
  if (packet != nullptr) {
    RTC_CHECK_LE(rtcp_packet.packet_data().size(),
                 static_cast<unsigned>(IP_PACKET_SIZE));
    memcpy(packet, rtcp_packet.packet_data().data(),
           rtcp_packet.packet_data().size());
  }
}

void ParsedRtcEventLog::GetVideoReceiveConfig(
    size_t index,
    VideoReceiveStream::Config* config) const {
  RTC_CHECK_LT(index, GetNumberOfEvents());
  const rtclog::Event& event = events_[index];
  RTC_CHECK(config != nullptr);
  RTC_CHECK(event.has_type());
  RTC_CHECK_EQ(event.type(), rtclog::Event::VIDEO_RECEIVER_CONFIG_EVENT);
  RTC_CHECK(event.has_video_receiver_config());
  const rtclog::VideoReceiveConfig& receiver_config =
      event.video_receiver_config();
  // Get SSRCs.
  RTC_CHECK(receiver_config.has_remote_ssrc());
  config->rtp.remote_ssrc = receiver_config.remote_ssrc();
  RTC_CHECK(receiver_config.has_local_ssrc());
  config->rtp.local_ssrc = receiver_config.local_ssrc();
  // Get RTCP settings.
  RTC_CHECK(receiver_config.has_rtcp_mode());
  config->rtp.rtcp_mode = GetRuntimeRtcpMode(receiver_config.rtcp_mode());
  RTC_CHECK(receiver_config.has_remb());
  config->rtp.remb = receiver_config.remb();
  // Get RTX map.
  std::vector<uint32_t> rtx_ssrcs(receiver_config.rtx_map_size());
  config->rtp.rtx_payload_types.clear();
  for (int i = 0; i < receiver_config.rtx_map_size(); i++) {
    const rtclog::RtxMap& map = receiver_config.rtx_map(i);
    RTC_CHECK(map.has_payload_type());
    RTC_CHECK(map.has_config());
    RTC_CHECK(map.config().has_rtx_ssrc());
    rtx_ssrcs[i] = map.config().rtx_ssrc();
    RTC_CHECK(map.config().has_rtx_payload_type());
    config->rtp.rtx_payload_types.insert(
        std::make_pair(map.payload_type(), map.config().rtx_payload_type()));
  }
  if (!rtx_ssrcs.empty()) {
    config->rtp.rtx_ssrc = rtx_ssrcs[0];

    auto pred = [&config](uint32_t ssrc) {
      return ssrc == config->rtp.rtx_ssrc;
    };
    if (!std::all_of(rtx_ssrcs.cbegin(), rtx_ssrcs.cend(), pred)) {
      LOG(LS_WARNING) << "RtcEventLog protobuf contained different SSRCs for "
                         "different received RTX payload types. Will only use "
                         "rtx_ssrc = "
                      << config->rtp.rtx_ssrc << ".";
    }
  }
  // Get header extensions.
  GetHeaderExtensions(&config->rtp.extensions,
                      receiver_config.header_extensions());
  // Get decoders.
  config->decoders.clear();
  for (int i = 0; i < receiver_config.decoders_size(); i++) {
    RTC_CHECK(receiver_config.decoders(i).has_name());
    RTC_CHECK(receiver_config.decoders(i).has_payload_type());
    VideoReceiveStream::Decoder decoder;
    decoder.payload_name = receiver_config.decoders(i).name();
    decoder.payload_type = receiver_config.decoders(i).payload_type();
    config->decoders.push_back(decoder);
  }
}

void ParsedRtcEventLog::GetVideoSendConfig(
    size_t index,
    VideoSendStream::Config* config) const {
  RTC_CHECK_LT(index, GetNumberOfEvents());
  const rtclog::Event& event = events_[index];
  RTC_CHECK(config != nullptr);
  RTC_CHECK(event.has_type());
  RTC_CHECK_EQ(event.type(), rtclog::Event::VIDEO_SENDER_CONFIG_EVENT);
  RTC_CHECK(event.has_video_sender_config());
  const rtclog::VideoSendConfig& sender_config = event.video_sender_config();
  // Get SSRCs.
  config->rtp.ssrcs.clear();
  for (int i = 0; i < sender_config.ssrcs_size(); i++) {
    config->rtp.ssrcs.push_back(sender_config.ssrcs(i));
  }
  // Get header extensions.
  GetHeaderExtensions(&config->rtp.extensions,
                      sender_config.header_extensions());
  // Get RTX settings.
  config->rtp.rtx.ssrcs.clear();
  for (int i = 0; i < sender_config.rtx_ssrcs_size(); i++) {
    config->rtp.rtx.ssrcs.push_back(sender_config.rtx_ssrcs(i));
  }
  if (sender_config.rtx_ssrcs_size() > 0) {
    RTC_CHECK(sender_config.has_rtx_payload_type());
    config->rtp.rtx.payload_type = sender_config.rtx_payload_type();
  } else {
    // Reset RTX payload type default value if no RTX SSRCs are used.
    config->rtp.rtx.payload_type = -1;
  }
  // Get encoder.
  RTC_CHECK(sender_config.has_encoder());
  RTC_CHECK(sender_config.encoder().has_name());
  RTC_CHECK(sender_config.encoder().has_payload_type());
  config->encoder_settings.payload_name = sender_config.encoder().name();
  config->encoder_settings.payload_type =
      sender_config.encoder().payload_type();
}

void ParsedRtcEventLog::GetAudioReceiveConfig(
    size_t index,
    AudioReceiveStream::Config* config) const {
  RTC_CHECK_LT(index, GetNumberOfEvents());
  const rtclog::Event& event = events_[index];
  RTC_CHECK(config != nullptr);
  RTC_CHECK(event.has_type());
  RTC_CHECK_EQ(event.type(), rtclog::Event::AUDIO_RECEIVER_CONFIG_EVENT);
  RTC_CHECK(event.has_audio_receiver_config());
  const rtclog::AudioReceiveConfig& receiver_config =
      event.audio_receiver_config();
  // Get SSRCs.
  RTC_CHECK(receiver_config.has_remote_ssrc());
  config->rtp.remote_ssrc = receiver_config.remote_ssrc();
  RTC_CHECK(receiver_config.has_local_ssrc());
  config->rtp.local_ssrc = receiver_config.local_ssrc();
  // Get header extensions.
  GetHeaderExtensions(&config->rtp.extensions,
                      receiver_config.header_extensions());
}

void ParsedRtcEventLog::GetAudioSendConfig(
    size_t index,
    AudioSendStream::Config* config) const {
  RTC_CHECK_LT(index, GetNumberOfEvents());
  const rtclog::Event& event = events_[index];
  RTC_CHECK(config != nullptr);
  RTC_CHECK(event.has_type());
  RTC_CHECK_EQ(event.type(), rtclog::Event::AUDIO_SENDER_CONFIG_EVENT);
  RTC_CHECK(event.has_audio_sender_config());
  const rtclog::AudioSendConfig& sender_config = event.audio_sender_config();
  // Get SSRCs.
  RTC_CHECK(sender_config.has_ssrc());
  config->rtp.ssrc = sender_config.ssrc();
  // Get header extensions.
  GetHeaderExtensions(&config->rtp.extensions,
                      sender_config.header_extensions());
}

void ParsedRtcEventLog::GetAudioPlayout(size_t index, uint32_t* ssrc) const {
  RTC_CHECK_LT(index, GetNumberOfEvents());
  const rtclog::Event& event = events_[index];
  RTC_CHECK(event.has_type());
  RTC_CHECK_EQ(event.type(), rtclog::Event::AUDIO_PLAYOUT_EVENT);
  RTC_CHECK(event.has_audio_playout_event());
  const rtclog::AudioPlayoutEvent& loss_event = event.audio_playout_event();
  RTC_CHECK(loss_event.has_local_ssrc());
  if (ssrc != nullptr) {
    *ssrc = loss_event.local_ssrc();
  }
}

void ParsedRtcEventLog::GetLossBasedBweUpdate(size_t index,
                                              int32_t* bitrate_bps,
                                              uint8_t* fraction_loss,
                                              int32_t* total_packets) const {
  RTC_CHECK_LT(index, GetNumberOfEvents());
  const rtclog::Event& event = events_[index];
  RTC_CHECK(event.has_type());
  RTC_CHECK_EQ(event.type(), rtclog::Event::LOSS_BASED_BWE_UPDATE);
  RTC_CHECK(event.has_loss_based_bwe_update());
  const rtclog::LossBasedBweUpdate& loss_event = event.loss_based_bwe_update();
  RTC_CHECK(loss_event.has_bitrate_bps());
  if (bitrate_bps != nullptr) {
    *bitrate_bps = loss_event.bitrate_bps();
  }
  RTC_CHECK(loss_event.has_fraction_loss());
  if (fraction_loss != nullptr) {
    *fraction_loss = loss_event.fraction_loss();
  }
  RTC_CHECK(loss_event.has_total_packets());
  if (total_packets != nullptr) {
    *total_packets = loss_event.total_packets();
  }
}

void ParsedRtcEventLog::GetDelayBasedBweUpdate(
    size_t index,
    int32_t* bitrate_bps,
    BandwidthUsage* detector_state) const {
  RTC_CHECK_LT(index, GetNumberOfEvents());
  const rtclog::Event& event = events_[index];
  RTC_CHECK(event.has_type());
  RTC_CHECK_EQ(event.type(), rtclog::Event::DELAY_BASED_BWE_UPDATE);
  RTC_CHECK(event.has_delay_based_bwe_update());
  const rtclog::DelayBasedBweUpdate& delay_event =
      event.delay_based_bwe_update();
  RTC_CHECK(delay_event.has_bitrate_bps());
  if (bitrate_bps != nullptr) {
    *bitrate_bps = delay_event.bitrate_bps();
  }
  RTC_CHECK(delay_event.has_detector_state());
  if (detector_state != nullptr) {
    *detector_state = GetRuntimeDetectorState(delay_event.detector_state());
  }
}

void ParsedRtcEventLog::GetAudioNetworkAdaptation(
    size_t index,
    AudioNetworkAdaptor::EncoderRuntimeConfig* config) const {
  RTC_CHECK_LT(index, GetNumberOfEvents());
  const rtclog::Event& event = events_[index];
  RTC_CHECK(event.has_type());
  RTC_CHECK_EQ(event.type(), rtclog::Event::AUDIO_NETWORK_ADAPTATION_EVENT);
  RTC_CHECK(event.has_audio_network_adaptation());
  const rtclog::AudioNetworkAdaptation& ana_event =
      event.audio_network_adaptation();
  if (ana_event.has_bitrate_bps())
    config->bitrate_bps = rtc::Optional<int>(ana_event.bitrate_bps());
  if (ana_event.has_enable_fec())
    config->enable_fec = rtc::Optional<bool>(ana_event.enable_fec());
  if (ana_event.has_enable_dtx())
    config->enable_dtx = rtc::Optional<bool>(ana_event.enable_dtx());
  if (ana_event.has_frame_length_ms())
    config->frame_length_ms = rtc::Optional<int>(ana_event.frame_length_ms());
  if (ana_event.has_num_channels())
    config->num_channels = rtc::Optional<size_t>(ana_event.num_channels());
  if (ana_event.has_uplink_packet_loss_fraction())
    config->uplink_packet_loss_fraction =
        rtc::Optional<float>(ana_event.uplink_packet_loss_fraction());
}

}  // namespace webrtc
