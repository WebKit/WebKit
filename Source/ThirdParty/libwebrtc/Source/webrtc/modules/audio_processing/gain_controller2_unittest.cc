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

void SetAudioBufferSamples(float value, AudioBuffer* ab) {
  // Sets all the samples in |ab| to |value|.
  for (size_t k = 0; k < ab->num_channels(); ++k) {
    std::fill(ab->channels()[k], ab->channels()[k] + ab->num_frames(), value);
  }
}

float RunAgc2WithConstantInput(GainController2* agc2,
                               float input_level,
                               size_t num_frames,
                               int sample_rate) {
  const int num_samples = rtc::CheckedDivExact(sample_rate, 100);
  AudioBuffer ab(sample_rate, 1, sample_rate, 1, sample_rate, 1);

  // Give time to the level estimator to converge.
  for (size_t i = 0; i < num_frames + 1; ++i) {
    SetAudioBufferSamples(input_level, &ab);
    agc2->Process(&ab);
  }

  // Return the last sample from the last processed frame.
  return ab.channels()[0][num_samples - 1];
}

AudioProcessing::Config::GainController2 CreateAgc2FixedDigitalModeConfig(
    float fixed_gain_db) {
  AudioProcessing::Config::GainController2 config;
  config.adaptive_digital.enabled = false;
  config.fixed_digital.gain_db = fixed_gain_db;
  // TODO(alessiob): Check why ASSERT_TRUE() below does not compile.
  EXPECT_TRUE(GainController2::Validate(config));
  return config;
}

std::unique_ptr<GainController2> CreateAgc2FixedDigitalMode(
    float fixed_gain_db,
    size_t sample_rate_hz) {
  auto agc2 = std::make_unique<GainController2>();
  agc2->ApplyConfig(CreateAgc2FixedDigitalModeConfig(fixed_gain_db));
  agc2->Initialize(sample_rate_hz, /*num_channels=*/1);
  return agc2;
}

float GainDbAfterProcessingFile(GainController2& gain_controller,
                                int max_duration_ms) {
  // Set up an AudioBuffer to be filled from the speech file.
  constexpr size_t kStereo = 2u;
  const StreamConfig capture_config(AudioProcessing::kSampleRate48kHz, kStereo,
                                    false);
  AudioBuffer ab(capture_config.sample_rate_hz(), capture_config.num_channels(),
                 capture_config.sample_rate_hz(), capture_config.num_channels(),
                 capture_config.sample_rate_hz(),
                 capture_config.num_channels());
  test::InputAudioFile capture_file(
      test::GetApmCaptureTestVectorFileName(AudioProcessing::kSampleRate48kHz));
  std::vector<float> capture_input(capture_config.num_frames() *
                                   capture_config.num_channels());

  // Process the input file which must be long enough to cover
  // `max_duration_ms`.
  RTC_DCHECK_GT(max_duration_ms, 0);
  const int num_frames = rtc::CheckedDivExact(max_duration_ms, 10);
  for (int i = 0; i < num_frames; ++i) {
    ReadFloatSamplesFromStereoFile(capture_config.num_frames(),
                                   capture_config.num_channels(), &capture_file,
                                   capture_input);
    test::CopyVectorToAudioBuffer(capture_config, capture_input, &ab);
    gain_controller.Process(&ab);
  }

  // Send in a last frame with minimum dBFS level.
  constexpr float sample_value = 1.f;
  SetAudioBufferSamples(sample_value, &ab);
  gain_controller.Process(&ab);
  // Measure the RMS level after processing.
  float rms = 0.0f;
  for (size_t i = 0; i < capture_config.num_frames(); ++i) {
    rms += ab.channels()[0][i] * ab.channels()[0][i];
  }
  // Return the applied gain in dB.
  return 20.0f * std::log10(std::sqrt(rms / capture_config.num_frames()));
}

}  // namespace

TEST(GainController2, CheckDefaultConfig) {
  AudioProcessing::Config::GainController2 config;
  EXPECT_TRUE(GainController2::Validate(config));
}

TEST(GainController2, CheckFixedDigitalConfig) {
  AudioProcessing::Config::GainController2 config;
  // Attenuation is not allowed.
  config.fixed_digital.gain_db = -5.f;
  EXPECT_FALSE(GainController2::Validate(config));
  // No gain is allowed.
  config.fixed_digital.gain_db = 0.f;
  EXPECT_TRUE(GainController2::Validate(config));
  // Positive gain is allowed.
  config.fixed_digital.gain_db = 15.f;
  EXPECT_TRUE(GainController2::Validate(config));
}

