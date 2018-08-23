/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "logging/rtc_event_log/rtc_event_log_parser.h"

#include <stdint.h>
#include <string.h>

#include <algorithm>
#include <fstream>
#include <istream>
#include <map>
#include <utility>

#include "logging/rtc_event_log/rtc_event_log.h"
#include "modules/audio_coding/audio_network_adaptor/include/audio_network_adaptor.h"
#include "modules/remote_bitrate_estimator/include/bwe_defines.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/protobuf_utils.h"

namespace webrtc {

namespace {
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
    case rtclog::Event::ALR_STATE_EVENT:
      return ParsedRtcEventLog::EventType::ALR_STATE_EVENT;
    case rtclog::Event::ICE_CANDIDATE_PAIR_CONFIG:
      return ParsedRtcEventLog::EventType::ICE_CANDIDATE_PAIR_CONFIG;
    case rtclog::Event::ICE_CANDIDATE_PAIR_EVENT:
      return ParsedRtcEventLog::EventType::ICE_CANDIDATE_PAIR_EVENT;
  }
  return ParsedRtcEventLog::EventType::UNKNOWN_EVENT;
}

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

IceCandidatePairConfigType GetRuntimeIceCandidatePairConfigType(
    rtclog::IceCandidatePairConfig::IceCandidatePairConfigType type) {
  switch (type) {
    case rtclog::IceCandidatePairConfig::ADDED:
      return IceCandidatePairConfigType::kAdded;
    case rtclog::IceCandidatePairConfig::UPDATED:
      return IceCandidatePairConfigType::kUpdated;
    case rtclog::IceCandidatePairConfig::DESTROYED:
      return IceCandidatePairConfigType::kDestroyed;
    case rtclog::IceCandidatePairConfig::SELECTED:
      return IceCandidatePairConfigType::kSelected;
  }
  RTC_NOTREACHED();
  return IceCandidatePairConfigType::kAdded;
}

IceCandidateType GetRuntimeIceCandidateType(
    rtclog::IceCandidatePairConfig::IceCandidateType type) {
  switch (type) {
    case rtclog::IceCandidatePairConfig::LOCAL:
      return IceCandidateType::kLocal;
    case rtclog::IceCandidatePairConfig::STUN:
      return IceCandidateType::kStun;
    case rtclog::IceCandidatePairConfig::PRFLX:
      return IceCandidateType::kPrflx;
    case rtclog::IceCandidatePairConfig::RELAY:
      return IceCandidateType::kRelay;
    case rtclog::IceCandidatePairConfig::UNKNOWN_CANDIDATE_TYPE:
      return IceCandidateType::kUnknown;
  }
  RTC_NOTREACHED();
  return IceCandidateType::kUnknown;
}

IceCandidatePairProtocol GetRuntimeIceCandidatePairProtocol(
    rtclog::IceCandidatePairConfig::Protocol protocol) {
  switch (protocol) {
    case rtclog::IceCandidatePairConfig::UDP:
      return IceCandidatePairProtocol::kUdp;
    case rtclog::IceCandidatePairConfig::TCP:
      return IceCandidatePairProtocol::kTcp;
    case rtclog::IceCandidatePairConfig::SSLTCP:
      return IceCandidatePairProtocol::kSsltcp;
    case rtclog::IceCandidatePairConfig::TLS:
      return IceCandidatePairProtocol::kTls;
    case rtclog::IceCandidatePairConfig::UNKNOWN_PROTOCOL:
      return IceCandidatePairProtocol::kUnknown;
  }
  RTC_NOTREACHED();
  return IceCandidatePairProtocol::kUnknown;
}

IceCandidatePairAddressFamily GetRuntimeIceCandidatePairAddressFamily(
    rtclog::IceCandidatePairConfig::AddressFamily address_family) {
  switch (address_family) {
    case rtclog::IceCandidatePairConfig::IPV4:
      return IceCandidatePairAddressFamily::kIpv4;
    case rtclog::IceCandidatePairConfig::IPV6:
      return IceCandidatePairAddressFamily::kIpv6;
    case rtclog::IceCandidatePairConfig::UNKNOWN_ADDRESS_FAMILY:
      return IceCandidatePairAddressFamily::kUnknown;
  }
  RTC_NOTREACHED();
  return IceCandidatePairAddressFamily::kUnknown;
}

