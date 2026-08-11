/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/include/remote_ntp_time_estimator.h"

#include <cstdint>
#include <optional>

#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "modules/rtp_rtcp/source/ntp_time_util.h"
#include "system_wrappers/include/clock.h"
#include "system_wrappers/include/ntp_time.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/near_matcher.h"

namespace webrtc {
namespace {

using ::testing::Optional;

constexpr TimeDelta kTestRtt = TimeDelta::Millis(10);
constexpr Timestamp kLocalClockInitialTime = Timestamp::Millis(123);
constexpr Timestamp kRemoteClockInitialTime = Timestamp::Millis(373);
constexpr uint32_t kTimestampOffset = 567;
constexpr int64_t kRemoteToLocalClockOffsetNtp =
    ToNtpUnits(kLocalClockInitialTime - kRemoteClockInitialTime);
// There can be small rounding differences when converting to the
// sub nano second precision of the NTP timestamps.
constexpr int64_t kEpsilon = 1;

class RemoteNtpTimeEstimatorTest : public ::testing::Test {
 protected:
  void AdvanceTime(TimeDelta delta) {
    local_clock_.AdvanceTime(delta);
    remote_clock_.AdvanceTime(delta);
  }

  uint32_t GetRemoteTimestamp() {
    return static_cast<uint32_t>(remote_clock_.TimeInMilliseconds()) * 90 +
           kTimestampOffset;
  }

  void SendRtcpSr() {
    uint32_t rtcp_timestamp = GetRemoteTimestamp();
    NtpTime ntp = remote_clock_.CurrentNtpTime();

    AdvanceTime(kTestRtt / 2);
    EXPECT_TRUE(estimator_.UpdateRtcpTimestamp(kTestRtt, ntp, rtcp_timestamp));
  }

  void SendRtcpSrInaccurately(TimeDelta ntp_error, TimeDelta networking_delay) {
    uint32_t rtcp_timestamp = GetRemoteTimestamp();
    int64_t ntp_error_fractions = ToNtpUnits(ntp_error);
    NtpTime ntp(static_cast<uint64_t>(remote_clock_.CurrentNtpTime()) +
                ntp_error_fractions);
    AdvanceTime(kTestRtt / 2 + networking_delay);
    EXPECT_TRUE(estimator_.UpdateRtcpTimestamp(kTestRtt, ntp, rtcp_timestamp));
  }

  void ReceiveRemoteSr(TimeDelta delivery_delay, TimeDelta rtt) {
    const uint32_t rtp_sr = GetRemoteTimestamp();
    const NtpTime ntp_sr = remote_clock_.CurrentNtpTime();

    AdvanceTime(delivery_delay);
    EXPECT_TRUE(estimator_.UpdateRtcpTimestamp(rtt, ntp_sr, rtp_sr));
  }

