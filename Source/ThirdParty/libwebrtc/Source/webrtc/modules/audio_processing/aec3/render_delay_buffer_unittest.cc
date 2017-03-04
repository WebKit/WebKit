/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_processing/aec3/render_delay_buffer.h"

#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "webrtc/base/array_view.h"
#include "webrtc/base/random.h"
#include "webrtc/modules/audio_processing/aec3/aec3_common.h"
#include "webrtc/modules/audio_processing/logging/apm_data_dumper.h"
#include "webrtc/test/gtest.h"

namespace webrtc {
namespace {

std::string ProduceDebugText(int sample_rate_hz) {
  std::ostringstream ss;
  ss << "Sample rate: " << sample_rate_hz;
  return ss.str();
}

std::string ProduceDebugText(int sample_rate_hz, size_t delay) {
  std::ostringstream ss;
  ss << "Sample rate: " << sample_rate_hz;
  ss << ", Delay: " << delay;
  return ss.str();
}

constexpr size_t kMaxApiCallJitter = 30;

}  // namespace

// Verifies that the basic swap in the insert call works.
TEST(RenderDelayBuffer, InsertSwap) {
  for (auto rate : {8000, 16000, 32000, 48000}) {
    SCOPED_TRACE(ProduceDebugText(rate));
    std::unique_ptr<RenderDelayBuffer> delay_buffer(RenderDelayBuffer::Create(
        250, NumBandsForRate(rate), kMaxApiCallJitter));
    for (size_t k = 0; k < 10; ++k) {
      std::vector<std::vector<float>> block_to_insert(
          NumBandsForRate(rate), std::vector<float>(kBlockSize, k + 1));
      std::vector<std::vector<float>> reference_block = block_to_insert;

      EXPECT_TRUE(delay_buffer->Insert(&block_to_insert));
      EXPECT_NE(reference_block, block_to_insert);
    }
  }
}

// Verifies that the buffer passes the blocks in a bitexact manner when the
// delay is zero.
TEST(RenderDelayBuffer, BasicBitexactness) {
  for (auto rate : {8000, 16000, 32000, 48000}) {
    SCOPED_TRACE(ProduceDebugText(rate));
    std::unique_ptr<RenderDelayBuffer> delay_buffer(RenderDelayBuffer::Create(
        20, NumBandsForRate(rate), kMaxApiCallJitter));
    for (size_t k = 0; k < 200; ++k) {
      std::vector<std::vector<float>> block_to_insert(
          NumBandsForRate(rate), std::vector<float>(kBlockSize, k));
      std::vector<std::vector<float>> reference_block = block_to_insert;
      EXPECT_TRUE(delay_buffer->Insert(&block_to_insert));
      ASSERT_TRUE(delay_buffer->IsBlockAvailable());
      const std::vector<std::vector<float>>& output_block =
          delay_buffer->GetNext();
      EXPECT_EQ(reference_block, output_block);
    }
  }
}

// Verifies that the buffer passes the blocks in a bitexact manner when the
// delay is non-zero.
TEST(RenderDelayBuffer, BitexactnessWithNonZeroDelay) {
  constexpr size_t kMaxDelay = 200;
  for (auto rate : {8000, 16000, 32000, 48000}) {
    for (size_t delay = 0; delay < kMaxDelay; ++delay) {
      SCOPED_TRACE(ProduceDebugText(rate, delay));
      std::unique_ptr<RenderDelayBuffer> delay_buffer(RenderDelayBuffer::Create(
          20 + kMaxDelay, NumBandsForRate(rate), kMaxApiCallJitter));
      delay_buffer->SetDelay(delay);
      for (size_t k = 0; k < 200 + delay; ++k) {
        std::vector<std::vector<float>> block_to_insert(
            NumBandsForRate(rate), std::vector<float>(kBlockSize, k));
        EXPECT_TRUE(delay_buffer->Insert(&block_to_insert));
        ASSERT_TRUE(delay_buffer->IsBlockAvailable());
        const std::vector<std::vector<float>>& output_block =
            delay_buffer->GetNext();
        if (k >= delay) {
          std::vector<std::vector<float>> reference_block(
              NumBandsForRate(rate), std::vector<float>(kBlockSize, k - delay));
          EXPECT_EQ(reference_block, output_block);
        }
      }
    }
  }
}

// Verifies that the buffer passes the blocks in a bitexact manner when the
// delay is zero and there is jitter in the Insert and GetNext calls.
TEST(RenderDelayBuffer, BasicBitexactnessWithJitter) {
  for (auto rate : {8000, 16000, 32000, 48000}) {
    SCOPED_TRACE(ProduceDebugText(rate));
    std::unique_ptr<RenderDelayBuffer> delay_buffer(RenderDelayBuffer::Create(
        20, NumBandsForRate(rate), kMaxApiCallJitter));
    for (size_t k = 0; k < kMaxApiCallJitter; ++k) {
      std::vector<std::vector<float>> block_to_insert(
          NumBandsForRate(rate), std::vector<float>(kBlockSize, k));
      EXPECT_TRUE(delay_buffer->Insert(&block_to_insert));
    }

    for (size_t k = 0; k < kMaxApiCallJitter; ++k) {
      std::vector<std::vector<float>> reference_block(
          NumBandsForRate(rate), std::vector<float>(kBlockSize, k));
      ASSERT_TRUE(delay_buffer->IsBlockAvailable());
      const std::vector<std::vector<float>>& output_block =
          delay_buffer->GetNext();
      EXPECT_EQ(reference_block, output_block);
    }
    EXPECT_FALSE(delay_buffer->IsBlockAvailable());
  }
}

// Verifies that the buffer passes the blocks in a bitexact manner when the
// delay is non-zero and there is jitter in the Insert and GetNext calls.
TEST(RenderDelayBuffer, BitexactnessWithNonZeroDelayAndJitter) {
  constexpr size_t kMaxDelay = 200;
  for (auto rate : {8000, 16000, 32000, 48000}) {
    for (size_t delay = 0; delay < kMaxDelay; ++delay) {
      SCOPED_TRACE(ProduceDebugText(rate, delay));
      std::unique_ptr<RenderDelayBuffer> delay_buffer(RenderDelayBuffer::Create(
          20 + kMaxDelay, NumBandsForRate(rate), kMaxApiCallJitter));
      delay_buffer->SetDelay(delay);
      for (size_t j = 0; j < 10; ++j) {
        for (size_t k = 0; k < kMaxApiCallJitter; ++k) {
          const size_t block_value = k + j * kMaxApiCallJitter;
          std::vector<std::vector<float>> block_to_insert(
              NumBandsForRate(rate),
              std::vector<float>(kBlockSize, block_value));
          EXPECT_TRUE(delay_buffer->Insert(&block_to_insert));
        }

        for (size_t k = 0; k < kMaxApiCallJitter; ++k) {
          ASSERT_TRUE(delay_buffer->IsBlockAvailable());
          const std::vector<std::vector<float>>& output_block =
              delay_buffer->GetNext();
          const size_t block_value = k + j * kMaxApiCallJitter;
          if (block_value >= delay) {
            std::vector<std::vector<float>> reference_block(
                NumBandsForRate(rate),
                std::vector<float>(kBlockSize, block_value - delay));
            EXPECT_EQ(reference_block, output_block);
          }
        }
      }
    }
  }
}

// Verifies that no blocks present in the buffer are lost when the buffer is
// overflowed.
TEST(RenderDelayBuffer, BufferOverflowBitexactness) {
  for (auto rate : {8000, 16000, 32000, 48000}) {
    SCOPED_TRACE(ProduceDebugText(rate));
    std::unique_ptr<RenderDelayBuffer> delay_buffer(RenderDelayBuffer::Create(
        20, NumBandsForRate(rate), kMaxApiCallJitter));
    for (size_t k = 0; k < kMaxApiCallJitter; ++k) {
      std::vector<std::vector<float>> block_to_insert(
          NumBandsForRate(rate), std::vector<float>(kBlockSize, k));
      EXPECT_TRUE(delay_buffer->Insert(&block_to_insert));
    }

    std::vector<std::vector<float>> block_to_insert(
        NumBandsForRate(rate),
        std::vector<float>(kBlockSize, kMaxApiCallJitter + 1));
    auto block_to_insert_copy = block_to_insert;
    EXPECT_FALSE(delay_buffer->Insert(&block_to_insert));
    EXPECT_EQ(block_to_insert_copy, block_to_insert);

    for (size_t k = 0; k < kMaxApiCallJitter; ++k) {
      std::vector<std::vector<float>> reference_block(
          NumBandsForRate(rate), std::vector<float>(kBlockSize, k));
      ASSERT_TRUE(delay_buffer->IsBlockAvailable());
      const std::vector<std::vector<float>>& output_block =
          delay_buffer->GetNext();
      EXPECT_EQ(reference_block, output_block);
    }
    EXPECT_FALSE(delay_buffer->IsBlockAvailable());
  }
}

// Verifies that the buffer overflow is correctly reported.
TEST(RenderDelayBuffer, BufferOverflow) {
  for (auto rate : {8000, 16000, 32000, 48000}) {
    SCOPED_TRACE(ProduceDebugText(rate));
    std::unique_ptr<RenderDelayBuffer> delay_buffer(RenderDelayBuffer::Create(
        20, NumBandsForRate(rate), kMaxApiCallJitter));
    std::vector<std::vector<float>> block_to_insert(
        NumBandsForRate(rate), std::vector<float>(kBlockSize, 0.f));
    for (size_t k = 0; k < kMaxApiCallJitter; ++k) {
      EXPECT_TRUE(delay_buffer->Insert(&block_to_insert));
    }
    EXPECT_FALSE(delay_buffer->Insert(&block_to_insert));
  }
}

// Verifies that the check for available block works.
TEST(RenderDelayBuffer, AvailableBlock) {
  constexpr size_t kNumBands = 1;
  std::unique_ptr<RenderDelayBuffer> delay_buffer(
      RenderDelayBuffer::Create(20, kNumBands, kMaxApiCallJitter));
  EXPECT_FALSE(delay_buffer->IsBlockAvailable());
  std::vector<std::vector<float>> input_block(
      kNumBands, std::vector<float>(kBlockSize, 1.f));
  EXPECT_TRUE(delay_buffer->Insert(&input_block));
  ASSERT_TRUE(delay_buffer->IsBlockAvailable());
  delay_buffer->GetNext();
  EXPECT_FALSE(delay_buffer->IsBlockAvailable());
}

// Verifies that the maximum delay is computed correctly.
TEST(RenderDelayBuffer, MaxDelay) {
  for (size_t max_delay = 1; max_delay < 20; ++max_delay) {
    std::unique_ptr<RenderDelayBuffer> delay_buffer(
        RenderDelayBuffer::Create(max_delay, 1, kMaxApiCallJitter));
    EXPECT_EQ(max_delay, delay_buffer->MaxDelay());
  }
}

// Verifies the SetDelay method.
TEST(RenderDelayBuffer, SetDelay) {
  std::unique_ptr<RenderDelayBuffer> delay_buffer(
      RenderDelayBuffer::Create(20, 1, kMaxApiCallJitter));
  EXPECT_EQ(0u, delay_buffer->Delay());
  for (size_t delay = 0; delay < 20; ++delay) {
    delay_buffer->SetDelay(delay);
    EXPECT_EQ(delay, delay_buffer->Delay());
  }
}

#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)

// Verifies the check for null insert.
// TODO(peah): Re-enable the test once the issue with memory leaks during DEATH
// tests on test bots has been fixed.
TEST(RenderDelayBuffer, DISABLED_NullPointerInInsert) {
  std::unique_ptr<RenderDelayBuffer> delay_buffer(
      RenderDelayBuffer::Create(20, 1, kMaxApiCallJitter));
  EXPECT_DEATH(delay_buffer->Insert(nullptr), "");
}

// Verifies the check for feasible delay.
// TODO(peah): Re-enable the test once the issue with memory leaks during DEATH
// tests on test bots has been fixed.
TEST(RenderDelayBuffer, DISABLED_WrongDelay) {
  std::unique_ptr<RenderDelayBuffer> delay_buffer(
      RenderDelayBuffer::Create(20, 1, kMaxApiCallJitter));
  EXPECT_DEATH(delay_buffer->SetDelay(21), "");
}

// Verifies the check for the number of bands in the inserted blocks.
TEST(RenderDelayBuffer, WrongNumberOfBands) {
  for (auto rate : {16000, 32000, 48000}) {
    SCOPED_TRACE(ProduceDebugText(rate));
    std::unique_ptr<RenderDelayBuffer> delay_buffer(RenderDelayBuffer::Create(
        20, NumBandsForRate(rate), kMaxApiCallJitter));
    std::vector<std::vector<float>> block_to_insert(
        NumBandsForRate(rate < 48000 ? rate + 16000 : 16000),
        std::vector<float>(kBlockSize, 0.f));
    EXPECT_DEATH(delay_buffer->Insert(&block_to_insert), "");
  }
}

// Verifies the check of the length of the inserted blocks.
TEST(RenderDelayBuffer, WrongBlockLength) {
  for (auto rate : {8000, 16000, 32000, 48000}) {
    SCOPED_TRACE(ProduceDebugText(rate));
    std::unique_ptr<RenderDelayBuffer> delay_buffer(RenderDelayBuffer::Create(
        20, NumBandsForRate(rate), kMaxApiCallJitter));
    std::vector<std::vector<float>> block_to_insert(
        NumBandsForRate(rate), std::vector<float>(kBlockSize - 1, 0.f));
    EXPECT_DEATH(delay_buffer->Insert(&block_to_insert), "");
  }
}

// Verifies the behavior when getting a block from an empty buffer.
// TODO(peah): Re-enable the test once the issue with memory leaks during DEATH
// tests on test bots has been fixed.
TEST(RenderDelayBuffer, DISABLED_GetNextWithNoAvailableBlockVariant1) {
  std::unique_ptr<RenderDelayBuffer> delay_buffer(
      RenderDelayBuffer::Create(20, 1, kMaxApiCallJitter));
  EXPECT_DEATH(delay_buffer->GetNext(), "");
}

// Verifies the behavior when getting a block from an empty buffer.
// TODO(peah): Re-enable the test once the issue with memory leaks during DEATH
// tests on test bots has been fixed.
TEST(RenderDelayBuffer, DISABLED_GetNextWithNoAvailableBlockVariant2) {
  std::unique_ptr<RenderDelayBuffer> delay_buffer(
      RenderDelayBuffer::Create(20, 1, kMaxApiCallJitter));
  std::vector<std::vector<float>> input_block(
      1, std::vector<float>(kBlockSize, 1.f));
  EXPECT_TRUE(delay_buffer->Insert(&input_block));
  delay_buffer->GetNext();
  EXPECT_DEATH(delay_buffer->GetNext(), "");
}

#endif

}  // namespace webrtc
