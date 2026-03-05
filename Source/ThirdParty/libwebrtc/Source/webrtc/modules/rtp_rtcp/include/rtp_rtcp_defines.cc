/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"

#include <cctype>
#include <cstring>

#include "absl/algorithm/container.h"
#include "absl/strings/string_view.h"
#include "api/transport/ecn_marking.h"
#include "api/units/time_delta.h"
#include "modules/rtp_rtcp/source/rtp_packet.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "modules/rtp_rtcp/source/rtp_packet_to_send.h"

namespace webrtc {

namespace {
constexpr size_t kMidRsidMaxSize = 16;

// Check if passed character is a "token-char" from RFC 4566.
// https://datatracker.ietf.org/doc/html/rfc4566#section-9
//    token-char =          %x21 / %x23-27 / %x2A-2B / %x2D-2E / %x30-39
//                         / %x41-5A / %x5E-7E
bool IsTokenChar(char ch) {
  return ch == 0x21 || (ch >= 0x23 && ch <= 0x27) || ch == 0x2a || ch == 0x2b ||
         ch == 0x2d || ch == 0x2e || (ch >= 0x30 && ch <= 0x39) ||
         (ch >= 0x41 && ch <= 0x5a) || (ch >= 0x5e && ch <= 0x7e);
}
}  // namespace

bool IsLegalMidName(absl::string_view name) {
  return (name.size() <= kMidRsidMaxSize && !name.empty() &&
          absl::c_all_of(name, IsTokenChar));
}

bool IsLegalRsidName(absl::string_view name) {
  return (name.size() <= kMidRsidMaxSize && !name.empty() &&
          absl::c_all_of(name, isalnum));
}

StreamDataCounters::StreamDataCounters() = default;

RtpPacketCounter::RtpPacketCounter(const RtpPacket& packet)
    : header_bytes(packet.headers_size()),
      payload_bytes(packet.payload_size()),
      padding_bytes(packet.padding_size()),
      packets(1),
      packets_with_ect1(0),
      packets_with_ce(0) {}

RtpPacketCounter::RtpPacketCounter(const RtpPacketToSend& packet_to_send)
    : RtpPacketCounter(static_cast<const RtpPacket&>(packet_to_send)) {
  total_packet_delay =
      packet_to_send.time_in_send_queue().value_or(TimeDelta::Zero());
  if (packet_to_send.send_as_ect1()) {
    ++packets_with_ect1;
  }
}

RtpPacketCounter::RtpPacketCounter(const RtpPacketReceived& packet_received)
    : RtpPacketCounter(static_cast<const RtpPacket&>(packet_received)) {
  EcnMarking ecn = packet_received.ecn();
  if (ecn == EcnMarking::kEct1) {
    ++packets_with_ect1;
  } else if (ecn == EcnMarking::kCe) {
    ++packets_with_ce;
  }
}

void RtpPacketCounter::AddPacket(const RtpPacket& packet) {
  ++packets;
  header_bytes += packet.headers_size();
  padding_bytes += packet.padding_size();
  payload_bytes += packet.payload_size();
}

void RtpPacketCounter::AddPacket(const RtpPacketToSend& packet_to_send) {
  AddPacket(static_cast<const RtpPacket&>(packet_to_send));
  total_packet_delay +=
      packet_to_send.time_in_send_queue().value_or(TimeDelta::Zero());
  if (packet_to_send.send_as_ect1()) {
    ++packets_with_ect1;
  }
}

void RtpPacketCounter::AddPacket(const RtpPacketReceived& packet_received) {
  AddPacket(static_cast<const RtpPacket&>(packet_received));
  EcnMarking ecn = packet_received.ecn();
  if (ecn == EcnMarking::kEct1) {
    ++packets_with_ect1;
  } else if (ecn == EcnMarking::kCe) {
    ++packets_with_ce;
  }
}

}  // namespace webrtc
