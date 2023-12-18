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

using std::get;
using std::make_tuple;
using std::tuple;

using libaom_test::ACMRandom;

// <BLOCK_SIZE, optimized subtract func, reference subtract func>
using Params = tuple<BLOCK_SIZE, SubtractFunc, SubtractFunc>;

class AV1SubtractBlockTestBase : public ::testing::Test {
 public:
  AV1SubtractBlockTestBase(BLOCK_SIZE bs, int bit_depth, SubtractFunc func,
                           SubtractFunc ref_func) {
    block_width_ = block_size_wide[bs];
    block_height_ = block_size_high[bs];
    func_ = func;
    ref_func_ = ref_func;
    if (bit_depth == -1) {
      hbd_ = false;
      bit_depth_ = AOM_BITS_8;
    } else {
      hbd_ = true;
      bit_depth_ = static_cast<aom_bit_depth_t>(bit_depth);
    }
  }

  void SetUp() override {
    rnd_.Reset(ACMRandom::DeterministicSeed());

    const size_t max_width = 128;
    const size_t max_block_size = max_width * max_width;
    if (hbd_) {
      src_ = CONVERT_TO_BYTEPTR(reinterpret_cast<uint16_t *>(
          aom_memalign(16, max_block_size * sizeof(uint16_t))));
      ASSERT_NE(src_, nullptr);
      pred_ = CONVERT_TO_BYTEPTR(reinterpret_cast<uint16_t *>(
          aom_memalign(16, max_block_size * sizeof(uint16_t))));
      ASSERT_NE(pred_, nullptr);
    } else {
      src_ = reinterpret_cast<uint8_t *>(
          aom_memalign(16, max_block_size * sizeof(uint8_t)));
      ASSERT_NE(src_, nullptr);
      pred_ = reinterpret_cast<uint8_t *>(
          aom_memalign(16, max_block_size * sizeof(uint8_t)));
      ASSERT_NE(pred_, nullptr);
    }
    diff_ = reinterpret_cast<int16_t *>(
        aom_memalign(32, max_block_size * sizeof(int16_t)));
    ASSERT_NE(diff_, nullptr);
  }

  void TearDown() override {
    if (hbd_) {
      aom_free(CONVERT_TO_SHORTPTR(src_));
      aom_free(CONVERT_TO_SHORTPTR(pred_));
    } else {
      aom_free(src_);
      aom_free(pred_);
    }
    aom_free(diff_);
  }

 protected:
  void CheckResult();
  void RunForSpeed();

 private:
  void FillInputs();

  ACMRandom rnd_;
  int block_height_;
  int block_width_;
  bool hbd_;
  aom_bit_depth_t bit_depth_;
  SubtractFunc func_;
  SubtractFunc ref_func_;
  uint8_t *src_;
  uint8_t *pred_;
  int16_t *diff_;
};

void AV1SubtractBlockTestBase::FillInputs() {
  const size_t max_width = 128;
  const int max_block_size = max_width * max_width;
  if (hbd_) {
    const int mask = (1 << bit_depth_) - 1;
    for (int i = 0; i < max_block_size; ++i) {
      CONVERT_TO_SHORTPTR(src_)[i] = rnd_.Rand16() & mask;
      CONVERT_TO_SHORTPTR(pred_)[i] = rnd_.Rand16() & mask;
    }
  } else {
    if (src_ == nullptr) {
      std::cerr << "gadfg" << std::endl;
    }
    for (int i = 0; i < max_block_size; ++i) {
      src_[i] = rnd_.Rand8();
      pred_[i] = rnd_.Rand8();
    }
  }
}

void AV1SubtractBlockTestBase::CheckResult() {
  const int test_num = 100;
  int i;

  for (i = 0; i < test_num; ++i) {
    FillInputs();

    func_(block_height_, block_width_, diff_, block_width_, src_, block_width_,
          pred_, block_width_);

    if (hbd_)
      for (int r = 0; r < block_height_; ++r) {
        for (int c = 0; c < block_width_; ++c) {
          EXPECT_EQ(diff_[r * block_width_ + c],
                    (CONVERT_TO_SHORTPTR(src_)[r * block_width_ + c] -
                     CONVERT_TO_SHORTPTR(pred_)[r * block_width_ + c]))
              << "r = " << r << ", c = " << c << ", test: " << i;
        }
      }
    else {
      for (int r = 0; r < block_height_; ++r) {
        for (int c = 0; c < block_width_; ++c) {
          EXPECT_EQ(diff_[r * block_width_ + c],
                    src_[r * block_width_ + c] - pred_[r * block_width_ + c])
              << "r = " << r << ", c = " << c << ", test: " << i;
        }
      }
    }
  }
}

