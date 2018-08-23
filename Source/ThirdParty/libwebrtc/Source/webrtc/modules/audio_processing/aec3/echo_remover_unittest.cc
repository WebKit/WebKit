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
#include <sstream>
#include <string>

#include "modules/audio_processing/aec3/aec3_common.h"
#include "modules/audio_processing/aec3/render_buffer.h"
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

std::string ProduceDebugText(int sample_rate_hz, int delay) {
  std::ostringstream ss(ProduceDebugText(sample_rate_hz));
  ss << ", Delay: " << delay;
  return ss.str();
}

}  // namespace

// Verifies the basic API call sequence
TEST(EchoRemover, BasicApiCalls) {
  absl::optional<DelayEstimate> delay_estimate;
  for (auto rate : {8000, 16000, 32000, 48000}) {
    SCOPED_TRACE(ProduceDebugText(rate));
    std::unique_ptr<EchoRemover> remover(
        EchoRemover::Create(EchoCanceller3Config(), rate));
    std::unique_ptr<RenderDelayBuffer> render_buffer(RenderDelayBuffer::Create(
        EchoCanceller3Config(), NumBandsForRate(rate)));

    std::vector<std::vector<float>> render(NumBandsForRate(rate),
                                           std::vector<float>(kBlockSize, 0.f));
    std::vector<std::vector<float>> capture(
        NumBandsForRate(rate), std::vector<float>(kBlockSize, 0.f));
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
                              &capture);
    }
  }
}

#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)

// Verifies the check for the samplerate.
// TODO(peah): Re-enable the test once the issue with memory leaks during DEATH
// tests on test bots has been fixed.
TEST(EchoRemover, DISABLED_WrongSampleRate) {
  EXPECT_DEATH(std::unique_ptr<EchoRemover>(
                   EchoRemover::Create(EchoCanceller3Config(), 8001)),
               "");
}

// Verifies the check for the capture block size.
TEST(EchoRemover, WrongCaptureBlockSize) {
  absl::optional<DelayEstimate> delay_estimate;
  for (auto rate : {8000, 16000, 32000, 48000}) {
    SCOPED_TRACE(ProduceDebugText(rate));
    std::unique_ptr<EchoRemover> remover(
        EchoRemover::Create(EchoCanceller3Config(), rate));
    std::unique_ptr<RenderDelayBuffer> render_buffer(RenderDelayBuffer::Create(
        EchoCanceller3Config(), NumBandsForRate(rate)));
    std::vector<std::vector<float>> capture(
        NumBandsForRate(rate), std::vector<float>(kBlockSize - 1, 0.f));
    EchoPathVariability echo_path_variability(
        false, EchoPathVariability::DelayAdjustment::kNone, false);
    EXPECT_DEATH(
        remover->ProcessCapture(echo_path_variability, false, delay_estimate,
                                render_buffer->GetRenderBuffer(), &capture),
        "");
  }
}

// Verifies the check for the number of capture bands.
// TODO(peah): Re-enable the test once the issue with memory leaks during DEATH
// tests on test bots has been fixed.c
TEST(EchoRemover, DISABLED_WrongCaptureNumBands) {
  absl::optional<DelayEstimate> delay_estimate;
  for (auto rate : {16000, 32000, 48000}) {
    SCOPED_TRACE(ProduceDebugText(rate));
    std::unique_ptr<EchoRemover> remover(
        EchoRemover::Create(EchoCanceller3Config(), rate));
    std::unique_ptr<RenderDelayBuffer> render_buffer(RenderDelayBuffer::Create(
        EchoCanceller3Config(), NumBandsForRate(rate)));
    std::vector<std::vector<float>> capture(
        NumBandsForRate(rate == 48000 ? 16000 : rate + 16000),
        std::vector<float>(kBlockSize, 0.f));
    EchoPathVariability echo_path_variability(
        false, EchoPathVariability::DelayAdjustment::kNone, false);
    EXPECT_DEATH(
        remover->ProcessCapture(echo_path_variability, false, delay_estimate,
                                render_buffer->GetRenderBuffer(), &capture),
        "");
  }
}

// Verifies the check for non-null capture block.
TEST(EchoRemover, NullCapture) {
  absl::optional<DelayEstimate> delay_estimate;
  std::unique_ptr<EchoRemover> remover(
      EchoRemover::Create(EchoCanceller3Config(), 8000));
  std::unique_ptr<RenderDelayBuffer> render_buffer(
      RenderDelayBuffer::Create(EchoCanceller3Config(), 3));
  EchoPathVariability echo_path_variability(
      false, EchoPathVariability::DelayAdjustment::kNone, false);
  EXPECT_DEATH(
      remover->ProcessCapture(echo_path_variability, false, delay_estimate,
                              render_buffer->GetRenderBuffer(), nullptr),
      "");
}

#endif

// Performs a sanity check that the echo_remover is able to properly
// remove echoes.
TEST(EchoRemover, BasicEchoRemoval) {
  constexpr int kNumBlocksToProcess = 500;
  Random random_generator(42U);
  absl::optional<DelayEstimate> delay_estimate;
  for (auto rate : {8000, 16000, 32000, 48000}) {
    std::vector<std::vector<float>> x(NumBandsForRate(rate),
                                      std::vector<float>(kBlockSize, 0.f));
    std::vector<std::vector<float>> y(NumBandsForRate(rate),
                                      std::vector<float>(kBlockSize, 0.f));
    EchoPathVariability echo_path_variability(
        false, EchoPathVariability::DelayAdjustment::kNone, false);
    for (size_t delay_samples : {0, 64, 150, 200, 301}) {
      SCOPED_TRACE(ProduceDebugText(rate, delay_samples));
      EchoCanceller3Config config;
      config.delay.min_echo_path_delay_blocks = 0;
      std::unique_ptr<EchoRemover> remover(EchoRemover::Create(config, rate));
      std::unique_ptr<RenderDelayBuffer> render_buffer(
          RenderDelayBuffer::Create(config, NumBandsForRate(rate)));
      render_buffer->SetDelay(delay_samples / kBlockSize);

      std::vector<std::unique_ptr<DelayBuffer<float>>> delay_buffers(x.size());
      for (size_t j = 0; j < x.size(); ++j) {
        delay_buffers[j].reset(new DelayBuffer<float>(delay_samples));
      }

      float input_energy = 0.f;
      float output_energy = 0.f;
      for (int k = 0; k < kNumBlocksToProcess; ++k) {
        const bool silence = k < 100 || (k % 100 >= 10);

        for (size_t j = 0; j < x.size(); ++j) {
          if (silence) {
            std::fill(x[j].begin(), x[j].end(), 0.f);
          } else {
            RandomizeSampleVector(&random_generator, x[j]);
          }
          delay_buffers[j]->Delay(x[j], y[j]);
        }

        if (k > kNumBlocksToProcess / 2) {
          for (size_t j = 0; j < x.size(); ++j) {
            input_energy = std::inner_product(y[j].begin(), y[j].end(),
                                              y[j].begin(), input_energy);
          }
        }

        render_buffer->Insert(x);
        render_buffer->PrepareCaptureProcessing();

        remover->ProcessCapture(echo_path_variability, false, delay_estimate,
                                render_buffer->GetRenderBuffer(), &y);

        if (k > kNumBlocksToProcess / 2) {
          for (size_t j = 0; j < x.size(); ++j) {
            output_energy = std::inner_product(y[j].begin(), y[j].end(),
                                               y[j].begin(), output_energy);
          }
        }
      }
      EXPECT_GT(input_energy, 10.f * output_energy);
    }
  }
}

}  // namespace webrtc
