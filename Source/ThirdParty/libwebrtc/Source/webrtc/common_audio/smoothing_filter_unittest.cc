/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <cmath>
#include <memory>

#include "webrtc/common_audio/smoothing_filter.h"
#include "webrtc/test/gtest.h"

namespace webrtc {

namespace {

constexpr float kMaxAbsError = 1e-5f;
constexpr int64_t kClockInitialTime = 123456;

struct SmoothingFilterStates {
  std::unique_ptr<SimulatedClock> simulated_clock;
  std::unique_ptr<SmoothingFilterImpl> smoothing_filter;
};

SmoothingFilterStates CreateSmoothingFilter(int init_time_ms) {
  SmoothingFilterStates states;
  states.simulated_clock.reset(new SimulatedClock(kClockInitialTime));
  states.smoothing_filter.reset(
      new SmoothingFilterImpl(init_time_ms, states.simulated_clock.get()));
  return states;
}

// This function does the following:
//   1. Add a sample to filter at current clock,
//   2. Advance the clock by |advance_time_ms|,
//   3. Get the output of both SmoothingFilter and verify that it equals to an
//      expected value.
void CheckOutput(SmoothingFilterStates* states,
                 float sample,
                 int advance_time_ms,
                 float expected_ouput) {
  states->smoothing_filter->AddSample(sample);
  states->simulated_clock->AdvanceTimeMilliseconds(advance_time_ms);
  auto output = states->smoothing_filter->GetAverage();
  EXPECT_TRUE(output);
  EXPECT_NEAR(expected_ouput, *output, kMaxAbsError);
}

}  // namespace

TEST(SmoothingFilterTest, NoOutputWhenNoSampleAdded) {
  constexpr int kInitTimeMs = 100;
  auto states = CreateSmoothingFilter(kInitTimeMs);
  EXPECT_FALSE(states.smoothing_filter->GetAverage());
}

// Python script to calculate the reference values used in this test.
//   import math
//
//   class ExpFilter:
//     def add_sample(self, new_value):
//       self.state = self.state * self.alpha + (1.0 - self.alpha) * new_value
//
//   filter = ExpFilter()
//   init_time = 795
//   init_factor = (1.0 / init_time) ** (1.0 / init_time)
//
//   filter.state = 1.0
//
//   for time_now in range(1, 500):
//     filter.alpha = math.exp(-init_factor ** time_now)
//     filter.add_sample(1.0)
//   print filter.state
//
//   for time_now in range(500, 600):
//     filter.alpha = math.exp(-init_factor ** time_now)
//     filter.add_sample(0.5)
//   print filter.state
//
//   for time_now in range(600, 700):
//     filter.alpha = math.exp(-init_factor ** time_now)
//     filter.add_sample(1.0)
//   print filter.state
//
//   for time_now in range(700, init_time):
//     filter.alpha = math.exp(-init_factor ** time_now)
//     filter.add_sample(1.0)
//
//   filter.alpha = math.exp(-1.0 / init_time)
//   for time_now in range(init_time, 800):
//     filter.add_sample(1.0)
//   print filter.state
//
//   for i in range(800, 900):
//     filter.add_sample(0.5)
//   print filter.state
//
//   for i in range(900, 1000):
//     filter.add_sample(1.0)
//   print filter.state
TEST(SmoothingFilterTest, CheckBehaviorAroundInitTime) {
  constexpr int kInitTimeMs = 795;
  auto states = CreateSmoothingFilter(kInitTimeMs);
  CheckOutput(&states, 1.0f, 500, 1.0f);
  CheckOutput(&states, 0.5f, 100, 0.680562264029f);
  CheckOutput(&states, 1.0f, 100, 0.794207139813f);
  // Next step will go across initialization time.
  CheckOutput(&states, 1.0f, 100, 0.829803409752f);
  CheckOutput(&states, 0.5f, 100, 0.790821764210f);
  CheckOutput(&states, 1.0f, 100, 0.815545922911f);
}

TEST(SmoothingFilterTest, InitTimeEqualsZero) {
  constexpr int kInitTimeMs = 0;
  auto states = CreateSmoothingFilter(kInitTimeMs);
  CheckOutput(&states, 1.0f, 1, 1.0f);
  CheckOutput(&states, 0.5f, 1, 0.5f);
}

TEST(SmoothingFilterTest, InitTimeEqualsOne) {
  constexpr int kInitTimeMs = 1;
  auto states = CreateSmoothingFilter(kInitTimeMs);
  CheckOutput(&states, 1.0f, 1, 1.0f);
  CheckOutput(&states, 0.5f, 1, 1.0f * exp(-1.0f) + (1.0f - exp(-1.0f)) * 0.5f);
}

TEST(SmoothingFilterTest, GetAverageOutputsEmptyBeforeFirstSample) {
  constexpr int kInitTimeMs = 100;
  auto states = CreateSmoothingFilter(kInitTimeMs);
  EXPECT_FALSE(states.smoothing_filter->GetAverage());
  constexpr float kFirstSample = 1.2345f;
  states.smoothing_filter->AddSample(kFirstSample);
  EXPECT_EQ(rtc::Optional<float>(kFirstSample),
            states.smoothing_filter->GetAverage());
}

TEST(SmoothingFilterTest, CannotChangeTimeConstantDuringInitialization) {
  constexpr int kInitTimeMs = 100;
  auto states = CreateSmoothingFilter(kInitTimeMs);
  states.smoothing_filter->AddSample(0.0);

  // During initialization, |SetTimeConstantMs| does not take effect.
  states.simulated_clock->AdvanceTimeMilliseconds(kInitTimeMs - 1);
  states.smoothing_filter->AddSample(0.0);

  EXPECT_FALSE(states.smoothing_filter->SetTimeConstantMs(kInitTimeMs * 2));
  EXPECT_NE(exp(-1.0f / (kInitTimeMs * 2)), states.smoothing_filter->alpha());

  states.simulated_clock->AdvanceTimeMilliseconds(1);
  states.smoothing_filter->AddSample(0.0);
  // When initialization finishes, the time constant should be come
  // |kInitTimeConstantMs|.
  EXPECT_FLOAT_EQ(exp(-1.0f / kInitTimeMs), states.smoothing_filter->alpha());

  // After initialization, |SetTimeConstantMs| takes effect.
  EXPECT_TRUE(states.smoothing_filter->SetTimeConstantMs(kInitTimeMs * 2));
  EXPECT_FLOAT_EQ(exp(-1.0f / (kInitTimeMs * 2)),
                  states.smoothing_filter->alpha());
}

}  // namespace webrtc
