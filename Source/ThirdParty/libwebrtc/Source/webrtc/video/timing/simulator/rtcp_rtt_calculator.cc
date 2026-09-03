/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/timing/simulator/rtcp_rtt_calculator.h"

#include <cstdint>
#include <optional>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "api/sequence_checker.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "modules/rtp_rtcp/source/ntp_time_util.h"
#include "modules/rtp_rtcp/source/rtcp_packet/extended_reports.h"
#include "modules/rtp_rtcp/source/rtcp_packet/receiver_report.h"
#include "modules/rtp_rtcp/source/rtcp_packet/report_block.h"
#include "modules/rtp_rtcp/source/rtcp_packet/sender_report.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc::video_timing_simulator {

RtcpRttCalculator::RtcpRttCalculator() = default;

RtcpRttCalculator::~RtcpRttCalculator() {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
}

void RtcpRttCalculator::OnOutgoingSenderReport(const rtcp::SenderReport& sr,
                                               Timestamp now) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  RTC_DCHECK(now.IsFinite());
  CleanOldReports(now);
  // https://www.rfc-editor.org/info/rfc3550/#section-6.4.1
  uint32_t compact_ntp = CompactNtp(sr.ntp());
  if (compact_ntp == 0) {
    return;
  }
  outgoing_srs_[{sr.sender_ssrc(), compact_ntp}] =
      SentReportValue{.sent_time = now};
}

void RtcpRttCalculator::OnOutgoingExtendedReports(
    const rtcp::ExtendedReports& xr,
    Timestamp now) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  RTC_DCHECK(now.IsFinite());
  CleanOldReports(now);
  if (!xr.rrtr().has_value()) {
    return;
  }
  // https://www.rfc-editor.org/info/rfc3611/#section-4.4
  uint32_t compact_ntp = CompactNtp(xr.rrtr()->ntp());
  if (compact_ntp == 0) {
    return;
  }
  outgoing_xrs_[{xr.sender_ssrc(), compact_ntp}] =
      SentReportValue{.sent_time = now};
}

std::vector<TimeDelta> RtcpRttCalculator::OnIncomingSenderReport(
    const rtcp::SenderReport& sr,
    Timestamp now) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  RTC_DCHECK(now.IsFinite());
  CleanOldReports(now);
  return ProcessReportBlocks(sr.report_blocks(), now);
}

std::vector<TimeDelta> RtcpRttCalculator::OnIncomingReceiverReport(
    const rtcp::ReceiverReport& rr,
    Timestamp now) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  RTC_DCHECK(now.IsFinite());
  CleanOldReports(now);
  return ProcessReportBlocks(rr.report_blocks(), now);
}

std::vector<TimeDelta> RtcpRttCalculator::OnIncomingExtendedReports(
    const rtcp::ExtendedReports& xr,
    Timestamp now) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  RTC_DCHECK(now.IsFinite());
  CleanOldReports(now);
  std::vector<TimeDelta> rtt_samples;
  rtt_samples.reserve(xr.dlrr().sub_blocks().size());
  for (const auto& block : xr.dlrr().sub_blocks()) {
    uint32_t sender_ssrc = block.ssrc;
    uint32_t last_rr = block.last_rr;
    // (Quotes from https://www.rfc-editor.org/info/rfc3611/#section-4.5)
    // "If no such block has been received, the field is set to zero."
    if (last_rr == 0) {
      continue;
    }
    if (auto it = outgoing_xrs_.find({sender_ssrc, last_rr});
        it != outgoing_xrs_.end()) {
      TimeDelta delay_since_last_rr =
          CompactNtpIntervalToTimeDelta(block.delay_since_last_rr);
      // "It calculates the total round-trip time A-LRR using the"
      // "last RR timestamp (LRR) field, and then subtracting this field to"
      // "leave the round-trip propagation delay as A-LRR-DLRR."
      TimeDelta rtt = now - it->second.sent_time - delay_since_last_rr;
      if (rtt <= TimeDelta::Zero()) {
        RTC_LOG(LS_INFO) << "Ignoring non-positive RTT: " << rtt.ms()
                         << "ms (now: " << now.ms()
                         << ", sent: " << it->second.sent_time.ms()
                         << ", delay_since_last_rr: "
                         << delay_since_last_rr.ms() << ")";
        continue;
      }
      rtt_samples.push_back(rtt);
    }
  }
  return rtt_samples;
}

std::vector<TimeDelta> RtcpRttCalculator::ProcessReportBlocks(
    const std::vector<rtcp::ReportBlock>& report_blocks,
    Timestamp now) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  RTC_DCHECK(now.IsFinite());
  std::vector<TimeDelta> rtt_samples;
  rtt_samples.reserve(report_blocks.size());
  for (const auto& block : report_blocks) {
    uint32_t sender_ssrc = block.source_ssrc();
    uint32_t last_sr = block.last_sr();
    // (Quotes from https://www.rfc-editor.org/info/rfc3550/#section-6.4.1)
    if (last_sr == 0) {
      // "If no SR has been received yet, the field is set to zero."
      continue;
    }
    if (auto it = outgoing_srs_.find({sender_ssrc, last_sr});
        it != outgoing_srs_.end()) {
      TimeDelta delay_since_last_sr =
          CompactNtpIntervalToTimeDelta(block.delay_since_last_sr());
      // "It calculates the total round-trip time A-LSR using the"
      // "last SR timestamp (LSR) field, and then subtracting this field to"
      // "leave the round-trip propagation delay as (A - LSR - DLSR)."
      TimeDelta rtt = now - it->second.sent_time - delay_since_last_sr;
      if (rtt <= TimeDelta::Zero()) {
        RTC_LOG(LS_INFO) << "Ignoring non-positive RTT: " << rtt.ms()
                         << "ms (now: " << now.ms()
                         << ", sent: " << it->second.sent_time.ms()
                         << ", delay_since_last_sr: "
                         << delay_since_last_sr.ms() << ")";
        continue;
      }
      rtt_samples.push_back(rtt);
    }
  }
  return rtt_samples;
}

void RtcpRttCalculator::CleanOldReports(Timestamp now) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  RTC_DCHECK(now.IsFinite());
  constexpr TimeDelta kCleanupTimeout = TimeDelta::Minutes(1);
  absl::erase_if(outgoing_srs_, [&](const auto& kv) {
    return now - kv.second.sent_time > kCleanupTimeout;
  });
  absl::erase_if(outgoing_xrs_, [&](const auto& kv) {
    return now - kv.second.sent_time > kCleanupTimeout;
  });
}

}  // namespace webrtc::video_timing_simulator
