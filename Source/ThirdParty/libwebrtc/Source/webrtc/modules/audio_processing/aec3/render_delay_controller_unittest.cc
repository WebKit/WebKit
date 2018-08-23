/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/render_delay_controller.h"

#include <algorithm>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "modules/audio_processing/aec3/aec3_common.h"
#include "modules/audio_processing/aec3/block_processor.h"
#include "modules/audio_processing/aec3/decimator.h"
#include "modules/audio_processing/aec3/render_delay_buffer.h"
#include "modules/audio_processing/logging/apm_data_dumper.h"
#include "modules/audio_processing/test/echo_canceller_test_tools.h"
#include "rtc_base/random.h"
#include "test/gtest.h"

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

constexpr size_t kDownSamplingFactors[] = {2, 4, 8};

}  // namespace

// Verifies the output of GetDelay when there are no AnalyzeRender calls.
TEST(RenderDelayController, NoRenderSignal) {
  std::vector<float> block(kBlockSize, 0.f);
  EchoCanceller3Config config;
  absl::optional<int> echo_remover_delay_;
  for (size_t num_matched_filters = 4; num_matched_filters == 10;
       num_matched_filters++) {
    for (auto down_sampling_factor : kDownSamplingFactors) {
      config.delay.down_sampling_factor = down_sampling_factor;
      config.delay.num_filters = num_matched_filters;
      for (auto rate : {8000, 16000, 32000, 48000}) {
        SCOPED_TRACE(ProduceDebugText(rate));
        std::unique_ptr<RenderDelayBuffer> delay_buffer(
            RenderDelayBuffer::Create(config, NumBandsForRate(rate)));
        std::unique_ptr<RenderDelayController> delay_controller(
            RenderDelayController::Create(
                config, RenderDelayBuffer::DelayEstimatorOffset(config), rate));
        for (size_t k = 0; k < 100; ++k) {
          auto delay = delay_controller->GetDelay(
              delay_buffer->GetDownsampledRenderBuffer(), delay_buffer->Delay(),
              echo_remover_delay_, block);
          EXPECT_EQ(config.delay.min_echo_path_delay_blocks, delay->delay);
        }
      }
    }
  }
}

// Verifies the basic API call sequence.
TEST(RenderDelayController, BasicApiCalls) {
  std::vector<float> capture_block(kBlockSize, 0.f);
  absl::optional<DelayEstimate> delay_blocks;
  absl::optional<int> echo_remover_delay;
  for (size_t num_matched_filters = 4; num_matched_filters == 10;
       num_matched_filters++) {
    for (auto down_sampling_factor : kDownSamplingFactors) {
      EchoCanceller3Config config;
      config.delay.down_sampling_factor = down_sampling_factor;
      config.delay.num_filters = num_matched_filters;
      for (auto rate : {8000, 16000, 32000, 48000}) {
        std::vector<std::vector<float>> render_block(
            NumBandsForRate(rate), std::vector<float>(kBlockSize, 0.f));
        std::unique_ptr<RenderDelayBuffer> render_delay_buffer(
            RenderDelayBuffer::Create(config, NumBandsForRate(rate)));
        std::unique_ptr<RenderDelayController> delay_controller(
            RenderDelayController::Create(
                EchoCanceller3Config(),
                RenderDelayBuffer::DelayEstimatorOffset(config), rate));
        for (size_t k = 0; k < 10; ++k) {
          render_delay_buffer->Insert(render_block);
          render_delay_buffer->PrepareCaptureProcessing();

          delay_blocks = delay_controller->GetDelay(
              render_delay_buffer->GetDownsampledRenderBuffer(),
              render_delay_buffer->Delay(), echo_remover_delay, capture_block);
        }
        EXPECT_TRUE(delay_blocks);
        EXPECT_EQ(config.delay.min_echo_path_delay_blocks, delay_blocks->delay);
      }
    }
  }
}

