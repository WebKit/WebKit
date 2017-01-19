/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/rpsi.h"

#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/modules/rtp_rtcp/source/byte_io.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/common_header.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_utility.h"

using webrtc::RtpUtility::Word32Align;

namespace webrtc {
namespace rtcp {
constexpr uint8_t Rpsi::kFeedbackMessageType;
// RFC 4585: Feedback format.
// Reference picture selection indication (RPSI) (RFC 4585).
//
//     0                   1                   2                   3
//     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//    |V=2|P| RPSI=3  |  PT=PSFB=206  |          length               |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  0 |                  SSRC of packet sender                        |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  4 |                  SSRC of media source                         |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  8 | Padding Bits  |
//  9                 |0| Payload Type|
// 10                                 |  Native RPSI bit string       :
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//    :   defined per codec          ...                | Padding (0) |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
namespace {
constexpr size_t kPaddingSizeOffset = 8;
constexpr size_t kPayloadTypeOffset = 9;
constexpr size_t kBitStringOffset = 10;

constexpr size_t kPidBits = 7;
// Calculates number of bytes required to store given picture id.
uint8_t RequiredBytes(uint64_t picture_id) {
  uint8_t required_bytes = 0;
  uint64_t shifted_pid = picture_id;
  do {
    ++required_bytes;
    shifted_pid >>= kPidBits;
  } while (shifted_pid > 0);

  return required_bytes;
}
}  // namespace

Rpsi::Rpsi()
    : payload_type_(0),
      picture_id_(0),
      block_length_(CalculateBlockLength(1)) {}

bool Rpsi::Parse(const CommonHeader& packet) {
  RTC_DCHECK_EQ(packet.type(), kPacketType);
  RTC_DCHECK_EQ(packet.fmt(), kFeedbackMessageType);

  if (packet.payload_size_bytes() < kCommonFeedbackLength + 4) {
    LOG(LS_WARNING) << "Packet is too small to be a valid RPSI packet.";
    return false;
  }

  ParseCommonFeedback(packet.payload());

  uint8_t padding_bits = packet.payload()[kPaddingSizeOffset];
  if (padding_bits % 8 != 0) {
    LOG(LS_WARNING) << "Unknown rpsi packet with fractional number of bytes.";
    return false;
  }
  size_t padding_bytes = padding_bits / 8;
  if (padding_bytes + kBitStringOffset >= packet.payload_size_bytes()) {
    LOG(LS_WARNING) << "Too many padding bytes in a RPSI packet.";
    return false;
  }
  size_t padding_offset = packet.payload_size_bytes() - padding_bytes;
  payload_type_ = packet.payload()[kPayloadTypeOffset] & 0x7f;
  picture_id_ = 0;
  for (size_t pos = kBitStringOffset; pos < padding_offset; ++pos) {
    picture_id_ <<= kPidBits;
    picture_id_ |= (packet.payload()[pos] & 0x7f);
  }
  // Required bytes might become less than came in the packet.
  block_length_ = CalculateBlockLength(RequiredBytes(picture_id_));
  return true;
}

bool Rpsi::Create(uint8_t* packet,
                  size_t* index,
                  size_t max_length,
                  RtcpPacket::PacketReadyCallback* callback) const {
  while (*index + BlockLength() > max_length) {
    if (!OnBufferFull(packet, index, callback))
      return false;
  }
  size_t index_end = *index + BlockLength();

  CreateHeader(kFeedbackMessageType, kPacketType, HeaderLength(), packet,
               index);
  CreateCommonFeedback(packet + *index);
  *index += kCommonFeedbackLength;

  size_t bitstring_size_bytes = RequiredBytes(picture_id_);
  size_t padding_bytes =
      Word32Align(2 + bitstring_size_bytes) - (2 + bitstring_size_bytes);
  packet[(*index)++] = padding_bytes * 8;
  packet[(*index)++] = payload_type_;

  // Convert picture id to native bit string (defined by the video codec).
  for (size_t i = bitstring_size_bytes - 1; i > 0; --i) {
    packet[(*index)++] =
        0x80 | static_cast<uint8_t>(picture_id_ >> (i * kPidBits));
  }
  packet[(*index)++] = static_cast<uint8_t>(picture_id_ & 0x7f);
  constexpr uint8_t kPadding = 0;
  for (size_t i = 0; i < padding_bytes; ++i)
    packet[(*index)++] = kPadding;
  RTC_CHECK_EQ(*index, index_end);
  return true;
}

void Rpsi::SetPayloadType(uint8_t payload) {
  RTC_DCHECK_LE(payload, 0x7f);
  payload_type_ = payload;
}

void Rpsi::SetPictureId(uint64_t picture_id) {
  picture_id_ = picture_id;
  block_length_ = CalculateBlockLength(RequiredBytes(picture_id_));
}

size_t Rpsi::CalculateBlockLength(uint8_t bitstring_size_bytes) {
  return RtcpPacket::kHeaderLength + Psfb::kCommonFeedbackLength +
         Word32Align(2 + bitstring_size_bytes);
}
}  // namespace rtcp
}  // namespace webrtc
