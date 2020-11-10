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

// Constants used in place of estimated noise levels.
constexpr float kNoNoiseDbfs = -90.f;
constexpr float kWithNoiseDbfs = -20.f;
static_assert(std::is_trivially_destructible<VadLevelAnalyzer::Result>::value,
              "");
constexpr VadLevelAnalyzer::Result kVadSpeech{1.f, -20.f, 0.f};

constexpr float kMaxGainChangePerSecondDb = 3.f;
constexpr float kMaxGainChangePerFrameDb =
    kMaxGainChangePerSecondDb * kFrameDurationMs / 1000.f;
constexpr float kMaxOutputNoiseLevelDbfs = -50.f;

// Helper to instance `AdaptiveDigitalGainApplier`.
struct GainApplierHelper {
  GainApplierHelper()
      : GainApplierHelper(/*adjacent_speech_frames_threshold=*/1) {}
  explicit GainApplierHelper(int adjacent_speech_frames_threshold)
      : apm_data_dumper(0),
        gain_applier(&apm_data_dumper,
                     adjacent_speech_frames_threshold,
                     kMaxGainChangePerSecondDb,
                     kMaxOutputNoiseLevelDbfs) {}
  ApmDataDumper apm_data_dumper;
  AdaptiveDigitalGainApplier gain_applier;
};

// Runs gain applier and returns the applied gain in linear scale.
float RunOnConstantLevel(int num_iterations,
                         VadLevelAnalyzer::Result vad_level,
                         float input_level_dbfs,
                         AdaptiveDigitalGainApplier* gain_applier) {
  float gain_linear = 0.f;

  for (int i = 0; i < num_iterations; ++i) {
    VectorFloatFrame fake_audio(kMono, kFrameLen10ms8kHz, 1.f);
    AdaptiveDigitalGainApplier::FrameInfo info;
    info.input_level_dbfs = input_level_dbfs;
    info.input_noise_level_dbfs = kNoNoiseDbfs;
    info.vad_result = vad_level;
    info.limiter_envelope_dbfs = -2.f;
    info.estimate_is_confident = true;
    gain_applier->Process(info, fake_audio.float_frame_view());
    gain_linear = fake_audio.float_frame_view().channel(0)[0];
  }
  return gain_linear;
}

// Voice on, no noise, low limiter, confident level.
constexpr AdaptiveDigitalGainApplier::FrameInfo kFrameInfo{
    /*input_level_dbfs=*/-1.f,
    /*input_noise_level_dbfs=*/kNoNoiseDbfs,
    /*vad_result=*/kVadSpeech,
    /*limiter_envelope_dbfs=*/-2.f,
    /*estimate_is_confident=*/true};

TEST(AutomaticGainController2AdaptiveGainApplier, GainApplierShouldNotCrash) {
  GainApplierHelper helper;
  // Make one call with reasonable audio level values and settings.
  VectorFloatFrame fake_audio(kStereo, kFrameLen10ms48kHz, 10000.f);
  AdaptiveDigitalGainApplier::FrameInfo info = kFrameInfo;
  info.input_level_dbfs = -5.0;
  helper.gain_applier.Process(kFrameInfo, fake_audio.float_frame_view());
}

// Check that the output is -kHeadroom dBFS.
TEST(AutomaticGainController2AdaptiveGainApplier, TargetLevelIsReached) {
  GainApplierHelper helper;

  constexpr float initial_level_dbfs = -5.f;

  const float applied_gain = RunOnConstantLevel(
      200, kVadSpeech, initial_level_dbfs, &helper.gain_applier);

  EXPECT_NEAR(applied_gain, DbToRatio(-kHeadroomDbfs - initial_level_dbfs),
              0.1f);
}

