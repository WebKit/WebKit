/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/transient/voice_probability_delay_unit.h"

#include "test/gtest.h"

namespace webrtc {
namespace {

// Checks that with zero delay, the observed value is immediately returned as
// delayed value.
TEST(VoiceProbabilityDelayUnit, NoDelay) {
  VoiceProbabilityDelayUnit delay_unit(/*delay_num_samples=*/0,
                                       /*sample_rate_hz=*/48000);
  constexpr int kMax = 5;
  for (int i = 0; i <= kMax; ++i) {
    SCOPED_TRACE(i);
    float voice_probability = static_cast<float>(i) / kMax;
    EXPECT_EQ(voice_probability, delay_unit.Delay(voice_probability));
  }
}

// Checks that with integer delays, an exact copy of a previously observed value
// is returned.
TEST(VoiceProbabilityDelayUnit, IntegerDelay) {
  VoiceProbabilityDelayUnit delay_unit_10ms(/*delay_num_samples=*/480,
                                            /*sample_rate_hz=*/48000);
  delay_unit_10ms.Delay(0.125f);
  EXPECT_EQ(0.125f, delay_unit_10ms.Delay(0.9f));

  VoiceProbabilityDelayUnit delay_unit_20ms(/*delay_num_samples=*/960,
                                            /*sample_rate_hz=*/48000);
  delay_unit_20ms.Delay(0.125f);
  delay_unit_20ms.Delay(0.8f);
  EXPECT_EQ(0.125f, delay_unit_20ms.Delay(0.9f));
}

// Checks that with a fractional delay < 10 ms, interpolation is applied.
TEST(VoiceProbabilityDelayUnit, FractionalDelayLessThan10ms) {
  // Create delay unit with fractional delay of 6 ms.
  VoiceProbabilityDelayUnit delay_unit(/*delay_num_samples=*/288,
                                       /*sample_rate_hz=*/48000);
  //  frame 0
  // --------- frame 1
  //          ---------
  //    0000001111
  delay_unit.Delay(1.0f);
  EXPECT_FLOAT_EQ(0.68f, delay_unit.Delay(0.2f));
}

// Checks that with a fractional delay > 10 ms, interpolation is applied.
TEST(VoiceProbabilityDelayUnit, FractionalDelayGreaterThan10ms) {
  // Create delay unit with fractional delay of 14 ms.
  VoiceProbabilityDelayUnit delay_unit(/*delay_num_samples=*/672,
                                       /*sample_rate_hz=*/48000);
  //  frame 0
  // --------- frame 1
  //          --------- frame 2
  //                   ---------
  //      0000111111
  delay_unit.Delay(1.0f);
  delay_unit.Delay(0.2f);
  EXPECT_FLOAT_EQ(0.52f, delay_unit.Delay(1.0f));
}

// Checks that `Initialize()` resets the delay unit.
TEST(VoiceProbabilityDelayUnit, InitializeResetsDelayUnit) {
  VoiceProbabilityDelayUnit delay_unit(/*delay_num_samples=*/960,
                                       /*sample_rate_hz=*/48000);
  delay_unit.Delay(1.0f);
  delay_unit.Delay(0.9f);

  delay_unit.Initialize(/*delay_num_samples=*/160, /*sample_rate_hz=*/8000);
  EXPECT_EQ(0.0f, delay_unit.Delay(0.1f));
  EXPECT_EQ(0.0f, delay_unit.Delay(0.2f));
  EXPECT_EQ(0.1f, delay_unit.Delay(0.3f));
}

// Checks that `Initialize()` handles delay changes.
TEST(VoiceProbabilityDelayUnit, InitializeHandlesDelayChanges) {
  // Start with a 20 ms delay.
  VoiceProbabilityDelayUnit delay_unit(/*delay_num_samples=*/960,
                                       /*sample_rate_hz=*/48000);
  delay_unit.Delay(1.0f);
  delay_unit.Delay(0.9f);

  // Lower the delay to 10 ms.
  delay_unit.Initialize(/*delay_num_samples=*/80, /*sample_rate_hz=*/8000);
  EXPECT_EQ(0.0f, delay_unit.Delay(0.1f));
  EXPECT_EQ(0.1f, delay_unit.Delay(0.2f));

  // Increase the delay to 15 ms.
  delay_unit.Initialize(/*delay_num_samples=*/120, /*sample_rate_hz=*/8000);
  EXPECT_EQ(0.0f, delay_unit.Delay(0.1f));
  EXPECT_EQ(0.05f, delay_unit.Delay(0.2f));
  EXPECT_EQ(0.15f, delay_unit.Delay(0.3f));
}

}  // namespace
}  // namespace webrtc
