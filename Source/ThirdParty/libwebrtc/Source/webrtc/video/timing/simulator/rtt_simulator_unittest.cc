/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/timing/simulator/rtt_simulator.h"

#include <cstdint>
#include <memory>

#include "api/sequence_checker.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "logging/rtc_event_log/events/logged_rtp_rtcp.h"
#include "modules/rtp_rtcp/source/ntp_time_util.h"
#include "modules/rtp_rtcp/source/rtcp_packet/dlrr.h"
#include "modules/rtp_rtcp/source/rtcp_packet/extended_reports.h"
#include "modules/rtp_rtcp/source/rtcp_packet/receiver_report.h"
#include "modules/rtp_rtcp/source/rtcp_packet/report_block.h"
#include "modules/rtp_rtcp/source/rtcp_packet/rrtr.h"
#include "modules/rtp_rtcp/source/rtcp_packet/sender_report.h"
#include "system_wrappers/include/ntp_time.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "video/timing/simulator/test/simulated_time_test_fixture.h"

namespace webrtc::video_timing_simulator {
namespace {

constexpr uint32_t kSenderSsrc = 123456;
constexpr uint32_t kReceiverSsrc = 987654;

class MockSimulatedRttCallback : public SimulatedRttCallback {
 public:
  MOCK_METHOD(void, OnMaxRttUpdate, (TimeDelta max_rtt), (override));
};

class RttSimulatorTest : public SimulatedTimeTestFixture {
 protected:
  RttSimulatorTest() {
    SendTask([this]() {
      RTC_DCHECK_RUN_ON(queue_ptr_);
      rtt_simulator_ =
          std::make_unique<RttSimulator>(env_, queue_ptr_, &callback_);
    });
  }

  ~RttSimulatorTest() override {
    SendTask([this]() {
      RTC_DCHECK_RUN_ON(queue_ptr_);
      rtt_simulator_.reset();
    });
  }

