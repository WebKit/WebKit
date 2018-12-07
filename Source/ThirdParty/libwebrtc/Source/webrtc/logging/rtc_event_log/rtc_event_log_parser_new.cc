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
#include "logging/rtc_event_log/encoder/blob_encoding.h"
#include "logging/rtc_event_log/encoder/delta_encoding.h"
#include "logging/rtc_event_log/encoder/rtc_event_log_encoder_common.h"
#include "logging/rtc_event_log/rtc_event_log.h"
#include "modules/audio_coding/audio_network_adaptor/include/audio_network_adaptor.h"
#include "modules/congestion_controller/rtp/transport_feedback_adapter.h"
#include "modules/remote_bitrate_estimator/include/bwe_defines.h"
#include "modules/rtp_rtcp/include/rtp_cvo.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/byte_io.h"
#include "modules/rtp_rtcp/source/rtp_header_extensions.h"
#include "modules/rtp_rtcp/source/rtp_utility.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/safe_conversions.h"
#include "rtc_base/protobuf_utils.h"

using webrtc_event_logging::ToSigned;
using webrtc_event_logging::ToUnsigned;

namespace webrtc {

namespace {
// Conversion functions for legacy wire format.
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

// Conversion functions for version 2 of the wire format.
BandwidthUsage GetRuntimeDetectorState(
    rtclog2::DelayBasedBweUpdates::DetectorState detector_state) {
  switch (detector_state) {
    case rtclog2::DelayBasedBweUpdates::BWE_NORMAL:
      return BandwidthUsage::kBwNormal;
    case rtclog2::DelayBasedBweUpdates::BWE_UNDERUSING:
      return BandwidthUsage::kBwUnderusing;
    case rtclog2::DelayBasedBweUpdates::BWE_OVERUSING:
      return BandwidthUsage::kBwOverusing;
    case rtclog2::DelayBasedBweUpdates::BWE_UNKNOWN_STATE:
      break;
  }
  RTC_NOTREACHED();
  return BandwidthUsage::kBwNormal;
}

ProbeFailureReason GetRuntimeProbeFailureReason(
    rtclog2::BweProbeResultFailure::FailureReason failure) {
  switch (failure) {
    case rtclog2::BweProbeResultFailure::INVALID_SEND_RECEIVE_INTERVAL:
      return ProbeFailureReason::kInvalidSendReceiveInterval;
    case rtclog2::BweProbeResultFailure::INVALID_SEND_RECEIVE_RATIO:
      return ProbeFailureReason::kInvalidSendReceiveRatio;
    case rtclog2::BweProbeResultFailure::TIMEOUT:
      return ProbeFailureReason::kTimeout;
    case rtclog2::BweProbeResultFailure::UNKNOWN:
      break;
  }
  RTC_NOTREACHED();
  return ProbeFailureReason::kTimeout;
}

IceCandidatePairConfigType GetRuntimeIceCandidatePairConfigType(
    rtclog2::IceCandidatePairConfig::IceCandidatePairConfigType type) {
  switch (type) {
    case rtclog2::IceCandidatePairConfig::ADDED:
      return IceCandidatePairConfigType::kAdded;
    case rtclog2::IceCandidatePairConfig::UPDATED:
      return IceCandidatePairConfigType::kUpdated;
    case rtclog2::IceCandidatePairConfig::DESTROYED:
      return IceCandidatePairConfigType::kDestroyed;
    case rtclog2::IceCandidatePairConfig::SELECTED:
      return IceCandidatePairConfigType::kSelected;
    case rtclog2::IceCandidatePairConfig::UNKNOWN_CONFIG_TYPE:
      break;
  }
  RTC_NOTREACHED();
  return IceCandidatePairConfigType::kAdded;
}

IceCandidateType GetRuntimeIceCandidateType(
    rtclog2::IceCandidatePairConfig::IceCandidateType type) {
  switch (type) {
    case rtclog2::IceCandidatePairConfig::LOCAL:
      return IceCandidateType::kLocal;
    case rtclog2::IceCandidatePairConfig::STUN:
      return IceCandidateType::kStun;
    case rtclog2::IceCandidatePairConfig::PRFLX:
      return IceCandidateType::kPrflx;
    case rtclog2::IceCandidatePairConfig::RELAY:
      return IceCandidateType::kRelay;
    case rtclog2::IceCandidatePairConfig::UNKNOWN_CANDIDATE_TYPE:
      return IceCandidateType::kUnknown;
  }
  RTC_NOTREACHED();
  return IceCandidateType::kUnknown;
}

IceCandidatePairProtocol GetRuntimeIceCandidatePairProtocol(
    rtclog2::IceCandidatePairConfig::Protocol protocol) {
  switch (protocol) {
    case rtclog2::IceCandidatePairConfig::UDP:
      return IceCandidatePairProtocol::kUdp;
    case rtclog2::IceCandidatePairConfig::TCP:
      return IceCandidatePairProtocol::kTcp;
    case rtclog2::IceCandidatePairConfig::SSLTCP:
      return IceCandidatePairProtocol::kSsltcp;
    case rtclog2::IceCandidatePairConfig::TLS:
      return IceCandidatePairProtocol::kTls;
    case rtclog2::IceCandidatePairConfig::UNKNOWN_PROTOCOL:
      return IceCandidatePairProtocol::kUnknown;
  }
  RTC_NOTREACHED();
  return IceCandidatePairProtocol::kUnknown;
}

IceCandidatePairAddressFamily GetRuntimeIceCandidatePairAddressFamily(
    rtclog2::IceCandidatePairConfig::AddressFamily address_family) {
  switch (address_family) {
    case rtclog2::IceCandidatePairConfig::IPV4:
      return IceCandidatePairAddressFamily::kIpv4;
    case rtclog2::IceCandidatePairConfig::IPV6:
      return IceCandidatePairAddressFamily::kIpv6;
    case rtclog2::IceCandidatePairConfig::UNKNOWN_ADDRESS_FAMILY:
      return IceCandidatePairAddressFamily::kUnknown;
  }
  RTC_NOTREACHED();
  return IceCandidatePairAddressFamily::kUnknown;
}

IceCandidateNetworkType GetRuntimeIceCandidateNetworkType(
    rtclog2::IceCandidatePairConfig::NetworkType network_type) {
  switch (network_type) {
    case rtclog2::IceCandidatePairConfig::ETHERNET:
      return IceCandidateNetworkType::kEthernet;
    case rtclog2::IceCandidatePairConfig::LOOPBACK:
      return IceCandidateNetworkType::kLoopback;
    case rtclog2::IceCandidatePairConfig::WIFI:
      return IceCandidateNetworkType::kWifi;
    case rtclog2::IceCandidatePairConfig::VPN:
      return IceCandidateNetworkType::kVpn;
    case rtclog2::IceCandidatePairConfig::CELLULAR:
      return IceCandidateNetworkType::kCellular;
    case rtclog2::IceCandidatePairConfig::UNKNOWN_NETWORK_TYPE:
      return IceCandidateNetworkType::kUnknown;
  }
  RTC_NOTREACHED();
  return IceCandidateNetworkType::kUnknown;
}

IceCandidatePairEventType GetRuntimeIceCandidatePairEventType(
    rtclog2::IceCandidatePairEvent::IceCandidatePairEventType type) {
  switch (type) {
    case rtclog2::IceCandidatePairEvent::CHECK_SENT:
      return IceCandidatePairEventType::kCheckSent;
    case rtclog2::IceCandidatePairEvent::CHECK_RECEIVED:
      return IceCandidatePairEventType::kCheckReceived;
    case rtclog2::IceCandidatePairEvent::CHECK_RESPONSE_SENT:
      return IceCandidatePairEventType::kCheckResponseSent;
    case rtclog2::IceCandidatePairEvent::CHECK_RESPONSE_RECEIVED:
      return IceCandidatePairEventType::kCheckResponseReceived;
    case rtclog2::IceCandidatePairEvent::UNKNOWN_CHECK_TYPE:
      break;
  }
  RTC_NOTREACHED();
  return IceCandidatePairEventType::kCheckSent;
}

std::vector<RtpExtension> GetRuntimeRtpHeaderExtensionConfig(
    const rtclog2::RtpHeaderExtensionConfig& proto_header_extensions) {
  std::vector<RtpExtension> rtp_extensions;
  if (proto_header_extensions.has_transmission_time_offset_id()) {
    rtp_extensions.emplace_back(
        RtpExtension::kTimestampOffsetUri,
        proto_header_extensions.transmission_time_offset_id());
  }
  if (proto_header_extensions.has_absolute_send_time_id()) {
    rtp_extensions.emplace_back(
        RtpExtension::kAbsSendTimeUri,
        proto_header_extensions.absolute_send_time_id());
  }
  if (proto_header_extensions.has_transport_sequence_number_id()) {
    rtp_extensions.emplace_back(
        RtpExtension::kTransportSequenceNumberUri,
        proto_header_extensions.transport_sequence_number_id());
  }
  if (proto_header_extensions.has_audio_level_id()) {
    rtp_extensions.emplace_back(RtpExtension::kAudioLevelUri,
                                proto_header_extensions.audio_level_id());
  }
  if (proto_header_extensions.has_video_rotation_id()) {
    rtp_extensions.emplace_back(RtpExtension::kVideoRotationUri,
                                proto_header_extensions.video_rotation_id());
  }
  return rtp_extensions;
}
// End of conversion functions.

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

template <typename ProtoType, typename LoggedType>
void StoreRtpPackets(
    const ProtoType& proto,
    std::map<uint32_t, std::vector<LoggedType>>* rtp_packets_map) {
  RTC_CHECK(proto.has_timestamp_ms());
  RTC_CHECK(proto.has_marker());
  RTC_CHECK(proto.has_payload_type());
  RTC_CHECK(proto.has_sequence_number());
  RTC_CHECK(proto.has_rtp_timestamp());
  RTC_CHECK(proto.has_ssrc());
  RTC_CHECK(proto.has_payload_size());
  RTC_CHECK(proto.has_header_size());
  RTC_CHECK(proto.has_padding_size());

  // Base event
  {
    RTPHeader header;
    header.markerBit = rtc::checked_cast<bool>(proto.marker());
    header.payloadType = rtc::checked_cast<uint8_t>(proto.payload_type());
    header.sequenceNumber =
        rtc::checked_cast<uint16_t>(proto.sequence_number());
    header.timestamp = rtc::checked_cast<uint32_t>(proto.rtp_timestamp());
    header.ssrc = rtc::checked_cast<uint32_t>(proto.ssrc());
    header.numCSRCs = 0;  // TODO(terelius): Implement CSRC.
    header.paddingLength = rtc::checked_cast<size_t>(proto.padding_size());
    header.headerLength = rtc::checked_cast<size_t>(proto.header_size());
    // TODO(terelius): Should we implement payload_type_frequency?
    if (proto.has_transport_sequence_number()) {
      header.extension.hasTransportSequenceNumber = true;
      header.extension.transportSequenceNumber =
          rtc::checked_cast<uint16_t>(proto.transport_sequence_number());
    }
    if (proto.has_transmission_time_offset()) {
      header.extension.hasTransmissionTimeOffset = true;
      header.extension.transmissionTimeOffset =
          rtc::checked_cast<int32_t>(proto.transmission_time_offset());
    }
    if (proto.has_absolute_send_time()) {
      header.extension.hasAbsoluteSendTime = true;
      header.extension.absoluteSendTime =
          rtc::checked_cast<uint32_t>(proto.absolute_send_time());
    }
    if (proto.has_video_rotation()) {
      header.extension.hasVideoRotation = true;
      header.extension.videoRotation = ConvertCVOByteToVideoRotation(
          rtc::checked_cast<uint8_t>(proto.video_rotation()));
    }
    if (proto.has_audio_level()) {
      RTC_CHECK(proto.has_voice_activity());
      header.extension.hasAudioLevel = true;
      header.extension.voiceActivity =
          rtc::checked_cast<bool>(proto.voice_activity());
      const uint8_t audio_level =
          rtc::checked_cast<uint8_t>(proto.audio_level());
      RTC_CHECK_LE(audio_level, 0x7Fu);
      header.extension.audioLevel = audio_level;
    } else {
      RTC_CHECK(!proto.has_voice_activity());
    }
    (*rtp_packets_map)[header.ssrc].emplace_back(
        proto.timestamp_ms() * 1000, header, proto.header_size(),
        proto.payload_size() + header.headerLength + header.paddingLength);
  }

  const size_t number_of_deltas =
      proto.has_number_of_deltas() ? proto.number_of_deltas() : 0u;
  if (number_of_deltas == 0) {
    return;
  }

  // timestamp_ms (event)
  std::vector<absl::optional<uint64_t>> timestamp_ms_values =
      DecodeDeltas(proto.timestamp_ms_deltas(),
                   ToUnsigned(proto.timestamp_ms()), number_of_deltas);
  RTC_CHECK_EQ(timestamp_ms_values.size(), number_of_deltas);

  // marker (RTP base)
  std::vector<absl::optional<uint64_t>> marker_values =
      DecodeDeltas(proto.marker_deltas(), proto.marker(), number_of_deltas);
  RTC_CHECK_EQ(marker_values.size(), number_of_deltas);

  // payload_type (RTP base)
  std::vector<absl::optional<uint64_t>> payload_type_values = DecodeDeltas(
      proto.payload_type_deltas(), proto.payload_type(), number_of_deltas);
  RTC_CHECK_EQ(payload_type_values.size(), number_of_deltas);

  // sequence_number (RTP base)
  std::vector<absl::optional<uint64_t>> sequence_number_values =
      DecodeDeltas(proto.sequence_number_deltas(), proto.sequence_number(),
                   number_of_deltas);
  RTC_CHECK_EQ(sequence_number_values.size(), number_of_deltas);

  // rtp_timestamp (RTP base)
  std::vector<absl::optional<uint64_t>> rtp_timestamp_values = DecodeDeltas(
      proto.rtp_timestamp_deltas(), proto.rtp_timestamp(), number_of_deltas);
  RTC_CHECK_EQ(rtp_timestamp_values.size(), number_of_deltas);

  // ssrc (RTP base)
  std::vector<absl::optional<uint64_t>> ssrc_values =
      DecodeDeltas(proto.ssrc_deltas(), proto.ssrc(), number_of_deltas);
  RTC_CHECK_EQ(ssrc_values.size(), number_of_deltas);

  // payload_size (RTP base)
  std::vector<absl::optional<uint64_t>> payload_size_values = DecodeDeltas(
      proto.payload_size_deltas(), proto.payload_size(), number_of_deltas);
  RTC_CHECK_EQ(payload_size_values.size(), number_of_deltas);

  // header_size (RTP base)
  std::vector<absl::optional<uint64_t>> header_size_values = DecodeDeltas(
      proto.header_size_deltas(), proto.header_size(), number_of_deltas);
  RTC_CHECK_EQ(header_size_values.size(), number_of_deltas);

  // padding_size (RTP base)
  std::vector<absl::optional<uint64_t>> padding_size_values = DecodeDeltas(
      proto.padding_size_deltas(), proto.padding_size(), number_of_deltas);
  RTC_CHECK_EQ(padding_size_values.size(), number_of_deltas);

  // transport_sequence_number (RTP extension)
  std::vector<absl::optional<uint64_t>> transport_sequence_number_values;
  {
    const absl::optional<uint64_t> base_transport_sequence_number =
        proto.has_transport_sequence_number()
            ? proto.transport_sequence_number()
            : absl::optional<uint64_t>();
    transport_sequence_number_values =
        DecodeDeltas(proto.transport_sequence_number_deltas(),
                     base_transport_sequence_number, number_of_deltas);
    RTC_CHECK_EQ(transport_sequence_number_values.size(), number_of_deltas);
  }

  // transmission_time_offset (RTP extension)
  std::vector<absl::optional<uint64_t>> transmission_time_offset_values;
  {
    const absl::optional<uint64_t> unsigned_base_transmission_time_offset =
        proto.has_transmission_time_offset()
            ? ToUnsigned(proto.transmission_time_offset())
            : absl::optional<uint64_t>();
    transmission_time_offset_values =
        DecodeDeltas(proto.transmission_time_offset_deltas(),
                     unsigned_base_transmission_time_offset, number_of_deltas);
    RTC_CHECK_EQ(transmission_time_offset_values.size(), number_of_deltas);
  }

  // absolute_send_time (RTP extension)
  std::vector<absl::optional<uint64_t>> absolute_send_time_values;
  {
    const absl::optional<uint64_t> base_absolute_send_time =
        proto.has_absolute_send_time() ? proto.absolute_send_time()
                                       : absl::optional<uint64_t>();
    absolute_send_time_values =
        DecodeDeltas(proto.absolute_send_time_deltas(), base_absolute_send_time,
                     number_of_deltas);
    RTC_CHECK_EQ(absolute_send_time_values.size(), number_of_deltas);
  }

  // video_rotation (RTP extension)
  std::vector<absl::optional<uint64_t>> video_rotation_values;
  {
    const absl::optional<uint64_t> base_video_rotation =
        proto.has_video_rotation() ? proto.video_rotation()
                                   : absl::optional<uint64_t>();
    video_rotation_values = DecodeDeltas(proto.video_rotation_deltas(),
                                         base_video_rotation, number_of_deltas);
    RTC_CHECK_EQ(video_rotation_values.size(), number_of_deltas);
  }

  // audio_level (RTP extension)
  std::vector<absl::optional<uint64_t>> audio_level_values;
  {
    const absl::optional<uint64_t> base_audio_level =
        proto.has_audio_level() ? proto.audio_level()
                                : absl::optional<uint64_t>();
    audio_level_values = DecodeDeltas(proto.audio_level_deltas(),
                                      base_audio_level, number_of_deltas);
    RTC_CHECK_EQ(audio_level_values.size(), number_of_deltas);
  }

  // voice_activity (RTP extension)
  std::vector<absl::optional<uint64_t>> voice_activity_values;
  {
    const absl::optional<uint64_t> base_voice_activity =
        proto.has_voice_activity() ? proto.voice_activity()
                                   : absl::optional<uint64_t>();
    voice_activity_values = DecodeDeltas(proto.voice_activity_deltas(),
                                         base_voice_activity, number_of_deltas);
    RTC_CHECK_EQ(voice_activity_values.size(), number_of_deltas);
  }

  // Delta decoding
  for (size_t i = 0; i < number_of_deltas; ++i) {
    RTC_CHECK(timestamp_ms_values[i].has_value());
    RTC_CHECK(marker_values[i].has_value());
    RTC_CHECK(payload_type_values[i].has_value());
    RTC_CHECK(sequence_number_values[i].has_value());
    RTC_CHECK(rtp_timestamp_values[i].has_value());
    RTC_CHECK(ssrc_values[i].has_value());
    RTC_CHECK(payload_size_values[i].has_value());
    RTC_CHECK(header_size_values[i].has_value());
    RTC_CHECK(padding_size_values[i].has_value());

    int64_t timestamp_ms;
    RTC_CHECK(ToSigned(timestamp_ms_values[i].value(), &timestamp_ms));

    RTPHeader header;
    header.markerBit = rtc::checked_cast<bool>(*marker_values[i]);
    header.payloadType = rtc::checked_cast<uint8_t>(*payload_type_values[i]);
    header.sequenceNumber =
        rtc::checked_cast<uint16_t>(*sequence_number_values[i]);
    header.timestamp = rtc::checked_cast<uint32_t>(*rtp_timestamp_values[i]);
    header.ssrc = rtc::checked_cast<uint32_t>(*ssrc_values[i]);
    header.numCSRCs = 0;  // TODO(terelius): Implement CSRC.
    header.paddingLength = rtc::checked_cast<size_t>(*padding_size_values[i]);
    header.headerLength = rtc::checked_cast<size_t>(*header_size_values[i]);
    // TODO(terelius): Should we implement payload_type_frequency?
    if (transport_sequence_number_values.size() > i &&
        transport_sequence_number_values[i].has_value()) {
      header.extension.hasTransportSequenceNumber = true;
      header.extension.transportSequenceNumber = rtc::checked_cast<uint16_t>(
          transport_sequence_number_values[i].value());
    }
    if (transmission_time_offset_values.size() > i &&
        transmission_time_offset_values[i].has_value()) {
      header.extension.hasTransmissionTimeOffset = true;
      int32_t transmission_time_offset;
      RTC_CHECK(ToSigned(transmission_time_offset_values[i].value(),
                         &transmission_time_offset));
      header.extension.transmissionTimeOffset = transmission_time_offset;
    }
    if (absolute_send_time_values.size() > i &&
        absolute_send_time_values[i].has_value()) {
      header.extension.hasAbsoluteSendTime = true;
      header.extension.absoluteSendTime =
          rtc::checked_cast<uint32_t>(absolute_send_time_values[i].value());
    }
    if (video_rotation_values.size() > i &&
        video_rotation_values[i].has_value()) {
      header.extension.hasVideoRotation = true;
      header.extension.videoRotation = ConvertCVOByteToVideoRotation(
          rtc::checked_cast<uint8_t>(video_rotation_values[i].value()));
    }
    if (audio_level_values.size() > i && audio_level_values[i].has_value()) {
      RTC_CHECK(voice_activity_values.size() > i &&
                voice_activity_values[i].has_value());
      header.extension.hasAudioLevel = true;
      header.extension.voiceActivity =
          rtc::checked_cast<bool>(voice_activity_values[i].value());
      const uint8_t audio_level =
          rtc::checked_cast<uint8_t>(audio_level_values[i].value());
      RTC_CHECK_LE(audio_level, 0x7Fu);
      header.extension.audioLevel = audio_level;
    } else {
      RTC_CHECK(voice_activity_values.size() <= i ||
                !voice_activity_values[i].has_value());
    }
    (*rtp_packets_map)[header.ssrc].emplace_back(
        1000 * timestamp_ms, header, header.headerLength,
        payload_size_values[i].value() + header.headerLength +
            header.paddingLength);
  }
}

template <typename ProtoType, typename LoggedType>
void StoreRtcpPackets(const ProtoType& proto,
                      std::vector<LoggedType>* rtcp_packets) {
  RTC_CHECK(proto.has_timestamp_ms());
  RTC_CHECK(proto.has_raw_packet());

  // Base event
  rtcp_packets->emplace_back(proto.timestamp_ms() * 1000, proto.raw_packet());

  const size_t number_of_deltas =
      proto.has_number_of_deltas() ? proto.number_of_deltas() : 0u;
  if (number_of_deltas == 0) {
    return;
  }

  // timestamp_ms
  std::vector<absl::optional<uint64_t>> timestamp_ms_values =
      DecodeDeltas(proto.timestamp_ms_deltas(),
                   ToUnsigned(proto.timestamp_ms()), number_of_deltas);
  RTC_CHECK_EQ(timestamp_ms_values.size(), number_of_deltas);

  // raw_packet
  RTC_CHECK(proto.has_raw_packet_blobs());
  std::vector<absl::string_view> raw_packet_values =
      DecodeBlobs(proto.raw_packet_blobs(), number_of_deltas);
  RTC_CHECK_EQ(raw_packet_values.size(), number_of_deltas);

  // Delta decoding
  for (size_t i = 0; i < number_of_deltas; ++i) {
    RTC_CHECK(timestamp_ms_values[i].has_value());
    int64_t timestamp_ms;
    RTC_CHECK(ToSigned(timestamp_ms_values[i].value(), &timestamp_ms));

    rtcp_packets->emplace_back(
        1000 * timestamp_ms,
        reinterpret_cast<const uint8_t*>(raw_packet_values[i].data()),
        raw_packet_values[i].size());
  }
}

}  // namespace

LoggedRtcpPacket::LoggedRtcpPacket(uint64_t timestamp_us,
                                   const uint8_t* packet,
                                   size_t total_length)
    : timestamp_us(timestamp_us), raw_data(packet, packet + total_length) {}
LoggedRtcpPacket::LoggedRtcpPacket(uint64_t timestamp_us,
                                   const std::string& packet)
    : timestamp_us(timestamp_us), raw_data(packet.size()) {
  memcpy(raw_data.data(), packet.data(), packet.size());
}
LoggedRtcpPacket::LoggedRtcpPacket(const LoggedRtcpPacket& rhs) = default;
LoggedRtcpPacket::~LoggedRtcpPacket() = default;

LoggedVideoSendConfig::LoggedVideoSendConfig() = default;
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
  constexpr uint64_t kMaxEventSize = 10000000;  // Sanity check.
  std::vector<char> buffer(0xFFFF);

