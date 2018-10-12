/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "logging/rtc_event_log/rtc_event_log_parser_new.h"

#include <stdint.h>
#include <string.h>

#include <algorithm>
#include <fstream>
#include <istream>  // no-presubmit-check TODO(webrtc:8982)
#include <limits>
#include <map>
#include <utility>

#include "absl/memory/memory.h"
#include "absl/types/optional.h"
#include "api/rtp_headers.h"
#include "api/rtpparameters.h"
#include "logging/rtc_event_log/rtc_event_log.h"
#include "modules/audio_coding/audio_network_adaptor/include/audio_network_adaptor.h"
#include "modules/congestion_controller/transport_feedback_adapter.h"
#include "modules/remote_bitrate_estimator/include/bwe_defines.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/byte_io.h"
#include "modules/rtp_rtcp/source/rtp_header_extensions.h"
#include "modules/rtp_rtcp/source/rtp_utility.h"
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

ParsedRtcEventLogNew::EventType GetRuntimeEventType(
    rtclog::Event::EventType event_type) {
  switch (event_type) {
    case rtclog::Event::UNKNOWN_EVENT:
      return ParsedRtcEventLogNew::EventType::UNKNOWN_EVENT;
    case rtclog::Event::LOG_START:
      return ParsedRtcEventLogNew::EventType::LOG_START;
    case rtclog::Event::LOG_END:
      return ParsedRtcEventLogNew::EventType::LOG_END;
    case rtclog::Event::RTP_EVENT:
      return ParsedRtcEventLogNew::EventType::RTP_EVENT;
    case rtclog::Event::RTCP_EVENT:
      return ParsedRtcEventLogNew::EventType::RTCP_EVENT;
    case rtclog::Event::AUDIO_PLAYOUT_EVENT:
      return ParsedRtcEventLogNew::EventType::AUDIO_PLAYOUT_EVENT;
    case rtclog::Event::LOSS_BASED_BWE_UPDATE:
      return ParsedRtcEventLogNew::EventType::LOSS_BASED_BWE_UPDATE;
    case rtclog::Event::DELAY_BASED_BWE_UPDATE:
      return ParsedRtcEventLogNew::EventType::DELAY_BASED_BWE_UPDATE;
    case rtclog::Event::VIDEO_RECEIVER_CONFIG_EVENT:
      return ParsedRtcEventLogNew::EventType::VIDEO_RECEIVER_CONFIG_EVENT;
    case rtclog::Event::VIDEO_SENDER_CONFIG_EVENT:
      return ParsedRtcEventLogNew::EventType::VIDEO_SENDER_CONFIG_EVENT;
    case rtclog::Event::AUDIO_RECEIVER_CONFIG_EVENT:
      return ParsedRtcEventLogNew::EventType::AUDIO_RECEIVER_CONFIG_EVENT;
    case rtclog::Event::AUDIO_SENDER_CONFIG_EVENT:
      return ParsedRtcEventLogNew::EventType::AUDIO_SENDER_CONFIG_EVENT;
    case rtclog::Event::AUDIO_NETWORK_ADAPTATION_EVENT:
      return ParsedRtcEventLogNew::EventType::AUDIO_NETWORK_ADAPTATION_EVENT;
    case rtclog::Event::BWE_PROBE_CLUSTER_CREATED_EVENT:
      return ParsedRtcEventLogNew::EventType::BWE_PROBE_CLUSTER_CREATED_EVENT;
    case rtclog::Event::BWE_PROBE_RESULT_EVENT:
      // Probe successes and failures are currently stored in the same proto
      // message, we are moving towards separate messages. Probe results
      // therefore need special treatment in the parser.
      return ParsedRtcEventLogNew::EventType::UNKNOWN_EVENT;
    case rtclog::Event::ALR_STATE_EVENT:
      return ParsedRtcEventLogNew::EventType::ALR_STATE_EVENT;
    case rtclog::Event::ICE_CANDIDATE_PAIR_CONFIG:
      return ParsedRtcEventLogNew::EventType::ICE_CANDIDATE_PAIR_CONFIG;
    case rtclog::Event::ICE_CANDIDATE_PAIR_EVENT:
      return ParsedRtcEventLogNew::EventType::ICE_CANDIDATE_PAIR_EVENT;
  }
  return ParsedRtcEventLogNew::EventType::UNKNOWN_EVENT;
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

// Reads a VarInt from |stream| and returns it. Also writes the read bytes to
// |buffer| starting |bytes_written| bytes into the buffer. |bytes_written| is
// incremented for each written byte.
absl::optional<uint64_t> ParseVarInt(
    std::istream& stream,  // no-presubmit-check TODO(webrtc:8982)
    char* buffer,
    size_t* bytes_written) {
  uint64_t varint = 0;
  for (size_t bytes_read = 0; bytes_read < 10; ++bytes_read) {
    // The most significant bit of each byte is 0 if it is the last byte in
    // the varint and 1 otherwise. Thus, we take the 7 least significant bits
    // of each byte and shift them 7 bits for each byte read previously to get
    // the (unsigned) integer.
    int byte = stream.get();
    if (stream.eof()) {
      return absl::nullopt;
    }
    RTC_DCHECK_GE(byte, 0);
    RTC_DCHECK_LE(byte, 255);
    varint |= static_cast<uint64_t>(byte & 0x7F) << (7 * bytes_read);
    buffer[*bytes_written] = byte;
    *bytes_written += 1;
    if ((byte & 0x80) == 0) {
      return varint;
    }
  }
  return absl::nullopt;
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

void SortPacketFeedbackVectorWithLoss(std::vector<PacketFeedback>* vec) {
  class LossHandlingPacketFeedbackComparator {
   public:
    inline bool operator()(const PacketFeedback& lhs,
                           const PacketFeedback& rhs) {
      if (lhs.arrival_time_ms != PacketFeedback::kNotReceived &&
          rhs.arrival_time_ms != PacketFeedback::kNotReceived &&
          lhs.arrival_time_ms != rhs.arrival_time_ms)
        return lhs.arrival_time_ms < rhs.arrival_time_ms;
      if (lhs.send_time_ms != rhs.send_time_ms)
        return lhs.send_time_ms < rhs.send_time_ms;
      return lhs.sequence_number < rhs.sequence_number;
    }
  };
  std::sort(vec->begin(), vec->end(), LossHandlingPacketFeedbackComparator());
}

}  // namespace

LoggedRtcpPacket::LoggedRtcpPacket(uint64_t timestamp_us,
                                   const uint8_t* packet,
                                   size_t total_length)
    : timestamp_us(timestamp_us), raw_data(packet, packet + total_length) {}
LoggedRtcpPacket::LoggedRtcpPacket(const LoggedRtcpPacket& rhs) = default;
LoggedRtcpPacket::~LoggedRtcpPacket() = default;

LoggedVideoSendConfig::LoggedVideoSendConfig(
    int64_t timestamp_us,
    const std::vector<rtclog::StreamConfig>& configs)
    : timestamp_us(timestamp_us), configs(configs) {}
LoggedVideoSendConfig::LoggedVideoSendConfig(const LoggedVideoSendConfig& rhs) =
    default;
LoggedVideoSendConfig::~LoggedVideoSendConfig() = default;

ParsedRtcEventLogNew::~ParsedRtcEventLogNew() = default;

ParsedRtcEventLogNew::LoggedRtpStreamIncoming::LoggedRtpStreamIncoming() =
    default;
ParsedRtcEventLogNew::LoggedRtpStreamIncoming::LoggedRtpStreamIncoming(
    const LoggedRtpStreamIncoming& rhs) = default;
ParsedRtcEventLogNew::LoggedRtpStreamIncoming::~LoggedRtpStreamIncoming() =
    default;

ParsedRtcEventLogNew::LoggedRtpStreamOutgoing::LoggedRtpStreamOutgoing() =
    default;
ParsedRtcEventLogNew::LoggedRtpStreamOutgoing::LoggedRtpStreamOutgoing(
    const LoggedRtpStreamOutgoing& rhs) = default;
ParsedRtcEventLogNew::LoggedRtpStreamOutgoing::~LoggedRtpStreamOutgoing() =
    default;

ParsedRtcEventLogNew::LoggedRtpStreamView::LoggedRtpStreamView(
    uint32_t ssrc,
    const LoggedRtpPacketIncoming* ptr,
    size_t num_elements)
    : ssrc(ssrc),
      packet_view(PacketView<const LoggedRtpPacket>::Create(
          ptr,
          num_elements,
          offsetof(LoggedRtpPacketIncoming, rtp))) {}

ParsedRtcEventLogNew::LoggedRtpStreamView::LoggedRtpStreamView(
    uint32_t ssrc,
    const LoggedRtpPacketOutgoing* ptr,
    size_t num_elements)
    : ssrc(ssrc),
      packet_view(PacketView<const LoggedRtpPacket>::Create(
          ptr,
          num_elements,
          offsetof(LoggedRtpPacketOutgoing, rtp))) {}

ParsedRtcEventLogNew::LoggedRtpStreamView::LoggedRtpStreamView(
    const LoggedRtpStreamView&) = default;

// Return default values for header extensions, to use on streams without stored
// mapping data. Currently this only applies to audio streams, since the mapping
// is not stored in the event log.
// TODO(ivoc): Remove this once this mapping is stored in the event log for
//             audio streams. Tracking bug: webrtc:6399
webrtc::RtpHeaderExtensionMap
ParsedRtcEventLogNew::GetDefaultHeaderExtensionMap() {
  webrtc::RtpHeaderExtensionMap default_map;
  default_map.Register<AudioLevel>(webrtc::RtpExtension::kAudioLevelDefaultId);
  default_map.Register<TransmissionOffset>(
      webrtc::RtpExtension::kTimestampOffsetDefaultId);
  default_map.Register<AbsoluteSendTime>(
      webrtc::RtpExtension::kAbsSendTimeDefaultId);
  default_map.Register<VideoOrientation>(
      webrtc::RtpExtension::kVideoRotationDefaultId);
  default_map.Register<VideoContentTypeExtension>(
      webrtc::RtpExtension::kVideoContentTypeDefaultId);
  default_map.Register<VideoTimingExtension>(
      webrtc::RtpExtension::kVideoTimingDefaultId);
  default_map.Register<TransportSequenceNumber>(
      webrtc::RtpExtension::kTransportSequenceNumberDefaultId);
  default_map.Register<PlayoutDelayLimits>(
      webrtc::RtpExtension::kPlayoutDelayDefaultId);
  return default_map;
}

ParsedRtcEventLogNew::ParsedRtcEventLogNew(
    UnconfiguredHeaderExtensions parse_unconfigured_header_extensions)
    : parse_unconfigured_header_extensions_(
          parse_unconfigured_header_extensions) {
  Clear();
}

void ParsedRtcEventLogNew::Clear() {
  events_.clear();
  default_extension_map_ = GetDefaultHeaderExtensionMap();

  incoming_rtx_ssrcs_.clear();
  incoming_video_ssrcs_.clear();
  incoming_audio_ssrcs_.clear();
  outgoing_rtx_ssrcs_.clear();
  outgoing_video_ssrcs_.clear();
  outgoing_audio_ssrcs_.clear();

  incoming_rtp_packets_map_.clear();
  outgoing_rtp_packets_map_.clear();
  incoming_rtp_packets_by_ssrc_.clear();
  outgoing_rtp_packets_by_ssrc_.clear();
  incoming_rtp_packet_views_by_ssrc_.clear();
  outgoing_rtp_packet_views_by_ssrc_.clear();

  incoming_rtcp_packets_.clear();
  outgoing_rtcp_packets_.clear();

  incoming_rr_.clear();
  outgoing_rr_.clear();
  incoming_sr_.clear();
  outgoing_sr_.clear();
  incoming_nack_.clear();
  outgoing_nack_.clear();
  incoming_remb_.clear();
  outgoing_remb_.clear();
  incoming_transport_feedback_.clear();
  outgoing_transport_feedback_.clear();

  start_log_events_.clear();
  stop_log_events_.clear();
  audio_playout_events_.clear();
  audio_network_adaptation_events_.clear();
  bwe_probe_cluster_created_events_.clear();
  bwe_probe_failure_events_.clear();
  bwe_probe_success_events_.clear();
  bwe_delay_updates_.clear();
  bwe_loss_updates_.clear();
  alr_state_events_.clear();
  ice_candidate_pair_configs_.clear();
  ice_candidate_pair_events_.clear();
  audio_recv_configs_.clear();
  audio_send_configs_.clear();
  video_recv_configs_.clear();
  video_send_configs_.clear();

  memset(last_incoming_rtcp_packet_, 0, IP_PACKET_SIZE);
  last_incoming_rtcp_packet_length_ = 0;

  first_timestamp_ = std::numeric_limits<int64_t>::max();
  last_timestamp_ = std::numeric_limits<int64_t>::min();

  incoming_rtp_extensions_maps_.clear();
  outgoing_rtp_extensions_maps_.clear();
}

bool ParsedRtcEventLogNew::ParseFile(const std::string& filename) {
  std::ifstream file(  // no-presubmit-check TODO(webrtc:8982)
      filename, std::ios_base::in | std::ios_base::binary);
  if (!file.good() || !file.is_open()) {
    RTC_LOG(LS_WARNING) << "Could not open file for reading.";
    return false;
  }

  return ParseStream(file);
}

bool ParsedRtcEventLogNew::ParseString(const std::string& s) {
  std::istringstream stream(  // no-presubmit-check TODO(webrtc:8982)
      s, std::ios_base::in | std::ios_base::binary);
  return ParseStream(stream);
}

bool ParsedRtcEventLogNew::ParseStream(
    std::istream& stream) {  // no-presubmit-check TODO(webrtc:8982)
  Clear();
  bool success = ParseStreamInternal(stream);

  // ParseStreamInternal stores the RTP packets in a map indexed by SSRC.
  // Since we dont need rapid lookup based on SSRC after parsing, we move the
  // packets_streams from map to vector.
  incoming_rtp_packets_by_ssrc_.reserve(incoming_rtp_packets_map_.size());
  for (const auto& kv : incoming_rtp_packets_map_) {
    incoming_rtp_packets_by_ssrc_.emplace_back(LoggedRtpStreamIncoming());
    incoming_rtp_packets_by_ssrc_.back().ssrc = kv.first;
    incoming_rtp_packets_by_ssrc_.back().incoming_packets =
        std::move(kv.second);
  }
  incoming_rtp_packets_map_.clear();
  outgoing_rtp_packets_by_ssrc_.reserve(outgoing_rtp_packets_map_.size());
  for (const auto& kv : outgoing_rtp_packets_map_) {
    outgoing_rtp_packets_by_ssrc_.emplace_back(LoggedRtpStreamOutgoing());
    outgoing_rtp_packets_by_ssrc_.back().ssrc = kv.first;
    outgoing_rtp_packets_by_ssrc_.back().outgoing_packets =
        std::move(kv.second);
  }
  outgoing_rtp_packets_map_.clear();

  // Build PacketViews for easier iteration over RTP packets
  for (const auto& stream : incoming_rtp_packets_by_ssrc_) {
    incoming_rtp_packet_views_by_ssrc_.emplace_back(
        LoggedRtpStreamView(stream.ssrc, stream.incoming_packets.data(),
                            stream.incoming_packets.size()));
  }
  for (const auto& stream : outgoing_rtp_packets_by_ssrc_) {
    outgoing_rtp_packet_views_by_ssrc_.emplace_back(
        LoggedRtpStreamView(stream.ssrc, stream.outgoing_packets.data(),
                            stream.outgoing_packets.size()));
  }

  return success;
}

bool ParsedRtcEventLogNew::ParseStreamInternal(
    std::istream& stream) {  // no-presubmit-check TODO(webrtc:8982)
  const size_t kMaxEventSize = (1u << 16) - 1;
  const size_t kMaxVarintSize = 10;
  std::vector<char> buffer(kMaxEventSize + 2 * kMaxVarintSize);

  RTC_DCHECK(stream.good());

  while (1) {
    // Check whether we have reached end of file.
    stream.peek();
    if (stream.eof()) {
      break;
    }

    // Read the next message tag. The tag number is defined as
    // (fieldnumber << 3) | wire_type. In our case, the field number is
    // supposed to be 1 and the wire type for a length-delimited field is 2.
    const uint64_t kExpectedV1Tag = (1 << 3) | 2;
    size_t bytes_written = 0;
    absl::optional<uint64_t> tag =
        ParseVarInt(stream, buffer.data(), &bytes_written);
    if (!tag) {
      RTC_LOG(LS_WARNING)
          << "Missing field tag from beginning of protobuf event.";
      return false;
    } else if (*tag != kExpectedV1Tag) {
      RTC_LOG(LS_WARNING)
          << "Unexpected field tag at beginning of protobuf event.";
      return false;
    }

    // Read the length field.
    absl::optional<uint64_t> message_length =
        ParseVarInt(stream, buffer.data(), &bytes_written);
    if (!message_length) {
      RTC_LOG(LS_WARNING) << "Missing message length after protobuf field tag.";
      return false;
    } else if (*message_length > kMaxEventSize) {
      RTC_LOG(LS_WARNING) << "Protobuf message length is too large.";
      return false;
    }

    // Read the next protobuf event to a temporary char buffer.
    stream.read(buffer.data() + bytes_written, *message_length);
    if (stream.gcount() != static_cast<int>(*message_length)) {
      RTC_LOG(LS_WARNING) << "Failed to read protobuf message from file.";
      return false;
    }
    size_t buffer_size = bytes_written + *message_length;

    // Parse the protobuf event from the buffer.
    rtclog::EventStream event_stream;
    if (!event_stream.ParseFromArray(buffer.data(), buffer_size)) {
      RTC_LOG(LS_WARNING) << "Failed to parse protobuf message.";
      return false;
    }

    RTC_CHECK_EQ(event_stream.stream_size(), 1);
    StoreParsedEvent(event_stream.stream(0));
    events_.push_back(event_stream.stream(0));
  }
  return true;
}

void ParsedRtcEventLogNew::StoreParsedEvent(const rtclog::Event& event) {
  if (event.type() != rtclog::Event::VIDEO_RECEIVER_CONFIG_EVENT &&
      event.type() != rtclog::Event::VIDEO_SENDER_CONFIG_EVENT &&
      event.type() != rtclog::Event::AUDIO_RECEIVER_CONFIG_EVENT &&
      event.type() != rtclog::Event::AUDIO_SENDER_CONFIG_EVENT &&
      event.type() != rtclog::Event::LOG_START &&
      event.type() != rtclog::Event::LOG_END) {
    RTC_CHECK(event.has_timestamp_us());
    int64_t timestamp = event.timestamp_us();
    first_timestamp_ = std::min(first_timestamp_, timestamp);
    last_timestamp_ = std::max(last_timestamp_, timestamp);
  }

  switch (GetEventType(event)) {
    case ParsedRtcEventLogNew::EventType::VIDEO_RECEIVER_CONFIG_EVENT: {
      rtclog::StreamConfig config = GetVideoReceiveConfig(event);
      video_recv_configs_.emplace_back(GetTimestamp(event), config);
      incoming_rtp_extensions_maps_[config.remote_ssrc] =
          RtpHeaderExtensionMap(config.rtp_extensions);
      // TODO(terelius): I don't understand the reason for configuring header
      // extensions for the local SSRC. I think it should be removed, but for
      // now I want to preserve the previous functionality.
      incoming_rtp_extensions_maps_[config.local_ssrc] =
          RtpHeaderExtensionMap(config.rtp_extensions);
      incoming_video_ssrcs_.insert(config.remote_ssrc);
      incoming_video_ssrcs_.insert(config.rtx_ssrc);
      incoming_rtx_ssrcs_.insert(config.rtx_ssrc);
      break;
    }
    case ParsedRtcEventLogNew::EventType::VIDEO_SENDER_CONFIG_EVENT: {
      std::vector<rtclog::StreamConfig> configs = GetVideoSendConfig(event);
      video_send_configs_.emplace_back(GetTimestamp(event), configs);
      for (const auto& config : configs) {
        outgoing_rtp_extensions_maps_[config.local_ssrc] =
            RtpHeaderExtensionMap(config.rtp_extensions);
        outgoing_rtp_extensions_maps_[config.rtx_ssrc] =
            RtpHeaderExtensionMap(config.rtp_extensions);
        outgoing_video_ssrcs_.insert(config.local_ssrc);
        outgoing_video_ssrcs_.insert(config.rtx_ssrc);
        outgoing_rtx_ssrcs_.insert(config.rtx_ssrc);
      }
      break;
    }
    case ParsedRtcEventLogNew::EventType::AUDIO_RECEIVER_CONFIG_EVENT: {
      rtclog::StreamConfig config = GetAudioReceiveConfig(event);
      audio_recv_configs_.emplace_back(GetTimestamp(event), config);
      incoming_rtp_extensions_maps_[config.remote_ssrc] =
          RtpHeaderExtensionMap(config.rtp_extensions);
      incoming_rtp_extensions_maps_[config.local_ssrc] =
          RtpHeaderExtensionMap(config.rtp_extensions);
      incoming_audio_ssrcs_.insert(config.remote_ssrc);
      break;
    }
    case ParsedRtcEventLogNew::EventType::AUDIO_SENDER_CONFIG_EVENT: {
      rtclog::StreamConfig config = GetAudioSendConfig(event);
      audio_send_configs_.emplace_back(GetTimestamp(event), config);
      outgoing_rtp_extensions_maps_[config.local_ssrc] =
          RtpHeaderExtensionMap(config.rtp_extensions);
      outgoing_audio_ssrcs_.insert(config.local_ssrc);
      break;
    }
    case ParsedRtcEventLogNew::EventType::RTP_EVENT: {
      PacketDirection direction;
      uint8_t header[IP_PACKET_SIZE];
      size_t header_length;
      size_t total_length;
      const RtpHeaderExtensionMap* extension_map = GetRtpHeader(
          event, &direction, header, &header_length, &total_length, nullptr);
      RtpUtility::RtpHeaderParser rtp_parser(header, header_length);
      RTPHeader parsed_header;

      if (extension_map != nullptr) {
        rtp_parser.Parse(&parsed_header, extension_map);
      } else {
        // Use the default extension map.
        // TODO(terelius): This should be removed. GetRtpHeader will return the
        // default map if the parser is configured for it.
        // TODO(ivoc): Once configuration of audio streams is stored in the
        //             event log, this can be removed.
        //             Tracking bug: webrtc:6399
        rtp_parser.Parse(&parsed_header, &default_extension_map_);
      }

      // Since we give the parser only a header, there is no way for it to know
      // the padding length. The best solution would be to log the padding
      // length in RTC event log. In absence of it, we assume the RTP packet to
      // contain only padding, if the padding bit is set.
      // TODO(webrtc:9730): Use a generic way to obtain padding length.
      if ((header[0] & 0x20) != 0)
        parsed_header.paddingLength = total_length - header_length;

      RTC_CHECK(event.has_timestamp_us());
      uint64_t timestamp_us = event.timestamp_us();
      if (direction == kIncomingPacket) {
        incoming_rtp_packets_map_[parsed_header.ssrc].push_back(
            LoggedRtpPacketIncoming(timestamp_us, parsed_header, header_length,
                                    total_length));
      } else {
        outgoing_rtp_packets_map_[parsed_header.ssrc].push_back(
            LoggedRtpPacketOutgoing(timestamp_us, parsed_header, header_length,
                                    total_length));
      }
      break;
    }
    case ParsedRtcEventLogNew::EventType::RTCP_EVENT: {
      PacketDirection direction;
      uint8_t packet[IP_PACKET_SIZE];
      size_t total_length;
      GetRtcpPacket(event, &direction, packet, &total_length);
      uint64_t timestamp_us = GetTimestamp(event);
      RTC_CHECK_LE(total_length, IP_PACKET_SIZE);
      if (direction == kIncomingPacket) {
        // Currently incoming RTCP packets are logged twice, both for audio and
        // video. Only act on one of them. Compare against the previous parsed
        // incoming RTCP packet.
        if (total_length == last_incoming_rtcp_packet_length_ &&
            memcmp(last_incoming_rtcp_packet_, packet, total_length) == 0)
          break;
        incoming_rtcp_packets_.push_back(
            LoggedRtcpPacketIncoming(timestamp_us, packet, total_length));
        last_incoming_rtcp_packet_length_ = total_length;
        memcpy(last_incoming_rtcp_packet_, packet, total_length);
      } else {
        outgoing_rtcp_packets_.push_back(
            LoggedRtcpPacketOutgoing(timestamp_us, packet, total_length));
      }
      rtcp::CommonHeader header;
      const uint8_t* packet_end = packet + total_length;
      for (const uint8_t* block = packet; block < packet_end;
           block = header.NextPacket()) {
        RTC_CHECK(header.Parse(block, packet_end - block));
        if (header.type() == rtcp::TransportFeedback::kPacketType &&
            header.fmt() == rtcp::TransportFeedback::kFeedbackMessageType) {
          if (direction == kIncomingPacket) {
            incoming_transport_feedback_.emplace_back();
            LoggedRtcpPacketTransportFeedback& parsed_block =
                incoming_transport_feedback_.back();
            parsed_block.timestamp_us = GetTimestamp(event);
            if (!parsed_block.transport_feedback.Parse(header))
              incoming_transport_feedback_.pop_back();
          } else {
            outgoing_transport_feedback_.emplace_back();
            LoggedRtcpPacketTransportFeedback& parsed_block =
                outgoing_transport_feedback_.back();
            parsed_block.timestamp_us = GetTimestamp(event);
            if (!parsed_block.transport_feedback.Parse(header))
              outgoing_transport_feedback_.pop_back();
          }
        } else if (header.type() == rtcp::SenderReport::kPacketType) {
          LoggedRtcpPacketSenderReport parsed_block;
          parsed_block.timestamp_us = GetTimestamp(event);
          if (parsed_block.sr.Parse(header)) {
            if (direction == kIncomingPacket)
              incoming_sr_.push_back(std::move(parsed_block));
            else
              outgoing_sr_.push_back(std::move(parsed_block));
          }
        } else if (header.type() == rtcp::ReceiverReport::kPacketType) {
          LoggedRtcpPacketReceiverReport parsed_block;
          parsed_block.timestamp_us = GetTimestamp(event);
          if (parsed_block.rr.Parse(header)) {
            if (direction == kIncomingPacket)
              incoming_rr_.push_back(std::move(parsed_block));
            else
              outgoing_rr_.push_back(std::move(parsed_block));
          }
        } else if (header.type() == rtcp::Remb::kPacketType &&
                   header.fmt() == rtcp::Remb::kFeedbackMessageType) {
          LoggedRtcpPacketRemb parsed_block;
          parsed_block.timestamp_us = GetTimestamp(event);
          if (parsed_block.remb.Parse(header)) {
            if (direction == kIncomingPacket)
              incoming_remb_.push_back(std::move(parsed_block));
            else
              outgoing_remb_.push_back(std::move(parsed_block));
          }
        } else if (header.type() == rtcp::Nack::kPacketType &&
                   header.fmt() == rtcp::Nack::kFeedbackMessageType) {
          LoggedRtcpPacketNack parsed_block;
          parsed_block.timestamp_us = GetTimestamp(event);
          if (parsed_block.nack.Parse(header)) {
            if (direction == kIncomingPacket)
              incoming_nack_.push_back(std::move(parsed_block));
            else
              outgoing_nack_.push_back(std::move(parsed_block));
          }
        }
      }
      break;
    }
    case ParsedRtcEventLogNew::EventType::LOG_START: {
      start_log_events_.push_back(LoggedStartEvent(GetTimestamp(event)));
      break;
    }
    case ParsedRtcEventLogNew::EventType::LOG_END: {
      stop_log_events_.push_back(LoggedStopEvent(GetTimestamp(event)));
      break;
    }
    case ParsedRtcEventLogNew::EventType::AUDIO_PLAYOUT_EVENT: {
      LoggedAudioPlayoutEvent playout_event = GetAudioPlayout(event);
      audio_playout_events_[playout_event.ssrc].push_back(playout_event);
      break;
    }
    case ParsedRtcEventLogNew::EventType::LOSS_BASED_BWE_UPDATE: {
      bwe_loss_updates_.push_back(GetLossBasedBweUpdate(event));
      break;
    }
    case ParsedRtcEventLogNew::EventType::DELAY_BASED_BWE_UPDATE: {
      bwe_delay_updates_.push_back(GetDelayBasedBweUpdate(event));
      break;
    }
    case ParsedRtcEventLogNew::EventType::AUDIO_NETWORK_ADAPTATION_EVENT: {
      LoggedAudioNetworkAdaptationEvent ana_event =
          GetAudioNetworkAdaptation(event);
      audio_network_adaptation_events_.push_back(ana_event);
      break;
    }
    case ParsedRtcEventLogNew::EventType::BWE_PROBE_CLUSTER_CREATED_EVENT: {
      bwe_probe_cluster_created_events_.push_back(
          GetBweProbeClusterCreated(event));
      break;
    }
    case ParsedRtcEventLogNew::EventType::BWE_PROBE_FAILURE_EVENT: {
      bwe_probe_failure_events_.push_back(GetBweProbeFailure(event));
      break;
    }
    case ParsedRtcEventLogNew::EventType::BWE_PROBE_SUCCESS_EVENT: {
      bwe_probe_success_events_.push_back(GetBweProbeSuccess(event));
      break;
    }
    case ParsedRtcEventLogNew::EventType::ALR_STATE_EVENT: {
      alr_state_events_.push_back(GetAlrState(event));
      break;
    }
    case ParsedRtcEventLogNew::EventType::ICE_CANDIDATE_PAIR_CONFIG: {
      ice_candidate_pair_configs_.push_back(GetIceCandidatePairConfig(event));
      break;
    }
    case ParsedRtcEventLogNew::EventType::ICE_CANDIDATE_PAIR_EVENT: {
      ice_candidate_pair_events_.push_back(GetIceCandidatePairEvent(event));
      break;
    }
    case ParsedRtcEventLogNew::EventType::UNKNOWN_EVENT: {
      break;
    }
  }
}

size_t ParsedRtcEventLogNew::GetNumberOfEvents() const {
  return events_.size();
}

int64_t ParsedRtcEventLogNew::GetTimestamp(size_t index) const {
  RTC_CHECK_LT(index, GetNumberOfEvents());
  const rtclog::Event& event = events_[index];
  return GetTimestamp(event);
}

int64_t ParsedRtcEventLogNew::GetTimestamp(const rtclog::Event& event) const {
  RTC_CHECK(event.has_timestamp_us());
  return event.timestamp_us();
}

ParsedRtcEventLogNew::EventType ParsedRtcEventLogNew::GetEventType(
    size_t index) const {
  RTC_CHECK_LT(index, GetNumberOfEvents());
  const rtclog::Event& event = events_[index];
  return GetEventType(event);
}

ParsedRtcEventLogNew::EventType ParsedRtcEventLogNew::GetEventType(
    const rtclog::Event& event) const {
  RTC_CHECK(event.has_type());
  if (event.type() == rtclog::Event::BWE_PROBE_RESULT_EVENT) {
    RTC_CHECK(event.has_probe_result());
    RTC_CHECK(event.probe_result().has_result());
    if (event.probe_result().result() == rtclog::BweProbeResult::SUCCESS)
      return ParsedRtcEventLogNew::EventType::BWE_PROBE_SUCCESS_EVENT;
    return ParsedRtcEventLogNew::EventType::BWE_PROBE_FAILURE_EVENT;
  }
  return GetRuntimeEventType(event.type());
}

// The header must have space for at least IP_PACKET_SIZE bytes.
const webrtc::RtpHeaderExtensionMap* ParsedRtcEventLogNew::GetRtpHeader(
    size_t index,
    PacketDirection* incoming,
    uint8_t* header,
    size_t* header_length,
    size_t* total_length,
    int* probe_cluster_id) const {
  RTC_CHECK_LT(index, GetNumberOfEvents());
  const rtclog::Event& event = events_[index];
  return GetRtpHeader(event, incoming, header, header_length, total_length,
                      probe_cluster_id);
}

const webrtc::RtpHeaderExtensionMap* ParsedRtcEventLogNew::GetRtpHeader(
    const rtclog::Event& event,
    PacketDirection* incoming,
    uint8_t* header,
    size_t* header_length,
    size_t* total_length,
    int* probe_cluster_id) const {
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
  if (probe_cluster_id != nullptr) {
    if (rtp_packet.has_probe_cluster_id()) {
      *probe_cluster_id = rtp_packet.probe_cluster_id();
      RTC_CHECK_NE(*probe_cluster_id, PacedPacketInfo::kNotAProbe);
    } else {
      *probe_cluster_id = PacedPacketInfo::kNotAProbe;
    }
  }
  // Get header contents.
  if (header != nullptr) {
    const size_t kMinRtpHeaderSize = 12;
    RTC_CHECK_GE(rtp_packet.header().size(), kMinRtpHeaderSize);
    RTC_CHECK_LE(rtp_packet.header().size(),
                 static_cast<size_t>(IP_PACKET_SIZE));
    memcpy(header, rtp_packet.header().data(), rtp_packet.header().size());
    uint32_t ssrc = ByteReader<uint32_t>::ReadBigEndian(header + 8);
    auto& extensions_maps = rtp_packet.incoming()
                                ? incoming_rtp_extensions_maps_
                                : outgoing_rtp_extensions_maps_;
    auto it = extensions_maps.find(ssrc);
    if (it != extensions_maps.end()) {
      return &(it->second);
    }
    if (parse_unconfigured_header_extensions_ ==
        UnconfiguredHeaderExtensions::kAttemptWebrtcDefaultConfig) {
      RTC_LOG(LS_WARNING) << "Using default header extension map for SSRC "
                          << ssrc;
      extensions_maps.insert(std::make_pair(ssrc, default_extension_map_));
      return &default_extension_map_;
    }
  }
  return nullptr;
}

// The packet must have space for at least IP_PACKET_SIZE bytes.
void ParsedRtcEventLogNew::GetRtcpPacket(size_t index,
                                         PacketDirection* incoming,
                                         uint8_t* packet,
                                         size_t* length) const {
  RTC_CHECK_LT(index, GetNumberOfEvents());
  const rtclog::Event& event = events_[index];
  GetRtcpPacket(event, incoming, packet, length);
}

void ParsedRtcEventLogNew::GetRtcpPacket(const rtclog::Event& event,
                                         PacketDirection* incoming,
                                         uint8_t* packet,
                                         size_t* length) const {
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

rtclog::StreamConfig ParsedRtcEventLogNew::GetVideoReceiveConfig(
    size_t index) const {
  RTC_CHECK_LT(index, GetNumberOfEvents());
  return GetVideoReceiveConfig(events_[index]);
}

rtclog::StreamConfig ParsedRtcEventLogNew::GetVideoReceiveConfig(
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

std::vector<rtclog::StreamConfig> ParsedRtcEventLogNew::GetVideoSendConfig(
    size_t index) const {
  RTC_CHECK_LT(index, GetNumberOfEvents());
  return GetVideoSendConfig(events_[index]);
}

std::vector<rtclog::StreamConfig> ParsedRtcEventLogNew::GetVideoSendConfig(
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

rtclog::StreamConfig ParsedRtcEventLogNew::GetAudioReceiveConfig(
    size_t index) const {
  RTC_CHECK_LT(index, GetNumberOfEvents());
  return GetAudioReceiveConfig(events_[index]);
}

rtclog::StreamConfig ParsedRtcEventLogNew::GetAudioReceiveConfig(
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

rtclog::StreamConfig ParsedRtcEventLogNew::GetAudioSendConfig(
    size_t index) const {
  RTC_CHECK_LT(index, GetNumberOfEvents());
  return GetAudioSendConfig(events_[index]);
}

rtclog::StreamConfig ParsedRtcEventLogNew::GetAudioSendConfig(
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

LoggedAudioPlayoutEvent ParsedRtcEventLogNew::GetAudioPlayout(
    size_t index) const {
  RTC_CHECK_LT(index, GetNumberOfEvents());
  const rtclog::Event& event = events_[index];
  return GetAudioPlayout(event);
}

LoggedAudioPlayoutEvent ParsedRtcEventLogNew::GetAudioPlayout(
    const rtclog::Event& event) const {
  RTC_CHECK(event.has_type());
  RTC_CHECK_EQ(event.type(), rtclog::Event::AUDIO_PLAYOUT_EVENT);
  RTC_CHECK(event.has_audio_playout_event());
  const rtclog::AudioPlayoutEvent& playout_event = event.audio_playout_event();
  LoggedAudioPlayoutEvent res;
  res.timestamp_us = GetTimestamp(event);
  RTC_CHECK(playout_event.has_local_ssrc());
  res.ssrc = playout_event.local_ssrc();
  return res;
}

LoggedBweLossBasedUpdate ParsedRtcEventLogNew::GetLossBasedBweUpdate(
    size_t index) const {
  RTC_CHECK_LT(index, GetNumberOfEvents());
  const rtclog::Event& event = events_[index];
  return GetLossBasedBweUpdate(event);
}

LoggedBweLossBasedUpdate ParsedRtcEventLogNew::GetLossBasedBweUpdate(
    const rtclog::Event& event) const {
  RTC_CHECK(event.has_type());
  RTC_CHECK_EQ(event.type(), rtclog::Event::LOSS_BASED_BWE_UPDATE);
  RTC_CHECK(event.has_loss_based_bwe_update());
  const rtclog::LossBasedBweUpdate& loss_event = event.loss_based_bwe_update();

  LoggedBweLossBasedUpdate bwe_update;
  bwe_update.timestamp_us = GetTimestamp(event);
  RTC_CHECK(loss_event.has_bitrate_bps());
  bwe_update.bitrate_bps = loss_event.bitrate_bps();
  RTC_CHECK(loss_event.has_fraction_loss());
  bwe_update.fraction_lost = loss_event.fraction_loss();
  RTC_CHECK(loss_event.has_total_packets());
  bwe_update.expected_packets = loss_event.total_packets();
  return bwe_update;
}

LoggedBweDelayBasedUpdate ParsedRtcEventLogNew::GetDelayBasedBweUpdate(
    size_t index) const {
  RTC_CHECK_LT(index, GetNumberOfEvents());
  const rtclog::Event& event = events_[index];
  return GetDelayBasedBweUpdate(event);
}

LoggedBweDelayBasedUpdate ParsedRtcEventLogNew::GetDelayBasedBweUpdate(
    const rtclog::Event& event) const {
  RTC_CHECK(event.has_type());
  RTC_CHECK_EQ(event.type(), rtclog::Event::DELAY_BASED_BWE_UPDATE);
  RTC_CHECK(event.has_delay_based_bwe_update());
  const rtclog::DelayBasedBweUpdate& delay_event =
      event.delay_based_bwe_update();

  LoggedBweDelayBasedUpdate res;
  res.timestamp_us = GetTimestamp(event);
  RTC_CHECK(delay_event.has_bitrate_bps());
  res.bitrate_bps = delay_event.bitrate_bps();
  RTC_CHECK(delay_event.has_detector_state());
  res.detector_state = GetRuntimeDetectorState(delay_event.detector_state());
  return res;
}

LoggedAudioNetworkAdaptationEvent
ParsedRtcEventLogNew::GetAudioNetworkAdaptation(size_t index) const {
  RTC_CHECK_LT(index, GetNumberOfEvents());
  const rtclog::Event& event = events_[index];
  return GetAudioNetworkAdaptation(event);
}

LoggedAudioNetworkAdaptationEvent
ParsedRtcEventLogNew::GetAudioNetworkAdaptation(
    const rtclog::Event& event) const {
  RTC_CHECK(event.has_type());
  RTC_CHECK_EQ(event.type(), rtclog::Event::AUDIO_NETWORK_ADAPTATION_EVENT);
  RTC_CHECK(event.has_audio_network_adaptation());
  const rtclog::AudioNetworkAdaptation& ana_event =
      event.audio_network_adaptation();

  LoggedAudioNetworkAdaptationEvent res;
  res.timestamp_us = GetTimestamp(event);
  if (ana_event.has_bitrate_bps())
    res.config.bitrate_bps = ana_event.bitrate_bps();
  if (ana_event.has_enable_fec())
    res.config.enable_fec = ana_event.enable_fec();
  if (ana_event.has_enable_dtx())
    res.config.enable_dtx = ana_event.enable_dtx();
  if (ana_event.has_frame_length_ms())
    res.config.frame_length_ms = ana_event.frame_length_ms();
  if (ana_event.has_num_channels())
    res.config.num_channels = ana_event.num_channels();
  if (ana_event.has_uplink_packet_loss_fraction())
    res.config.uplink_packet_loss_fraction =
        ana_event.uplink_packet_loss_fraction();
  return res;
}

LoggedBweProbeClusterCreatedEvent
ParsedRtcEventLogNew::GetBweProbeClusterCreated(size_t index) const {
  RTC_CHECK_LT(index, GetNumberOfEvents());
  const rtclog::Event& event = events_[index];
  return GetBweProbeClusterCreated(event);
}

LoggedBweProbeClusterCreatedEvent
ParsedRtcEventLogNew::GetBweProbeClusterCreated(
    const rtclog::Event& event) const {
  RTC_CHECK(event.has_type());
  RTC_CHECK_EQ(event.type(), rtclog::Event::BWE_PROBE_CLUSTER_CREATED_EVENT);
  RTC_CHECK(event.has_probe_cluster());
  const rtclog::BweProbeCluster& pcc_event = event.probe_cluster();
  LoggedBweProbeClusterCreatedEvent res;
  res.timestamp_us = GetTimestamp(event);
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

LoggedBweProbeFailureEvent ParsedRtcEventLogNew::GetBweProbeFailure(
    size_t index) const {
  RTC_CHECK_LT(index, GetNumberOfEvents());
  const rtclog::Event& event = events_[index];
  return GetBweProbeFailure(event);
}

LoggedBweProbeFailureEvent ParsedRtcEventLogNew::GetBweProbeFailure(
    const rtclog::Event& event) const {
  RTC_CHECK(event.has_type());
  RTC_CHECK_EQ(event.type(), rtclog::Event::BWE_PROBE_RESULT_EVENT);
  RTC_CHECK(event.has_probe_result());
  const rtclog::BweProbeResult& pr_event = event.probe_result();
  RTC_CHECK(pr_event.has_result());
  RTC_CHECK_NE(pr_event.result(), rtclog::BweProbeResult::SUCCESS);

  LoggedBweProbeFailureEvent res;
  res.timestamp_us = GetTimestamp(event);
  RTC_CHECK(pr_event.has_id());
  res.id = pr_event.id();
  RTC_CHECK(pr_event.has_result());
  if (pr_event.result() ==
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
  RTC_CHECK(!pr_event.has_bitrate_bps());

  return res;
}

LoggedBweProbeSuccessEvent ParsedRtcEventLogNew::GetBweProbeSuccess(
    size_t index) const {
  RTC_CHECK_LT(index, GetNumberOfEvents());
  const rtclog::Event& event = events_[index];
  return GetBweProbeSuccess(event);
}

LoggedBweProbeSuccessEvent ParsedRtcEventLogNew::GetBweProbeSuccess(
    const rtclog::Event& event) const {
  RTC_CHECK(event.has_type());
  RTC_CHECK_EQ(event.type(), rtclog::Event::BWE_PROBE_RESULT_EVENT);
  RTC_CHECK(event.has_probe_result());
  const rtclog::BweProbeResult& pr_event = event.probe_result();
  RTC_CHECK(pr_event.has_result());
  RTC_CHECK_EQ(pr_event.result(), rtclog::BweProbeResult::SUCCESS);

  LoggedBweProbeSuccessEvent res;
  res.timestamp_us = GetTimestamp(event);
  RTC_CHECK(pr_event.has_id());
  res.id = pr_event.id();
  RTC_CHECK(pr_event.has_bitrate_bps());
  res.bitrate_bps = pr_event.bitrate_bps();

  return res;
}

LoggedAlrStateEvent ParsedRtcEventLogNew::GetAlrState(size_t index) const {
  RTC_CHECK_LT(index, GetNumberOfEvents());
  const rtclog::Event& event = events_[index];
  return GetAlrState(event);
}

LoggedAlrStateEvent ParsedRtcEventLogNew::GetAlrState(
    const rtclog::Event& event) const {
  RTC_CHECK(event.has_type());
  RTC_CHECK_EQ(event.type(), rtclog::Event::ALR_STATE_EVENT);
  RTC_CHECK(event.has_alr_state());
  const rtclog::AlrState& alr_event = event.alr_state();
  LoggedAlrStateEvent res;
  res.timestamp_us = GetTimestamp(event);
  RTC_CHECK(alr_event.has_in_alr());
  res.in_alr = alr_event.in_alr();

  return res;
}

LoggedIceCandidatePairConfig ParsedRtcEventLogNew::GetIceCandidatePairConfig(
    size_t index) const {
  RTC_CHECK_LT(index, GetNumberOfEvents());
  const rtclog::Event& rtc_event = events_[index];
  return GetIceCandidatePairConfig(rtc_event);
}

LoggedIceCandidatePairConfig ParsedRtcEventLogNew::GetIceCandidatePairConfig(
    const rtclog::Event& rtc_event) const {
  RTC_CHECK(rtc_event.has_type());
  RTC_CHECK_EQ(rtc_event.type(), rtclog::Event::ICE_CANDIDATE_PAIR_CONFIG);
  LoggedIceCandidatePairConfig res;
  const rtclog::IceCandidatePairConfig& config =
      rtc_event.ice_candidate_pair_config();
  res.timestamp_us = GetTimestamp(rtc_event);
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

LoggedIceCandidatePairEvent ParsedRtcEventLogNew::GetIceCandidatePairEvent(
    size_t index) const {
  RTC_CHECK_LT(index, GetNumberOfEvents());
  const rtclog::Event& rtc_event = events_[index];
  return GetIceCandidatePairEvent(rtc_event);
}

LoggedIceCandidatePairEvent ParsedRtcEventLogNew::GetIceCandidatePairEvent(
    const rtclog::Event& rtc_event) const {
  RTC_CHECK(rtc_event.has_type());
  RTC_CHECK_EQ(rtc_event.type(), rtclog::Event::ICE_CANDIDATE_PAIR_EVENT);
  LoggedIceCandidatePairEvent res;
  const rtclog::IceCandidatePairEvent& event =
      rtc_event.ice_candidate_pair_event();
  res.timestamp_us = GetTimestamp(rtc_event);
  RTC_CHECK(event.has_event_type());
  res.type = GetRuntimeIceCandidatePairEventType(event.event_type());
  RTC_CHECK(event.has_candidate_pair_id());
  res.candidate_pair_id = event.candidate_pair_id();
  return res;
}

// Returns the MediaType for registered SSRCs. Search from the end to use last
// registered types first.
ParsedRtcEventLogNew::MediaType ParsedRtcEventLogNew::GetMediaType(
    uint32_t ssrc,
    PacketDirection direction) const {
  if (direction == kIncomingPacket) {
    if (std::find(incoming_video_ssrcs_.begin(), incoming_video_ssrcs_.end(),
                  ssrc) != incoming_video_ssrcs_.end()) {
      return MediaType::VIDEO;
    }
    if (std::find(incoming_audio_ssrcs_.begin(), incoming_audio_ssrcs_.end(),
                  ssrc) != incoming_audio_ssrcs_.end()) {
      return MediaType::AUDIO;
    }
  } else {
    if (std::find(outgoing_video_ssrcs_.begin(), outgoing_video_ssrcs_.end(),
                  ssrc) != outgoing_video_ssrcs_.end()) {
      return MediaType::VIDEO;
    }
    if (std::find(outgoing_audio_ssrcs_.begin(), outgoing_audio_ssrcs_.end(),
                  ssrc) != outgoing_audio_ssrcs_.end()) {
      return MediaType::AUDIO;
    }
  }
  return MediaType::ANY;
}

const std::vector<MatchedSendArrivalTimes> GetNetworkTrace(
    const ParsedRtcEventLogNew& parsed_log) {
  using RtpPacketType = LoggedRtpPacketOutgoing;
  using TransportFeedbackType = LoggedRtcpPacketTransportFeedback;

  std::multimap<int64_t, const RtpPacketType*> outgoing_rtp;
  for (const auto& stream : parsed_log.outgoing_rtp_packets_by_ssrc()) {
    for (const RtpPacketType& rtp_packet : stream.outgoing_packets)
      outgoing_rtp.insert(
          std::make_pair(rtp_packet.rtp.log_time_us(), &rtp_packet));
  }

  const std::vector<TransportFeedbackType>& incoming_rtcp =
      parsed_log.transport_feedbacks(kIncomingPacket);

  SimulatedClock clock(0);
  TransportFeedbackAdapter feedback_adapter(&clock);

  auto rtp_iterator = outgoing_rtp.begin();
  auto rtcp_iterator = incoming_rtcp.begin();

  auto NextRtpTime = [&]() {
    if (rtp_iterator != outgoing_rtp.end())
      return static_cast<int64_t>(rtp_iterator->first);
    return std::numeric_limits<int64_t>::max();
  };

  auto NextRtcpTime = [&]() {
    if (rtcp_iterator != incoming_rtcp.end())
      return static_cast<int64_t>(rtcp_iterator->log_time_us());
    return std::numeric_limits<int64_t>::max();
  };

  int64_t time_us = std::min(NextRtpTime(), NextRtcpTime());

  std::vector<MatchedSendArrivalTimes> rtp_rtcp_matched;
  while (time_us != std::numeric_limits<int64_t>::max()) {
    clock.AdvanceTimeMicroseconds(time_us - clock.TimeInMicroseconds());
    if (clock.TimeInMicroseconds() >= NextRtcpTime()) {
      RTC_DCHECK_EQ(clock.TimeInMicroseconds(), NextRtcpTime());
      feedback_adapter.OnTransportFeedback(rtcp_iterator->transport_feedback);
      std::vector<PacketFeedback> feedback =
          feedback_adapter.GetTransportFeedbackVector();
      SortPacketFeedbackVectorWithLoss(&feedback);
      for (const PacketFeedback& packet : feedback) {
        rtp_rtcp_matched.emplace_back(
            clock.TimeInMilliseconds(), packet.send_time_ms,
            packet.arrival_time_ms, packet.payload_size);
      }
      ++rtcp_iterator;
    }
    if (clock.TimeInMicroseconds() >= NextRtpTime()) {
      RTC_DCHECK_EQ(clock.TimeInMicroseconds(), NextRtpTime());
      const RtpPacketType& rtp_packet = *rtp_iterator->second;
      if (rtp_packet.rtp.header.extension.hasTransportSequenceNumber) {
        feedback_adapter.AddPacket(
            rtp_packet.rtp.header.ssrc,
            rtp_packet.rtp.header.extension.transportSequenceNumber,
            rtp_packet.rtp.total_length, PacedPacketInfo());
        feedback_adapter.OnSentPacket(
            rtp_packet.rtp.header.extension.transportSequenceNumber,
            rtp_packet.rtp.log_time_ms());
      }
      ++rtp_iterator;
    }
    time_us = std::min(NextRtpTime(), NextRtcpTime());
  }
  return rtp_rtcp_matched;
}

}  // namespace webrtc
