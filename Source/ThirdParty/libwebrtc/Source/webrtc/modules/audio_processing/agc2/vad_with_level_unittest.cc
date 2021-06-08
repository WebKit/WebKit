/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc2/vad_with_level.h"

#include <memory>
#include <vector>

#include "modules/audio_processing/agc2/agc2_common.h"
#include "modules/audio_processing/include/audio_frame_view.h"
#include "rtc_base/gunit.h"
#include "rtc_base/numerics/safe_compare.h"
#include "test/gmock.h"

namespace webrtc {
namespace {

using ::testing::AnyNumber;
using ::testing::ReturnRoundRobin;

constexpr float kInstantAttack = 1.f;
constexpr float kSlowAttack = 0.1f;

constexpr int kSampleRateHz = 8000;

class MockVad : public VadLevelAnalyzer::VoiceActivityDetector {
 public:
  MOCK_METHOD(float,
              ComputeProbability,
              (AudioFrameView<const float> frame),
              (override));
};

// Creates a `VadLevelAnalyzer` injecting a mock VAD which repeatedly returns
// the next value from `speech_probabilities` until it reaches the end and will
// restart from the beginning.
std::unique_ptr<VadLevelAnalyzer> CreateVadLevelAnalyzerWithMockVad(
    float vad_probability_attack,
    const std::vector<float>& speech_probabilities) {
  auto vad = std::make_unique<MockVad>();
  EXPECT_CALL(*vad, ComputeProbability)
      .Times(AnyNumber())
      .WillRepeatedly(ReturnRoundRobin(speech_probabilities));
  return std::make_unique<VadLevelAnalyzer>(vad_probability_attack,
                                            std::move(vad));
}

// 10 ms mono frame.
struct FrameWithView {
  // Ctor. Initializes the frame samples with `value`.
  FrameWithView(float value = 0.f)
      : channel0(samples.data()),
        view(&channel0, /*num_channels=*/1, samples.size()) {
    samples.fill(value);
  }
  std::array<float, kSampleRateHz / 100> samples;
  const float* const channel0;
  const AudioFrameView<const float> view;
};

TEST(AutomaticGainController2VadLevelAnalyzer, PeakLevelGreaterThanRmsLevel) {
  // Handcrafted frame so that the average is lower than the peak value.
  FrameWithView frame(1000.f);  // Constant frame.
  frame.samples[10] = 2000.f;   // Except for one peak value.

  // Compute audio frame levels (the VAD result is ignored).
  VadLevelAnalyzer analyzer;
  auto levels_and_vad_prob = analyzer.AnalyzeFrame(frame.view);

  // Compare peak and RMS levels.
  EXPECT_LT(levels_and_vad_prob.rms_dbfs, levels_and_vad_prob.peak_dbfs);
}

// Checks that the unprocessed and the smoothed speech probabilities match when
// instant attack is used.
TEST(AutomaticGainController2VadLevelAnalyzer, NoSpeechProbabilitySmoothing) {
  const std::vector<float> speech_probabilities{0.709f, 0.484f, 0.882f, 0.167f,
                                                0.44f,  0.525f, 0.858f, 0.314f,
                                                0.653f, 0.965f, 0.413f, 0.f};
  auto analyzer =
      CreateVadLevelAnalyzerWithMockVad(kInstantAttack, speech_probabilities);
  FrameWithView frame;
  for (int i = 0; rtc::SafeLt(i, speech_probabilities.size()); ++i) {
    SCOPED_TRACE(i);
    EXPECT_EQ(speech_probabilities[i],
              analyzer->AnalyzeFrame(frame.view).speech_probability);
  }
}

// Checks that the smoothed speech probability does not instantly converge to
// the unprocessed one when slow attack is used.
TEST(AutomaticGainController2VadLevelAnalyzer,
     SlowAttackSpeechProbabilitySmoothing) {
  const std::vector<float> speech_probabilities{0.f, 0.f, 1.f, 1.f, 1.f, 1.f};
  auto analyzer =
      CreateVadLevelAnalyzerWithMockVad(kSlowAttack, speech_probabilities);
  FrameWithView frame;
  float prev_probability = 0.f;
  for (int i = 0; rtc::SafeLt(i, speech_probabilities.size()); ++i) {
    SCOPED_TRACE(i);
    const float smoothed_probability =
        analyzer->AnalyzeFrame(frame.view).speech_probability;
    EXPECT_LT(smoothed_probability, 1.f);  // Not enough time to reach 1.
    EXPECT_LE(prev_probability, smoothed_probability);  // Converge towards 1.
    prev_probability = smoothed_probability;
  }
}

// Checks that the smoothed speech probability instantly decays to the
// unprocessed one when slow attack is used.
TEST(AutomaticGainController2VadLevelAnalyzer, SpeechProbabilityInstantDecay) {
  const std::vector<float> speech_probabilities{1.f, 1.f, 1.f, 1.f, 1.f, 0.f};
  auto analyzer =
      CreateVadLevelAnalyzerWithMockVad(kSlowAttack, speech_probabilities);
  FrameWithView frame;
  for (int i = 0; rtc::SafeLt(i, speech_probabilities.size() - 1); ++i) {
    analyzer->AnalyzeFrame(frame.view);
  }
  EXPECT_EQ(0.f, analyzer->AnalyzeFrame(frame.view).speech_probability);
}

}  // namespace
}  // namespace webrtc