  RTC_DCHECK(stream.good());

  while (1) {
    // Check whether we have reached end of file.
    stream.peek();
    if (stream.eof()) {
      break;
    }

    // Read the next message tag. Protobuf defines the message tag as
    // (field_number << 3) | wire_type. In the legacy encoding, the field number
    // is supposed to be 1 and the wire type for a length-delimited field is 2.
    // In the new encoding we still expect the wire type to be 2, but the field
    // number will be greater than 1.
    constexpr uint64_t kExpectedV1Tag = (1 << 3) | 2;
    size_t bytes_written = 0;
    absl::optional<uint64_t> tag =
        ParseVarInt(stream, buffer.data(), &bytes_written);
    if (!tag) {
      RTC_LOG(LS_WARNING)
          << "Missing field tag from beginning of protobuf event.";
      return false;
    }
    constexpr uint64_t kWireTypeMask = 0x07;
    const uint64_t wire_type = *tag & kWireTypeMask;
    if (wire_type != 2) {
      RTC_LOG(LS_WARNING) << "Expected field tag with wire type 2 (length "
                             "delimited message). Found wire type "
                          << wire_type;
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
    if (buffer.size() < bytes_written + *message_length)
      buffer.resize(bytes_written + *message_length);
    stream.read(buffer.data() + bytes_written, *message_length);
    if (stream.gcount() != static_cast<int>(*message_length)) {
      RTC_LOG(LS_WARNING) << "Failed to read protobuf message from file.";
      return false;
    }
    size_t buffer_size = bytes_written + *message_length;

    if (*tag == kExpectedV1Tag) {
      // Parse the protobuf event from the buffer.
      rtclog::EventStream event_stream;
      if (!event_stream.ParseFromArray(buffer.data(), buffer_size)) {
        RTC_LOG(LS_WARNING)
            << "Failed to parse legacy-format protobuf message.";
        return false;
      }

      RTC_CHECK_EQ(event_stream.stream_size(), 1);
      StoreParsedLegacyEvent(event_stream.stream(0));
    } else {
      // Parse the protobuf event from the buffer.
      rtclog2::EventStream event_stream;
      if (!event_stream.ParseFromArray(buffer.data(), buffer_size)) {
        RTC_LOG(LS_WARNING) << "Failed to parse new-format protobuf message.";
        return false;
      }
      StoreParsedNewFormatEvent(event_stream);
    }
  }
  return true;
}

void ParsedRtcEventLogNew::StoreParsedLegacyEvent(const rtclog::Event& event) {
  RTC_CHECK(event.has_type());
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

  switch (event.type()) {
    case rtclog::Event::VIDEO_RECEIVER_CONFIG_EVENT: {
      rtclog::StreamConfig config = GetVideoReceiveConfig(event);
      video_recv_configs_.emplace_back(GetTimestamp(event), config);
      if (!config.rtp_extensions.empty()) {
        incoming_rtp_extensions_maps_[config.remote_ssrc] =
            RtpHeaderExtensionMap(config.rtp_extensions);
        // TODO(terelius): I don't understand the reason for configuring header
        // extensions for the local SSRC. I think it should be removed, but for
        // now I want to preserve the previous functionality.
        incoming_rtp_extensions_maps_[config.local_ssrc] =
            RtpHeaderExtensionMap(config.rtp_extensions);
      }
      incoming_video_ssrcs_.insert(config.remote_ssrc);
      incoming_video_ssrcs_.insert(config.rtx_ssrc);
      incoming_rtx_ssrcs_.insert(config.rtx_ssrc);
      break;
    }
    case rtclog::Event::VIDEO_SENDER_CONFIG_EVENT: {
      std::vector<rtclog::StreamConfig> configs = GetVideoSendConfig(event);
      video_send_configs_.emplace_back(GetTimestamp(event), configs);
      for (const auto& config : configs) {
        if (!config.rtp_extensions.empty()) {
          outgoing_rtp_extensions_maps_[config.local_ssrc] =
              RtpHeaderExtensionMap(config.rtp_extensions);
          outgoing_rtp_extensions_maps_[config.rtx_ssrc] =
              RtpHeaderExtensionMap(config.rtp_extensions);
        }
        outgoing_video_ssrcs_.insert(config.local_ssrc);
        outgoing_video_ssrcs_.insert(config.rtx_ssrc);
        outgoing_rtx_ssrcs_.insert(config.rtx_ssrc);
      }
      break;
    }
    case rtclog::Event::AUDIO_RECEIVER_CONFIG_EVENT: {
      rtclog::StreamConfig config = GetAudioReceiveConfig(event);
      audio_recv_configs_.emplace_back(GetTimestamp(event), config);
      if (!config.rtp_extensions.empty()) {
        incoming_rtp_extensions_maps_[config.remote_ssrc] =
            RtpHeaderExtensionMap(config.rtp_extensions);
        incoming_rtp_extensions_maps_[config.local_ssrc] =
            RtpHeaderExtensionMap(config.rtp_extensions);
      }
      incoming_audio_ssrcs_.insert(config.remote_ssrc);
      break;
    }
    case rtclog::Event::AUDIO_SENDER_CONFIG_EVENT: {
      rtclog::StreamConfig config = GetAudioSendConfig(event);
      audio_send_configs_.emplace_back(GetTimestamp(event), config);
      if (!config.rtp_extensions.empty()) {
        outgoing_rtp_extensions_maps_[config.local_ssrc] =
            RtpHeaderExtensionMap(config.rtp_extensions);
      }
      outgoing_audio_ssrcs_.insert(config.local_ssrc);
      break;
    }
    case rtclog::Event::RTP_EVENT: {
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
    case rtclog::Event::RTCP_EVENT: {
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
    case rtclog::Event::LOG_START: {
      start_log_events_.push_back(LoggedStartEvent(GetTimestamp(event)));
      break;
    }
    case rtclog::Event::LOG_END: {
      stop_log_events_.push_back(LoggedStopEvent(GetTimestamp(event)));
      break;
    }
    case rtclog::Event::AUDIO_PLAYOUT_EVENT: {
      LoggedAudioPlayoutEvent playout_event = GetAudioPlayout(event);
      audio_playout_events_[playout_event.ssrc].push_back(playout_event);
      break;
    }
    case rtclog::Event::LOSS_BASED_BWE_UPDATE: {
      bwe_loss_updates_.push_back(GetLossBasedBweUpdate(event));
      break;
    }
    case rtclog::Event::DELAY_BASED_BWE_UPDATE: {
      bwe_delay_updates_.push_back(GetDelayBasedBweUpdate(event));
      break;
    }
    case rtclog::Event::AUDIO_NETWORK_ADAPTATION_EVENT: {
      LoggedAudioNetworkAdaptationEvent ana_event =
          GetAudioNetworkAdaptation(event);
      audio_network_adaptation_events_.push_back(ana_event);
      break;
    }
    case rtclog::Event::BWE_PROBE_CLUSTER_CREATED_EVENT: {
      bwe_probe_cluster_created_events_.push_back(
          GetBweProbeClusterCreated(event));
      break;
    }
    case rtclog::Event::BWE_PROBE_RESULT_EVENT: {
      // Probe successes and failures are currently stored in the same proto
      // message, we are moving towards separate messages. Probe results
      // therefore need special treatment in the parser.
      RTC_CHECK(event.has_probe_result());
      RTC_CHECK(event.probe_result().has_result());
      if (event.probe_result().result() == rtclog::BweProbeResult::SUCCESS) {
        bwe_probe_success_events_.push_back(GetBweProbeSuccess(event));
      } else {
        bwe_probe_failure_events_.push_back(GetBweProbeFailure(event));
      }
      break;
    }
    case rtclog::Event::ALR_STATE_EVENT: {
      alr_state_events_.push_back(GetAlrState(event));
      break;
    }
    case rtclog::Event::ICE_CANDIDATE_PAIR_CONFIG: {
      ice_candidate_pair_configs_.push_back(GetIceCandidatePairConfig(event));
      break;
    }
    case rtclog::Event::ICE_CANDIDATE_PAIR_EVENT: {
      ice_candidate_pair_events_.push_back(GetIceCandidatePairEvent(event));
      break;
    }
    case rtclog::Event::UNKNOWN_EVENT: {
      break;
    }
  }
}


int64_t ParsedRtcEventLogNew::GetTimestamp(const rtclog::Event& event) const {
  RTC_CHECK(event.has_timestamp_us());
  return event.timestamp_us();
}

// The header must have space for at least IP_PACKET_SIZE bytes.
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
      feedback_adapter.ProcessTransportFeedback(
          rtcp_iterator->transport_feedback);
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
      rtc::SentPacket sent_packet;
      sent_packet.send_time_ms = rtp_packet.rtp.log_time_ms();
      sent_packet.info.packet_size_bytes = rtp_packet.rtp.total_length;
      if (rtp_packet.rtp.header.extension.hasTransportSequenceNumber) {
        feedback_adapter.AddPacket(
            rtp_packet.rtp.header.ssrc,
            rtp_packet.rtp.header.extension.transportSequenceNumber,
            rtp_packet.rtp.total_length, PacedPacketInfo());
        sent_packet.packet_id =
            rtp_packet.rtp.header.extension.transportSequenceNumber;
        sent_packet.info.included_in_feedback = true;
        sent_packet.info.included_in_allocation = true;
        feedback_adapter.ProcessSentPacket(sent_packet);
      } else {
        sent_packet.info.included_in_feedback = false;
        // TODO(srte): Make it possible to indicate that all packets are part of
        // allocation.
        sent_packet.info.included_in_allocation = false;
        feedback_adapter.ProcessSentPacket(sent_packet);
      }
      ++rtp_iterator;
    }
    time_us = std::min(NextRtpTime(), NextRtcpTime());
  }
  return rtp_rtcp_matched;
}