IceCandidateNetworkType GetRuntimeIceCandidateNetworkType(
    rtclog::IceCandidatePairConfig::NetworkType network_type) {
  switch (network_type) {
    case rtclog::IceCandidatePairConfig::ETHERNET:
      return IceCandidateNetworkType::kEthernet;
    case rtclog::IceCandidatePairConfig::LOOPBACK:
      return IceCandidateNetworkType::kLoopback;
    case rtclog::IceCandidatePairConfig::WIFI:
      return IceCandidateNetworkType::kWifi;
    case rtclog::IceCandidatePairConfig::VPN:
      return IceCandidateNetworkType::kVpn;
    case rtclog::IceCandidatePairConfig::CELLULAR:
      return IceCandidateNetworkType::kCellular;
    case rtclog::IceCandidatePairConfig::UNKNOWN_NETWORK_TYPE:
      return IceCandidateNetworkType::kUnknown;
  }
  RTC_NOTREACHED();
  return IceCandidateNetworkType::kUnknown;
}

IceCandidatePairEventType GetRuntimeIceCandidatePairEventType(
    rtclog::IceCandidatePairEvent::IceCandidatePairEventType type) {
  switch (type) {
    case rtclog::IceCandidatePairEvent::CHECK_SENT:
      return IceCandidatePairEventType::kCheckSent;
    case rtclog::IceCandidatePairEvent::CHECK_RECEIVED:
      return IceCandidatePairEventType::kCheckReceived;
    case rtclog::IceCandidatePairEvent::CHECK_RESPONSE_SENT:
      return IceCandidatePairEventType::kCheckResponseSent;
    case rtclog::IceCandidatePairEvent::CHECK_RESPONSE_RECEIVED:
      return IceCandidatePairEventType::kCheckResponseReceived;
  }
  RTC_NOTREACHED();
  return IceCandidatePairEventType::kCheckSent;
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
    RTC_DCHECK_GE(byte, 0);
    RTC_DCHECK_LE(byte, 255);
    varint |= static_cast<uint64_t>(byte & 0x7F) << (7 * bytes_read);
    if ((byte & 0x80) == 0) {
      return std::make_pair(varint, true);
    }
  }
  return std::make_pair(varint, false);
}

