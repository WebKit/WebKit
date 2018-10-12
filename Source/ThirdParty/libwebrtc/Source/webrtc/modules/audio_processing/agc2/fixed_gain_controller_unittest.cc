/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc2/fixed_gain_controller.h"

#include "absl/memory/memory.h"
#include "api/array_view.h"
#include "modules/audio_processing/agc2/agc2_testing_common.h"
#include "modules/audio_processing/agc2/vector_float_frame.h"
#include "modules/audio_processing/logging/apm_data_dumper.h"
#include "rtc_base/gunit.h"
#include "system_wrappers/include/metrics.h"

namespace webrtc {
namespace {

constexpr float kInputLevelLinear = 15000.f;

constexpr float kGainToApplyDb = 15.f;

float RunFixedGainControllerWithConstantInput(FixedGainController* fixed_gc,
                                              const float input_level,
                                              const size_t num_frames,
                                              const int sample_rate) {
  // Give time to the level etimator to converge.
  for (size_t i = 0; i < num_frames; ++i) {
    VectorFloatFrame vectors_with_float_frame(
        1, rtc::CheckedDivExact(sample_rate, 100), input_level);
    fixed_gc->Process(vectors_with_float_frame.float_frame_view());
  }

  // Process the last frame with constant input level.
  VectorFloatFrame vectors_with_float_frame_last(
      1, rtc::CheckedDivExact(sample_rate, 100), input_level);
  fixed_gc->Process(vectors_with_float_frame_last.float_frame_view());

  // Return the last sample from the last processed frame.
  const auto channel =
      vectors_with_float_frame_last.float_frame_view().channel(0);
  return channel[channel.size() - 1];
}

std::unique_ptr<ApmDataDumper> GetApmDataDumper() {
  return absl::make_unique<ApmDataDumper>(0);
}

std::unique_ptr<FixedGainController> CreateFixedGainController(
    float gain_to_apply,
    size_t rate,
    std::string histogram_name_prefix,
    ApmDataDumper* test_data_dumper) {
  std::unique_ptr<FixedGainController> fgc =
      absl::make_unique<FixedGainController>(test_data_dumper,
                                             histogram_name_prefix);
  fgc->SetGain(gain_to_apply);
  fgc->SetSampleRate(rate);
  return fgc;
}

std::unique_ptr<FixedGainController> CreateFixedGainController(
    float gain_to_apply,
    size_t rate,
    ApmDataDumper* test_data_dumper) {
  return CreateFixedGainController(gain_to_apply, rate, "", test_data_dumper);
}

}  // namespace

TEST(AutomaticGainController2FixedDigital, CreateUse) {
  const int kSampleRate = 44000;
  auto test_data_dumper = GetApmDataDumper();
  std::unique_ptr<FixedGainController> fixed_gc = CreateFixedGainController(
      kGainToApplyDb, kSampleRate, test_data_dumper.get());
  VectorFloatFrame vectors_with_float_frame(
      1, rtc::CheckedDivExact(kSampleRate, 100), kInputLevelLinear);
  auto float_frame = vectors_with_float_frame.float_frame_view();
  fixed_gc->Process(float_frame);
  const auto channel = float_frame.channel(0);
  EXPECT_LT(kInputLevelLinear, channel[0]);
}

TEST(AutomaticGainController2FixedDigital, CheckSaturationBehaviorWithLimiter) {
  const float kInputLevel = 32767.f;
  const size_t kNumFrames = 5;
  const size_t kSampleRate = 42000;

  auto test_data_dumper = GetApmDataDumper();

  const auto gains_no_saturation =
      test::LinSpace(0.1, test::kLimiterMaxInputLevelDbFs - 0.01, 10);
  for (const auto gain_db : gains_no_saturation) {
    // Since |test::kLimiterMaxInputLevelDbFs| > |gain_db|, the
    // limiter will not saturate the signal.
    std::unique_ptr<FixedGainController> fixed_gc_no_saturation =
        CreateFixedGainController(gain_db, kSampleRate, test_data_dumper.get());

    // Saturation not expected.
    SCOPED_TRACE(std::to_string(gain_db));
    EXPECT_LT(
        RunFixedGainControllerWithConstantInput(
            fixed_gc_no_saturation.get(), kInputLevel, kNumFrames, kSampleRate),
        32767.f);
  }

  const auto gains_saturation =
      test::LinSpace(test::kLimiterMaxInputLevelDbFs + 0.01, 10, 10);
  for (const auto gain_db : gains_saturation) {
    // Since |test::kLimiterMaxInputLevelDbFs| < |gain|, the limiter
    // will saturate the signal.
    std::unique_ptr<FixedGainController> fixed_gc_saturation =
        CreateFixedGainController(gain_db, kSampleRate, test_data_dumper.get());

    // Saturation expected.
    SCOPED_TRACE(std::to_string(gain_db));
    EXPECT_FLOAT_EQ(
        RunFixedGainControllerWithConstantInput(
            fixed_gc_saturation.get(), kInputLevel, kNumFrames, kSampleRate),
        32767.f);
  }
}

TEST(AutomaticGainController2FixedDigital,
     CheckSaturationBehaviorWithLimiterSingleSample) {
  const float kInputLevel = 32767.f;
  const size_t kNumFrames = 5;
  const size_t kSampleRate = 8000;

  auto test_data_dumper = GetApmDataDumper();

  const auto gains_no_saturation =
      test::LinSpace(0.1, test::kLimiterMaxInputLevelDbFs - 0.01, 10);
  for (const auto gain_db : gains_no_saturation) {
    // Since |gain| > |test::kLimiterMaxInputLevelDbFs|, the limiter will
    // not saturate the signal.
    std::unique_ptr<FixedGainController> fixed_gc_no_saturation =
        CreateFixedGainController(gain_db, kSampleRate, test_data_dumper.get());

    // Saturation not expected.
    SCOPED_TRACE(std::to_string(gain_db));
    EXPECT_LT(
        RunFixedGainControllerWithConstantInput(
            fixed_gc_no_saturation.get(), kInputLevel, kNumFrames, kSampleRate),
        32767.f);
  }

  const auto gains_saturation =
      test::LinSpace(test::kLimiterMaxInputLevelDbFs + 0.01, 10, 10);
  for (const auto gain_db : gains_saturation) {
    // Singe |gain| < |test::kLimiterMaxInputLevelDbFs|, the limiter will
    // saturate the signal.
    std::unique_ptr<FixedGainController> fixed_gc_saturation =
        CreateFixedGainController(gain_db, kSampleRate, test_data_dumper.get());

    // Saturation expected.
    SCOPED_TRACE(std::to_string(gain_db));
    EXPECT_FLOAT_EQ(
        RunFixedGainControllerWithConstantInput(
            fixed_gc_saturation.get(), kInputLevel, kNumFrames, kSampleRate),
        32767.f);
  }
}

TEST(AutomaticGainController2FixedDigital, GainShouldChangeOnSetGain) {
  constexpr float kInputLevel = 1000.f;
  constexpr size_t kNumFrames = 5;
  constexpr size_t kSampleRate = 8000;
  constexpr float kGainDbNoChange = 0.f;
  constexpr float kGainDbFactor10 = 20.f;

  auto test_data_dumper = GetApmDataDumper();
  std::unique_ptr<FixedGainController> fixed_gc_no_saturation =
      CreateFixedGainController(kGainDbNoChange, kSampleRate,
                                test_data_dumper.get());

  // Signal level is unchanged with 0 db gain.
  EXPECT_FLOAT_EQ(
      RunFixedGainControllerWithConstantInput(
          fixed_gc_no_saturation.get(), kInputLevel, kNumFrames, kSampleRate),
      kInputLevel);

  fixed_gc_no_saturation->SetGain(kGainDbFactor10);

  // +20db should increase signal by a factor of 10.
  EXPECT_FLOAT_EQ(
      RunFixedGainControllerWithConstantInput(
          fixed_gc_no_saturation.get(), kInputLevel, kNumFrames, kSampleRate),
      kInputLevel * 10);
}

TEST(AutomaticGainController2FixedDigital,
     SetGainShouldBeFastAndTimeInvariant) {
  // Number of frames required for the fixed gain controller to adapt on the
  // input signal when the gain changes.
  constexpr size_t kNumFrames = 5;

  constexpr float kInputLevel = 1000.f;
  constexpr size_t kSampleRate = 8000;
  constexpr float kGainDbLow = 0.f;
  constexpr float kGainDbHigh = 40.f;
  static_assert(kGainDbLow < kGainDbHigh, "");

  auto test_data_dumper = GetApmDataDumper();
  std::unique_ptr<FixedGainController> fixed_gc = CreateFixedGainController(
      kGainDbLow, kSampleRate, test_data_dumper.get());

  fixed_gc->SetGain(kGainDbLow);
  const float output_level_pre = RunFixedGainControllerWithConstantInput(
      fixed_gc.get(), kInputLevel, kNumFrames, kSampleRate);

  fixed_gc->SetGain(kGainDbHigh);
  RunFixedGainControllerWithConstantInput(fixed_gc.get(), kInputLevel,
                                          kNumFrames, kSampleRate);

  fixed_gc->SetGain(kGainDbLow);
  const float output_level_post = RunFixedGainControllerWithConstantInput(
      fixed_gc.get(), kInputLevel, kNumFrames, kSampleRate);

  EXPECT_EQ(output_level_pre, output_level_post);
}

TEST(AutomaticGainController2FixedDigital, RegionHistogramIsUpdated) {
  constexpr size_t kSampleRate = 8000;
  constexpr float kGainDb = 0.f;
  constexpr float kInputLevel = 1000.f;
  constexpr size_t kNumFrames = 5;

  metrics::Reset();

  auto test_data_dumper = GetApmDataDumper();
  std::unique_ptr<FixedGainController> fixed_gc_no_saturation =
      CreateFixedGainController(kGainDb, kSampleRate, "Test",
                                test_data_dumper.get());

  static_cast<void>(RunFixedGainControllerWithConstantInput(
      fixed_gc_no_saturation.get(), kInputLevel, kNumFrames, kSampleRate));

  // Destroying FixedGainController should cause the last limiter region to be
  // logged.
  fixed_gc_no_saturation.reset();

  EXPECT_EQ(1, metrics::NumSamples(
                   "WebRTC.Audio.Test.FixedDigitalGainCurveRegion.Identity"));
  EXPECT_EQ(0, metrics::NumSamples(
                   "WebRTC.Audio.Test.FixedDigitalGainCurveRegion.Knee"));
  EXPECT_EQ(0, metrics::NumSamples(
                   "WebRTC.Audio.Test.FixedDigitalGainCurveRegion.Limiter"));
  EXPECT_EQ(0, metrics::NumSamples(
                   "WebRTC.Audio.Test.FixedDigitalGainCurveRegion.Saturation"));
}

}  // namespace webrtc