TEST(GainController2, CheckAdaptiveDigitalVadProbabilityAttackConfig) {
  AudioProcessing::Config::GainController2 config;
  // Reject invalid attack.
  config.adaptive_digital.vad_probability_attack = -123.f;
  EXPECT_FALSE(GainController2::Validate(config));
  config.adaptive_digital.vad_probability_attack = 0.f;
  EXPECT_FALSE(GainController2::Validate(config));
  config.adaptive_digital.vad_probability_attack = 42.f;
  EXPECT_FALSE(GainController2::Validate(config));
  // Accept valid attack.
  config.adaptive_digital.vad_probability_attack = 0.1f;
  EXPECT_TRUE(GainController2::Validate(config));
  config.adaptive_digital.vad_probability_attack = 1.f;
  EXPECT_TRUE(GainController2::Validate(config));
}

TEST(GainController2,
     CheckAdaptiveDigitalLevelEstimatorSpeechFramesThresholdConfig) {
  AudioProcessing::Config::GainController2 config;
  config.adaptive_digital.level_estimator_adjacent_speech_frames_threshold = 0;
  EXPECT_FALSE(GainController2::Validate(config));
  config.adaptive_digital.level_estimator_adjacent_speech_frames_threshold = 1;
  EXPECT_TRUE(GainController2::Validate(config));
  config.adaptive_digital.level_estimator_adjacent_speech_frames_threshold = 7;
  EXPECT_TRUE(GainController2::Validate(config));
}

TEST(GainController2, CheckAdaptiveDigitalInitialSaturationMarginConfig) {
  AudioProcessing::Config::GainController2 config;
  config.adaptive_digital.initial_saturation_margin_db = -1.f;
  EXPECT_FALSE(GainController2::Validate(config));
  config.adaptive_digital.initial_saturation_margin_db = 0.f;
  EXPECT_TRUE(GainController2::Validate(config));
  config.adaptive_digital.initial_saturation_margin_db = 50.f;
  EXPECT_TRUE(GainController2::Validate(config));
}

TEST(GainController2, CheckAdaptiveDigitalExtraSaturationMarginConfig) {
  AudioProcessing::Config::GainController2 config;
  config.adaptive_digital.extra_saturation_margin_db = -1.f;
  EXPECT_FALSE(GainController2::Validate(config));
  config.adaptive_digital.extra_saturation_margin_db = 0.f;
  EXPECT_TRUE(GainController2::Validate(config));
  config.adaptive_digital.extra_saturation_margin_db = 50.f;
  EXPECT_TRUE(GainController2::Validate(config));
}

TEST(GainController2,
     CheckAdaptiveDigitalGainApplierSpeechFramesThresholdConfig) {
  AudioProcessing::Config::GainController2 config;
  config.adaptive_digital.gain_applier_adjacent_speech_frames_threshold = 0;
  EXPECT_FALSE(GainController2::Validate(config));
  config.adaptive_digital.gain_applier_adjacent_speech_frames_threshold = 1;
  EXPECT_TRUE(GainController2::Validate(config));
  config.adaptive_digital.gain_applier_adjacent_speech_frames_threshold = 7;
  EXPECT_TRUE(GainController2::Validate(config));
}

TEST(GainController2, CheckAdaptiveDigitalMaxGainChangeSpeedConfig) {
  AudioProcessing::Config::GainController2 config;
  config.adaptive_digital.max_gain_change_db_per_second = -1.f;
  EXPECT_FALSE(GainController2::Validate(config));
  config.adaptive_digital.max_gain_change_db_per_second = 0.f;
  EXPECT_FALSE(GainController2::Validate(config));
  config.adaptive_digital.max_gain_change_db_per_second = 5.f;
  EXPECT_TRUE(GainController2::Validate(config));
}

TEST(GainController2, CheckAdaptiveDigitalMaxOutputNoiseLevelConfig) {
  AudioProcessing::Config::GainController2 config;
  config.adaptive_digital.max_output_noise_level_dbfs = 5.f;
  EXPECT_FALSE(GainController2::Validate(config));
  config.adaptive_digital.max_output_noise_level_dbfs = 0.f;
  EXPECT_TRUE(GainController2::Validate(config));
  config.adaptive_digital.max_output_noise_level_dbfs = -5.f;
  EXPECT_TRUE(GainController2::Validate(config));
}

// Checks that the default config is applied.
TEST(GainController2, ApplyDefaultConfig) {
  auto gain_controller2 = std::make_unique<GainController2>();
  AudioProcessing::Config::GainController2 config;
  gain_controller2->ApplyConfig(config);
}

