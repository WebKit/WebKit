/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/echo_remover.h"

#include <algorithm>
#include <memory>
#include <numeric>
#include <string>

#include "modules/audio_processing/aec3/aec3_common.h"
#include "modules/audio_processing/aec3/render_buffer.h"
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

std::string ProduceDebugText(int sample_rate_hz, int delay) {
  rtc::StringBuilder ss(ProduceDebugText(sample_rate_hz));
  ss << ", Delay: " << delay;
  return ss.Release();
}

}  // namespace

class EchoRemoverMultiChannel
    : public ::testing::Test,
      public ::testing::WithParamInterface<std::tuple<size_t, size_t>> {};

INSTANTIATE_TEST_SUITE_P(MultiChannel,
                         EchoRemoverMultiChannel,
                         ::testing::Combine(::testing::Values(1, 2, 8),
                                            ::testing::Values(1, 2, 8)));

// Verifies the basic API call sequence
TEST_P(EchoRemoverMultiChannel, BasicApiCalls) {
  const size_t num_render_channels = std::get<0>(GetParam());
  const size_t num_capture_channels = std::get<1>(GetParam());
  absl::optional<DelayEstimate> delay_estimate;
  for (auto rate : {16000, 32000, 48000}) {
    SCOPED_TRACE(ProduceDebugText(rate));
    std::unique_ptr<EchoRemover> remover(
        EchoRemover::Create(EchoCanceller3Config(), rate, num_render_channels,
                            num_capture_channels));
    std::unique_ptr<RenderDelayBuffer> render_buffer(RenderDelayBuffer::Create(
        EchoCanceller3Config(), rate, num_render_channels));

    std::vector<std::vector<std::vector<float>>> render(
        NumBandsForRate(rate),
        std::vector<std::vector<float>>(num_render_channels,
                                        std::vector<float>(kBlockSize, 0.f)));
    std::vector<std::vector<std::vector<float>>> capture(
        NumBandsForRate(rate),
        std::vector<std::vector<float>>(num_capture_channels,
                                        std::vector<float>(kBlockSize, 0.f)));
    for (size_t k = 0; k < 100; ++k) {
      EchoPathVariability echo_path_variability(
          k % 3 == 0 ? true : false,
          k % 5 == 0 ? EchoPathVariability::DelayAdjustment::kNewDetectedDelay
                     : EchoPathVariability::DelayAdjustment::kNone,
          false);
      render_buffer->Insert(render);
      render_buffer->PrepareCaptureProcessing();

      remover->ProcessCapture(echo_path_variability, k % 2 == 0 ? true : false,
                              delay_estimate, render_buffer->GetRenderBuffer(),
                              nullptr, &capture);
    }
  }
}

#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)

// Verifies the check for the samplerate.
// TODO(peah): Re-enable the test once the issue with memory leaks during DEATH
// tests on test bots has been fixed.
TEST(EchoRemoverDeathTest, DISABLED_WrongSampleRate) {
  EXPECT_DEATH(std::unique_ptr<EchoRemover>(
                   EchoRemover::Create(EchoCanceller3Config(), 8001, 1, 1)),
               "");
}

// Verifies the check for the capture block size.
TEST(EchoRemoverDeathTest, WrongCaptureBlockSize) {
  absl::optional<DelayEstimate> delay_estimate;
  for (auto rate : {16000, 32000, 48000}) {
    SCOPED_TRACE(ProduceDebugText(rate));
    std::unique_ptr<EchoRemover> remover(
        EchoRemover::Create(EchoCanceller3Config(), rate, 1, 1));
    std::unique_ptr<RenderDelayBuffer> render_buffer(
        RenderDelayBuffer::Create(EchoCanceller3Config(), rate, 1));
    std::vector<std::vector<std::vector<float>>> capture(
        NumBandsForRate(rate), std::vector<std::vector<float>>(
                                   1, std::vector<float>(kBlockSize - 1, 0.f)));
    EchoPathVariability echo_path_variability(
        false, EchoPathVariability::DelayAdjustment::kNone, false);
    EXPECT_DEATH(remover->ProcessCapture(
                     echo_path_variability, false, delay_estimate,
                     render_buffer->GetRenderBuffer(), nullptr, &capture),
                 "");
  }
}

