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

std::string ProduceDebugText(int sample_rate_hz,
                             size_t delay,
                             size_t max_jitter) {
  std::ostringstream ss;
  ss << ProduceDebugText(sample_rate_hz, delay)
     << ", Max Api call jitter: " << max_jitter;
  return ss.str();
}

constexpr size_t kMaxApiCallJitter = 30;

}  // namespace

// Verifies the output of GetDelay when there are no AnalyzeRender calls.
TEST(RenderDelayController, NoRenderSignal) {
  std::vector<float> block(kBlockSize, 0.f);
  for (auto rate : {8000, 16000, 32000, 48000}) {
    SCOPED_TRACE(ProduceDebugText(rate));
    std::unique_ptr<RenderDelayBuffer> delay_buffer(RenderDelayBuffer::Create(
        250, NumBandsForRate(rate), kMaxApiCallJitter));
    std::unique_ptr<RenderDelayController> delay_controller(
        RenderDelayController::Create(rate, *delay_buffer));
    for (size_t k = 0; k < 100; ++k) {
      EXPECT_EQ(0u, delay_controller->GetDelay(block));
    }
  }
}

// Verifies the behavior when there are too many AnalyzeRender calls.
TEST(RenderDelayController, RenderOverflow) {
  std::vector<float> block(kBlockSize, 0.f);
  for (auto rate : {8000, 16000, 32000, 48000}) {
    SCOPED_TRACE(ProduceDebugText(rate));
    std::unique_ptr<RenderDelayBuffer> delay_buffer(RenderDelayBuffer::Create(
        250, NumBandsForRate(rate), kMaxApiCallJitter));
    std::unique_ptr<RenderDelayController> delay_controller(
        RenderDelayController::Create(rate, *delay_buffer));
    for (size_t k = 0; k < kMaxApiCallJitter; ++k) {
      EXPECT_TRUE(delay_controller->AnalyzeRender(block));
    }
    EXPECT_FALSE(delay_controller->AnalyzeRender(block));
    delay_controller->GetDelay(block);
    EXPECT_TRUE(delay_controller->AnalyzeRender(block));
  }
}

// Verifies the basic API call sequence.
TEST(RenderDelayController, BasicApiCalls) {
  std::vector<float> render_block(kBlockSize, 0.f);
  std::vector<float> capture_block(kBlockSize, 0.f);
  size_t delay_blocks = 0;
  for (auto rate : {8000, 16000, 32000, 48000}) {
    std::unique_ptr<RenderDelayBuffer> render_delay_buffer(
        RenderDelayBuffer::Create(50, NumBandsForRate(rate),
                                  kMaxApiCallJitter));
    std::unique_ptr<RenderDelayController> delay_controller(
        RenderDelayController::Create(rate, *render_delay_buffer));
    for (size_t k = 0; k < 10; ++k) {
      EXPECT_TRUE(delay_controller->AnalyzeRender(render_block));
      delay_blocks = delay_controller->GetDelay(capture_block);
    }
    EXPECT_FALSE(delay_controller->AlignmentHeadroomSamples());
    EXPECT_EQ(0u, delay_blocks);
  }
}