TEST(GainController2FixedDigital, GainShouldChangeOnSetGain) {
  constexpr float kInputLevel = 1000.f;
  constexpr size_t kNumFrames = 5;
  constexpr size_t kSampleRateHz = 8000;
  constexpr float kGain0Db = 0.f;
  constexpr float kGain20Db = 20.f;

  auto agc2_fixed = CreateAgc2FixedDigitalMode(kGain0Db, kSampleRateHz);

  // Signal level is unchanged with 0 db gain.
  EXPECT_FLOAT_EQ(RunAgc2WithConstantInput(agc2_fixed.get(), kInputLevel,
                                           kNumFrames, kSampleRateHz),
                  kInputLevel);

  // +20 db should increase signal by a factor of 10.
  agc2_fixed->ApplyConfig(CreateAgc2FixedDigitalModeConfig(kGain20Db));
  EXPECT_FLOAT_EQ(RunAgc2WithConstantInput(agc2_fixed.get(), kInputLevel,
                                           kNumFrames, kSampleRateHz),
                  kInputLevel * 10);
}

TEST(GainController2FixedDigital, ChangeFixedGainShouldBeFastAndTimeInvariant) {
  // Number of frames required for the fixed gain controller to adapt on the
  // input signal when the gain changes.
  constexpr size_t kNumFrames = 5;

  constexpr float kInputLevel = 1000.f;
  constexpr size_t kSampleRateHz = 8000;
  constexpr float kGainDbLow = 0.f;
  constexpr float kGainDbHigh = 25.f;
  static_assert(kGainDbLow < kGainDbHigh, "");

  auto agc2_fixed = CreateAgc2FixedDigitalMode(kGainDbLow, kSampleRateHz);

  // Start with a lower gain.
  const float output_level_pre = RunAgc2WithConstantInput(
      agc2_fixed.get(), kInputLevel, kNumFrames, kSampleRateHz);

  // Increase gain.
  agc2_fixed->ApplyConfig(CreateAgc2FixedDigitalModeConfig(kGainDbHigh));
  static_cast<void>(RunAgc2WithConstantInput(agc2_fixed.get(), kInputLevel,
                                             kNumFrames, kSampleRateHz));

  // Back to the lower gain.
  agc2_fixed->ApplyConfig(CreateAgc2FixedDigitalModeConfig(kGainDbLow));
  const float output_level_post = RunAgc2WithConstantInput(
      agc2_fixed.get(), kInputLevel, kNumFrames, kSampleRateHz);

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
  const float kInputLevel = 32767.f;
  const size_t kNumFrames = 5;

  const auto params = GetParam();

  const auto gains_db =
      test::LinSpace(params.gain_db_min, params.gain_db_max, 10);
  for (const auto gain_db : gains_db) {
    SCOPED_TRACE(std::to_string(gain_db));
    auto agc2_fixed = CreateAgc2FixedDigitalMode(gain_db, params.sample_rate);
    const float processed_sample = RunAgc2WithConstantInput(
        agc2_fixed.get(), kInputLevel, kNumFrames, params.sample_rate);
    if (params.saturation_expected) {
      EXPECT_FLOAT_EQ(processed_sample, 32767.f);
    } else {
      EXPECT_LT(processed_sample, 32767.f);
    }
  }
}

static_assert(test::kLimiterMaxInputLevelDbFs < 10, "");
INSTANTIATE_TEST_SUITE_P(
    GainController2,
    FixedDigitalTest,
    ::testing::Values(
        // When gain < |test::kLimiterMaxInputLevelDbFs|, the limiter will not
        // saturate the signal (at any sample rate).
        FixedDigitalTestParams(0.1f,
                               test::kLimiterMaxInputLevelDbFs - 0.01f,
                               8000,
                               false),
        FixedDigitalTestParams(0.1,
                               test::kLimiterMaxInputLevelDbFs - 0.01f,
                               48000,
                               false),
        // When gain > |test::kLimiterMaxInputLevelDbFs|, the limiter will
        // saturate the signal (at any sample rate).
        FixedDigitalTestParams(test::kLimiterMaxInputLevelDbFs + 0.01f,
                               10.f,
                               8000,
                               true),
        FixedDigitalTestParams(test::kLimiterMaxInputLevelDbFs + 0.01f,
                               10.f,
                               48000,
                               true)));

// Checks that the gain applied at the end of a PCM samples file is close to the
// expected value.
TEST(GainController2, CheckGainAdaptiveDigital) {
  constexpr float kExpectedGainDb = 4.3f;
  constexpr float kToleranceDb = 0.5f;
  GainController2 gain_controller2;
  gain_controller2.Initialize(AudioProcessing::kSampleRate48kHz,
                              /*num_channels=*/1);
  AudioProcessing::Config::GainController2 config;
  config.fixed_digital.gain_db = 0.0f;
  config.adaptive_digital.enabled = true;
  gain_controller2.ApplyConfig(config);
  EXPECT_NEAR(
      GainDbAfterProcessingFile(gain_controller2, /*max_duration_ms=*/2000),
      kExpectedGainDb, kToleranceDb);
}

}  // namespace test
}  // namespace webrtc
