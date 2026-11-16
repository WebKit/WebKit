/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_TIMING_SIMULATOR_RTCP_RTT_CALCULATOR_H_
#define VIDEO_TIMING_SIMULATOR_RTCP_RTT_CALCULATOR_H_

#include <cstdint>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "api/sequence_checker.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "modules/rtp_rtcp/source/rtcp_packet/extended_reports.h"
#include "modules/rtp_rtcp/source/rtcp_packet/receiver_report.h"
#include "modules/rtp_rtcp/source/rtcp_packet/report_block.h"
#include "modules/rtp_rtcp/source/rtcp_packet/sender_report.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc::video_timing_simulator {

// Calculates Round Trip Time (RTT) samples by pairing outgoing reports with
// incoming feedback. It covers all standard RTCP RTT mechanisms:
//
// 1. Sender RTT (SR/RR):
//    Maps outgoing Sender Reports (SR) to incoming Receiver Reports (RR) or
//    incoming Sender Reports (SR) containing reception report blocks.
//
// 2. Receiver RTT (XR RRTR/DLRR):
//    Maps outgoing Extended Reports (XR) containing Receiver Reference Time
//    Reports (RRTR) to incoming XRs containing Delay since Last RR (DLRR).
//
// Outgoing reports are cached for up to 1 minute before being cleaned up.
class RtcpRttCalculator {
 public:
  RtcpRttCalculator();
  ~RtcpRttCalculator();

  RtcpRttCalculator(const RtcpRttCalculator&) = delete;
  RtcpRttCalculator& operator=(const RtcpRttCalculator&) = delete;

  // Registers an outgoing SR to map incoming RRs to.
  void OnOutgoingSenderReport(const rtcp::SenderReport& sr, Timestamp now);

  // Registers an outgoing XR (specifically RRTR) to map incoming XRs (DLRR) to.
  void OnOutgoingExtendedReports(const rtcp::ExtendedReports& xr,
                                 Timestamp now);

  // Processes an incoming SR and returns a list of calculated RTTs.
  std::vector<TimeDelta> OnIncomingSenderReport(const rtcp::SenderReport& sr,
                                                Timestamp now);

  // Processes an incoming RR and returns a list of calculated RTTs.
  std::vector<TimeDelta> OnIncomingReceiverReport(
      const rtcp::ReceiverReport& rr,
      Timestamp now);

  // Processes an incoming XR and returns a list of calculated RTTs.
  std::vector<TimeDelta> OnIncomingExtendedReports(
      const rtcp::ExtendedReports& xr,
      Timestamp now);

 private:
  // See https://www.rfc-editor.org/info/rfc3550/#section-6.4.1.
  using SentReportKey =
      std::pair</*sender_ssrc=*/uint32_t, /*compact_ntp=*/uint32_t>;
  struct SentReportValue {
    Timestamp sent_time = Timestamp::MinusInfinity();
  };

  // Helper to process report blocks from either SR or RR.
  std::vector<TimeDelta> ProcessReportBlocks(
      const std::vector<rtcp::ReportBlock>& report_blocks,
      Timestamp now);

  // Cleans up registered outgoing reports that are older than 1 minute.
  void CleanOldReports(Timestamp now);

  SequenceChecker sequence_checker_;

  // Outgoing SRs: (sender_ssrc, lsr) -> sent_time.
  absl::flat_hash_map<SentReportKey, SentReportValue> outgoing_srs_
      RTC_GUARDED_BY(sequence_checker_);

  // Outgoing XRs: (sender_ssrc, lrr) -> sent_time.
  absl::flat_hash_map<SentReportKey, SentReportValue> outgoing_xrs_
      RTC_GUARDED_BY(sequence_checker_);
};

}  // namespace webrtc::video_timing_simulator

#endif  // VIDEO_TIMING_SIMULATOR_RTCP_RTT_CALCULATOR_H_
