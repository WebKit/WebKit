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

#include "absl/types/optional.h"
#include "modules/rtp_rtcp/source/time_util.h"
#include "system_wrappers/include/clock.h"
#include "system_wrappers/include/ntp_time.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

constexpr TimeDelta kTestRtt = TimeDelta::Millis(10);
constexpr Timestamp kLocalClockInitialTime = Timestamp::Millis(123);
constexpr Timestamp kRemoteClockInitialTime = Timestamp::Millis(373);
constexpr uint32_t kTimestampOffset = 567;
constexpr int64_t kRemoteToLocalClockOffsetNtp =
    ToNtpUnits(kLocalClockInitialTime - kRemoteClockInitialTime);

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
    RTC_DCHECK(estimator_.UpdateRtcpTimestamp(kTestRtt, ntp, rtcp_timestamp));
  }

  void SendRtcpSrInaccurately(TimeDelta ntp_error, TimeDelta networking_delay) {
    uint32_t rtcp_timestamp = GetRemoteTimestamp();
    int64_t ntp_error_fractions = ToNtpUnits(ntp_error);
    NtpTime ntp(static_cast<uint64_t>(remote_clock_.CurrentNtpTime()) +
                ntp_error_fractions);
    AdvanceTime(kTestRtt / 2 + networking_delay);
    RTC_DCHECK(estimator_.UpdateRtcpTimestamp(kTestRtt, ntp, rtcp_timestamp));
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
  EXPECT_EQ(estimator_.EstimateRemoteToLocalClockOffset(), absl::nullopt);

  AdvanceTime(TimeDelta::Millis(800));
  // Remote sends second RTCP SR.
  SendRtcpSr();

  // Local peer gets enough RTCP SR to calculate the capture time.
  EXPECT_EQ(capture_ntp_time_ms, estimator_.Estimate(rtp_timestamp));
  EXPECT_EQ(estimator_.EstimateRemoteToLocalClockOffset(),
            kRemoteToLocalClockOffsetNtp);
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
  EXPECT_EQ(kRemoteToLocalClockOffsetNtp,
            estimator_.EstimateRemoteToLocalClockOffset());

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
  EXPECT_EQ(kRemoteToLocalClockOffsetNtp,
            estimator_.EstimateRemoteToLocalClockOffset());
}

}  // namespace
}  // namespace webrtc
