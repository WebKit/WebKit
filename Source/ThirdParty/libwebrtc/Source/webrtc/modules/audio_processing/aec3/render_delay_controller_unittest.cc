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
#include <string>
#include <vector>

#include "modules/audio_processing/aec3/aec3_common.h"
#include "modules/audio_processing/aec3/block_processor.h"
#include "modules/audio_processing/aec3/decimator.h"
#include "modules/audio_processing/aec3/render_delay_buffer.h"
#include "modules/audio_processing/logging/apm_data_dumper.h"
#include "modules/audio_processing/test/echo_canceller_test_tools.h"
#include "rtc_base/random.h"
#include "rtc_base/strings/string_builder.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

std::string ProduceDebugText(int sample_rate_hz) {
  rtc::StringBuilder ss;
  ss << "Sample rate: " << sample_rate_hz;
  return ss.Release();
}

std::string ProduceDebugText(int sample_rate_hz,
                             size_t delay,
                             size_t num_render_channels,
                             size_t num_capture_channels) {
  rtc::StringBuilder ss;
  ss << ProduceDebugText(sample_rate_hz) << ", Delay: " << delay
     << ", Num render channels: " << num_render_channels
     << ", Num capture channels: " << num_capture_channels;
  return ss.Release();
}

constexpr size_t kDownSamplingFactors[] = {2, 4, 8};

}  // namespace

// Verifies the output of GetDelay when there are no AnalyzeRender calls.
// TODO(bugs.webrtc.org/11161): Re-enable tests.
TEST(RenderDelayController, DISABLED_NoRenderSignal) {
  for (size_t num_render_channels : {1, 2, 8}) {
    Block block(/*num_bands=1*/ 1, /*num_channels=*/1);
    EchoCanceller3Config config;
    for (size_t num_matched_filters = 4; num_matched_filters <= 10;
         num_matched_filters++) {
      for (auto down_sampling_factor : kDownSamplingFactors) {
        config.delay.down_sampling_factor = down_sampling_factor;
        config.delay.num_filters = num_matched_filters;
        for (auto rate : {16000, 32000, 48000}) {
          SCOPED_TRACE(ProduceDebugText(rate));
          std::unique_ptr<RenderDelayBuffer> delay_buffer(
              RenderDelayBuffer::Create(config, rate, num_render_channels));
          std::unique_ptr<RenderDelayController> delay_controller(
              RenderDelayController::Create(config, rate,
                                            /*num_capture_channels*/ 1));
          for (size_t k = 0; k < 100; ++k) {
            auto delay = delay_controller->GetDelay(
                delay_buffer->GetDownsampledRenderBuffer(),
                delay_buffer->Delay(), block);
            EXPECT_FALSE(delay->delay);
          }
        }
      }
    }
  }
}

// Verifies the basic API call sequence.
// TODO(bugs.webrtc.org/11161): Re-enable tests.
TEST(RenderDelayController, DISABLED_BasicApiCalls) {
  for (size_t num_capture_channels : {1, 2, 4}) {
    for (size_t num_render_channels : {1, 2, 8}) {
      Block capture_block(/*num_bands=*/1, num_capture_channels);
      absl::optional<DelayEstimate> delay_blocks;
      for (size_t num_matched_filters = 4; num_matched_filters <= 10;
           num_matched_filters++) {
        for (auto down_sampling_factor : kDownSamplingFactors) {
          EchoCanceller3Config config;
          config.delay.down_sampling_factor = down_sampling_factor;
          config.delay.num_filters = num_matched_filters;
          config.delay.capture_alignment_mixing.downmix = false;
          config.delay.capture_alignment_mixing.adaptive_selection = false;

          for (auto rate : {16000, 32000, 48000}) {
            Block render_block(NumBandsForRate(rate), num_render_channels);
            std::unique_ptr<RenderDelayBuffer> render_delay_buffer(
                RenderDelayBuffer::Create(config, rate, num_render_channels));
            std::unique_ptr<RenderDelayController> delay_controller(
                RenderDelayController::Create(EchoCanceller3Config(), rate,
                                              num_capture_channels));
            for (size_t k = 0; k < 10; ++k) {
              render_delay_buffer->Insert(render_block);
              render_delay_buffer->PrepareCaptureProcessing();

              delay_blocks = delay_controller->GetDelay(
                  render_delay_buffer->GetDownsampledRenderBuffer(),
                  render_delay_buffer->Delay(), capture_block);
            }
            EXPECT_TRUE(delay_blocks);
            EXPECT_FALSE(delay_blocks->delay);
          }
        }
      }
    }
  }
}

