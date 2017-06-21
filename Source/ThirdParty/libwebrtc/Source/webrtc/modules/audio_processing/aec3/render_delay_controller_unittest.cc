/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_processing/aec3/render_delay_controller.h"

#include <algorithm>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "webrtc/base/random.h"
#include "webrtc/modules/audio_processing/aec3/aec3_common.h"
#include "webrtc/modules/audio_processing/aec3/block_processor.h"
#include "webrtc/modules/audio_processing/aec3/decimator_by_4.h"
#include "webrtc/modules/audio_processing/aec3/render_delay_buffer.h"
#include "webrtc/modules/audio_processing/logging/apm_data_dumper.h"
#include "webrtc/modules/audio_processing/test/echo_canceller_test_tools.h"
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
  ss << ProduceDebugText(sample_rate_hz) << ", Delay: " << delay;
  return ss.str();
}

}  // namespace

// Verifies the output of GetDelay when there are no AnalyzeRender calls.
TEST(RenderDelayController, NoRenderSignal) {
  std::vector<float> block(kBlockSize, 0.f);
  for (auto rate : {8000, 16000, 32000, 48000}) {
    SCOPED_TRACE(ProduceDebugText(rate));
    std::unique_ptr<RenderDelayBuffer> delay_buffer(
        RenderDelayBuffer::Create(NumBandsForRate(rate)));
    std::unique_ptr<RenderDelayController> delay_controller(
        RenderDelayController::Create(rate));
    for (size_t k = 0; k < 100; ++k) {
      EXPECT_EQ(0u, delay_controller->GetDelay(
                        delay_buffer->GetDownsampledRenderBuffer(), block));
    }
  }
}

// Verifies the basic API call sequence.
TEST(RenderDelayController, BasicApiCalls) {
  std::vector<float> capture_block(kBlockSize, 0.f);
  size_t delay_blocks = 0;
  for (auto rate : {8000, 16000, 32000, 48000}) {
    std::vector<std::vector<float>> render_block(
        NumBandsForRate(rate), std::vector<float>(kBlockSize, 0.f));
    std::unique_ptr<RenderDelayBuffer> render_delay_buffer(
        RenderDelayBuffer::Create(NumBandsForRate(rate)));
    std::unique_ptr<RenderDelayController> delay_controller(
        RenderDelayController::Create(rate));
    for (size_t k = 0; k < 10; ++k) {
      render_delay_buffer->Insert(render_block);
      render_delay_buffer->UpdateBuffers();
      delay_blocks = delay_controller->GetDelay(
          render_delay_buffer->GetDownsampledRenderBuffer(), capture_block);
    }
    EXPECT_FALSE(delay_controller->AlignmentHeadroomSamples());
    EXPECT_EQ(0u, delay_blocks);
  }
}

// Verifies that the RenderDelayController is able to align the signals for
// simple timeshifts between the signals.
TEST(RenderDelayController, Alignment) {
  Random random_generator(42U);
  std::vector<float> capture_block(kBlockSize, 0.f);
  size_t delay_blocks = 0;
  for (auto rate : {8000, 16000, 32000, 48000}) {
    std::vector<std::vector<float>> render_block(
        NumBandsForRate(rate), std::vector<float>(kBlockSize, 0.f));

    for (size_t delay_samples : {15, 50, 150, 200, 800, 4000}) {
      SCOPED_TRACE(ProduceDebugText(rate, delay_samples));
      std::unique_ptr<RenderDelayBuffer> render_delay_buffer(
          RenderDelayBuffer::Create(NumBandsForRate(rate)));
      std::unique_ptr<RenderDelayController> delay_controller(
          RenderDelayController::Create(rate));
      DelayBuffer<float> signal_delay_buffer(delay_samples);
      for (size_t k = 0; k < (400 + delay_samples / kBlockSize); ++k) {
        RandomizeSampleVector(&random_generator, render_block[0]);
        signal_delay_buffer.Delay(render_block[0], capture_block);
        render_delay_buffer->Insert(render_block);
        render_delay_buffer->UpdateBuffers();
        delay_blocks = delay_controller->GetDelay(
            render_delay_buffer->GetDownsampledRenderBuffer(), capture_block);
      }

      constexpr int kDelayHeadroomBlocks = 1;
      size_t expected_delay_blocks =
          std::max(0, static_cast<int>(delay_samples / kBlockSize) -
                          kDelayHeadroomBlocks);
      if (expected_delay_blocks < 2) {
        expected_delay_blocks = 0;
      }

      EXPECT_EQ(expected_delay_blocks, delay_blocks);

      const rtc::Optional<size_t> headroom_samples =
          delay_controller->AlignmentHeadroomSamples();
      ASSERT_TRUE(headroom_samples);
      EXPECT_NEAR(delay_samples - delay_blocks * kBlockSize, *headroom_samples,
                  4);
    }
  }
}

