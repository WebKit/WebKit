/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc2/rnn_vad/features_extraction.h"

#include <cmath>
#include <vector>

#include "modules/audio_processing/agc2/rnn_vad/test_utils.h"
// TODO(bugs.webrtc.org/8948): Add when the issue is fixed.
// #include "test/fpe_observer.h"
#include "test/gtest.h"

namespace webrtc {
namespace rnn_vad {
namespace test {
namespace {

constexpr size_t ceil(size_t n, size_t m) {
  return (n + m - 1) / m;
}

// Number of 10 ms frames required to fill a pitch buffer having size
// |kBufSize24kHz|.
constexpr size_t kNumTestDataFrames = ceil(kBufSize24kHz, kFrameSize10ms24kHz);
// Number of samples for the test data.
constexpr size_t kNumTestDataSize = kNumTestDataFrames * kFrameSize10ms24kHz;

// Verifies that the pitch in Hz is in the detectable range.
bool PitchIsValid(float pitch_hz) {
  const size_t pitch_period =
      static_cast<size_t>(static_cast<float>(kSampleRate24kHz) / pitch_hz);
  return kInitialMinPitch24kHz <= pitch_period &&
         pitch_period <= kMaxPitch24kHz;
}

void CreatePureTone(float amplitude, float freq_hz, rtc::ArrayView<float> dst) {
  for (size_t i = 0; i < dst.size(); ++i) {
    dst[i] = amplitude * std::sin(2.f * kPi * freq_hz * i / kSampleRate24kHz);
  }
}

// Feeds |features_extractor| with |samples| splitting it in 10 ms frames.
// For every frame, the output is written into |feature_vector|. Returns true
// if silence is detected in the last frame.
bool FeedTestData(FeaturesExtractor* features_extractor,
                  rtc::ArrayView<const float> samples,
                  rtc::ArrayView<float, kFeatureVectorSize> feature_vector) {
  // TODO(bugs.webrtc.org/8948): Add when the issue is fixed.
  // FloatingPointExceptionObserver fpe_observer;
  bool is_silence = true;
  const size_t num_frames = samples.size() / kFrameSize10ms24kHz;
  for (size_t i = 0; i < num_frames; ++i) {
    is_silence = features_extractor->CheckSilenceComputeFeatures(
        {samples.data() + i * kFrameSize10ms24kHz, kFrameSize10ms24kHz},
        feature_vector);
  }
  return is_silence;
}

}  // namespace

// Extracts the features for two pure tones and verifies that the pitch field
// values reflect the known tone frequencies.
TEST(RnnVadTest, FeatureExtractionLowHighPitch) {
  constexpr float amplitude = 1000.f;
  constexpr float low_pitch_hz = 150.f;
  constexpr float high_pitch_hz = 250.f;
  ASSERT_TRUE(PitchIsValid(low_pitch_hz));
  ASSERT_TRUE(PitchIsValid(high_pitch_hz));

  FeaturesExtractor features_extractor;
  std::vector<float> samples(kNumTestDataSize);
  std::vector<float> feature_vector(kFeatureVectorSize);
  ASSERT_EQ(kFeatureVectorSize, feature_vector.size());
  rtc::ArrayView<float, kFeatureVectorSize> feature_vector_view(
      feature_vector.data(), kFeatureVectorSize);

  // Extract the normalized scalar feature that is proportional to the estimated
  // pitch period.
  constexpr size_t pitch_feature_index = kFeatureVectorSize - 2;
  // Low frequency tone - i.e., high period.
  CreatePureTone(amplitude, low_pitch_hz, samples);
  ASSERT_FALSE(FeedTestData(&features_extractor, samples, feature_vector_view));
  float high_pitch_period = feature_vector_view[pitch_feature_index];
  // High frequency tone - i.e., low period.
  features_extractor.Reset();
  CreatePureTone(amplitude, high_pitch_hz, samples);
  ASSERT_FALSE(FeedTestData(&features_extractor, samples, feature_vector_view));
  float low_pitch_period = feature_vector_view[pitch_feature_index];
  // Check.
  EXPECT_LT(low_pitch_period, high_pitch_period);
}

}  // namespace test
}  // namespace rnn_vad
}  // namespace webrtc