// Check that the output is -kHeadroom dBFS
TEST(AutomaticGainController2AdaptiveGainApplier, GainApproachesMaxGain) {
  GainApplierHelper helper;

  constexpr float initial_level_dbfs = -kHeadroomDbfs - kMaxGainDb - 10.f;
  // A few extra frames for safety.
  constexpr int kNumFramesToAdapt =
      static_cast<int>(kMaxGainDb / kMaxGainChangePerFrameDb) + 10;

  const float applied_gain = RunOnConstantLevel(
      kNumFramesToAdapt, kVadSpeech, initial_level_dbfs, &helper.gain_applier);
  EXPECT_NEAR(applied_gain, DbToRatio(kMaxGainDb), 0.1f);

  const float applied_gain_db = 20.f * std::log10(applied_gain);
  EXPECT_NEAR(applied_gain_db, kMaxGainDb, 0.1f);
}

TEST(AutomaticGainController2AdaptiveGainApplier, GainDoesNotChangeFast) {
  GainApplierHelper helper;

  constexpr float initial_level_dbfs = -25.f;
  // A few extra frames for safety.
  constexpr int kNumFramesToAdapt =
      static_cast<int>(initial_level_dbfs / kMaxGainChangePerFrameDb) + 10;

  const float kMaxChangePerFrameLinear = DbToRatio(kMaxGainChangePerFrameDb);

  float last_gain_linear = 1.f;
  for (int i = 0; i < kNumFramesToAdapt; ++i) {
    SCOPED_TRACE(i);
    VectorFloatFrame fake_audio(kMono, kFrameLen10ms8kHz, 1.f);
    AdaptiveDigitalGainApplier::FrameInfo info = kFrameInfo;
    info.input_level_dbfs = initial_level_dbfs;
    helper.gain_applier.Process(info, fake_audio.float_frame_view());
    float current_gain_linear = fake_audio.float_frame_view().channel(0)[0];
    EXPECT_LE(std::abs(current_gain_linear - last_gain_linear),
              kMaxChangePerFrameLinear);
    last_gain_linear = current_gain_linear;
  }

  // Check that the same is true when gain decreases as well.
  for (int i = 0; i < kNumFramesToAdapt; ++i) {
    SCOPED_TRACE(i);
    VectorFloatFrame fake_audio(kMono, kFrameLen10ms8kHz, 1.f);
    AdaptiveDigitalGainApplier::FrameInfo info = kFrameInfo;
    info.input_level_dbfs = 0.f;
    helper.gain_applier.Process(info, fake_audio.float_frame_view());
    float current_gain_linear = fake_audio.float_frame_view().channel(0)[0];
    EXPECT_LE(std::abs(current_gain_linear - last_gain_linear),
              kMaxChangePerFrameLinear);
    last_gain_linear = current_gain_linear;
  }
}

