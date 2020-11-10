/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc2/saturation_protector.h"

#include <algorithm>

#include "modules/audio_processing/agc2/agc2_common.h"
#include "modules/audio_processing/logging/apm_data_dumper.h"
#include "rtc_base/gunit.h"
#include "test/gmock.h"

namespace webrtc {
namespace {

constexpr float kInitialMarginDb = 20.f;

using saturation_protector_impl::RingBuffer;

SaturationProtectorState CreateSaturationProtectorState() {
  SaturationProtectorState state;
  ResetSaturationProtectorState(kInitialMarginDb, state);
  return state;
}

// Updates `state` for `num_iterations` times with constant speech level and
// peak powers and returns the maximum margin.
float RunOnConstantLevel(int num_iterations,
                         float speech_peak_dbfs,
                         float speech_level_dbfs,
                         SaturationProtectorState& state) {
  float last_margin = state.margin_db;
  float max_difference = 0.f;
  for (int i = 0; i < num_iterations; ++i) {
    UpdateSaturationProtectorState(speech_peak_dbfs, speech_level_dbfs, state);
    const float new_margin = state.margin_db;
    max_difference =
        std::max(max_difference, std::abs(new_margin - last_margin));
    last_margin = new_margin;
  }
  return max_difference;
}

}  // namespace

TEST(AutomaticGainController2SaturationProtector, RingBufferInit) {
  RingBuffer b;
  EXPECT_EQ(b.Size(), 0);
  EXPECT_FALSE(b.Front().has_value());
}

TEST(AutomaticGainController2SaturationProtector, RingBufferPushBack) {
  RingBuffer b;
  constexpr float kValue = 123.f;
  b.PushBack(kValue);
  EXPECT_EQ(b.Size(), 1);
  ASSERT_TRUE(b.Front().has_value());
  EXPECT_EQ(b.Front().value(), kValue);
}

TEST(AutomaticGainController2SaturationProtector, RingBufferReset) {
  RingBuffer b;
  b.PushBack(123.f);
  b.Reset();
  EXPECT_EQ(b.Size(), 0);
  EXPECT_FALSE(b.Front().has_value());
}

// Checks that the front value does not change until the ring buffer gets full.
TEST(AutomaticGainController2SaturationProtector,
     RingBufferFrontUntilBufferIsFull) {
  RingBuffer b;
  constexpr float kValue = 123.f;
  b.PushBack(kValue);
  for (int i = 1; i < b.Capacity(); ++i) {
    EXPECT_EQ(b.Front().value(), kValue);
    b.PushBack(kValue + i);
  }
}

// Checks that when the buffer is full it behaves as a shift register.
TEST(AutomaticGainController2SaturationProtector,
     FullRingBufferFrontIsDelayed) {
  RingBuffer b;
  // Fill the buffer.
  for (int i = 0; i < b.Capacity(); ++i) {
    b.PushBack(i);
  }
  // The ring buffer should now behave as a shift register with a delay equal to
  // its capacity.
  for (int i = b.Capacity(); i < 2 * b.Capacity() + 1; ++i) {
    EXPECT_EQ(b.Front().value(), i - b.Capacity());
    b.PushBack(i);
  }
}

// Checks that a state after reset equals a state after construction.
TEST(AutomaticGainController2SaturationProtector, ResetState) {
  SaturationProtectorState init_state;
  ResetSaturationProtectorState(kInitialMarginDb, init_state);

  SaturationProtectorState state;
  ResetSaturationProtectorState(kInitialMarginDb, state);
  RunOnConstantLevel(/*num_iterations=*/10, /*speech_level_dbfs=*/-20.f,
                     /*speech_peak_dbfs=*/-10.f, state);
  ASSERT_NE(init_state, state);  // Make sure that there are side-effects.
  ResetSaturationProtectorState(kInitialMarginDb, state);

  EXPECT_EQ(init_state, state);
}

// Checks that the estimate converges to the ratio between peaks and level
// estimator values after a while.
TEST(AutomaticGainController2SaturationProtector,
     ProtectorEstimatesCrestRatio) {
  constexpr int kNumIterations = 2000;
  constexpr float kPeakLevel = -20.f;
  constexpr float kCrestFactor = kInitialMarginDb + 1.f;
  constexpr float kSpeechLevel = kPeakLevel - kCrestFactor;
  const float kMaxDifference = 0.5f * std::abs(kInitialMarginDb - kCrestFactor);

  auto state = CreateSaturationProtectorState();
  RunOnConstantLevel(kNumIterations, kPeakLevel, kSpeechLevel, state);

  EXPECT_NEAR(state.margin_db, kCrestFactor, kMaxDifference);
}

// Checks that the margin does not change too quickly.
TEST(AutomaticGainController2SaturationProtector, ChangeSlowly) {
  constexpr int kNumIterations = 1000;
  constexpr float kPeakLevel = -20.f;
  constexpr float kCrestFactor = kInitialMarginDb - 5.f;
  constexpr float kOtherCrestFactor = kInitialMarginDb;
  constexpr float kSpeechLevel = kPeakLevel - kCrestFactor;
  constexpr float kOtherSpeechLevel = kPeakLevel - kOtherCrestFactor;

  auto state = CreateSaturationProtectorState();
  float max_difference =
      RunOnConstantLevel(kNumIterations, kPeakLevel, kSpeechLevel, state);
  max_difference = std::max(
      RunOnConstantLevel(kNumIterations, kPeakLevel, kOtherSpeechLevel, state),
      max_difference);

  constexpr float kMaxChangeSpeedDbPerSecond = 0.5f;  // 1 db / 2 seconds.
  EXPECT_LE(max_difference,
            kMaxChangeSpeedDbPerSecond / 1000 * kFrameDurationMs);
}

// Checks that there is a delay between input change and margin adaptations.
TEST(AutomaticGainController2SaturationProtector, AdaptToDelayedChanges) {
  constexpr int kDelayIterations = kFullBufferSizeMs / kFrameDurationMs;
  constexpr float kInitialSpeechLevelDbfs = -30.f;
  constexpr float kLaterSpeechLevelDbfs = -15.f;

  auto state = CreateSaturationProtectorState();
  // First run on initial level.
  float max_difference = RunOnConstantLevel(
      kDelayIterations, kInitialSpeechLevelDbfs + kInitialMarginDb,
      kInitialSpeechLevelDbfs, state);
  // Then peak changes, but not RMS.
  max_difference =
      std::max(RunOnConstantLevel(kDelayIterations,
                                  kLaterSpeechLevelDbfs + kInitialMarginDb,
                                  kInitialSpeechLevelDbfs, state),
               max_difference);
  // Then both change.
  max_difference =
      std::max(RunOnConstantLevel(kDelayIterations,
                                  kLaterSpeechLevelDbfs + kInitialMarginDb,
                                  kLaterSpeechLevelDbfs, state),
               max_difference);

  // The saturation protector expects that the RMS changes roughly
  // 'kFullBufferSizeMs' after peaks change. This is to account for delay
  // introduced by the level estimator. Therefore, the input above is 'normal'
  // and 'expected', and shouldn't influence the margin by much.
  const float total_difference = std::abs(state.margin_db - kInitialMarginDb);

  EXPECT_LE(total_difference, 0.05f);
  EXPECT_LE(max_difference, 0.01f);
}

}  // namespace webrtc
