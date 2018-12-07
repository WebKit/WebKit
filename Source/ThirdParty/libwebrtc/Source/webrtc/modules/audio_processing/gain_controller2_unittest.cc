/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <algorithm>

#include "absl/memory/memory.h"
#include "api/array_view.h"
#include "modules/audio_processing/agc2/agc2_testing_common.h"
#include "modules/audio_processing/audio_buffer.h"
#include "modules/audio_processing/gain_controller2.h"
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
    std::fill(ab->channels_f()[k], ab->channels_f()[k] + ab->num_frames(),
              value);
  }
}

float RunAgc2WithConstantInput(GainController2* agc2,
                               float input_level,
                               size_t num_frames,
                               int sample_rate) {
  const int num_samples = rtc::CheckedDivExact(sample_rate, 100);
  AudioBuffer ab(num_samples, 1, num_samples, 1, num_samples);

  // Give time to the level estimator to converge.
  for (size_t i = 0; i < num_frames + 1; ++i) {
    SetAudioBufferSamples(input_level, &ab);
    agc2->Process(&ab);
  }

  // Return the last sample from the last processed frame.
  return ab.channels_f()[0][num_samples - 1];
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
  auto agc2 = absl::make_unique<GainController2>();
  agc2->ApplyConfig(CreateAgc2FixedDigitalModeConfig(fixed_gain_db));
  agc2->Initialize(sample_rate_hz);
  return agc2;
}

float GainAfterProcessingFile(GainController2* gain_controller) {
  // Set up an AudioBuffer to be filled from the speech file.
  constexpr size_t kStereo = 2u;
  const StreamConfig capture_config(AudioProcessing::kSampleRate48kHz, kStereo,
                                    false);
  AudioBuffer ab(capture_config.num_frames(), capture_config.num_channels(),
                 capture_config.num_frames(), capture_config.num_channels(),
                 capture_config.num_frames());
  test::InputAudioFile capture_file(
      test::GetApmCaptureTestVectorFileName(AudioProcessing::kSampleRate48kHz));
  std::vector<float> capture_input(capture_config.num_frames() *
                                   capture_config.num_channels());

  // The file should contain at least this many frames. Every iteration, we put
  // a frame through the gain controller.
  const int kNumFramesToProcess = 100;
  for (int frame_no = 0; frame_no < kNumFramesToProcess; ++frame_no) {
    ReadFloatSamplesFromStereoFile(capture_config.num_frames(),
                                   capture_config.num_channels(), &capture_file,
                                   capture_input);

    test::CopyVectorToAudioBuffer(capture_config, capture_input, &ab);
    gain_controller->Process(&ab);
  }

  // Send in a last frame with values constant 1 (It's low enough to detect high
  // gain, and for ease of computation). The applied gain is the result.
  constexpr float sample_value = 1.f;
  SetAudioBufferSamples(sample_value, &ab);
  gain_controller->Process(&ab);
  return ab.channels_f()[0][0];
}

}  // namespace

TEST(GainController2, CreateApplyConfig) {
  // Instances GainController2 and applies different configurations.
  std::unique_ptr<GainController2> gain_controller2(new GainController2());

  // Check that the default config is valid.
  AudioProcessing::Config::GainController2 config;
  EXPECT_TRUE(GainController2::Validate(config));
  gain_controller2->ApplyConfig(config);

  // Check that attenuation is not allowed.
  config.fixed_digital.gain_db = -5.f;
  EXPECT_FALSE(GainController2::Validate(config));

  // Check that valid configurations are applied.
  for (const float& fixed_gain_db : {0.f, 5.f, 10.f, 40.f}) {
    config.fixed_digital.gain_db = fixed_gain_db;
    EXPECT_TRUE(GainController2::Validate(config));
    gain_controller2->ApplyConfig(config);
  }
}

TEST(GainController2, ToString) {
  // Tests GainController2::ToString(). Only test the enabled property.
  AudioProcessing::Config::GainController2 config;

  config.enabled = false;
  EXPECT_EQ("{enabled: false", GainController2::ToString(config).substr(0, 15));

  config.enabled = true;
  EXPECT_EQ("{enabled: true", GainController2::ToString(config).substr(0, 14));
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
    : public testing::Test,
      public testing::WithParamInterface<FixedDigitalTestParams> {};

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
INSTANTIATE_TEST_CASE_P(
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

TEST(GainController2, UsageSaturationMargin) {
  GainController2 gain_controller2;
  gain_controller2.Initialize(AudioProcessing::kSampleRate48kHz);

  AudioProcessing::Config::GainController2 config;
  // Check that samples are not amplified as much when extra margin is
  // high. They should not be amplified at all, but only after convergence. GC2
  // starts with a gain, and it takes time until it's down to 0 dB.
  config.fixed_digital.gain_db = 0.f;
  config.adaptive_digital.enabled = true;
  config.adaptive_digital.extra_saturation_margin_db = 50.f;
  gain_controller2.ApplyConfig(config);

  EXPECT_LT(GainAfterProcessingFile(&gain_controller2), 2.f);
}

TEST(GainController2, UsageNoSaturationMargin) {
  GainController2 gain_controller2;
  gain_controller2.Initialize(AudioProcessing::kSampleRate48kHz);

  AudioProcessing::Config::GainController2 config;
  // Check that some gain is applied if there is no margin.
  config.fixed_digital.gain_db = 0.f;
  config.adaptive_digital.enabled = true;
  config.adaptive_digital.extra_saturation_margin_db = 0.f;
  gain_controller2.ApplyConfig(config);

  EXPECT_GT(GainAfterProcessingFile(&gain_controller2), 2.f);
}

}  // namespace test
}  // namespace webrtc
