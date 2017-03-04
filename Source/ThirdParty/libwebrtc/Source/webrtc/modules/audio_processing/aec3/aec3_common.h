/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_AEC3_AEC3_COMMON_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_AEC3_AEC3_COMMON_H_

#include <stddef.h>
#include "webrtc/typedefs.h"

namespace webrtc {

#ifdef _MSC_VER /* visual c++ */
#define ALIGN16_BEG __declspec(align(16))
#define ALIGN16_END
#else /* gcc or icc */
#define ALIGN16_BEG
#define ALIGN16_END __attribute__((aligned(16)))
#endif

enum class Aec3Optimization { kNone, kSse2 };

constexpr int kMetricsReportingIntervalBlocks = 10 * 250;
constexpr int kMetricsComputationBlocks = 9;
constexpr int kMetricsCollectionBlocks =
    kMetricsReportingIntervalBlocks - kMetricsComputationBlocks;

constexpr size_t kFftLengthBy2 = 64;
constexpr size_t kFftLengthBy2Plus1 = kFftLengthBy2 + 1;
constexpr size_t kFftLengthBy2Minus1 = kFftLengthBy2 - 1;
constexpr size_t kFftLength = 2 * kFftLengthBy2;

constexpr size_t kMaxNumBands = 3;
constexpr size_t kSubFrameLength = 80;

constexpr size_t kBlockSize = kFftLengthBy2;
constexpr size_t kExtendedBlockSize = 2 * kFftLengthBy2;
constexpr size_t kSubBlockSize = 16;

constexpr size_t NumBandsForRate(int sample_rate_hz) {
  return static_cast<size_t>(sample_rate_hz == 8000 ? 1
                                                    : sample_rate_hz / 16000);
}
constexpr int LowestBandRate(int sample_rate_hz) {
  return sample_rate_hz == 8000 ? sample_rate_hz : 16000;
}

constexpr bool ValidFullBandRate(int sample_rate_hz) {
  return sample_rate_hz == 8000 || sample_rate_hz == 16000 ||
         sample_rate_hz == 32000 || sample_rate_hz == 48000;
}

// Detects what kind of optimizations to use for the code.
Aec3Optimization DetectOptimization();

static_assert(1 == NumBandsForRate(8000), "Number of bands for 8 kHz");
static_assert(1 == NumBandsForRate(16000), "Number of bands for 16 kHz");
static_assert(2 == NumBandsForRate(32000), "Number of bands for 32 kHz");
static_assert(3 == NumBandsForRate(48000), "Number of bands for 48 kHz");

static_assert(8000 == LowestBandRate(8000), "Sample rate of band 0 for 8 kHz");
static_assert(16000 == LowestBandRate(16000),
              "Sample rate of band 0 for 16 kHz");
static_assert(16000 == LowestBandRate(32000),
              "Sample rate of band 0 for 32 kHz");
static_assert(16000 == LowestBandRate(48000),
              "Sample rate of band 0 for 48 kHz");

static_assert(ValidFullBandRate(8000),
              "Test that 8 kHz is a valid sample rate");
static_assert(ValidFullBandRate(16000),
              "Test that 16 kHz is a valid sample rate");
static_assert(ValidFullBandRate(32000),
              "Test that 32 kHz is a valid sample rate");
static_assert(ValidFullBandRate(48000),
              "Test that 48 kHz is a valid sample rate");
static_assert(!ValidFullBandRate(8001),
              "Test that 8001 Hz is not a valid sample rate");

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_AEC3_AEC3_COMMON_H_
