/*
 * Copyright (c) 2018, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <tuple>

#include "gtest/gtest.h"

#include "test/register_state_check.h"
#include "test/acm_random.h"
#include "test/util.h"

#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"

#include "aom/aom_integer.h"
#include "aom_ports/aom_timer.h"
#include "av1/encoder/pickrst.h"

#define MAX_DATA_BLOCK 384

namespace pickrst_test_lowbd {
static const int kIterations = 100;

typedef int64_t (*lowbd_pixel_proj_error_func)(
    const uint8_t *src8, int width, int height, int src_stride,
    const uint8_t *dat8, int dat_stride, int32_t *flt0, int flt0_stride,
    int32_t *flt1, int flt1_stride, int xq[2], const sgr_params_type *params);

////////////////////////////////////////////////////////////////////////////////
// 8 bit
////////////////////////////////////////////////////////////////////////////////

typedef std::tuple<const lowbd_pixel_proj_error_func> PixelProjErrorTestParam;

class PixelProjErrorTest
    : public ::testing::TestWithParam<PixelProjErrorTestParam> {
 public:
  void SetUp() override {
    target_func_ = GET_PARAM(0);
    src_ = (uint8_t *)(aom_malloc(MAX_DATA_BLOCK * MAX_DATA_BLOCK *
                                  sizeof(*src_)));
    ASSERT_NE(src_, nullptr);
    dgd_ = (uint8_t *)(aom_malloc(MAX_DATA_BLOCK * MAX_DATA_BLOCK *
                                  sizeof(*dgd_)));
    ASSERT_NE(dgd_, nullptr);
    flt0_ = (int32_t *)(aom_malloc(MAX_DATA_BLOCK * MAX_DATA_BLOCK *
                                   sizeof(*flt0_)));
    ASSERT_NE(flt0_, nullptr);
    flt1_ = (int32_t *)(aom_malloc(MAX_DATA_BLOCK * MAX_DATA_BLOCK *
                                   sizeof(*flt1_)));
    ASSERT_NE(flt1_, nullptr);
  }
  void TearDown() override {
    aom_free(src_);
    aom_free(dgd_);
    aom_free(flt0_);
    aom_free(flt1_);
  }
  void RunPixelProjErrorTest(int32_t run_times);
  void RunPixelProjErrorTest_ExtremeValues();

 private:
  lowbd_pixel_proj_error_func target_func_;
  libaom_test::ACMRandom rng_;
  uint8_t *src_;
  uint8_t *dgd_;
  int32_t *flt0_;
  int32_t *flt1_;
};
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(PixelProjErrorTest);

void PixelProjErrorTest::RunPixelProjErrorTest(int32_t run_times) {
  int h_end = run_times != 1 ? 128 : (rng_.Rand16() % MAX_DATA_BLOCK) + 1;
  int v_end = run_times != 1 ? 128 : (rng_.Rand16() % MAX_DATA_BLOCK) + 1;
  const int dgd_stride = MAX_DATA_BLOCK;
  const int src_stride = MAX_DATA_BLOCK;
  const int flt0_stride = MAX_DATA_BLOCK;
  const int flt1_stride = MAX_DATA_BLOCK;
  sgr_params_type params;
  int xq[2];
  const int iters = run_times == 1 ? kIterations : 4;
  for (int iter = 0; iter < iters && !HasFatalFailure(); ++iter) {
    int64_t err_ref = 0, err_test = 1;
    for (int i = 0; i < MAX_DATA_BLOCK * MAX_DATA_BLOCK; ++i) {
      dgd_[i] = rng_.Rand8();
      src_[i] = rng_.Rand8();
      flt0_[i] = rng_.Rand15Signed();
      flt1_[i] = rng_.Rand15Signed();
    }
    xq[0] = rng_.Rand8() % (1 << SGRPROJ_PRJ_BITS);
    xq[1] = rng_.Rand8() % (1 << SGRPROJ_PRJ_BITS);
    params.r[0] = run_times == 1 ? (rng_.Rand8() % MAX_RADIUS) : (iter % 2);
    params.r[1] = run_times == 1 ? (rng_.Rand8() % MAX_RADIUS) : (iter / 2);
    params.s[0] = run_times == 1 ? (rng_.Rand8() % MAX_RADIUS) : (iter % 2);
    params.s[1] = run_times == 1 ? (rng_.Rand8() % MAX_RADIUS) : (iter / 2);
    uint8_t *dgd = dgd_;
    uint8_t *src = src_;

    aom_usec_timer timer;
    aom_usec_timer_start(&timer);
    for (int i = 0; i < run_times; ++i) {
      err_ref = av1_lowbd_pixel_proj_error_c(src, h_end, v_end, src_stride, dgd,
                                             dgd_stride, flt0_, flt0_stride,
                                             flt1_, flt1_stride, xq, &params);
    }
    aom_usec_timer_mark(&timer);
    const double time1 = static_cast<double>(aom_usec_timer_elapsed(&timer));
    aom_usec_timer_start(&timer);
    for (int i = 0; i < run_times; ++i) {
      err_test =
          target_func_(src, h_end, v_end, src_stride, dgd, dgd_stride, flt0_,
                       flt0_stride, flt1_, flt1_stride, xq, &params);
    }
    aom_usec_timer_mark(&timer);
    const double time2 = static_cast<double>(aom_usec_timer_elapsed(&timer));
    if (run_times > 10) {
      printf("r0 %d r1 %d %3dx%-3d:%7.2f/%7.2fns (%3.2f)\n", params.r[0],
             params.r[1], h_end, v_end, time1, time2, time1 / time2);
    }
    ASSERT_EQ(err_ref, err_test);
  }
}

void PixelProjErrorTest::RunPixelProjErrorTest_ExtremeValues() {
  const int h_start = 0;
  int h_end = 192;
  const int v_start = 0;
  int v_end = 192;
  const int dgd_stride = MAX_DATA_BLOCK;
  const int src_stride = MAX_DATA_BLOCK;
  const int flt0_stride = MAX_DATA_BLOCK;
  const int flt1_stride = MAX_DATA_BLOCK;
  sgr_params_type params;
  int xq[2];
  const int iters = kIterations;
  for (int iter = 0; iter < iters && !HasFatalFailure(); ++iter) {
    int64_t err_ref = 0, err_test = 1;
    for (int i = 0; i < MAX_DATA_BLOCK * MAX_DATA_BLOCK; ++i) {
      dgd_[i] = 0;
      src_[i] = 255;
      flt0_[i] = rng_.Rand15Signed();
      flt1_[i] = rng_.Rand15Signed();
    }
    xq[0] = rng_.Rand8() % (1 << SGRPROJ_PRJ_BITS);
    xq[1] = rng_.Rand8() % (1 << SGRPROJ_PRJ_BITS);
    params.r[0] = rng_.Rand8() % MAX_RADIUS;
    params.r[1] = rng_.Rand8() % MAX_RADIUS;
    params.s[0] = rng_.Rand8() % MAX_RADIUS;
    params.s[1] = rng_.Rand8() % MAX_RADIUS;
    uint8_t *dgd = dgd_;
    uint8_t *src = src_;

    err_ref = av1_lowbd_pixel_proj_error_c(
        src, h_end - h_start, v_end - v_start, src_stride, dgd, dgd_stride,
        flt0_, flt0_stride, flt1_, flt1_stride, xq, &params);

    err_test = target_func_(src, h_end - h_start, v_end - v_start, src_stride,
                            dgd, dgd_stride, flt0_, flt0_stride, flt1_,
                            flt1_stride, xq, &params);

    ASSERT_EQ(err_ref, err_test);
  }
}

TEST_P(PixelProjErrorTest, RandomValues) { RunPixelProjErrorTest(1); }

TEST_P(PixelProjErrorTest, ExtremeValues) {
  RunPixelProjErrorTest_ExtremeValues();
}

TEST_P(PixelProjErrorTest, DISABLED_Speed) { RunPixelProjErrorTest(200000); }

#if HAVE_SSE4_1
INSTANTIATE_TEST_SUITE_P(SSE4_1, PixelProjErrorTest,
                         ::testing::Values(av1_lowbd_pixel_proj_error_sse4_1));
#endif  // HAVE_SSE4_1

#if HAVE_AVX2

INSTANTIATE_TEST_SUITE_P(AVX2, PixelProjErrorTest,
                         ::testing::Values(av1_lowbd_pixel_proj_error_avx2));
#endif  // HAVE_AVX2

#if HAVE_NEON

INSTANTIATE_TEST_SUITE_P(NEON, PixelProjErrorTest,
                         ::testing::Values(av1_lowbd_pixel_proj_error_neon));
#endif  // HAVE_NEON

}  // namespace pickrst_test_lowbd

#if CONFIG_AV1_HIGHBITDEPTH
namespace pickrst_test_highbd {
static const int kIterations = 100;

typedef int64_t (*highbd_pixel_proj_error_func)(
    const uint8_t *src8, int width, int height, int src_stride,
    const uint8_t *dat8, int dat_stride, int32_t *flt0, int flt0_stride,
    int32_t *flt1, int flt1_stride, int xq[2], const sgr_params_type *params);

////////////////////////////////////////////////////////////////////////////////
// High bit-depth
////////////////////////////////////////////////////////////////////////////////

typedef std::tuple<const highbd_pixel_proj_error_func> PixelProjErrorTestParam;

class PixelProjHighbdErrorTest
    : public ::testing::TestWithParam<PixelProjErrorTestParam> {
 public:
  void SetUp() override {
    target_func_ = GET_PARAM(0);
    src_ =
        (uint16_t *)aom_malloc(MAX_DATA_BLOCK * MAX_DATA_BLOCK * sizeof(*src_));
    ASSERT_NE(src_, nullptr);
    dgd_ =
        (uint16_t *)aom_malloc(MAX_DATA_BLOCK * MAX_DATA_BLOCK * sizeof(*dgd_));
    ASSERT_NE(dgd_, nullptr);
    flt0_ =
        (int32_t *)aom_malloc(MAX_DATA_BLOCK * MAX_DATA_BLOCK * sizeof(*flt0_));
    ASSERT_NE(flt0_, nullptr);
    flt1_ =
        (int32_t *)aom_malloc(MAX_DATA_BLOCK * MAX_DATA_BLOCK * sizeof(*flt1_));
    ASSERT_NE(flt1_, nullptr);
  }
  void TearDown() override {
    aom_free(src_);
    aom_free(dgd_);
    aom_free(flt0_);
    aom_free(flt1_);
  }
  void RunPixelProjErrorTest(int32_t run_times);
  void RunPixelProjErrorTest_ExtremeValues();

 private:
  highbd_pixel_proj_error_func target_func_;
  libaom_test::ACMRandom rng_;
  uint16_t *src_;
  uint16_t *dgd_;
  int32_t *flt0_;
  int32_t *flt1_;
};
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(PixelProjHighbdErrorTest);

void PixelProjHighbdErrorTest::RunPixelProjErrorTest(int32_t run_times) {
  int h_end = run_times != 1 ? 128 : (rng_.Rand16() % MAX_DATA_BLOCK) + 1;
  int v_end = run_times != 1 ? 128 : (rng_.Rand16() % MAX_DATA_BLOCK) + 1;
  const int dgd_stride = MAX_DATA_BLOCK;
  const int src_stride = MAX_DATA_BLOCK;
  const int flt0_stride = MAX_DATA_BLOCK;
  const int flt1_stride = MAX_DATA_BLOCK;
  sgr_params_type params;
  int xq[2];
  const int iters = run_times == 1 ? kIterations : 4;
  for (int iter = 0; iter < iters && !HasFatalFailure(); ++iter) {
    int64_t err_ref = 0, err_test = 1;
    for (int i = 0; i < MAX_DATA_BLOCK * MAX_DATA_BLOCK; ++i) {
      dgd_[i] = rng_.Rand16() % (1 << 12);
      src_[i] = rng_.Rand16() % (1 << 12);
      flt0_[i] = rng_.Rand15Signed();
      flt1_[i] = rng_.Rand15Signed();
    }
    xq[0] = rng_.Rand8() % (1 << SGRPROJ_PRJ_BITS);
    xq[1] = rng_.Rand8() % (1 << SGRPROJ_PRJ_BITS);
    params.r[0] = run_times == 1 ? (rng_.Rand8() % MAX_RADIUS) : (iter % 2);
    params.r[1] = run_times == 1 ? (rng_.Rand8() % MAX_RADIUS) : (iter / 2);
    params.s[0] = run_times == 1 ? (rng_.Rand8() % MAX_RADIUS) : (iter % 2);
    params.s[1] = run_times == 1 ? (rng_.Rand8() % MAX_RADIUS) : (iter / 2);
    uint8_t *dgd8 = CONVERT_TO_BYTEPTR(dgd_);
    uint8_t *src8 = CONVERT_TO_BYTEPTR(src_);

    aom_usec_timer timer;
    aom_usec_timer_start(&timer);
    for (int i = 0; i < run_times; ++i) {
      err_ref = av1_highbd_pixel_proj_error_c(
          src8, h_end, v_end, src_stride, dgd8, dgd_stride, flt0_, flt0_stride,
          flt1_, flt1_stride, xq, &params);
    }
    aom_usec_timer_mark(&timer);
    const double time1 = static_cast<double>(aom_usec_timer_elapsed(&timer));
    aom_usec_timer_start(&timer);
    for (int i = 0; i < run_times; ++i) {
      err_test =
          target_func_(src8, h_end, v_end, src_stride, dgd8, dgd_stride, flt0_,
                       flt0_stride, flt1_, flt1_stride, xq, &params);
    }
    aom_usec_timer_mark(&timer);
    const double time2 = static_cast<double>(aom_usec_timer_elapsed(&timer));
    if (run_times > 10) {
      printf("r0 %d r1 %d %3dx%-3d:%7.2f/%7.2fns (%3.2f)\n", params.r[0],
             params.r[1], h_end, v_end, time1, time2, time1 / time2);
    }
    ASSERT_EQ(err_ref, err_test);
  }
}

void PixelProjHighbdErrorTest::RunPixelProjErrorTest_ExtremeValues() {
  const int h_start = 0;
  int h_end = 192;
  const int v_start = 0;
  int v_end = 192;
  const int dgd_stride = MAX_DATA_BLOCK;
  const int src_stride = MAX_DATA_BLOCK;
  const int flt0_stride = MAX_DATA_BLOCK;
  const int flt1_stride = MAX_DATA_BLOCK;
  sgr_params_type params;
  int xq[2];
  const int iters = kIterations;
  for (int iter = 0; iter < iters && !HasFatalFailure(); ++iter) {
    int64_t err_ref = 0, err_test = 1;
    for (int i = 0; i < MAX_DATA_BLOCK * MAX_DATA_BLOCK; ++i) {
      dgd_[i] = 0;
      src_[i] = (1 << 12) - 1;
      flt0_[i] = rng_.Rand15Signed();
      flt1_[i] = rng_.Rand15Signed();
    }
    xq[0] = rng_.Rand8() % (1 << SGRPROJ_PRJ_BITS);
    xq[1] = rng_.Rand8() % (1 << SGRPROJ_PRJ_BITS);
    params.r[0] = rng_.Rand8() % MAX_RADIUS;
    params.r[1] = rng_.Rand8() % MAX_RADIUS;
    params.s[0] = rng_.Rand8() % MAX_RADIUS;
    params.s[1] = rng_.Rand8() % MAX_RADIUS;
    uint8_t *dgd8 = CONVERT_TO_BYTEPTR(dgd_);
    uint8_t *src8 = CONVERT_TO_BYTEPTR(src_);

    err_ref = av1_highbd_pixel_proj_error_c(
        src8, h_end - h_start, v_end - v_start, src_stride, dgd8, dgd_stride,
        flt0_, flt0_stride, flt1_, flt1_stride, xq, &params);

    err_test = target_func_(src8, h_end - h_start, v_end - v_start, src_stride,
                            dgd8, dgd_stride, flt0_, flt0_stride, flt1_,
                            flt1_stride, xq, &params);

    ASSERT_EQ(err_ref, err_test);
  }
}

TEST_P(PixelProjHighbdErrorTest, RandomValues) { RunPixelProjErrorTest(1); }

TEST_P(PixelProjHighbdErrorTest, ExtremeValues) {
  RunPixelProjErrorTest_ExtremeValues();
}

TEST_P(PixelProjHighbdErrorTest, DISABLED_Speed) {
  RunPixelProjErrorTest(200000);
}

#if HAVE_SSE4_1
INSTANTIATE_TEST_SUITE_P(SSE4_1, PixelProjHighbdErrorTest,
                         ::testing::Values(av1_highbd_pixel_proj_error_sse4_1));
#endif  // HAVE_SSE4_1

#if HAVE_AVX2

INSTANTIATE_TEST_SUITE_P(AVX2, PixelProjHighbdErrorTest,
                         ::testing::Values(av1_highbd_pixel_proj_error_avx2));
#endif  // HAVE_AVX2

#if HAVE_NEON

INSTANTIATE_TEST_SUITE_P(NEON, PixelProjHighbdErrorTest,
                         ::testing::Values(av1_highbd_pixel_proj_error_neon));
#endif  // HAVE_NEON

}  // namespace pickrst_test_highbd
#endif  // CONFIG_AV1_HIGHBITDEPTH

////////////////////////////////////////////////////////////////////////////////
// Get_proj_subspace_Test
////////////////////////////////////////////////////////////////////////////////

namespace get_proj_subspace_test_lowbd {
static const int kIterations = 100;

typedef void (*set_get_proj_subspace)(const uint8_t *src8, int width,
                                      int height, int src_stride,
                                      const uint8_t *dat8, int dat_stride,
                                      int32_t *flt0, int flt0_stride,
                                      int32_t *flt1, int flt1_stride,
                                      int64_t H[2][2], int64_t C[2],
                                      const sgr_params_type *params);

typedef std::tuple<const set_get_proj_subspace> GetProjSubspaceTestParam;

class GetProjSubspaceTest
    : public ::testing::TestWithParam<GetProjSubspaceTestParam> {
 public:
  void SetUp() override {
    target_func_ = GET_PARAM(0);
    src_ = (uint8_t *)(aom_malloc(MAX_DATA_BLOCK * MAX_DATA_BLOCK *
                                  sizeof(*src_)));
    ASSERT_NE(src_, nullptr);
    dgd_ = (uint8_t *)(aom_malloc(MAX_DATA_BLOCK * MAX_DATA_BLOCK *
                                  sizeof(*dgd_)));
    ASSERT_NE(dgd_, nullptr);
    flt0_ = (int32_t *)(aom_malloc(MAX_DATA_BLOCK * MAX_DATA_BLOCK *
                                   sizeof(*flt0_)));
    ASSERT_NE(flt0_, nullptr);
    flt1_ = (int32_t *)(aom_malloc(MAX_DATA_BLOCK * MAX_DATA_BLOCK *
                                   sizeof(*flt1_)));
    ASSERT_NE(flt1_, nullptr);
  }
  void TearDown() override {
    aom_free(src_);
    aom_free(dgd_);
    aom_free(flt0_);
    aom_free(flt1_);
  }
  void RunGetProjSubspaceTest(int32_t run_times);
  void RunGetProjSubspaceTest_ExtremeValues();

 private:
  set_get_proj_subspace target_func_;
  libaom_test::ACMRandom rng_;
  uint8_t *src_;
  uint8_t *dgd_;
  int32_t *flt0_;
  int32_t *flt1_;
};
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(GetProjSubspaceTest);

void GetProjSubspaceTest::RunGetProjSubspaceTest(int32_t run_times) {
  int h_end = run_times != 1
                  ? 128
                  : ((rng_.Rand16() % MAX_DATA_BLOCK) &
                     2147483640);  // We test for widths divisible by 8.
  int v_end =
      run_times != 1 ? 128 : ((rng_.Rand16() % MAX_DATA_BLOCK) & 2147483640);
  const int dgd_stride = MAX_DATA_BLOCK;
  const int src_stride = MAX_DATA_BLOCK;
  const int flt0_stride = MAX_DATA_BLOCK;
  const int flt1_stride = MAX_DATA_BLOCK;
  sgr_params_type params;
  const int iters = run_times == 1 ? kIterations : 3;
  static constexpr int kR0[3] = { 1, 1, 0 };
  static constexpr int kR1[3] = { 1, 0, 1 };
  for (int iter = 0; iter < iters && !HasFatalFailure(); ++iter) {
    int64_t C_ref[2] = { 0 }, C_test[2] = { 0 };
    int64_t H_ref[2][2] = { { 0, 0 }, { 0, 0 } };
    int64_t H_test[2][2] = { { 0, 0 }, { 0, 0 } };
    for (int i = 0; i < MAX_DATA_BLOCK * MAX_DATA_BLOCK; ++i) {
      dgd_[i] = rng_.Rand8();
      src_[i] = rng_.Rand8();
      flt0_[i] = rng_.Rand15Signed();
      flt1_[i] = rng_.Rand15Signed();
    }

    params.r[0] = run_times == 1 ? (rng_.Rand8() % MAX_RADIUS) : kR0[iter];
    params.r[1] = run_times == 1 ? (rng_.Rand8() % MAX_RADIUS) : kR1[iter];
    uint8_t *dgd = dgd_;
    uint8_t *src = src_;

    aom_usec_timer timer;
    aom_usec_timer_start(&timer);
    for (int i = 0; i < run_times; ++i) {
      av1_calc_proj_params_c(src, v_end, h_end, src_stride, dgd, dgd_stride,
                             flt0_, flt0_stride, flt1_, flt1_stride, H_ref,
                             C_ref, &params);
    }
    aom_usec_timer_mark(&timer);
    const double time1 = static_cast<double>(aom_usec_timer_elapsed(&timer));
    aom_usec_timer_start(&timer);
    for (int i = 0; i < run_times; ++i) {
      target_func_(src, v_end, h_end, src_stride, dgd, dgd_stride, flt0_,
                   flt0_stride, flt1_, flt1_stride, H_test, C_test, &params);
    }
    aom_usec_timer_mark(&timer);
    const double time2 = static_cast<double>(aom_usec_timer_elapsed(&timer));
    if (run_times > 10) {
      printf("r0 %d r1 %d %3dx%-3d:%7.2f/%7.2fns (%3.2f)\n", params.r[0],
             params.r[1], h_end, v_end, time1, time2, time1 / time2);
    } else {
      ASSERT_EQ(H_ref[0][0], H_test[0][0]);
      ASSERT_EQ(H_ref[0][1], H_test[0][1]);
      ASSERT_EQ(H_ref[1][0], H_test[1][0]);
      ASSERT_EQ(H_ref[1][1], H_test[1][1]);
      ASSERT_EQ(C_ref[0], C_test[0]);
      ASSERT_EQ(C_ref[1], C_test[1]);
    }
  }
}

void GetProjSubspaceTest::RunGetProjSubspaceTest_ExtremeValues() {
  const int h_start = 0;
  int h_end = MAX_DATA_BLOCK;
  const int v_start = 0;
  int v_end = MAX_DATA_BLOCK;
  const int dgd_stride = MAX_DATA_BLOCK;
  const int src_stride = MAX_DATA_BLOCK;
  const int flt0_stride = MAX_DATA_BLOCK;
  const int flt1_stride = MAX_DATA_BLOCK;
  sgr_params_type params;
  const int iters = kIterations;
  static constexpr int kR0[3] = { 1, 1, 0 };
  static constexpr int kR1[3] = { 1, 0, 1 };
  for (int iter = 0; iter < iters && !HasFatalFailure(); ++iter) {
    int64_t C_ref[2] = { 0 }, C_test[2] = { 0 };
    int64_t H_ref[2][2] = { { 0, 0 }, { 0, 0 } };
    int64_t H_test[2][2] = { { 0, 0 }, { 0, 0 } };
    for (int i = 0; i < MAX_DATA_BLOCK * MAX_DATA_BLOCK; ++i) {
      dgd_[i] = 0;
      src_[i] = 255;
      flt0_[i] = rng_.Rand15Signed();
      flt1_[i] = rng_.Rand15Signed();
    }
    params.r[0] = kR0[iter % 3];
    params.r[1] = kR1[iter % 3];
    uint8_t *dgd = dgd_;
    uint8_t *src = src_;

    av1_calc_proj_params_c(src, h_end - h_start, v_end - v_start, src_stride,
                           dgd, dgd_stride, flt0_, flt0_stride, flt1_,
                           flt1_stride, H_ref, C_ref, &params);

    target_func_(src, h_end - h_start, v_end - v_start, src_stride, dgd,
                 dgd_stride, flt0_, flt0_stride, flt1_, flt1_stride, H_test,
                 C_test, &params);

    ASSERT_EQ(H_ref[0][0], H_test[0][0]);
    ASSERT_EQ(H_ref[0][1], H_test[0][1]);
    ASSERT_EQ(H_ref[1][0], H_test[1][0]);
    ASSERT_EQ(H_ref[1][1], H_test[1][1]);
    ASSERT_EQ(C_ref[0], C_test[0]);
    ASSERT_EQ(C_ref[1], C_test[1]);
  }
}

TEST_P(GetProjSubspaceTest, RandomValues) { RunGetProjSubspaceTest(1); }

TEST_P(GetProjSubspaceTest, ExtremeValues) {
  RunGetProjSubspaceTest_ExtremeValues();
}

TEST_P(GetProjSubspaceTest, DISABLED_Speed) { RunGetProjSubspaceTest(200000); }

#if HAVE_SSE4_1

INSTANTIATE_TEST_SUITE_P(SSE4_1, GetProjSubspaceTest,
                         ::testing::Values(av1_calc_proj_params_sse4_1));
#endif  // HAVE_SSE4_1

#if HAVE_AVX2

INSTANTIATE_TEST_SUITE_P(AVX2, GetProjSubspaceTest,
                         ::testing::Values(av1_calc_proj_params_avx2));
#endif  // HAVE_AVX2

#if HAVE_NEON

INSTANTIATE_TEST_SUITE_P(NEON, GetProjSubspaceTest,
                         ::testing::Values(av1_calc_proj_params_neon));
#endif  // HAVE_NEON

}  // namespace get_proj_subspace_test_lowbd

#if CONFIG_AV1_HIGHBITDEPTH
namespace get_proj_subspace_test_hbd {
static const int kIterations = 100;

typedef void (*set_get_proj_subspace_hbd)(const uint8_t *src8, int width,
                                          int height, int src_stride,
                                          const uint8_t *dat8, int dat_stride,
                                          int32_t *flt0, int flt0_stride,
                                          int32_t *flt1, int flt1_stride,
                                          int64_t H[2][2], int64_t C[2],
                                          const sgr_params_type *params);

typedef std::tuple<const set_get_proj_subspace_hbd> GetProjSubspaceHBDTestParam;

class GetProjSubspaceTestHBD
    : public ::testing::TestWithParam<GetProjSubspaceHBDTestParam> {
 public:
  void SetUp() override {
    target_func_ = GET_PARAM(0);
    src_ = (uint16_t *)(aom_malloc(MAX_DATA_BLOCK * MAX_DATA_BLOCK *
                                   sizeof(*src_)));
    ASSERT_NE(src_, nullptr);
    dgd_ = (uint16_t *)(aom_malloc(MAX_DATA_BLOCK * MAX_DATA_BLOCK *
                                   sizeof(*dgd_)));
    ASSERT_NE(dgd_, nullptr);
    flt0_ = (int32_t *)(aom_malloc(MAX_DATA_BLOCK * MAX_DATA_BLOCK *
                                   sizeof(*flt0_)));
    ASSERT_NE(flt0_, nullptr);
    flt1_ = (int32_t *)(aom_malloc(MAX_DATA_BLOCK * MAX_DATA_BLOCK *
                                   sizeof(*flt1_)));
    ASSERT_NE(flt1_, nullptr);
  }
  void TearDown() override {
    aom_free(src_);
    aom_free(dgd_);
    aom_free(flt0_);
    aom_free(flt1_);
  }
  void RunGetProjSubspaceTestHBD(int32_t run_times);
  void RunGetProjSubspaceTestHBD_ExtremeValues();

 private:
  set_get_proj_subspace_hbd target_func_;
  libaom_test::ACMRandom rng_;
  uint16_t *src_;
  uint16_t *dgd_;
  int32_t *flt0_;
  int32_t *flt1_;
};
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(GetProjSubspaceTestHBD);

void GetProjSubspaceTestHBD::RunGetProjSubspaceTestHBD(int32_t run_times) {
  int h_end = run_times != 1
                  ? 128
                  : ((rng_.Rand16() % MAX_DATA_BLOCK) &
                     2147483640);  // We test for widths divisible by 8.
  int v_end =
      run_times != 1 ? 128 : ((rng_.Rand16() % MAX_DATA_BLOCK) & 2147483640);
  const int dgd_stride = MAX_DATA_BLOCK;
  const int src_stride = MAX_DATA_BLOCK;
  const int flt0_stride = MAX_DATA_BLOCK;
  const int flt1_stride = MAX_DATA_BLOCK;
  sgr_params_type params;
  const int iters = run_times == 1 ? kIterations : 3;
  static constexpr int kR0[3] = { 1, 1, 0 };
  static constexpr int kR1[3] = { 1, 0, 1 };
  for (int iter = 0; iter < iters && !HasFatalFailure(); ++iter) {
    int64_t C_ref[2] = { 0 }, C_test[2] = { 0 };
    int64_t H_ref[2][2] = { { 0, 0 }, { 0, 0 } };
    int64_t H_test[2][2] = { { 0, 0 }, { 0, 0 } };
    for (int i = 0; i < MAX_DATA_BLOCK * MAX_DATA_BLOCK; ++i) {
      dgd_[i] = rng_.Rand16() % 4095;
      src_[i] = rng_.Rand16() % 4095;
      flt0_[i] = rng_.Rand15Signed();
      flt1_[i] = rng_.Rand15Signed();
    }

    params.r[0] = run_times == 1 ? (rng_.Rand8() % MAX_RADIUS) : kR0[iter];
    params.r[1] = run_times == 1 ? (rng_.Rand8() % MAX_RADIUS) : kR1[iter];
    uint8_t *dgd = CONVERT_TO_BYTEPTR(dgd_);
    uint8_t *src = CONVERT_TO_BYTEPTR(src_);

    aom_usec_timer timer;
    aom_usec_timer_start(&timer);
    for (int i = 0; i < run_times; ++i) {
      av1_calc_proj_params_high_bd_c(src, v_end, h_end, src_stride, dgd,
                                     dgd_stride, flt0_, flt0_stride, flt1_,
                                     flt1_stride, H_ref, C_ref, &params);
    }
    aom_usec_timer_mark(&timer);
    const double time1 = static_cast<double>(aom_usec_timer_elapsed(&timer));
    aom_usec_timer_start(&timer);
    for (int i = 0; i < run_times; ++i) {
      target_func_(src, v_end, h_end, src_stride, dgd, dgd_stride, flt0_,
                   flt0_stride, flt1_, flt1_stride, H_test, C_test, &params);
    }
    aom_usec_timer_mark(&timer);
    const double time2 = static_cast<double>(aom_usec_timer_elapsed(&timer));
    if (run_times > 10) {
      printf("r0 %d r1 %d %3dx%-3d:%7.2f/%7.2fns (%3.2f)\n", params.r[0],
             params.r[1], h_end, v_end, time1, time2, time1 / time2);
    } else {
      ASSERT_EQ(H_ref[0][0], H_test[0][0]);
      ASSERT_EQ(H_ref[0][1], H_test[0][1]);
      ASSERT_EQ(H_ref[1][0], H_test[1][0]);
      ASSERT_EQ(H_ref[1][1], H_test[1][1]);
      ASSERT_EQ(C_ref[0], C_test[0]);
      ASSERT_EQ(C_ref[1], C_test[1]);
    }
  }
}

void GetProjSubspaceTestHBD::RunGetProjSubspaceTestHBD_ExtremeValues() {
  const int h_start = 0;
  int h_end = MAX_DATA_BLOCK;
  const int v_start = 0;
  int v_end = MAX_DATA_BLOCK;
  const int dgd_stride = MAX_DATA_BLOCK;
  const int src_stride = MAX_DATA_BLOCK;
  const int flt0_stride = MAX_DATA_BLOCK;
  const int flt1_stride = MAX_DATA_BLOCK;
  sgr_params_type params;
  const int iters = kIterations;
  static constexpr int kR0[3] = { 1, 1, 0 };
  static constexpr int kR1[3] = { 1, 0, 1 };
  for (int iter = 0; iter < iters && !HasFatalFailure(); ++iter) {
    int64_t C_ref[2] = { 0 }, C_test[2] = { 0 };
    int64_t H_ref[2][2] = { { 0, 0 }, { 0, 0 } };
    int64_t H_test[2][2] = { { 0, 0 }, { 0, 0 } };
    for (int i = 0; i < MAX_DATA_BLOCK * MAX_DATA_BLOCK; ++i) {
      dgd_[i] = 0;
      src_[i] = 4095;
      flt0_[i] = rng_.Rand15Signed();
      flt1_[i] = rng_.Rand15Signed();
    }
    params.r[0] = kR0[iter % 3];
    params.r[1] = kR1[iter % 3];
    uint8_t *dgd = CONVERT_TO_BYTEPTR(dgd_);
    uint8_t *src = CONVERT_TO_BYTEPTR(src_);

    av1_calc_proj_params_high_bd_c(
        src, h_end - h_start, v_end - v_start, src_stride, dgd, dgd_stride,
        flt0_, flt0_stride, flt1_, flt1_stride, H_ref, C_ref, &params);

    target_func_(src, h_end - h_start, v_end - v_start, src_stride, dgd,
                 dgd_stride, flt0_, flt0_stride, flt1_, flt1_stride, H_test,
                 C_test, &params);

    ASSERT_EQ(H_ref[0][0], H_test[0][0]);
    ASSERT_EQ(H_ref[0][1], H_test[0][1]);
    ASSERT_EQ(H_ref[1][0], H_test[1][0]);
    ASSERT_EQ(H_ref[1][1], H_test[1][1]);
    ASSERT_EQ(C_ref[0], C_test[0]);
    ASSERT_EQ(C_ref[1], C_test[1]);
  }
}

TEST_P(GetProjSubspaceTestHBD, RandomValues) { RunGetProjSubspaceTestHBD(1); }

TEST_P(GetProjSubspaceTestHBD, ExtremeValues) {
  RunGetProjSubspaceTestHBD_ExtremeValues();
}

TEST_P(GetProjSubspaceTestHBD, DISABLED_Speed) {
  RunGetProjSubspaceTestHBD(200000);
}

#if HAVE_SSE4_1

INSTANTIATE_TEST_SUITE_P(
    SSE4_1, GetProjSubspaceTestHBD,
    ::testing::Values(av1_calc_proj_params_high_bd_sse4_1));
#endif  // HAVE_SSE4_1

#if HAVE_AVX2

INSTANTIATE_TEST_SUITE_P(AVX2, GetProjSubspaceTestHBD,
                         ::testing::Values(av1_calc_proj_params_high_bd_avx2));
#endif  // HAVE_AVX2

#if HAVE_NEON

INSTANTIATE_TEST_SUITE_P(NEON, GetProjSubspaceTestHBD,
                         ::testing::Values(av1_calc_proj_params_high_bd_neon));
#endif  // HAVE_NEON
}  // namespace get_proj_subspace_test_hbd

#endif  // CONFIG_AV1_HIGHBITDEPTH
