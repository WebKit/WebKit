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
constexpr float kWithNoiseDbfs = -20.f;

constexpr float kMaxGainChangePerSecondDb = 3.0f;
constexpr float kMaxGainChangePerFrameDb =
    kMaxGainChangePerSecondDb * kFrameDurationMs / 1000.0f;
constexpr float kMaxOutputNoiseLevelDbfs = -50.0f;

// Helper to create initialized `AdaptiveDigitalGainApplier` objects.
struct GainApplierHelper {
  GainApplierHelper()
      : GainApplierHelper(/*adjacent_speech_frames_threshold=*/1) {}
  explicit GainApplierHelper(int adjacent_speech_frames_threshold)
      : apm_data_dumper(0),
        gain_applier(std::make_unique<AdaptiveDigitalGainApplier>(
            &apm_data_dumper,
            adjacent_speech_frames_threshold,
            kMaxGainChangePerSecondDb,
            kMaxOutputNoiseLevelDbfs,
            /*dry_run=*/false)) {}
  ApmDataDumper apm_data_dumper;
  std::unique_ptr<AdaptiveDigitalGainApplier> gain_applier;
};

// Voice on, no noise, low limiter, confident level.
static_assert(std::is_trivially_destructible<
                  AdaptiveDigitalGainApplier::FrameInfo>::value,
              "");
constexpr AdaptiveDigitalGainApplier::FrameInfo kFrameInfo{
    /*speech_probability=*/kMaxSpeechProbability,
    /*speech_level_dbfs=*/kInitialSpeechLevelEstimateDbfs,
    /*speech_level_reliable=*/true,
    /*noise_rms_dbfs=*/kNoNoiseDbfs,
    /*headroom_db=*/kSaturationProtectorInitialHeadroomDb,
    /*limiter_envelope_dbfs=*/-2.0f};

TEST(GainController2AdaptiveGainApplier, GainApplierShouldNotCrash) {
  GainApplierHelper helper;
  helper.gain_applier->Initialize(/*sample_rate_hz=*/48000, kStereo);
  // Make one call with reasonable audio level values and settings.
  VectorFloatFrame fake_audio(kStereo, kFrameLen10ms48kHz, 10000.0f);
  AdaptiveDigitalGainApplier::FrameInfo info = kFrameInfo;
  info.speech_level_dbfs = -5.0f;
  helper.gain_applier->Process(kFrameInfo, fake_audio.float_frame_view());
}

// Checks that the maximum allowed gain is applied.
TEST(GainController2AdaptiveGainApplier, MaxGainApplied) {
  constexpr int kNumFramesToAdapt =
      static_cast<int>(kMaxGainDb / kMaxGainChangePerFrameDb) + 10;

  GainApplierHelper helper;
  helper.gain_applier->Initialize(/*sample_rate_hz=*/8000, kMono);
  AdaptiveDigitalGainApplier::FrameInfo info = kFrameInfo;
  info.speech_level_dbfs = -60.0f;
  float applied_gain;
  for (int i = 0; i < kNumFramesToAdapt; ++i) {
    VectorFloatFrame fake_audio(kMono, kFrameLen10ms8kHz, 1.0f);
    helper.gain_applier->Process(info, fake_audio.float_frame_view());
    applied_gain = fake_audio.float_frame_view().channel(0)[0];
  }
  const float applied_gain_db = 20.0f * std::log10f(applied_gain);
  EXPECT_NEAR(applied_gain_db, kMaxGainDb, 0.1f);
}