  MockSimulatedRttCallback callback_;
  std::unique_ptr<RttSimulator> rtt_simulator_;
};

LoggedRtcpPacketSenderReport CreateLoggedRtcpPacketSenderReport(
    NtpTime ntp,
    Timestamp log_time) {
  rtcp::SenderReport sr;
  sr.SetSenderSsrc(kSenderSsrc);
  sr.SetNtp(ntp);
  return LoggedRtcpPacketSenderReport(log_time, sr);
}

LoggedRtcpPacketSenderReport CreateLoggedRtcpPacketSenderReportWithReportBlock(
    NtpTime ntp,
    uint32_t last_sr,
    uint32_t delay_since_last_sr,
    Timestamp log_time) {
  rtcp::SenderReport sr;
  sr.SetSenderSsrc(kReceiverSsrc);
  sr.SetNtp(ntp);
  rtcp::ReportBlock block;
  block.SetMediaSsrc(kSenderSsrc);
  block.SetLastSr(last_sr);
  block.SetDelayLastSr(delay_since_last_sr);
  sr.AddReportBlock(block);
  return LoggedRtcpPacketSenderReport(log_time, sr);
}

LoggedRtcpPacketReceiverReport CreateLoggedRtcpPacketReceiverReport(
    uint32_t last_sr,
    uint32_t delay_since_last_sr,
    Timestamp log_time) {
  rtcp::ReceiverReport rr;
  rr.SetSenderSsrc(kReceiverSsrc);
  rtcp::ReportBlock block;
  block.SetMediaSsrc(kSenderSsrc);
  block.SetLastSr(last_sr);
  block.SetDelayLastSr(delay_since_last_sr);
  rr.AddReportBlock(block);
  return LoggedRtcpPacketReceiverReport(log_time, rr);
}

LoggedRtcpPacketExtendedReports CreateLoggedRtcpPacketExtendedReportsWithRrtr(
    NtpTime ntp,
    Timestamp log_time) {
  rtcp::ExtendedReports xr;
  xr.SetSenderSsrc(kReceiverSsrc);
  rtcp::Rrtr rrtr;
  rrtr.SetNtp(ntp);
  xr.SetRrtr(rrtr);

  LoggedRtcpPacketExtendedReports logged_xr;
  logged_xr.timestamp = log_time;
  logged_xr.xr = xr;
  return logged_xr;
}

LoggedRtcpPacketExtendedReports CreateLoggedRtcpPacketExtendedReportsWithDlrr(
    uint32_t last_rr,
    uint32_t delay_since_last_rr,
    Timestamp log_time) {
  rtcp::ExtendedReports xr;
  xr.SetSenderSsrc(kSenderSsrc);
  rtcp::ReceiveTimeInfo dlrr_block;
  dlrr_block.ssrc = kReceiverSsrc;
  dlrr_block.last_rr = last_rr;
  dlrr_block.delay_since_last_rr = delay_since_last_rr;
  xr.AddDlrrItem(dlrr_block);

  LoggedRtcpPacketExtendedReports logged_xr;
  logged_xr.timestamp = log_time;
  logged_xr.xr = xr;
  return logged_xr;
}

TEST_F(RttSimulatorTest, SenderCalculatesRttFromIncomingRr) {
  // Outgoing SR.
  Timestamp sent_timestamp = time_controller_.GetClock()->CurrentTime();
  NtpTime ntp = env_.clock().ConvertTimestampToNtpTime(sent_timestamp);
  auto sr = CreateLoggedRtcpPacketSenderReport(ntp, sent_timestamp);
  SendTask([this, sr]() { rtt_simulator_->OnOutgoingSenderReport(sr); });

  // Incoming RR: 50ms delay, arrives after 150ms => RTT is 100ms.
  time_controller_.AdvanceTime(TimeDelta::Millis(150));
  Timestamp recv_timestamp = time_controller_.GetClock()->CurrentTime();
  uint32_t last_sr = CompactNtp(ntp);
  uint32_t delay_since_last_sr = SaturatedToCompactNtp(TimeDelta::Millis(50));
  auto rr = CreateLoggedRtcpPacketReceiverReport(last_sr, delay_since_last_sr,
                                                 recv_timestamp);
  EXPECT_CALL(callback_, OnMaxRttUpdate(TimeDelta::Millis(100)));
  SendTask([this, rr]() { rtt_simulator_->OnIncomingReceiverReport(rr); });
}

TEST_F(RttSimulatorTest, SenderCalculatesRttFromIncomingSr) {
  // Outgoing SR.
  Timestamp sent_timestamp = time_controller_.GetClock()->CurrentTime();
  NtpTime ntp = env_.clock().ConvertTimestampToNtpTime(sent_timestamp);
  auto sr = CreateLoggedRtcpPacketSenderReport(ntp, sent_timestamp);
  SendTask([this, sr]() { rtt_simulator_->OnOutgoingSenderReport(sr); });

  // Incoming SR: 50ms delay, arrives after 150ms => RTT is 100ms.
  time_controller_.AdvanceTime(TimeDelta::Millis(150));
  Timestamp recv_timestamp = time_controller_.GetClock()->CurrentTime();
  uint32_t last_sr = CompactNtp(ntp);
  uint32_t delay_since_last_sr = SaturatedToCompactNtp(TimeDelta::Millis(50));
  NtpTime ntp_incoming;
  auto sr_incoming = CreateLoggedRtcpPacketSenderReportWithReportBlock(
      ntp_incoming, last_sr, delay_since_last_sr, recv_timestamp);

  EXPECT_CALL(callback_, OnMaxRttUpdate(TimeDelta::Millis(100)));
  SendTask([this, sr_incoming]() {
    rtt_simulator_->OnIncomingSenderReport(sr_incoming);
  });
}

TEST_F(RttSimulatorTest, ReceiverCalculatesRttFromIncomingXr) {
  // Outgoing XR with RRTR.
  Timestamp sent_timestamp = time_controller_.GetClock()->CurrentTime();
  NtpTime ntp = env_.clock().ConvertTimestampToNtpTime(sent_timestamp);
  auto xr_rrtr =
      CreateLoggedRtcpPacketExtendedReportsWithRrtr(ntp, sent_timestamp);
  SendTask([this, xr_rrtr]() {
    rtt_simulator_->OnOutgoingExtendedReports(xr_rrtr);
  });

  // Incoming XR with DLRR: 50ms delay, arrives after 150ms => RTT is 100ms.
  time_controller_.AdvanceTime(TimeDelta::Millis(150));
  Timestamp recv_timestamp = time_controller_.GetClock()->CurrentTime();
  uint32_t last_rr = CompactNtp(ntp);
  uint32_t delay_since_last_rr = SaturatedToCompactNtp(TimeDelta::Millis(50));
  auto xr_dlrr = CreateLoggedRtcpPacketExtendedReportsWithDlrr(
      last_rr, delay_since_last_rr, recv_timestamp);

  EXPECT_CALL(callback_, OnMaxRttUpdate(TimeDelta::Millis(100)));
  SendTask([this, xr_dlrr]() {
    rtt_simulator_->OnIncomingExtendedReports(xr_dlrr);
  });
}

}  // namespace
}  // namespace webrtc::video_timing_simulator