// Helper functions for new format start here
void ParsedRtcEventLogNew::StoreParsedNewFormatEvent(
    const rtclog2::EventStream& stream) {
  RTC_DCHECK_EQ(stream.stream_size(), 0);

  RTC_DCHECK_EQ(
      stream.incoming_rtp_packets_size() + stream.outgoing_rtp_packets_size() +
          stream.incoming_rtcp_packets_size() +
          stream.outgoing_rtcp_packets_size() +
          stream.audio_playout_events_size() + stream.begin_log_events_size() +
          stream.end_log_events_size() + stream.loss_based_bwe_updates_size() +
          stream.delay_based_bwe_updates_size() +
          stream.audio_network_adaptations_size() +
          stream.probe_clusters_size() + stream.probe_success_size() +
          stream.probe_failure_size() + stream.alr_states_size() +
          stream.ice_candidate_configs_size() +
          stream.ice_candidate_events_size() +
          stream.audio_recv_stream_configs_size() +
          stream.audio_send_stream_configs_size() +
          stream.video_recv_stream_configs_size() +
          stream.video_send_stream_configs_size(),
      1u);

  if (stream.incoming_rtp_packets_size() == 1) {
    StoreIncomingRtpPackets(stream.incoming_rtp_packets(0));
  } else if (stream.outgoing_rtp_packets_size() == 1) {
    StoreOutgoingRtpPackets(stream.outgoing_rtp_packets(0));
  } else if (stream.incoming_rtcp_packets_size() == 1) {
    StoreIncomingRtcpPackets(stream.incoming_rtcp_packets(0));
  } else if (stream.outgoing_rtcp_packets_size() == 1) {
    StoreOutgoingRtcpPackets(stream.outgoing_rtcp_packets(0));
  } else if (stream.audio_playout_events_size() == 1) {
    StoreAudioPlayoutEvent(stream.audio_playout_events(0));
  } else if (stream.begin_log_events_size() == 1) {
    StoreStartEvent(stream.begin_log_events(0));
  } else if (stream.end_log_events_size() == 1) {
    StoreStopEvent(stream.end_log_events(0));
  } else if (stream.loss_based_bwe_updates_size() == 1) {
    StoreBweLossBasedUpdate(stream.loss_based_bwe_updates(0));
  } else if (stream.delay_based_bwe_updates_size() == 1) {
    StoreBweDelayBasedUpdate(stream.delay_based_bwe_updates(0));
  } else if (stream.audio_network_adaptations_size() == 1) {
    StoreAudioNetworkAdaptationEvent(stream.audio_network_adaptations(0));
  } else if (stream.probe_clusters_size() == 1) {
    StoreBweProbeClusterCreated(stream.probe_clusters(0));
  } else if (stream.probe_success_size() == 1) {
    StoreBweProbeSuccessEvent(stream.probe_success(0));
  } else if (stream.probe_failure_size() == 1) {
    StoreBweProbeFailureEvent(stream.probe_failure(0));
  } else if (stream.alr_states_size() == 1) {
    StoreAlrStateEvent(stream.alr_states(0));
  } else if (stream.ice_candidate_configs_size() == 1) {
    StoreIceCandidatePairConfig(stream.ice_candidate_configs(0));
  } else if (stream.ice_candidate_events_size() == 1) {
    StoreIceCandidateEvent(stream.ice_candidate_events(0));
  } else if (stream.audio_recv_stream_configs_size() == 1) {
    StoreAudioRecvConfig(stream.audio_recv_stream_configs(0));
  } else if (stream.audio_send_stream_configs_size() == 1) {
    StoreAudioSendConfig(stream.audio_send_stream_configs(0));
  } else if (stream.video_recv_stream_configs_size() == 1) {
    StoreVideoRecvConfig(stream.video_recv_stream_configs(0));
  } else if (stream.video_send_stream_configs_size() == 1) {
    StoreVideoSendConfig(stream.video_send_stream_configs(0));
  } else {
    RTC_NOTREACHED();
  }
}