// Verifies that the RenderDelayController is able to align the signals for
// simple timeshifts between the signals when there is jitter in the API calls.
TEST(RenderDelayController, AlignmentWithJitter) {
  Random random_generator(42U);
  std::vector<float> capture_block(kBlockSize, 0.f);
  for (auto rate : {8000, 16000, 32000, 48000}) {
    std::vector<std::vector<float>> render_block(
        NumBandsForRate(rate), std::vector<float>(kBlockSize, 0.f));
    for (size_t delay_samples : {15, 50, 300, 800}) {
      size_t delay_blocks = 0;
      SCOPED_TRACE(ProduceDebugText(rate, delay_samples));
      std::unique_ptr<RenderDelayBuffer> render_delay_buffer(
          RenderDelayBuffer::Create(NumBandsForRate(rate)));
      std::unique_ptr<RenderDelayController> delay_controller(
          RenderDelayController::Create(rate));
      DelayBuffer<float> signal_delay_buffer(delay_samples);
      for (size_t j = 0;
           j <
           (1000 + delay_samples / kBlockSize) / kMaxApiCallsJitterBlocks + 1;
           ++j) {
        std::vector<std::vector<float>> capture_block_buffer;
        for (size_t k = 0; k < (kMaxApiCallsJitterBlocks - 1); ++k) {
          RandomizeSampleVector(&random_generator, render_block[0]);
          signal_delay_buffer.Delay(render_block[0], capture_block);
          capture_block_buffer.push_back(capture_block);
          render_delay_buffer->Insert(render_block);
        }
        for (size_t k = 0; k < (kMaxApiCallsJitterBlocks - 1); ++k) {
          render_delay_buffer->UpdateBuffers();
          delay_blocks = delay_controller->GetDelay(
              render_delay_buffer->GetDownsampledRenderBuffer(),
              capture_block_buffer[k]);
        }
      }

      constexpr int kDelayHeadroomBlocks = 1;
      size_t expected_delay_blocks =
          std::max(0, static_cast<int>(delay_samples / kBlockSize) -
                          kDelayHeadroomBlocks);
      if (expected_delay_blocks < 2) {
        expected_delay_blocks = 0;
      }

      EXPECT_EQ(expected_delay_blocks, delay_blocks);

      const rtc::Optional<size_t> headroom_samples =
          delay_controller->AlignmentHeadroomSamples();
      ASSERT_TRUE(headroom_samples);
      EXPECT_NEAR(delay_samples - delay_blocks * kBlockSize, *headroom_samples,
                  4);
      }
    }
  }

// Verifies the initial value for the AlignmentHeadroomSamples.
TEST(RenderDelayController, InitialHeadroom) {
  std::vector<float> render_block(kBlockSize, 0.f);
  std::vector<float> capture_block(kBlockSize, 0.f);
  for (auto rate : {8000, 16000, 32000, 48000}) {
    SCOPED_TRACE(ProduceDebugText(rate));
    std::unique_ptr<RenderDelayBuffer> render_delay_buffer(
        RenderDelayBuffer::Create(NumBandsForRate(rate)));
    std::unique_ptr<RenderDelayController> delay_controller(
        RenderDelayController::Create(rate));
    EXPECT_FALSE(delay_controller->AlignmentHeadroomSamples());
  }
}

#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)

// Verifies the check for the capture signal block size.
TEST(RenderDelayController, WrongCaptureSize) {
  std::vector<float> block(kBlockSize - 1, 0.f);
  for (auto rate : {8000, 16000, 32000, 48000}) {
    SCOPED_TRACE(ProduceDebugText(rate));
    std::unique_ptr<RenderDelayBuffer> render_delay_buffer(
        RenderDelayBuffer::Create(NumBandsForRate(rate)));
    EXPECT_DEATH(
        std::unique_ptr<RenderDelayController>(
            RenderDelayController::Create(rate))
            ->GetDelay(render_delay_buffer->GetDownsampledRenderBuffer(),
                       block),
        "");
  }
}

// Verifies the check for correct sample rate.
// TODO(peah): Re-enable the test once the issue with memory leaks during DEATH
// tests on test bots has been fixed.
TEST(RenderDelayController, DISABLED_WrongSampleRate) {
  for (auto rate : {-1, 0, 8001, 16001}) {
    SCOPED_TRACE(ProduceDebugText(rate));
    std::unique_ptr<RenderDelayBuffer> render_delay_buffer(
        RenderDelayBuffer::Create(NumBandsForRate(rate)));
    EXPECT_DEATH(std::unique_ptr<RenderDelayController>(
                     RenderDelayController::Create(rate)),
                 "");
  }
}

#endif

}  // namespace webrtc
