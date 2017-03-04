/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_RPSI_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_RPSI_H_

#include "webrtc/base/basictypes.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/psfb.h"

namespace webrtc {
namespace rtcp {
class CommonHeader;

// Reference picture selection indication (RPSI) (RFC 4585).
// Assumes native bit string stores PictureId (VP8, VP9).
class Rpsi : public Psfb {
 public:
  static constexpr uint8_t kFeedbackMessageType = 3;
  Rpsi();
  ~Rpsi() override {}

  // Parse assumes header is already parsed and validated.
  bool Parse(const CommonHeader& packet);

  void SetPayloadType(uint8_t payload);
  void SetPictureId(uint64_t picture_id);

  uint8_t payload_type() const { return payload_type_; }
  uint64_t picture_id() const { return picture_id_; }

 protected:
  bool Create(uint8_t* packet,
              size_t* index,
              size_t max_length,
              RtcpPacket::PacketReadyCallback* callback) const override;

 private:
  size_t BlockLength() const override { return block_length_; }
  static size_t CalculateBlockLength(uint8_t bitstring_size_bytes);

  uint8_t payload_type_;
  uint64_t picture_id_;
  size_t block_length_;
};
}  // namespace rtcp
}  // namespace webrtc
#endif  // WEBRTC_MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_RPSI_H_