  SimulatedClock local_clock_{kLocalClockInitialTime};
  SimulatedClock remote_clock_{kRemoteClockInitialTime};
  RemoteNtpTimeEstimator estimator_{&local_clock_};
};

TEST_F(RemoteNtpTimeEstimatorTest, FailsWithoutValidNtpTime) {
  EXPECT_FALSE(
      estimator_.UpdateRtcpTimestamp(kTestRtt, NtpTime(), /*rtp_timestamp=*/0));
}

TEST_F(RemoteNtpTimeEstimatorTest, Estimate) {
  // Remote peer sends first RTCP SR.
  SendRtcpSr();

  // Remote sends a RTP packet.
  AdvanceTime(TimeDelta::Millis(15));
  uint32_t rtp_timestamp = GetRemoteTimestamp();
  int64_t capture_ntp_time_ms = local_clock_.CurrentNtpInMilliseconds();

  // Local peer needs at least 2 RTCP SR to calculate the capture time.
  const int64_t kNotEnoughRtcpSr = -1;
  EXPECT_EQ(kNotEnoughRtcpSr, estimator_.Estimate(rtp_timestamp));
  EXPECT_EQ(estimator_.EstimateRemoteToLocalClockOffset(), std::nullopt);

  AdvanceTime(TimeDelta::Millis(800));
  // Remote sends second RTCP SR.
  SendRtcpSr();

  AdvanceTime(TimeDelta::Millis(800));
  // Remote sends third RTCP SR.
  SendRtcpSr();

  // Local peer gets enough RTCP SR to calculate the capture time.
  EXPECT_EQ(capture_ntp_time_ms, estimator_.Estimate(rtp_timestamp));
  EXPECT_NEAR(*estimator_.EstimateRemoteToLocalClockOffset(),
              kRemoteToLocalClockOffsetNtp, kEpsilon);
}

TEST_F(RemoteNtpTimeEstimatorTest, AveragesErrorsOut) {
  // Remote peer sends first 10 RTCP SR without errors.
  for (int i = 0; i < 10; ++i) {
    AdvanceTime(TimeDelta::Seconds(1));
    SendRtcpSr();
  }

  AdvanceTime(TimeDelta::Millis(150));
  uint32_t rtp_timestamp = GetRemoteTimestamp();
  int64_t capture_ntp_time_ms = local_clock_.CurrentNtpInMilliseconds();
  // Local peer gets enough RTCP SR to calculate the capture time.
  EXPECT_EQ(capture_ntp_time_ms, estimator_.Estimate(rtp_timestamp));
  EXPECT_NEAR(kRemoteToLocalClockOffsetNtp,
              *estimator_.EstimateRemoteToLocalClockOffset(), kEpsilon);

  // Remote sends corrupted RTCP SRs
  AdvanceTime(TimeDelta::Seconds(1));
  SendRtcpSrInaccurately(/*ntp_error=*/TimeDelta::Millis(2),
                         /*networking_delay=*/TimeDelta::Millis(-1));
  AdvanceTime(TimeDelta::Seconds(1));
  SendRtcpSrInaccurately(/*ntp_error=*/TimeDelta::Millis(-2),
                         /*networking_delay=*/TimeDelta::Millis(1));

  // New RTP packet to estimate timestamp.
  AdvanceTime(TimeDelta::Millis(150));
  rtp_timestamp = GetRemoteTimestamp();
  capture_ntp_time_ms = local_clock_.CurrentNtpInMilliseconds();

  // Errors should be averaged out.
  EXPECT_EQ(capture_ntp_time_ms, estimator_.Estimate(rtp_timestamp));
  EXPECT_NEAR(kRemoteToLocalClockOffsetNtp,
              *estimator_.EstimateRemoteToLocalClockOffset(), kEpsilon);
}

TEST_F(RemoteNtpTimeEstimatorTest, EstimateUsingRrtrLogic) {
  // This test emulates estimation using the logic embedded in the
  // handler code for RRTR and DLRR (receiver side RTT estimate).
  // It is subtly different from the sender-side RTT estimate simulated
  // in the "Estimate" test.

  // 1. Simulate receiver sending RRTR.
  const NtpTime t1 = local_clock_.CurrentNtpTime();

  // 2. Simulate sender receiving RRTR and sending DLRR.
  // Assume a one-way delay of 10ms and a remote processing delay of 5ms.
  const TimeDelta kOneWayDelay = TimeDelta::Millis(10);
  const TimeDelta kRemoteProcessingDelay = TimeDelta::Millis(5);

  AdvanceTime(kOneWayDelay);            // Remote receives RRTR at t2.
  AdvanceTime(kRemoteProcessingDelay);  // Remote sends DLRR at t3.

  // 3. Receiver receives DLRR.
  AdvanceTime(kOneWayDelay);  // Local receives DLRR at t4.
  const NtpTime t4 = local_clock_.CurrentNtpTime();

  // RTT calculation (as done in RTCPReceiver::HandleXrDlrrReportBlock):
  // RTT = (t4 - t1) - (t3 - t2)
  const uint32_t last_rr = CompactNtp(t1);
  const uint32_t delay_since_last_rr =
      SaturatedToCompactNtp(kRemoteProcessingDelay);
  const uint32_t now_ntp = CompactNtp(t4);
  const uint32_t rtt_compact = now_ntp - delay_since_last_rr - last_rr;
  const TimeDelta rtt = CompactNtpRttToTimeDelta(rtt_compact);

  // Expect RTT to be approximately 20ms (2 * kOneWayDelay).
  EXPECT_THAT(rtt, Near(2 * kOneWayDelay, TimeDelta::Millis(1)));

  AdvanceTime(TimeDelta::Millis(100));
  // Remote sends Sender Report.
  ReceiveRemoteSr(kOneWayDelay, rtt);
  // Local peer needs at least 2 RTCP SR to calculate the capture time.
  EXPECT_EQ(estimator_.EstimateRemoteToLocalClockOffset(), std::nullopt);

  // Second SR update.
  AdvanceTime(TimeDelta::Millis(800));
  ReceiveRemoteSr(kOneWayDelay, rtt);

  // Third SR update.
  AdvanceTime(TimeDelta::Millis(800));
  ReceiveRemoteSr(kOneWayDelay, rtt);

  // Verify that the estimated offset is correct.
  // The epsilon is in NTP ticks (approx 0.2 ns), so this number
  // corresponds to an epsilon of 5 microseconds.
  constexpr int64_t kDlrrEpsilon = 10000;
  EXPECT_THAT(estimator_.EstimateRemoteToLocalClockOffset(),
              Optional(Near(kRemoteToLocalClockOffsetNtp, kDlrrEpsilon)));
}

}  // namespace
}  // namespace webrtc