void ParsedRtcEventLogNew::StoreAlrStateEvent(const rtclog2::AlrState& proto) {
  RTC_CHECK(proto.has_timestamp_ms());
  RTC_CHECK(proto.has_in_alr());
  LoggedAlrStateEvent alr_event;
  alr_event.timestamp_us = proto.timestamp_ms() * 1000;
  alr_event.in_alr = proto.in_alr();

  alr_state_events_.push_back(alr_event);
  // TODO(terelius): Should we delta encode this event type?
}

void ParsedRtcEventLogNew::StoreAudioPlayoutEvent(
    const rtclog2::AudioPlayoutEvents& proto) {
  RTC_CHECK(proto.has_timestamp_ms());
  RTC_CHECK(proto.has_local_ssrc());

  // Base event
  auto map_it = audio_playout_events_[proto.local_ssrc()];
  audio_playout_events_[proto.local_ssrc()].emplace_back(
      1000 * proto.timestamp_ms(), proto.local_ssrc());

  const size_t number_of_deltas =
      proto.has_number_of_deltas() ? proto.number_of_deltas() : 0u;
  if (number_of_deltas == 0) {
    return;
  }

  // timestamp_ms
  std::vector<absl::optional<uint64_t>> timestamp_ms_values =
      DecodeDeltas(proto.timestamp_ms_deltas(),
                   ToUnsigned(proto.timestamp_ms()), number_of_deltas);
  RTC_CHECK_EQ(timestamp_ms_values.size(), number_of_deltas);

  // local_ssrc
  std::vector<absl::optional<uint64_t>> local_ssrc_values = DecodeDeltas(
      proto.local_ssrc_deltas(), proto.local_ssrc(), number_of_deltas);
  RTC_CHECK_EQ(local_ssrc_values.size(), number_of_deltas);

  // Delta decoding
  for (size_t i = 0; i < number_of_deltas; ++i) {
    RTC_CHECK(timestamp_ms_values[i].has_value());
    RTC_CHECK(local_ssrc_values[i].has_value());
    RTC_CHECK_LE(local_ssrc_values[i].value(),
                 std::numeric_limits<uint32_t>::max());

    int64_t timestamp_ms;
    RTC_CHECK(ToSigned(timestamp_ms_values[i].value(), &timestamp_ms));

    const uint32_t local_ssrc =
        static_cast<uint32_t>(local_ssrc_values[i].value());
    audio_playout_events_[local_ssrc].emplace_back(1000 * timestamp_ms,
                                                   local_ssrc);
  }
}

