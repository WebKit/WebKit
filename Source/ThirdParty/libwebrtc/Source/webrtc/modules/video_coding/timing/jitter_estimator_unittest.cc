/*  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/timing/jitter_estimator.h"

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "api/array_view.h"
#include "api/field_trials.h"
#include "api/units/data_size.h"
#include "api/units/frequency.h"
#include "api/units/time_delta.h"
#include "rtc_base/numerics/histogram_percentile_counter.h"
#include "rtc_base/time_utils.h"
#include "system_wrappers/include/clock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

// Generates some simple test data in the form of a sawtooth wave.
class ValueGenerator {
 public:
  explicit ValueGenerator(int32_t amplitude)
      : amplitude_(amplitude), counter_(0) {}

  virtual ~ValueGenerator() = default;

  TimeDelta Delay() const {
    return TimeDelta::Millis((counter_ % 11) - 5) * amplitude_;
  }

  DataSize FrameSize() const {
    return DataSize::Bytes(1000 + Delay().ms() / 5);
  }

  void Advance() { ++counter_; }

 private:
  const int32_t amplitude_;
  int64_t counter_;
};

class JitterEstimatorTest : public ::testing::Test {
 protected:
  explicit JitterEstimatorTest(const std::string& field_trials)
      : fake_clock_(0),
        field_trials_(FieldTrials::CreateNoGlobal(field_trials)),
        estimator_(&fake_clock_, *field_trials_) {}
  JitterEstimatorTest() : JitterEstimatorTest("") {}
  virtual ~JitterEstimatorTest() {}

  void Run(int duration_s, int framerate_fps, ValueGenerator& gen) {
    TimeDelta tick = 1 / Frequency::Hertz(framerate_fps);
    for (int i = 0; i < duration_s * framerate_fps; ++i) {
      estimator_.UpdateEstimate(gen.Delay(), gen.FrameSize());
      fake_clock_.AdvanceTime(tick);
      gen.Advance();
    }
  }

  SimulatedClock fake_clock_;
  std::unique_ptr<FieldTrials> field_trials_;
  JitterEstimator estimator_;
};

TEST_F(JitterEstimatorTest, SteadyStateConvergence) {
  ValueGenerator gen(10);
  Run(/*duration_s=*/60, /*framerate_fps=*/30, gen);
  EXPECT_EQ(estimator_.GetJitterEstimate(0, std::nullopt).ms(), 54);
}

TEST_F(JitterEstimatorTest,
       SizeOutlierIsNotRejectedAndIncreasesJitterEstimate) {
  ValueGenerator gen(10);

  // Steady state.
  Run(/*duration_s=*/60, /*framerate_fps=*/30, gen);
  TimeDelta steady_state_jitter = estimator_.GetJitterEstimate(0, std::nullopt);

  // A single outlier frame size...
  estimator_.UpdateEstimate(gen.Delay(), 10 * gen.FrameSize());
  TimeDelta outlier_jitter = estimator_.GetJitterEstimate(0, std::nullopt);

  // ...changes the estimate.
  EXPECT_GT(outlier_jitter.ms(), 1.25 * steady_state_jitter.ms());
}

TEST_F(JitterEstimatorTest, LowFramerateDisablesJitterEstimator) {
  ValueGenerator gen(10);
  // At 5 fps, we disable jitter delay altogether.
  TimeDelta time_delta = 1 / Frequency::Hertz(5);
  for (int i = 0; i < 60; ++i) {
    estimator_.UpdateEstimate(gen.Delay(), gen.FrameSize());
    fake_clock_.AdvanceTime(time_delta);
    if (i > 2)
      EXPECT_EQ(estimator_.GetJitterEstimate(0, std::nullopt),
                TimeDelta::Zero());
    gen.Advance();
  }
}

