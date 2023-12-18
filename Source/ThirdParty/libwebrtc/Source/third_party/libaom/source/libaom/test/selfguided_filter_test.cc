/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <ctime>
#include <tuple>

#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

#include "config/av1_rtcd.h"

#include "test/acm_random.h"
#include "test/register_state_check.h"
#include "test/util.h"

#include "aom_ports/aom_timer.h"
#include "av1/common/mv.h"
#include "av1/common/restoration.h"

namespace {

using libaom_test::ACMRandom;
using std::make_tuple;
using std::tuple;

typedef void (*SgrFunc)(const uint8_t *dat8, int width, int height, int stride,
                        int eps, const int *xqd, uint8_t *dst8, int dst_stride,
                        int32_t *tmpbuf, int bit_depth, int highbd);

// Test parameter list:
//  <tst_fun_>
typedef tuple<SgrFunc> FilterTestParam;

class AV1SelfguidedFilterTest
    : public ::testing::TestWithParam<FilterTestParam> {
 public:
  ~AV1SelfguidedFilterTest() override = default;
  void SetUp() override {}

 protected:
  void RunSpeedTest() {
    tst_fun_ = GET_PARAM(0);
    const int pu_width = RESTORATION_PROC_UNIT_SIZE;
    const int pu_height = RESTORATION_PROC_UNIT_SIZE;
    const int width = 256, height = 256, stride = 288, out_stride = 288;
    const int NUM_ITERS = 2000;
    int i, j, k;

    uint8_t *input_ =
        (uint8_t *)aom_memalign(32, stride * (height + 32) * sizeof(uint8_t));
    ASSERT_NE(input_, nullptr);
    uint8_t *output_ = (uint8_t *)aom_memalign(
        32, out_stride * (height + 32) * sizeof(uint8_t));
    ASSERT_NE(output_, nullptr);
    int32_t *tmpbuf = (int32_t *)aom_memalign(32, RESTORATION_TMPBUF_SIZE);
    ASSERT_NE(tmpbuf, nullptr);
    uint8_t *input = input_ + stride * 16 + 16;
    uint8_t *output = output_ + out_stride * 16 + 16;

    ACMRandom rnd(ACMRandom::DeterministicSeed());

    for (i = -16; i < height + 16; ++i)
      for (j = -16; j < width + 16; ++j)
        input[i * stride + j] = rnd.Rand16() & 0xFF;

    int xqd[2] = { SGRPROJ_PRJ_MIN0 + rnd.PseudoUniform(SGRPROJ_PRJ_MAX0 + 1 -
                                                        SGRPROJ_PRJ_MIN0),
                   SGRPROJ_PRJ_MIN1 + rnd.PseudoUniform(SGRPROJ_PRJ_MAX1 + 1 -
                                                        SGRPROJ_PRJ_MIN1) };
    // Fix a parameter set, since the speed depends slightly on r.
    // Change this to test different combinations of values of r.
    int eps = 15;

    av1_loop_restoration_precal();

    aom_usec_timer ref_timer;
    aom_usec_timer_start(&ref_timer);
    for (i = 0; i < NUM_ITERS; ++i) {
      for (k = 0; k < height; k += pu_height)
        for (j = 0; j < width; j += pu_width) {
          int w = AOMMIN(pu_width, width - j);
          int h = AOMMIN(pu_height, height - k);
          uint8_t *input_p = input + k * stride + j;
          uint8_t *output_p = output + k * out_stride + j;
          av1_apply_selfguided_restoration_c(input_p, w, h, stride, eps, xqd,
                                             output_p, out_stride, tmpbuf, 8,
                                             0);
        }
    }
    aom_usec_timer_mark(&ref_timer);
    const int64_t ref_time = aom_usec_timer_elapsed(&ref_timer);

    aom_usec_timer tst_timer;
    aom_usec_timer_start(&tst_timer);
    for (i = 0; i < NUM_ITERS; ++i) {
      for (k = 0; k < height; k += pu_height)
        for (j = 0; j < width; j += pu_width) {
          int w = AOMMIN(pu_width, width - j);
          int h = AOMMIN(pu_height, height - k);
          uint8_t *input_p = input + k * stride + j;
          uint8_t *output_p = output + k * out_stride + j;
          tst_fun_(input_p, w, h, stride, eps, xqd, output_p, out_stride,
                   tmpbuf, 8, 0);
        }
    }
    aom_usec_timer_mark(&tst_timer);
    const int64_t tst_time = aom_usec_timer_elapsed(&tst_timer);

    std::cout << "[          ] C time = " << ref_time / 1000
              << " ms, SIMD time = " << tst_time / 1000 << " ms\n";

    EXPECT_GT(ref_time, tst_time)
        << "Error: AV1SelfguidedFilterTest.SpeedTest, SIMD slower than C.\n"
        << "C time: " << ref_time << " us\n"
        << "SIMD time: " << tst_time << " us\n";

    aom_free(input_);
    aom_free(output_);
    aom_free(tmpbuf);
  }

  void RunCorrectnessTest() {
    tst_fun_ = GET_PARAM(0);
    const int pu_width = RESTORATION_PROC_UNIT_SIZE;
    const int pu_height = RESTORATION_PROC_UNIT_SIZE;
    // Set the maximum width/height to test here. We actually test a small
    // range of sizes *up to* this size, so that we can check, eg.,
    // the behaviour on tiles which are not a multiple of 4 wide.
    const int max_w = 260, max_h = 260, stride = 672, out_stride = 672;
    const int NUM_ITERS = 81;
    int i, j, k;

    uint8_t *input_ =
        (uint8_t *)aom_memalign(32, stride * (max_h + 32) * sizeof(uint8_t));
    ASSERT_NE(input_, nullptr);
    uint8_t *output_ = (uint8_t *)aom_memalign(
        32, out_stride * (max_h + 32) * sizeof(uint8_t));
    ASSERT_NE(output_, nullptr);
    uint8_t *output2_ = (uint8_t *)aom_memalign(
        32, out_stride * (max_h + 32) * sizeof(uint8_t));
    ASSERT_NE(output2_, nullptr);
    int32_t *tmpbuf = (int32_t *)aom_memalign(32, RESTORATION_TMPBUF_SIZE);
    ASSERT_NE(tmpbuf, nullptr);

    uint8_t *input = input_ + stride * 16 + 16;
    uint8_t *output = output_ + out_stride * 16 + 16;
    uint8_t *output2 = output2_ + out_stride * 16 + 16;

    ACMRandom rnd(ACMRandom::DeterministicSeed());

    av1_loop_restoration_precal();

    for (i = 0; i < NUM_ITERS; ++i) {
      for (j = -16; j < max_h + 16; ++j)
        for (k = -16; k < max_w + 16; ++k)
          input[j * stride + k] = rnd.Rand16() & 0xFF;

      int xqd[2] = { SGRPROJ_PRJ_MIN0 + rnd.PseudoUniform(SGRPROJ_PRJ_MAX0 + 1 -
                                                          SGRPROJ_PRJ_MIN0),
                     SGRPROJ_PRJ_MIN1 + rnd.PseudoUniform(SGRPROJ_PRJ_MAX1 + 1 -
                                                          SGRPROJ_PRJ_MIN1) };
      int eps = rnd.PseudoUniform(1 << SGRPROJ_PARAMS_BITS);

      // Test various tile sizes around 256x256
      int test_w = max_w - (i / 9);
      int test_h = max_h - (i % 9);

      for (k = 0; k < test_h; k += pu_height)
        for (j = 0; j < test_w; j += pu_width) {
          int w = AOMMIN(pu_width, test_w - j);
          int h = AOMMIN(pu_height, test_h - k);
          uint8_t *input_p = input + k * stride + j;
          uint8_t *output_p = output + k * out_stride + j;
          uint8_t *output2_p = output2 + k * out_stride + j;
          tst_fun_(input_p, w, h, stride, eps, xqd, output_p, out_stride,
                   tmpbuf, 8, 0);
          av1_apply_selfguided_restoration_c(input_p, w, h, stride, eps, xqd,
                                             output2_p, out_stride, tmpbuf, 8,
                                             0);
        }

      for (j = 0; j < test_h; ++j)
        for (k = 0; k < test_w; ++k) {
          ASSERT_EQ(output[j * out_stride + k], output2[j * out_stride + k]);
        }
    }

    aom_free(input_);
    aom_free(output_);
    aom_free(output2_);
    aom_free(tmpbuf);
  }

 private:
  SgrFunc tst_fun_;
};
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(AV1SelfguidedFilterTest);

TEST_P(AV1SelfguidedFilterTest, DISABLED_SpeedTest) { RunSpeedTest(); }
TEST_P(AV1SelfguidedFilterTest, CorrectnessTest) { RunCorrectnessTest(); }

#if HAVE_SSE4_1
INSTANTIATE_TEST_SUITE_P(
    SSE4_1, AV1SelfguidedFilterTest,
    ::testing::Values(av1_apply_selfguided_restoration_sse4_1));
#endif

#if HAVE_AVX2
INSTANTIATE_TEST_SUITE_P(
    AVX2, AV1SelfguidedFilterTest,
    ::testing::Values(av1_apply_selfguided_restoration_avx2));
#endif

#if HAVE_NEON
INSTANTIATE_TEST_SUITE_P(
    NEON, AV1SelfguidedFilterTest,
    ::testing::Values(av1_apply_selfguided_restoration_neon));
#endif

#if CONFIG_AV1_HIGHBITDEPTH
// Test parameter list:
//  <tst_fun_, bit_depth>
typedef tuple<SgrFunc, int> HighbdFilterTestParam;

class AV1HighbdSelfguidedFilterTest
    : public ::testing::TestWithParam<HighbdFilterTestParam> {
 public:
  ~AV1HighbdSelfguidedFilterTest() override = default;
  void SetUp() override {}

 protected:
  void RunSpeedTest() {
    tst_fun_ = GET_PARAM(0);
    const int pu_width = RESTORATION_PROC_UNIT_SIZE;
    const int pu_height = RESTORATION_PROC_UNIT_SIZE;
    const int width = 256, height = 256, stride = 288, out_stride = 288;
    const int NUM_ITERS = 2000;
    int i, j, k;
    int bit_depth = GET_PARAM(1);
    int mask = (1 << bit_depth) - 1;

    uint16_t *input_ =
        (uint16_t *)aom_memalign(32, stride * (height + 32) * sizeof(uint16_t));
    ASSERT_NE(input_, nullptr);
    uint16_t *output_ = (uint16_t *)aom_memalign(
        32, out_stride * (height + 32) * sizeof(uint16_t));
    ASSERT_NE(output_, nullptr);
    int32_t *tmpbuf = (int32_t *)aom_memalign(32, RESTORATION_TMPBUF_SIZE);
    ASSERT_NE(tmpbuf, nullptr);
    uint16_t *input = input_ + stride * 16 + 16;
    uint16_t *output = output_ + out_stride * 16 + 16;

    ACMRandom rnd(ACMRandom::DeterministicSeed());

    for (i = -16; i < height + 16; ++i)
      for (j = -16; j < width + 16; ++j)
        input[i * stride + j] = rnd.Rand16() & mask;

    int xqd[2] = { SGRPROJ_PRJ_MIN0 + rnd.PseudoUniform(SGRPROJ_PRJ_MAX0 + 1 -
                                                        SGRPROJ_PRJ_MIN0),
                   SGRPROJ_PRJ_MIN1 + rnd.PseudoUniform(SGRPROJ_PRJ_MAX1 + 1 -
                                                        SGRPROJ_PRJ_MIN1) };
    // Fix a parameter set, since the speed depends slightly on r.
    // Change this to test different combinations of values of r.
    int eps = 15;

    av1_loop_restoration_precal();

    aom_usec_timer ref_timer;
    aom_usec_timer_start(&ref_timer);
    for (i = 0; i < NUM_ITERS; ++i) {
      for (k = 0; k < height; k += pu_height)
        for (j = 0; j < width; j += pu_width) {
          int w = AOMMIN(pu_width, width - j);
          int h = AOMMIN(pu_height, height - k);
          uint16_t *input_p = input + k * stride + j;
          uint16_t *output_p = output + k * out_stride + j;
          av1_apply_selfguided_restoration_c(
              CONVERT_TO_BYTEPTR(input_p), w, h, stride, eps, xqd,
              CONVERT_TO_BYTEPTR(output_p), out_stride, tmpbuf, bit_depth, 1);
        }
    }
    aom_usec_timer_mark(&ref_timer);
    const int64_t ref_time = aom_usec_timer_elapsed(&ref_timer);

    aom_usec_timer tst_timer;
    aom_usec_timer_start(&tst_timer);
    for (i = 0; i < NUM_ITERS; ++i) {
      for (k = 0; k < height; k += pu_height)
        for (j = 0; j < width; j += pu_width) {
          int w = AOMMIN(pu_width, width - j);
          int h = AOMMIN(pu_height, height - k);
          uint16_t *input_p = input + k * stride + j;
          uint16_t *output_p = output + k * out_stride + j;
          tst_fun_(CONVERT_TO_BYTEPTR(input_p), w, h, stride, eps, xqd,
                   CONVERT_TO_BYTEPTR(output_p), out_stride, tmpbuf, bit_depth,
                   1);
        }
    }
    aom_usec_timer_mark(&tst_timer);
    const int64_t tst_time = aom_usec_timer_elapsed(&tst_timer);

    std::cout << "[          ] C time = " << ref_time / 1000
              << " ms, SIMD time = " << tst_time / 1000 << " ms\n";

    EXPECT_GT(ref_time, tst_time)
        << "Error: AV1HighbdSelfguidedFilterTest.SpeedTest, SIMD slower than "
           "C.\n"
        << "C time: " << ref_time << " us\n"
        << "SIMD time: " << tst_time << " us\n";

    aom_free(input_);
    aom_free(output_);
    aom_free(tmpbuf);
  }

  void RunCorrectnessTest() {
    tst_fun_ = GET_PARAM(0);
    const int pu_width = RESTORATION_PROC_UNIT_SIZE;
    const int pu_height = RESTORATION_PROC_UNIT_SIZE;
    // Set the maximum width/height to test here. We actually test a small
    // range of sizes *up to* this size, so that we can check, eg.,
    // the behaviour on tiles which are not a multiple of 4 wide.
    const int max_w = 260, max_h = 260, stride = 672, out_stride = 672;
    const int NUM_ITERS = 81;
    int i, j, k;
    int bit_depth = GET_PARAM(1);
    int mask = (1 << bit_depth) - 1;

    uint16_t *input_ =
        (uint16_t *)aom_memalign(32, stride * (max_h + 32) * sizeof(uint16_t));
    ASSERT_NE(input_, nullptr);
    uint16_t *output_ = (uint16_t *)aom_memalign(
        32, out_stride * (max_h + 32) * sizeof(uint16_t));
    ASSERT_NE(output_, nullptr);
    uint16_t *output2_ = (uint16_t *)aom_memalign(
        32, out_stride * (max_h + 32) * sizeof(uint16_t));
    ASSERT_NE(output2_, nullptr);
    int32_t *tmpbuf = (int32_t *)aom_memalign(32, RESTORATION_TMPBUF_SIZE);
    ASSERT_NE(tmpbuf, nullptr);

    uint16_t *input = input_ + stride * 16 + 16;
    uint16_t *output = output_ + out_stride * 16 + 16;
    uint16_t *output2 = output2_ + out_stride * 16 + 16;

    ACMRandom rnd(ACMRandom::DeterministicSeed());

    av1_loop_restoration_precal();

    for (i = 0; i < NUM_ITERS; ++i) {
      for (j = -16; j < max_h + 16; ++j)
        for (k = -16; k < max_w + 16; ++k)
          input[j * stride + k] = rnd.Rand16() & mask;

      int xqd[2] = { SGRPROJ_PRJ_MIN0 + rnd.PseudoUniform(SGRPROJ_PRJ_MAX0 + 1 -
                                                          SGRPROJ_PRJ_MIN0),
                     SGRPROJ_PRJ_MIN1 + rnd.PseudoUniform(SGRPROJ_PRJ_MAX1 + 1 -
                                                          SGRPROJ_PRJ_MIN1) };
      int eps = rnd.PseudoUniform(1 << SGRPROJ_PARAMS_BITS);

      // Test various tile sizes around 256x256
      int test_w = max_w - (i / 9);
      int test_h = max_h - (i % 9);

      for (k = 0; k < test_h; k += pu_height)
        for (j = 0; j < test_w; j += pu_width) {
          int w = AOMMIN(pu_width, test_w - j);
          int h = AOMMIN(pu_height, test_h - k);
          uint16_t *input_p = input + k * stride + j;
          uint16_t *output_p = output + k * out_stride + j;
          uint16_t *output2_p = output2 + k * out_stride + j;
          tst_fun_(CONVERT_TO_BYTEPTR(input_p), w, h, stride, eps, xqd,
                   CONVERT_TO_BYTEPTR(output_p), out_stride, tmpbuf, bit_depth,
                   1);
          av1_apply_selfguided_restoration_c(
              CONVERT_TO_BYTEPTR(input_p), w, h, stride, eps, xqd,
              CONVERT_TO_BYTEPTR(output2_p), out_stride, tmpbuf, bit_depth, 1);
        }

      for (j = 0; j < test_h; ++j)
        for (k = 0; k < test_w; ++k)
          ASSERT_EQ(output[j * out_stride + k], output2[j * out_stride + k]);
    }

    aom_free(input_);
    aom_free(output_);
    aom_free(output2_);
    aom_free(tmpbuf);
  }

 private:
  SgrFunc tst_fun_;
};
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(AV1HighbdSelfguidedFilterTest);

TEST_P(AV1HighbdSelfguidedFilterTest, DISABLED_SpeedTest) { RunSpeedTest(); }
TEST_P(AV1HighbdSelfguidedFilterTest, CorrectnessTest) { RunCorrectnessTest(); }

#if HAVE_SSE4_1
const int highbd_params_sse4_1[] = { 8, 10, 12 };
INSTANTIATE_TEST_SUITE_P(
    SSE4_1, AV1HighbdSelfguidedFilterTest,
    ::testing::Combine(
        ::testing::Values(av1_apply_selfguided_restoration_sse4_1),
        ::testing::ValuesIn(highbd_params_sse4_1)));
#endif

#if HAVE_AVX2
const int highbd_params_avx2[] = { 8, 10, 12 };
INSTANTIATE_TEST_SUITE_P(
    AVX2, AV1HighbdSelfguidedFilterTest,
    ::testing::Combine(::testing::Values(av1_apply_selfguided_restoration_avx2),
                       ::testing::ValuesIn(highbd_params_avx2)));
#endif

#if HAVE_NEON
const int highbd_params_neon[] = { 8, 10, 12 };
INSTANTIATE_TEST_SUITE_P(
    NEON, AV1HighbdSelfguidedFilterTest,
    ::testing::Combine(::testing::Values(av1_apply_selfguided_restoration_neon),
                       ::testing::ValuesIn(highbd_params_neon)));
#endif
#endif  // CONFIG_AV1_HIGHBITDEPTH
}  // namespace
