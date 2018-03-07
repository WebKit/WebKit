/*  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/jitter_estimator.h"

#include "system_wrappers/include/clock.h"
#include "test/gtest.h"

namespace webrtc {

class TestEstimator : public VCMJitterEstimator {
 public:
  explicit TestEstimator(bool exp_enabled)
      : VCMJitterEstimator(&fake_clock_, 0, 0),
        fake_clock_(0),
        exp_enabled_(exp_enabled) {}

  virtual bool LowRateExperimentEnabled() { return exp_enabled_; }

  void AdvanceClock(int64_t microseconds) {
    fake_clock_.AdvanceTimeMicroseconds(microseconds);
  }

 private:
  SimulatedClock fake_clock_;
  const bool exp_enabled_;
};

class TestVCMJitterEstimator : public ::testing::Test {
 protected:
  TestVCMJitterEstimator()
      : regular_estimator_(false), low_rate_estimator_(true) {}

  virtual void SetUp() { regular_estimator_.Reset(); }

  TestEstimator regular_estimator_;
  TestEstimator low_rate_estimator_;
};

// Generates some simple test data in the form of a sawtooth wave.
class ValueGenerator {
 public:
  explicit ValueGenerator(int32_t amplitude)
      : amplitude_(amplitude), counter_(0) {}
  virtual ~ValueGenerator() {}

  int64_t Delay() { return ((counter_ % 11) - 5) * amplitude_; }

  uint32_t FrameSize() { return 1000 + Delay(); }

  void Advance() { ++counter_; }

 private:
  const int32_t amplitude_;
  int64_t counter_;
};

// 5 fps, disable jitter delay altogether.
TEST_F(TestVCMJitterEstimator, TestLowRate) {
  ValueGenerator gen(10);
  uint64_t time_delta = 1000000 / 5;
  for (int i = 0; i < 60; ++i) {
    regular_estimator_.UpdateEstimate(gen.Delay(), gen.FrameSize());
    regular_estimator_.AdvanceClock(time_delta);
    low_rate_estimator_.UpdateEstimate(gen.Delay(), gen.FrameSize());
    low_rate_estimator_.AdvanceClock(time_delta);
    EXPECT_GT(regular_estimator_.GetJitterEstimate(0), 0);
    if (i > 2)
      EXPECT_EQ(low_rate_estimator_.GetJitterEstimate(0), 0);
    gen.Advance();
  }
}

// 8 fps, steady state estimate should be in interpolated interval between 0
// and value of previous method.
TEST_F(TestVCMJitterEstimator, TestMidRate) {
  ValueGenerator gen(10);
  uint64_t time_delta = 1000000 / 8;
  for (int i = 0; i < 60; ++i) {
    regular_estimator_.UpdateEstimate(gen.Delay(), gen.FrameSize());
    regular_estimator_.AdvanceClock(time_delta);
    low_rate_estimator_.UpdateEstimate(gen.Delay(), gen.FrameSize());
    low_rate_estimator_.AdvanceClock(time_delta);
    EXPECT_GT(regular_estimator_.GetJitterEstimate(0), 0);
    EXPECT_GT(low_rate_estimator_.GetJitterEstimate(0), 0);
    EXPECT_GE(regular_estimator_.GetJitterEstimate(0),
              low_rate_estimator_.GetJitterEstimate(0));
    gen.Advance();
  }
}

// 30 fps, steady state estimate should be same as previous method.
TEST_F(TestVCMJitterEstimator, TestHighRate) {
  ValueGenerator gen(10);
  uint64_t time_delta = 1000000 / 30;
  for (int i = 0; i < 60; ++i) {
    regular_estimator_.UpdateEstimate(gen.Delay(), gen.FrameSize());
    regular_estimator_.AdvanceClock(time_delta);
    low_rate_estimator_.UpdateEstimate(gen.Delay(), gen.FrameSize());
    low_rate_estimator_.AdvanceClock(time_delta);
    EXPECT_EQ(regular_estimator_.GetJitterEstimate(0),
              low_rate_estimator_.GetJitterEstimate(0));
    gen.Advance();
  }
}

// 10 fps, high jitter then low jitter. Low rate estimator should converge
// faster to low noise estimate.
TEST_F(TestVCMJitterEstimator, TestConvergence) {
  // Reach a steady state with high noise.
  ValueGenerator gen(50);
  uint64_t time_delta = 1000000 / 10;
  for (int i = 0; i < 100; ++i) {
    regular_estimator_.UpdateEstimate(gen.Delay(), gen.FrameSize());
    regular_estimator_.AdvanceClock(time_delta * 2);
    low_rate_estimator_.UpdateEstimate(gen.Delay(), gen.FrameSize());
    low_rate_estimator_.AdvanceClock(time_delta * 2);
    gen.Advance();
  }

  int threshold = regular_estimator_.GetJitterEstimate(0) / 2;

  // New generator with zero noise.
  ValueGenerator low_gen(0);
  int regular_iterations = 0;
  int low_rate_iterations = 0;
  for (int i = 0; i < 500; ++i) {
    if (regular_iterations == 0) {
      regular_estimator_.UpdateEstimate(low_gen.Delay(), low_gen.FrameSize());
      regular_estimator_.AdvanceClock(time_delta);
      if (regular_estimator_.GetJitterEstimate(0) < threshold) {
        regular_iterations = i;
      }
    }

    if (low_rate_iterations == 0) {
      low_rate_estimator_.UpdateEstimate(low_gen.Delay(), low_gen.FrameSize());
      low_rate_estimator_.AdvanceClock(time_delta);
      if (low_rate_estimator_.GetJitterEstimate(0) < threshold) {
        low_rate_iterations = i;
      }
    }

    if (regular_iterations != 0 && low_rate_iterations != 0) {
      break;
    }

    gen.Advance();
  }

  EXPECT_NE(regular_iterations, 0);
  EXPECT_NE(low_rate_iterations, 0);
  EXPECT_LE(low_rate_iterations, regular_iterations);
}
}  // namespace webrtc
