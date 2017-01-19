/*
*  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
*
*  Use of this source code is governed by a BSD-style license
*  that can be found in the LICENSE file in the root of the source
*  tree. An additional intellectual property rights grant can be found
*  in the file PATENTS.  All contributing project authors may
*  be found in the AUTHORS file in the root of the source tree.
*/

#include "webrtc/common_types.h"
#include "webrtc/modules/rtp_rtcp/include/remote_ntp_time_estimator.h"
#include "webrtc/system_wrappers/include/clock.h"
#include "webrtc/test/gmock.h"
#include "webrtc/test/gtest.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::Return;
using ::testing::SetArgPointee;

namespace webrtc {

static const int64_t kTestRtt = 10;
static const int64_t kLocalClockInitialTimeMs = 123;
static const int64_t kRemoteClockInitialTimeMs = 345;
static const uint32_t kTimestampOffset = 567;

class RemoteNtpTimeEstimatorTest : public ::testing::Test {
 protected:
  RemoteNtpTimeEstimatorTest()
      : local_clock_(kLocalClockInitialTimeMs * 1000),
        remote_clock_(kRemoteClockInitialTimeMs * 1000),
        estimator_(&local_clock_) {}
  ~RemoteNtpTimeEstimatorTest() {}

  void AdvanceTimeMilliseconds(int64_t ms) {
    local_clock_.AdvanceTimeMilliseconds(ms);
    remote_clock_.AdvanceTimeMilliseconds(ms);
  }

  uint32_t GetRemoteTimestamp() {
    return static_cast<uint32_t>(remote_clock_.TimeInMilliseconds()) * 90 +
           kTimestampOffset;
  }

  void SendRtcpSr() {
    uint32_t rtcp_timestamp = GetRemoteTimestamp();
    uint32_t ntp_seconds;
    uint32_t ntp_fractions;
    remote_clock_.CurrentNtp(ntp_seconds, ntp_fractions);

    AdvanceTimeMilliseconds(kTestRtt / 2);
    ReceiveRtcpSr(kTestRtt, rtcp_timestamp, ntp_seconds, ntp_fractions);
  }

  void UpdateRtcpTimestamp(int64_t rtt, uint32_t ntp_secs, uint32_t ntp_frac,
                           uint32_t rtp_timestamp, bool expected_result) {
    EXPECT_EQ(expected_result,
              estimator_.UpdateRtcpTimestamp(rtt, ntp_secs, ntp_frac,
                                             rtp_timestamp));
  }

  void ReceiveRtcpSr(int64_t rtt,
                     uint32_t rtcp_timestamp,
                     uint32_t ntp_seconds,
                     uint32_t ntp_fractions) {
    UpdateRtcpTimestamp(rtt, ntp_seconds, ntp_fractions, rtcp_timestamp, true);
  }

  SimulatedClock local_clock_;
  SimulatedClock remote_clock_;
  RemoteNtpTimeEstimator estimator_;
};

TEST_F(RemoteNtpTimeEstimatorTest, Estimate) {
  // Failed without valid NTP.
  UpdateRtcpTimestamp(kTestRtt, 0, 0, 0, false);

  AdvanceTimeMilliseconds(1000);
  // Remote peer sends first RTCP SR.
  SendRtcpSr();

  // Remote sends a RTP packet.
  AdvanceTimeMilliseconds(15);
  uint32_t rtp_timestamp = GetRemoteTimestamp();
  int64_t capture_ntp_time_ms = local_clock_.CurrentNtpInMilliseconds();

  // Local peer needs at least 2 RTCP SR to calculate the capture time.
  const int64_t kNotEnoughRtcpSr = -1;
  EXPECT_EQ(kNotEnoughRtcpSr, estimator_.Estimate(rtp_timestamp));

  AdvanceTimeMilliseconds(800);
  // Remote sends second RTCP SR.
  SendRtcpSr();

  // Local peer gets enough RTCP SR to calculate the capture time.
  EXPECT_EQ(capture_ntp_time_ms, estimator_.Estimate(rtp_timestamp));
}

}  // namespace webrtc
