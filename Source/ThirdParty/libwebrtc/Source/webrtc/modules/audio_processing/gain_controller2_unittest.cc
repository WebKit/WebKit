/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/gain_controller2.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <numeric>

#include "api/array_view.h"
#include "modules/audio_processing/agc2/agc2_testing_common.h"
#include "modules/audio_processing/audio_buffer.h"
#include "modules/audio_processing/test/audio_buffer_tools.h"
#include "modules/audio_processing/test/bitexactness_tools.h"
#include "rtc_base/checks.h"
#include "test/gtest.h"

namespace webrtc {
namespace test {
namespace {

// Sets all the samples in `ab` to `value`.
void SetAudioBufferSamples(float value, AudioBuffer& ab) {
  for (size_t k = 0; k < ab.num_channels(); ++k) {
    std::fill(ab.channels()[k], ab.channels()[k] + ab.num_frames(), value);
  }
}

float RunAgc2WithConstantInput(GainController2& agc2,
                               float input_level,
                               int num_frames,
                               int sample_rate_hz) {
  const int num_samples = rtc::CheckedDivExact(sample_rate_hz, 100);
  AudioBuffer ab(sample_rate_hz, 1, sample_rate_hz, 1, sample_rate_hz, 1);

  // Give time to the level estimator to converge.
  for (int i = 0; i < num_frames + 1; ++i) {
    SetAudioBufferSamples(input_level, ab);
    agc2.Process(&ab);
  }

  // Return the last sample from the last processed frame.
  return ab.channels()[0][num_samples - 1];
}

AudioProcessing::Config::GainController2 CreateAgc2FixedDigitalModeConfig(
    float fixed_gain_db) {
  AudioProcessing::Config::GainController2 config;
  config.adaptive_digital.enabled = false;
  config.fixed_digital.gain_db = fixed_gain_db;
  EXPECT_TRUE(GainController2::Validate(config));
  return config;
}

std::unique_ptr<GainController2> CreateAgc2FixedDigitalMode(
    float fixed_gain_db,
    int sample_rate_hz) {
  auto agc2 = std::make_unique<GainController2>();
  agc2->ApplyConfig(CreateAgc2FixedDigitalModeConfig(fixed_gain_db));
  agc2->Initialize(sample_rate_hz, /*num_channels=*/1);
  return agc2;
}

}  // namespace

TEST(GainController2, CheckDefaultConfig) {
  AudioProcessing::Config::GainController2 config;
  EXPECT_TRUE(GainController2::Validate(config));
}

TEST(GainController2, CheckFixedDigitalConfig) {
  AudioProcessing::Config::GainController2 config;
  // Attenuation is not allowed.
  config.fixed_digital.gain_db = -5.0f;
  EXPECT_FALSE(GainController2::Validate(config));
  // No gain is allowed.
  config.fixed_digital.gain_db = 0.0f;
  EXPECT_TRUE(GainController2::Validate(config));
  // Positive gain is allowed.
  config.fixed_digital.gain_db = 15.0f;
  EXPECT_TRUE(GainController2::Validate(config));
}

TEST(GainController2, CheckHeadroomDb) {
  AudioProcessing::Config::GainController2 config;
  config.adaptive_digital.headroom_db = -1.0f;
  EXPECT_FALSE(GainController2::Validate(config));
  config.adaptive_digital.headroom_db = 0.0f;
  EXPECT_TRUE(GainController2::Validate(config));
  config.adaptive_digital.headroom_db = 5.0f;
  EXPECT_TRUE(GainController2::Validate(config));
}

TEST(GainController2, CheckMaxGainDb) {
  AudioProcessing::Config::GainController2 config;
  config.adaptive_digital.max_gain_db = -1.0f;
  EXPECT_FALSE(GainController2::Validate(config));
  config.adaptive_digital.max_gain_db = 0.0f;
  EXPECT_FALSE(GainController2::Validate(config));
  config.adaptive_digital.max_gain_db = 5.0f;
  EXPECT_TRUE(GainController2::Validate(config));
}

TEST(GainController2, CheckInitialGainDb) {
  AudioProcessing::Config::GainController2 config;
  config.adaptive_digital.initial_gain_db = -1.0f;
  EXPECT_FALSE(GainController2::Validate(config));
  config.adaptive_digital.initial_gain_db = 0.0f;
  EXPECT_TRUE(GainController2::Validate(config));
  config.adaptive_digital.initial_gain_db = 5.0f;
  EXPECT_TRUE(GainController2::Validate(config));
}

TEST(GainController2, CheckAdaptiveDigitalMaxGainChangeSpeedConfig) {
  AudioProcessing::Config::GainController2 config;
  config.adaptive_digital.max_gain_change_db_per_second = -1.0f;
  EXPECT_FALSE(GainController2::Validate(config));
  config.adaptive_digital.max_gain_change_db_per_second = 0.0f;
  EXPECT_FALSE(GainController2::Validate(config));
  config.adaptive_digital.max_gain_change_db_per_second = 5.0f;
  EXPECT_TRUE(GainController2::Validate(config));
}

TEST(GainController2, CheckAdaptiveDigitalMaxOutputNoiseLevelConfig) {
  AudioProcessing::Config::GainController2 config;
  config.adaptive_digital.max_output_noise_level_dbfs = 5.0f;
  EXPECT_FALSE(GainController2::Validate(config));
  config.adaptive_digital.max_output_noise_level_dbfs = 0.0f;
  EXPECT_TRUE(GainController2::Validate(config));
  config.adaptive_digital.max_output_noise_level_dbfs = -5.0f;
  EXPECT_TRUE(GainController2::Validate(config));
}

// Checks that the default config is applied.
TEST(GainController2, ApplyDefaultConfig) {
  auto gain_controller2 = std::make_unique<GainController2>();
  AudioProcessing::Config::GainController2 config;
  gain_controller2->ApplyConfig(config);
}

TEST(GainController2FixedDigital, GainShouldChangeOnSetGain) {
  constexpr float kInputLevel = 1000.0f;
  constexpr size_t kNumFrames = 5;
  constexpr size_t kSampleRateHz = 8000;
  constexpr float kGain0Db = 0.0f;
  constexpr float kGain20Db = 20.0f;

  auto agc2_fixed = CreateAgc2FixedDigitalMode(kGain0Db, kSampleRateHz);

  // Signal level is unchanged with 0 db gain.
  EXPECT_FLOAT_EQ(RunAgc2WithConstantInput(*agc2_fixed, kInputLevel, kNumFrames,
                                           kSampleRateHz),
                  kInputLevel);

  // +20 db should increase signal by a factor of 10.
  agc2_fixed->ApplyConfig(CreateAgc2FixedDigitalModeConfig(kGain20Db));
  EXPECT_FLOAT_EQ(RunAgc2WithConstantInput(*agc2_fixed, kInputLevel, kNumFrames,
                                           kSampleRateHz),
                  kInputLevel * 10);
}

TEST(GainController2FixedDigital, ChangeFixedGainShouldBeFastAndTimeInvariant) {
  // Number of frames required for the fixed gain controller to adapt on the
  // input signal when the gain changes.
  constexpr size_t kNumFrames = 5;

  constexpr float kInputLevel = 1000.0f;
  constexpr size_t kSampleRateHz = 8000;
  constexpr float kGainDbLow = 0.0f;
  constexpr float kGainDbHigh = 25.0f;
  static_assert(kGainDbLow < kGainDbHigh, "");

  auto agc2_fixed = CreateAgc2FixedDigitalMode(kGainDbLow, kSampleRateHz);

  // Start with a lower gain.
  const float output_level_pre = RunAgc2WithConstantInput(
      *agc2_fixed, kInputLevel, kNumFrames, kSampleRateHz);

  // Increase gain.
  agc2_fixed->ApplyConfig(CreateAgc2FixedDigitalModeConfig(kGainDbHigh));
  static_cast<void>(RunAgc2WithConstantInput(*agc2_fixed, kInputLevel,
                                             kNumFrames, kSampleRateHz));

  // Back to the lower gain.
  agc2_fixed->ApplyConfig(CreateAgc2FixedDigitalModeConfig(kGainDbLow));
  const float output_level_post = RunAgc2WithConstantInput(
      *agc2_fixed, kInputLevel, kNumFrames, kSampleRateHz);

  EXPECT_EQ(output_level_pre, output_level_post);
}

struct FixedDigitalTestParams {
  FixedDigitalTestParams(float gain_db_min,
                         float gain_db_max,
                         size_t sample_rate,
                         bool saturation_expected)
      : gain_db_min(gain_db_min),
        gain_db_max(gain_db_max),
        sample_rate(sample_rate),
        saturation_expected(saturation_expected) {}
  float gain_db_min;
  float gain_db_max;
  size_t sample_rate;
  bool saturation_expected;
};

class FixedDigitalTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<FixedDigitalTestParams> {};

TEST_P(FixedDigitalTest, CheckSaturationBehaviorWithLimiter) {
  const float kInputLevel = 32767.0f;
  const size_t kNumFrames = 5;

  const auto params = GetParam();

  const auto gains_db =
      test::LinSpace(params.gain_db_min, params.gain_db_max, 10);
  for (const auto gain_db : gains_db) {
    SCOPED_TRACE(std::to_string(gain_db));
    auto agc2_fixed = CreateAgc2FixedDigitalMode(gain_db, params.sample_rate);
    const float processed_sample = RunAgc2WithConstantInput(
        *agc2_fixed, kInputLevel, kNumFrames, params.sample_rate);
    if (params.saturation_expected) {
      EXPECT_FLOAT_EQ(processed_sample, 32767.0f);
    } else {
      EXPECT_LT(processed_sample, 32767.0f);
    }
  }
}

static_assert(test::kLimiterMaxInputLevelDbFs < 10, "");
INSTANTIATE_TEST_SUITE_P(
    GainController2,
    FixedDigitalTest,
    ::testing::Values(
        // When gain < `test::kLimiterMaxInputLevelDbFs`, the limiter will not
        // saturate the signal (at any sample rate).
        FixedDigitalTestParams(0.1f,
                               test::kLimiterMaxInputLevelDbFs - 0.01f,
                               8000,
                               false),
        FixedDigitalTestParams(0.1,
                               test::kLimiterMaxInputLevelDbFs - 0.01f,
                               48000,
                               false),
        // When gain > `test::kLimiterMaxInputLevelDbFs`, the limiter will
        // saturate the signal (at any sample rate).
        FixedDigitalTestParams(test::kLimiterMaxInputLevelDbFs + 0.01f,
                               10.0f,
                               8000,
                               true),
        FixedDigitalTestParams(test::kLimiterMaxInputLevelDbFs + 0.01f,
                               10.0f,
                               48000,
                               true)));

// Processes a test audio file and checks that the gain applied at the end of
// the recording is close to the expected value.
TEST(GainController2, CheckFinalGainWithAdaptiveDigitalController) {
  // Create AGC2 enabling only the adaptive digital controller.
  GainController2 agc2;
  AudioProcessing::Config::GainController2 config;
  config.fixed_digital.gain_db = 0.0f;
  config.adaptive_digital.enabled = true;
  agc2.ApplyConfig(config);

  // The input audio is a 48k stereo recording.
  constexpr int kSampleRateHz = AudioProcessing::kSampleRate48kHz;
  constexpr int kStereo = 2;
  test::InputAudioFile input_file(
      test::GetApmCaptureTestVectorFileName(kSampleRateHz),
      /*loop_at_end=*/true);
  const StreamConfig stream_config(kSampleRateHz, kStereo,
                                   /*has_keyboard=*/false);

  // Initialize AGC2 for the used input.
  agc2.Initialize(kSampleRateHz, kStereo);

  // Init buffers.
  constexpr int kFrameDurationMs = 10;
  std::vector<float> frame(kStereo * stream_config.num_frames());
  AudioBuffer audio_buffer(kSampleRateHz, kStereo, kSampleRateHz, kStereo,
                           kSampleRateHz, kStereo);

  // Simulate.
  constexpr float kGainDb = -6.0f;
  const float gain = std::pow(10.0f, kGainDb / 20.0f);
  constexpr int kDurationMs = 10000;
  constexpr int kNumFramesToProcess = kDurationMs / kFrameDurationMs;
  for (int i = 0; i < kNumFramesToProcess; ++i) {
    ReadFloatSamplesFromStereoFile(stream_config.num_frames(),
                                   stream_config.num_channels(), &input_file,
                                   frame);
    // Apply a fixed gain to the input audio.
    for (float& x : frame)
      x *= gain;
    test::CopyVectorToAudioBuffer(stream_config, frame, &audio_buffer);
    // Process.
    agc2.Process(&audio_buffer);
  }

  // Estimate the applied gain by processing a probing frame.
  SetAudioBufferSamples(/*value=*/1.0f, audio_buffer);
  agc2.Process(&audio_buffer);
  const float applied_gain_db =
      20.0f * std::log10(audio_buffer.channels_const()[0][0]);

  constexpr float kExpectedGainDb = 5.6f;
  constexpr float kToleranceDb = 0.3f;
  EXPECT_NEAR(applied_gain_db, kExpectedGainDb, kToleranceDb);
}

}  // namespace test
}  // namespace webrtc