// Verifies the check for the number of capture bands.
// TODO(peah): Re-enable the test once the issue with memory leaks during DEATH
// tests on test bots has been fixed.c
TEST(EchoRemoverDeathTest, DISABLED_WrongCaptureNumBands) {
  absl::optional<DelayEstimate> delay_estimate;
  for (auto rate : {16000, 32000, 48000}) {
    SCOPED_TRACE(ProduceDebugText(rate));
    std::unique_ptr<EchoRemover> remover(
        EchoRemover::Create(EchoCanceller3Config(), rate, 1, 1));
    std::unique_ptr<RenderDelayBuffer> render_buffer(
        RenderDelayBuffer::Create(EchoCanceller3Config(), rate, 1));
    std::vector<std::vector<std::vector<float>>> capture(
        NumBandsForRate(rate == 48000 ? 16000 : rate + 16000),
        std::vector<std::vector<float>>(1,
                                        std::vector<float>(kBlockSize, 0.f)));
    EchoPathVariability echo_path_variability(
        false, EchoPathVariability::DelayAdjustment::kNone, false);
    EXPECT_DEATH(remover->ProcessCapture(
                     echo_path_variability, false, delay_estimate,
                     render_buffer->GetRenderBuffer(), nullptr, &capture),
                 "");
  }
}

// Verifies the check for non-null capture block.
TEST(EchoRemoverDeathTest, NullCapture) {
  absl::optional<DelayEstimate> delay_estimate;
  std::unique_ptr<EchoRemover> remover(
      EchoRemover::Create(EchoCanceller3Config(), 16000, 1, 1));
  std::unique_ptr<RenderDelayBuffer> render_buffer(
      RenderDelayBuffer::Create(EchoCanceller3Config(), 16000, 1));
  EchoPathVariability echo_path_variability(
      false, EchoPathVariability::DelayAdjustment::kNone, false);
  EXPECT_DEATH(remover->ProcessCapture(
                   echo_path_variability, false, delay_estimate,
                   render_buffer->GetRenderBuffer(), nullptr, nullptr),
               "");
}

#endif

// Performs a sanity check that the echo_remover is able to properly
// remove echoes.
TEST(EchoRemover, BasicEchoRemoval) {
  constexpr int kNumBlocksToProcess = 500;
  Random random_generator(42U);
  absl::optional<DelayEstimate> delay_estimate;
  for (size_t num_channels : {1, 2, 4}) {
    for (auto rate : {16000, 32000, 48000}) {
      std::vector<std::vector<std::vector<float>>> x(
          NumBandsForRate(rate),
          std::vector<std::vector<float>>(num_channels,
                                          std::vector<float>(kBlockSize, 0.f)));
      std::vector<std::vector<std::vector<float>>> y(
          NumBandsForRate(rate),
          std::vector<std::vector<float>>(num_channels,
                                          std::vector<float>(kBlockSize, 0.f)));
      EchoPathVariability echo_path_variability(
          false, EchoPathVariability::DelayAdjustment::kNone, false);
      for (size_t delay_samples : {0, 64, 150, 200, 301}) {
        SCOPED_TRACE(ProduceDebugText(rate, delay_samples));
        EchoCanceller3Config config;
        std::unique_ptr<EchoRemover> remover(
            EchoRemover::Create(config, rate, num_channels, num_channels));
        std::unique_ptr<RenderDelayBuffer> render_buffer(
            RenderDelayBuffer::Create(config, rate, num_channels));
        render_buffer->AlignFromDelay(delay_samples / kBlockSize);

        std::vector<std::vector<std::unique_ptr<DelayBuffer<float>>>>
            delay_buffers(x.size());
        for (size_t band = 0; band < delay_buffers.size(); ++band) {
          delay_buffers[band].resize(x[0].size());
        }

        for (size_t band = 0; band < x.size(); ++band) {
          for (size_t channel = 0; channel < x[0].size(); ++channel) {
            delay_buffers[band][channel].reset(
                new DelayBuffer<float>(delay_samples));
          }
        }

        float input_energy = 0.f;
        float output_energy = 0.f;
        for (int k = 0; k < kNumBlocksToProcess; ++k) {
          const bool silence = k < 100 || (k % 100 >= 10);

          for (size_t band = 0; band < x.size(); ++band) {
            for (size_t channel = 0; channel < x[0].size(); ++channel) {
              if (silence) {
                std::fill(x[band][channel].begin(), x[band][channel].end(),
                          0.f);
              } else {
                RandomizeSampleVector(&random_generator, x[band][channel]);
              }
              delay_buffers[band][channel]->Delay(x[band][channel],
                                                  y[band][channel]);
            }
          }

          if (k > kNumBlocksToProcess / 2) {
            input_energy = std::inner_product(y[0][0].begin(), y[0][0].end(),
                                              y[0][0].begin(), input_energy);
          }

          render_buffer->Insert(x);
          render_buffer->PrepareCaptureProcessing();

          remover->ProcessCapture(echo_path_variability, false, delay_estimate,
                                  render_buffer->GetRenderBuffer(), nullptr,
                                  &y);

          if (k > kNumBlocksToProcess / 2) {
            output_energy = std::inner_product(y[0][0].begin(), y[0][0].end(),
                                               y[0][0].begin(), output_energy);
          }
        }
        EXPECT_GT(input_energy, 10.f * output_energy);
      }
    }
  }
}

}  // namespace webrtc
