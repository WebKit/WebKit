/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_EXTENDED_JITTER_REPORT_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_EXTENDED_JITTER_REPORT_H_

#include <vector>

#include "webrtc/base/constructormagic.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet.h"

namespace webrtc {
namespace rtcp {
class CommonHeader;

class ExtendedJitterReport : public RtcpPacket {
 public:
  static constexpr uint8_t kPacketType = 195;
  static constexpr size_t kMaxNumberOfJitterValues = 0x1f;

  ExtendedJitterReport() {}
  ~ExtendedJitterReport() override {}

  // Parse assumes header is already parsed and validated.
  bool Parse(const CommonHeader& packet);

  bool SetJitterValues(std::vector<uint32_t> jitter_values);

  const std::vector<uint32_t>& jitter_values() {
    return inter_arrival_jitters_;
  }

 protected:
  bool Create(uint8_t* packet,
              size_t* index,
              size_t max_length,
              RtcpPacket::PacketReadyCallback* callback) const override;

 private:
  static constexpr size_t kJitterSizeBytes = 4;

  size_t BlockLength() const override {
    return kHeaderLength + kJitterSizeBytes * inter_arrival_jitters_.size();
  }

  std::vector<uint32_t> inter_arrival_jitters_;

  RTC_DISALLOW_COPY_AND_ASSIGN(ExtendedJitterReport);
};

}  // namespace rtcp
}  // namespace webrtc
#endif  // WEBRTC_MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_EXTENDED_JITTER_REPORT_H_