void ParsedRtcEventLogNew::StoreIncomingRtpPackets(
    const rtclog2::IncomingRtpPackets& proto) {
  StoreRtpPackets(proto, &incoming_rtp_packets_map_);
}

void ParsedRtcEventLogNew::StoreOutgoingRtpPackets(
    const rtclog2::OutgoingRtpPackets& proto) {
  StoreRtpPackets(proto, &outgoing_rtp_packets_map_);
}

void ParsedRtcEventLogNew::StoreIncomingRtcpPackets(
    const rtclog2::IncomingRtcpPackets& proto) {
  StoreRtcpPackets(proto, &incoming_rtcp_packets_);
}

void ParsedRtcEventLogNew::StoreOutgoingRtcpPackets(
    const rtclog2::OutgoingRtcpPackets& proto) {
  StoreRtcpPackets(proto, &outgoing_rtcp_packets_);
}

void ParsedRtcEventLogNew::StoreStartEvent(
    const rtclog2::BeginLogEvent& proto) {
  RTC_CHECK(proto.has_timestamp_ms());
  RTC_CHECK(proto.has_version());
  RTC_CHECK(proto.has_utc_time_ms());
  RTC_CHECK_EQ(proto.version(), 2);
  LoggedStartEvent start_event(proto.timestamp_ms() * 1000,
                               proto.utc_time_ms());

  start_log_events_.push_back(start_event);
}

