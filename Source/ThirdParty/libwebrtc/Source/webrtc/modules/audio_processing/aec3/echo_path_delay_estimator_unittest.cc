/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/echo_path_delay_estimator.h"

#include <algorithm>
#include <sstream>
#include <string>

#include "modules/audio_processing/aec3/aec3_common.h"
#include "modules/audio_processing/aec3/render_delay_buffer.h"
#include "modules/audio_processing/logging/apm_data_dumper.h"
#include "modules/audio_processing/test/echo_canceller_test_tools.h"
#include "rtc_base/random.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

std::string ProduceDebugText(size_t delay) {
  std::ostringstream ss;
  ss << "Delay: " << delay;
  return ss.str();
}

}  // namespace

// Verifies that the basic API calls work.
TEST(EchoPathDelayEstimator, BasicApiCalls) {
  ApmDataDumper data_dumper(0);
  EchoCanceller3Config config;
  std::unique_ptr<RenderDelayBuffer> render_delay_buffer(
      RenderDelayBuffer::Create(
          3, config.delay.down_sampling_factor,
          GetDownSampledBufferSize(config.delay.down_sampling_factor,
                                   config.delay.num_filters),
          GetRenderDelayBufferSize(config.delay.down_sampling_factor,
                                   config.delay.num_filters)));
  EchoPathDelayEstimator estimator(&data_dumper, config);
  std::vector<std::vector<float>> render(3, std::vector<float>(kBlockSize));
  std::vector<float> capture(kBlockSize);
  for (size_t k = 0; k < 100; ++k) {
    render_delay_buffer->Insert(render);
    estimator.EstimateDelay(render_delay_buffer->GetDownsampledRenderBuffer(),
                            capture);
  }
}

// Verifies that the delay estimator produces correct delay for artificially
// delayed signals.
TEST(EchoPathDelayEstimator, DelayEstimation) {
  Random random_generator(42U);
  std::vector<std::vector<float>> render(3, std::vector<float>(kBlockSize));
  std::vector<float> capture(kBlockSize);
  ApmDataDumper data_dumper(0);
  constexpr size_t kDownSamplingFactors[] = {2, 4, 8};
  for (auto down_sampling_factor : kDownSamplingFactors) {
    EchoCanceller3Config config;
    config.delay.down_sampling_factor = down_sampling_factor;
    config.delay.num_filters = 10;
    for (size_t delay_samples : {30, 64, 150, 200, 800, 4000}) {
      SCOPED_TRACE(ProduceDebugText(delay_samples));
      std::unique_ptr<RenderDelayBuffer> render_delay_buffer(
          RenderDelayBuffer::Create(
              3, config.delay.down_sampling_factor,
              GetDownSampledBufferSize(config.delay.down_sampling_factor,
                                       config.delay.num_filters),
              GetRenderDelayBufferSize(config.delay.down_sampling_factor,
                                       config.delay.num_filters)));
      DelayBuffer<float> signal_delay_buffer(delay_samples);
      EchoPathDelayEstimator estimator(&data_dumper, config);

      rtc::Optional<size_t> estimated_delay_samples;
      for (size_t k = 0; k < (300 + delay_samples / kBlockSize); ++k) {
        RandomizeSampleVector(&random_generator, render[0]);
        signal_delay_buffer.Delay(render[0], capture);
        render_delay_buffer->Insert(render);
        render_delay_buffer->UpdateBuffers();
        estimated_delay_samples = estimator.EstimateDelay(
            render_delay_buffer->GetDownsampledRenderBuffer(), capture);
      }
      if (estimated_delay_samples) {
        // Due to the internal down-sampling done inside the delay estimator
        // the estimated delay cannot be expected to be exact to the true delay.
        EXPECT_NEAR(delay_samples, *estimated_delay_samples,
                    config.delay.down_sampling_factor);
      } else {
        ADD_FAILURE();
      }
    }
  }
}

// Verifies that the delay estimator does not produce delay estimates too
// quickly.
TEST(EchoPathDelayEstimator, NoInitialDelayestimates) {
  Random random_generator(42U);
  EchoCanceller3Config config;
  std::vector<std::vector<float>> render(3, std::vector<float>(kBlockSize));
  std::vector<float> capture(kBlockSize);
  ApmDataDumper data_dumper(0);
  std::unique_ptr<RenderDelayBuffer> render_delay_buffer(
      RenderDelayBuffer::Create(
          3, config.delay.down_sampling_factor,
          GetDownSampledBufferSize(config.delay.down_sampling_factor,
                                   config.delay.num_filters),
          GetRenderDelayBufferSize(config.delay.down_sampling_factor,
                                   config.delay.num_filters)));

  EchoPathDelayEstimator estimator(&data_dumper, config);
  for (size_t k = 0; k < 19; ++k) {
    RandomizeSampleVector(&random_generator, render[0]);
    std::copy(render[0].begin(), render[0].end(), capture.begin());
    render_delay_buffer->Insert(render);
    render_delay_buffer->UpdateBuffers();
    EXPECT_FALSE(estimator.EstimateDelay(
        render_delay_buffer->GetDownsampledRenderBuffer(), capture));
  }
}

