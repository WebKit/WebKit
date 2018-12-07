/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/congestion_controller/goog_cc/congestion_window_pushback_controller.h"
#include "test/gmock.h"
#include "test/gtest.h"

using testing::_;

namespace webrtc {
namespace test {

class CongestionWindowPushbackControllerTest : public ::testing::Test {
 protected:
  CongestionWindowPushbackController cwnd_controller_;
};

TEST_F(CongestionWindowPushbackControllerTest, FullCongestionWindow) {
  cwnd_controller_.UpdateOutstandingData(100000);
  cwnd_controller_.UpdateMaxOutstandingData(50000);

  uint32_t bitrate_bps = 80000;
  bitrate_bps = cwnd_controller_.UpdateTargetBitrate(bitrate_bps);
  EXPECT_EQ(72000u, bitrate_bps);

  cwnd_controller_.UpdateMaxOutstandingData(50000);
  bitrate_bps = cwnd_controller_.UpdateTargetBitrate(bitrate_bps);
  EXPECT_EQ(static_cast<uint32_t>(72000 * 0.9 * 0.9), bitrate_bps);
}

TEST_F(CongestionWindowPushbackControllerTest, NormalCongestionWindow) {
  cwnd_controller_.UpdateOutstandingData(100000);
  cwnd_controller_.SetDataWindow(DataSize::bytes(200000));

  uint32_t bitrate_bps = 80000;
  bitrate_bps = cwnd_controller_.UpdateTargetBitrate(bitrate_bps);
  EXPECT_EQ(80000u, bitrate_bps);

  cwnd_controller_.UpdateMaxOutstandingData(20000);
  bitrate_bps = cwnd_controller_.UpdateTargetBitrate(bitrate_bps);
  EXPECT_EQ(80000u, bitrate_bps);
}

TEST_F(CongestionWindowPushbackControllerTest, LowBitrate) {
  cwnd_controller_.UpdateOutstandingData(100000);
  cwnd_controller_.SetDataWindow(DataSize::bytes(50000));

  uint32_t bitrate_bps = 35000;
  bitrate_bps = cwnd_controller_.UpdateTargetBitrate(bitrate_bps);
  EXPECT_EQ(static_cast<uint32_t>(35000 * 0.9), bitrate_bps);

  cwnd_controller_.UpdateMaxOutstandingData(20000);
  bitrate_bps = cwnd_controller_.UpdateTargetBitrate(bitrate_bps);
  EXPECT_EQ(30000u, bitrate_bps);
}

}  // namespace test
}  // namespace webrtc