void ParsedRtcEventLogNew::StoreStopEvent(const rtclog2::EndLogEvent& proto) {
  RTC_CHECK(proto.has_timestamp_ms());
  LoggedStopEvent stop_event(proto.timestamp_ms() * 1000);

  stop_log_events_.push_back(stop_event);
}

void ParsedRtcEventLogNew::StoreBweLossBasedUpdate(
    const rtclog2::LossBasedBweUpdates& proto) {
  RTC_CHECK(proto.has_timestamp_ms());
  RTC_CHECK(proto.has_bitrate_bps());
  RTC_CHECK(proto.has_fraction_loss());
  RTC_CHECK(proto.has_total_packets());

  // Base event
  bwe_loss_updates_.emplace_back(1000 * proto.timestamp_ms(),
                                 proto.bitrate_bps(), proto.fraction_loss(),
                                 proto.total_packets());

  const size_t number_of_deltas =
      proto.has_number_of_deltas() ? proto.number_of_deltas() : 0u;
  if (number_of_deltas == 0) {
    return;
  }

  // timestamp_ms
  std::vector<absl::optional<uint64_t>> timestamp_ms_values =
      DecodeDeltas(proto.timestamp_ms_deltas(),
                   ToUnsigned(proto.timestamp_ms()), number_of_deltas);
  RTC_CHECK_EQ(timestamp_ms_values.size(), number_of_deltas);

  // bitrate_bps
  std::vector<absl::optional<uint64_t>> bitrate_bps_values = DecodeDeltas(
      proto.bitrate_bps_deltas(), proto.bitrate_bps(), number_of_deltas);
  RTC_CHECK_EQ(bitrate_bps_values.size(), number_of_deltas);

  // fraction_loss
  std::vector<absl::optional<uint64_t>> fraction_loss_values = DecodeDeltas(
      proto.fraction_loss_deltas(), proto.fraction_loss(), number_of_deltas);
  RTC_CHECK_EQ(fraction_loss_values.size(), number_of_deltas);

  // total_packets
  std::vector<absl::optional<uint64_t>> total_packets_values = DecodeDeltas(
      proto.total_packets_deltas(), proto.total_packets(), number_of_deltas);
  RTC_CHECK_EQ(total_packets_values.size(), number_of_deltas);

  // Delta decoding
  for (size_t i = 0; i < number_of_deltas; ++i) {
    RTC_CHECK(timestamp_ms_values[i].has_value());
    int64_t timestamp_ms;
    RTC_CHECK(ToSigned(timestamp_ms_values[i].value(), &timestamp_ms));

    RTC_CHECK(bitrate_bps_values[i].has_value());
    RTC_CHECK_LE(bitrate_bps_values[i].value(),
                 std::numeric_limits<uint32_t>::max());
    const uint32_t bitrate_bps =
        static_cast<uint32_t>(bitrate_bps_values[i].value());

    RTC_CHECK(fraction_loss_values[i].has_value());
    RTC_CHECK_LE(fraction_loss_values[i].value(),
                 std::numeric_limits<uint32_t>::max());
    const uint32_t fraction_loss =
        static_cast<uint32_t>(fraction_loss_values[i].value());

    RTC_CHECK(total_packets_values[i].has_value());
    RTC_CHECK_LE(total_packets_values[i].value(),
                 std::numeric_limits<uint32_t>::max());
    const uint32_t total_packets =
        static_cast<uint32_t>(total_packets_values[i].value());

    bwe_loss_updates_.emplace_back(1000 * timestamp_ms, bitrate_bps,
                                   fraction_loss, total_packets);
  }
}

void ParsedRtcEventLogNew::StoreBweDelayBasedUpdate(
    const rtclog2::DelayBasedBweUpdates& proto) {
  RTC_CHECK(proto.has_timestamp_ms());
  RTC_CHECK(proto.has_bitrate_bps());
  RTC_CHECK(proto.has_detector_state());

  // Base event
  const BandwidthUsage base_detector_state =
      GetRuntimeDetectorState(proto.detector_state());
  bwe_delay_updates_.emplace_back(1000 * proto.timestamp_ms(),
                                  proto.bitrate_bps(), base_detector_state);

  const size_t number_of_deltas =
      proto.has_number_of_deltas() ? proto.number_of_deltas() : 0u;
  if (number_of_deltas == 0) {
    return;
  }

  // timestamp_ms
  std::vector<absl::optional<uint64_t>> timestamp_ms_values =
      DecodeDeltas(proto.timestamp_ms_deltas(),
                   ToUnsigned(proto.timestamp_ms()), number_of_deltas);
  RTC_CHECK_EQ(timestamp_ms_values.size(), number_of_deltas);

  // bitrate_bps
  std::vector<absl::optional<uint64_t>> bitrate_bps_values = DecodeDeltas(
      proto.bitrate_bps_deltas(), proto.bitrate_bps(), number_of_deltas);
  RTC_CHECK_EQ(bitrate_bps_values.size(), number_of_deltas);

  // detector_state
  std::vector<absl::optional<uint64_t>> detector_state_values = DecodeDeltas(
      proto.detector_state_deltas(),
      static_cast<uint64_t>(proto.detector_state()), number_of_deltas);
  RTC_CHECK_EQ(detector_state_values.size(), number_of_deltas);

  // Delta decoding
  for (size_t i = 0; i < number_of_deltas; ++i) {
    RTC_CHECK(timestamp_ms_values[i].has_value());
    int64_t timestamp_ms;
    RTC_CHECK(ToSigned(timestamp_ms_values[i].value(), &timestamp_ms));

    RTC_CHECK(bitrate_bps_values[i].has_value());
    RTC_CHECK_LE(bitrate_bps_values[i].value(),
                 std::numeric_limits<uint32_t>::max());
    const uint32_t bitrate_bps =
        static_cast<uint32_t>(bitrate_bps_values[i].value());

    RTC_CHECK(detector_state_values[i].has_value());
    const auto detector_state =
        static_cast<rtclog2::DelayBasedBweUpdates::DetectorState>(
            detector_state_values[i].value());

    bwe_delay_updates_.emplace_back(1000 * timestamp_ms, bitrate_bps,
                                    GetRuntimeDetectorState(detector_state));
  }
}

