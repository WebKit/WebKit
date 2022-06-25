/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc/clipping_predictor.h"

#include <cstdint>
#include <limits>
#include <tuple>

#include "rtc_base/checks.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::Eq;
using ::testing::Optional;
using ClippingPredictorConfig = AudioProcessing::Config::GainController1::
    AnalogGainController::ClippingPredictor;
using ClippingPredictorMode = AudioProcessing::Config::GainController1::
    AnalogGainController::ClippingPredictor::Mode;

constexpr int kSampleRateHz = 32000;
constexpr int kNumChannels = 1;
constexpr int kSamplesPerChannel = kSampleRateHz / 100;
constexpr int kMaxMicLevel = 255;
constexpr int kMinMicLevel = 12;
constexpr int kDefaultClippedLevelStep = 15;
constexpr float kMaxSampleS16 =
    static_cast<float>(std::numeric_limits<int16_t>::max());

// Threshold in dB corresponding to a signal with an amplitude equal to 99% of
// the dynamic range - i.e., computed as `20*log10(0.99)`.
constexpr float kClippingThresholdDb = -0.08729610804900176f;

void CallAnalyze(int num_calls,
                 const AudioFrameView<const float>& frame,
                 ClippingPredictor& predictor) {
  for (int i = 0; i < num_calls; ++i) {
    predictor.Analyze(frame);
  }
}

// Creates and analyzes an audio frame with a non-zero (approx. 4.15dB) crest
// factor.
void AnalyzeNonZeroCrestFactorAudio(int num_calls,
                                    int num_channels,
                                    float peak_ratio,
                                    ClippingPredictor& predictor) {
  RTC_DCHECK_GT(num_calls, 0);
  RTC_DCHECK_GT(num_channels, 0);
  RTC_DCHECK_LE(peak_ratio, 1.0f);
  std::vector<float*> audio(num_channels);
  std::vector<float> audio_data(num_channels * kSamplesPerChannel, 0.0f);
  for (int channel = 0; channel < num_channels; ++channel) {
    audio[channel] = &audio_data[channel * kSamplesPerChannel];
    for (int sample = 0; sample < kSamplesPerChannel; sample += 10) {
      audio[channel][sample] = 0.1f * peak_ratio * kMaxSampleS16;
      audio[channel][sample + 1] = 0.2f * peak_ratio * kMaxSampleS16;
      audio[channel][sample + 2] = 0.3f * peak_ratio * kMaxSampleS16;
      audio[channel][sample + 3] = 0.4f * peak_ratio * kMaxSampleS16;
      audio[channel][sample + 4] = 0.5f * peak_ratio * kMaxSampleS16;
      audio[channel][sample + 5] = 0.6f * peak_ratio * kMaxSampleS16;
      audio[channel][sample + 6] = 0.7f * peak_ratio * kMaxSampleS16;
      audio[channel][sample + 7] = 0.8f * peak_ratio * kMaxSampleS16;
      audio[channel][sample + 8] = 0.9f * peak_ratio * kMaxSampleS16;
      audio[channel][sample + 9] = 1.0f * peak_ratio * kMaxSampleS16;
    }
  }
  AudioFrameView<const float> frame(audio.data(), num_channels,
                                    kSamplesPerChannel);
  CallAnalyze(num_calls, frame, predictor);
}

void CheckChannelEstimatesWithValue(int num_channels,
                                    int level,
                                    int default_step,
                                    int min_mic_level,
                                    int max_mic_level,
                                    const ClippingPredictor& predictor,
                                    int expected) {
  for (int i = 0; i < num_channels; ++i) {
    SCOPED_TRACE(i);
    EXPECT_THAT(predictor.EstimateClippedLevelStep(
                    i, level, default_step, min_mic_level, max_mic_level),
                Optional(Eq(expected)));
  }
}

void CheckChannelEstimatesWithoutValue(int num_channels,
                                       int level,
                                       int default_step,
                                       int min_mic_level,
                                       int max_mic_level,
                                       const ClippingPredictor& predictor) {
  for (int i = 0; i < num_channels; ++i) {
    SCOPED_TRACE(i);
    EXPECT_EQ(predictor.EstimateClippedLevelStep(i, level, default_step,
                                                 min_mic_level, max_mic_level),
              absl::nullopt);
  }
}

