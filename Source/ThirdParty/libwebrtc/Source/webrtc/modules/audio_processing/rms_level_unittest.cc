/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include <cmath>
#include <memory>
#include <vector>

#include "api/array_view.h"
#include "modules/audio_processing/rms_level.h"
#include "rtc_base/checks.h"
#include "rtc_base/numerics/mathutils.h"
#include "rtc_base/numerics/safe_conversions.h"
#include "test/gtest.h"

namespace webrtc {
namespace {
constexpr int kSampleRateHz = 48000;
constexpr size_t kBlockSizeSamples = kSampleRateHz / 100;

std::unique_ptr<RmsLevel> RunTest(rtc::ArrayView<const int16_t> input) {
  std::unique_ptr<RmsLevel> level(new RmsLevel);
  for (size_t n = 0; n + kBlockSizeSamples <= input.size();
       n += kBlockSizeSamples) {
    level->Analyze(input.subview(n, kBlockSizeSamples));
  }
  return level;
}

std::vector<int16_t> CreateSinusoid(int frequency_hz,
                                    int amplitude,
                                    size_t num_samples) {
  std::vector<int16_t> x(num_samples);
  for (size_t n = 0; n < num_samples; ++n) {
    x[n] = rtc::saturated_cast<int16_t>(
        amplitude * std::sin(2 * M_PI * n * frequency_hz / kSampleRateHz));
  }
  return x;
}
}  // namespace

TEST(RmsLevelTest, Run1000HzFullScale) {
  auto x = CreateSinusoid(1000, INT16_MAX, kSampleRateHz);
  auto level = RunTest(x);
  EXPECT_EQ(3, level->Average());  // -3 dBFS
}

TEST(RmsLevelTest, Run1000HzFullScaleAverageAndPeak) {
  auto x = CreateSinusoid(1000, INT16_MAX, kSampleRateHz);
  auto level = RunTest(x);
  auto stats = level->AverageAndPeak();
  EXPECT_EQ(3, stats.average);  // -3 dBFS
  EXPECT_EQ(3, stats.peak);
}

TEST(RmsLevelTest, Run1000HzHalfScale) {
  auto x = CreateSinusoid(1000, INT16_MAX / 2, kSampleRateHz);
  auto level = RunTest(x);
  EXPECT_EQ(9, level->Average());  // -9 dBFS
}

TEST(RmsLevelTest, RunZeros) {
  std::vector<int16_t> x(kSampleRateHz, 0);  // 1 second of pure silence.
  auto level = RunTest(x);
  EXPECT_EQ(127, level->Average());
}

TEST(RmsLevelTest, RunZerosAverageAndPeak) {
  std::vector<int16_t> x(kSampleRateHz, 0);  // 1 second of pure silence.
  auto level = RunTest(x);
  auto stats = level->AverageAndPeak();
  EXPECT_EQ(127, stats.average);
  EXPECT_EQ(127, stats.peak);
}

TEST(RmsLevelTest, NoSamples) {
  RmsLevel level;
  EXPECT_EQ(127, level.Average());  // Return minimum if no samples are given.
}

TEST(RmsLevelTest, NoSamplesAverageAndPeak) {
  RmsLevel level;
  auto stats = level.AverageAndPeak();
  EXPECT_EQ(127, stats.average);
  EXPECT_EQ(127, stats.peak);
}

TEST(RmsLevelTest, PollTwice) {
  auto x = CreateSinusoid(1000, INT16_MAX, kSampleRateHz);
  auto level = RunTest(x);
  level->Average();
  EXPECT_EQ(127, level->Average());  // Stats should be reset at this point.
}

TEST(RmsLevelTest, Reset) {
  auto x = CreateSinusoid(1000, INT16_MAX, kSampleRateHz);
  auto level = RunTest(x);
  level->Reset();
  EXPECT_EQ(127, level->Average());  // Stats should be reset at this point.
}

// Inserts 1 second of full-scale sinusoid, followed by 1 second of muted.
TEST(RmsLevelTest, ProcessMuted) {
  auto x = CreateSinusoid(1000, INT16_MAX, kSampleRateHz);
  auto level = RunTest(x);
  const size_t kBlocksPerSecond = rtc::CheckedDivExact(
      static_cast<size_t>(kSampleRateHz), kBlockSizeSamples);
  for (size_t i = 0; i < kBlocksPerSecond; ++i) {
    level->AnalyzeMuted(kBlockSizeSamples);
  }
  EXPECT_EQ(6, level->Average());  // Average RMS halved due to the silence.
}

// Inserts 1 second of half-scale sinusoid, follwed by 10 ms of full-scale, and
// finally 1 second of half-scale again. Expect the average to be -9 dBFS due
// to the vast majority of the signal being half-scale, and the peak to be
// -3 dBFS.
TEST(RmsLevelTest, RunHalfScaleAndInsertFullScale) {
  auto half_scale = CreateSinusoid(1000, INT16_MAX / 2, kSampleRateHz);
  auto full_scale = CreateSinusoid(1000, INT16_MAX, kSampleRateHz / 100);
  auto x = half_scale;
  x.insert(x.end(), full_scale.begin(), full_scale.end());
  x.insert(x.end(), half_scale.begin(), half_scale.end());
  ASSERT_EQ(static_cast<size_t>(2 * kSampleRateHz + kSampleRateHz / 100),
            x.size());
  auto level = RunTest(x);
  auto stats = level->AverageAndPeak();
  EXPECT_EQ(9, stats.average);
  EXPECT_EQ(3, stats.peak);
}

TEST(RmsLevelTest, ResetOnBlockSizeChange) {
  auto x = CreateSinusoid(1000, INT16_MAX, kSampleRateHz);
  auto level = RunTest(x);
  // Create a new signal with half amplitude, but double block length.
  auto y = CreateSinusoid(1000, INT16_MAX / 2, kBlockSizeSamples * 2);
  level->Analyze(y);
  auto stats = level->AverageAndPeak();
  // Expect all stats to only be influenced by the last signal (y), since the
  // changed block size should reset the stats.
  EXPECT_EQ(9, stats.average);
  EXPECT_EQ(9, stats.peak);
}

}  // namespace webrtc
