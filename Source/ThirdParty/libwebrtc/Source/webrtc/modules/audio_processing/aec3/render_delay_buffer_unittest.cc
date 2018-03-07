/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/render_delay_buffer.h"

#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "api/array_view.h"
#include "modules/audio_processing/aec3/aec3_common.h"
#include "modules/audio_processing/logging/apm_data_dumper.h"
#include "rtc_base/random.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

std::string ProduceDebugText(int sample_rate_hz) {
  std::ostringstream ss;
  ss << "Sample rate: " << sample_rate_hz;
  return ss.str();
}

constexpr size_t kDownSamplingFactor = 4;
constexpr size_t kNumMatchedFilters = 4;

}  // namespace

// Verifies that the buffer overflow is correctly reported.
TEST(RenderDelayBuffer, BufferOverflow) {
  for (auto rate : {8000, 16000, 32000, 48000}) {
    SCOPED_TRACE(ProduceDebugText(rate));
    std::unique_ptr<RenderDelayBuffer> delay_buffer(RenderDelayBuffer::Create(
        NumBandsForRate(rate), kDownSamplingFactor,
        GetDownSampledBufferSize(kDownSamplingFactor, kNumMatchedFilters),
        GetRenderDelayBufferSize(kDownSamplingFactor, kNumMatchedFilters)));
    std::vector<std::vector<float>> block_to_insert(
        NumBandsForRate(rate), std::vector<float>(kBlockSize, 0.f));
    for (size_t k = 0; k < kMaxApiCallsJitterBlocks; ++k) {
      EXPECT_TRUE(delay_buffer->Insert(block_to_insert));
    }
    EXPECT_FALSE(delay_buffer->Insert(block_to_insert));
  }
}

// Verifies that the check for available block works.
TEST(RenderDelayBuffer, AvailableBlock) {
  constexpr size_t kNumBands = 1;
  std::unique_ptr<RenderDelayBuffer> delay_buffer(RenderDelayBuffer::Create(
      kNumBands, kDownSamplingFactor,
      GetDownSampledBufferSize(kDownSamplingFactor, kNumMatchedFilters),
      GetRenderDelayBufferSize(kDownSamplingFactor, kNumMatchedFilters)));
  std::vector<std::vector<float>> input_block(
      kNumBands, std::vector<float>(kBlockSize, 1.f));
  EXPECT_TRUE(delay_buffer->Insert(input_block));
  delay_buffer->UpdateBuffers();
}

// Verifies the SetDelay method.
TEST(RenderDelayBuffer, SetDelay) {
  std::unique_ptr<RenderDelayBuffer> delay_buffer(RenderDelayBuffer::Create(
      1, kDownSamplingFactor,
      GetDownSampledBufferSize(kDownSamplingFactor, kNumMatchedFilters),
      GetRenderDelayBufferSize(kDownSamplingFactor, kNumMatchedFilters)));
  EXPECT_EQ(0u, delay_buffer->Delay());
  for (size_t delay = 0; delay < 20; ++delay) {
    delay_buffer->SetDelay(delay);
    EXPECT_EQ(delay, delay_buffer->Delay());
  }
}

#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)

// Verifies the check for feasible delay.
// TODO(peah): Re-enable the test once the issue with memory leaks during DEATH
// tests on test bots has been fixed.
TEST(RenderDelayBuffer, DISABLED_WrongDelay) {
  std::unique_ptr<RenderDelayBuffer> delay_buffer(RenderDelayBuffer::Create(
      3, kDownSamplingFactor,
      GetDownSampledBufferSize(kDownSamplingFactor, kNumMatchedFilters),
      GetRenderDelayBufferSize(kDownSamplingFactor, kNumMatchedFilters)));
  EXPECT_DEATH(delay_buffer->SetDelay(21), "");
}

// Verifies the check for the number of bands in the inserted blocks.
TEST(RenderDelayBuffer, WrongNumberOfBands) {
  for (auto rate : {16000, 32000, 48000}) {
    SCOPED_TRACE(ProduceDebugText(rate));
    std::unique_ptr<RenderDelayBuffer> delay_buffer(RenderDelayBuffer::Create(
        NumBandsForRate(rate), kDownSamplingFactor,
        GetDownSampledBufferSize(kDownSamplingFactor, kNumMatchedFilters),
        GetRenderDelayBufferSize(kDownSamplingFactor, kNumMatchedFilters)));
    std::vector<std::vector<float>> block_to_insert(
        NumBandsForRate(rate < 48000 ? rate + 16000 : 16000),
        std::vector<float>(kBlockSize, 0.f));
    EXPECT_DEATH(delay_buffer->Insert(block_to_insert), "");
  }
}

// Verifies the check of the length of the inserted blocks.
TEST(RenderDelayBuffer, WrongBlockLength) {
  for (auto rate : {8000, 16000, 32000, 48000}) {
    SCOPED_TRACE(ProduceDebugText(rate));
    std::unique_ptr<RenderDelayBuffer> delay_buffer(RenderDelayBuffer::Create(
        3, kDownSamplingFactor,
        GetDownSampledBufferSize(kDownSamplingFactor, kNumMatchedFilters),
        GetRenderDelayBufferSize(kDownSamplingFactor, kNumMatchedFilters)));
    std::vector<std::vector<float>> block_to_insert(
        NumBandsForRate(rate), std::vector<float>(kBlockSize - 1, 0.f));
    EXPECT_DEATH(delay_buffer->Insert(block_to_insert), "");
  }
}

#endif

}  // namespace webrtc
