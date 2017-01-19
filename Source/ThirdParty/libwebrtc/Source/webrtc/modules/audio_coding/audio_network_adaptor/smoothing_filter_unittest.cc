/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>

#include "webrtc/modules/audio_coding/audio_network_adaptor/smoothing_filter.h"
#include "webrtc/test/gtest.h"

namespace webrtc {

namespace {

constexpr int kTimeConstantMs = 1000;
constexpr float kMaxAbsError = 0.0001f;
constexpr int64_t kClockInitialTime = 123456;

struct SmoothingFilterStates {
  std::unique_ptr<SimulatedClock> simulated_clock;
  std::unique_ptr<SmoothingFilter> smoothing_filter;
};

SmoothingFilterStates CreateSmoothingFilter() {
  SmoothingFilterStates states;
  states.simulated_clock.reset(new SimulatedClock(kClockInitialTime));
  states.smoothing_filter.reset(
      new SmoothingFilterImpl(kTimeConstantMs, states.simulated_clock.get()));
  return states;
}

void CheckOutput(SmoothingFilterStates* states,
                 int advance_time_ms,
                 float sample,
                 float expected_ouput) {
  states->simulated_clock->AdvanceTimeMilliseconds(advance_time_ms);
  states->smoothing_filter->AddSample(sample);
  auto output = states->smoothing_filter->GetAverage();
  EXPECT_TRUE(output);
  EXPECT_NEAR(expected_ouput, *output, kMaxAbsError);
}

}  // namespace

TEST(SmoothingFilterTest, NoOutputWhenNoSampleAdded) {
  auto states = CreateSmoothingFilter();
  EXPECT_FALSE(states.smoothing_filter->GetAverage());
}

// Python script to calculate the reference values used in this test.
//  import math
//
//  class ExpFilter:
//    alpha = 0.0
//    old_value = 0.0
//    def calc(self, new_value):
//      self.old_value = self.old_value * self.alpha
//                       + (1.0 - self.alpha) * new_value
//      return self.old_value
//
//  delta_t = 100.0
//  filter = ExpFilter()
//  total_t = 100.0
//  filter.alpha = math.exp(-delta_t/ total_t)
//  print filter.calc(1.0)
//  total_t = 200.0
//  filter.alpha = math.exp(-delta_t/ total_t)
//  print filter.calc(0.0)
//  total_t = 300.0
//  filter.alpha = math.exp(-delta_t/ total_t)
//  print filter.calc(1.0)
TEST(SmoothingFilterTest, CheckBehaviorBeforeInitialized) {
  // Adding three samples, all added before |kTimeConstantMs| is reached.
  constexpr int kTimeIntervalMs = 100;
  auto states = CreateSmoothingFilter();
  states.smoothing_filter->AddSample(0.0);
  CheckOutput(&states, kTimeIntervalMs, 1.0, 0.63212f);
  CheckOutput(&states, kTimeIntervalMs, 0.0, 0.38340f);
  CheckOutput(&states, kTimeIntervalMs, 1.0, 0.55818f);
}

// Python script to calculate the reference value used in this test.
// (after defining ExpFilter as for CheckBehaviorBeforeInitialized)
//  time_constant_ms = 1000.0
//  filter = ExpFilter()
//  delta_t = 1100.0
//  filter.alpha = math.exp(-delta_t/ time_constant_ms)
//  print filter.calc(1.0)
//  delta_t = 100.0
//  filter.alpha = math.exp(-delta_t/ time_constant_ms)
//  print filter.calc(0.0)
//  print filter.calc(1.0)
TEST(SmoothingFilterTest, CheckBehaviorAfterInitialized) {
  constexpr int kTimeIntervalMs = 100;
  auto states = CreateSmoothingFilter();
  states.smoothing_filter->AddSample(0.0);
  states.simulated_clock->AdvanceTimeMilliseconds(kTimeConstantMs);
  CheckOutput(&states, kTimeIntervalMs, 1.0, 0.66713f);
  CheckOutput(&states, kTimeIntervalMs, 0.0, 0.60364f);
  CheckOutput(&states, kTimeIntervalMs, 1.0, 0.64136f);
}

}  // namespace webrtc
