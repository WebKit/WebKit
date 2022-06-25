/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc2/interpolated_gain_curve.h"

#include <array>
#include <type_traits>
#include <vector>

#include "api/array_view.h"
#include "common_audio/include/audio_util.h"
#include "modules/audio_processing/agc2/agc2_common.h"
#include "modules/audio_processing/agc2/compute_interpolated_gain_curve.h"
#include "modules/audio_processing/agc2/limiter_db_gain_curve.h"
#include "modules/audio_processing/logging/apm_data_dumper.h"
#include "rtc_base/checks.h"
#include "rtc_base/gunit.h"

namespace webrtc {
namespace {

constexpr double kLevelEpsilon = 1e-2 * kMaxAbsFloatS16Value;
constexpr float kInterpolatedGainCurveTolerance = 1.f / 32768.f;
ApmDataDumper apm_data_dumper(0);
static_assert(std::is_trivially_destructible<LimiterDbGainCurve>::value, "");
const LimiterDbGainCurve limiter;

}  // namespace

TEST(GainController2InterpolatedGainCurve, CreateUse) {
  InterpolatedGainCurve igc(&apm_data_dumper, "");

  const auto levels = test::LinSpace(
      kLevelEpsilon, DbfsToFloatS16(limiter.max_input_level_db() + 1), 500);
  for (const auto level : levels) {
    EXPECT_GE(igc.LookUpGainToApply(level), 0.0f);
  }
}

TEST(GainController2InterpolatedGainCurve, CheckValidOutput) {
  InterpolatedGainCurve igc(&apm_data_dumper, "");

  const auto levels = test::LinSpace(
      kLevelEpsilon, limiter.max_input_level_linear() * 2.0, 500);
  for (const auto level : levels) {
    SCOPED_TRACE(std::to_string(level));
    const float gain = igc.LookUpGainToApply(level);
    EXPECT_LE(0.0f, gain);
    EXPECT_LE(gain, 1.0f);
  }
}

TEST(GainController2InterpolatedGainCurve, CheckMonotonicity) {
  InterpolatedGainCurve igc(&apm_data_dumper, "");

  const auto levels = test::LinSpace(
      kLevelEpsilon, limiter.max_input_level_linear() + kLevelEpsilon + 0.5,
      500);
  float prev_gain = igc.LookUpGainToApply(0.0f);
  for (const auto level : levels) {
    const float gain = igc.LookUpGainToApply(level);
    EXPECT_GE(prev_gain, gain);
    prev_gain = gain;
  }
}

TEST(GainController2InterpolatedGainCurve, CheckApproximation) {
  InterpolatedGainCurve igc(&apm_data_dumper, "");

  const auto levels = test::LinSpace(
      kLevelEpsilon, limiter.max_input_level_linear() - kLevelEpsilon, 500);
  for (const auto level : levels) {
    SCOPED_TRACE(std::to_string(level));
    EXPECT_LT(
        std::fabs(limiter.GetGainLinear(level) - igc.LookUpGainToApply(level)),
        kInterpolatedGainCurveTolerance);
  }
}

TEST(GainController2InterpolatedGainCurve, CheckRegionBoundaries) {
  InterpolatedGainCurve igc(&apm_data_dumper, "");

  const std::vector<double> levels{
      {kLevelEpsilon, limiter.knee_start_linear() + kLevelEpsilon,
       limiter.limiter_start_linear() + kLevelEpsilon,
       limiter.max_input_level_linear() + kLevelEpsilon}};
  for (const auto level : levels) {
    igc.LookUpGainToApply(level);
  }

  const auto stats = igc.get_stats();
  EXPECT_EQ(1ul, stats.look_ups_identity_region);
  EXPECT_EQ(1ul, stats.look_ups_knee_region);
  EXPECT_EQ(1ul, stats.look_ups_limiter_region);
  EXPECT_EQ(1ul, stats.look_ups_saturation_region);
}

TEST(GainController2InterpolatedGainCurve, CheckIdentityRegion) {
  constexpr size_t kNumSteps = 10;
  InterpolatedGainCurve igc(&apm_data_dumper, "");

  const auto levels =
      test::LinSpace(kLevelEpsilon, limiter.knee_start_linear(), kNumSteps);
  for (const auto level : levels) {
    SCOPED_TRACE(std::to_string(level));
    EXPECT_EQ(1.0f, igc.LookUpGainToApply(level));
  }

  const auto stats = igc.get_stats();
  EXPECT_EQ(kNumSteps - 1, stats.look_ups_identity_region);
  EXPECT_EQ(1ul, stats.look_ups_knee_region);
  EXPECT_EQ(0ul, stats.look_ups_limiter_region);
  EXPECT_EQ(0ul, stats.look_ups_saturation_region);
}

TEST(GainController2InterpolatedGainCurve, CheckNoOverApproximationKnee) {
  constexpr size_t kNumSteps = 10;
  InterpolatedGainCurve igc(&apm_data_dumper, "");

  const auto levels =
      test::LinSpace(limiter.knee_start_linear() + kLevelEpsilon,
                     limiter.limiter_start_linear(), kNumSteps);
  for (const auto level : levels) {
    SCOPED_TRACE(std::to_string(level));
    // Small tolerance added (needed because comparing a float with a double).
    EXPECT_LE(igc.LookUpGainToApply(level),
              limiter.GetGainLinear(level) + 1e-7);
  }

  const auto stats = igc.get_stats();
  EXPECT_EQ(0ul, stats.look_ups_identity_region);
  EXPECT_EQ(kNumSteps - 1, stats.look_ups_knee_region);
  EXPECT_EQ(1ul, stats.look_ups_limiter_region);
  EXPECT_EQ(0ul, stats.look_ups_saturation_region);
}

TEST(GainController2InterpolatedGainCurve, CheckNoOverApproximationBeyondKnee) {
  constexpr size_t kNumSteps = 10;
  InterpolatedGainCurve igc(&apm_data_dumper, "");

  const auto levels = test::LinSpace(
      limiter.limiter_start_linear() + kLevelEpsilon,
      limiter.max_input_level_linear() - kLevelEpsilon, kNumSteps);
  for (const auto level : levels) {
    SCOPED_TRACE(std::to_string(level));
    // Small tolerance added (needed because comparing a float with a double).
    EXPECT_LE(igc.LookUpGainToApply(level),
              limiter.GetGainLinear(level) + 1e-7);
  }

  const auto stats = igc.get_stats();
  EXPECT_EQ(0ul, stats.look_ups_identity_region);
  EXPECT_EQ(0ul, stats.look_ups_knee_region);
  EXPECT_EQ(kNumSteps, stats.look_ups_limiter_region);
  EXPECT_EQ(0ul, stats.look_ups_saturation_region);
}

TEST(GainController2InterpolatedGainCurve,
     CheckNoOverApproximationWithSaturation) {
  constexpr size_t kNumSteps = 3;
  InterpolatedGainCurve igc(&apm_data_dumper, "");

  const auto levels = test::LinSpace(
      limiter.max_input_level_linear() + kLevelEpsilon,
      limiter.max_input_level_linear() + kLevelEpsilon + 0.5, kNumSteps);
  for (const auto level : levels) {
    SCOPED_TRACE(std::to_string(level));
    EXPECT_LE(igc.LookUpGainToApply(level), limiter.GetGainLinear(level));
  }

  const auto stats = igc.get_stats();
  EXPECT_EQ(0ul, stats.look_ups_identity_region);
  EXPECT_EQ(0ul, stats.look_ups_knee_region);
  EXPECT_EQ(0ul, stats.look_ups_limiter_region);
  EXPECT_EQ(kNumSteps, stats.look_ups_saturation_region);
}

TEST(GainController2InterpolatedGainCurve, CheckApproximationParams) {
  test::InterpolatedParameters parameters =
      test::ComputeInterpolatedGainCurveApproximationParams();

  InterpolatedGainCurve igc(&apm_data_dumper, "");

  for (size_t i = 0; i < kInterpolatedGainCurveTotalPoints; ++i) {
    // The tolerance levels are chosen to account for deviations due
    // to computing with single precision floating point numbers.
    EXPECT_NEAR(igc.approximation_params_x_[i],
                parameters.computed_approximation_params_x[i], 0.9f);
    EXPECT_NEAR(igc.approximation_params_m_[i],
                parameters.computed_approximation_params_m[i], 0.00001f);
    EXPECT_NEAR(igc.approximation_params_q_[i],
                parameters.computed_approximation_params_q[i], 0.001f);
  }
}

}  // namespace webrtc
