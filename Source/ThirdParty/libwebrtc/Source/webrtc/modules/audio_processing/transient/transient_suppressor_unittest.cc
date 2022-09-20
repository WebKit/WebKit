/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/transient/transient_suppressor.h"

#include <vector>

#include "absl/types/optional.h"
#include "modules/audio_processing/transient/common.h"
#include "modules/audio_processing/transient/transient_suppressor_impl.h"
#include "test/gtest.h"

namespace webrtc {
namespace {
constexpr int kMono = 1;

// Returns the index of the first non-zero sample in `samples` or an unspecified
// value if no value is zero.
absl::optional<int> FindFirstNonZeroSample(const std::vector<float>& samples) {
  for (size_t i = 0; i < samples.size(); ++i) {
    if (samples[i] != 0.0f) {
      return i;
    }
  }
  return absl::nullopt;
}

}  // namespace

class TransientSuppressorVadModeParametrization
    : public ::testing::TestWithParam<TransientSuppressor::VadMode> {};

TEST_P(TransientSuppressorVadModeParametrization,
       TypingDetectionLogicWorksAsExpectedForMono) {
  TransientSuppressorImpl ts(GetParam(), ts::kSampleRate16kHz,
                             ts::kSampleRate16kHz, kMono);

  // Each key-press enables detection.
  EXPECT_FALSE(ts.detection_enabled_);
  ts.UpdateKeypress(true);
  EXPECT_TRUE(ts.detection_enabled_);

  // It takes four seconds without any key-press to disable the detection
  for (int time_ms = 0; time_ms < 3990; time_ms += ts::kChunkSizeMs) {
    ts.UpdateKeypress(false);
    EXPECT_TRUE(ts.detection_enabled_);
  }
  ts.UpdateKeypress(false);
  EXPECT_FALSE(ts.detection_enabled_);

  // Key-presses that are more than a second apart from each other don't enable
  // suppression.
  for (int i = 0; i < 100; ++i) {
    EXPECT_FALSE(ts.suppression_enabled_);
    ts.UpdateKeypress(true);
    EXPECT_TRUE(ts.detection_enabled_);
    EXPECT_FALSE(ts.suppression_enabled_);
    for (int time_ms = 0; time_ms < 990; time_ms += ts::kChunkSizeMs) {
      ts.UpdateKeypress(false);
      EXPECT_TRUE(ts.detection_enabled_);
      EXPECT_FALSE(ts.suppression_enabled_);
    }
    ts.UpdateKeypress(false);
  }

  // Two consecutive key-presses is enough to enable the suppression.
  ts.UpdateKeypress(true);
  EXPECT_FALSE(ts.suppression_enabled_);
  ts.UpdateKeypress(true);
  EXPECT_TRUE(ts.suppression_enabled_);

  // Key-presses that are less than a second apart from each other don't disable
  // detection nor suppression.
  for (int i = 0; i < 100; ++i) {
    for (int time_ms = 0; time_ms < 1000; time_ms += ts::kChunkSizeMs) {
      ts.UpdateKeypress(false);
      EXPECT_TRUE(ts.detection_enabled_);
      EXPECT_TRUE(ts.suppression_enabled_);
    }
    ts.UpdateKeypress(true);
    EXPECT_TRUE(ts.detection_enabled_);
    EXPECT_TRUE(ts.suppression_enabled_);
  }

  // It takes four seconds without any key-press to disable the detection and
  // suppression.
  for (int time_ms = 0; time_ms < 3990; time_ms += ts::kChunkSizeMs) {
    ts.UpdateKeypress(false);
    EXPECT_TRUE(ts.detection_enabled_);
    EXPECT_TRUE(ts.suppression_enabled_);
  }
  for (int time_ms = 0; time_ms < 1000; time_ms += ts::kChunkSizeMs) {
    ts.UpdateKeypress(false);
    EXPECT_FALSE(ts.detection_enabled_);
    EXPECT_FALSE(ts.suppression_enabled_);
  }
}

INSTANTIATE_TEST_SUITE_P(
    TransientSuppressorImplTest,
    TransientSuppressorVadModeParametrization,
    ::testing::Values(TransientSuppressor::VadMode::kDefault,
                      TransientSuppressor::VadMode::kRnnVad,
                      TransientSuppressor::VadMode::kNoVad));

class TransientSuppressorSampleRateParametrization
    : public ::testing::TestWithParam<int> {};

// Checks that voice probability and processed audio data are temporally aligned
// after `Suppress()` is called.
TEST_P(TransientSuppressorSampleRateParametrization,
       CheckAudioAndVoiceProbabilityTemporallyAligned) {
  const int sample_rate_hz = GetParam();
  TransientSuppressorImpl ts(TransientSuppressor::VadMode::kDefault,
                             sample_rate_hz,
                             /*detection_rate_hz=*/sample_rate_hz, kMono);

  const int frame_size = sample_rate_hz * ts::kChunkSizeMs / 1000;
  std::vector<float> frame(frame_size);

  constexpr int kMaxAttempts = 3;
  for (int i = 0; i < kMaxAttempts; ++i) {
    SCOPED_TRACE(i);

    // Call `Suppress()` on frames of non-zero audio samples.
    std::fill(frame.begin(), frame.end(), 1000.0f);
    float delayed_voice_probability = ts.Suppress(
        frame.data(), frame.size(), kMono, /*detection_data=*/nullptr,
        /*detection_length=*/frame_size, /*reference_data=*/nullptr,
        /*reference_length=*/frame_size, /*voice_probability=*/1.0f,
        /*key_pressed=*/false);

    // Detect the algorithmic delay of `TransientSuppressorImpl`.
    absl::optional<int> frame_delay = FindFirstNonZeroSample(frame);

    // Check that the delayed voice probability is delayed according to the
    // measured delay.
    if (frame_delay.has_value()) {
      if (*frame_delay == 0) {
        // When the delay is a multiple integer of the frame duration,
        // `Suppress()` returns a copy of a previously observed voice
        // probability value.
        EXPECT_EQ(delayed_voice_probability, 1.0f);
      } else {
        // Instead, when the delay is fractional, `Suppress()` returns an
        // interpolated value. Since the exact value depends on the
        // interpolation method, we only check that the delayed voice
        // probability is not zero as it must converge towards the previoulsy
        // observed value.
        EXPECT_GT(delayed_voice_probability, 0.0f);
      }
      break;
    } else {
      // The algorithmic delay is longer than the duration of a single frame.
      // Until the delay is detected, the delayed voice probability is zero.
      EXPECT_EQ(delayed_voice_probability, 0.0f);
    }
  }
}

INSTANTIATE_TEST_SUITE_P(TransientSuppressorImplTest,
                         TransientSuppressorSampleRateParametrization,
                         ::testing::Values(ts::kSampleRate8kHz,
                                           ts::kSampleRate16kHz,
                                           ts::kSampleRate32kHz,
                                           ts::kSampleRate48kHz));

}  // namespace webrtc
