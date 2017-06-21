/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
  */

#include "webrtc/modules/audio_processing/aec3/matched_filter.h"

#include "webrtc/typedefs.h"
#if defined(WEBRTC_ARCH_X86_FAMILY)
#include <emmintrin.h>
#endif
#include <algorithm>
#include <sstream>
#include <string>

#include "webrtc/base/random.h"
#include "webrtc/modules/audio_processing/aec3/aec3_common.h"
#include "webrtc/modules/audio_processing/aec3/decimator_by_4.h"
#include "webrtc/modules/audio_processing/aec3/render_delay_buffer.h"
#include "webrtc/modules/audio_processing/logging/apm_data_dumper.h"
#include "webrtc/modules/audio_processing/test/echo_canceller_test_tools.h"
#include "webrtc/system_wrappers/include/cpu_features_wrapper.h"
#include "webrtc/test/gtest.h"

namespace webrtc {
namespace aec3 {
namespace {

std::string ProduceDebugText(size_t delay) {
  std::ostringstream ss;
  ss << "Delay: " << delay;
  return ss.str();
}

constexpr size_t kWindowSizeSubBlocks = 32;
constexpr size_t kAlignmentShiftSubBlocks = kWindowSizeSubBlocks * 3 / 4;
constexpr size_t kNumMatchedFilters = 4;

}  // namespace

#if defined(WEBRTC_HAS_NEON)
// Verifies that the optimized methods for NEON are similar to their reference
// counterparts.
TEST(MatchedFilter, TestNeonOptimizations) {
  Random random_generator(42U);
  std::vector<float> x(2000);
  RandomizeSampleVector(&random_generator, x);
  std::vector<float> y(kSubBlockSize);
  std::vector<float> h_NEON(512);
  std::vector<float> h(512);
  int x_index = 0;
  for (int k = 0; k < 1000; ++k) {
    RandomizeSampleVector(&random_generator, y);

    bool filters_updated = false;
    float error_sum = 0.f;
    bool filters_updated_NEON = false;
    float error_sum_NEON = 0.f;

    MatchedFilterCore_NEON(x_index, h.size() * 150.f * 150.f, x, y, h_NEON,
                           &filters_updated_NEON, &error_sum_NEON);

    MatchedFilterCore(x_index, h.size() * 150.f * 150.f, x, y, h,
                      &filters_updated, &error_sum);

    EXPECT_EQ(filters_updated, filters_updated_NEON);
    EXPECT_NEAR(error_sum, error_sum_NEON, error_sum / 100000.f);

    for (size_t j = 0; j < h.size(); ++j) {
      EXPECT_NEAR(h[j], h_NEON[j], 0.00001f);
    }

    x_index = (x_index + kSubBlockSize) % x.size();
  }
}
#endif

#if defined(WEBRTC_ARCH_X86_FAMILY)
// Verifies that the optimized methods for SSE2 are bitexact to their reference
// counterparts.
TEST(MatchedFilter, TestSse2Optimizations) {
  bool use_sse2 = (WebRtc_GetCPUInfo(kSSE2) != 0);
  if (use_sse2) {
    Random random_generator(42U);
    std::vector<float> x(2000);
    RandomizeSampleVector(&random_generator, x);
    std::vector<float> y(kSubBlockSize);
    std::vector<float> h_SSE2(512);
    std::vector<float> h(512);
    int x_index = 0;
    for (int k = 0; k < 1000; ++k) {
      RandomizeSampleVector(&random_generator, y);

      bool filters_updated = false;
      float error_sum = 0.f;
      bool filters_updated_SSE2 = false;
      float error_sum_SSE2 = 0.f;

      MatchedFilterCore_SSE2(x_index, h.size() * 150.f * 150.f, x, y, h_SSE2,
                             &filters_updated_SSE2, &error_sum_SSE2);

      MatchedFilterCore(x_index, h.size() * 150.f * 150.f, x, y, h,
                        &filters_updated, &error_sum);

      EXPECT_EQ(filters_updated, filters_updated_SSE2);
      EXPECT_NEAR(error_sum, error_sum_SSE2, error_sum / 100000.f);

      for (size_t j = 0; j < h.size(); ++j) {
        EXPECT_NEAR(h[j], h_SSE2[j], 0.00001f);
      }

      x_index = (x_index + kSubBlockSize) % x.size();
    }
  }
}

#endif

// Verifies that the matched filter produces proper lag estimates for
// artificially
// delayed signals.
TEST(MatchedFilter, LagEstimation) {
  Random random_generator(42U);
  std::vector<std::vector<float>> render(3,
                                         std::vector<float>(kBlockSize, 0.f));
  std::array<float, kBlockSize> capture;
  capture.fill(0.f);
  ApmDataDumper data_dumper(0);
  for (size_t delay_samples : {5, 64, 150, 200, 800, 1000}) {
    SCOPED_TRACE(ProduceDebugText(delay_samples));
    DecimatorBy4 capture_decimator;
    DelayBuffer<float> signal_delay_buffer(4 * delay_samples);
    MatchedFilter filter(&data_dumper, DetectOptimization(),
                         kWindowSizeSubBlocks, kNumMatchedFilters,
                         kAlignmentShiftSubBlocks);
    std::unique_ptr<RenderDelayBuffer> render_delay_buffer(
        RenderDelayBuffer::Create(3));

    // Analyze the correlation between render and capture.
    for (size_t k = 0; k < (100 + delay_samples / kSubBlockSize); ++k) {
      RandomizeSampleVector(&random_generator, render[0]);
      signal_delay_buffer.Delay(render[0], capture);
      render_delay_buffer->Insert(render);
      render_delay_buffer->UpdateBuffers();
      std::array<float, kSubBlockSize> downsampled_capture;
      capture_decimator.Decimate(capture, downsampled_capture);
      filter.Update(render_delay_buffer->GetDownsampledRenderBuffer(),
                    downsampled_capture);
    }

    // Obtain the lag estimates.
    auto lag_estimates = filter.GetLagEstimates();

    // Find which lag estimate should be the most accurate.
    rtc::Optional<size_t> expected_most_accurate_lag_estimate;
    size_t alignment_shift_sub_blocks = 0;
    for (size_t k = 0; k < kNumMatchedFilters; ++k) {
      if ((alignment_shift_sub_blocks + kWindowSizeSubBlocks / 2) *
              kSubBlockSize >
          delay_samples) {
        expected_most_accurate_lag_estimate = rtc::Optional<size_t>(k);
        break;
      }
      alignment_shift_sub_blocks += kAlignmentShiftSubBlocks;
    }
    ASSERT_TRUE(expected_most_accurate_lag_estimate);

    // Verify that the expected most accurate lag estimate is the most accurate
    // estimate.
    for (size_t k = 0; k < kNumMatchedFilters; ++k) {
      if (k != *expected_most_accurate_lag_estimate) {
        EXPECT_GT(lag_estimates[*expected_most_accurate_lag_estimate].accuracy,
                  lag_estimates[k].accuracy);
      }
    }

    // Verify that all lag estimates are updated as expected for signals
    // containing strong noise.
    for (auto& le : lag_estimates) {
      EXPECT_TRUE(le.updated);
    }

    // Verify that the expected most accurate lag estimate is reliable.
    EXPECT_TRUE(lag_estimates[*expected_most_accurate_lag_estimate].reliable);

    // Verify that the expected most accurate lag estimate is correct.
    EXPECT_EQ(delay_samples,
              lag_estimates[*expected_most_accurate_lag_estimate].lag);
  }
}

// Verifies that the matched filter does not produce reliable and accurate
// estimates for uncorrelated render and capture signals.
TEST(MatchedFilter, LagNotReliableForUncorrelatedRenderAndCapture) {
  Random random_generator(42U);
  std::vector<std::vector<float>> render(3,
                                         std::vector<float>(kBlockSize, 0.f));
  std::array<float, kSubBlockSize> capture;
  capture.fill(0.f);
  ApmDataDumper data_dumper(0);
  std::unique_ptr<RenderDelayBuffer> render_delay_buffer(
      RenderDelayBuffer::Create(3));
  MatchedFilter filter(&data_dumper, DetectOptimization(), kWindowSizeSubBlocks,
                       kNumMatchedFilters, kAlignmentShiftSubBlocks);

  // Analyze the correlation between render and capture.
  for (size_t k = 0; k < 100; ++k) {
    RandomizeSampleVector(&random_generator, render[0]);
    RandomizeSampleVector(&random_generator, capture);
    render_delay_buffer->Insert(render);
    filter.Update(render_delay_buffer->GetDownsampledRenderBuffer(), capture);
  }

  // Obtain the lag estimates.
  auto lag_estimates = filter.GetLagEstimates();
  EXPECT_EQ(kNumMatchedFilters, lag_estimates.size());

  // Verify that no lag estimates are reliable.
  for (auto& le : lag_estimates) {
    EXPECT_FALSE(le.reliable);
  }
}

// Verifies that the matched filter does not produce updated lag estimates for
// render signals of low level.
TEST(MatchedFilter, LagNotUpdatedForLowLevelRender) {
  Random random_generator(42U);
  std::vector<std::vector<float>> render(3,
                                         std::vector<float>(kBlockSize, 0.f));
  std::array<float, kBlockSize> capture;
  capture.fill(0.f);
  ApmDataDumper data_dumper(0);
  MatchedFilter filter(&data_dumper, DetectOptimization(), kWindowSizeSubBlocks,
                       kNumMatchedFilters, kAlignmentShiftSubBlocks);
  std::unique_ptr<RenderDelayBuffer> render_delay_buffer(
      RenderDelayBuffer::Create(3));
  DecimatorBy4 capture_decimator;

  // Analyze the correlation between render and capture.
  for (size_t k = 0; k < 100; ++k) {
    RandomizeSampleVector(&random_generator, render[0]);
    for (auto& render_k : render[0]) {
      render_k *= 149.f / 32767.f;
    }
    std::copy(render[0].begin(), render[0].end(), capture.begin());
    std::array<float, kSubBlockSize> downsampled_capture;
    capture_decimator.Decimate(capture, downsampled_capture);
    filter.Update(render_delay_buffer->GetDownsampledRenderBuffer(),
                  downsampled_capture);
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

// Verifies that the correct number of lag estimates are produced for a certain
// number of alignment shifts.
TEST(MatchedFilter, NumberOfLagEstimates) {
  ApmDataDumper data_dumper(0);
  for (size_t num_matched_filters = 0; num_matched_filters < 10;
       ++num_matched_filters) {
    MatchedFilter filter(&data_dumper, DetectOptimization(), 32,
                         num_matched_filters, 1);
    EXPECT_EQ(num_matched_filters, filter.GetLagEstimates().size());
  }
}

#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)

// Verifies the check for non-zero windows size.
TEST(MatchedFilter, ZeroWindowSize) {
  ApmDataDumper data_dumper(0);
  EXPECT_DEATH(MatchedFilter(&data_dumper, DetectOptimization(), 0, 1, 1), "");
}

// Verifies the check for non-null data dumper.
TEST(MatchedFilter, NullDataDumper) {
  EXPECT_DEATH(MatchedFilter(nullptr, DetectOptimization(), 1, 1, 1), "");
}

#endif

}  // namespace aec3
}  // namespace webrtc
