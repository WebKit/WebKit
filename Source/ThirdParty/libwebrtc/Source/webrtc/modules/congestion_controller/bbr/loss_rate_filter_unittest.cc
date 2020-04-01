/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/congestion_controller/bbr/loss_rate_filter.h"

#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "test/gtest.h"

namespace webrtc {
namespace bbr {

namespace {
const Timestamp kTestStartTime = Timestamp::Seconds(100000);
}  // namespace

TEST(LossRateFilterTest, AccumulatesToOne) {
  LossRateFilter filter;
  Timestamp current_time = kTestStartTime;
  for (int i = 0; i < 10; i++) {
    filter.UpdateWithLossStatus(current_time.ms(), 10, 10);
    current_time += TimeDelta::Seconds(1);
  }
  EXPECT_NEAR(filter.GetLossRate(), 1.0, 0.01);
}

TEST(LossRateFilterTest, StaysAtZero) {
  LossRateFilter filter;
  Timestamp current_time = kTestStartTime;
  for (int i = 0; i < 10; i++) {
    filter.UpdateWithLossStatus(current_time.ms(), 10, 0);
    current_time += TimeDelta::Seconds(1);
  }
  EXPECT_NEAR(filter.GetLossRate(), 0.0, 0.01);
}

TEST(LossRateFilterTest, VariesWithInput) {
  LossRateFilter filter;
  Timestamp current_time = kTestStartTime;
  for (int j = 0; j < 10; j++) {
    for (int i = 0; i < 5; i++) {
      filter.UpdateWithLossStatus(current_time.ms(), 10, 10);
      current_time += TimeDelta::Seconds(1);
    }
    EXPECT_NEAR(filter.GetLossRate(), 1.0, 0.1);
    for (int i = 0; i < 5; i++) {
      filter.UpdateWithLossStatus(current_time.ms(), 10, 0);
      current_time += TimeDelta::Seconds(1);
    }
    EXPECT_NEAR(filter.GetLossRate(), 0.0, 0.1);
  }
}

TEST(LossRateFilterTest, DetectsChangingRate) {
  LossRateFilter filter;
  Timestamp current_time = kTestStartTime;
  for (int per_decile = 0; per_decile < 10; per_decile += 1) {
    // Update every 200 ms for 2 seconds
    for (int i = 0; i < 10; i++) {
      current_time += TimeDelta::Millis(200);
      filter.UpdateWithLossStatus(current_time.ms(), 10, per_decile);
    }
    EXPECT_NEAR(filter.GetLossRate(), per_decile / 10.0, 0.05);
  }
}
}  // namespace bbr
}  // namespace webrtc
