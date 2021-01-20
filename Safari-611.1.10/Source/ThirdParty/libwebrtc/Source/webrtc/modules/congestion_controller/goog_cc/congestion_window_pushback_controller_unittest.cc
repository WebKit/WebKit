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

#include <memory>

#include "api/transport/field_trial_based_config.h"
#include "test/field_trial.h"
#include "test/gmock.h"
#include "test/gtest.h"

using ::testing::_;

namespace webrtc {
namespace test {

class CongestionWindowPushbackControllerTest : public ::testing::Test {
 public:
  CongestionWindowPushbackControllerTest() {
    cwnd_controller_.reset(
        new CongestionWindowPushbackController(&field_trial_config_));
  }

 protected:
  FieldTrialBasedConfig field_trial_config_;

  std::unique_ptr<CongestionWindowPushbackController> cwnd_controller_;
};

TEST_F(CongestionWindowPushbackControllerTest, FullCongestionWindow) {
  cwnd_controller_->UpdateOutstandingData(100000);
  cwnd_controller_->SetDataWindow(DataSize::Bytes(50000));

  uint32_t bitrate_bps = 80000;
  bitrate_bps = cwnd_controller_->UpdateTargetBitrate(bitrate_bps);
  EXPECT_EQ(72000u, bitrate_bps);

  cwnd_controller_->SetDataWindow(DataSize::Bytes(50000));
  bitrate_bps = cwnd_controller_->UpdateTargetBitrate(bitrate_bps);
  EXPECT_EQ(static_cast<uint32_t>(72000 * 0.9 * 0.9), bitrate_bps);
}

TEST_F(CongestionWindowPushbackControllerTest, NormalCongestionWindow) {
  cwnd_controller_->UpdateOutstandingData(199999);
  cwnd_controller_->SetDataWindow(DataSize::Bytes(200000));

  uint32_t bitrate_bps = 80000;
  bitrate_bps = cwnd_controller_->UpdateTargetBitrate(bitrate_bps);
  EXPECT_EQ(80000u, bitrate_bps);
}

TEST_F(CongestionWindowPushbackControllerTest, LowBitrate) {
  cwnd_controller_->UpdateOutstandingData(100000);
  cwnd_controller_->SetDataWindow(DataSize::Bytes(50000));

  uint32_t bitrate_bps = 35000;
  bitrate_bps = cwnd_controller_->UpdateTargetBitrate(bitrate_bps);
  EXPECT_EQ(static_cast<uint32_t>(35000 * 0.9), bitrate_bps);

  cwnd_controller_->SetDataWindow(DataSize::Bytes(20000));
  bitrate_bps = cwnd_controller_->UpdateTargetBitrate(bitrate_bps);
  EXPECT_EQ(30000u, bitrate_bps);
}

TEST_F(CongestionWindowPushbackControllerTest, NoPushbackOnDataWindowUnset) {
  cwnd_controller_->UpdateOutstandingData(1e8);  // Large number

  uint32_t bitrate_bps = 80000;
  bitrate_bps = cwnd_controller_->UpdateTargetBitrate(bitrate_bps);
  EXPECT_EQ(80000u, bitrate_bps);
}

TEST_F(CongestionWindowPushbackControllerTest, PushbackOnInititialDataWindow) {
  test::ScopedFieldTrials trials("WebRTC-CongestionWindow/InitWin:100000/");
  cwnd_controller_.reset(
      new CongestionWindowPushbackController(&field_trial_config_));
  cwnd_controller_->UpdateOutstandingData(1e8);  // Large number

  uint32_t bitrate_bps = 80000;
  bitrate_bps = cwnd_controller_->UpdateTargetBitrate(bitrate_bps);
  EXPECT_GT(80000u, bitrate_bps);
}

TEST_F(CongestionWindowPushbackControllerTest, PushbackDropFrame) {
  test::ScopedFieldTrials trials("WebRTC-CongestionWindow/DropFrame:true/");
  cwnd_controller_.reset(
      new CongestionWindowPushbackController(&field_trial_config_));
  cwnd_controller_->UpdateOutstandingData(1e8);  // Large number
  cwnd_controller_->SetDataWindow(DataSize::Bytes(50000));

  uint32_t bitrate_bps = 80000;
  bitrate_bps = cwnd_controller_->UpdateTargetBitrate(bitrate_bps);
  EXPECT_GT(80000u, bitrate_bps);
}

}  // namespace test
}  // namespace webrtc
