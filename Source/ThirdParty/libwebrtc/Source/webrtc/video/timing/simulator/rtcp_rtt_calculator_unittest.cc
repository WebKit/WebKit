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
#include <vector>

#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "modules/rtp_rtcp/source/ntp_time_util.h"
#include "modules/rtp_rtcp/source/rtcp_packet/dlrr.h"
#include "modules/rtp_rtcp/source/rtcp_packet/extended_reports.h"
#include "modules/rtp_rtcp/source/rtcp_packet/receiver_report.h"
#include "modules/rtp_rtcp/source/rtcp_packet/report_block.h"
#include "modules/rtp_rtcp/source/rtcp_packet/rrtr.h"
#include "modules/rtp_rtcp/source/rtcp_packet/sender_report.h"
#include "system_wrappers/include/clock.h"
#include "system_wrappers/include/ntp_time.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/near_matcher.h"

namespace webrtc::video_timing_simulator {
namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;

constexpr uint32_t kSenderSsrc = 123456;
constexpr uint32_t kReceiverSsrc = 987654;

rtcp::SenderReport CreateSr(NtpTime ntp) {
  rtcp::SenderReport sr;
  sr.SetSenderSsrc(kSenderSsrc);
  sr.SetNtp(ntp);
  return sr;
}

rtcp::SenderReport CreateSrWithReportBlock(NtpTime ntp,
                                           uint32_t last_sr,
                                           uint32_t delay_since_last_sr) {
  rtcp::SenderReport sr = CreateSr(ntp);
  rtcp::ReportBlock block;
  block.SetMediaSsrc(kSenderSsrc);
  block.SetLastSr(last_sr);
  block.SetDelayLastSr(delay_since_last_sr);
  sr.AddReportBlock(block);
  return sr;
}

rtcp::ReceiverReport CreateRrWithReportBlock(uint32_t last_sr,
                                             uint32_t delay_since_last_sr) {
  rtcp::ReceiverReport rr;
  rr.SetSenderSsrc(kReceiverSsrc);
  rtcp::ReportBlock block;
  block.SetMediaSsrc(kSenderSsrc);
  block.SetLastSr(last_sr);
  block.SetDelayLastSr(delay_since_last_sr);
  rr.AddReportBlock(block);
  return rr;
}

rtcp::ExtendedReports CreateXrWithRrtr(NtpTime ntp) {
  rtcp::ExtendedReports xr;
  xr.SetSenderSsrc(kReceiverSsrc);
  rtcp::Rrtr rrtr;
  rrtr.SetNtp(ntp);
  xr.SetRrtr(rrtr);
  return xr;
}

rtcp::ExtendedReports CreateXrWithDlrr(uint32_t last_rr,
                                       uint32_t delay_since_last_rr) {
  rtcp::ExtendedReports xr;
  xr.SetSenderSsrc(kSenderSsrc);
  rtcp::ReceiveTimeInfo dlrr_block;
  dlrr_block.ssrc = kReceiverSsrc;
  dlrr_block.last_rr = last_rr;
  dlrr_block.delay_since_last_rr = delay_since_last_rr;
  xr.AddDlrrItem(dlrr_block);
  return xr;
}

TEST(RtcpRttCalculatorTest, SenderCalculatesRttFromIncomingSr) {
  RtcpRttCalculator calculator;
  SimulatedClock clock(Timestamp::Millis(10000));

  // Outgoing SR.
  NtpTime ntp = clock.ConvertTimestampToNtpTime(clock.CurrentTime());
  rtcp::SenderReport sr = CreateSr(ntp);
  calculator.OnOutgoingSenderReport(sr, clock.CurrentTime());

  // Incoming SR: 50ms delay, arrives after 150ms => RTT is 100ms.
  clock.AdvanceTime(TimeDelta::Millis(150));
  uint32_t last_sr = CompactNtp(ntp);
  uint32_t delay_since_last_sr = SaturatedToCompactNtp(TimeDelta::Millis(50));
  NtpTime ntp_incoming = clock.ConvertTimestampToNtpTime(clock.CurrentTime());
  rtcp::SenderReport sr_incoming =
      CreateSrWithReportBlock(ntp_incoming, last_sr, delay_since_last_sr);
  std::vector<TimeDelta> rtts =
      calculator.OnIncomingSenderReport(sr_incoming, clock.CurrentTime());

  EXPECT_THAT(rtts, ElementsAre(Near(TimeDelta::Millis(100))));
}

TEST(RtcpRttCalculatorTest, SenderCalculatesRttFromIncomingRr) {
  RtcpRttCalculator calculator;
  SimulatedClock clock(Timestamp::Millis(10000));

  // Outgoing SR.
  NtpTime ntp = clock.ConvertTimestampToNtpTime(clock.CurrentTime());
  rtcp::SenderReport sr = CreateSr(ntp);
  calculator.OnOutgoingSenderReport(sr, clock.CurrentTime());

  // Incoming RR: 50ms delay, arrives after 150ms => RTT is 100ms.
  clock.AdvanceTime(TimeDelta::Millis(150));
  uint32_t last_sr = CompactNtp(ntp);
  uint32_t delay_since_last_sr = SaturatedToCompactNtp(TimeDelta::Millis(50));
  rtcp::ReceiverReport rr =
      CreateRrWithReportBlock(last_sr, delay_since_last_sr);
  std::vector<TimeDelta> rtts =
      calculator.OnIncomingReceiverReport(rr, clock.CurrentTime());

  EXPECT_THAT(rtts, ElementsAre(Near(TimeDelta::Millis(100))));
}

TEST(RtcpRttCalculatorTest, ReceiverCalculatesRttFromIncomingXr) {
  RtcpRttCalculator calculator;
  SimulatedClock clock(Timestamp::Millis(10000));

  // Outgoing XR with RRTR.
  NtpTime ntp = clock.ConvertTimestampToNtpTime(clock.CurrentTime());
  rtcp::ExtendedReports xr_rrtr = CreateXrWithRrtr(ntp);
  calculator.OnOutgoingExtendedReports(xr_rrtr, clock.CurrentTime());

  // Incoming XR with DLRR: 50ms delay, arrives after 150ms  => RTT is 100ms.
  clock.AdvanceTime(TimeDelta::Millis(150));
  uint32_t last_rr = CompactNtp(ntp);
  uint32_t delay_since_last_rr = SaturatedToCompactNtp(TimeDelta::Millis(50));
  rtcp::ExtendedReports xr_dlrr =
      CreateXrWithDlrr(last_rr, delay_since_last_rr);
  std::vector<TimeDelta> rtts =
      calculator.OnIncomingExtendedReports(xr_dlrr, clock.CurrentTime());

  EXPECT_THAT(rtts, ElementsAre(Near(TimeDelta::Millis(100))));
}

TEST(RtcpRttCalculatorTest, SenderCalculatesRttForMultipleRrsMatchingSingleSr) {
  RtcpRttCalculator calculator;
  SimulatedClock clock(Timestamp::Millis(10000));

  // Single outgoing SR.
  NtpTime ntp = clock.ConvertTimestampToNtpTime(clock.CurrentTime());
  rtcp::SenderReport sr = CreateSr(ntp);
  calculator.OnOutgoingSenderReport(sr, clock.CurrentTime());

  // First incoming RR: 50ms delay, arrives after 150ms => RTT is 100ms.
  clock.AdvanceTime(TimeDelta::Millis(150));
  uint32_t last_sr = CompactNtp(ntp);
  uint32_t delay_since_last_sr1 = SaturatedToCompactNtp(TimeDelta::Millis(50));
  rtcp::ReceiverReport rr1 =
      CreateRrWithReportBlock(last_sr, delay_since_last_sr1);
  std::vector<TimeDelta> rtts1 =
      calculator.OnIncomingReceiverReport(rr1, clock.CurrentTime());
  EXPECT_THAT(rtts1, ElementsAre(Near(TimeDelta::Millis(100))));

  // Second incoming RR: 150ms delay, arrives after another 100ms (total 250ms
  // since SR)
  // => RTT is 100ms.
  clock.AdvanceTime(TimeDelta::Millis(100));
  uint32_t delay_since_last_sr2 = SaturatedToCompactNtp(TimeDelta::Millis(150));
  rtcp::ReceiverReport rr2 =
      CreateRrWithReportBlock(last_sr, delay_since_last_sr2);
  std::vector<TimeDelta> rtts2 =
      calculator.OnIncomingReceiverReport(rr2, clock.CurrentTime());
  EXPECT_THAT(rtts2, ElementsAre(Near(TimeDelta::Millis(100))));
}

TEST(RtcpRttCalculatorTest, RejectsNegativeRtt) {
  RtcpRttCalculator calculator;
  SimulatedClock clock(Timestamp::Millis(10000));

  // Outgoing SR.
  NtpTime ntp = clock.ConvertTimestampToNtpTime(clock.CurrentTime());
  rtcp::SenderReport sr = CreateSr(ntp);
  calculator.OnOutgoingSenderReport(sr, clock.CurrentTime());

  // Incoming RR with delay 100ms (RTT would be 50ms - 100ms = -50ms).
  clock.AdvanceTime(TimeDelta::Millis(50));
  uint32_t last_sr = CompactNtp(ntp);
  uint32_t delay_since_last_sr = SaturatedToCompactNtp(TimeDelta::Millis(100));
  rtcp::ReceiverReport rr =
      CreateRrWithReportBlock(last_sr, delay_since_last_sr);
  std::vector<TimeDelta> rtts =
      calculator.OnIncomingReceiverReport(rr, clock.CurrentTime());

  EXPECT_THAT(rtts, IsEmpty());
}

TEST(RtcpRttCalculatorTest, IgnoresRrWithZeroLastSr) {
  RtcpRttCalculator calculator;
  SimulatedClock clock(Timestamp::Millis(10000));

  // Incoming RR with last_sr = 0 (means no SR received yet).
  rtcp::ReceiverReport rr = CreateRrWithReportBlock(/*last_sr=*/0, 0);
  std::vector<TimeDelta> rtts =
      calculator.OnIncomingReceiverReport(rr, clock.CurrentTime());

  EXPECT_THAT(rtts, IsEmpty());
}

TEST(RtcpRttCalculatorTest, CleansOldReportsAfterOneMinute) {
  RtcpRttCalculator calculator;
  SimulatedClock clock(Timestamp::Millis(10000));

  // Outgoing SR.
  NtpTime ntp = clock.ConvertTimestampToNtpTime(clock.CurrentTime());
  rtcp::SenderReport sr = CreateSr(ntp);
  calculator.OnOutgoingSenderReport(sr, clock.CurrentTime());

  // Advance time by 61 seconds (timeout is 60 seconds).
  clock.AdvanceTime(TimeDelta::Seconds(61));

  // Incoming RR: should be ignored because the SR was cleaned up.
  uint32_t last_sr = CompactNtp(ntp);
  rtcp::ReceiverReport rr = CreateRrWithReportBlock(last_sr, 0);
  std::vector<TimeDelta> rtts =
      calculator.OnIncomingReceiverReport(rr, clock.CurrentTime());

  EXPECT_THAT(rtts, IsEmpty());
}

}  // namespace
}  // namespace webrtc::video_timing_simulator