// Verifies that the RenderDelayController is able to align the signals for
// simple timeshifts between the signals.
TEST(RenderDelayController, Alignment) {
  Random random_generator(42U);
  absl::optional<int> echo_remover_delay;
  std::vector<float> capture_block(kBlockSize, 0.f);
  for (size_t num_matched_filters = 4; num_matched_filters == 10;
       num_matched_filters++) {
    for (auto down_sampling_factor : kDownSamplingFactors) {
      EchoCanceller3Config config;
      config.delay.down_sampling_factor = down_sampling_factor;
      config.delay.num_filters = num_matched_filters;

      for (auto rate : {8000, 16000, 32000, 48000}) {
        std::vector<std::vector<float>> render_block(
            NumBandsForRate(rate), std::vector<float>(kBlockSize, 0.f));

        for (size_t delay_samples : {15, 50, 150, 200, 800, 4000}) {
          absl::optional<DelayEstimate> delay_blocks;
          SCOPED_TRACE(ProduceDebugText(rate, delay_samples));
          std::unique_ptr<RenderDelayBuffer> render_delay_buffer(
              RenderDelayBuffer::Create(config, NumBandsForRate(rate)));
          std::unique_ptr<RenderDelayController> delay_controller(
              RenderDelayController::Create(
                  config, RenderDelayBuffer::DelayEstimatorOffset(config),
                  rate));
          DelayBuffer<float> signal_delay_buffer(delay_samples);
          for (size_t k = 0; k < (400 + delay_samples / kBlockSize); ++k) {
            RandomizeSampleVector(&random_generator, render_block[0]);
            signal_delay_buffer.Delay(render_block[0], capture_block);
            render_delay_buffer->Insert(render_block);
            render_delay_buffer->PrepareCaptureProcessing();
            delay_blocks = delay_controller->GetDelay(
                render_delay_buffer->GetDownsampledRenderBuffer(),
                render_delay_buffer->Delay(), echo_remover_delay,
                capture_block);
          }
          ASSERT_TRUE(!!delay_blocks);

          constexpr int kDelayHeadroomBlocks = 1;
          size_t expected_delay_blocks =
              std::max(0, static_cast<int>(delay_samples / kBlockSize) -
                              kDelayHeadroomBlocks);

          EXPECT_EQ(expected_delay_blocks, delay_blocks->delay);
        }
      }
    }
  }
}

// Verifies that the RenderDelayController is able to properly handle noncausal
// delays.
TEST(RenderDelayController, NonCausalAlignment) {
  Random random_generator(42U);
  absl::optional<int> echo_remover_delay;
  for (size_t num_matched_filters = 4; num_matched_filters == 10;
       num_matched_filters++) {
    for (auto down_sampling_factor : kDownSamplingFactors) {
      EchoCanceller3Config config;
      config.delay.down_sampling_factor = down_sampling_factor;
      config.delay.num_filters = num_matched_filters;
      for (auto rate : {8000, 16000, 32000, 48000}) {
        std::vector<std::vector<float>> render_block(
            NumBandsForRate(rate), std::vector<float>(kBlockSize, 0.f));
        std::vector<std::vector<float>> capture_block(
            NumBandsForRate(rate), std::vector<float>(kBlockSize, 0.f));

        for (int delay_samples : {-15, -50, -150, -200}) {
          absl::optional<DelayEstimate> delay_blocks;
          SCOPED_TRACE(ProduceDebugText(rate, -delay_samples));
          std::unique_ptr<RenderDelayBuffer> render_delay_buffer(
              RenderDelayBuffer::Create(config, NumBandsForRate(rate)));
          std::unique_ptr<RenderDelayController> delay_controller(
              RenderDelayController::Create(
                  EchoCanceller3Config(),
                  RenderDelayBuffer::DelayEstimatorOffset(config), rate));
          DelayBuffer<float> signal_delay_buffer(-delay_samples);
          for (int k = 0;
               k < (400 - delay_samples / static_cast<int>(kBlockSize)); ++k) {
            RandomizeSampleVector(&random_generator, capture_block[0]);
            signal_delay_buffer.Delay(capture_block[0], render_block[0]);
            render_delay_buffer->Insert(render_block);
            render_delay_buffer->PrepareCaptureProcessing();
            delay_blocks = delay_controller->GetDelay(
                render_delay_buffer->GetDownsampledRenderBuffer(),
                render_delay_buffer->Delay(), echo_remover_delay,
                capture_block[0]);
          }

          ASSERT_FALSE(delay_blocks);
        }
      }
    }
  }
}

