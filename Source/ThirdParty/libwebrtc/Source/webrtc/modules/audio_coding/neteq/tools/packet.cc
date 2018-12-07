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

#include "modules/rtp_rtcp/include/rtp_header_parser.h"
#include "rtc_base/checks.h"

namespace webrtc {
namespace test {

Packet::Packet(uint8_t* packet_memory,
               size_t allocated_bytes,
               double time_ms,
               const RtpHeaderParser& parser)
    : payload_memory_(packet_memory),
      payload_(NULL),
      packet_length_bytes_(allocated_bytes),
      payload_length_bytes_(0),
      virtual_packet_length_bytes_(allocated_bytes),
      virtual_payload_length_bytes_(0),
      time_ms_(time_ms) {
  valid_header_ = ParseHeader(parser);
}

Packet::Packet(uint8_t* packet_memory,
               size_t allocated_bytes,
               size_t virtual_packet_length_bytes,
               double time_ms,
               const RtpHeaderParser& parser)
    : payload_memory_(packet_memory),
      payload_(NULL),
      packet_length_bytes_(allocated_bytes),
      payload_length_bytes_(0),
      virtual_packet_length_bytes_(virtual_packet_length_bytes),
      virtual_payload_length_bytes_(0),
      time_ms_(time_ms) {
  valid_header_ = ParseHeader(parser);
}

Packet::Packet(const RTPHeader& header,
               size_t virtual_packet_length_bytes,
               size_t virtual_payload_length_bytes,
               double time_ms)
    : header_(header),
      payload_memory_(),
      payload_(NULL),
      packet_length_bytes_(0),
      payload_length_bytes_(0),
      virtual_packet_length_bytes_(virtual_packet_length_bytes),
      virtual_payload_length_bytes_(virtual_payload_length_bytes),
      time_ms_(time_ms),
      valid_header_(true) {}

Packet::Packet(uint8_t* packet_memory, size_t allocated_bytes, double time_ms)
    : payload_memory_(packet_memory),
      payload_(NULL),
      packet_length_bytes_(allocated_bytes),
      payload_length_bytes_(0),
      virtual_packet_length_bytes_(allocated_bytes),
      virtual_payload_length_bytes_(0),
      time_ms_(time_ms) {
  std::unique_ptr<RtpHeaderParser> parser(RtpHeaderParser::Create());
  valid_header_ = ParseHeader(*parser);
}

Packet::Packet(uint8_t* packet_memory,
               size_t allocated_bytes,
               size_t virtual_packet_length_bytes,
               double time_ms)
    : payload_memory_(packet_memory),
      payload_(NULL),
      packet_length_bytes_(allocated_bytes),
      payload_length_bytes_(0),
      virtual_packet_length_bytes_(virtual_packet_length_bytes),
      virtual_payload_length_bytes_(0),
      time_ms_(time_ms) {
  std::unique_ptr<RtpHeaderParser> parser(RtpHeaderParser::Create());
  valid_header_ = ParseHeader(*parser);
}

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

  assert(payload_);
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
  assert(payload_ptr < payload_end_ptr);
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

bool Packet::ParseHeader(const RtpHeaderParser& parser) {
  bool valid_header = parser.Parse(
      payload_memory_.get(), static_cast<int>(packet_length_bytes_), &header_);
  // Special case for dummy packets that have padding marked in the RTP header.
  // This causes the RTP header parser to report failure, but is fine in this
  // context.
  const bool header_only_with_padding =
      (header_.headerLength == packet_length_bytes_ &&
       header_.paddingLength > 0);
  if (!valid_header && !header_only_with_padding) {
    return false;
  }
  assert(header_.headerLength <= packet_length_bytes_);
  payload_ = &payload_memory_[header_.headerLength];
  assert(packet_length_bytes_ >= header_.headerLength);
  payload_length_bytes_ = packet_length_bytes_ - header_.headerLength;
  RTC_CHECK_GE(virtual_packet_length_bytes_, packet_length_bytes_);
  assert(virtual_packet_length_bytes_ >= header_.headerLength);
  virtual_payload_length_bytes_ =
      virtual_packet_length_bytes_ - header_.headerLength;
  return true;
}

void Packet::CopyToHeader(RTPHeader* destination) const {
  destination->markerBit = header_.markerBit;
  destination->payloadType = header_.payloadType;
  destination->sequenceNumber = header_.sequenceNumber;
  destination->timestamp = header_.timestamp;
  destination->ssrc = header_.ssrc;
  destination->numCSRCs = header_.numCSRCs;
  destination->paddingLength = header_.paddingLength;
  destination->headerLength = header_.headerLength;
  destination->payload_type_frequency = header_.payload_type_frequency;
  memcpy(&destination->arrOfCSRCs, &header_.arrOfCSRCs,
         sizeof(header_.arrOfCSRCs));
  memcpy(&destination->extension, &header_.extension,
         sizeof(header_.extension));
}

}  // namespace test
}  // namespace webrtc
