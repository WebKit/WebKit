/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_REMB_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_REMB_H_

#include <vector>

#include "webrtc/base/basictypes.h"
#include "webrtc/base/constructormagic.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/psfb.h"

namespace webrtc {
namespace rtcp {
class CommonHeader;

// Receiver Estimated Max Bitrate (REMB) (draft-alvestrand-rmcat-remb).
class Remb : public Psfb {
 public:
  static constexpr uint8_t kFeedbackMessageType = 15;
  static constexpr size_t kMaxNumberOfSsrcs = 0xff;

  Remb() : bitrate_bps_(0) {}
  ~Remb() override {}

  // Parse assumes header is already parsed and validated.
  bool Parse(const CommonHeader& packet);

  bool SetSsrcs(std::vector<uint32_t> ssrcs);
  void SetBitrateBps(uint64_t bitrate_bps) { bitrate_bps_ = bitrate_bps; }

  uint64_t bitrate_bps() const { return bitrate_bps_; }
  const std::vector<uint32_t>& ssrcs() const { return ssrcs_; }

 protected:
  bool Create(uint8_t* packet,
              size_t* index,
              size_t max_length,
              RtcpPacket::PacketReadyCallback* callback) const override;

  size_t BlockLength() const override {
    return kHeaderLength + kCommonFeedbackLength + (2 + ssrcs_.size()) * 4;
  }

 private:
  static constexpr uint32_t kUniqueIdentifier = 0x52454D42;  // 'R' 'E' 'M' 'B'.

  // Media ssrc is unused, shadow base class setter and getter.
  void SetMediaSsrc(uint32_t);
  uint32_t media_ssrc() const;

  uint64_t bitrate_bps_;
  std::vector<uint32_t> ssrcs_;

  RTC_DISALLOW_COPY_AND_ASSIGN(Remb);
};
}  // namespace rtcp
}  // namespace webrtc
#endif  // WEBRTC_MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_REMB_H_
