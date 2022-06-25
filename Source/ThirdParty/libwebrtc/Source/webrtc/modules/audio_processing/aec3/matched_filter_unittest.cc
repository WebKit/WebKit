/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/matched_filter.h"

// Defines WEBRTC_ARCH_X86_FAMILY, used below.
#include "rtc_base/system/arch.h"

#if defined(WEBRTC_ARCH_X86_FAMILY)
#include <emmintrin.h>
#endif
#include <algorithm>
#include <string>

#include "modules/audio_processing/aec3/aec3_common.h"
#include "modules/audio_processing/aec3/decimator.h"
#include "modules/audio_processing/aec3/render_delay_buffer.h"
#include "modules/audio_processing/logging/apm_data_dumper.h"
#include "modules/audio_processing/test/echo_canceller_test_tools.h"
#include "rtc_base/random.h"
#include "rtc_base/strings/string_builder.h"
#include "system_wrappers/include/cpu_features_wrapper.h"
#include "test/gtest.h"

namespace webrtc {
namespace aec3 {
namespace {

std::string ProduceDebugText(size_t delay, size_t down_sampling_factor) {
  rtc::StringBuilder ss;
  ss << "Delay: " << delay;
  ss << ", Down sampling factor: " << down_sampling_factor;
  return ss.Release();
}

constexpr size_t kNumMatchedFilters = 10;
constexpr size_t kDownSamplingFactors[] = {2, 4, 8};
constexpr size_t kWindowSizeSubBlocks = 32;
constexpr size_t kAlignmentShiftSubBlocks = kWindowSizeSubBlocks * 3 / 4;

}  // namespace

#if defined(WEBRTC_HAS_NEON)
// Verifies that the optimized methods for NEON are similar to their reference
// counterparts.
TEST(MatchedFilter, TestNeonOptimizations) {
  Random random_generator(42U);
  constexpr float kSmoothing = 0.7f;
  for (auto down_sampling_factor : kDownSamplingFactors) {
    const size_t sub_block_size = kBlockSize / down_sampling_factor;

    std::vector<float> x(2000);
    RandomizeSampleVector(&random_generator, x);
    std::vector<float> y(sub_block_size);
    std::vector<float> h_NEON(512);
    std::vector<float> h(512);
    int x_index = 0;
    for (int k = 0; k < 1000; ++k) {
      RandomizeSampleVector(&random_generator, y);

      bool filters_updated = false;
      float error_sum = 0.f;
      bool filters_updated_NEON = false;
      float error_sum_NEON = 0.f;

      MatchedFilterCore_NEON(x_index, h.size() * 150.f * 150.f, kSmoothing, x,
                             y, h_NEON, &filters_updated_NEON, &error_sum_NEON);

      MatchedFilterCore(x_index, h.size() * 150.f * 150.f, kSmoothing, x, y, h,
                        &filters_updated, &error_sum);

      EXPECT_EQ(filters_updated, filters_updated_NEON);
      EXPECT_NEAR(error_sum, error_sum_NEON, error_sum / 100000.f);

      for (size_t j = 0; j < h.size(); ++j) {
        EXPECT_NEAR(h[j], h_NEON[j], 0.00001f);
      }

      x_index = (x_index + sub_block_size) % x.size();
    }
  }
}
#endif

#if defined(WEBRTC_ARCH_X86_FAMILY)
// Verifies that the optimized methods for SSE2 are bitexact to their reference
// counterparts.
TEST(MatchedFilter, TestSse2Optimizations) {
  bool use_sse2 = (GetCPUInfo(kSSE2) != 0);
  if (use_sse2) {
    Random random_generator(42U);
    constexpr float kSmoothing = 0.7f;
    for (auto down_sampling_factor : kDownSamplingFactors) {
      const size_t sub_block_size = kBlockSize / down_sampling_factor;
      std::vector<float> x(2000);
      RandomizeSampleVector(&random_generator, x);
      std::vector<float> y(sub_block_size);
      std::vector<float> h_SSE2(512);
      std::vector<float> h(512);
      int x_index = 0;
      for (int k = 0; k < 1000; ++k) {
        RandomizeSampleVector(&random_generator, y);

        bool filters_updated = false;
        float error_sum = 0.f;
        bool filters_updated_SSE2 = false;
        float error_sum_SSE2 = 0.f;

        MatchedFilterCore_SSE2(x_index, h.size() * 150.f * 150.f, kSmoothing, x,
                               y, h_SSE2, &filters_updated_SSE2,
                               &error_sum_SSE2);

        MatchedFilterCore(x_index, h.size() * 150.f * 150.f, kSmoothing, x, y,
                          h, &filters_updated, &error_sum);

        EXPECT_EQ(filters_updated, filters_updated_SSE2);
        EXPECT_NEAR(error_sum, error_sum_SSE2, error_sum / 100000.f);

        for (size_t j = 0; j < h.size(); ++j) {
          EXPECT_NEAR(h[j], h_SSE2[j], 0.00001f);
        }

        x_index = (x_index + sub_block_size) % x.size();
      }
    }
  }
}

TEST(MatchedFilter, TestAvx2Optimizations) {
  bool use_avx2 = (GetCPUInfo(kAVX2) != 0);
  if (use_avx2) {
    Random random_generator(42U);
    constexpr float kSmoothing = 0.7f;
    for (auto down_sampling_factor : kDownSamplingFactors) {
      const size_t sub_block_size = kBlockSize / down_sampling_factor;
      std::vector<float> x(2000);
      RandomizeSampleVector(&random_generator, x);
      std::vector<float> y(sub_block_size);
      std::vector<float> h_AVX2(512);
      std::vector<float> h(512);
      int x_index = 0;
      for (int k = 0; k < 1000; ++k) {
        RandomizeSampleVector(&random_generator, y);

        bool filters_updated = false;
        float error_sum = 0.f;
        bool filters_updated_AVX2 = false;
        float error_sum_AVX2 = 0.f;

        MatchedFilterCore_AVX2(x_index, h.size() * 150.f * 150.f, kSmoothing, x,
                               y, h_AVX2, &filters_updated_AVX2,
                               &error_sum_AVX2);

        MatchedFilterCore(x_index, h.size() * 150.f * 150.f, kSmoothing, x, y,
                          h, &filters_updated, &error_sum);

        EXPECT_EQ(filters_updated, filters_updated_AVX2);
        EXPECT_NEAR(error_sum, error_sum_AVX2, error_sum / 100000.f);

        for (size_t j = 0; j < h.size(); ++j) {
          EXPECT_NEAR(h[j], h_AVX2[j], 0.00001f);
        }

        x_index = (x_index + sub_block_size) % x.size();
      }
    }
  }
}

#endif

// Verifies that the matched filter produces proper lag estimates for
// artificially
// delayed signals.
TEST(MatchedFilter, LagEstimation) {
  Random random_generator(42U);
  constexpr size_t kNumChannels = 1;
  constexpr int kSampleRateHz = 48000;
  constexpr size_t kNumBands = NumBandsForRate(kSampleRateHz);

  for (auto down_sampling_factor : kDownSamplingFactors) {
    const size_t sub_block_size = kBlockSize / down_sampling_factor;

    std::vector<std::vector<std::vector<float>>> render(
        kNumBands, std::vector<std::vector<float>>(
                       kNumChannels, std::vector<float>(kBlockSize, 0.f)));
    std::vector<std::vector<float>> capture(
        1, std::vector<float>(kBlockSize, 0.f));
    ApmDataDumper data_dumper(0);
    for (size_t delay_samples : {5, 64, 150, 200, 800, 1000}) {
      SCOPED_TRACE(ProduceDebugText(delay_samples, down_sampling_factor));
      EchoCanceller3Config config;
      config.delay.down_sampling_factor = down_sampling_factor;
      config.delay.num_filters = kNumMatchedFilters;
      Decimator capture_decimator(down_sampling_factor);
      DelayBuffer<float> signal_delay_buffer(down_sampling_factor *
                                             delay_samples);
      MatchedFilter filter(&data_dumper, DetectOptimization(), sub_block_size,
                           kWindowSizeSubBlocks, kNumMatchedFilters,
                           kAlignmentShiftSubBlocks, 150,
                           config.delay.delay_estimate_smoothing,
                           config.delay.delay_estimate_smoothing_delay_found,
                           config.delay.delay_candidate_detection_threshold);

      std::unique_ptr<RenderDelayBuffer> render_delay_buffer(
          RenderDelayBuffer::Create(config, kSampleRateHz, kNumChannels));

      // Analyze the correlation between render and capture.
      for (size_t k = 0; k < (600 + delay_samples / sub_block_size); ++k) {
        for (size_t band = 0; band < kNumBands; ++band) {
          for (size_t channel = 0; channel < kNumChannels; ++channel) {
            RandomizeSampleVector(&random_generator, render[band][channel]);
          }
        }
        signal_delay_buffer.Delay(render[0][0], capture[0]);
        render_delay_buffer->Insert(render);

        if (k == 0) {
          render_delay_buffer->Reset();
        }

        render_delay_buffer->PrepareCaptureProcessing();
        std::array<float, kBlockSize> downsampled_capture_data;
        rtc::ArrayView<float> downsampled_capture(
            downsampled_capture_data.data(), sub_block_size);
        capture_decimator.Decimate(capture[0], downsampled_capture);
        filter.Update(render_delay_buffer->GetDownsampledRenderBuffer(),
                      downsampled_capture, false);
      }

      // Obtain the lag estimates.
      auto lag_estimates = filter.GetLagEstimates();

      // Find which lag estimate should be the most accurate.
      absl::optional<size_t> expected_most_accurate_lag_estimate;
      size_t alignment_shift_sub_blocks = 0;
      for (size_t k = 0; k < config.delay.num_filters; ++k) {
        if ((alignment_shift_sub_blocks + 3 * kWindowSizeSubBlocks / 4) *
                sub_block_size >
            delay_samples) {
          expected_most_accurate_lag_estimate = k > 0 ? k - 1 : 0;
          break;
        }
        alignment_shift_sub_blocks += kAlignmentShiftSubBlocks;
      }
      ASSERT_TRUE(expected_most_accurate_lag_estimate);

      // Verify that the expected most accurate lag estimate is the most
      // accurate estimate.
      for (size_t k = 0; k < kNumMatchedFilters; ++k) {
        if (k != *expected_most_accurate_lag_estimate &&
            k != (*expected_most_accurate_lag_estimate + 1)) {
          EXPECT_TRUE(
              lag_estimates[*expected_most_accurate_lag_estimate].accuracy >
                  lag_estimates[k].accuracy ||
              !lag_estimates[k].reliable ||
              !lag_estimates[*expected_most_accurate_lag_estimate].reliable);
        }
      }

      // Verify that all lag estimates are updated as expected for signals
      // containing strong noise.
      for (auto& le : lag_estimates) {
        EXPECT_TRUE(le.updated);
      }

      // Verify that the expected most accurate lag estimate is reliable.
      EXPECT_TRUE(
          lag_estimates[*expected_most_accurate_lag_estimate].reliable ||
          lag_estimates[std::min(*expected_most_accurate_lag_estimate + 1,
                                 lag_estimates.size() - 1)]
              .reliable);

      // Verify that the expected most accurate lag estimate is correct.
      if (lag_estimates[*expected_most_accurate_lag_estimate].reliable) {
        EXPECT_TRUE(delay_samples ==
                    lag_estimates[*expected_most_accurate_lag_estimate].lag);
      } else {
        EXPECT_TRUE(
            delay_samples ==
            lag_estimates[std::min(*expected_most_accurate_lag_estimate + 1,
                                   lag_estimates.size() - 1)]
                .lag);
      }
    }
  }
}

// Verifies that the matched filter does not produce reliable and accurate
// estimates for uncorrelated render and capture signals.
TEST(MatchedFilter, LagNotReliableForUncorrelatedRenderAndCapture) {
  constexpr size_t kNumChannels = 1;
  constexpr int kSampleRateHz = 48000;
  constexpr size_t kNumBands = NumBandsForRate(kSampleRateHz);
  Random random_generator(42U);
  for (auto down_sampling_factor : kDownSamplingFactors) {
    EchoCanceller3Config config;
    config.delay.down_sampling_factor = down_sampling_factor;
    config.delay.num_filters = kNumMatchedFilters;
    const size_t sub_block_size = kBlockSize / down_sampling_factor;

    std::vector<std::vector<std::vector<float>>> render(
        kNumBands, std::vector<std::vector<float>>(
                       kNumChannels, std::vector<float>(kBlockSize, 0.f)));
    std::array<float, kBlockSize> capture_data;
    rtc::ArrayView<float> capture(capture_data.data(), sub_block_size);
    std::fill(capture.begin(), capture.end(), 0.f);
    ApmDataDumper data_dumper(0);
    std::unique_ptr<RenderDelayBuffer> render_delay_buffer(
        RenderDelayBuffer::Create(config, kSampleRateHz, kNumChannels));
    MatchedFilter filter(&data_dumper, DetectOptimization(), sub_block_size,
                         kWindowSizeSubBlocks, kNumMatchedFilters,
                         kAlignmentShiftSubBlocks, 150,
                         config.delay.delay_estimate_smoothing,
                         config.delay.delay_estimate_smoothing_delay_found,
                         config.delay.delay_candidate_detection_threshold);

    // Analyze the correlation between render and capture.
    for (size_t k = 0; k < 100; ++k) {
      RandomizeSampleVector(&random_generator, render[0][0]);
      RandomizeSampleVector(&random_generator, capture);
      render_delay_buffer->Insert(render);
      filter.Update(render_delay_buffer->GetDownsampledRenderBuffer(), capture,
                    false);
    }

    // Obtain the lag estimates.
    auto lag_estimates = filter.GetLagEstimates();
    EXPECT_EQ(kNumMatchedFilters, lag_estimates.size());

    // Verify that no lag estimates are reliable.
    for (auto& le : lag_estimates) {
      EXPECT_FALSE(le.reliable);
    }
  }
}

// Verifies that the matched filter does not produce updated lag estimates for
// render signals of low level.
TEST(MatchedFilter, LagNotUpdatedForLowLevelRender) {
  Random random_generator(42U);
  constexpr size_t kNumChannels = 1;
  constexpr int kSampleRateHz = 48000;
  constexpr size_t kNumBands = NumBandsForRate(kSampleRateHz);

  for (auto down_sampling_factor : kDownSamplingFactors) {
    const size_t sub_block_size = kBlockSize / down_sampling_factor;

    std::vector<std::vector<std::vector<float>>> render(
        kNumBands, std::vector<std::vector<float>>(
                       kNumChannels, std::vector<float>(kBlockSize, 0.f)));
    std::vector<std::vector<float>> capture(
        1, std::vector<float>(kBlockSize, 0.f));
    ApmDataDumper data_dumper(0);
    EchoCanceller3Config config;
    MatchedFilter filter(&data_dumper, DetectOptimization(), sub_block_size,
                         kWindowSizeSubBlocks, kNumMatchedFilters,
                         kAlignmentShiftSubBlocks, 150,
                         config.delay.delay_estimate_smoothing,
                         config.delay.delay_estimate_smoothing_delay_found,
                         config.delay.delay_candidate_detection_threshold);
    std::unique_ptr<RenderDelayBuffer> render_delay_buffer(
        RenderDelayBuffer::Create(EchoCanceller3Config(), kSampleRateHz,
                                  kNumChannels));
    Decimator capture_decimator(down_sampling_factor);

    // Analyze the correlation between render and capture.
    for (size_t k = 0; k < 100; ++k) {
      RandomizeSampleVector(&random_generator, render[0][0]);
      for (auto& render_k : render[0][0]) {
        render_k *= 149.f / 32767.f;
      }
      std::copy(render[0][0].begin(), render[0][0].end(), capture[0].begin());
      std::array<float, kBlockSize> downsampled_capture_data;
      rtc::ArrayView<float> downsampled_capture(downsampled_capture_data.data(),
                                                sub_block_size);
      capture_decimator.Decimate(capture[0], downsampled_capture);
      filter.Update(render_delay_buffer->GetDownsampledRenderBuffer(),
                    downsampled_capture, false);
    }

    // Obtain the lag estimates.
    auto lag_estimates = filter.GetLagEstimates();
    EXPECT_EQ(kNumMatchedFilters, lag_estimates.size());

    // Verify that no lag estimates are updated and that no lag estimates are
    // reliable.
    for (auto& le : lag_estimates) {
      EXPECT_FALSE(le.updated);
      EXPECT_FALSE(le.reliable);
    }
  }
}

// Verifies that the correct number of lag estimates are produced for a certain
// number of alignment shifts.
TEST(MatchedFilter, NumberOfLagEstimates) {
  ApmDataDumper data_dumper(0);
  EchoCanceller3Config config;
  for (auto down_sampling_factor : kDownSamplingFactors) {
    const size_t sub_block_size = kBlockSize / down_sampling_factor;
    for (size_t num_matched_filters = 0; num_matched_filters < 10;
         ++num_matched_filters) {
      MatchedFilter filter(&data_dumper, DetectOptimization(), sub_block_size,
                           32, num_matched_filters, 1, 150,
                           config.delay.delay_estimate_smoothing,
                           config.delay.delay_estimate_smoothing_delay_found,
                           config.delay.delay_candidate_detection_threshold);
      EXPECT_EQ(num_matched_filters, filter.GetLagEstimates().size());
    }
  }
}

#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)

// Verifies the check for non-zero windows size.
TEST(MatchedFilterDeathTest, ZeroWindowSize) {
  ApmDataDumper data_dumper(0);
  EchoCanceller3Config config;
  EXPECT_DEATH(MatchedFilter(&data_dumper, DetectOptimization(), 16, 0, 1, 1,
                             150, config.delay.delay_estimate_smoothing,
                             config.delay.delay_estimate_smoothing_delay_found,
                             config.delay.delay_candidate_detection_threshold),
               "");
}

// Verifies the check for non-null data dumper.
TEST(MatchedFilterDeathTest, NullDataDumper) {
  EchoCanceller3Config config;
  EXPECT_DEATH(MatchedFilter(nullptr, DetectOptimization(), 16, 1, 1, 1, 150,
                             config.delay.delay_estimate_smoothing,
                             config.delay.delay_estimate_smoothing_delay_found,
                             config.delay.delay_candidate_detection_threshold),
               "");
}

// Verifies the check for that the sub block size is a multiple of 4.
// TODO(peah): Activate the unittest once the required code has been landed.
TEST(MatchedFilterDeathTest, DISABLED_BlockSizeMultipleOf4) {
  ApmDataDumper data_dumper(0);
  EchoCanceller3Config config;
  EXPECT_DEATH(MatchedFilter(&data_dumper, DetectOptimization(), 15, 1, 1, 1,
                             150, config.delay.delay_estimate_smoothing,
                             config.delay.delay_estimate_smoothing_delay_found,
                             config.delay.delay_candidate_detection_threshold),
               "");
}

// Verifies the check for that there is an integer number of sub blocks that add
// up to a block size.
// TODO(peah): Activate the unittest once the required code has been landed.
TEST(MatchedFilterDeathTest, DISABLED_SubBlockSizeAddsUpToBlockSize) {
  ApmDataDumper data_dumper(0);
  EchoCanceller3Config config;
  EXPECT_DEATH(MatchedFilter(&data_dumper, DetectOptimization(), 12, 1, 1, 1,
                             150, config.delay.delay_estimate_smoothing,
                             config.delay.delay_estimate_smoothing_delay_found,
                             config.delay.delay_candidate_detection_threshold),
               "");
}

#endif

}  // namespace aec3
}  // namespace webrtc