void AV1SubtractBlockTestBase::RunForSpeed() {
  const int test_num = 200000;
  int i;

  if (ref_func_ == func_) GTEST_SKIP();

  FillInputs();

  aom_usec_timer ref_timer;
  aom_usec_timer_start(&ref_timer);
  for (i = 0; i < test_num; ++i) {
    ref_func_(block_height_, block_width_, diff_, block_width_, src_,
              block_width_, pred_, block_width_);
  }
  aom_usec_timer_mark(&ref_timer);
  const int64_t ref_elapsed_time = aom_usec_timer_elapsed(&ref_timer);

  FillInputs();

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

class AV1SubtractBlockTest : public ::testing::WithParamInterface<Params>,
                             public AV1SubtractBlockTestBase {
 public:
  AV1SubtractBlockTest()
      : AV1SubtractBlockTestBase(GET_PARAM(0), -1, GET_PARAM(1), GET_PARAM(2)) {
  }
};
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(AV1SubtractBlockTest);

TEST_P(AV1SubtractBlockTest, CheckResult) { CheckResult(); }
TEST_P(AV1SubtractBlockTest, DISABLED_Speed) { RunForSpeed(); }

const BLOCK_SIZE kValidBlockSize[] = { BLOCK_4X4,    BLOCK_4X8,    BLOCK_8X4,
                                       BLOCK_8X8,    BLOCK_8X16,   BLOCK_16X8,
                                       BLOCK_16X16,  BLOCK_16X32,  BLOCK_32X16,
                                       BLOCK_32X32,  BLOCK_32X64,  BLOCK_64X32,
                                       BLOCK_64X64,  BLOCK_64X128, BLOCK_128X64,
                                       BLOCK_128X128 };

INSTANTIATE_TEST_SUITE_P(
    C, AV1SubtractBlockTest,
    ::testing::Combine(::testing::ValuesIn(kValidBlockSize),
                       ::testing::Values(&aom_subtract_block_c),
                       ::testing::Values(&aom_subtract_block_c)));

#if HAVE_SSE2
INSTANTIATE_TEST_SUITE_P(
    SSE2, AV1SubtractBlockTest,
    ::testing::Combine(::testing::ValuesIn(kValidBlockSize),
                       ::testing::Values(&aom_subtract_block_sse2),
                       ::testing::Values(&aom_subtract_block_c)));
#endif
#if HAVE_AVX2
INSTANTIATE_TEST_SUITE_P(
    AVX2, AV1SubtractBlockTest,
    ::testing::Combine(::testing::ValuesIn(kValidBlockSize),
                       ::testing::Values(&aom_subtract_block_avx2),
                       ::testing::Values(&aom_subtract_block_c)));

#endif
#if HAVE_NEON
INSTANTIATE_TEST_SUITE_P(
    NEON, AV1SubtractBlockTest,
    ::testing::Combine(::testing::ValuesIn(kValidBlockSize),
                       ::testing::Values(&aom_subtract_block_neon),
                       ::testing::Values(&aom_subtract_block_c)));

#endif

#if CONFIG_AV1_HIGHBITDEPTH

// <BLOCK_SIZE, bit_depth, optimized subtract func, reference subtract func>
using ParamsHBD = tuple<BLOCK_SIZE, int, SubtractFunc, SubtractFunc>;

class AV1HBDSubtractBlockTest : public ::testing::WithParamInterface<ParamsHBD>,
                                public AV1SubtractBlockTestBase {
 public:
  AV1HBDSubtractBlockTest()
      : AV1SubtractBlockTestBase(GET_PARAM(0), GET_PARAM(1), GET_PARAM(2),
                                 GET_PARAM(3)) {}
};
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(AV1HBDSubtractBlockTest);

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
