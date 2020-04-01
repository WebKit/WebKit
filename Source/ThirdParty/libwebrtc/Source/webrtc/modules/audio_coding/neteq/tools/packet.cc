/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_coding/neteq/tools/packet.h"

#include <string.h>

#include <memory>

#include "modules/rtp_rtcp/source/rtp_utility.h"
#include "rtc_base/checks.h"

namespace webrtc {
namespace test {

using webrtc::RtpUtility::RtpHeaderParser;

Packet::Packet(uint8_t* packet_memory,
               size_t allocated_bytes,
               size_t virtual_packet_length_bytes,
               double time_ms,
               const RtpUtility::RtpHeaderParser& parser,
               const RtpHeaderExtensionMap* extension_map /*= nullptr*/)
    : payload_memory_(packet_memory),
      packet_length_bytes_(allocated_bytes),
      virtual_packet_length_bytes_(virtual_packet_length_bytes),
      virtual_payload_length_bytes_(0),
      time_ms_(time_ms),
      valid_header_(ParseHeader(parser, extension_map)) {}

Packet::Packet(const RTPHeader& header,
               size_t virtual_packet_length_bytes,
               size_t virtual_payload_length_bytes,
               double time_ms)
    : header_(header),
      virtual_packet_length_bytes_(virtual_packet_length_bytes),
      virtual_payload_length_bytes_(virtual_payload_length_bytes),
      time_ms_(time_ms),
      valid_header_(true) {}

Packet::Packet(uint8_t* packet_memory, size_t allocated_bytes, double time_ms)
    : Packet(packet_memory,
             allocated_bytes,
             allocated_bytes,
             time_ms,
             RtpUtility::RtpHeaderParser(packet_memory, allocated_bytes)) {}

Packet::Packet(uint8_t* packet_memory,
               size_t allocated_bytes,
               size_t virtual_packet_length_bytes,
               double time_ms)
    : Packet(packet_memory,
             allocated_bytes,
             virtual_packet_length_bytes,
             time_ms,
             RtpUtility::RtpHeaderParser(packet_memory, allocated_bytes)) {}

Packet::~Packet() = default;

bool Packet::ExtractRedHeaders(std::list<RTPHeader*>* headers) const {
  //
  //  0                   1                    2                   3
  //  0 1 2 3 4 5 6 7 8 9 0 1 2 3  4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
  // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  // |1|   block PT  |  timestamp offset         |   block length    |
  // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  // |1|    ...                                                      |
  // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  // |0|   block PT  |
  // +-+-+-+-+-+-+-+-+
  //

  RTC_DCHECK(payload_);
  const uint8_t* payload_ptr = payload_;
  const uint8_t* payload_end_ptr = payload_ptr + payload_length_bytes_;

  // Find all RED headers with the extension bit set to 1. That is, all headers
  // but the last one.
  while ((payload_ptr < payload_end_ptr) && (*payload_ptr & 0x80)) {
    RTPHeader* header = new RTPHeader;
    CopyToHeader(header);
    header->payloadType = payload_ptr[0] & 0x7F;
    uint32_t offset = (payload_ptr[1] << 6) + ((payload_ptr[2] & 0xFC) >> 2);
    header->timestamp -= offset;
    headers->push_front(header);
    payload_ptr += 4;
  }
  // Last header.
  RTC_DCHECK_LT(payload_ptr, payload_end_ptr);
  if (payload_ptr >= payload_end_ptr) {
    return false;  // Payload too short.
  }
  RTPHeader* header = new RTPHeader;
  CopyToHeader(header);
  header->payloadType = payload_ptr[0] & 0x7F;
  headers->push_front(header);
  return true;
}

void Packet::DeleteRedHeaders(std::list<RTPHeader*>* headers) {
  while (!headers->empty()) {
    delete headers->front();
    headers->pop_front();
  }
}

bool Packet::ParseHeader(const RtpHeaderParser& parser,
                         const RtpHeaderExtensionMap* extension_map) {
  bool valid_header = parser.Parse(&header_, extension_map);

  // Special case for dummy packets that have padding marked in the RTP header.
  // This causes the RTP header parser to report failure, but is fine in this
  // context.
  const bool header_only_with_padding =
      (header_.headerLength == packet_length_bytes_ &&
       header_.paddingLength > 0);
  if (!valid_header && !header_only_with_padding) {
    return false;
  }
  RTC_DCHECK_LE(header_.headerLength, packet_length_bytes_);
  payload_ = &payload_memory_[header_.headerLength];
  RTC_DCHECK_GE(packet_length_bytes_, header_.headerLength);
  payload_length_bytes_ = packet_length_bytes_ - header_.headerLength;
  RTC_CHECK_GE(virtual_packet_length_bytes_, packet_length_bytes_);
  RTC_DCHECK_GE(virtual_packet_length_bytes_, header_.headerLength);
  virtual_payload_length_bytes_ =
      virtual_packet_length_bytes_ - header_.headerLength;
  return true;
}

void Packet::CopyToHeader(RTPHeader* destination) const {
  *destination = header_;
}

}  // namespace test
}  // namespace webrtc
