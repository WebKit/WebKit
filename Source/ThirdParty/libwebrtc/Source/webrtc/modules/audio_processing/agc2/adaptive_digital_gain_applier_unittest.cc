/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc2/adaptive_digital_gain_applier.h"

#include <algorithm>
#include <memory>

#include "common_audio/include/audio_util.h"
#include "modules/audio_processing/agc2/agc2_common.h"
#include "modules/audio_processing/agc2/vector_float_frame.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "modules/audio_processing/logging/apm_data_dumper.h"
#include "rtc_base/gunit.h"

namespace webrtc {
namespace {

constexpr int kMono = 1;
constexpr int kStereo = 2;
constexpr int kFrameLen10ms8kHz = 80;
constexpr int kFrameLen10ms48kHz = 480;

constexpr float kMaxSpeechProbability = 1.0f;

// Constants used in place of estimated noise levels.
constexpr float kNoNoiseDbfs = kMinLevelDbfs;
constexpr float kWithNoiseDbfs = -20.0f;

// Number of additional frames to process in the tests to ensure that the tested
// adaptation processes have converged.
constexpr int kNumExtraFrames = 10;

constexpr float GetMaxGainChangePerFrameDb(
    float max_gain_change_db_per_second) {
  return max_gain_change_db_per_second * kFrameDurationMs / 1000.0f;
}

using AdaptiveDigitalConfig =
    AudioProcessing::Config::GainController2::AdaptiveDigital;

constexpr AdaptiveDigitalConfig kDefaultConfig{};

// Helper to create initialized `AdaptiveDigitalGainApplier` objects.
struct GainApplierHelper {
  GainApplierHelper(const AdaptiveDigitalConfig& config,
                    int sample_rate_hz,
                    int num_channels)
      : apm_data_dumper(0),
        gain_applier(
            std::make_unique<AdaptiveDigitalGainApplier>(&apm_data_dumper,
                                                         config,
                                                         sample_rate_hz,
                                                         num_channels)) {}
  ApmDataDumper apm_data_dumper;
  std::unique_ptr<AdaptiveDigitalGainApplier> gain_applier;
};

// Returns a `FrameInfo` sample to simulate noiseless speech detected with
// maximum probability and with level, headroom and limiter envelope chosen
// so that the resulting gain equals the default initial adaptive digital gain
// i.e., no gain adaptation is expected.
AdaptiveDigitalGainApplier::FrameInfo GetFrameInfoToNotAdapt(
    const AdaptiveDigitalConfig& config) {
  AdaptiveDigitalGainApplier::FrameInfo info;
  info.speech_probability = kMaxSpeechProbability;
  info.speech_level_dbfs = -config.initial_gain_db - config.headroom_db;
  info.speech_level_reliable = true;
  info.noise_rms_dbfs = kNoNoiseDbfs;
  info.headroom_db = config.headroom_db;
  info.limiter_envelope_dbfs = -2.0f;
  return info;
}

TEST(GainController2AdaptiveGainApplier, GainApplierShouldNotCrash) {
  GainApplierHelper helper(kDefaultConfig, /*sample_rate_hz=*/48000, kStereo);
  // Make one call with reasonable audio level values and settings.
  VectorFloatFrame fake_audio(kStereo, kFrameLen10ms48kHz, 10000.0f);
  helper.gain_applier->Process(GetFrameInfoToNotAdapt(kDefaultConfig),
                               fake_audio.float_frame_view());
}

// Checks that the maximum allowed gain is applied.
TEST(GainController2AdaptiveGainApplier, MaxGainApplied) {
  constexpr int kNumFramesToAdapt =
      static_cast<int>(kDefaultConfig.max_gain_db /
                       GetMaxGainChangePerFrameDb(
                           kDefaultConfig.max_gain_change_db_per_second)) +
      kNumExtraFrames;

  GainApplierHelper helper(kDefaultConfig, /*sample_rate_hz=*/8000, kMono);
  AdaptiveDigitalGainApplier::FrameInfo info =
      GetFrameInfoToNotAdapt(kDefaultConfig);
  info.speech_level_dbfs = -60.0f;
  float applied_gain;
  for (int i = 0; i < kNumFramesToAdapt; ++i) {
    VectorFloatFrame fake_audio(kMono, kFrameLen10ms8kHz, 1.0f);
    helper.gain_applier->Process(info, fake_audio.float_frame_view());
    applied_gain = fake_audio.float_frame_view().channel(0)[0];
  }
  const float applied_gain_db = 20.0f * std::log10f(applied_gain);
  EXPECT_NEAR(applied_gain_db, kDefaultConfig.max_gain_db, 0.1f);
}

TEST(GainController2AdaptiveGainApplier, GainDoesNotChangeFast) {
  GainApplierHelper helper(kDefaultConfig, /*sample_rate_hz=*/8000, kMono);

  constexpr float initial_level_dbfs = -25.0f;
  constexpr float kMaxGainChangeDbPerFrame =
      GetMaxGainChangePerFrameDb(kDefaultConfig.max_gain_change_db_per_second);
  constexpr int kNumFramesToAdapt =
      static_cast<int>(initial_level_dbfs / kMaxGainChangeDbPerFrame) +
      kNumExtraFrames;

  const float max_change_per_frame_linear = DbToRatio(kMaxGainChangeDbPerFrame);

  float last_gain_linear = 1.f;
  for (int i = 0; i < kNumFramesToAdapt; ++i) {
    SCOPED_TRACE(i);
    VectorFloatFrame fake_audio(kMono, kFrameLen10ms8kHz, 1.0f);
    AdaptiveDigitalGainApplier::FrameInfo info =
        GetFrameInfoToNotAdapt(kDefaultConfig);
    info.speech_level_dbfs = initial_level_dbfs;
    helper.gain_applier->Process(info, fake_audio.float_frame_view());
    float current_gain_linear = fake_audio.float_frame_view().channel(0)[0];
    EXPECT_LE(std::abs(current_gain_linear - last_gain_linear),
              max_change_per_frame_linear);
    last_gain_linear = current_gain_linear;
  }

  // Check that the same is true when gain decreases as well.
  for (int i = 0; i < kNumFramesToAdapt; ++i) {
    SCOPED_TRACE(i);
    VectorFloatFrame fake_audio(kMono, kFrameLen10ms8kHz, 1.0f);
    AdaptiveDigitalGainApplier::FrameInfo info =
        GetFrameInfoToNotAdapt(kDefaultConfig);
    info.speech_level_dbfs = 0.f;
    helper.gain_applier->Process(info, fake_audio.float_frame_view());
    float current_gain_linear = fake_audio.float_frame_view().channel(0)[0];
    EXPECT_LE(std::abs(current_gain_linear - last_gain_linear),
              max_change_per_frame_linear);
    last_gain_linear = current_gain_linear;
  }
}

TEST(GainController2AdaptiveGainApplier, GainIsRampedInAFrame) {
  GainApplierHelper helper(kDefaultConfig, /*sample_rate_hz=*/48000, kMono);

  constexpr float initial_level_dbfs = -25.0f;

  VectorFloatFrame fake_audio(kMono, kFrameLen10ms48kHz, 1.0f);
  AdaptiveDigitalGainApplier::FrameInfo info =
      GetFrameInfoToNotAdapt(kDefaultConfig);
  info.speech_level_dbfs = initial_level_dbfs;
  helper.gain_applier->Process(info, fake_audio.float_frame_view());
  float maximal_difference = 0.0f;
  float current_value = 1.0f * DbToRatio(kDefaultConfig.initial_gain_db);
  for (const auto& x : fake_audio.float_frame_view().channel(0)) {
    const float difference = std::abs(x - current_value);
    maximal_difference = std::max(maximal_difference, difference);
    current_value = x;
  }

  const float max_change_per_frame_linear = DbToRatio(
      GetMaxGainChangePerFrameDb(kDefaultConfig.max_gain_change_db_per_second));
  const float max_change_per_sample =
      max_change_per_frame_linear / kFrameLen10ms48kHz;

  EXPECT_LE(maximal_difference, max_change_per_sample);
}

TEST(GainController2AdaptiveGainApplier, NoiseLimitsGain) {
  GainApplierHelper helper(kDefaultConfig, /*sample_rate_hz=*/48000, kMono);

  constexpr float initial_level_dbfs = -25.0f;
  constexpr int num_initial_frames =
      kDefaultConfig.initial_gain_db /
      GetMaxGainChangePerFrameDb(kDefaultConfig.max_gain_change_db_per_second);
  constexpr int num_frames = 50;

  ASSERT_GT(kWithNoiseDbfs, kDefaultConfig.max_output_noise_level_dbfs)
      << "kWithNoiseDbfs is too low";

  for (int i = 0; i < num_initial_frames + num_frames; ++i) {
    VectorFloatFrame fake_audio(kMono, kFrameLen10ms48kHz, 1.0f);
    AdaptiveDigitalGainApplier::FrameInfo info =
        GetFrameInfoToNotAdapt(kDefaultConfig);
    info.speech_level_dbfs = initial_level_dbfs;
    info.noise_rms_dbfs = kWithNoiseDbfs;
    helper.gain_applier->Process(info, fake_audio.float_frame_view());

    // Wait so that the adaptive gain applier has time to lower the gain.
    if (i > num_initial_frames) {
      const float maximal_ratio =
          *std::max_element(fake_audio.float_frame_view().channel(0).begin(),
                            fake_audio.float_frame_view().channel(0).end());

      EXPECT_NEAR(maximal_ratio, 1.0f, 0.001f);
    }
  }
}

TEST(GainController2GainApplier, CanHandlePositiveSpeechLevels) {
  GainApplierHelper helper(kDefaultConfig, /*sample_rate_hz=*/48000, kStereo);

  // Make one call with positive audio level values and settings.
  VectorFloatFrame fake_audio(kStereo, kFrameLen10ms48kHz, 10000.0f);
  AdaptiveDigitalGainApplier::FrameInfo info =
      GetFrameInfoToNotAdapt(kDefaultConfig);
  info.speech_level_dbfs = 5.0f;
  helper.gain_applier->Process(info, fake_audio.float_frame_view());
}

TEST(GainController2GainApplier, AudioLevelLimitsGain) {
  GainApplierHelper helper(kDefaultConfig, /*sample_rate_hz=*/48000, kMono);

  constexpr float initial_level_dbfs = -25.0f;
  constexpr int num_initial_frames =
      kDefaultConfig.initial_gain_db /
      GetMaxGainChangePerFrameDb(kDefaultConfig.max_gain_change_db_per_second);
  constexpr int num_frames = 50;

  ASSERT_GT(kWithNoiseDbfs, kDefaultConfig.max_output_noise_level_dbfs)
      << "kWithNoiseDbfs is too low";

  for (int i = 0; i < num_initial_frames + num_frames; ++i) {
    VectorFloatFrame fake_audio(kMono, kFrameLen10ms48kHz, 1.0f);
    AdaptiveDigitalGainApplier::FrameInfo info =
        GetFrameInfoToNotAdapt(kDefaultConfig);
    info.speech_level_dbfs = initial_level_dbfs;
    info.limiter_envelope_dbfs = 1.0f;
    info.speech_level_reliable = false;
    helper.gain_applier->Process(info, fake_audio.float_frame_view());

    // Wait so that the adaptive gain applier has time to lower the gain.
    if (i > num_initial_frames) {
      const float maximal_ratio =
          *std::max_element(fake_audio.float_frame_view().channel(0).begin(),
                            fake_audio.float_frame_view().channel(0).end());

      EXPECT_NEAR(maximal_ratio, 1.0f, 0.001f);
    }
  }
}

class AdaptiveDigitalGainApplierTest : public ::testing::TestWithParam<int> {
 protected:
  int adjacent_speech_frames_threshold() const { return GetParam(); }
};

TEST_P(AdaptiveDigitalGainApplierTest,
       DoNotIncreaseGainWithTooFewSpeechFrames) {
  AdaptiveDigitalConfig config;
  config.adjacent_speech_frames_threshold = adjacent_speech_frames_threshold();
  GainApplierHelper helper(config, /*sample_rate_hz=*/48000, kMono);

  // Lower the speech level so that the target gain will be increased.
  AdaptiveDigitalGainApplier::FrameInfo info = GetFrameInfoToNotAdapt(config);
  info.speech_level_dbfs -= 12.0f;

  float prev_gain = 0.0f;
  for (int i = 0; i < config.adjacent_speech_frames_threshold; ++i) {
    SCOPED_TRACE(i);
    VectorFloatFrame audio(kMono, kFrameLen10ms48kHz, 1.0f);
    helper.gain_applier->Process(info, audio.float_frame_view());
    const float gain = audio.float_frame_view().channel(0)[0];
    if (i > 0) {
      EXPECT_EQ(prev_gain, gain);  // No gain increase applied.
    }
    prev_gain = gain;
  }
}

TEST_P(AdaptiveDigitalGainApplierTest, IncreaseGainWithEnoughSpeechFrames) {
  AdaptiveDigitalConfig config;
  config.adjacent_speech_frames_threshold = adjacent_speech_frames_threshold();
  GainApplierHelper helper(config, /*sample_rate_hz=*/48000, kMono);

  // Lower the speech level so that the target gain will be increased.
  AdaptiveDigitalGainApplier::FrameInfo info = GetFrameInfoToNotAdapt(config);
  info.speech_level_dbfs -= 12.0f;

  float prev_gain = 0.0f;
  for (int i = 0; i < config.adjacent_speech_frames_threshold; ++i) {
    SCOPED_TRACE(i);
    VectorFloatFrame audio(kMono, kFrameLen10ms48kHz, 1.0f);
    helper.gain_applier->Process(info, audio.float_frame_view());
    prev_gain = audio.float_frame_view().channel(0)[0];
  }

  // Process one more speech frame.
  VectorFloatFrame audio(kMono, kFrameLen10ms48kHz, 1.0f);
  helper.gain_applier->Process(info, audio.float_frame_view());

  // An increased gain has been applied.
  EXPECT_GT(audio.float_frame_view().channel(0)[0], prev_gain);
}

INSTANTIATE_TEST_SUITE_P(GainController2,
                         AdaptiveDigitalGainApplierTest,
                         ::testing::Values(1, 7, 31));

// Checks that the input is never modified when running in dry run mode.
TEST(GainController2GainApplier, DryRunDoesNotChangeInput) {
  AdaptiveDigitalConfig config;
  config.dry_run = true;
  GainApplierHelper helper(config, /*sample_rate_hz=*/8000, kMono);

  // Simulate an input signal with log speech level.
  AdaptiveDigitalGainApplier::FrameInfo info = GetFrameInfoToNotAdapt(config);
  info.speech_level_dbfs = -60.0f;
  const int num_frames_to_adapt =
      static_cast<int>(
          config.max_gain_db /
          GetMaxGainChangePerFrameDb(config.max_gain_change_db_per_second)) +
      kNumExtraFrames;
  constexpr float kPcmSamples = 123.456f;
  // Run the gain applier and check that the PCM samples are not modified.
  for (int i = 0; i < num_frames_to_adapt; ++i) {
    SCOPED_TRACE(i);
    VectorFloatFrame fake_audio(kMono, kFrameLen10ms8kHz, kPcmSamples);
    helper.gain_applier->Process(info, fake_audio.float_frame_view());
    EXPECT_FLOAT_EQ(fake_audio.float_frame_view().channel(0)[0], kPcmSamples);
  }
}

// Checks that no sample is modified before and after the sample rate changes.
TEST(GainController2GainApplier, DryRunHandlesSampleRateChange) {
  AdaptiveDigitalConfig config;
  config.dry_run = true;
  GainApplierHelper helper(config, /*sample_rate_hz=*/8000, kMono);

  AdaptiveDigitalGainApplier::FrameInfo info = GetFrameInfoToNotAdapt(config);
  info.speech_level_dbfs = -60.0f;
  constexpr float kPcmSamples = 123.456f;
  VectorFloatFrame fake_audio_8k(kMono, kFrameLen10ms8kHz, kPcmSamples);
  helper.gain_applier->Process(info, fake_audio_8k.float_frame_view());
  EXPECT_FLOAT_EQ(fake_audio_8k.float_frame_view().channel(0)[0], kPcmSamples);
  helper.gain_applier->Initialize(/*sample_rate_hz=*/48000, kMono);
  VectorFloatFrame fake_audio_48k(kMono, kFrameLen10ms48kHz, kPcmSamples);
  helper.gain_applier->Process(info, fake_audio_48k.float_frame_view());
  EXPECT_FLOAT_EQ(fake_audio_48k.float_frame_view().channel(0)[0], kPcmSamples);
}

// Checks that no sample is modified before and after the number of channels
// changes.
TEST(GainController2GainApplier, DryRunHandlesNumChannelsChange) {
  AdaptiveDigitalConfig config;
  config.dry_run = true;
  GainApplierHelper helper(config, /*sample_rate_hz=*/8000, kMono);

  AdaptiveDigitalGainApplier::FrameInfo info = GetFrameInfoToNotAdapt(config);
  info.speech_level_dbfs = -60.0f;
  constexpr float kPcmSamples = 123.456f;
  VectorFloatFrame fake_audio_8k(kMono, kFrameLen10ms8kHz, kPcmSamples);
  helper.gain_applier->Process(info, fake_audio_8k.float_frame_view());
  EXPECT_FLOAT_EQ(fake_audio_8k.float_frame_view().channel(0)[0], kPcmSamples);
  VectorFloatFrame fake_audio_48k(kStereo, kFrameLen10ms8kHz, kPcmSamples);
  helper.gain_applier->Initialize(/*sample_rate_hz=*/8000, kStereo);
  helper.gain_applier->Process(info, fake_audio_48k.float_frame_view());
  EXPECT_FLOAT_EQ(fake_audio_48k.float_frame_view().channel(0)[0], kPcmSamples);
  EXPECT_FLOAT_EQ(fake_audio_48k.float_frame_view().channel(1)[0], kPcmSamples);
}

}  // namespace
}  // namespace webrtc