// Creates and analyzes an audio frame with a zero crest factor.
void AnalyzeZeroCrestFactorAudio(int num_calls,
                                 int num_channels,
                                 float peak_ratio,
                                 ClippingPredictor& predictor) {
  RTC_DCHECK_GT(num_calls, 0);
  RTC_DCHECK_GT(num_channels, 0);
  RTC_DCHECK_LE(peak_ratio, 1.f);
  std::vector<float*> audio(num_channels);
  std::vector<float> audio_data(num_channels * kSamplesPerChannel, 0.f);
  for (int channel = 0; channel < num_channels; ++channel) {
    audio[channel] = &audio_data[channel * kSamplesPerChannel];
    for (int sample = 0; sample < kSamplesPerChannel; ++sample) {
      audio[channel][sample] = peak_ratio * kMaxSampleS16;
    }
  }
  auto frame = AudioFrameView<const float>(audio.data(), num_channels,
                                           kSamplesPerChannel);
  CallAnalyze(num_calls, frame, predictor);
}

TEST(ClippingPeakPredictorTest, NoPredictorCreated) {
  auto predictor =
      CreateClippingPredictor(kNumChannels, /*config=*/{/*enabled=*/false});
  EXPECT_FALSE(predictor);
}

TEST(ClippingPeakPredictorTest, ClippingEventPredictionCreated) {
  // TODO(bugs.webrtc.org/12874): Use designated initializers one fixed.
  auto predictor = CreateClippingPredictor(
      kNumChannels,
      /*config=*/{/*enabled=*/true,
                  /*mode=*/ClippingPredictorMode::kClippingEventPrediction});
  EXPECT_TRUE(predictor);
}

TEST(ClippingPeakPredictorTest, AdaptiveStepClippingPeakPredictionCreated) {
  // TODO(bugs.webrtc.org/12874): Use designated initializers one fixed.
  auto predictor = CreateClippingPredictor(
      kNumChannels, /*config=*/{
          /*enabled=*/true,
          /*mode=*/ClippingPredictorMode::kAdaptiveStepClippingPeakPrediction});
  EXPECT_TRUE(predictor);
}

TEST(ClippingPeakPredictorTest, FixedStepClippingPeakPredictionCreated) {
  // TODO(bugs.webrtc.org/12874): Use designated initializers one fixed.
  auto predictor = CreateClippingPredictor(
      kNumChannels, /*config=*/{
          /*enabled=*/true,
          /*mode=*/ClippingPredictorMode::kFixedStepClippingPeakPrediction});
  EXPECT_TRUE(predictor);
}

class ClippingPredictorParameterization
    : public ::testing::TestWithParam<std::tuple<int, int, int, int>> {
 protected:
  int num_channels() const { return std::get<0>(GetParam()); }
  ClippingPredictorConfig GetConfig(ClippingPredictorMode mode) const {
    // TODO(bugs.webrtc.org/12874): Use designated initializers one fixed.
    return {/*enabled=*/true,
            /*mode=*/mode,
            /*window_length=*/std::get<1>(GetParam()),
            /*reference_window_length=*/std::get<2>(GetParam()),
            /*reference_window_delay=*/std::get<3>(GetParam()),
            /*clipping_threshold=*/-1.0f,
            /*crest_factor_margin=*/0.5f};
  }
};