void GetHeaderExtensions(std::vector<RtpExtension>* header_extensions,
                         const RepeatedPtrField<rtclog::RtpHeaderExtension>&
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
    RTC_LOG(LS_WARNING) << "Could not open file for reading.";
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
      // Process all extensions maps for faster look-up later.
      for (auto& event_stream : streams_) {
        rtp_extensions_maps_[StreamId(event_stream.ssrc,
                                      event_stream.direction)] =
            &event_stream.rtp_extensions_map;
      }
      return true;
    }

    // Read the next message tag. The tag number is defined as
    // (fieldnumber << 3) | wire_type. In our case, the field number is
    // supposed to be 1 and the wire type for an
    // length-delimited field is 2.
    const uint64_t kExpectedTag = (1 << 3) | 2;
    std::tie(tag, success) = ParseVarInt(stream);
    if (!success) {
      RTC_LOG(LS_WARNING)
          << "Missing field tag from beginning of protobuf event.";
      return false;
    } else if (tag != kExpectedTag) {
      RTC_LOG(LS_WARNING)
          << "Unexpected field tag at beginning of protobuf event.";
      return false;
    }

    // Read the length field.
    std::tie(message_length, success) = ParseVarInt(stream);
    if (!success) {
      RTC_LOG(LS_WARNING) << "Missing message length after protobuf field tag.";
      return false;
    } else if (message_length > kMaxEventSize) {
      RTC_LOG(LS_WARNING) << "Protobuf message length is too large.";
      return false;
    }

    // Read the next protobuf event to a temporary char buffer.
    stream.read(tmp_buffer.data(), message_length);
    if (stream.gcount() != static_cast<int>(message_length)) {
      RTC_LOG(LS_WARNING) << "Failed to read protobuf message from file.";
      return false;
    }

    // Parse the protobuf event from the buffer.
    rtclog::Event event;
    if (!event.ParseFromArray(tmp_buffer.data(), message_length)) {
      RTC_LOG(LS_WARNING) << "Failed to parse protobuf message.";
      return false;
    }

    EventType type = GetRuntimeEventType(event.type());
    switch (type) {
      case VIDEO_RECEIVER_CONFIG_EVENT: {
        rtclog::StreamConfig config = GetVideoReceiveConfig(event);
        streams_.emplace_back(config.remote_ssrc, MediaType::VIDEO,
                              kIncomingPacket,
                              RtpHeaderExtensionMap(config.rtp_extensions));
        streams_.emplace_back(config.local_ssrc, MediaType::VIDEO,
                              kOutgoingPacket,
                              RtpHeaderExtensionMap(config.rtp_extensions));
        break;
      }
      case VIDEO_SENDER_CONFIG_EVENT: {
        std::vector<rtclog::StreamConfig> configs = GetVideoSendConfig(event);
        for (size_t i = 0; i < configs.size(); i++) {
          streams_.emplace_back(
              configs[i].local_ssrc, MediaType::VIDEO, kOutgoingPacket,
              RtpHeaderExtensionMap(configs[i].rtp_extensions));

          streams_.emplace_back(
              configs[i].rtx_ssrc, MediaType::VIDEO, kOutgoingPacket,
              RtpHeaderExtensionMap(configs[i].rtp_extensions));
        }
        break;
      }
      case AUDIO_RECEIVER_CONFIG_EVENT: {
        rtclog::StreamConfig config = GetAudioReceiveConfig(event);
        streams_.emplace_back(config.remote_ssrc, MediaType::AUDIO,
                              kIncomingPacket,
                              RtpHeaderExtensionMap(config.rtp_extensions));
        streams_.emplace_back(config.local_ssrc, MediaType::AUDIO,
                              kOutgoingPacket,
                              RtpHeaderExtensionMap(config.rtp_extensions));
        break;
      }
      case AUDIO_SENDER_CONFIG_EVENT: {
        rtclog::StreamConfig config = GetAudioSendConfig(event);
        streams_.emplace_back(config.local_ssrc, MediaType::AUDIO,
                              kOutgoingPacket,
                              RtpHeaderExtensionMap(config.rtp_extensions));
        break;
      }
      default:
        break;
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
webrtc::RtpHeaderExtensionMap* ParsedRtcEventLog::GetRtpHeader(
    size_t index,
    PacketDirection* incoming,
    uint8_t* header,
    size_t* header_length,
    size_t* total_length,
    int* probe_cluster_id) const {
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
    uint32_t ssrc = ByteReader<uint32_t>::ReadBigEndian(header + 8);
    StreamId stream_id(
        ssrc, rtp_packet.incoming() ? kIncomingPacket : kOutgoingPacket);
    auto it = rtp_extensions_maps_.find(stream_id);
    if (it != rtp_extensions_maps_.end()) {
      return it->second;
    }
  }
  if (probe_cluster_id != nullptr) {
    if (rtp_packet.has_probe_cluster_id()) {
      *probe_cluster_id = rtp_packet.probe_cluster_id();
      RTC_CHECK_NE(*probe_cluster_id, PacedPacketInfo::kNotAProbe);
    } else {
      *probe_cluster_id = PacedPacketInfo::kNotAProbe;
    }
  }
  return nullptr;
}

