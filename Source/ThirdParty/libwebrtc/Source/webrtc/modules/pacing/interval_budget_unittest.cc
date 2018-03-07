/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/pacing/interval_budget.h"

#include "test/gtest.h"

namespace webrtc {

namespace {
constexpr int kWindowMs = 500;
constexpr int kBitrateKbps = 100;
constexpr bool kCanBuildUpUnderuse = true;
constexpr bool kCanNotBuildUpUnderuse = false;
size_t TimeToBytes(int bitrate_kbps, int time_ms) {
  return static_cast<size_t>(bitrate_kbps * time_ms / 8);
}
}  // namespace

TEST(IntervalBudgetTest, InitailState) {
  IntervalBudget interval_budget(kBitrateKbps);
  EXPECT_EQ(interval_budget.budget_level_percent(), 0);
  EXPECT_EQ(interval_budget.bytes_remaining(), 0u);
}

TEST(IntervalBudgetTest, Underuse) {
  IntervalBudget interval_budget(kBitrateKbps);
  int delta_time_ms = 50;
  interval_budget.IncreaseBudget(delta_time_ms);
  EXPECT_EQ(interval_budget.budget_level_percent(), kWindowMs / delta_time_ms);
  EXPECT_EQ(interval_budget.bytes_remaining(),
            TimeToBytes(kBitrateKbps, delta_time_ms));
}

TEST(IntervalBudgetTest, DontUnderuseMoreThanMaxWindow) {
  IntervalBudget interval_budget(kBitrateKbps);
  int delta_time_ms = 1000;
  interval_budget.IncreaseBudget(delta_time_ms);
  EXPECT_EQ(interval_budget.budget_level_percent(), 100);
  EXPECT_EQ(interval_budget.bytes_remaining(),
            TimeToBytes(kBitrateKbps, kWindowMs));
}

TEST(IntervalBudgetTest, DontUnderuseMoreThanMaxWindowWhenChangeBitrate) {
  IntervalBudget interval_budget(kBitrateKbps);
  int delta_time_ms = kWindowMs / 2;
  interval_budget.IncreaseBudget(delta_time_ms);
  interval_budget.set_target_rate_kbps(kBitrateKbps / 10);
  EXPECT_EQ(interval_budget.budget_level_percent(), 100);
  EXPECT_EQ(interval_budget.bytes_remaining(),
            TimeToBytes(kBitrateKbps / 10, kWindowMs));
}

TEST(IntervalBudgetTest, BalanceChangeOnBitrateChange) {
  IntervalBudget interval_budget(kBitrateKbps);
  int delta_time_ms = kWindowMs;
  interval_budget.IncreaseBudget(delta_time_ms);
  interval_budget.set_target_rate_kbps(kBitrateKbps * 2);
  EXPECT_EQ(interval_budget.budget_level_percent(), 50);
  EXPECT_EQ(interval_budget.bytes_remaining(),
            TimeToBytes(kBitrateKbps, kWindowMs));
}

TEST(IntervalBudgetTest, Overuse) {
  IntervalBudget interval_budget(kBitrateKbps);
  int overuse_time_ms = 50;
  int used_bytes = TimeToBytes(kBitrateKbps, overuse_time_ms);
  interval_budget.UseBudget(used_bytes);
  EXPECT_EQ(interval_budget.budget_level_percent(),
            -kWindowMs / overuse_time_ms);
  EXPECT_EQ(interval_budget.bytes_remaining(), 0u);
}

TEST(IntervalBudgetTest, DontOveruseMoreThanMaxWindow) {
  IntervalBudget interval_budget(kBitrateKbps);
  int overuse_time_ms = 1000;
  int used_bytes = TimeToBytes(kBitrateKbps, overuse_time_ms);
  interval_budget.UseBudget(used_bytes);
  EXPECT_EQ(interval_budget.budget_level_percent(), -100);
  EXPECT_EQ(interval_budget.bytes_remaining(), 0u);
}

TEST(IntervalBudgetTest, CanBuildUpUnderuseWhenConfigured) {
  IntervalBudget interval_budget(kBitrateKbps, kCanBuildUpUnderuse);
  int delta_time_ms = 50;
  interval_budget.IncreaseBudget(delta_time_ms);
  EXPECT_EQ(interval_budget.budget_level_percent(), kWindowMs / delta_time_ms);
  EXPECT_EQ(interval_budget.bytes_remaining(),
            TimeToBytes(kBitrateKbps, delta_time_ms));

  interval_budget.IncreaseBudget(delta_time_ms);
  EXPECT_EQ(interval_budget.budget_level_percent(),
            2 * kWindowMs / delta_time_ms);
  EXPECT_EQ(interval_budget.bytes_remaining(),
            TimeToBytes(kBitrateKbps, 2 * delta_time_ms));
}

TEST(IntervalBudgetTest, CanNotBuildUpUnderuseWhenConfigured) {
  IntervalBudget interval_budget(kBitrateKbps, kCanNotBuildUpUnderuse);
  int delta_time_ms = 50;
  interval_budget.IncreaseBudget(delta_time_ms);
  EXPECT_EQ(interval_budget.budget_level_percent(), kWindowMs / delta_time_ms);
  EXPECT_EQ(interval_budget.bytes_remaining(),
            TimeToBytes(kBitrateKbps, delta_time_ms));

  interval_budget.IncreaseBudget(delta_time_ms);
  EXPECT_EQ(interval_budget.budget_level_percent(), kWindowMs / delta_time_ms);
  EXPECT_EQ(interval_budget.bytes_remaining(),
            TimeToBytes(kBitrateKbps, delta_time_ms));
}

}  // namespace webrtc