TEST_P(ClippingPredictorParameterization,
       CheckClippingEventPredictorEstimateAfterCrestFactorDrop) {
  const ClippingPredictorConfig config =
      GetConfig(ClippingPredictorMode::kClippingEventPrediction);
  if (config.reference_window_length + config.reference_window_delay <=
      config.window_length) {
    return;
  }
  auto predictor = CreateClippingPredictor(num_channels(), config);
  AnalyzeNonZeroCrestFactorAudio(
      /*num_calls=*/config.reference_window_length +
          config.reference_window_delay - config.window_length,
      num_channels(), /*peak_ratio=*/0.99f, *predictor);
  CheckChannelEstimatesWithoutValue(num_channels(), /*level=*/255,
                                    kDefaultClippedLevelStep, kMinMicLevel,
                                    kMaxMicLevel, *predictor);
  AnalyzeZeroCrestFactorAudio(config.window_length, num_channels(),
                              /*peak_ratio=*/0.99f, *predictor);
  CheckChannelEstimatesWithValue(
      num_channels(), /*level=*/255, kDefaultClippedLevelStep, kMinMicLevel,
      kMaxMicLevel, *predictor, kDefaultClippedLevelStep);
}

TEST_P(ClippingPredictorParameterization,
       CheckClippingEventPredictorNoEstimateAfterConstantCrestFactor) {
  const ClippingPredictorConfig config =
      GetConfig(ClippingPredictorMode::kClippingEventPrediction);
  if (config.reference_window_length + config.reference_window_delay <=
      config.window_length) {
    return;
  }
  auto predictor = CreateClippingPredictor(num_channels(), config);
  AnalyzeNonZeroCrestFactorAudio(
      /*num_calls=*/config.reference_window_length +
          config.reference_window_delay - config.window_length,
      num_channels(), /*peak_ratio=*/0.99f, *predictor);
  CheckChannelEstimatesWithoutValue(num_channels(), /*level=*/255,
                                    kDefaultClippedLevelStep, kMinMicLevel,
                                    kMaxMicLevel, *predictor);
  AnalyzeNonZeroCrestFactorAudio(/*num_calls=*/config.window_length,
                                 num_channels(),
                                 /*peak_ratio=*/0.99f, *predictor);
  CheckChannelEstimatesWithoutValue(num_channels(), /*level=*/255,
                                    kDefaultClippedLevelStep, kMinMicLevel,
                                    kMaxMicLevel, *predictor);
}

TEST_P(ClippingPredictorParameterization,
       CheckClippingPeakPredictorEstimateAfterHighCrestFactor) {
  const ClippingPredictorConfig config =
      GetConfig(ClippingPredictorMode::kAdaptiveStepClippingPeakPrediction);
  if (config.reference_window_length + config.reference_window_delay <=
      config.window_length) {
    return;
  }
  auto predictor = CreateClippingPredictor(num_channels(), config);
  AnalyzeNonZeroCrestFactorAudio(
      /*num_calls=*/config.reference_window_length +
          config.reference_window_delay - config.window_length,
      num_channels(), /*peak_ratio=*/0.99f, *predictor);
  CheckChannelEstimatesWithoutValue(num_channels(), /*level=*/255,
                                    kDefaultClippedLevelStep, kMinMicLevel,
                                    kMaxMicLevel, *predictor);
  AnalyzeNonZeroCrestFactorAudio(/*num_calls=*/config.window_length,
                                 num_channels(),
                                 /*peak_ratio=*/0.99f, *predictor);
  CheckChannelEstimatesWithValue(
      num_channels(), /*level=*/255, kDefaultClippedLevelStep, kMinMicLevel,
      kMaxMicLevel, *predictor, kDefaultClippedLevelStep);
}

TEST_P(ClippingPredictorParameterization,
       CheckClippingPeakPredictorNoEstimateAfterLowCrestFactor) {
  const ClippingPredictorConfig config =
      GetConfig(ClippingPredictorMode::kAdaptiveStepClippingPeakPrediction);
  if (config.reference_window_length + config.reference_window_delay <=
      config.window_length) {
    return;
  }
  auto predictor = CreateClippingPredictor(num_channels(), config);
  AnalyzeZeroCrestFactorAudio(
      /*num_calls=*/config.reference_window_length +
          config.reference_window_delay - config.window_length,
      num_channels(), /*peak_ratio=*/0.99f, *predictor);
  CheckChannelEstimatesWithoutValue(num_channels(), /*level=*/255,
                                    kDefaultClippedLevelStep, kMinMicLevel,
                                    kMaxMicLevel, *predictor);
  AnalyzeNonZeroCrestFactorAudio(/*num_calls=*/config.window_length,
                                 num_channels(),
                                 /*peak_ratio=*/0.99f, *predictor);
  CheckChannelEstimatesWithoutValue(num_channels(), /*level=*/255,
                                    kDefaultClippedLevelStep, kMinMicLevel,
                                    kMaxMicLevel, *predictor);
}