TEST(GainController2AdaptiveGainApplier, GainDoesNotChangeFast) {
  GainApplierHelper helper;
  helper.gain_applier->Initialize(/*sample_rate_hz=*/8000, kMono);

  constexpr float initial_level_dbfs = -25.0f;
  // A few extra frames for safety.
  constexpr int kNumFramesToAdapt =
      static_cast<int>(initial_level_dbfs / kMaxGainChangePerFrameDb) + 10;

  const float kMaxChangePerFrameLinear = DbToRatio(kMaxGainChangePerFrameDb);

  float last_gain_linear = 1.f;
  for (int i = 0; i < kNumFramesToAdapt; ++i) {
    SCOPED_TRACE(i);
    VectorFloatFrame fake_audio(kMono, kFrameLen10ms8kHz, 1.0f);
    AdaptiveDigitalGainApplier::FrameInfo info = kFrameInfo;
    info.speech_level_dbfs = initial_level_dbfs;
    helper.gain_applier->Process(info, fake_audio.float_frame_view());
    float current_gain_linear = fake_audio.float_frame_view().channel(0)[0];
    EXPECT_LE(std::abs(current_gain_linear - last_gain_linear),
              kMaxChangePerFrameLinear);
    last_gain_linear = current_gain_linear;
  }

  // Check that the same is true when gain decreases as well.
  for (int i = 0; i < kNumFramesToAdapt; ++i) {
    SCOPED_TRACE(i);
    VectorFloatFrame fake_audio(kMono, kFrameLen10ms8kHz, 1.0f);
    AdaptiveDigitalGainApplier::FrameInfo info = kFrameInfo;
    info.speech_level_dbfs = 0.f;
    helper.gain_applier->Process(info, fake_audio.float_frame_view());
    float current_gain_linear = fake_audio.float_frame_view().channel(0)[0];
    EXPECT_LE(std::abs(current_gain_linear - last_gain_linear),
              kMaxChangePerFrameLinear);
    last_gain_linear = current_gain_linear;
  }
}

TEST(GainController2AdaptiveGainApplier, GainIsRampedInAFrame) {
  GainApplierHelper helper;
  helper.gain_applier->Initialize(/*sample_rate_hz=*/48000, kMono);

  constexpr float initial_level_dbfs = -25.0f;

  VectorFloatFrame fake_audio(kMono, kFrameLen10ms48kHz, 1.0f);
  AdaptiveDigitalGainApplier::FrameInfo info = kFrameInfo;
  info.speech_level_dbfs = initial_level_dbfs;
  helper.gain_applier->Process(info, fake_audio.float_frame_view());
  float maximal_difference = 0.0f;
  float current_value = 1.0f * DbToRatio(kInitialAdaptiveDigitalGainDb);
  for (const auto& x : fake_audio.float_frame_view().channel(0)) {
    const float difference = std::abs(x - current_value);
    maximal_difference = std::max(maximal_difference, difference);
    current_value = x;
  }

  const float kMaxChangePerFrameLinear = DbToRatio(kMaxGainChangePerFrameDb);
  const float kMaxChangePerSample =
      kMaxChangePerFrameLinear / kFrameLen10ms48kHz;

  EXPECT_LE(maximal_difference, kMaxChangePerSample);
}

