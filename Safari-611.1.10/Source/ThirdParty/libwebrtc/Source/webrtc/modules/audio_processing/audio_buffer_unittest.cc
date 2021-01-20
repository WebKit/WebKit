/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/audio_buffer.h"

#include <cmath>

#include "test/gtest.h"
#include "test/testsupport/rtc_expect_death.h"

namespace webrtc {

namespace {

const size_t kSampleRateHz = 48000u;
const size_t kStereo = 2u;
const size_t kMono = 1u;

void ExpectNumChannels(const AudioBuffer& ab, size_t num_channels) {
  EXPECT_EQ(ab.num_channels(), num_channels);
}

}  // namespace

TEST(AudioBufferTest, SetNumChannelsSetsChannelBuffersNumChannels) {
  AudioBuffer ab(kSampleRateHz, kStereo, kSampleRateHz, kStereo, kSampleRateHz,
                 kStereo);
  ExpectNumChannels(ab, kStereo);
  ab.set_num_channels(1);
  ExpectNumChannels(ab, kMono);
  ab.RestoreNumChannels();
  ExpectNumChannels(ab, kStereo);
}

#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)
TEST(AudioBufferDeathTest, SetNumChannelsDeathTest) {
  AudioBuffer ab(kSampleRateHz, kMono, kSampleRateHz, kMono, kSampleRateHz,
                 kMono);
  RTC_EXPECT_DEATH(ab.set_num_channels(kStereo), "num_channels");
}
#endif

TEST(AudioBufferTest, CopyWithoutResampling) {
  AudioBuffer ab1(32000, 2, 32000, 2, 32000, 2);
  AudioBuffer ab2(32000, 2, 32000, 2, 32000, 2);
  // Fill first buffer.
  for (size_t ch = 0; ch < ab1.num_channels(); ++ch) {
    for (size_t i = 0; i < ab1.num_frames(); ++i) {
      ab1.channels()[ch][i] = i + ch;
    }
  }
  // Copy to second buffer.
  ab1.CopyTo(&ab2);
  // Verify content of second buffer.
  for (size_t ch = 0; ch < ab2.num_channels(); ++ch) {
    for (size_t i = 0; i < ab2.num_frames(); ++i) {
      EXPECT_EQ(ab2.channels()[ch][i], i + ch);
    }
  }
}

TEST(AudioBufferTest, CopyWithResampling) {
  AudioBuffer ab1(32000, 2, 32000, 2, 48000, 2);
  AudioBuffer ab2(48000, 2, 48000, 2, 48000, 2);
  float energy_ab1 = 0.f;
  float energy_ab2 = 0.f;
  const float pi = std::acos(-1.f);
  // Put a sine and compute energy of first buffer.
  for (size_t ch = 0; ch < ab1.num_channels(); ++ch) {
    for (size_t i = 0; i < ab1.num_frames(); ++i) {
      ab1.channels()[ch][i] = std::sin(2 * pi * 100.f / 32000.f * i);
      energy_ab1 += ab1.channels()[ch][i] * ab1.channels()[ch][i];
    }
  }
  // Copy to second buffer.
  ab1.CopyTo(&ab2);
  // Compute energy of second buffer.
  for (size_t ch = 0; ch < ab2.num_channels(); ++ch) {
    for (size_t i = 0; i < ab2.num_frames(); ++i) {
      energy_ab2 += ab2.channels()[ch][i] * ab2.channels()[ch][i];
    }
  }
  // Verify that energies match.
  EXPECT_NEAR(energy_ab1, energy_ab2 * 32000.f / 48000.f, .01f * energy_ab1);
}
}  // namespace webrtc