TEST_F(JitterEstimatorTest, RttMultAddCap) {
  std::vector<std::pair<TimeDelta, rtc::HistogramPercentileCounter>>
      jitter_by_rtt_mult_cap;
  jitter_by_rtt_mult_cap.emplace_back(
      /*rtt_mult_add_cap=*/TimeDelta::Millis(10), /*long_tail_boundary=*/1000);
  jitter_by_rtt_mult_cap.emplace_back(
      /*rtt_mult_add_cap=*/TimeDelta::Millis(200), /*long_tail_boundary=*/1000);

  for (auto& [rtt_mult_add_cap, jitter] : jitter_by_rtt_mult_cap) {
    estimator_.Reset();

    ValueGenerator gen(50);
    TimeDelta time_delta = 1 / Frequency::Hertz(30);
    constexpr TimeDelta kRtt = TimeDelta::Millis(250);
    for (int i = 0; i < 100; ++i) {
      estimator_.UpdateEstimate(gen.Delay(), gen.FrameSize());
      fake_clock_.AdvanceTime(time_delta);
      estimator_.FrameNacked();
      estimator_.UpdateRtt(kRtt);
      jitter.Add(
          estimator_.GetJitterEstimate(/*rtt_mult=*/1.0, rtt_mult_add_cap)
              .ms());
      gen.Advance();
    }
  }

  // 200ms cap should result in at least 25% higher max compared to 10ms.
  EXPECT_GT(*jitter_by_rtt_mult_cap[1].second.GetPercentile(1.0),
            *jitter_by_rtt_mult_cap[0].second.GetPercentile(1.0) * 1.25);
}

// By default, the `JitterEstimator` is not robust against single large frames.
TEST_F(JitterEstimatorTest, Single2xFrameSizeImpactsJitterEstimate) {
  ValueGenerator gen(10);

  // Steady state.
  Run(/*duration_s=*/60, /*framerate_fps=*/30, gen);
  TimeDelta steady_state_jitter = estimator_.GetJitterEstimate(0, std::nullopt);

  // A single outlier frame size...
  estimator_.UpdateEstimate(gen.Delay(), 2 * gen.FrameSize());
  TimeDelta outlier_jitter = estimator_.GetJitterEstimate(0, std::nullopt);

  // ...impacts the estimate.
  EXPECT_GT(outlier_jitter.ms(), steady_state_jitter.ms());
}

// Under the default config, congested frames are used when calculating the
// noise variance, meaning that they will impact the final jitter estimate.
TEST_F(JitterEstimatorTest, CongestedFrameImpactsJitterEstimate) {
  ValueGenerator gen(10);

  // Steady state.
  Run(/*duration_s=*/10, /*framerate_fps=*/30, gen);
  TimeDelta steady_state_jitter = estimator_.GetJitterEstimate(0, std::nullopt);

  // Congested frame...
  estimator_.UpdateEstimate(-10 * gen.Delay(), 0.1 * gen.FrameSize());
  TimeDelta outlier_jitter = estimator_.GetJitterEstimate(0, std::nullopt);

  // ...impacts the estimate.
  EXPECT_GT(outlier_jitter.ms(), steady_state_jitter.ms());
}

TEST_F(JitterEstimatorTest, EmptyFieldTrialsParsesToUnsetConfig) {
  JitterEstimator::Config config = estimator_.GetConfigForTest();
  EXPECT_FALSE(config.avg_frame_size_median);
  EXPECT_FALSE(config.max_frame_size_percentile.has_value());
  EXPECT_FALSE(config.frame_size_window.has_value());
  EXPECT_FALSE(config.num_stddev_delay_clamp.has_value());
  EXPECT_FALSE(config.num_stddev_delay_outlier.has_value());
  EXPECT_FALSE(config.num_stddev_size_outlier.has_value());
  EXPECT_FALSE(config.congestion_rejection_factor.has_value());
  EXPECT_TRUE(config.estimate_noise_when_congested);
}

class FieldTrialsOverriddenJitterEstimatorTest : public JitterEstimatorTest {
 protected:
  FieldTrialsOverriddenJitterEstimatorTest()
      : JitterEstimatorTest(
            "WebRTC-JitterEstimatorConfig/"
            "avg_frame_size_median:true,"
            "max_frame_size_percentile:0.9,"
            "frame_size_window:30,"
            "num_stddev_delay_clamp:1.1,"
            "num_stddev_delay_outlier:2,"
            "num_stddev_size_outlier:3.1,"
            "congestion_rejection_factor:-1.55,"
            "estimate_noise_when_congested:false/") {}
  ~FieldTrialsOverriddenJitterEstimatorTest() {}
};

TEST_F(FieldTrialsOverriddenJitterEstimatorTest, FieldTrialsParsesCorrectly) {
  JitterEstimator::Config config = estimator_.GetConfigForTest();
  EXPECT_TRUE(config.avg_frame_size_median);
  EXPECT_EQ(*config.max_frame_size_percentile, 0.9);
  EXPECT_EQ(*config.frame_size_window, 30);
  EXPECT_EQ(*config.num_stddev_delay_clamp, 1.1);
  EXPECT_EQ(*config.num_stddev_delay_outlier, 2.0);
  EXPECT_EQ(*config.num_stddev_size_outlier, 3.1);
  EXPECT_EQ(*config.congestion_rejection_factor, -1.55);
  EXPECT_FALSE(config.estimate_noise_when_congested);
}

