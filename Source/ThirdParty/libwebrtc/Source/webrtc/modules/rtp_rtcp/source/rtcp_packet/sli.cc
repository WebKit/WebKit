/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/sli.h"

#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/modules/rtp_rtcp/source/byte_io.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/common_header.h"

namespace webrtc {
namespace rtcp {
constexpr uint8_t Sli::kFeedbackMessageType;
// RFC 4585: Feedback format.
//
// Common packet format:
//
//   0                   1                   2                   3
//   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |V=2|P|   FMT   |       PT      |          length               |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |                  SSRC of packet sender                        |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |                  SSRC of media source                         |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  :            Feedback Control Information (FCI)                 :
//  :                                                               :
//
// Slice loss indication (SLI) (RFC 4585).
// FCI:
//   0                   1                   2                   3
//   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |            First        |        Number           | PictureID |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
Sli::Macroblocks::Macroblocks(uint8_t picture_id,
                              uint16_t first,
                              uint16_t number) {
  RTC_DCHECK_LE(first, 0x1fff);
  RTC_DCHECK_LE(number, 0x1fff);
  RTC_DCHECK_LE(picture_id, 0x3f);
  item_ = (first << 19) | (number << 6) | picture_id;
}

void Sli::Macroblocks::Parse(const uint8_t* buffer) {
  item_ = ByteReader<uint32_t>::ReadBigEndian(buffer);
}

void Sli::Macroblocks::Create(uint8_t* buffer) const {
  ByteWriter<uint32_t>::WriteBigEndian(buffer, item_);
}

bool Sli::Parse(const CommonHeader& packet) {
  RTC_DCHECK_EQ(packet.type(), kPacketType);
  RTC_DCHECK_EQ(packet.fmt(), kFeedbackMessageType);

  if (packet.payload_size_bytes() <
      kCommonFeedbackLength + Macroblocks::kLength) {
    LOG(LS_WARNING) << "Packet is too small to be a valid SLI packet";
    return false;
  }

  size_t number_of_items =
      (packet.payload_size_bytes() - kCommonFeedbackLength) /
      Macroblocks::kLength;

  ParseCommonFeedback(packet.payload());
  items_.resize(number_of_items);

  const uint8_t* next_item = packet.payload() + kCommonFeedbackLength;
  for (Macroblocks& item : items_) {
    item.Parse(next_item);
    next_item += Macroblocks::kLength;
  }

  return true;
}

bool Sli::Create(uint8_t* packet,
                 size_t* index,
                 size_t max_length,
                 RtcpPacket::PacketReadyCallback* callback) const {
  RTC_DCHECK(!items_.empty());
  while (*index + BlockLength() > max_length) {
    if (!OnBufferFull(packet, index, callback))
      return false;
  }
  CreateHeader(kFeedbackMessageType, kPacketType, HeaderLength(), packet,
               index);
  CreateCommonFeedback(packet + *index);
  *index += kCommonFeedbackLength;
  for (const Macroblocks& item : items_) {
    item.Create(packet + *index);
    *index += Macroblocks::kLength;
  }
  return true;
}

}  // namespace rtcp
}  // namespace webrtc