TEST(GainController2AdaptiveGainApplier, NoiseLimitsGain) {
  GainApplierHelper helper;
  helper.gain_applier->Initialize(/*sample_rate_hz=*/48000, kMono);

  constexpr float initial_level_dbfs = -25.0f;
  constexpr int num_initial_frames =
      kInitialAdaptiveDigitalGainDb / kMaxGainChangePerFrameDb;
  constexpr int num_frames = 50;

  ASSERT_GT(kWithNoiseDbfs, kMaxOutputNoiseLevelDbfs)
      << "kWithNoiseDbfs is too low";

  for (int i = 0; i < num_initial_frames + num_frames; ++i) {
    VectorFloatFrame fake_audio(kMono, kFrameLen10ms48kHz, 1.0f);
    AdaptiveDigitalGainApplier::FrameInfo info = kFrameInfo;
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
  GainApplierHelper helper;
  helper.gain_applier->Initialize(/*sample_rate_hz=*/48000, kStereo);

  // Make one call with positive audio level values and settings.
  VectorFloatFrame fake_audio(kStereo, kFrameLen10ms48kHz, 10000.0f);
  AdaptiveDigitalGainApplier::FrameInfo info = kFrameInfo;
  info.speech_level_dbfs = 5.0f;
  helper.gain_applier->Process(info, fake_audio.float_frame_view());
}

TEST(GainController2GainApplier, AudioLevelLimitsGain) {
  GainApplierHelper helper;
  helper.gain_applier->Initialize(/*sample_rate_hz=*/48000, kMono);

  constexpr float initial_level_dbfs = -25.0f;
  constexpr int num_initial_frames =
      kInitialAdaptiveDigitalGainDb / kMaxGainChangePerFrameDb;
  constexpr int num_frames = 50;

  ASSERT_GT(kWithNoiseDbfs, kMaxOutputNoiseLevelDbfs)
      << "kWithNoiseDbfs is too low";

  for (int i = 0; i < num_initial_frames + num_frames; ++i) {
    VectorFloatFrame fake_audio(kMono, kFrameLen10ms48kHz, 1.0f);
    AdaptiveDigitalGainApplier::FrameInfo info = kFrameInfo;
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
  int AdjacentSpeechFramesThreshold() const { return GetParam(); }
};

TEST_P(AdaptiveDigitalGainApplierTest,
       DoNotIncreaseGainWithTooFewSpeechFrames) {
  const int adjacent_speech_frames_threshold = AdjacentSpeechFramesThreshold();
  GainApplierHelper helper(adjacent_speech_frames_threshold);
  helper.gain_applier->Initialize(/*sample_rate_hz=*/48000, kMono);

  float prev_gain = 0.0f;
  for (int i = 0; i < adjacent_speech_frames_threshold; ++i) {
    SCOPED_TRACE(i);
    VectorFloatFrame audio(kMono, kFrameLen10ms48kHz, 1.0f);
    helper.gain_applier->Process(kFrameInfo, audio.float_frame_view());
    const float gain = audio.float_frame_view().channel(0)[0];
    if (i > 0) {
      EXPECT_EQ(prev_gain, gain);  // No gain increase.
    }
    prev_gain = gain;
  }
}

TEST_P(AdaptiveDigitalGainApplierTest, IncreaseGainWithEnoughSpeechFrames) {
  const int adjacent_speech_frames_threshold = AdjacentSpeechFramesThreshold();
  GainApplierHelper helper(adjacent_speech_frames_threshold);
  helper.gain_applier->Initialize(/*sample_rate_hz=*/48000, kMono);

  float prev_gain = 0.0f;
  for (int i = 0; i < adjacent_speech_frames_threshold; ++i) {
    SCOPED_TRACE(i);
    VectorFloatFrame audio(kMono, kFrameLen10ms48kHz, 1.0f);
    helper.gain_applier->Process(kFrameInfo, audio.float_frame_view());
    prev_gain = audio.float_frame_view().channel(0)[0];
  }

  // Process one more speech frame.
  VectorFloatFrame audio(kMono, kFrameLen10ms48kHz, 1.0f);
  helper.gain_applier->Process(kFrameInfo, audio.float_frame_view());

  // The gain has increased.
  EXPECT_GT(audio.float_frame_view().channel(0)[0], prev_gain);
}

INSTANTIATE_TEST_SUITE_P(GainController2,
                         AdaptiveDigitalGainApplierTest,
                         ::testing::Values(1, 7, 31));

// Checks that the input is never modified when running in dry run mode.
TEST(GainController2GainApplier, DryRunDoesNotChangeInput) {
  ApmDataDumper apm_data_dumper(0);
  AdaptiveDigitalGainApplier gain_applier(
      &apm_data_dumper, /*adjacent_speech_frames_threshold=*/1,
      kMaxGainChangePerSecondDb, kMaxOutputNoiseLevelDbfs, /*dry_run=*/true);
  // Simulate an input signal with log speech level.
  AdaptiveDigitalGainApplier::FrameInfo info = kFrameInfo;
  info.speech_level_dbfs = -60.0f;
  // Allow enough time to reach the maximum gain.
  constexpr int kNumFramesToAdapt =
      static_cast<int>(kMaxGainDb / kMaxGainChangePerFrameDb) + 10;
  constexpr float kPcmSamples = 123.456f;
  // Run the gain applier and check that the PCM samples are not modified.
  gain_applier.Initialize(/*sample_rate_hz=*/8000, kMono);
  for (int i = 0; i < kNumFramesToAdapt; ++i) {
    SCOPED_TRACE(i);
    VectorFloatFrame fake_audio(kMono, kFrameLen10ms8kHz, kPcmSamples);
    gain_applier.Process(info, fake_audio.float_frame_view());
    EXPECT_FLOAT_EQ(fake_audio.float_frame_view().channel(0)[0], kPcmSamples);
  }
}

// Checks that no sample is modified before and after the sample rate changes.
TEST(GainController2GainApplier, DryRunHandlesSampleRateChange) {
  ApmDataDumper apm_data_dumper(0);
  AdaptiveDigitalGainApplier gain_applier(
      &apm_data_dumper, /*adjacent_speech_frames_threshold=*/1,
      kMaxGainChangePerSecondDb, kMaxOutputNoiseLevelDbfs, /*dry_run=*/true);
  AdaptiveDigitalGainApplier::FrameInfo info = kFrameInfo;
  info.speech_level_dbfs = -60.0f;
  constexpr float kPcmSamples = 123.456f;
  VectorFloatFrame fake_audio_8k(kMono, kFrameLen10ms8kHz, kPcmSamples);
  gain_applier.Initialize(/*sample_rate_hz=*/8000, kMono);
  gain_applier.Process(info, fake_audio_8k.float_frame_view());
  EXPECT_FLOAT_EQ(fake_audio_8k.float_frame_view().channel(0)[0], kPcmSamples);
  gain_applier.Initialize(/*sample_rate_hz=*/48000, kMono);
  VectorFloatFrame fake_audio_48k(kMono, kFrameLen10ms48kHz, kPcmSamples);
  gain_applier.Process(info, fake_audio_48k.float_frame_view());
  EXPECT_FLOAT_EQ(fake_audio_48k.float_frame_view().channel(0)[0], kPcmSamples);
}

// Checks that no sample is modified before and after the number of channels
// changes.
TEST(GainController2GainApplier, DryRunHandlesNumChannelsChange) {
  ApmDataDumper apm_data_dumper(0);
  AdaptiveDigitalGainApplier gain_applier(
      &apm_data_dumper, /*adjacent_speech_frames_threshold=*/1,
      kMaxGainChangePerSecondDb, kMaxOutputNoiseLevelDbfs, /*dry_run=*/true);
  AdaptiveDigitalGainApplier::FrameInfo info = kFrameInfo;
  info.speech_level_dbfs = -60.0f;
  constexpr float kPcmSamples = 123.456f;
  VectorFloatFrame fake_audio_8k(kMono, kFrameLen10ms8kHz, kPcmSamples);
  gain_applier.Initialize(/*sample_rate_hz=*/8000, kMono);
  gain_applier.Process(info, fake_audio_8k.float_frame_view());
  EXPECT_FLOAT_EQ(fake_audio_8k.float_frame_view().channel(0)[0], kPcmSamples);
  VectorFloatFrame fake_audio_48k(kStereo, kFrameLen10ms8kHz, kPcmSamples);
  gain_applier.Initialize(/*sample_rate_hz=*/8000, kStereo);
  gain_applier.Process(info, fake_audio_48k.float_frame_view());
  EXPECT_FLOAT_EQ(fake_audio_48k.float_frame_view().channel(0)[0], kPcmSamples);
  EXPECT_FLOAT_EQ(fake_audio_48k.float_frame_view().channel(1)[0], kPcmSamples);
}

}  // namespace
}  // namespace webrtc
