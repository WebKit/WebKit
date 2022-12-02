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

#include <cstdint>
#include <tuple>

#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"

#include "test/acm_random.h"
#include "test/register_state_check.h"
#include "test/util.h"
#include "av1/common/blockd.h"
#include "aom_mem/aom_mem.h"
#include "aom_ports/mem.h"

typedef void (*SubtractFunc)(int rows, int cols, int16_t *diff_ptr,
                             ptrdiff_t diff_stride, const uint8_t *src_ptr,
                             ptrdiff_t src_stride, const uint8_t *pred_ptr,
                             ptrdiff_t pred_stride);

namespace {

class AV1SubtractBlockTest : public ::testing::TestWithParam<SubtractFunc> {
 public:
  virtual void TearDown() {}
};

using libaom_test::ACMRandom;

TEST_P(AV1SubtractBlockTest, SimpleSubtract) {
  ACMRandom rnd(ACMRandom::DeterministicSeed());

  // FIXME(rbultje) split in its own file
  for (BLOCK_SIZE bsize = BLOCK_4X4; bsize < BLOCK_SIZES;
       bsize = static_cast<BLOCK_SIZE>(static_cast<int>(bsize) + 1)) {
    const int block_width = block_size_wide[bsize];
    const int block_height = block_size_high[bsize];
    int16_t *diff = reinterpret_cast<int16_t *>(
        aom_memalign(16, sizeof(*diff) * block_width * block_height * 2));
    ASSERT_NE(diff, nullptr);
    uint8_t *pred = reinterpret_cast<uint8_t *>(
        aom_memalign(16, block_width * block_height * 2));
    ASSERT_NE(pred, nullptr);
    uint8_t *src = reinterpret_cast<uint8_t *>(
        aom_memalign(16, block_width * block_height * 2));
    ASSERT_NE(src, nullptr);

    for (int n = 0; n < 100; n++) {
      for (int r = 0; r < block_height; ++r) {
        for (int c = 0; c < block_width * 2; ++c) {
          src[r * block_width * 2 + c] = rnd.Rand8();
          pred[r * block_width * 2 + c] = rnd.Rand8();
        }
      }

      GetParam()(block_height, block_width, diff, block_width, src, block_width,
                 pred, block_width);

      for (int r = 0; r < block_height; ++r) {
        for (int c = 0; c < block_width; ++c) {
          EXPECT_EQ(diff[r * block_width + c],
                    (src[r * block_width + c] - pred[r * block_width + c]))
              << "r = " << r << ", c = " << c << ", bs = " << bsize;
        }
      }

      GetParam()(block_height, block_width, diff, block_width * 2, src,
                 block_width * 2, pred, block_width * 2);

      for (int r = 0; r < block_height; ++r) {
        for (int c = 0; c < block_width; ++c) {
          EXPECT_EQ(
              diff[r * block_width * 2 + c],
              (src[r * block_width * 2 + c] - pred[r * block_width * 2 + c]))
              << "r = " << r << ", c = " << c << ", bs = " << bsize;
        }
      }
    }
    aom_free(diff);
    aom_free(pred);
    aom_free(src);
  }
}

INSTANTIATE_TEST_SUITE_P(C, AV1SubtractBlockTest,
                         ::testing::Values(aom_subtract_block_c));

#if HAVE_SSE2
INSTANTIATE_TEST_SUITE_P(SSE2, AV1SubtractBlockTest,
                         ::testing::Values(aom_subtract_block_sse2));
#endif
#if HAVE_NEON
INSTANTIATE_TEST_SUITE_P(NEON, AV1SubtractBlockTest,
                         ::testing::Values(aom_subtract_block_neon));
#endif
#if HAVE_MSA
INSTANTIATE_TEST_SUITE_P(MSA, AV1SubtractBlockTest,
                         ::testing::Values(aom_subtract_block_msa));
#endif

#if CONFIG_AV1_HIGHBITDEPTH
typedef void (*HBDSubtractFunc)(int rows, int cols, int16_t *diff_ptr,
                                ptrdiff_t diff_stride, const uint8_t *src_ptr,
                                ptrdiff_t src_stride, const uint8_t *pred_ptr,
                                ptrdiff_t pred_stride);

using std::get;
using std::make_tuple;
using std::tuple;

// <BLOCK_SIZE, bit_depth, optimized subtract func, reference subtract func>
typedef tuple<BLOCK_SIZE, int, HBDSubtractFunc, HBDSubtractFunc> Params;

class AV1HBDSubtractBlockTest : public ::testing::TestWithParam<Params> {
 public:
  virtual void SetUp() {
    block_width_ = block_size_wide[GET_PARAM(0)];
    block_height_ = block_size_high[GET_PARAM(0)];
    bit_depth_ = static_cast<aom_bit_depth_t>(GET_PARAM(1));
    func_ = GET_PARAM(2);
    ref_func_ = GET_PARAM(3);

    rnd_.Reset(ACMRandom::DeterministicSeed());

    const size_t max_width = 128;
    const size_t max_block_size = max_width * max_width;
    src_ = CONVERT_TO_BYTEPTR(reinterpret_cast<uint16_t *>(
        aom_memalign(16, max_block_size * sizeof(uint16_t))));
    ASSERT_NE(src_, nullptr);
    pred_ = CONVERT_TO_BYTEPTR(reinterpret_cast<uint16_t *>(
        aom_memalign(16, max_block_size * sizeof(uint16_t))));
    ASSERT_NE(pred_, nullptr);
    diff_ = reinterpret_cast<int16_t *>(
        aom_memalign(16, max_block_size * sizeof(int16_t)));
    ASSERT_NE(diff_, nullptr);
  }

