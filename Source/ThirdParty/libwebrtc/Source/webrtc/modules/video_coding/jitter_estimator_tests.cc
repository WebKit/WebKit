/*  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/jitter_estimator.h"

#include "rtc_base/experiments/jitter_upper_bound_experiment.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/histogram_percentile_counter.h"
#include "rtc_base/timeutils.h"
#include "system_wrappers/include/clock.h"
#include "test/field_trial.h"
#include "test/gtest.h"

namespace webrtc {

class TestVCMJitterEstimator : public ::testing::Test {
 protected:
  TestVCMJitterEstimator() : fake_clock_(0) {}

  virtual void SetUp() {
    estimator_ = absl::make_unique<VCMJitterEstimator>(&fake_clock_, 0, 0);
  }

  void AdvanceClock(int64_t microseconds) {
    fake_clock_.AdvanceTimeMicroseconds(microseconds);
  }

  SimulatedClock fake_clock_;
  std::unique_ptr<VCMJitterEstimator> estimator_;
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
  uint64_t time_delta_us = rtc::kNumMicrosecsPerSec / 5;
  for (int i = 0; i < 60; ++i) {
    estimator_->UpdateEstimate(gen.Delay(), gen.FrameSize());
    AdvanceClock(time_delta_us);
    if (i > 2)
      EXPECT_EQ(estimator_->GetJitterEstimate(0), 0);
    gen.Advance();
  }
}

TEST_F(TestVCMJitterEstimator, TestUpperBound) {
  struct TestContext {
    TestContext() : upper_bound(0.0), percentiles(1000) {}
    double upper_bound;
    rtc::HistogramPercentileCounter percentiles;
  };
  std::vector<TestContext> test_cases(2);

  test_cases[0].upper_bound = 100.0;  // First use essentially no cap.
  test_cases[1].upper_bound = 3.5;    // Second, reasonably small cap.

  for (TestContext& context : test_cases) {
    // Set up field trial and reset jitter estimator.
    char string_buf[64];
    rtc::SimpleStringBuilder ssb(string_buf);
    ssb << JitterUpperBoundExperiment::kJitterUpperBoundExperimentName
        << "/Enabled-" << context.upper_bound << "/";
    test::ScopedFieldTrials field_trials(ssb.str());
    SetUp();

    ValueGenerator gen(50);
    uint64_t time_delta_us = rtc::kNumMicrosecsPerSec / 30;
    for (int i = 0; i < 100; ++i) {
      estimator_->UpdateEstimate(gen.Delay(), gen.FrameSize());
      AdvanceClock(time_delta_us);
      context.percentiles.Add(
          static_cast<uint32_t>(estimator_->GetJitterEstimate(0)));
      gen.Advance();
    }
  }

  // Median should be similar after three seconds. Allow 5% error margin.
  uint32_t median_unbound = *test_cases[0].percentiles.GetPercentile(0.5);
  uint32_t median_bounded = *test_cases[1].percentiles.GetPercentile(0.5);
  EXPECT_NEAR(median_unbound, median_bounded, (median_unbound * 5) / 100);

  // Max should be lower for the bounded case.
  uint32_t max_unbound = *test_cases[0].percentiles.GetPercentile(1.0);
  uint32_t max_bounded = *test_cases[1].percentiles.GetPercentile(1.0);
  EXPECT_GT(max_unbound, static_cast<uint32_t>(max_bounded * 1.25));
}

}  // namespace webrtc
