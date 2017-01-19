/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_EXTENDED_REPORTS_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_EXTENDED_REPORTS_H_

#include <vector>

#include "webrtc/base/constructormagic.h"
#include "webrtc/base/optional.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/dlrr.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/rrtr.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/target_bitrate.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/voip_metric.h"

namespace webrtc {
namespace rtcp {
class CommonHeader;

// From RFC 3611: RTP Control Protocol Extended Reports (RTCP XR).
class ExtendedReports : public RtcpPacket {
 public:
  static constexpr uint8_t kPacketType = 207;

  ExtendedReports();
  ~ExtendedReports() override;

  // Parse assumes header is already parsed and validated.
  bool Parse(const CommonHeader& packet);

  void SetSenderSsrc(uint32_t ssrc) { sender_ssrc_ = ssrc; }

  void SetRrtr(const Rrtr& rrtr);
  void AddDlrrItem(const ReceiveTimeInfo& time_info);
  void SetVoipMetric(const VoipMetric& voip_metric);
  void SetTargetBitrate(const TargetBitrate& target_bitrate);

  uint32_t sender_ssrc() const { return sender_ssrc_; }
  const rtc::Optional<Rrtr>& rrtr() const { return rrtr_block_; }
  const Dlrr& dlrr() const { return dlrr_block_; }
  const rtc::Optional<VoipMetric>& voip_metric() const {
    return voip_metric_block_;
  }
  const rtc::Optional<TargetBitrate>& target_bitrate() {
    return target_bitrate_;
  }

 protected:
  bool Create(uint8_t* packet,
              size_t* index,
              size_t max_length,
              RtcpPacket::PacketReadyCallback* callback) const override;

 private:
  static constexpr size_t kXrBaseLength = 4;

  size_t BlockLength() const override {
    return kHeaderLength + kXrBaseLength + RrtrLength() + DlrrLength() +
           VoipMetricLength() + TargetBitrateLength();
  }

  size_t RrtrLength() const { return rrtr_block_ ? Rrtr::kLength : 0; }
  size_t DlrrLength() const { return dlrr_block_.BlockLength(); }
  size_t VoipMetricLength() const {
    return voip_metric_block_ ? VoipMetric::kLength : 0;
  }
  size_t TargetBitrateLength() const;

  void ParseRrtrBlock(const uint8_t* block, uint16_t block_length);
  void ParseDlrrBlock(const uint8_t* block, uint16_t block_length);
  void ParseVoipMetricBlock(const uint8_t* block, uint16_t block_length);
  void ParseTargetBitrateBlock(const uint8_t* block, uint16_t block_length);

  uint32_t sender_ssrc_;
  rtc::Optional<Rrtr> rrtr_block_;
  Dlrr dlrr_block_;  // Dlrr without items treated same as no dlrr block.
  rtc::Optional<VoipMetric> voip_metric_block_;
  rtc::Optional<TargetBitrate> target_bitrate_;

  RTC_DISALLOW_COPY_AND_ASSIGN(ExtendedReports);
};
}  // namespace rtcp
}  // namespace webrtc
#endif  // WEBRTC_MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_EXTENDED_REPORTS_H_