// The packet must have space for at least IP_PACKET_SIZE bytes.
void ParsedRtcEventLog::GetRtcpPacket(size_t index,
                                      PacketDirection* incoming,
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

rtclog::StreamConfig ParsedRtcEventLog::GetVideoReceiveConfig(
    size_t index) const {
  RTC_CHECK_LT(index, GetNumberOfEvents());
  return GetVideoReceiveConfig(events_[index]);
}

rtclog::StreamConfig ParsedRtcEventLog::GetVideoReceiveConfig(
    const rtclog::Event& event) const {
  rtclog::StreamConfig config;
  RTC_CHECK(event.has_type());
  RTC_CHECK_EQ(event.type(), rtclog::Event::VIDEO_RECEIVER_CONFIG_EVENT);
  RTC_CHECK(event.has_video_receiver_config());
  const rtclog::VideoReceiveConfig& receiver_config =
      event.video_receiver_config();
  // Get SSRCs.
  RTC_CHECK(receiver_config.has_remote_ssrc());
  config.remote_ssrc = receiver_config.remote_ssrc();
  RTC_CHECK(receiver_config.has_local_ssrc());
  config.local_ssrc = receiver_config.local_ssrc();
  config.rtx_ssrc = 0;
  // Get RTCP settings.
  RTC_CHECK(receiver_config.has_rtcp_mode());
  config.rtcp_mode = GetRuntimeRtcpMode(receiver_config.rtcp_mode());
  RTC_CHECK(receiver_config.has_remb());
  config.remb = receiver_config.remb();

  // Get RTX map.
  std::map<uint32_t, const rtclog::RtxConfig> rtx_map;
  for (int i = 0; i < receiver_config.rtx_map_size(); i++) {
    const rtclog::RtxMap& map = receiver_config.rtx_map(i);
    RTC_CHECK(map.has_payload_type());
    RTC_CHECK(map.has_config());
    RTC_CHECK(map.config().has_rtx_ssrc());
    RTC_CHECK(map.config().has_rtx_payload_type());
    rtx_map.insert(std::make_pair(map.payload_type(), map.config()));
  }

  // Get header extensions.
  GetHeaderExtensions(&config.rtp_extensions,
                      receiver_config.header_extensions());
  // Get decoders.
  config.codecs.clear();
  for (int i = 0; i < receiver_config.decoders_size(); i++) {
    RTC_CHECK(receiver_config.decoders(i).has_name());
    RTC_CHECK(receiver_config.decoders(i).has_payload_type());
    int rtx_payload_type = 0;
    auto rtx_it = rtx_map.find(receiver_config.decoders(i).payload_type());
    if (rtx_it != rtx_map.end()) {
      rtx_payload_type = rtx_it->second.rtx_payload_type();
      if (config.rtx_ssrc != 0 &&
          config.rtx_ssrc != rtx_it->second.rtx_ssrc()) {
        RTC_LOG(LS_WARNING)
            << "RtcEventLog protobuf contained different SSRCs for "
               "different received RTX payload types. Will only use "
               "rtx_ssrc = "
            << config.rtx_ssrc << ".";
      } else {
        config.rtx_ssrc = rtx_it->second.rtx_ssrc();
      }
    }
    config.codecs.emplace_back(receiver_config.decoders(i).name(),
                               receiver_config.decoders(i).payload_type(),
                               rtx_payload_type);
  }
  return config;
}

std::vector<rtclog::StreamConfig> ParsedRtcEventLog::GetVideoSendConfig(
    size_t index) const {
  RTC_CHECK_LT(index, GetNumberOfEvents());
  return GetVideoSendConfig(events_[index]);
}

std::vector<rtclog::StreamConfig> ParsedRtcEventLog::GetVideoSendConfig(
    const rtclog::Event& event) const {
  std::vector<rtclog::StreamConfig> configs;
  RTC_CHECK(event.has_type());
  RTC_CHECK_EQ(event.type(), rtclog::Event::VIDEO_SENDER_CONFIG_EVENT);
  RTC_CHECK(event.has_video_sender_config());
  const rtclog::VideoSendConfig& sender_config = event.video_sender_config();
  if (sender_config.rtx_ssrcs_size() > 0 &&
      sender_config.ssrcs_size() != sender_config.rtx_ssrcs_size()) {
    RTC_LOG(WARNING)
        << "VideoSendConfig is configured for RTX but the number of "
           "SSRCs doesn't match the number of RTX SSRCs.";
  }
  configs.resize(sender_config.ssrcs_size());
  for (int i = 0; i < sender_config.ssrcs_size(); i++) {
    // Get SSRCs.
    configs[i].local_ssrc = sender_config.ssrcs(i);
    if (sender_config.rtx_ssrcs_size() > 0 &&
        i < sender_config.rtx_ssrcs_size()) {
      RTC_CHECK(sender_config.has_rtx_payload_type());
      configs[i].rtx_ssrc = sender_config.rtx_ssrcs(i);
    }
    // Get header extensions.
    GetHeaderExtensions(&configs[i].rtp_extensions,
                        sender_config.header_extensions());

    // Get the codec.
    RTC_CHECK(sender_config.has_encoder());
    RTC_CHECK(sender_config.encoder().has_name());
    RTC_CHECK(sender_config.encoder().has_payload_type());
    configs[i].codecs.emplace_back(
        sender_config.encoder().name(), sender_config.encoder().payload_type(),
        sender_config.has_rtx_payload_type() ? sender_config.rtx_payload_type()
                                             : 0);
  }
  return configs;
}

rtclog::StreamConfig ParsedRtcEventLog::GetAudioReceiveConfig(
    size_t index) const {
  RTC_CHECK_LT(index, GetNumberOfEvents());
  return GetAudioReceiveConfig(events_[index]);
}

rtclog::StreamConfig ParsedRtcEventLog::GetAudioReceiveConfig(
    const rtclog::Event& event) const {
  rtclog::StreamConfig config;
  RTC_CHECK(event.has_type());
  RTC_CHECK_EQ(event.type(), rtclog::Event::AUDIO_RECEIVER_CONFIG_EVENT);
  RTC_CHECK(event.has_audio_receiver_config());
  const rtclog::AudioReceiveConfig& receiver_config =
      event.audio_receiver_config();
  // Get SSRCs.
  RTC_CHECK(receiver_config.has_remote_ssrc());
  config.remote_ssrc = receiver_config.remote_ssrc();
  RTC_CHECK(receiver_config.has_local_ssrc());
  config.local_ssrc = receiver_config.local_ssrc();
  // Get header extensions.
  GetHeaderExtensions(&config.rtp_extensions,
                      receiver_config.header_extensions());
  return config;
}

rtclog::StreamConfig ParsedRtcEventLog::GetAudioSendConfig(size_t index) const {
  RTC_CHECK_LT(index, GetNumberOfEvents());
  return GetAudioSendConfig(events_[index]);
}

rtclog::StreamConfig ParsedRtcEventLog::GetAudioSendConfig(
    const rtclog::Event& event) const {
  rtclog::StreamConfig config;
  RTC_CHECK(event.has_type());
  RTC_CHECK_EQ(event.type(), rtclog::Event::AUDIO_SENDER_CONFIG_EVENT);
  RTC_CHECK(event.has_audio_sender_config());
  const rtclog::AudioSendConfig& sender_config = event.audio_sender_config();
  // Get SSRCs.
  RTC_CHECK(sender_config.has_ssrc());
  config.local_ssrc = sender_config.ssrc();
  // Get header extensions.
  GetHeaderExtensions(&config.rtp_extensions,
                      sender_config.header_extensions());
  return config;
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

ParsedRtcEventLog::BweDelayBasedUpdate
ParsedRtcEventLog::GetDelayBasedBweUpdate(size_t index) const {
  RTC_CHECK_LT(index, GetNumberOfEvents());
  const rtclog::Event& event = events_[index];
  RTC_CHECK(event.has_type());
  RTC_CHECK_EQ(event.type(), rtclog::Event::DELAY_BASED_BWE_UPDATE);
  RTC_CHECK(event.has_delay_based_bwe_update());
  const rtclog::DelayBasedBweUpdate& delay_event =
      event.delay_based_bwe_update();

  BweDelayBasedUpdate res;
  res.timestamp = GetTimestamp(index);
  RTC_CHECK(delay_event.has_bitrate_bps());
  res.bitrate_bps = delay_event.bitrate_bps();
  RTC_CHECK(delay_event.has_detector_state());
  res.detector_state = GetRuntimeDetectorState(delay_event.detector_state());
  return res;
}

void ParsedRtcEventLog::GetAudioNetworkAdaptation(
    size_t index,
    AudioEncoderRuntimeConfig* config) const {
  RTC_CHECK_LT(index, GetNumberOfEvents());
  const rtclog::Event& event = events_[index];
  RTC_CHECK(event.has_type());
  RTC_CHECK_EQ(event.type(), rtclog::Event::AUDIO_NETWORK_ADAPTATION_EVENT);
  RTC_CHECK(event.has_audio_network_adaptation());
  const rtclog::AudioNetworkAdaptation& ana_event =
      event.audio_network_adaptation();
  if (ana_event.has_bitrate_bps())
    config->bitrate_bps = ana_event.bitrate_bps();
  if (ana_event.has_enable_fec())
    config->enable_fec = ana_event.enable_fec();
  if (ana_event.has_enable_dtx())
    config->enable_dtx = ana_event.enable_dtx();
  if (ana_event.has_frame_length_ms())
    config->frame_length_ms = ana_event.frame_length_ms();
  if (ana_event.has_num_channels())
    config->num_channels = ana_event.num_channels();
  if (ana_event.has_uplink_packet_loss_fraction())
    config->uplink_packet_loss_fraction =
        ana_event.uplink_packet_loss_fraction();
}

ParsedRtcEventLog::BweProbeClusterCreatedEvent
ParsedRtcEventLog::GetBweProbeClusterCreated(size_t index) const {
  RTC_CHECK_LT(index, GetNumberOfEvents());
  const rtclog::Event& event = events_[index];
  RTC_CHECK(event.has_type());
  RTC_CHECK_EQ(event.type(), rtclog::Event::BWE_PROBE_CLUSTER_CREATED_EVENT);
  RTC_CHECK(event.has_probe_cluster());
  const rtclog::BweProbeCluster& pcc_event = event.probe_cluster();
  BweProbeClusterCreatedEvent res;
  res.timestamp = GetTimestamp(index);
  RTC_CHECK(pcc_event.has_id());
  res.id = pcc_event.id();
  RTC_CHECK(pcc_event.has_bitrate_bps());
  res.bitrate_bps = pcc_event.bitrate_bps();
  RTC_CHECK(pcc_event.has_min_packets());
  res.min_packets = pcc_event.min_packets();
  RTC_CHECK(pcc_event.has_min_bytes());
  res.min_bytes = pcc_event.min_bytes();
  return res;
}

ParsedRtcEventLog::BweProbeResultEvent ParsedRtcEventLog::GetBweProbeResult(
    size_t index) const {
  RTC_CHECK_LT(index, GetNumberOfEvents());
  const rtclog::Event& event = events_[index];
  RTC_CHECK(event.has_type());
  RTC_CHECK_EQ(event.type(), rtclog::Event::BWE_PROBE_RESULT_EVENT);
  RTC_CHECK(event.has_probe_result());
  const rtclog::BweProbeResult& pr_event = event.probe_result();
  BweProbeResultEvent res;
  res.timestamp = GetTimestamp(index);
  RTC_CHECK(pr_event.has_id());
  res.id = pr_event.id();

  RTC_CHECK(pr_event.has_result());
  if (pr_event.result() == rtclog::BweProbeResult::SUCCESS) {
    RTC_CHECK(pr_event.has_bitrate_bps());
    res.bitrate_bps = pr_event.bitrate_bps();
  } else if (pr_event.result() ==
             rtclog::BweProbeResult::INVALID_SEND_RECEIVE_INTERVAL) {
    res.failure_reason = ProbeFailureReason::kInvalidSendReceiveInterval;
  } else if (pr_event.result() ==
             rtclog::BweProbeResult::INVALID_SEND_RECEIVE_RATIO) {
    res.failure_reason = ProbeFailureReason::kInvalidSendReceiveRatio;
  } else if (pr_event.result() == rtclog::BweProbeResult::TIMEOUT) {
    res.failure_reason = ProbeFailureReason::kTimeout;
  } else {
    RTC_NOTREACHED();
  }

  return res;
}

ParsedRtcEventLog::AlrStateEvent ParsedRtcEventLog::GetAlrState(
    size_t index) const {
  RTC_CHECK_LT(index, GetNumberOfEvents());
  const rtclog::Event& event = events_[index];
  RTC_CHECK(event.has_type());
  RTC_CHECK_EQ(event.type(), rtclog::Event::ALR_STATE_EVENT);
  RTC_CHECK(event.has_alr_state());
  const rtclog::AlrState& alr_event = event.alr_state();
  AlrStateEvent res;
  res.timestamp = GetTimestamp(index);
  RTC_CHECK(alr_event.has_in_alr());
  res.in_alr = alr_event.in_alr();

  return res;
}

ParsedRtcEventLog::IceCandidatePairConfig
ParsedRtcEventLog::GetIceCandidatePairConfig(size_t index) const {
  RTC_CHECK_LT(index, GetNumberOfEvents());
  const rtclog::Event& rtc_event = events_[index];
  RTC_CHECK(rtc_event.has_type());
  RTC_CHECK_EQ(rtc_event.type(), rtclog::Event::ICE_CANDIDATE_PAIR_CONFIG);
  IceCandidatePairConfig res;
  const rtclog::IceCandidatePairConfig& config =
      rtc_event.ice_candidate_pair_config();
  res.timestamp = GetTimestamp(index);
  RTC_CHECK(config.has_config_type());
  res.type = GetRuntimeIceCandidatePairConfigType(config.config_type());
  RTC_CHECK(config.has_candidate_pair_id());
  res.candidate_pair_id = config.candidate_pair_id();
  RTC_CHECK(config.has_local_candidate_type());
  res.local_candidate_type =
      GetRuntimeIceCandidateType(config.local_candidate_type());
  RTC_CHECK(config.has_local_relay_protocol());
  res.local_relay_protocol =
      GetRuntimeIceCandidatePairProtocol(config.local_relay_protocol());
  RTC_CHECK(config.has_local_network_type());
  res.local_network_type =
      GetRuntimeIceCandidateNetworkType(config.local_network_type());
  RTC_CHECK(config.has_local_address_family());
  res.local_address_family =
      GetRuntimeIceCandidatePairAddressFamily(config.local_address_family());
  RTC_CHECK(config.has_remote_candidate_type());
  res.remote_candidate_type =
      GetRuntimeIceCandidateType(config.remote_candidate_type());
  RTC_CHECK(config.has_remote_address_family());
  res.remote_address_family =
      GetRuntimeIceCandidatePairAddressFamily(config.remote_address_family());
  RTC_CHECK(config.has_candidate_pair_protocol());
  res.candidate_pair_protocol =
      GetRuntimeIceCandidatePairProtocol(config.candidate_pair_protocol());
  return res;
}

ParsedRtcEventLog::IceCandidatePairEvent
ParsedRtcEventLog::GetIceCandidatePairEvent(size_t index) const {
  RTC_CHECK_LT(index, GetNumberOfEvents());
  const rtclog::Event& rtc_event = events_[index];
  RTC_CHECK(rtc_event.has_type());
  RTC_CHECK_EQ(rtc_event.type(), rtclog::Event::ICE_CANDIDATE_PAIR_EVENT);
  IceCandidatePairEvent res;
  const rtclog::IceCandidatePairEvent& event =
      rtc_event.ice_candidate_pair_event();
  res.timestamp = GetTimestamp(index);
  RTC_CHECK(event.has_event_type());
  res.type = GetRuntimeIceCandidatePairEventType(event.event_type());
  RTC_CHECK(event.has_candidate_pair_id());
  res.candidate_pair_id = event.candidate_pair_id();
  return res;
}

// Returns the MediaType for registered SSRCs. Search from the end to use last
// registered types first.
ParsedRtcEventLog::MediaType ParsedRtcEventLog::GetMediaType(
    uint32_t ssrc,
    PacketDirection direction) const {
  for (auto rit = streams_.rbegin(); rit != streams_.rend(); ++rit) {
    if (rit->ssrc == ssrc && rit->direction == direction)
      return rit->media_type;
  }
  return MediaType::ANY;
}
}  // namespace webrtc