// Verifies that the RenderDelayController is able to align the signals for
// simple timeshifts between the signals when there is jitter in the API calls.
TEST(RenderDelayController, AlignmentWithJitter) {
  Random random_generator(42U);
  absl::optional<int> echo_remover_delay;
  std::vector<float> capture_block(kBlockSize, 0.f);
  for (size_t num_matched_filters = 4; num_matched_filters == 10;
       num_matched_filters++) {
    for (auto down_sampling_factor : kDownSamplingFactors) {
      EchoCanceller3Config config;
      config.delay.down_sampling_factor = down_sampling_factor;
      config.delay.num_filters = num_matched_filters;
      for (auto rate : {8000, 16000, 32000, 48000}) {
        std::vector<std::vector<float>> render_block(
            NumBandsForRate(rate), std::vector<float>(kBlockSize, 0.f));
        for (size_t delay_samples : {15, 50, 300, 800}) {
          absl::optional<DelayEstimate> delay_blocks;
          SCOPED_TRACE(ProduceDebugText(rate, delay_samples));
          std::unique_ptr<RenderDelayBuffer> render_delay_buffer(
              RenderDelayBuffer::Create(config, NumBandsForRate(rate)));
          std::unique_ptr<RenderDelayController> delay_controller(
              RenderDelayController::Create(
                  config, RenderDelayBuffer::DelayEstimatorOffset(config),
                  rate));
          DelayBuffer<float> signal_delay_buffer(delay_samples);
          for (size_t j = 0; j < (1000 + delay_samples / kBlockSize) /
                                         config.delay.api_call_jitter_blocks +
                                     1;
               ++j) {
            std::vector<std::vector<float>> capture_block_buffer;
            for (size_t k = 0; k < (config.delay.api_call_jitter_blocks - 1);
                 ++k) {
              RandomizeSampleVector(&random_generator, render_block[0]);
              signal_delay_buffer.Delay(render_block[0], capture_block);
              capture_block_buffer.push_back(capture_block);
              render_delay_buffer->Insert(render_block);
            }
            for (size_t k = 0; k < (config.delay.api_call_jitter_blocks - 1);
                 ++k) {
              render_delay_buffer->PrepareCaptureProcessing();
              delay_blocks = delay_controller->GetDelay(
                  render_delay_buffer->GetDownsampledRenderBuffer(),
                  render_delay_buffer->Delay(), echo_remover_delay,
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

          ASSERT_TRUE(delay_blocks);
          EXPECT_EQ(expected_delay_blocks, delay_blocks->delay);
        }
      }
    }
  }
}

// Verifies the initial value for the AlignmentHeadroomSamples.
TEST(RenderDelayController, InitialHeadroom) {
  std::vector<float> render_block(kBlockSize, 0.f);
  std::vector<float> capture_block(kBlockSize, 0.f);
  for (size_t num_matched_filters = 4; num_matched_filters == 10;
       num_matched_filters++) {
    for (auto down_sampling_factor : kDownSamplingFactors) {
      EchoCanceller3Config config;
      config.delay.down_sampling_factor = down_sampling_factor;
      config.delay.num_filters = num_matched_filters;
      for (auto rate : {8000, 16000, 32000, 48000}) {
        SCOPED_TRACE(ProduceDebugText(rate));
        std::unique_ptr<RenderDelayBuffer> render_delay_buffer(
            RenderDelayBuffer::Create(config, NumBandsForRate(rate)));

        std::unique_ptr<RenderDelayController> delay_controller(
            RenderDelayController::Create(
                config, RenderDelayBuffer::DelayEstimatorOffset(config), rate));
      }
    }
  }
}

#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)

// Verifies the check for the capture signal block size.
TEST(RenderDelayController, WrongCaptureSize) {
  std::vector<float> block(kBlockSize - 1, 0.f);
  EchoCanceller3Config config;
  absl::optional<int> echo_remover_delay;
  for (auto rate : {8000, 16000, 32000, 48000}) {
    SCOPED_TRACE(ProduceDebugText(rate));
    std::unique_ptr<RenderDelayBuffer> render_delay_buffer(
        RenderDelayBuffer::Create(config, NumBandsForRate(rate)));
    EXPECT_DEATH(
        std::unique_ptr<RenderDelayController>(
            RenderDelayController::Create(
                EchoCanceller3Config(),
                RenderDelayBuffer::DelayEstimatorOffset(config), rate))
            ->GetDelay(render_delay_buffer->GetDownsampledRenderBuffer(),
                       render_delay_buffer->Delay(), echo_remover_delay, block),
        "");
  }
}

// Verifies the check for correct sample rate.
// TODO(peah): Re-enable the test once the issue with memory leaks during DEATH
// tests on test bots has been fixed.
TEST(RenderDelayController, DISABLED_WrongSampleRate) {
  for (auto rate : {-1, 0, 8001, 16001}) {
    SCOPED_TRACE(ProduceDebugText(rate));
    EchoCanceller3Config config;
    std::unique_ptr<RenderDelayBuffer> render_delay_buffer(
        RenderDelayBuffer::Create(config, NumBandsForRate(rate)));
    EXPECT_DEATH(
        std::unique_ptr<RenderDelayController>(RenderDelayController::Create(
            EchoCanceller3Config(),
            RenderDelayBuffer::DelayEstimatorOffset(config), rate)),
        "");
  }
}

#endif

}  // namespace webrtc