void ParsedRtcEventLogNew::StoreBweProbeClusterCreated(
    const rtclog2::BweProbeCluster& proto) {
  LoggedBweProbeClusterCreatedEvent probe_cluster;
  RTC_CHECK(proto.has_timestamp_ms());
  probe_cluster.timestamp_us = proto.timestamp_ms() * 1000;
  RTC_CHECK(proto.has_id());
  probe_cluster.id = proto.id();
  RTC_CHECK(proto.has_bitrate_bps());
  probe_cluster.bitrate_bps = proto.bitrate_bps();
  RTC_CHECK(proto.has_min_packets());
  probe_cluster.min_packets = proto.min_packets();
  RTC_CHECK(proto.has_min_bytes());
  probe_cluster.min_bytes = proto.min_bytes();

  bwe_probe_cluster_created_events_.push_back(probe_cluster);

  // TODO(terelius): Should we delta encode this event type?
}

void ParsedRtcEventLogNew::StoreBweProbeSuccessEvent(
    const rtclog2::BweProbeResultSuccess& proto) {
  LoggedBweProbeSuccessEvent probe_result;
  RTC_CHECK(proto.has_timestamp_ms());
  probe_result.timestamp_us = proto.timestamp_ms() * 1000;
  RTC_CHECK(proto.has_id());
  probe_result.id = proto.id();
  RTC_CHECK(proto.has_bitrate_bps());
  probe_result.bitrate_bps = proto.bitrate_bps();

  bwe_probe_success_events_.push_back(probe_result);

  // TODO(terelius): Should we delta encode this event type?
}

void ParsedRtcEventLogNew::StoreBweProbeFailureEvent(
    const rtclog2::BweProbeResultFailure& proto) {
  LoggedBweProbeFailureEvent probe_result;
  RTC_CHECK(proto.has_timestamp_ms());
  probe_result.timestamp_us = proto.timestamp_ms() * 1000;
  RTC_CHECK(proto.has_id());
  probe_result.id = proto.id();
  RTC_CHECK(proto.has_failure());
  probe_result.failure_reason = GetRuntimeProbeFailureReason(proto.failure());

  bwe_probe_failure_events_.push_back(probe_result);

  // TODO(terelius): Should we delta encode this event type?
}

void ParsedRtcEventLogNew::StoreAudioNetworkAdaptationEvent(
    const rtclog2::AudioNetworkAdaptations& proto) {
  RTC_CHECK(proto.has_timestamp_ms());

  // Base event
  {
    AudioEncoderRuntimeConfig runtime_config;
    if (proto.has_bitrate_bps()) {
      runtime_config.bitrate_bps = proto.bitrate_bps();
    }
    if (proto.has_frame_length_ms()) {
      runtime_config.frame_length_ms = proto.frame_length_ms();
    }
    if (proto.has_uplink_packet_loss_fraction()) {
      float uplink_packet_loss_fraction;
      RTC_CHECK(ParsePacketLossFractionFromProtoFormat(
          proto.uplink_packet_loss_fraction(), &uplink_packet_loss_fraction));
      runtime_config.uplink_packet_loss_fraction = uplink_packet_loss_fraction;
    }
    if (proto.has_enable_fec()) {
      runtime_config.enable_fec = proto.enable_fec();
    }
    if (proto.has_enable_dtx()) {
      runtime_config.enable_dtx = proto.enable_dtx();
    }
    if (proto.has_num_channels()) {
      // Note: Encoding N as N-1 only done for |num_channels_deltas|.
      runtime_config.num_channels = proto.num_channels();
    }
    audio_network_adaptation_events_.emplace_back(1000 * proto.timestamp_ms(),
                                                  runtime_config);
  }

  const size_t number_of_deltas =
      proto.has_number_of_deltas() ? proto.number_of_deltas() : 0u;
  if (number_of_deltas == 0) {
    return;
  }

  // timestamp_ms
  std::vector<absl::optional<uint64_t>> timestamp_ms_values =
      DecodeDeltas(proto.timestamp_ms_deltas(),
                   ToUnsigned(proto.timestamp_ms()), number_of_deltas);
  RTC_CHECK_EQ(timestamp_ms_values.size(), number_of_deltas);

  // bitrate_bps
  const absl::optional<uint64_t> unsigned_base_bitrate_bps =
      proto.has_bitrate_bps()
          ? absl::optional<uint64_t>(ToUnsigned(proto.bitrate_bps()))
          : absl::optional<uint64_t>();
  std::vector<absl::optional<uint64_t>> bitrate_bps_values = DecodeDeltas(
      proto.bitrate_bps_deltas(), unsigned_base_bitrate_bps, number_of_deltas);
  RTC_CHECK_EQ(bitrate_bps_values.size(), number_of_deltas);

  // frame_length_ms
  const absl::optional<uint64_t> unsigned_base_frame_length_ms =
      proto.has_frame_length_ms()
          ? absl::optional<uint64_t>(ToUnsigned(proto.frame_length_ms()))
          : absl::optional<uint64_t>();
  std::vector<absl::optional<uint64_t>> frame_length_ms_values =
      DecodeDeltas(proto.frame_length_ms_deltas(),
                   unsigned_base_frame_length_ms, number_of_deltas);
  RTC_CHECK_EQ(frame_length_ms_values.size(), number_of_deltas);

  // uplink_packet_loss_fraction
  const absl::optional<uint64_t> uplink_packet_loss_fraction =
      proto.has_uplink_packet_loss_fraction()
          ? absl::optional<uint64_t>(proto.uplink_packet_loss_fraction())
          : absl::optional<uint64_t>();
  std::vector<absl::optional<uint64_t>> uplink_packet_loss_fraction_values =
      DecodeDeltas(proto.uplink_packet_loss_fraction_deltas(),
                   uplink_packet_loss_fraction, number_of_deltas);
  RTC_CHECK_EQ(uplink_packet_loss_fraction_values.size(), number_of_deltas);

  // enable_fec
  const absl::optional<uint64_t> enable_fec =
      proto.has_enable_fec() ? absl::optional<uint64_t>(proto.enable_fec())
                             : absl::optional<uint64_t>();
  std::vector<absl::optional<uint64_t>> enable_fec_values =
      DecodeDeltas(proto.enable_fec_deltas(), enable_fec, number_of_deltas);
  RTC_CHECK_EQ(enable_fec_values.size(), number_of_deltas);

  // enable_dtx
  const absl::optional<uint64_t> enable_dtx =
      proto.has_enable_dtx() ? absl::optional<uint64_t>(proto.enable_dtx())
                             : absl::optional<uint64_t>();
  std::vector<absl::optional<uint64_t>> enable_dtx_values =
      DecodeDeltas(proto.enable_dtx_deltas(), enable_dtx, number_of_deltas);
  RTC_CHECK_EQ(enable_dtx_values.size(), number_of_deltas);

  // num_channels
  // Note: For delta encoding, all num_channel values, including the base,
  // were shifted down by one, but in the base event, they were not.
  // We likewise shift the base event down by one, to get the same base as
  // encoding had, but then shift all of the values (except the base) back up
  //  to their original value.
  absl::optional<uint64_t> shifted_base_num_channels;
  if (proto.has_num_channels()) {
    shifted_base_num_channels =
        absl::optional<uint64_t>(proto.num_channels() - 1);
  }
  std::vector<absl::optional<uint64_t>> num_channels_values = DecodeDeltas(
      proto.num_channels_deltas(), shifted_base_num_channels, number_of_deltas);
  for (size_t i = 0; i < num_channels_values.size(); ++i) {
    if (num_channels_values[i].has_value()) {
      num_channels_values[i] = num_channels_values[i].value() + 1;
    }
  }
  RTC_CHECK_EQ(num_channels_values.size(), number_of_deltas);

  // Delta decoding
  for (size_t i = 0; i < number_of_deltas; ++i) {
    RTC_CHECK(timestamp_ms_values[i].has_value());
    int64_t timestamp_ms;
    RTC_CHECK(ToSigned(timestamp_ms_values[i].value(), &timestamp_ms));

    AudioEncoderRuntimeConfig runtime_config;
    if (bitrate_bps_values[i].has_value()) {
      int signed_bitrate_bps;
      RTC_CHECK(ToSigned(bitrate_bps_values[i].value(), &signed_bitrate_bps));
      runtime_config.bitrate_bps = signed_bitrate_bps;
    }
    if (frame_length_ms_values[i].has_value()) {
      int signed_frame_length_ms;
      RTC_CHECK(
          ToSigned(frame_length_ms_values[i].value(), &signed_frame_length_ms));
      runtime_config.frame_length_ms = signed_frame_length_ms;
    }
    if (uplink_packet_loss_fraction_values[i].has_value()) {
      float uplink_packet_loss_fraction;
      RTC_CHECK(ParsePacketLossFractionFromProtoFormat(
          rtc::checked_cast<uint32_t>(
              uplink_packet_loss_fraction_values[i].value()),
          &uplink_packet_loss_fraction));
      runtime_config.uplink_packet_loss_fraction = uplink_packet_loss_fraction;
    }
    if (enable_fec_values[i].has_value()) {
      runtime_config.enable_fec =
          rtc::checked_cast<bool>(enable_fec_values[i].value());
    }
    if (enable_dtx_values[i].has_value()) {
      runtime_config.enable_dtx =
          rtc::checked_cast<bool>(enable_dtx_values[i].value());
    }
    if (num_channels_values[i].has_value()) {
      runtime_config.num_channels =
          rtc::checked_cast<size_t>(num_channels_values[i].value());
    }
    audio_network_adaptation_events_.emplace_back(1000 * timestamp_ms,
                                                  runtime_config);
  }
}