INSTANTIATE_TEST_SUITE_P(GainController1ClippingPredictor,
                         ClippingPredictorParameterization,
                         ::testing::Combine(::testing::Values(1, 5),
                                            ::testing::Values(1, 5, 10),
                                            ::testing::Values(1, 5),
                                            ::testing::Values(0, 1, 5)));

class ClippingEventPredictorParameterization
    : public ::testing::TestWithParam<std::tuple<float, float>> {
 protected:
  ClippingPredictorConfig GetConfig() const {
    // TODO(bugs.webrtc.org/12874): Use designated initializers one fixed.
    return {/*enabled=*/true,
            /*mode=*/ClippingPredictorMode::kClippingEventPrediction,
            /*window_length=*/5,
            /*reference_window_length=*/5,
            /*reference_window_delay=*/5,
            /*clipping_threshold=*/std::get<0>(GetParam()),
            /*crest_factor_margin=*/std::get<1>(GetParam())};
  }
};

TEST_P(ClippingEventPredictorParameterization,
       CheckEstimateAfterCrestFactorDrop) {
  const ClippingPredictorConfig config = GetConfig();
  auto predictor = CreateClippingPredictor(kNumChannels, config);
  AnalyzeNonZeroCrestFactorAudio(/*num_calls=*/config.reference_window_length,
                                 kNumChannels, /*peak_ratio=*/0.99f,
                                 *predictor);
  CheckChannelEstimatesWithoutValue(kNumChannels, /*level=*/255,
                                    kDefaultClippedLevelStep, kMinMicLevel,
                                    kMaxMicLevel, *predictor);
  AnalyzeZeroCrestFactorAudio(config.window_length, kNumChannels,
                              /*peak_ratio=*/0.99f, *predictor);
  // TODO(bugs.webrtc.org/12774): Add clarifying comment.
  // TODO(bugs.webrtc.org/12774): Remove 4.15f threshold and split tests.
  if (config.clipping_threshold < kClippingThresholdDb &&
      config.crest_factor_margin < 4.15f) {
    CheckChannelEstimatesWithValue(
        kNumChannels, /*level=*/255, kDefaultClippedLevelStep, kMinMicLevel,
        kMaxMicLevel, *predictor, kDefaultClippedLevelStep);
  } else {
    CheckChannelEstimatesWithoutValue(kNumChannels, /*level=*/255,
                                      kDefaultClippedLevelStep, kMinMicLevel,
                                      kMaxMicLevel, *predictor);
  }
}

INSTANTIATE_TEST_SUITE_P(GainController1ClippingPredictor,
                         ClippingEventPredictorParameterization,
                         ::testing::Combine(::testing::Values(-1.0f, 0.0f),
                                            ::testing::Values(3.0f, 4.16f)));

class ClippingPredictorModeParameterization
    : public ::testing::TestWithParam<ClippingPredictorMode> {
 protected:
  ClippingPredictorConfig GetConfig(float clipping_threshold_dbfs) const {
    // TODO(bugs.webrtc.org/12874): Use designated initializers one fixed.
    return {/*enabled=*/true,
            /*mode=*/GetParam(),
            /*window_length=*/5,
            /*reference_window_length=*/5,
            /*reference_window_delay=*/5,
            /*clipping_threshold=*/clipping_threshold_dbfs,
            /*crest_factor_margin=*/3.0f};
  }
};