  virtual void TearDown() {
    aom_free(CONVERT_TO_SHORTPTR(src_));
    aom_free(CONVERT_TO_SHORTPTR(pred_));
    aom_free(diff_);
  }

 protected:
  void CheckResult();
  void RunForSpeed();

 private:
  ACMRandom rnd_;
  int block_height_;
  int block_width_;
  aom_bit_depth_t bit_depth_;
  HBDSubtractFunc func_;
  HBDSubtractFunc ref_func_;
  uint8_t *src_;
  uint8_t *pred_;
  int16_t *diff_;
};
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(AV1HBDSubtractBlockTest);

void AV1HBDSubtractBlockTest::CheckResult() {
  const int test_num = 100;
  const size_t max_width = 128;
  const int max_block_size = max_width * max_width;
  const int mask = (1 << bit_depth_) - 1;
  int i, j;

  for (i = 0; i < test_num; ++i) {
    for (j = 0; j < max_block_size; ++j) {
      CONVERT_TO_SHORTPTR(src_)[j] = rnd_.Rand16() & mask;
      CONVERT_TO_SHORTPTR(pred_)[j] = rnd_.Rand16() & mask;
    }

    func_(block_height_, block_width_, diff_, block_width_, src_, block_width_,
          pred_, block_width_);

    for (int r = 0; r < block_height_; ++r) {
      for (int c = 0; c < block_width_; ++c) {
        EXPECT_EQ(diff_[r * block_width_ + c],
                  (CONVERT_TO_SHORTPTR(src_)[r * block_width_ + c] -
                   CONVERT_TO_SHORTPTR(pred_)[r * block_width_ + c]))
            << "r = " << r << ", c = " << c << ", test: " << i;
      }
    }
  }
}

TEST_P(AV1HBDSubtractBlockTest, CheckResult) { CheckResult(); }

void AV1HBDSubtractBlockTest::RunForSpeed() {
  const int test_num = 200000;
  const size_t max_width = 128;
  const int max_block_size = max_width * max_width;
  const int mask = (1 << bit_depth_) - 1;
  int i, j;

  if (ref_func_ == func_) GTEST_SKIP();

  for (j = 0; j < max_block_size; ++j) {
    CONVERT_TO_SHORTPTR(src_)[j] = rnd_.Rand16() & mask;
    CONVERT_TO_SHORTPTR(pred_)[j] = rnd_.Rand16() & mask;
  }

  aom_usec_timer ref_timer;
  aom_usec_timer_start(&ref_timer);
  for (i = 0; i < test_num; ++i) {
    ref_func_(block_height_, block_width_, diff_, block_width_, src_,
              block_width_, pred_, block_width_);
  }
  aom_usec_timer_mark(&ref_timer);
  const int64_t ref_elapsed_time = aom_usec_timer_elapsed(&ref_timer);

  for (j = 0; j < max_block_size; ++j) {
    CONVERT_TO_SHORTPTR(src_)[j] = rnd_.Rand16() & mask;
    CONVERT_TO_SHORTPTR(pred_)[j] = rnd_.Rand16() & mask;
  }

  aom_usec_timer timer;
  aom_usec_timer_start(&timer);
  for (i = 0; i < test_num; ++i) {
    func_(block_height_, block_width_, diff_, block_width_, src_, block_width_,
          pred_, block_width_);
  }
  aom_usec_timer_mark(&timer);
  const int64_t elapsed_time = aom_usec_timer_elapsed(&timer);

  printf(
      "[%dx%d]: "
      "ref_time=%6" PRId64 " \t simd_time=%6" PRId64
      " \t "
      "gain=%f \n",
      block_width_, block_height_, ref_elapsed_time, elapsed_time,
      static_cast<double>(ref_elapsed_time) /
          static_cast<double>(elapsed_time));
}

TEST_P(AV1HBDSubtractBlockTest, DISABLED_Speed) { RunForSpeed(); }

const BLOCK_SIZE kValidBlockSize[] = { BLOCK_4X4,    BLOCK_4X8,    BLOCK_8X4,
                                       BLOCK_8X8,    BLOCK_8X16,   BLOCK_16X8,
                                       BLOCK_16X16,  BLOCK_16X32,  BLOCK_32X16,
                                       BLOCK_32X32,  BLOCK_32X64,  BLOCK_64X32,
                                       BLOCK_64X64,  BLOCK_64X128, BLOCK_128X64,
                                       BLOCK_128X128 };

INSTANTIATE_TEST_SUITE_P(
    C, AV1HBDSubtractBlockTest,
    ::testing::Combine(::testing::ValuesIn(kValidBlockSize),
                       ::testing::Values(12),
                       ::testing::Values(&aom_highbd_subtract_block_c),
                       ::testing::Values(&aom_highbd_subtract_block_c)));

#if HAVE_SSE2
INSTANTIATE_TEST_SUITE_P(
    SSE2, AV1HBDSubtractBlockTest,
    ::testing::Combine(::testing::ValuesIn(kValidBlockSize),
                       ::testing::Values(12),
                       ::testing::Values(&aom_highbd_subtract_block_sse2),
                       ::testing::Values(&aom_highbd_subtract_block_c)));
#endif  // HAVE_SSE2

#if HAVE_NEON
INSTANTIATE_TEST_SUITE_P(
    NEON, AV1HBDSubtractBlockTest,
    ::testing::Combine(::testing::ValuesIn(kValidBlockSize),
                       ::testing::Values(12),
                       ::testing::Values(&aom_highbd_subtract_block_neon),
                       ::testing::Values(&aom_highbd_subtract_block_c)));
#endif
#endif  // CONFIG_AV1_HIGHBITDEPTH
}  // namespace
