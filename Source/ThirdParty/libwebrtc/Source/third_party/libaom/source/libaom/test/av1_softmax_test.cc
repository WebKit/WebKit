/*
 * Copyright (c) 2021, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <memory>
#include <new>
#include <tuple>

#include "aom/aom_integer.h"
#include "aom_ports/aom_timer.h"
#include "av1/encoder/ml.h"
#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"
#include "config/av1_rtcd.h"
#include "gtest/gtest.h"
#include "test/acm_random.h"
#include "test/register_state_check.h"
#include "test/util.h"

namespace {
using FastSoftmaxFn = void (*)(const float *const input, float *output);
using FastSoftmaxTestParams = std::tuple<const FastSoftmaxFn, int>;

// Error thresholds for functional equivalence
constexpr float kRelEpsilon = 5e-2f;
constexpr float kAbsEpsilon = 5e-3f;

class FastSoftmaxTest : public ::testing::TestWithParam<FastSoftmaxTestParams> {
 public:
  FastSoftmaxTest() : target_fn_(GET_PARAM(0)), num_classes_(GET_PARAM(1)) {}
  void SetUp() override {
    ref_buf_.reset(new (std::nothrow) float[num_classes_]());
    ASSERT_NE(ref_buf_, nullptr);
    dst_buf_.reset(new (std::nothrow) float[num_classes_]());
    ASSERT_NE(dst_buf_, nullptr);
    input_.reset(new (std::nothrow) float[num_classes_]());
    ASSERT_NE(input_, nullptr);
  }
  void RunSoftmaxTest();
  void RunSoftmaxSpeedTest(const int run_times);
  void FillInputBuf();

 private:
  const FastSoftmaxFn target_fn_;
  const int num_classes_;
  std::unique_ptr<float[]> ref_buf_, dst_buf_, input_;
  libaom_test::ACMRandom rng_;
};

void FastSoftmaxTest::FillInputBuf() {
  for (int idx = 0; idx < num_classes_; idx++) {
    input_[idx] = ((float)rng_.Rand31() - (1 << 30)) / (1u << 30);
  }
}

void FastSoftmaxTest::RunSoftmaxTest() {
  av1_nn_softmax(input_.get(), ref_buf_.get(), num_classes_);
  target_fn_(input_.get(), dst_buf_.get());

  for (int idx = 0; idx < num_classes_; idx++) {
    if (ref_buf_[idx] < kAbsEpsilon) {
      ASSERT_LE(dst_buf_[idx], kAbsEpsilon)
          << "Reference output was near-zero, test output was not" << std::endl;
    } else {
      const float error = dst_buf_[idx] - ref_buf_[idx];
      const float relative_error = fabsf(error / ref_buf_[idx]);
      ASSERT_LE(relative_error, kRelEpsilon)
          << "Excessive relative error between reference and test output"
          << std::endl;
      ASSERT_LE(error, kAbsEpsilon)
          << "Excessive absolute error between reference and test output"
          << std::endl;
    }
  }
}

void FastSoftmaxTest::RunSoftmaxSpeedTest(const int run_times) {
  aom_usec_timer timer;
  aom_usec_timer_start(&timer);
  for (int idx = 0; idx < run_times; idx++) {
    target_fn_(input_.get(), dst_buf_.get());
  }
  aom_usec_timer_mark(&timer);
  const int64_t time = aom_usec_timer_elapsed(&timer);
  std::cout << "Test with " << num_classes_ << " classes took " << time
            << " us." << std::endl;
}

TEST_P(FastSoftmaxTest, RandomValues) {
  FillInputBuf();
  RunSoftmaxTest();
}

TEST_P(FastSoftmaxTest, DISABLED_Speed) {
  constexpr int kNumTimes = 1000000;
  RunSoftmaxSpeedTest(kNumTimes);
}

void AnchorSoftmax16Fn(const float *input, float *output) {
  av1_nn_softmax(input, output, 16);
}

const FastSoftmaxTestParams kArrayParams_c[] = {
  FastSoftmaxTestParams(AnchorSoftmax16Fn, 16),
  FastSoftmaxTestParams(av1_nn_fast_softmax_16_c, 16)
};
INSTANTIATE_TEST_SUITE_P(C, FastSoftmaxTest,
                         ::testing::ValuesIn(kArrayParams_c));

#if HAVE_SSE3 && !CONFIG_EXCLUDE_SIMD_MISMATCH
INSTANTIATE_TEST_SUITE_P(
    SSE3, FastSoftmaxTest,
    ::testing::Values(FastSoftmaxTestParams(av1_nn_fast_softmax_16_sse3, 16)));
#endif
}  // namespace