TEST_P(ClippingPredictorModeParameterization,
       CheckEstimateAfterHighCrestFactorWithNoClippingMargin) {
  const ClippingPredictorConfig config = GetConfig(
      /*clipping_threshold_dbfs=*/0.0f);
  auto predictor = CreateClippingPredictor(kNumChannels, config);
  AnalyzeNonZeroCrestFactorAudio(/*num_calls=*/config.reference_window_length,
                                 kNumChannels, /*peak_ratio=*/0.99f,
                                 *predictor);
  CheckChannelEstimatesWithoutValue(kNumChannels, /*level=*/255,
                                    kDefaultClippedLevelStep, kMinMicLevel,
                                    kMaxMicLevel, *predictor);
  AnalyzeZeroCrestFactorAudio(config.window_length, kNumChannels,
                              /*peak_ratio=*/0.99f, *predictor);
  // Since the clipping threshold is set to 0 dBFS, `EstimateClippedLevelStep()`
  // is expected to return an unavailable value.
  CheckChannelEstimatesWithoutValue(kNumChannels, /*level=*/255,
                                    kDefaultClippedLevelStep, kMinMicLevel,
                                    kMaxMicLevel, *predictor);
}

TEST_P(ClippingPredictorModeParameterization,
       CheckEstimateAfterHighCrestFactorWithClippingMargin) {
  const ClippingPredictorConfig config =
      GetConfig(/*clipping_threshold_dbfs=*/-1.0f);
  auto predictor = CreateClippingPredictor(kNumChannels, config);
  AnalyzeNonZeroCrestFactorAudio(/*num_calls=*/config.reference_window_length,
                                 kNumChannels,
                                 /*peak_ratio=*/0.99f, *predictor);
  CheckChannelEstimatesWithoutValue(kNumChannels, /*level=*/255,
                                    kDefaultClippedLevelStep, kMinMicLevel,
                                    kMaxMicLevel, *predictor);
  AnalyzeZeroCrestFactorAudio(config.window_length, kNumChannels,
                              /*peak_ratio=*/0.99f, *predictor);
  // TODO(bugs.webrtc.org/12774): Add clarifying comment.
  const float expected_step =
      config.mode == ClippingPredictorMode::kAdaptiveStepClippingPeakPrediction
          ? 17
          : kDefaultClippedLevelStep;
  CheckChannelEstimatesWithValue(kNumChannels, /*level=*/255,
                                 kDefaultClippedLevelStep, kMinMicLevel,
                                 kMaxMicLevel, *predictor, expected_step);
}

INSTANTIATE_TEST_SUITE_P(
    GainController1ClippingPredictor,
    ClippingPredictorModeParameterization,
    ::testing::Values(
        ClippingPredictorMode::kAdaptiveStepClippingPeakPrediction,
        ClippingPredictorMode::kFixedStepClippingPeakPrediction));

TEST(ClippingEventPredictorTest, CheckEstimateAfterReset) {
  // TODO(bugs.webrtc.org/12874): Use designated initializers one fixed.
  constexpr ClippingPredictorConfig kConfig{
      /*enabled=*/true,
      /*mode=*/ClippingPredictorMode::kClippingEventPrediction,
      /*window_length=*/5,
      /*reference_window_length=*/5,
      /*reference_window_delay=*/5,
      /*clipping_threshold=*/-1.0f,
      /*crest_factor_margin=*/3.0f};
  auto predictor = CreateClippingPredictor(kNumChannels, kConfig);
  AnalyzeNonZeroCrestFactorAudio(/*num_calls=*/kConfig.reference_window_length,
                                 kNumChannels,
                                 /*peak_ratio=*/0.99f, *predictor);
  CheckChannelEstimatesWithoutValue(kNumChannels, /*level=*/255,
                                    kDefaultClippedLevelStep, kMinMicLevel,
                                    kMaxMicLevel, *predictor);
  predictor->Reset();
  AnalyzeZeroCrestFactorAudio(kConfig.window_length, kNumChannels,
                              /*peak_ratio=*/0.99f, *predictor);
  CheckChannelEstimatesWithoutValue(kNumChannels, /*level=*/255,
                                    kDefaultClippedLevelStep, kMinMicLevel,
                                    kMaxMicLevel, *predictor);
}

