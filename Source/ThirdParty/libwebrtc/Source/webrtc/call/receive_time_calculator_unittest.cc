/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "call/receive_time_calculator.h"

#include "test/gtest.h"

namespace webrtc {
namespace test {
namespace {

int64_t ReconcileMs(ReceiveTimeCalculator* calc,
                    int64_t packet_time_ms,
                    int64_t safe_time_ms) {
  return calc->ReconcileReceiveTimes(packet_time_ms * 1000,
                                     safe_time_ms * 1000) /
         1000;
}
}  // namespace

TEST(ReceiveTimeCalculatorTest, UsesSmallerIncrements) {
  int64_t kMinDeltaMs = -20;
  int64_t kMaxDeltaDiffMs = 100;
  ReceiveTimeCalculator calc(kMinDeltaMs, kMaxDeltaDiffMs);
  // Initialize offset.
  ReconcileMs(&calc, 10000, 20000);

  EXPECT_EQ(ReconcileMs(&calc, 10010, 20100), 20010);
  EXPECT_EQ(ReconcileMs(&calc, 10020, 20100), 20020);
  EXPECT_EQ(ReconcileMs(&calc, 10030, 20100), 20030);

  EXPECT_EQ(ReconcileMs(&calc, 10110, 20200), 20110);
  EXPECT_EQ(ReconcileMs(&calc, 10120, 20200), 20120);
  EXPECT_EQ(ReconcileMs(&calc, 10130, 20200), 20130);

  // Small jumps backwards are let trough, they might be due to reordering.
  EXPECT_EQ(ReconcileMs(&calc, 10120, 20200), 20120);
  // The safe clock might be smaller than the packet clock.
  EXPECT_EQ(ReconcileMs(&calc, 10210, 20200), 20210);
  EXPECT_EQ(ReconcileMs(&calc, 10240, 20200), 20240);
}

TEST(ReceiveTimeCalculatorTest, CorrectsJumps) {
  int64_t kMinDeltaMs = -20;
  int64_t kMaxDeltaDiffMs = 100;
  ReceiveTimeCalculator calc(kMinDeltaMs, kMaxDeltaDiffMs);
  // Initialize offset.
  ReconcileMs(&calc, 10000, 20000);

  EXPECT_EQ(ReconcileMs(&calc, 10010, 20100), 20010);
  EXPECT_EQ(ReconcileMs(&calc, 10020, 20100), 20020);
  EXPECT_EQ(ReconcileMs(&calc, 10030, 20100), 20030);

  // Jump forward in time.
  EXPECT_EQ(ReconcileMs(&calc, 10240, 20200), 20200);
  EXPECT_EQ(ReconcileMs(&calc, 10250, 20200), 20210);
  EXPECT_EQ(ReconcileMs(&calc, 10260, 20200), 20220);

  // Jump backward in time.
  EXPECT_EQ(ReconcileMs(&calc, 10230, 20300), 20300);
  EXPECT_EQ(ReconcileMs(&calc, 10240, 20300), 20310);
  EXPECT_EQ(ReconcileMs(&calc, 10250, 20300), 20320);
}

}  // namespace test

}  // namespace webrtc