// Verifies that the RenderDelayController is able to align the signals for
// simple timeshifts between the signals.
TEST(RenderDelayController, Alignment) {
  Random random_generator(42U);
  std::vector<float> render_block(kBlockSize, 0.f);
  std::vector<float> capture_block(kBlockSize, 0.f);
  size_t delay_blocks = 0;
  for (auto rate : {8000, 16000, 32000, 48000}) {
    for (size_t delay_samples : {15, 50, 150, 200, 800, 4000}) {
      SCOPED_TRACE(ProduceDebugText(rate, delay_samples));
      std::unique_ptr<RenderDelayBuffer> render_delay_buffer(
          RenderDelayBuffer::Create(250, NumBandsForRate(rate),
                                    kMaxApiCallJitter));
      std::unique_ptr<RenderDelayController> delay_controller(
          RenderDelayController::Create(rate, *render_delay_buffer));
      DelayBuffer<float> signal_delay_buffer(delay_samples);
      for (size_t k = 0; k < (400 + delay_samples / kBlockSize); ++k) {
        RandomizeSampleVector(&random_generator, render_block);
        signal_delay_buffer.Delay(render_block, capture_block);
        EXPECT_TRUE(delay_controller->AnalyzeRender(render_block));
        delay_blocks = delay_controller->GetDelay(capture_block);
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
  std::vector<float> render_block(kBlockSize, 0.f);
  std::vector<float> capture_block(kBlockSize, 0.f);
  for (auto rate : {8000, 16000, 32000, 48000}) {
    for (size_t delay_samples : {15, 50, 800}) {
      for (size_t max_jitter : {1, 9, 20}) {
        size_t delay_blocks = 0;
        SCOPED_TRACE(ProduceDebugText(rate, delay_samples, max_jitter));
        std::unique_ptr<RenderDelayBuffer> render_delay_buffer(
            RenderDelayBuffer::Create(250, NumBandsForRate(rate), max_jitter));
        std::unique_ptr<RenderDelayController> delay_controller(
            RenderDelayController::Create(rate, *render_delay_buffer));
        DelayBuffer<float> signal_delay_buffer(delay_samples);
        for (size_t j = 0;
             j < (300 + delay_samples / kBlockSize) / max_jitter + 1; ++j) {
          std::vector<std::vector<float>> capture_block_buffer;
          for (size_t k = 0; k < max_jitter; ++k) {
            RandomizeSampleVector(&random_generator, render_block);
            signal_delay_buffer.Delay(render_block, capture_block);
            capture_block_buffer.push_back(capture_block);
            EXPECT_TRUE(delay_controller->AnalyzeRender(render_block));
          }
          for (size_t k = 0; k < max_jitter; ++k) {
            delay_blocks = delay_controller->GetDelay(capture_block_buffer[k]);
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
        EXPECT_NEAR(delay_samples - delay_blocks * kBlockSize,
                    *headroom_samples, 4);
      }
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
        RenderDelayBuffer::Create(250, NumBandsForRate(rate),
                                  kMaxApiCallJitter));
    std::unique_ptr<RenderDelayController> delay_controller(
        RenderDelayController::Create(rate, *render_delay_buffer));
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
        RenderDelayBuffer::Create(250, NumBandsForRate(rate),
                                  kMaxApiCallJitter));
    EXPECT_DEATH(std::unique_ptr<RenderDelayController>(
                     RenderDelayController::Create(rate, *render_delay_buffer))
                     ->GetDelay(block),
                 "");
  }
}

// Verifies the check for the render signal block size.
// TODO(peah): Re-enable the test once the issue with memory leaks during DEATH
// tests on test bots has been fixed.
TEST(RenderDelayController, DISABLED_WrongRenderSize) {
  std::vector<float> block(kBlockSize - 1, 0.f);
  for (auto rate : {8000, 16000, 32000, 48000}) {
    SCOPED_TRACE(ProduceDebugText(rate));
    std::unique_ptr<RenderDelayBuffer> render_delay_buffer(
        RenderDelayBuffer::Create(250, NumBandsForRate(rate),
                                  kMaxApiCallJitter));
    EXPECT_DEATH(std::unique_ptr<RenderDelayController>(
                     RenderDelayController::Create(rate, *render_delay_buffer))
                     ->AnalyzeRender(block),
                 "");
  }
}

// Verifies the check for correct sample rate.
TEST(RenderDelayController, WrongSampleRate) {
  for (auto rate : {-1, 0, 8001, 16001}) {
    SCOPED_TRACE(ProduceDebugText(rate));
    std::unique_ptr<RenderDelayBuffer> render_delay_buffer(
        RenderDelayBuffer::Create(10, NumBandsForRate(rate),
                                  kMaxApiCallJitter));
    EXPECT_DEATH(std::unique_ptr<RenderDelayController>(
                     RenderDelayController::Create(rate, *render_delay_buffer)),
                 "");
  }
}

#endif

}  // namespace webrtc