TEST(AutomaticGainController2AdaptiveGainApplier, GainIsRampedInAFrame) {
  GainApplierHelper helper;

  constexpr float initial_level_dbfs = -25.f;

  VectorFloatFrame fake_audio(kMono, kFrameLen10ms48kHz, 1.f);
  AdaptiveDigitalGainApplier::FrameInfo info = kFrameInfo;
  info.input_level_dbfs = initial_level_dbfs;
  helper.gain_applier.Process(info, fake_audio.float_frame_view());
  float maximal_difference = 0.f;
  float current_value = 1.f * DbToRatio(kInitialAdaptiveDigitalGainDb);
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

TEST(AutomaticGainController2AdaptiveGainApplier, NoiseLimitsGain) {
  GainApplierHelper helper;

  constexpr float initial_level_dbfs = -25.f;
  constexpr int num_initial_frames =
      kInitialAdaptiveDigitalGainDb / kMaxGainChangePerFrameDb;
  constexpr int num_frames = 50;

  ASSERT_GT(kWithNoiseDbfs, kMaxOutputNoiseLevelDbfs)
      << "kWithNoiseDbfs is too low";

  for (int i = 0; i < num_initial_frames + num_frames; ++i) {
    VectorFloatFrame fake_audio(kMono, kFrameLen10ms48kHz, 1.f);
    AdaptiveDigitalGainApplier::FrameInfo info = kFrameInfo;
    info.input_level_dbfs = initial_level_dbfs;
    info.input_noise_level_dbfs = kWithNoiseDbfs;
    helper.gain_applier.Process(info, fake_audio.float_frame_view());

    // Wait so that the adaptive gain applier has time to lower the gain.
    if (i > num_initial_frames) {
      const float maximal_ratio =
          *std::max_element(fake_audio.float_frame_view().channel(0).begin(),
                            fake_audio.float_frame_view().channel(0).end());

      EXPECT_NEAR(maximal_ratio, 1.f, 0.001f);
    }
  }
}

TEST(AutomaticGainController2GainApplier, CanHandlePositiveSpeechLevels) {
  GainApplierHelper helper;

  // Make one call with positive audio level values and settings.
  VectorFloatFrame fake_audio(kStereo, kFrameLen10ms48kHz, 10000.f);
  AdaptiveDigitalGainApplier::FrameInfo info = kFrameInfo;
  info.input_level_dbfs = 5.f;
  helper.gain_applier.Process(info, fake_audio.float_frame_view());
}

TEST(AutomaticGainController2GainApplier, AudioLevelLimitsGain) {
  GainApplierHelper helper;

  constexpr float initial_level_dbfs = -25.f;
  constexpr int num_initial_frames =
      kInitialAdaptiveDigitalGainDb / kMaxGainChangePerFrameDb;
  constexpr int num_frames = 50;

  ASSERT_GT(kWithNoiseDbfs, kMaxOutputNoiseLevelDbfs)
      << "kWithNoiseDbfs is too low";

  for (int i = 0; i < num_initial_frames + num_frames; ++i) {
    VectorFloatFrame fake_audio(kMono, kFrameLen10ms48kHz, 1.f);
    AdaptiveDigitalGainApplier::FrameInfo info = kFrameInfo;
    info.input_level_dbfs = initial_level_dbfs;
    info.limiter_envelope_dbfs = 1.f;
    info.estimate_is_confident = false;
    helper.gain_applier.Process(info, fake_audio.float_frame_view());

    // Wait so that the adaptive gain applier has time to lower the gain.
    if (i > num_initial_frames) {
      const float maximal_ratio =
          *std::max_element(fake_audio.float_frame_view().channel(0).begin(),
                            fake_audio.float_frame_view().channel(0).end());

      EXPECT_NEAR(maximal_ratio, 1.f, 0.001f);
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

  AdaptiveDigitalGainApplier::FrameInfo info = kFrameInfo;
  info.input_level_dbfs = -25.0;

  float prev_gain = 0.f;
  for (int i = 0; i < adjacent_speech_frames_threshold; ++i) {
    SCOPED_TRACE(i);
    VectorFloatFrame audio(kMono, kFrameLen10ms48kHz, 1.f);
    helper.gain_applier.Process(info, audio.float_frame_view());
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

  AdaptiveDigitalGainApplier::FrameInfo info = kFrameInfo;
  info.input_level_dbfs = -25.0;

  float prev_gain = 0.f;
  for (int i = 0; i < adjacent_speech_frames_threshold; ++i) {
    VectorFloatFrame audio(kMono, kFrameLen10ms48kHz, 1.f);
    helper.gain_applier.Process(info, audio.float_frame_view());
    prev_gain = audio.float_frame_view().channel(0)[0];
  }

  // Process one more speech frame.
  VectorFloatFrame audio(kMono, kFrameLen10ms48kHz, 1.f);
  helper.gain_applier.Process(info, audio.float_frame_view());

  // The gain has increased.
  EXPECT_GT(audio.float_frame_view().channel(0)[0], prev_gain);
}

INSTANTIATE_TEST_SUITE_P(AutomaticGainController2,
                         AdaptiveDigitalGainApplierTest,
                         ::testing::Values(1, 7, 31));

}  // namespace
}  // namespace webrtc
