/*
 * Copyright (c) 2018, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <cstdlib>
#include <new>
#include <tuple>

#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"

#include "aom/aom_codec.h"
#include "aom/aom_integer.h"
#include "aom_dsp/variance.h"
#include "aom_mem/aom_mem.h"
#include "aom_ports/aom_timer.h"
#include "aom_ports/mem.h"
#include "av1/common/reconinter.h"
#include "av1/encoder/reconinter_enc.h"
#include "test/acm_random.h"
#include "test/register_state_check.h"
#include "test/util.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

namespace {
typedef void (*comp_mask_pred_func)(uint8_t *comp_pred, const uint8_t *pred,
                                    int width, int height, const uint8_t *ref,
                                    int ref_stride, const uint8_t *mask,
                                    int mask_stride, int invert_mask);

typedef void (*comp_avg_pred_func)(uint8_t *comp_pred, const uint8_t *pred,
                                   int width, int height, const uint8_t *ref,
                                   int ref_stride);

#if HAVE_SSSE3 || HAVE_SSE2 || HAVE_AVX2 || HAVE_NEON
const BLOCK_SIZE kCompMaskPredParams[] = {
  BLOCK_8X8,   BLOCK_8X16, BLOCK_8X32,  BLOCK_16X8, BLOCK_16X16,
  BLOCK_16X32, BLOCK_32X8, BLOCK_32X16, BLOCK_32X32
};
#endif

class AV1CompMaskPredBase : public ::testing::Test {
 public:
  ~AV1CompMaskPredBase() override;
  void SetUp() override;

  void TearDown() override;

 protected:
  bool CheckResult(int width, int height) {
    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x) {
        const int idx = y * width + x;
        if (comp_pred1_[idx] != comp_pred2_[idx]) {
          printf("%dx%d mismatch @%d(%d,%d) ", width, height, idx, y, x);
          printf("%d != %d ", comp_pred1_[idx], comp_pred2_[idx]);
          return false;
        }
      }
    }
    return true;
  }

  libaom_test::ACMRandom rnd_;
  uint8_t *comp_pred1_;
  uint8_t *comp_pred2_;
  uint8_t *pred_;
  uint8_t *ref_buffer_;
  uint8_t *ref_;
};

AV1CompMaskPredBase::~AV1CompMaskPredBase() = default;

void AV1CompMaskPredBase::SetUp() {
  rnd_.Reset(libaom_test::ACMRandom::DeterministicSeed());
  av1_init_wedge_masks();
  comp_pred1_ = (uint8_t *)aom_memalign(16, MAX_SB_SQUARE);
  ASSERT_NE(comp_pred1_, nullptr);
  comp_pred2_ = (uint8_t *)aom_memalign(16, MAX_SB_SQUARE);
  ASSERT_NE(comp_pred2_, nullptr);
  pred_ = (uint8_t *)aom_memalign(16, MAX_SB_SQUARE);
  ASSERT_NE(pred_, nullptr);
  // The biggest block size is MAX_SB_SQUARE(128*128), however for the
  // convolution we need to access 3 bytes before and 4 bytes after (for an
  // 8-tap filter), in both directions, so we need to allocate
  // (128 + 7) * (128 + 7) = MAX_SB_SQUARE + (14 * MAX_SB_SIZE) + 49
  ref_buffer_ =
      (uint8_t *)aom_memalign(16, MAX_SB_SQUARE + (14 * MAX_SB_SIZE) + 49);
  ASSERT_NE(ref_buffer_, nullptr);
  // Start of the actual block where the convolution will be computed
  ref_ = ref_buffer_ + (3 * MAX_SB_SIZE + 3);
  for (int i = 0; i < MAX_SB_SQUARE; ++i) {
    pred_[i] = rnd_.Rand8();
  }
  for (int i = 0; i < MAX_SB_SQUARE + (14 * MAX_SB_SIZE) + 49; ++i) {
    ref_buffer_[i] = rnd_.Rand8();
  }
}

void AV1CompMaskPredBase::TearDown() {
  aom_free(comp_pred1_);
  aom_free(comp_pred2_);
  aom_free(pred_);
  aom_free(ref_buffer_);
}

typedef std::tuple<comp_mask_pred_func, BLOCK_SIZE> CompMaskPredParam;

class AV1CompMaskPredTest
    : public AV1CompMaskPredBase,
      public ::testing::WithParamInterface<CompMaskPredParam> {
 protected:
  void RunCheckOutput(comp_mask_pred_func test_impl, BLOCK_SIZE bsize, int inv);
  void RunSpeedTest(comp_mask_pred_func test_impl, BLOCK_SIZE bsize);
};

void AV1CompMaskPredTest::RunCheckOutput(comp_mask_pred_func test_impl,
                                         BLOCK_SIZE bsize, int inv) {
  const int w = block_size_wide[bsize];
  const int h = block_size_high[bsize];
  const int wedge_types = get_wedge_types_lookup(bsize);
  for (int wedge_index = 0; wedge_index < wedge_types; ++wedge_index) {
    const uint8_t *mask = av1_get_contiguous_soft_mask(wedge_index, 1, bsize);

    aom_comp_mask_pred_c(comp_pred1_, pred_, w, h, ref_, MAX_SB_SIZE, mask, w,
                         inv);
    test_impl(comp_pred2_, pred_, w, h, ref_, MAX_SB_SIZE, mask, w, inv);

    ASSERT_EQ(CheckResult(w, h), true)
        << " wedge " << wedge_index << " inv " << inv;
  }
}

void AV1CompMaskPredTest::RunSpeedTest(comp_mask_pred_func test_impl,
                                       BLOCK_SIZE bsize) {
  const int w = block_size_wide[bsize];
  const int h = block_size_high[bsize];
  const int wedge_types = get_wedge_types_lookup(bsize);
  int wedge_index = wedge_types / 2;
  const uint8_t *mask = av1_get_contiguous_soft_mask(wedge_index, 1, bsize);
  const int num_loops = 1000000000 / (w + h);

  comp_mask_pred_func funcs[2] = { aom_comp_mask_pred_c, test_impl };
  double elapsed_time[2] = { 0 };
  for (int i = 0; i < 2; ++i) {
    aom_usec_timer timer;
    aom_usec_timer_start(&timer);
    comp_mask_pred_func func = funcs[i];
    for (int j = 0; j < num_loops; ++j) {
      func(comp_pred1_, pred_, w, h, ref_, MAX_SB_SIZE, mask, w, 0);
    }
    aom_usec_timer_mark(&timer);
    double time = static_cast<double>(aom_usec_timer_elapsed(&timer));
    elapsed_time[i] = 1000.0 * time / num_loops;
  }
  printf("compMask %3dx%-3d: %7.2f/%7.2fns", w, h, elapsed_time[0],
         elapsed_time[1]);
  printf("(%3.2f)\n", elapsed_time[0] / elapsed_time[1]);
}

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(AV1CompMaskPredTest);

TEST_P(AV1CompMaskPredTest, CheckOutput) {
  // inv = 0, 1
  RunCheckOutput(GET_PARAM(0), GET_PARAM(1), 0);
  RunCheckOutput(GET_PARAM(0), GET_PARAM(1), 1);
}

TEST_P(AV1CompMaskPredTest, DISABLED_Speed) {
  RunSpeedTest(GET_PARAM(0), GET_PARAM(1));
}

#if HAVE_SSSE3
INSTANTIATE_TEST_SUITE_P(
    SSSE3, AV1CompMaskPredTest,
    ::testing::Combine(::testing::Values(&aom_comp_mask_pred_ssse3),
                       ::testing::ValuesIn(kCompMaskPredParams)));
#endif

#if HAVE_AVX2
INSTANTIATE_TEST_SUITE_P(
    AVX2, AV1CompMaskPredTest,
    ::testing::Combine(::testing::Values(&aom_comp_mask_pred_avx2),
                       ::testing::ValuesIn(kCompMaskPredParams)));
#endif

#if HAVE_NEON
INSTANTIATE_TEST_SUITE_P(
    NEON, AV1CompMaskPredTest,
    ::testing::Combine(::testing::Values(&aom_comp_mask_pred_neon),
                       ::testing::ValuesIn(kCompMaskPredParams)));
#endif

#if HAVE_SSSE3 || HAVE_SSE2 || HAVE_AVX2 || HAVE_NEON
const BLOCK_SIZE kValidBlockSize[] = {
  BLOCK_4X4,     BLOCK_8X8,   BLOCK_8X16,  BLOCK_8X32,   BLOCK_16X8,
  BLOCK_16X16,   BLOCK_16X32, BLOCK_32X8,  BLOCK_32X16,  BLOCK_32X32,
  BLOCK_32X64,   BLOCK_64X32, BLOCK_64X64, BLOCK_64X128, BLOCK_128X64,
  BLOCK_128X128, BLOCK_16X64, BLOCK_64X16
};
#endif

typedef void (*upsampled_pred_func)(MACROBLOCKD *xd, const AV1_COMMON *const cm,
                                    int mi_row, int mi_col, const MV *const mv,
                                    uint8_t *comp_pred, int width, int height,
                                    int subpel_x_q3, int subpel_y_q3,
                                    const uint8_t *ref, int ref_stride,
                                    int subpel_search);

typedef std::tuple<upsampled_pred_func, BLOCK_SIZE> UpsampledPredParam;

class AV1UpsampledPredTest
    : public AV1CompMaskPredBase,
      public ::testing::WithParamInterface<UpsampledPredParam> {
 protected:
  void RunCheckOutput(upsampled_pred_func test_impl, BLOCK_SIZE bsize);
  void RunSpeedTest(upsampled_pred_func test_impl, BLOCK_SIZE bsize,
                    int havSub);
};

void AV1UpsampledPredTest::RunCheckOutput(upsampled_pred_func test_impl,
                                          BLOCK_SIZE bsize) {
  const int w = block_size_wide[bsize];
  const int h = block_size_high[bsize];
  for (int subpel_search = USE_4_TAPS; subpel_search <= USE_8_TAPS;
       ++subpel_search) {
    // loop through subx and suby
    for (int sub = 0; sub < 8 * 8; ++sub) {
      int subx = sub & 0x7;
      int suby = (sub >> 3);

      aom_upsampled_pred_c(nullptr, nullptr, 0, 0, nullptr, comp_pred1_, w, h,
                           subx, suby, ref_, MAX_SB_SIZE, subpel_search);

      test_impl(nullptr, nullptr, 0, 0, nullptr, comp_pred2_, w, h, subx, suby,
                ref_, MAX_SB_SIZE, subpel_search);
      ASSERT_EQ(CheckResult(w, h), true)
          << "sub (" << subx << "," << suby << ")";
    }
  }
}

void AV1UpsampledPredTest::RunSpeedTest(upsampled_pred_func test_impl,
                                        BLOCK_SIZE bsize, int havSub) {
  const int w = block_size_wide[bsize];
  const int h = block_size_high[bsize];
  const int subx = havSub ? 3 : 0;
  const int suby = havSub ? 4 : 0;

  const int num_loops = 1000000000 / (w + h);
  upsampled_pred_func funcs[2] = { aom_upsampled_pred_c, test_impl };
  double elapsed_time[2] = { 0 };
  int subpel_search = USE_8_TAPS;  // set to USE_4_TAPS to test 4-tap filter.
  for (int i = 0; i < 2; ++i) {
    aom_usec_timer timer;
    aom_usec_timer_start(&timer);
    upsampled_pred_func func = funcs[i];
    for (int j = 0; j < num_loops; ++j) {
      func(nullptr, nullptr, 0, 0, nullptr, comp_pred1_, w, h, subx, suby, ref_,
           MAX_SB_SIZE, subpel_search);
    }
    aom_usec_timer_mark(&timer);
    double time = static_cast<double>(aom_usec_timer_elapsed(&timer));
    elapsed_time[i] = 1000.0 * time / num_loops;
  }
  printf("UpsampledPred[%d] %3dx%-3d:%7.2f/%7.2fns", havSub, w, h,
         elapsed_time[0], elapsed_time[1]);
  printf("(%3.2f)\n", elapsed_time[0] / elapsed_time[1]);
}

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(AV1UpsampledPredTest);

TEST_P(AV1UpsampledPredTest, CheckOutput) {
  RunCheckOutput(GET_PARAM(0), GET_PARAM(1));
}

TEST_P(AV1UpsampledPredTest, DISABLED_Speed) {
  RunSpeedTest(GET_PARAM(0), GET_PARAM(1), 1);
}

#if HAVE_SSE2
INSTANTIATE_TEST_SUITE_P(
    SSE2, AV1UpsampledPredTest,
    ::testing::Combine(::testing::Values(&aom_upsampled_pred_sse2),
                       ::testing::ValuesIn(kValidBlockSize)));
#endif

#if HAVE_NEON
INSTANTIATE_TEST_SUITE_P(
    NEON, AV1UpsampledPredTest,
    ::testing::Combine(::testing::Values(&aom_upsampled_pred_neon),
                       ::testing::ValuesIn(kValidBlockSize)));
#endif

typedef std::tuple<comp_avg_pred_func, BLOCK_SIZE> CompAvgPredParam;

class AV1CompAvgPredTest : public ::testing::TestWithParam<CompAvgPredParam> {
 public:
  ~AV1CompAvgPredTest() override;
  void SetUp() override;

  void TearDown() override;

 protected:
  void RunCheckOutput(comp_avg_pred_func test_impl, BLOCK_SIZE bsize);
  void RunSpeedTest(comp_avg_pred_func test_impl, BLOCK_SIZE bsize);
  bool CheckResult(int width, int height) {
    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x) {
        const int idx = y * width + x;
        if (comp_pred1_[idx] != comp_pred2_[idx]) {
          printf("%dx%d mismatch @%d(%d,%d) ", width, height, idx, x, y);
          printf("%d != %d ", comp_pred1_[idx], comp_pred2_[idx]);
          return false;
        }
      }
    }
    return true;
  }

  libaom_test::ACMRandom rnd_;
  uint8_t *comp_pred1_;
  uint8_t *comp_pred2_;
  uint8_t *pred_;
  uint8_t *ref_;
};
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(AV1CompAvgPredTest);

AV1CompAvgPredTest::~AV1CompAvgPredTest() = default;

void AV1CompAvgPredTest::SetUp() {
  rnd_.Reset(libaom_test::ACMRandom::DeterministicSeed());

  comp_pred1_ = (uint8_t *)aom_memalign(16, MAX_SB_SQUARE);
  ASSERT_NE(comp_pred1_, nullptr);
  comp_pred2_ = (uint8_t *)aom_memalign(16, MAX_SB_SQUARE);
  ASSERT_NE(comp_pred2_, nullptr);
  pred_ = (uint8_t *)aom_memalign(16, MAX_SB_SQUARE);
  ASSERT_NE(pred_, nullptr);
  ref_ = (uint8_t *)aom_memalign(16, MAX_SB_SQUARE);
  ASSERT_NE(ref_, nullptr);
  for (int i = 0; i < MAX_SB_SQUARE; ++i) {
    pred_[i] = rnd_.Rand8();
  }
  for (int i = 0; i < MAX_SB_SQUARE; ++i) {
    ref_[i] = rnd_.Rand8();
  }
}

void AV1CompAvgPredTest::TearDown() {
  aom_free(comp_pred1_);
  aom_free(comp_pred2_);
  aom_free(pred_);
  aom_free(ref_);
}

void AV1CompAvgPredTest::RunCheckOutput(comp_avg_pred_func test_impl,
                                        BLOCK_SIZE bsize) {
  const int w = block_size_wide[bsize];
  const int h = block_size_high[bsize];
  aom_comp_avg_pred_c(comp_pred1_, pred_, w, h, ref_, MAX_SB_SIZE);
  test_impl(comp_pred2_, pred_, w, h, ref_, MAX_SB_SIZE);

  ASSERT_EQ(CheckResult(w, h), true);
}

void AV1CompAvgPredTest::RunSpeedTest(comp_avg_pred_func test_impl,
                                      BLOCK_SIZE bsize) {
  const int w = block_size_wide[bsize];
  const int h = block_size_high[bsize];
  const int num_loops = 1000000000 / (w + h);

  comp_avg_pred_func functions[2] = { aom_comp_avg_pred_c, test_impl };
  double elapsed_time[2] = { 0.0 };
  for (int i = 0; i < 2; ++i) {
    aom_usec_timer timer;
    aom_usec_timer_start(&timer);
    comp_avg_pred_func func = functions[i];
    for (int j = 0; j < num_loops; ++j) {
      func(comp_pred1_, pred_, w, h, ref_, MAX_SB_SIZE);
    }
    aom_usec_timer_mark(&timer);
    const double time = static_cast<double>(aom_usec_timer_elapsed(&timer));
    elapsed_time[i] = 1000.0 * time;
  }
  printf("CompAvgPred %3dx%-3d: %7.2f/%7.2fns", w, h, elapsed_time[0],
         elapsed_time[1]);
  printf("(%3.2f)\n", elapsed_time[0] / elapsed_time[1]);
}

TEST_P(AV1CompAvgPredTest, CheckOutput) {
  RunCheckOutput(GET_PARAM(0), GET_PARAM(1));
}

TEST_P(AV1CompAvgPredTest, DISABLED_Speed) {
  RunSpeedTest(GET_PARAM(0), GET_PARAM(1));
}

#if HAVE_AVX2
INSTANTIATE_TEST_SUITE_P(
    AVX2, AV1CompAvgPredTest,
    ::testing::Combine(::testing::Values(&aom_comp_avg_pred_avx2),
                       ::testing::ValuesIn(kValidBlockSize)));
#endif

#if HAVE_NEON
INSTANTIATE_TEST_SUITE_P(
    NEON, AV1CompAvgPredTest,
    ::testing::Combine(::testing::Values(&aom_comp_avg_pred_neon),
                       ::testing::ValuesIn(kValidBlockSize)));
#endif

#if CONFIG_AV1_HIGHBITDEPTH
class AV1HighbdCompMaskPredTestBase : public ::testing::Test {
 public:
  ~AV1HighbdCompMaskPredTestBase() override;
  void SetUp() override;

  void TearDown() override;

 protected:
  bool CheckResult(int width, int height) {
    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x) {
        const int idx = y * width + x;
        if (comp_pred1_[idx] != comp_pred2_[idx]) {
          printf("%dx%d mismatch @%d(%d,%d) ", width, height, idx, y, x);
          printf("%d != %d ", comp_pred1_[idx], comp_pred2_[idx]);
          return false;
        }
      }
    }
    return true;
  }

  libaom_test::ACMRandom rnd_;
  uint16_t *comp_pred1_;
  uint16_t *comp_pred2_;
  uint16_t *pred_;
  uint16_t *ref_buffer_;
  uint16_t *ref_;
};

AV1HighbdCompMaskPredTestBase::~AV1HighbdCompMaskPredTestBase() = default;

void AV1HighbdCompMaskPredTestBase::SetUp() {
  rnd_.Reset(libaom_test::ACMRandom::DeterministicSeed());
  av1_init_wedge_masks();

  comp_pred1_ =
      (uint16_t *)aom_memalign(16, MAX_SB_SQUARE * sizeof(*comp_pred1_));
  ASSERT_NE(comp_pred1_, nullptr);
  comp_pred2_ =
      (uint16_t *)aom_memalign(16, MAX_SB_SQUARE * sizeof(*comp_pred2_));
  ASSERT_NE(comp_pred2_, nullptr);
  pred_ = (uint16_t *)aom_memalign(16, MAX_SB_SQUARE * sizeof(*pred_));
  ASSERT_NE(pred_, nullptr);
  // The biggest block size is MAX_SB_SQUARE(128*128), however for the
  // convolution we need to access 3 elements before and 4 elements after (for
  // an 8-tap filter), in both directions, so we need to allocate (128 + 7) *
  // (128 + 7) = (MAX_SB_SQUARE + (14 * MAX_SB_SIZE) + 49) *
  // sizeof(*ref_buffer_)
  ref_buffer_ = (uint16_t *)aom_memalign(
      16, (MAX_SB_SQUARE + (14 * MAX_SB_SIZE) + 49) * sizeof(*ref_buffer_));
  ASSERT_NE(ref_buffer_, nullptr);
  // Start of the actual block where the convolution will be computed
  ref_ = ref_buffer_ + (3 * MAX_SB_SIZE + 3);
}

void AV1HighbdCompMaskPredTestBase::TearDown() {
  aom_free(comp_pred1_);
  aom_free(comp_pred2_);
  aom_free(pred_);
  aom_free(ref_buffer_);
}

typedef void (*highbd_comp_mask_pred_func)(uint8_t *comp_pred8,
                                           const uint8_t *pred8, int width,
                                           int height, const uint8_t *ref8,
                                           int ref_stride, const uint8_t *mask,
                                           int mask_stride, int invert_mask);

typedef std::tuple<highbd_comp_mask_pred_func, BLOCK_SIZE, int>
    HighbdCompMaskPredParam;

class AV1HighbdCompMaskPredTest
    : public AV1HighbdCompMaskPredTestBase,
      public ::testing::WithParamInterface<HighbdCompMaskPredParam> {
 public:
  ~AV1HighbdCompMaskPredTest() override;

 protected:
  void RunCheckOutput(comp_mask_pred_func test_impl, BLOCK_SIZE bsize, int inv);
  void RunSpeedTest(comp_mask_pred_func test_impl, BLOCK_SIZE bsize);
};

AV1HighbdCompMaskPredTest::~AV1HighbdCompMaskPredTest() = default;

void AV1HighbdCompMaskPredTest::RunCheckOutput(
    highbd_comp_mask_pred_func test_impl, BLOCK_SIZE bsize, int inv) {
  int bd_ = GET_PARAM(2);
  const int w = block_size_wide[bsize];
  const int h = block_size_high[bsize];
  const int wedge_types = get_wedge_types_lookup(bsize);

  for (int i = 0; i < MAX_SB_SQUARE; ++i) {
    pred_[i] = rnd_.Rand16() & ((1 << bd_) - 1);
  }
  for (int i = 0; i < MAX_SB_SQUARE + (8 * MAX_SB_SIZE); ++i) {
    ref_buffer_[i] = rnd_.Rand16() & ((1 << bd_) - 1);
  }

  for (int wedge_index = 0; wedge_index < wedge_types; ++wedge_index) {
    const uint8_t *mask = av1_get_contiguous_soft_mask(wedge_index, 1, bsize);

    aom_highbd_comp_mask_pred_c(
        CONVERT_TO_BYTEPTR(comp_pred1_), CONVERT_TO_BYTEPTR(pred_), w, h,
        CONVERT_TO_BYTEPTR(ref_), MAX_SB_SIZE, mask, w, inv);

    test_impl(CONVERT_TO_BYTEPTR(comp_pred2_), CONVERT_TO_BYTEPTR(pred_), w, h,
              CONVERT_TO_BYTEPTR(ref_), MAX_SB_SIZE, mask, w, inv);

    ASSERT_EQ(CheckResult(w, h), true)
        << " wedge " << wedge_index << " inv " << inv;
  }
}

void AV1HighbdCompMaskPredTest::RunSpeedTest(
    highbd_comp_mask_pred_func test_impl, BLOCK_SIZE bsize) {
  int bd_ = GET_PARAM(2);

  const int w = block_size_wide[bsize];
  const int h = block_size_high[bsize];
  const int wedge_types = get_wedge_types_lookup(bsize);
  int wedge_index = wedge_types / 2;

  for (int i = 0; i < MAX_SB_SQUARE; ++i) {
    pred_[i] = rnd_.Rand16() & ((1 << bd_) - 1);
  }
  for (int i = 0; i < MAX_SB_SQUARE + (8 * MAX_SB_SIZE); ++i) {
    ref_buffer_[i] = rnd_.Rand16() & ((1 << bd_) - 1);
  }

  const uint8_t *mask = av1_get_contiguous_soft_mask(wedge_index, 1, bsize);
  const int num_loops = 1000000000 / (w + h);

  highbd_comp_mask_pred_func funcs[2] = { aom_highbd_comp_mask_pred_c,
                                          test_impl };
  double elapsed_time[2] = { 0 };
  for (int i = 0; i < 2; ++i) {
    aom_usec_timer timer;
    aom_usec_timer_start(&timer);
    highbd_comp_mask_pred_func func = funcs[i];
    for (int j = 0; j < num_loops; ++j) {
      func(CONVERT_TO_BYTEPTR(comp_pred1_), CONVERT_TO_BYTEPTR(pred_), w, h,
           CONVERT_TO_BYTEPTR(ref_), MAX_SB_SIZE, mask, w, 0);
    }
    aom_usec_timer_mark(&timer);
    double time = static_cast<double>(aom_usec_timer_elapsed(&timer));
    elapsed_time[i] = 1000.0 * time / num_loops;
  }
  printf("compMask %3dx%-3d: %7.2f/%7.2fns", w, h, elapsed_time[0],
         elapsed_time[1]);
  printf("(%3.2f)\n", elapsed_time[0] / elapsed_time[1]);
}

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(AV1HighbdCompMaskPredTest);

TEST_P(AV1HighbdCompMaskPredTest, CheckOutput) {
  // inv = 0, 1
  RunCheckOutput(GET_PARAM(0), GET_PARAM(1), 0);
  RunCheckOutput(GET_PARAM(0), GET_PARAM(1), 1);
}

TEST_P(AV1HighbdCompMaskPredTest, DISABLED_Speed) {
  RunSpeedTest(GET_PARAM(0), GET_PARAM(1));
}

#if HAVE_NEON
INSTANTIATE_TEST_SUITE_P(
    NEON, AV1HighbdCompMaskPredTest,
    ::testing::Combine(::testing::Values(&aom_highbd_comp_mask_pred_neon),
                       ::testing::ValuesIn(kCompMaskPredParams),
                       ::testing::Range(8, 13, 2)));
#endif

#if HAVE_AVX2
INSTANTIATE_TEST_SUITE_P(
    AVX2, AV1HighbdCompMaskPredTest,
    ::testing::Combine(::testing::Values(&aom_highbd_comp_mask_pred_avx2),
                       ::testing::ValuesIn(kCompMaskPredParams),
                       ::testing::Range(8, 13, 2)));
#endif

#if HAVE_SSE2
INSTANTIATE_TEST_SUITE_P(
    SSE2, AV1HighbdCompMaskPredTest,
    ::testing::Combine(::testing::Values(&aom_highbd_comp_mask_pred_sse2),
                       ::testing::ValuesIn(kCompMaskPredParams),
                       ::testing::Range(8, 13, 2)));
#endif

typedef void (*highbd_upsampled_pred_func)(
    MACROBLOCKD *xd, const struct AV1Common *const cm, int mi_row, int mi_col,
    const MV *const mv, uint8_t *comp_pred8, int width, int height,
    int subpel_x_q3, int subpel_y_q3, const uint8_t *ref8, int ref_stride,
    int bd, int subpel_search);

typedef std::tuple<highbd_upsampled_pred_func, BLOCK_SIZE, int>
    HighbdUpsampledPredParam;

class AV1HighbdUpsampledPredTest
    : public AV1HighbdCompMaskPredTestBase,
      public ::testing::WithParamInterface<HighbdUpsampledPredParam> {
 public:
  ~AV1HighbdUpsampledPredTest() override;

 protected:
  void RunCheckOutput(highbd_upsampled_pred_func test_impl, BLOCK_SIZE bsize);
  void RunSpeedTest(highbd_upsampled_pred_func test_impl, BLOCK_SIZE bsize,
                    int havSub);
};

AV1HighbdUpsampledPredTest::~AV1HighbdUpsampledPredTest() = default;

void AV1HighbdUpsampledPredTest::RunCheckOutput(
    highbd_upsampled_pred_func test_impl, BLOCK_SIZE bsize) {
  int bd_ = GET_PARAM(2);
  const int w = block_size_wide[bsize];
  const int h = block_size_high[bsize];

  for (int i = 0; i < MAX_SB_SQUARE; ++i) {
    pred_[i] = rnd_.Rand16() & ((1 << bd_) - 1);
  }
  for (int i = 0; i < MAX_SB_SQUARE + (8 * MAX_SB_SIZE); ++i) {
    ref_buffer_[i] = rnd_.Rand16() & ((1 << bd_) - 1);
  }

  for (int subpel_search = 1; subpel_search <= 2; ++subpel_search) {
    // loop through subx and suby
    for (int sub = 0; sub < 8 * 8; ++sub) {
      int subx = sub & 0x7;
      int suby = (sub >> 3);

      aom_highbd_upsampled_pred_c(nullptr, nullptr, 0, 0, nullptr,
                                  CONVERT_TO_BYTEPTR(comp_pred1_), w, h, subx,
                                  suby, CONVERT_TO_BYTEPTR(ref_), MAX_SB_SIZE,
                                  bd_, subpel_search);

      test_impl(nullptr, nullptr, 0, 0, nullptr,
                CONVERT_TO_BYTEPTR(comp_pred2_), w, h, subx, suby,
                CONVERT_TO_BYTEPTR(ref_), MAX_SB_SIZE, bd_, subpel_search);

      ASSERT_EQ(CheckResult(w, h), true)
          << "sub (" << subx << "," << suby << ")";
    }
  }
}

void AV1HighbdUpsampledPredTest::RunSpeedTest(
    highbd_upsampled_pred_func test_impl, BLOCK_SIZE bsize, int havSub) {
  int bd_ = GET_PARAM(2);
  const int w = block_size_wide[bsize];
  const int h = block_size_high[bsize];
  const int subx = havSub ? 3 : 0;
  const int suby = havSub ? 4 : 0;

  for (int i = 0; i < MAX_SB_SQUARE; ++i) {
    pred_[i] = rnd_.Rand16() & ((1 << bd_) - 1);
  }
  for (int i = 0; i < MAX_SB_SQUARE + (8 * MAX_SB_SIZE); ++i) {
    ref_buffer_[i] = rnd_.Rand16() & ((1 << bd_) - 1);
  }

  const int num_loops = 1000000000 / (w + h);
  highbd_upsampled_pred_func funcs[2] = { &aom_highbd_upsampled_pred_c,
                                          test_impl };
  double elapsed_time[2] = { 0 };
  for (int i = 0; i < 2; ++i) {
    aom_usec_timer timer;
    aom_usec_timer_start(&timer);
    highbd_upsampled_pred_func func = funcs[i];
    int subpel_search = 2;  // set to 1 to test 4-tap filter.
    for (int j = 0; j < num_loops; ++j) {
      func(nullptr, nullptr, 0, 0, nullptr, CONVERT_TO_BYTEPTR(comp_pred1_), w,
           h, subx, suby, CONVERT_TO_BYTEPTR(ref_), MAX_SB_SIZE, bd_,
           subpel_search);
    }
    aom_usec_timer_mark(&timer);
    double time = static_cast<double>(aom_usec_timer_elapsed(&timer));
    elapsed_time[i] = 1000.0 * time / num_loops;
  }
  printf("CompMaskUp[%d] %3dx%-3d:%7.2f/%7.2fns", havSub, w, h, elapsed_time[0],
         elapsed_time[1]);
  printf("(%3.2f)\n", elapsed_time[0] / elapsed_time[1]);
}

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(AV1HighbdUpsampledPredTest);

TEST_P(AV1HighbdUpsampledPredTest, CheckOutput) {
  RunCheckOutput(GET_PARAM(0), GET_PARAM(1));
}

TEST_P(AV1HighbdUpsampledPredTest, DISABLED_Speed) {
  RunSpeedTest(GET_PARAM(0), GET_PARAM(1), 1);
}

#if HAVE_SSE2
INSTANTIATE_TEST_SUITE_P(
    SSE2, AV1HighbdUpsampledPredTest,
    ::testing::Combine(::testing::Values(&aom_highbd_upsampled_pred_sse2),
                       ::testing::ValuesIn(kValidBlockSize),
                       ::testing::Range(8, 13, 2)));
#endif

#if HAVE_NEON
INSTANTIATE_TEST_SUITE_P(
    NEON, AV1HighbdUpsampledPredTest,
    ::testing::Combine(::testing::Values(&aom_highbd_upsampled_pred_neon),
                       ::testing::ValuesIn(kValidBlockSize),
                       ::testing::Range(8, 13, 2)));
#endif

typedef void (*highbd_comp_avg_pred_func)(uint8_t *comp_pred,
                                          const uint8_t *pred, int width,
                                          int height, const uint8_t *ref,
                                          int ref_stride);

typedef std::tuple<highbd_comp_avg_pred_func, BLOCK_SIZE, int>
    HighbdCompAvgPredParam;

class AV1HighbdCompAvgPredTest
    : public ::testing::TestWithParam<HighbdCompAvgPredParam> {
 public:
  ~AV1HighbdCompAvgPredTest() override;
  void SetUp() override;

 protected:
  void RunCheckOutput(highbd_comp_avg_pred_func test_impl, BLOCK_SIZE bsize);
  void RunSpeedTest(highbd_comp_avg_pred_func test_impl, BLOCK_SIZE bsize);
  bool CheckResult(int width, int height) const {
    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x) {
        const int idx = y * width + x;
        if (comp_pred1_[idx] != comp_pred2_[idx]) {
          printf("%dx%d mismatch @%d(%d,%d) ", width, height, idx, x, y);
          printf("%d != %d ", comp_pred1_[idx], comp_pred2_[idx]);
          return false;
        }
      }
    }
    return true;
  }

  libaom_test::ACMRandom rnd_;
  uint16_t *comp_pred1_;
  uint16_t *comp_pred2_;
  uint16_t *pred_;
  uint16_t *ref_;
};

AV1HighbdCompAvgPredTest::~AV1HighbdCompAvgPredTest() {
  aom_free(comp_pred1_);
  aom_free(comp_pred2_);
  aom_free(pred_);
  aom_free(ref_);
}

void AV1HighbdCompAvgPredTest::SetUp() {
  int bd_ = GET_PARAM(2);
  rnd_.Reset(libaom_test::ACMRandom::DeterministicSeed());

  comp_pred1_ =
      (uint16_t *)aom_memalign(16, MAX_SB_SQUARE * sizeof(*comp_pred1_));
  ASSERT_NE(comp_pred1_, nullptr);
  comp_pred2_ =
      (uint16_t *)aom_memalign(16, MAX_SB_SQUARE * sizeof(*comp_pred2_));
  ASSERT_NE(comp_pred2_, nullptr);
  pred_ = (uint16_t *)aom_memalign(16, MAX_SB_SQUARE * sizeof(*pred_));
  ASSERT_NE(pred_, nullptr);
  ref_ = (uint16_t *)aom_memalign(16, MAX_SB_SQUARE * sizeof(*ref_));
  ASSERT_NE(ref_, nullptr);
  for (int i = 0; i < MAX_SB_SQUARE; ++i) {
    pred_[i] = rnd_.Rand16() & ((1 << bd_) - 1);
  }
  for (int i = 0; i < MAX_SB_SQUARE; ++i) {
    ref_[i] = rnd_.Rand16() & ((1 << bd_) - 1);
  }
}

void AV1HighbdCompAvgPredTest::RunCheckOutput(
    highbd_comp_avg_pred_func test_impl, BLOCK_SIZE bsize) {
  const int w = block_size_wide[bsize];
  const int h = block_size_high[bsize];
  aom_highbd_comp_avg_pred_c(CONVERT_TO_BYTEPTR(comp_pred1_),
                             CONVERT_TO_BYTEPTR(pred_), w, h,
                             CONVERT_TO_BYTEPTR(ref_), MAX_SB_SIZE);
  test_impl(CONVERT_TO_BYTEPTR(comp_pred2_), CONVERT_TO_BYTEPTR(pred_), w, h,
            CONVERT_TO_BYTEPTR(ref_), MAX_SB_SIZE);

  ASSERT_EQ(CheckResult(w, h), true);
}

void AV1HighbdCompAvgPredTest::RunSpeedTest(highbd_comp_avg_pred_func test_impl,
                                            BLOCK_SIZE bsize) {
  const int w = block_size_wide[bsize];
  const int h = block_size_high[bsize];
  const int num_loops = 1000000000 / (w + h);

  highbd_comp_avg_pred_func functions[2] = { aom_highbd_comp_avg_pred_c,
                                             test_impl };
  double elapsed_time[2] = { 0.0 };
  for (int i = 0; i < 2; ++i) {
    aom_usec_timer timer;
    aom_usec_timer_start(&timer);
    highbd_comp_avg_pred_func func = functions[i];
    for (int j = 0; j < num_loops; ++j) {
      func(CONVERT_TO_BYTEPTR(comp_pred1_), CONVERT_TO_BYTEPTR(pred_), w, h,
           CONVERT_TO_BYTEPTR(ref_), MAX_SB_SIZE);
    }
    aom_usec_timer_mark(&timer);
    const double time = static_cast<double>(aom_usec_timer_elapsed(&timer));
    elapsed_time[i] = 1000.0 * time;
  }
  printf("HighbdCompAvg %3dx%-3d: %7.2f/%7.2fns", w, h, elapsed_time[0],
         elapsed_time[1]);
  printf("(%3.2f)\n", elapsed_time[0] / elapsed_time[1]);
}

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(AV1HighbdCompAvgPredTest);

TEST_P(AV1HighbdCompAvgPredTest, CheckOutput) {
  RunCheckOutput(GET_PARAM(0), GET_PARAM(1));
}

TEST_P(AV1HighbdCompAvgPredTest, DISABLED_Speed) {
  RunSpeedTest(GET_PARAM(0), GET_PARAM(1));
}

#if HAVE_NEON
INSTANTIATE_TEST_SUITE_P(
    NEON, AV1HighbdCompAvgPredTest,
    ::testing::Combine(::testing::Values(&aom_highbd_comp_avg_pred_neon),
                       ::testing::ValuesIn(kValidBlockSize),
                       ::testing::Range(8, 13, 2)));
#endif

#endif  // CONFIG_AV1_HIGHBITDEPTH
}  // namespace
