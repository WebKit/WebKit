/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_RECEIVER_REPORT_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_RECEIVER_REPORT_H_

#include <vector>

#include "webrtc/base/basictypes.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/report_block.h"

namespace webrtc {
namespace rtcp {
class CommonHeader;

class ReceiverReport : public RtcpPacket {
 public:
  static constexpr uint8_t kPacketType = 201;
  ReceiverReport();
  ~ReceiverReport() override;

  // Parse assumes header is already parsed and validated.
  bool Parse(const CommonHeader& packet);

  void SetSenderSsrc(uint32_t ssrc) { sender_ssrc_ = ssrc; }
  bool AddReportBlock(const ReportBlock& block);

  uint32_t sender_ssrc() const { return sender_ssrc_; }
  const std::vector<ReportBlock>& report_blocks() const {
    return report_blocks_;
  }

  size_t BlockLength() const override;

  bool Create(uint8_t* packet,
              size_t* index,
              size_t max_length,
              RtcpPacket::PacketReadyCallback* callback) const override;

 private:
  static const size_t kRrBaseLength = 4;
  static const size_t kMaxNumberOfReportBlocks = 0x1F;

  uint32_t sender_ssrc_;
  std::vector<ReportBlock> report_blocks_;
};

}  // namespace rtcp
}  // namespace webrtc
#endif  // WEBRTC_MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_RECEIVER_REPORT_H_