TEST_F(FieldTrialsOverriddenJitterEstimatorTest,
       DelayOutlierIsRejectedAndMaintainsJitterEstimate) {
  ValueGenerator gen(10);

  // Steady state.
  Run(/*duration_s=*/60, /*framerate_fps=*/30, gen);
  TimeDelta steady_state_jitter = estimator_.GetJitterEstimate(0, std::nullopt);

  // A single outlier frame size...
  estimator_.UpdateEstimate(10 * gen.Delay(), gen.FrameSize());
  TimeDelta outlier_jitter = estimator_.GetJitterEstimate(0, std::nullopt);

  // ...does not change the estimate.
  EXPECT_EQ(outlier_jitter.ms(), steady_state_jitter.ms());
}

// The field trial is configured to be robust against the `(1 - 0.9) = 10%`
// largest frames over a window of length `30`.
TEST_F(FieldTrialsOverriddenJitterEstimatorTest,
       Four2xFrameSizesImpactJitterEstimate) {
  ValueGenerator gen(10);

  // Steady state.
  Run(/*duration_s=*/60, /*framerate_fps=*/30, gen);
  TimeDelta steady_state_jitter = estimator_.GetJitterEstimate(0, std::nullopt);

  // Three outlier frames do not impact the jitter estimate.
  for (int i = 0; i < 3; ++i) {
    estimator_.UpdateEstimate(gen.Delay(), 2 * gen.FrameSize());
  }
  TimeDelta outlier_jitter_3x = estimator_.GetJitterEstimate(0, std::nullopt);
  EXPECT_EQ(outlier_jitter_3x.ms(), steady_state_jitter.ms());

  // Four outlier frames do impact the jitter estimate.
  estimator_.UpdateEstimate(gen.Delay(), 2 * gen.FrameSize());
  TimeDelta outlier_jitter_4x = estimator_.GetJitterEstimate(0, std::nullopt);
  EXPECT_GT(outlier_jitter_4x.ms(), outlier_jitter_3x.ms());
}

// When so configured, congested frames are NOT used when calculating the
// noise variance, meaning that they will NOT impact the final jitter estimate.
TEST_F(FieldTrialsOverriddenJitterEstimatorTest,
       CongestedFrameDoesNotImpactJitterEstimate) {
  ValueGenerator gen(10);

  // Steady state.
  Run(/*duration_s=*/10, /*framerate_fps=*/30, gen);
  TimeDelta steady_state_jitter = estimator_.GetJitterEstimate(0, std::nullopt);

  // Congested frame...
  estimator_.UpdateEstimate(-10 * gen.Delay(), 0.1 * gen.FrameSize());
  TimeDelta outlier_jitter = estimator_.GetJitterEstimate(0, std::nullopt);

  // ...does not impact the estimate.
  EXPECT_EQ(outlier_jitter.ms(), steady_state_jitter.ms());
}

class MisconfiguredFieldTrialsJitterEstimatorTest : public JitterEstimatorTest {
 protected:
  MisconfiguredFieldTrialsJitterEstimatorTest()
      : JitterEstimatorTest(
            "WebRTC-JitterEstimatorConfig/"
            "max_frame_size_percentile:-0.9,"
            "frame_size_window:-1,"
            "num_stddev_delay_clamp:-1.9,"
            "num_stddev_delay_outlier:-2,"
            "num_stddev_size_outlier:-23.1/") {}
  ~MisconfiguredFieldTrialsJitterEstimatorTest() {}
};

TEST_F(MisconfiguredFieldTrialsJitterEstimatorTest, FieldTrialsAreValidated) {
  JitterEstimator::Config config = estimator_.GetConfigForTest();
  EXPECT_EQ(*config.max_frame_size_percentile, 0.0);
  EXPECT_EQ(*config.frame_size_window, 1);
  EXPECT_EQ(*config.num_stddev_delay_clamp, 0.0);
  EXPECT_EQ(*config.num_stddev_delay_outlier, 0.0);
  EXPECT_EQ(*config.num_stddev_size_outlier, 0.0);
}

}  // namespace
}  // namespace webrtc