void ParsedRtcEventLogNew::StoreIceCandidatePairConfig(
    const rtclog2::IceCandidatePairConfig& proto) {
  LoggedIceCandidatePairConfig ice_config;
  RTC_CHECK(proto.has_timestamp_ms());
  ice_config.timestamp_us = proto.timestamp_ms() * 1000;

  RTC_CHECK(proto.has_config_type());
  ice_config.type = GetRuntimeIceCandidatePairConfigType(proto.config_type());
  RTC_CHECK(proto.has_candidate_pair_id());
  ice_config.candidate_pair_id = proto.candidate_pair_id();
  RTC_CHECK(proto.has_local_candidate_type());
  ice_config.local_candidate_type =
      GetRuntimeIceCandidateType(proto.local_candidate_type());
  RTC_CHECK(proto.has_local_relay_protocol());
  ice_config.local_relay_protocol =
      GetRuntimeIceCandidatePairProtocol(proto.local_relay_protocol());
  RTC_CHECK(proto.has_local_network_type());
  ice_config.local_network_type =
      GetRuntimeIceCandidateNetworkType(proto.local_network_type());
  RTC_CHECK(proto.has_local_address_family());
  ice_config.local_address_family =
      GetRuntimeIceCandidatePairAddressFamily(proto.local_address_family());
  RTC_CHECK(proto.has_remote_candidate_type());
  ice_config.remote_candidate_type =
      GetRuntimeIceCandidateType(proto.remote_candidate_type());
  RTC_CHECK(proto.has_remote_address_family());
  ice_config.remote_address_family =
      GetRuntimeIceCandidatePairAddressFamily(proto.remote_address_family());
  RTC_CHECK(proto.has_candidate_pair_protocol());
  ice_config.candidate_pair_protocol =
      GetRuntimeIceCandidatePairProtocol(proto.candidate_pair_protocol());

  ice_candidate_pair_configs_.push_back(ice_config);

  // TODO(terelius): Should we delta encode this event type?
}

void ParsedRtcEventLogNew::StoreIceCandidateEvent(
    const rtclog2::IceCandidatePairEvent& proto) {
  LoggedIceCandidatePairEvent ice_event;
  RTC_CHECK(proto.has_timestamp_ms());
  ice_event.timestamp_us = proto.timestamp_ms() * 1000;
  RTC_CHECK(proto.has_event_type());
  ice_event.type = GetRuntimeIceCandidatePairEventType(proto.event_type());
  RTC_CHECK(proto.has_candidate_pair_id());
  ice_event.candidate_pair_id = proto.candidate_pair_id();

  ice_candidate_pair_events_.push_back(ice_event);

  // TODO(terelius): Should we delta encode this event type?
}

void ParsedRtcEventLogNew::StoreVideoRecvConfig(
    const rtclog2::VideoRecvStreamConfig& proto) {
  LoggedVideoRecvConfig stream;
  RTC_CHECK(proto.has_timestamp_ms());
  stream.timestamp_us = proto.timestamp_ms() * 1000;
  RTC_CHECK(proto.has_remote_ssrc());
  stream.config.remote_ssrc = proto.remote_ssrc();
  RTC_CHECK(proto.has_local_ssrc());
  stream.config.local_ssrc = proto.local_ssrc();
  if (proto.has_rtx_ssrc()) {
    stream.config.rtx_ssrc = proto.rtx_ssrc();
  }
  if (proto.has_header_extensions()) {
    stream.config.rtp_extensions =
        GetRuntimeRtpHeaderExtensionConfig(proto.header_extensions());
  }
  video_recv_configs_.push_back(stream);
}

void ParsedRtcEventLogNew::StoreVideoSendConfig(
    const rtclog2::VideoSendStreamConfig& proto) {
  LoggedVideoSendConfig stream;
  RTC_CHECK(proto.has_timestamp_ms());
  stream.timestamp_us = proto.timestamp_ms() * 1000;
  rtclog::StreamConfig config;
  RTC_CHECK(proto.has_ssrc());
  config.local_ssrc = proto.ssrc();
  if (proto.has_rtx_ssrc()) {
    config.rtx_ssrc = proto.rtx_ssrc();
  }
  if (proto.has_header_extensions()) {
    config.rtp_extensions =
        GetRuntimeRtpHeaderExtensionConfig(proto.header_extensions());
  }
  stream.configs.push_back(config);
  video_send_configs_.push_back(stream);
}

void ParsedRtcEventLogNew::StoreAudioRecvConfig(
    const rtclog2::AudioRecvStreamConfig& proto) {
  LoggedAudioRecvConfig stream;
  RTC_CHECK(proto.has_timestamp_ms());
  stream.timestamp_us = proto.timestamp_ms() * 1000;
  RTC_CHECK(proto.has_remote_ssrc());
  stream.config.remote_ssrc = proto.remote_ssrc();
  RTC_CHECK(proto.has_local_ssrc());
  stream.config.local_ssrc = proto.local_ssrc();
  if (proto.has_header_extensions()) {
    stream.config.rtp_extensions =
        GetRuntimeRtpHeaderExtensionConfig(proto.header_extensions());
  }
  audio_recv_configs_.push_back(stream);
}

void ParsedRtcEventLogNew::StoreAudioSendConfig(
    const rtclog2::AudioSendStreamConfig& proto) {
  LoggedAudioSendConfig stream;
  RTC_CHECK(proto.has_timestamp_ms());
  stream.timestamp_us = proto.timestamp_ms() * 1000;
  RTC_CHECK(proto.has_ssrc());
  stream.config.local_ssrc = proto.ssrc();
  if (proto.has_header_extensions()) {
    stream.config.rtp_extensions =
        GetRuntimeRtpHeaderExtensionConfig(proto.header_extensions());
  }
  audio_send_configs_.push_back(stream);
}

}  // namespace webrtc
