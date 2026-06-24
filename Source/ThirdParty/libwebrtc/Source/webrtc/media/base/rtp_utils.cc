/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media/base/rtp_utils.h"

#include <cstdint>
#include <cstring>
#include <span>

// PacketTimeUpdateParams is defined in asyncpacketsocket.h.
// TODO(sergeyu): Find more appropriate place for PacketTimeUpdateParams.

#include "absl/strings/string_view.h"
#include "modules/rtp_rtcp/source/rtp_util.h"
#include "rtc_base/byte_order.h"
#include "rtc_base/checks.h"

namespace webrtc {

static const size_t kRtcpPayloadTypeOffset = 1;
static const size_t kRtpExtensionHeaderLen = 4;

namespace {

bool GetUint8(const void* data, size_t offset, int* value) {
  if (!data || !value) {
    return false;
  }
  *value = *(static_cast<const uint8_t*>(data) + offset);
  return true;
}

}  // namespace

bool GetRtcpType(const void* data, size_t len, int* value) {
  if (len < kMinRtcpPacketLen) {
    return false;
  }
  return GetUint8(data, kRtcpPayloadTypeOffset, value);
}

// This method returns SSRC first of RTCP packet, except if packet is SDES.
// TODO(mallinath) - Fully implement RFC 5506. This standard doesn't restrict
// to send non-compound packets only to feedback messages.
bool GetRtcpSsrc(const void* data, size_t len, uint32_t* value) {
  // Packet should be at least of 8 bytes, to get SSRC from a RTCP packet.
  if (!data || len < kMinRtcpPacketLen + 4 || !value)
    return false;
  int pl_type;
  if (!GetRtcpType(data, len, &pl_type))
    return false;
  // SDES packet parsing is not supported.
  if (pl_type == kRtcpTypeSDES)
    return false;
  *value = GetBE32(
      std::span<const uint8_t>(static_cast<const uint8_t*>(data) + 4, 4));
  return true;
}

bool IsValidRtpPayloadType(int payload_type) {
  return payload_type >= 0 && payload_type <= 127;
}

bool IsValidRtpPacketSize(RtpPacketType packet_type, size_t size) {
  RTC_DCHECK_NE(RtpPacketType::kUnknown, packet_type);
  size_t min_packet_length = packet_type == RtpPacketType::kRtcp
                                 ? kMinRtcpPacketLen
                                 : kMinRtpPacketLen;
  return size >= min_packet_length && size <= kMaxRtpPacketLen;
}

absl::string_view RtpPacketTypeToString(RtpPacketType packet_type) {
  switch (packet_type) {
    case RtpPacketType::kRtp:
      return "RTP";
    case RtpPacketType::kRtcp:
      return "RTCP";
    case RtpPacketType::kUnknown:
      return "Unknown";
  }
  RTC_CHECK_NOTREACHED();
}

RtpPacketType InferRtpPacketType(std::span<const uint8_t> packet) {
  if (IsRtcpPacket(packet)) {
    return RtpPacketType::kRtcp;
  }
  if (IsRtpPacket(packet)) {
    return RtpPacketType::kRtp;
  }
  return RtpPacketType::kUnknown;
}

bool ValidateRtpHeader(std::span<const uint8_t> rtp, size_t* header_length) {
  size_t length = rtp.size();
  if (header_length) {
    *header_length = 0;
  }

  if (length < kMinRtpPacketLen) {
    return false;
  }

  size_t cc_count = rtp[0] & 0x0F;
  size_t header_length_without_extension = kMinRtpPacketLen + 4 * cc_count;
  if (header_length_without_extension > length) {
    return false;
  }

  // If extension bit is not set, we are done with header processing, as input
  // length is verified above.
  if (!(rtp[0] & 0x10)) {
    if (header_length)
      *header_length = header_length_without_extension;

    return true;
  }

  if (header_length_without_extension + kRtpExtensionHeaderLen > length) {
    return false;
  }

  // Getting extension profile length.
  // Length is in 32 bit words.
  uint16_t extension_length_in_32bits =
      GetBE16(rtp.subspan(header_length_without_extension + 2, 2));
  size_t extension_length = extension_length_in_32bits * 4;

  size_t rtp_header_length = extension_length +
                             header_length_without_extension +
                             kRtpExtensionHeaderLen;

  // Verify input length against total header size.
  if (rtp_header_length > length) {
    return false;
  }

  if (header_length) {
    *header_length = rtp_header_length;
  }
  return true;
}
}  // namespace webrtc
