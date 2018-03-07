/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_RTP_RTCP_SOURCE_RTCP_TRANSCEIVER_IMPL_H_
#define MODULES_RTP_RTCP_SOURCE_RTCP_TRANSCEIVER_IMPL_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "api/array_view.h"
#include "api/optional.h"
#include "modules/rtp_rtcp/source/rtcp_packet/common_header.h"
#include "modules/rtp_rtcp/source/rtcp_packet/remb.h"
#include "modules/rtp_rtcp/source/rtcp_packet/report_block.h"
#include "modules/rtp_rtcp/source/rtcp_transceiver_config.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/function_view.h"
#include "rtc_base/weak_ptr.h"
#include "system_wrappers/include/ntp_time.h"

namespace webrtc {
//
// Manage incoming and outgoing rtcp messages for multiple BUNDLED streams.
//
// This class is not thread-safe.
class RtcpTransceiverImpl {
 public:
  explicit RtcpTransceiverImpl(const RtcpTransceiverConfig& config);
  ~RtcpTransceiverImpl();

  void ReceivePacket(rtc::ArrayView<const uint8_t> packet, int64_t now_us);

  void SendCompoundPacket();

  void SetRemb(int bitrate_bps, std::vector<uint32_t> ssrcs);
  void UnsetRemb();

  void SendNack(uint32_t ssrc, std::vector<uint16_t> sequence_numbers);

  void SendPictureLossIndication(rtc::ArrayView<const uint32_t> ssrcs);
  void SendFullIntraRequest(rtc::ArrayView<const uint32_t> ssrcs);

 private:
  class PacketSender;
  struct RemoteSenderState;

  void HandleReceivedPacket(const rtcp::CommonHeader& rtcp_packet_header,
                            int64_t now_us);

  void ReschedulePeriodicCompoundPackets();
  void SchedulePeriodicCompoundPackets(int64_t delay_ms);
  // Creates compound RTCP packet, as defined in
  // https://tools.ietf.org/html/rfc5506#section-2
  void CreateCompoundPacket(PacketSender* sender);
  // Sends RTCP packets.
  void SendPeriodicCompoundPacket();
  void SendImmediateFeedback(
      rtc::FunctionView<void(PacketSender*)> append_feedback);
  // Generate Report Blocks to be send in Sender or Receiver Report.
  std::vector<rtcp::ReportBlock> CreateReportBlocks();

  const RtcpTransceiverConfig config_;

  rtc::Optional<rtcp::Remb> remb_;
  std::map<uint32_t, RemoteSenderState> remote_senders_;
  rtc::WeakPtrFactory<RtcpTransceiverImpl> ptr_factory_;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(RtcpTransceiverImpl);
};

}  // namespace webrtc

#endif  // MODULES_RTP_RTCP_SOURCE_RTCP_TRANSCEIVER_IMPL_H_