TEST(ClippingPeakPredictorTest, CheckNoEstimateAfterReset) {
  // TODO(bugs.webrtc.org/12874): Use designated initializers one fixed.
  constexpr ClippingPredictorConfig kConfig{
      /*enabled=*/true,
      /*mode=*/ClippingPredictorMode::kAdaptiveStepClippingPeakPrediction,
      /*window_length=*/5,
      /*reference_window_length=*/5,
      /*reference_window_delay=*/5,
      /*clipping_threshold=*/-1.0f};
  auto predictor = CreateClippingPredictor(kNumChannels, kConfig);
  AnalyzeNonZeroCrestFactorAudio(/*num_calls=*/kConfig.reference_window_length,
                                 kNumChannels,
                                 /*peak_ratio=*/0.99f, *predictor);
  CheckChannelEstimatesWithoutValue(kNumChannels, /*level=*/255,
                                    kDefaultClippedLevelStep, kMinMicLevel,
                                    kMaxMicLevel, *predictor);
  predictor->Reset();
  AnalyzeZeroCrestFactorAudio(kConfig.window_length, kNumChannels,
                              /*peak_ratio=*/0.99f, *predictor);
  CheckChannelEstimatesWithoutValue(kNumChannels, /*level=*/255,
                                    kDefaultClippedLevelStep, kMinMicLevel,
                                    kMaxMicLevel, *predictor);
}

TEST(ClippingPeakPredictorTest, CheckAdaptiveStepEstimate) {
  // TODO(bugs.webrtc.org/12874): Use designated initializers one fixed.
  constexpr ClippingPredictorConfig kConfig{
      /*enabled=*/true,
      /*mode=*/ClippingPredictorMode::kAdaptiveStepClippingPeakPrediction,
      /*window_length=*/5,
      /*reference_window_length=*/5,
      /*reference_window_delay=*/5,
      /*clipping_threshold=*/-1.0f};
  auto predictor = CreateClippingPredictor(kNumChannels, kConfig);
  AnalyzeNonZeroCrestFactorAudio(/*num_calls=*/kConfig.reference_window_length,
                                 kNumChannels, /*peak_ratio=*/0.99f,
                                 *predictor);
  CheckChannelEstimatesWithoutValue(kNumChannels, /*level=*/255,
                                    kDefaultClippedLevelStep, kMinMicLevel,
                                    kMaxMicLevel, *predictor);
  AnalyzeZeroCrestFactorAudio(kConfig.window_length, kNumChannels,
                              /*peak_ratio=*/0.99f, *predictor);
  CheckChannelEstimatesWithValue(kNumChannels, /*level=*/255,
                                 kDefaultClippedLevelStep, kMinMicLevel,
                                 kMaxMicLevel, *predictor, /*expected=*/17);
}

TEST(ClippingPeakPredictorTest, CheckFixedStepEstimate) {
  // TODO(bugs.webrtc.org/12874): Use designated initializers one fixed.
  constexpr ClippingPredictorConfig kConfig{
      /*enabled=*/true,
      /*mode=*/ClippingPredictorMode::kFixedStepClippingPeakPrediction,
      /*window_length=*/5,
      /*reference_window_length=*/5,
      /*reference_window_delay=*/5,
      /*clipping_threshold=*/-1.0f};
  auto predictor = CreateClippingPredictor(kNumChannels, kConfig);
  AnalyzeNonZeroCrestFactorAudio(/*num_calls=*/kConfig.reference_window_length,
                                 kNumChannels, /*peak_ratio=*/0.99f,
                                 *predictor);
  CheckChannelEstimatesWithoutValue(kNumChannels, /*level=*/255,
                                    kDefaultClippedLevelStep, kMinMicLevel,
                                    kMaxMicLevel, *predictor);
  AnalyzeZeroCrestFactorAudio(kConfig.window_length, kNumChannels,
                              /*peak_ratio=*/0.99f, *predictor);
  CheckChannelEstimatesWithValue(
      kNumChannels, /*level=*/255, kDefaultClippedLevelStep, kMinMicLevel,
      kMaxMicLevel, *predictor, kDefaultClippedLevelStep);
}

}  // namespace
}  // namespace webrtc
