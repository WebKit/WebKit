/*
 * Copyright (c) 2017, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <tuple>

#include "config/aom_config.h"
#include "config/av1_rtcd.h"

#include "aom_ports/mem.h"
#include "av1/common/scan.h"
#include "av1/common/txb_common.h"
#include "test/acm_random.h"
#include "test/register_state_check.h"
#include "test/util.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

namespace {
using libaom_test::ACMRandom;

using BuildCompDiffWtdMaskFunc = void (*)(uint8_t *mask,
                                          DIFFWTD_MASK_TYPE mask_type,
                                          const uint8_t *src0, int src0_stride,
                                          const uint8_t *src1, int src1_stride,
                                          int h, int w);

using BuildCompDiffwtdMaskDParam =
    std::tuple<BLOCK_SIZE, BuildCompDiffWtdMaskFunc>;

#if HAVE_SSE4_1 || HAVE_AVX2 || HAVE_NEON
::testing::internal::ParamGenerator<BuildCompDiffwtdMaskDParam> BuildParams(
    BuildCompDiffWtdMaskFunc filter) {
  return ::testing::Combine(::testing::Range(BLOCK_4X4, BLOCK_SIZES_ALL),
                            ::testing::Values(filter));
}
#endif

class BuildCompDiffwtdMaskTest
    : public ::testing::TestWithParam<BuildCompDiffwtdMaskDParam> {
 public:
  BuildCompDiffwtdMaskTest() : rnd_(ACMRandom::DeterministicSeed()) {}
  ~BuildCompDiffwtdMaskTest() override = default;

 protected:
  void RunTest(BuildCompDiffWtdMaskFunc test_impl, bool is_speed,
               const DIFFWTD_MASK_TYPE type) {
    const int sb_type = GET_PARAM(0);
    const int width = block_size_wide[sb_type];
    const int height = block_size_high[sb_type];
    DECLARE_ALIGNED(16, uint8_t, mask_ref[MAX_SB_SQUARE]);
    DECLARE_ALIGNED(16, uint8_t, mask_test[MAX_SB_SQUARE]);
    DECLARE_ALIGNED(16, uint8_t, src0[MAX_SB_SQUARE]);
    DECLARE_ALIGNED(16, uint8_t, src1[MAX_SB_SQUARE]);
    for (int i = 0; i < width * height; i++) {
      src0[i] = rnd_.Rand8();
      src1[i] = rnd_.Rand8();
    }
    const int run_times = is_speed ? (10000000 / (width + height)) : 1;
    aom_usec_timer timer;
    aom_usec_timer_start(&timer);
    for (int i = 0; i < run_times; ++i) {
      av1_build_compound_diffwtd_mask_c(mask_ref, type, src0, width, src1,
                                        width, height, width);
    }
    const double t1 = get_time_mark(&timer);
    aom_usec_timer_start(&timer);
    for (int i = 0; i < run_times; ++i) {
      test_impl(mask_test, type, src0, width, src1, width, height, width);
    }
    const double t2 = get_time_mark(&timer);
    if (is_speed) {
      printf("mask %d %3dx%-3d:%7.2f/%7.2fns", type, width, height, t1, t2);
      printf("(%3.2f)\n", t1 / t2);
    }
    for (int r = 0; r < height; ++r) {
      for (int c = 0; c < width; ++c) {
        ASSERT_EQ(mask_ref[c + r * width], mask_test[c + r * width])
            << "[" << r << "," << c << "] " << run_times << " @ " << width
            << "x" << height << " inv " << type;
      }
    }
  }

 private:
  ACMRandom rnd_;
};
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(BuildCompDiffwtdMaskTest);

TEST_P(BuildCompDiffwtdMaskTest, match) {
  RunTest(GET_PARAM(1), 0, DIFFWTD_38);
  RunTest(GET_PARAM(1), 0, DIFFWTD_38_INV);
}
TEST_P(BuildCompDiffwtdMaskTest, DISABLED_Speed) {
  RunTest(GET_PARAM(1), 1, DIFFWTD_38);
  RunTest(GET_PARAM(1), 1, DIFFWTD_38_INV);
}

#if HAVE_SSE4_1
INSTANTIATE_TEST_SUITE_P(SSE4_1, BuildCompDiffwtdMaskTest,
                         BuildParams(av1_build_compound_diffwtd_mask_sse4_1));
#endif

#if HAVE_AVX2
INSTANTIATE_TEST_SUITE_P(AVX2, BuildCompDiffwtdMaskTest,
                         BuildParams(av1_build_compound_diffwtd_mask_avx2));
#endif

#if HAVE_NEON
INSTANTIATE_TEST_SUITE_P(NEON, BuildCompDiffwtdMaskTest,
                         BuildParams(av1_build_compound_diffwtd_mask_neon));
#endif

#if CONFIG_AV1_HIGHBITDEPTH

using BuildCompDiffWtdMaskHighbdFunc =
    void (*)(uint8_t *mask, DIFFWTD_MASK_TYPE mask_type, const uint8_t *src0,
             int src0_stride, const uint8_t *src1, int src1_stride, int h,
             int w, int bd);

using BuildCompDiffwtdMaskHighbdParam =
    std::tuple<BLOCK_SIZE, int, BuildCompDiffWtdMaskHighbdFunc>;

#if HAVE_SSSE3 || HAVE_AVX2 || HAVE_NEON
::testing::internal::ParamGenerator<BuildCompDiffwtdMaskHighbdParam>
BuildParamsHighbd(BuildCompDiffWtdMaskHighbdFunc filter) {
  return ::testing::Combine(::testing::Range(BLOCK_4X4, BLOCK_SIZES_ALL),
                            ::testing::Values(8, 10, 12),
                            ::testing::Values(filter));
}
#endif

class BuildCompDiffwtdMaskHighbdTest
    : public ::testing::TestWithParam<BuildCompDiffwtdMaskHighbdParam> {
 public:
  BuildCompDiffwtdMaskHighbdTest() : rnd_(ACMRandom::DeterministicSeed()) {}
  ~BuildCompDiffwtdMaskHighbdTest() override = default;

 protected:
  void RunTest(BuildCompDiffWtdMaskHighbdFunc test_impl, bool is_speed,
               const DIFFWTD_MASK_TYPE type) {
    const int sb_type = GET_PARAM(0);
    const int bd = GET_PARAM(1);
    const int width = block_size_wide[sb_type];
    const int height = block_size_high[sb_type];
    const int mask = (1 << bd) - 1;
    DECLARE_ALIGNED(16, uint8_t, mask_ref[MAX_SB_SQUARE]);
    DECLARE_ALIGNED(16, uint8_t, mask_test[MAX_SB_SQUARE]);
    DECLARE_ALIGNED(16, uint16_t, src0[MAX_SB_SQUARE]);
    DECLARE_ALIGNED(16, uint16_t, src1[MAX_SB_SQUARE]);
    for (int i = 0; i < width * height; i++) {
      src0[i] = rnd_.Rand16() & mask;
      src1[i] = rnd_.Rand16() & mask;
    }
    const int run_times = is_speed ? (10000000 / (width + height)) : 1;
    aom_usec_timer timer;

    aom_usec_timer_start(&timer);
    for (int i = 0; i < run_times; ++i) {
      uint8_t *src0_8 = CONVERT_TO_BYTEPTR(src0);
      uint8_t *src1_8 = CONVERT_TO_BYTEPTR(src1);
      av1_build_compound_diffwtd_mask_highbd_c(
          mask_ref, type, src0_8, width, src1_8, width, height, width, bd);
    }
    const double t1 = get_time_mark(&timer);

    aom_usec_timer_start(&timer);
    for (int i = 0; i < run_times; ++i) {
      uint8_t *src0_8 = CONVERT_TO_BYTEPTR(src0);
      uint8_t *src1_8 = CONVERT_TO_BYTEPTR(src1);
      test_impl(mask_test, type, src0_8, width, src1_8, width, height, width,
                bd);
    }
    const double t2 = get_time_mark(&timer);

    if (is_speed) {
      printf("mask %d %3dx%-3d:%7.2f/%7.2fns", type, width, height, t1, t2);
      printf("(%3.2f)\n", t1 / t2);
    }
    for (int r = 0; r < height; ++r) {
      for (int c = 0; c < width; ++c) {
        ASSERT_EQ(mask_ref[c + r * width], mask_test[c + r * width])
            << "[" << r << "," << c << "] " << run_times << " @ " << width
            << "x" << height << " inv " << type;
      }
    }
  }

 private:
  ACMRandom rnd_;
};
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(BuildCompDiffwtdMaskHighbdTest);

TEST_P(BuildCompDiffwtdMaskHighbdTest, match) {
  RunTest(GET_PARAM(2), 0, DIFFWTD_38);
  RunTest(GET_PARAM(2), 0, DIFFWTD_38_INV);
}
TEST_P(BuildCompDiffwtdMaskHighbdTest, DISABLED_Speed) {
  RunTest(GET_PARAM(2), 1, DIFFWTD_38);
  RunTest(GET_PARAM(2), 1, DIFFWTD_38_INV);
}

#if HAVE_SSSE3
INSTANTIATE_TEST_SUITE_P(
    SSSE3, BuildCompDiffwtdMaskHighbdTest,
    BuildParamsHighbd(av1_build_compound_diffwtd_mask_highbd_ssse3));
#endif

#if HAVE_AVX2
INSTANTIATE_TEST_SUITE_P(
    AVX2, BuildCompDiffwtdMaskHighbdTest,
    BuildParamsHighbd(av1_build_compound_diffwtd_mask_highbd_avx2));
#endif

#if HAVE_NEON
INSTANTIATE_TEST_SUITE_P(
    NEON, BuildCompDiffwtdMaskHighbdTest,
    BuildParamsHighbd(av1_build_compound_diffwtd_mask_highbd_neon));
#endif
#endif  // CONFIG_AV1_HIGHBITDEPTH

using BuildCompDiffWtdMaskD16Func = void (*)(
    uint8_t *mask, DIFFWTD_MASK_TYPE mask_type, const CONV_BUF_TYPE *src0,
    int src0_stride, const CONV_BUF_TYPE *src1, int src1_stride, int h, int w,
    ConvolveParams *conv_params, int bd);

using BuildCompDiffwtdMaskD16Param =
    std::tuple<int, BuildCompDiffWtdMaskD16Func, BLOCK_SIZE>;

#if HAVE_SSE4_1 || HAVE_AVX2 || HAVE_NEON
::testing::internal::ParamGenerator<BuildCompDiffwtdMaskD16Param> BuildParams(
    BuildCompDiffWtdMaskD16Func filter) {
  return ::testing::Combine(::testing::Range(8, 13, 2),
                            ::testing::Values(filter),
                            ::testing::Range(BLOCK_4X4, BLOCK_SIZES_ALL));
}
#endif

class BuildCompDiffwtdMaskD16Test
    : public ::testing::TestWithParam<BuildCompDiffwtdMaskD16Param> {
 public:
  BuildCompDiffwtdMaskD16Test() : rnd_(ACMRandom::DeterministicSeed()) {}
  ~BuildCompDiffwtdMaskD16Test() override = default;

 protected:
  void RunCheckOutput(BuildCompDiffWtdMaskD16Func test_impl) {
    const int block_idx = GET_PARAM(2);
    const int bd = GET_PARAM(0);
    const int width = block_size_wide[block_idx];
    const int height = block_size_high[block_idx];
    DECLARE_ALIGNED(16, uint8_t, mask_ref[2 * MAX_SB_SQUARE]);
    DECLARE_ALIGNED(16, uint8_t, mask_test[2 * MAX_SB_SQUARE]);
    DECLARE_ALIGNED(32, uint16_t, src0[MAX_SB_SQUARE]);
    DECLARE_ALIGNED(32, uint16_t, src1[MAX_SB_SQUARE]);

    ConvolveParams conv_params =
        get_conv_params_no_round(0, 0, nullptr, 0, 1, bd);

    const int in_precision =
        bd + 2 * FILTER_BITS - conv_params.round_0 - conv_params.round_1 + 2;

    for (int i = 0; i < MAX_SB_SQUARE; i++) {
      src0[i] = rnd_.Rand16() & ((1 << in_precision) - 1);
      src1[i] = rnd_.Rand16() & ((1 << in_precision) - 1);
    }

    for (int mask_type = 0; mask_type < DIFFWTD_MASK_TYPES; mask_type++) {
      av1_build_compound_diffwtd_mask_d16_c(
          mask_ref, (DIFFWTD_MASK_TYPE)mask_type, src0, width, src1, width,
          height, width, &conv_params, bd);

      test_impl(mask_test, (DIFFWTD_MASK_TYPE)mask_type, src0, width, src1,
                width, height, width, &conv_params, bd);

      for (int r = 0; r < height; ++r) {
        for (int c = 0; c < width; ++c) {
          ASSERT_EQ(mask_ref[c + r * width], mask_test[c + r * width])
              << "Mismatch at unit tests for BuildCompDiffwtdMaskD16Test\n"
              << " Pixel mismatch at index "
              << "[" << r << "," << c << "] "
              << " @ " << width << "x" << height << " inv " << mask_type;
        }
      }
    }
  }

  void RunSpeedTest(BuildCompDiffWtdMaskD16Func test_impl,
                    DIFFWTD_MASK_TYPE mask_type) {
    const int block_idx = GET_PARAM(2);
    const int bd = GET_PARAM(0);
    const int width = block_size_wide[block_idx];
    const int height = block_size_high[block_idx];
    DECLARE_ALIGNED(16, uint8_t, mask[MAX_SB_SQUARE]);
    DECLARE_ALIGNED(32, uint16_t, src0[MAX_SB_SQUARE]);
    DECLARE_ALIGNED(32, uint16_t, src1[MAX_SB_SQUARE]);

    ConvolveParams conv_params =
        get_conv_params_no_round(0, 0, nullptr, 0, 1, bd);

    const int in_precision =
        bd + 2 * FILTER_BITS - conv_params.round_0 - conv_params.round_1 + 2;

    for (int i = 0; i < MAX_SB_SQUARE; i++) {
      src0[i] = rnd_.Rand16() & ((1 << in_precision) - 1);
      src1[i] = rnd_.Rand16() & ((1 << in_precision) - 1);
    }

    const int num_loops = 10000000 / (width + height);
    aom_usec_timer timer;
    aom_usec_timer_start(&timer);

    for (int i = 0; i < num_loops; ++i)
      av1_build_compound_diffwtd_mask_d16_c(mask, mask_type, src0, width, src1,
                                            width, height, width, &conv_params,
                                            bd);

    aom_usec_timer_mark(&timer);
    const int elapsed_time = static_cast<int>(aom_usec_timer_elapsed(&timer));

    aom_usec_timer timer1;
    aom_usec_timer_start(&timer1);

    for (int i = 0; i < num_loops; ++i)
      test_impl(mask, mask_type, src0, width, src1, width, height, width,
                &conv_params, bd);

    aom_usec_timer_mark(&timer1);
    const int elapsed_time1 = static_cast<int>(aom_usec_timer_elapsed(&timer1));
    printf("av1_build_compound_diffwtd_mask_d16  %3dx%-3d: %7.2f \n", width,
           height, elapsed_time / double(elapsed_time1));
  }

 private:
  ACMRandom rnd_;
};  // class BuildCompDiffwtdMaskD16Test
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(BuildCompDiffwtdMaskD16Test);

TEST_P(BuildCompDiffwtdMaskD16Test, CheckOutput) {
  RunCheckOutput(GET_PARAM(1));
}

TEST_P(BuildCompDiffwtdMaskD16Test, DISABLED_Speed) {
  RunSpeedTest(GET_PARAM(1), DIFFWTD_38);
  RunSpeedTest(GET_PARAM(1), DIFFWTD_38_INV);
}

#if HAVE_SSE4_1
INSTANTIATE_TEST_SUITE_P(
    SSE4_1, BuildCompDiffwtdMaskD16Test,
    BuildParams(av1_build_compound_diffwtd_mask_d16_sse4_1));
#endif

#if HAVE_AVX2
INSTANTIATE_TEST_SUITE_P(AVX2, BuildCompDiffwtdMaskD16Test,
                         BuildParams(av1_build_compound_diffwtd_mask_d16_avx2));
#endif

#if HAVE_NEON
INSTANTIATE_TEST_SUITE_P(NEON, BuildCompDiffwtdMaskD16Test,
                         BuildParams(av1_build_compound_diffwtd_mask_d16_neon));
#endif

}  // namespace