// Verifies that the RenderDelayController is able to align the signals for
// simple timeshifts between the signals.
// TODO(bugs.webrtc.org/11161): Re-enable tests.
TEST(RenderDelayController, DISABLED_Alignment) {
  Random random_generator(42U);
  for (size_t num_capture_channels : {1, 2, 4}) {
    Block capture_block(/*num_bands=*/1, num_capture_channels);
    for (size_t num_matched_filters = 4; num_matched_filters <= 10;
         num_matched_filters++) {
      for (auto down_sampling_factor : kDownSamplingFactors) {
        EchoCanceller3Config config;
        config.delay.down_sampling_factor = down_sampling_factor;
        config.delay.num_filters = num_matched_filters;
        config.delay.capture_alignment_mixing.downmix = false;
        config.delay.capture_alignment_mixing.adaptive_selection = false;

        for (size_t num_render_channels : {1, 2, 8}) {
          for (auto rate : {16000, 32000, 48000}) {
            Block render_block(NumBandsForRate(rate), num_render_channels);

            for (size_t delay_samples : {15, 50, 150, 200, 800, 4000}) {
              absl::optional<DelayEstimate> delay_blocks;
              SCOPED_TRACE(ProduceDebugText(rate, delay_samples,
                                            num_render_channels,
                                            num_capture_channels));
              std::unique_ptr<RenderDelayBuffer> render_delay_buffer(
                  RenderDelayBuffer::Create(config, rate, num_render_channels));
              std::unique_ptr<RenderDelayController> delay_controller(
                  RenderDelayController::Create(config, rate,
                                                num_capture_channels));
              DelayBuffer<float> signal_delay_buffer(delay_samples);
              for (size_t k = 0; k < (400 + delay_samples / kBlockSize); ++k) {
                for (int band = 0; band < render_block.NumBands(); ++band) {
                  for (int channel = 0; channel < render_block.NumChannels();
                       ++channel) {
                    RandomizeSampleVector(&random_generator,
                                          render_block.View(band, channel));
                  }
                }
                signal_delay_buffer.Delay(
                    render_block.View(/*band=*/0, /*channel=*/0),
                    capture_block.View(/*band=*/0, /*channel=*/0));
                render_delay_buffer->Insert(render_block);
                render_delay_buffer->PrepareCaptureProcessing();
                delay_blocks = delay_controller->GetDelay(
                    render_delay_buffer->GetDownsampledRenderBuffer(),
                    render_delay_buffer->Delay(), capture_block);
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
  }
}

// Verifies that the RenderDelayController is able to properly handle noncausal
// delays.
// TODO(bugs.webrtc.org/11161): Re-enable tests.
TEST(RenderDelayController, DISABLED_NonCausalAlignment) {
  Random random_generator(42U);
  for (size_t num_capture_channels : {1, 2, 4}) {
    for (size_t num_render_channels : {1, 2, 8}) {
      for (size_t num_matched_filters = 4; num_matched_filters <= 10;
           num_matched_filters++) {
        for (auto down_sampling_factor : kDownSamplingFactors) {
          EchoCanceller3Config config;
          config.delay.down_sampling_factor = down_sampling_factor;
          config.delay.num_filters = num_matched_filters;
          config.delay.capture_alignment_mixing.downmix = false;
          config.delay.capture_alignment_mixing.adaptive_selection = false;
          for (auto rate : {16000, 32000, 48000}) {
            Block render_block(NumBandsForRate(rate), num_render_channels);
            Block capture_block(NumBandsForRate(rate), num_capture_channels);

            for (int delay_samples : {-15, -50, -150, -200}) {
              absl::optional<DelayEstimate> delay_blocks;
              SCOPED_TRACE(ProduceDebugText(rate, -delay_samples,
                                            num_render_channels,
                                            num_capture_channels));
              std::unique_ptr<RenderDelayBuffer> render_delay_buffer(
                  RenderDelayBuffer::Create(config, rate, num_render_channels));
              std::unique_ptr<RenderDelayController> delay_controller(
                  RenderDelayController::Create(EchoCanceller3Config(), rate,
                                                num_capture_channels));
              DelayBuffer<float> signal_delay_buffer(-delay_samples);
              for (int k = 0;
                   k < (400 - delay_samples / static_cast<int>(kBlockSize));
                   ++k) {
                RandomizeSampleVector(
                    &random_generator,
                    capture_block.View(/*band=*/0, /*channel=*/0));
                signal_delay_buffer.Delay(
                    capture_block.View(/*band=*/0, /*channel=*/0),
                    render_block.View(/*band=*/0, /*channel=*/0));
                render_delay_buffer->Insert(render_block);
                render_delay_buffer->PrepareCaptureProcessing();
                delay_blocks = delay_controller->GetDelay(
                    render_delay_buffer->GetDownsampledRenderBuffer(),
                    render_delay_buffer->Delay(), capture_block);
              }

              ASSERT_FALSE(delay_blocks);
            }
          }
        }
      }
    }
  }
}

// Verifies that the RenderDelayController is able to align the signals for
// simple timeshifts between the signals when there is jitter in the API calls.
// TODO(bugs.webrtc.org/11161): Re-enable tests.
TEST(RenderDelayController, DISABLED_AlignmentWithJitter) {
  Random random_generator(42U);
  for (size_t num_capture_channels : {1, 2, 4}) {
    for (size_t num_render_channels : {1, 2, 8}) {
      Block capture_block(
          /*num_bands=*/1, num_capture_channels);
      for (size_t num_matched_filters = 4; num_matched_filters <= 10;
           num_matched_filters++) {
        for (auto down_sampling_factor : kDownSamplingFactors) {
          EchoCanceller3Config config;
          config.delay.down_sampling_factor = down_sampling_factor;
          config.delay.num_filters = num_matched_filters;
          config.delay.capture_alignment_mixing.downmix = false;
          config.delay.capture_alignment_mixing.adaptive_selection = false;

          for (auto rate : {16000, 32000, 48000}) {
            Block render_block(NumBandsForRate(rate), num_render_channels);
            for (size_t delay_samples : {15, 50, 300, 800}) {
              absl::optional<DelayEstimate> delay_blocks;
              SCOPED_TRACE(ProduceDebugText(rate, delay_samples,
                                            num_render_channels,
                                            num_capture_channels));
              std::unique_ptr<RenderDelayBuffer> render_delay_buffer(
                  RenderDelayBuffer::Create(config, rate, num_render_channels));
              std::unique_ptr<RenderDelayController> delay_controller(
                  RenderDelayController::Create(config, rate,
                                                num_capture_channels));
              DelayBuffer<float> signal_delay_buffer(delay_samples);
              constexpr size_t kMaxTestJitterBlocks = 26;
              for (size_t j = 0; j < (1000 + delay_samples / kBlockSize) /
                                             kMaxTestJitterBlocks +
                                         1;
                   ++j) {
                std::vector<Block> capture_block_buffer;
                for (size_t k = 0; k < (kMaxTestJitterBlocks - 1); ++k) {
                  RandomizeSampleVector(
                      &random_generator,
                      render_block.View(/*band=*/0, /*channel=*/0));
                  signal_delay_buffer.Delay(
                      render_block.View(/*band=*/0, /*channel=*/0),
                      capture_block.View(/*band=*/0, /*channel=*/0));
                  capture_block_buffer.push_back(capture_block);
                  render_delay_buffer->Insert(render_block);
                }
                for (size_t k = 0; k < (kMaxTestJitterBlocks - 1); ++k) {
                  render_delay_buffer->PrepareCaptureProcessing();
                  delay_blocks = delay_controller->GetDelay(
                      render_delay_buffer->GetDownsampledRenderBuffer(),
                      render_delay_buffer->Delay(), capture_block_buffer[k]);
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
  }
}

#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)

// Verifies the check for correct sample rate.
// TODO(peah): Re-enable the test once the issue with memory leaks during DEATH
// tests on test bots has been fixed.
TEST(RenderDelayControllerDeathTest, DISABLED_WrongSampleRate) {
  for (auto rate : {-1, 0, 8001, 16001}) {
    SCOPED_TRACE(ProduceDebugText(rate));
    EchoCanceller3Config config;
    std::unique_ptr<RenderDelayBuffer> render_delay_buffer(
        RenderDelayBuffer::Create(config, rate, 1));
    EXPECT_DEATH(
        std::unique_ptr<RenderDelayController>(
            RenderDelayController::Create(EchoCanceller3Config(), rate, 1)),
        "");
  }
}

#endif

}  // namespace webrtc