// Verifies that the delay estimator does not produce delay estimates for render
// signals of low level.
TEST(EchoPathDelayEstimator, NoDelayEstimatesForLowLevelRenderSignals) {
  Random random_generator(42U);
  EchoCanceller3Config config;
  std::vector<std::vector<float>> render(3, std::vector<float>(kBlockSize));
  std::vector<float> capture(kBlockSize);
  ApmDataDumper data_dumper(0);
  EchoPathDelayEstimator estimator(&data_dumper, config);
  std::unique_ptr<RenderDelayBuffer> render_delay_buffer(
      RenderDelayBuffer::Create(
          3, config.delay.down_sampling_factor,
          GetDownSampledBufferSize(config.delay.down_sampling_factor,
                                   config.delay.num_filters),
          GetRenderDelayBufferSize(config.delay.down_sampling_factor,
                                   config.delay.num_filters)));
  for (size_t k = 0; k < 100; ++k) {
    RandomizeSampleVector(&random_generator, render[0]);
    for (auto& render_k : render[0]) {
      render_k *= 100.f / 32767.f;
    }
    std::copy(render[0].begin(), render[0].end(), capture.begin());
    render_delay_buffer->Insert(render);
    render_delay_buffer->UpdateBuffers();
    EXPECT_FALSE(estimator.EstimateDelay(
        render_delay_buffer->GetDownsampledRenderBuffer(), capture));
  }
}

// Verifies that the delay estimator does not produce delay estimates for
// uncorrelated signals.
TEST(EchoPathDelayEstimator, NoDelayEstimatesForUncorrelatedSignals) {
  Random random_generator(42U);
  EchoCanceller3Config config;
  std::vector<std::vector<float>> render(3, std::vector<float>(kBlockSize));
  std::vector<float> capture(kBlockSize);
  ApmDataDumper data_dumper(0);
  EchoPathDelayEstimator estimator(&data_dumper, config);
  std::unique_ptr<RenderDelayBuffer> render_delay_buffer(
      RenderDelayBuffer::Create(
          3, config.delay.down_sampling_factor,
          GetDownSampledBufferSize(config.delay.down_sampling_factor,
                                   config.delay.num_filters),
          GetRenderDelayBufferSize(config.delay.down_sampling_factor,
                                   config.delay.num_filters)));
  for (size_t k = 0; k < 100; ++k) {
    RandomizeSampleVector(&random_generator, render[0]);
    RandomizeSampleVector(&random_generator, capture);
    render_delay_buffer->Insert(render);
    render_delay_buffer->UpdateBuffers();
    EXPECT_FALSE(estimator.EstimateDelay(
        render_delay_buffer->GetDownsampledRenderBuffer(), capture));
  }
}

#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)

// Verifies the check for the render blocksize.
// TODO(peah): Re-enable the test once the issue with memory leaks during DEATH
// tests on test bots has been fixed.
TEST(EchoPathDelayEstimator, DISABLED_WrongRenderBlockSize) {
  ApmDataDumper data_dumper(0);
  EchoCanceller3Config config;
  EchoPathDelayEstimator estimator(&data_dumper, config);
  std::unique_ptr<RenderDelayBuffer> render_delay_buffer(
      RenderDelayBuffer::Create(
          3, config.delay.down_sampling_factor,
          GetDownSampledBufferSize(config.delay.down_sampling_factor,
                                   config.delay.num_filters),
          GetRenderDelayBufferSize(config.delay.down_sampling_factor,
                                   config.delay.num_filters)));
  std::vector<float> capture(kBlockSize);
  EXPECT_DEATH(estimator.EstimateDelay(
                   render_delay_buffer->GetDownsampledRenderBuffer(), capture),
               "");
}

// Verifies the check for the capture blocksize.
// TODO(peah): Re-enable the test once the issue with memory leaks during DEATH
// tests on test bots has been fixed.
TEST(EchoPathDelayEstimator, WrongCaptureBlockSize) {
  ApmDataDumper data_dumper(0);
  EchoCanceller3Config config;
  EchoPathDelayEstimator estimator(&data_dumper, config);
  std::unique_ptr<RenderDelayBuffer> render_delay_buffer(
      RenderDelayBuffer::Create(
          3, config.delay.down_sampling_factor,
          GetDownSampledBufferSize(config.delay.down_sampling_factor,
                                   config.delay.num_filters),
          GetRenderDelayBufferSize(config.delay.down_sampling_factor,
                                   config.delay.num_filters)));
  std::vector<float> capture(std::vector<float>(kBlockSize - 1));
  EXPECT_DEATH(estimator.EstimateDelay(
                   render_delay_buffer->GetDownsampledRenderBuffer(), capture),
               "");
}

// Verifies the check for non-null data dumper.
TEST(EchoPathDelayEstimator, NullDataDumper) {
  EXPECT_DEATH(EchoPathDelayEstimator(nullptr, EchoCanceller3Config()), "");
}

#endif

}  // namespace webrtc
