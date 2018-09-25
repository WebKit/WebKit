/*
 *  Copyright (c) 2012 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "third_party/googletest/src/include/gtest/gtest.h"

#include "./vp9_rtcd.h"
#include "./vpx_config.h"
#include "./vpx_dsp_rtcd.h"

#include "test/acm_random.h"
#include "test/clear_system_state.h"
#include "test/register_state_check.h"
#include "test/util.h"
#include "vpx_mem/vpx_mem.h"
#include "vpx_ports/vpx_timer.h"

using libvpx_test::ACMRandom;

namespace {
class AverageTestBase : public ::testing::Test {
 public:
  AverageTestBase(int width, int height) : width_(width), height_(height) {}

  static void SetUpTestCase() {
    source_data_ = reinterpret_cast<uint8_t *>(
        vpx_memalign(kDataAlignment, kDataBlockSize));
  }

  static void TearDownTestCase() {
    vpx_free(source_data_);
    source_data_ = NULL;
  }

  virtual void TearDown() { libvpx_test::ClearSystemState(); }

 protected:
  // Handle blocks up to 4 blocks 64x64 with stride up to 128
  static const int kDataAlignment = 16;
  static const int kDataBlockSize = 64 * 128;

  virtual void SetUp() {
    source_stride_ = (width_ + 31) & ~31;
    rnd_.Reset(ACMRandom::DeterministicSeed());
  }

  // Sum Pixels
  static unsigned int ReferenceAverage8x8(const uint8_t *source, int pitch) {
    unsigned int average = 0;
    for (int h = 0; h < 8; ++h) {
      for (int w = 0; w < 8; ++w) average += source[h * pitch + w];
    }
    return ((average + 32) >> 6);
  }

  static unsigned int ReferenceAverage4x4(const uint8_t *source, int pitch) {
    unsigned int average = 0;
    for (int h = 0; h < 4; ++h) {
      for (int w = 0; w < 4; ++w) average += source[h * pitch + w];
    }
    return ((average + 8) >> 4);
  }

  void FillConstant(uint8_t fill_constant) {
    for (int i = 0; i < width_ * height_; ++i) {
      source_data_[i] = fill_constant;
    }
  }

  void FillRandom() {
    for (int i = 0; i < width_ * height_; ++i) {
      source_data_[i] = rnd_.Rand8();
    }
  }

  int width_, height_;
  static uint8_t *source_data_;
  int source_stride_;

  ACMRandom rnd_;
};
typedef unsigned int (*AverageFunction)(const uint8_t *s, int pitch);

typedef std::tr1::tuple<int, int, int, int, AverageFunction> AvgFunc;

class AverageTest : public AverageTestBase,
                    public ::testing::WithParamInterface<AvgFunc> {
 public:
  AverageTest() : AverageTestBase(GET_PARAM(0), GET_PARAM(1)) {}

 protected:
  void CheckAverages() {
    const int block_size = GET_PARAM(3);
    unsigned int expected = 0;
    if (block_size == 8) {
      expected =
          ReferenceAverage8x8(source_data_ + GET_PARAM(2), source_stride_);
    } else if (block_size == 4) {
      expected =
          ReferenceAverage4x4(source_data_ + GET_PARAM(2), source_stride_);
    }

    ASM_REGISTER_STATE_CHECK(
        GET_PARAM(4)(source_data_ + GET_PARAM(2), source_stride_));
    unsigned int actual =
        GET_PARAM(4)(source_data_ + GET_PARAM(2), source_stride_);

    EXPECT_EQ(expected, actual);
  }
};

typedef void (*IntProRowFunc)(int16_t hbuf[16], uint8_t const *ref,
                              const int ref_stride, const int height);

typedef std::tr1::tuple<int, IntProRowFunc, IntProRowFunc> IntProRowParam;

class IntProRowTest : public AverageTestBase,
                      public ::testing::WithParamInterface<IntProRowParam> {
 public:
  IntProRowTest()
      : AverageTestBase(16, GET_PARAM(0)), hbuf_asm_(NULL), hbuf_c_(NULL) {
    asm_func_ = GET_PARAM(1);
    c_func_ = GET_PARAM(2);
  }

 protected:
  virtual void SetUp() {
    hbuf_asm_ = reinterpret_cast<int16_t *>(
        vpx_memalign(kDataAlignment, sizeof(*hbuf_asm_) * 16));
    hbuf_c_ = reinterpret_cast<int16_t *>(
        vpx_memalign(kDataAlignment, sizeof(*hbuf_c_) * 16));
  }

  virtual void TearDown() {
    vpx_free(hbuf_c_);
    hbuf_c_ = NULL;
    vpx_free(hbuf_asm_);
    hbuf_asm_ = NULL;
  }

  void RunComparison() {
    ASM_REGISTER_STATE_CHECK(c_func_(hbuf_c_, source_data_, 0, height_));
    ASM_REGISTER_STATE_CHECK(asm_func_(hbuf_asm_, source_data_, 0, height_));
    EXPECT_EQ(0, memcmp(hbuf_c_, hbuf_asm_, sizeof(*hbuf_c_) * 16))
        << "Output mismatch";
  }

 private:
  IntProRowFunc asm_func_;
  IntProRowFunc c_func_;
  int16_t *hbuf_asm_;
  int16_t *hbuf_c_;
};

typedef int16_t (*IntProColFunc)(uint8_t const *ref, const int width);

typedef std::tr1::tuple<int, IntProColFunc, IntProColFunc> IntProColParam;

class IntProColTest : public AverageTestBase,
                      public ::testing::WithParamInterface<IntProColParam> {
 public:
  IntProColTest() : AverageTestBase(GET_PARAM(0), 1), sum_asm_(0), sum_c_(0) {
    asm_func_ = GET_PARAM(1);
    c_func_ = GET_PARAM(2);
  }

 protected:
  void RunComparison() {
    ASM_REGISTER_STATE_CHECK(sum_c_ = c_func_(source_data_, width_));
    ASM_REGISTER_STATE_CHECK(sum_asm_ = asm_func_(source_data_, width_));
    EXPECT_EQ(sum_c_, sum_asm_) << "Output mismatch";
  }

 private:
  IntProColFunc asm_func_;
  IntProColFunc c_func_;
  int16_t sum_asm_;
  int16_t sum_c_;
};

typedef int (*SatdFunc)(const tran_low_t *coeffs, int length);
typedef std::tr1::tuple<int, SatdFunc> SatdTestParam;

class SatdTest : public ::testing::Test,
                 public ::testing::WithParamInterface<SatdTestParam> {
 protected:
  virtual void SetUp() {
    satd_size_ = GET_PARAM(0);
    satd_func_ = GET_PARAM(1);
    rnd_.Reset(ACMRandom::DeterministicSeed());
    src_ = reinterpret_cast<tran_low_t *>(
        vpx_memalign(16, sizeof(*src_) * satd_size_));
    ASSERT_TRUE(src_ != NULL);
  }

  virtual void TearDown() {
    libvpx_test::ClearSystemState();
    vpx_free(src_);
  }

  void FillConstant(const tran_low_t val) {
    for (int i = 0; i < satd_size_; ++i) src_[i] = val;
  }

  void FillRandom() {
    for (int i = 0; i < satd_size_; ++i) {
      const int16_t tmp = rnd_.Rand16();
      src_[i] = (tran_low_t)tmp;
    }
  }

  void Check(const int expected) {
    int total;
    ASM_REGISTER_STATE_CHECK(total = satd_func_(src_, satd_size_));
    EXPECT_EQ(expected, total);
  }

  int satd_size_;

 private:
  tran_low_t *src_;
  SatdFunc satd_func_;
  ACMRandom rnd_;
};

typedef int64_t (*BlockErrorFunc)(const tran_low_t *coeff,
                                  const tran_low_t *dqcoeff, int block_size);
typedef std::tr1::tuple<int, BlockErrorFunc> BlockErrorTestFPParam;

class BlockErrorTestFP
    : public ::testing::Test,
      public ::testing::WithParamInterface<BlockErrorTestFPParam> {
 protected:
  virtual void SetUp() {
    txfm_size_ = GET_PARAM(0);
    block_error_func_ = GET_PARAM(1);
    rnd_.Reset(ACMRandom::DeterministicSeed());
    coeff_ = reinterpret_cast<tran_low_t *>(
        vpx_memalign(16, sizeof(*coeff_) * txfm_size_));
    dqcoeff_ = reinterpret_cast<tran_low_t *>(
        vpx_memalign(16, sizeof(*dqcoeff_) * txfm_size_));
    ASSERT_TRUE(coeff_ != NULL);
    ASSERT_TRUE(dqcoeff_ != NULL);
  }

  virtual void TearDown() {
    libvpx_test::ClearSystemState();
    vpx_free(coeff_);
    vpx_free(dqcoeff_);
  }

  void FillConstant(const tran_low_t coeff_val, const tran_low_t dqcoeff_val) {
    for (int i = 0; i < txfm_size_; ++i) coeff_[i] = coeff_val;
    for (int i = 0; i < txfm_size_; ++i) dqcoeff_[i] = dqcoeff_val;
  }

  void FillRandom() {
    // Just two fixed seeds
    rnd_.Reset(0xb0b9);
    for (int i = 0; i < txfm_size_; ++i) coeff_[i] = rnd_.Rand16() >> 1;
    rnd_.Reset(0xb0c8);
    for (int i = 0; i < txfm_size_; ++i) dqcoeff_[i] = rnd_.Rand16() >> 1;
  }

  void Check(const int64_t expected) {
    int64_t total;
    ASM_REGISTER_STATE_CHECK(
        total = block_error_func_(coeff_, dqcoeff_, txfm_size_));
    EXPECT_EQ(expected, total);
  }

  int txfm_size_;

 private:
  tran_low_t *coeff_;
  tran_low_t *dqcoeff_;
  BlockErrorFunc block_error_func_;
  ACMRandom rnd_;
};

uint8_t *AverageTestBase::source_data_ = NULL;

TEST_P(AverageTest, MinValue) {
  FillConstant(0);
  CheckAverages();
}

TEST_P(AverageTest, MaxValue) {
  FillConstant(255);
  CheckAverages();
}

TEST_P(AverageTest, Random) {
  // The reference frame, but not the source frame, may be unaligned for
  // certain types of searches.
  for (int i = 0; i < 1000; i++) {
    FillRandom();
    CheckAverages();
  }
}

TEST_P(IntProRowTest, MinValue) {
  FillConstant(0);
  RunComparison();
}

TEST_P(IntProRowTest, MaxValue) {
  FillConstant(255);
  RunComparison();
}

TEST_P(IntProRowTest, Random) {
  FillRandom();
  RunComparison();
}

TEST_P(IntProColTest, MinValue) {
  FillConstant(0);
  RunComparison();
}

TEST_P(IntProColTest, MaxValue) {
  FillConstant(255);
  RunComparison();
}

TEST_P(IntProColTest, Random) {
  FillRandom();
  RunComparison();
}

TEST_P(SatdTest, MinValue) {
  const int kMin = -32640;
  const int expected = -kMin * satd_size_;
  FillConstant(kMin);
  Check(expected);
}

TEST_P(SatdTest, MaxValue) {
  const int kMax = 32640;
  const int expected = kMax * satd_size_;
  FillConstant(kMax);
  Check(expected);
}

TEST_P(SatdTest, Random) {
  int expected;
  switch (satd_size_) {
    case 16: expected = 205298; break;
    case 64: expected = 1113950; break;
    case 256: expected = 4268415; break;
    case 1024: expected = 16954082; break;
    default:
      FAIL() << "Invalid satd size (" << satd_size_
             << ") valid: 16/64/256/1024";
  }
  FillRandom();
  Check(expected);
}

TEST_P(SatdTest, DISABLED_Speed) {
  const int kCountSpeedTestBlock = 20000;
  vpx_usec_timer timer;
  DECLARE_ALIGNED(16, tran_low_t, coeff[1024]);
  const int blocksize = GET_PARAM(0);

  vpx_usec_timer_start(&timer);
  for (int i = 0; i < kCountSpeedTestBlock; ++i) {
    GET_PARAM(1)(coeff, blocksize);
  }
  vpx_usec_timer_mark(&timer);
  const int elapsed_time = static_cast<int>(vpx_usec_timer_elapsed(&timer));
  printf("blocksize: %4d time: %4d us\n", blocksize, elapsed_time);
}

TEST_P(BlockErrorTestFP, MinValue) {
  const int64_t kMin = -32640;
  const int64_t expected = kMin * kMin * txfm_size_;
  FillConstant(kMin, 0);
  Check(expected);
}

TEST_P(BlockErrorTestFP, MaxValue) {
  const int64_t kMax = 32640;
  const int64_t expected = kMax * kMax * txfm_size_;
  FillConstant(kMax, 0);
  Check(expected);
}

TEST_P(BlockErrorTestFP, Random) {
  int64_t expected;
  switch (txfm_size_) {
    case 16: expected = 2051681432; break;
    case 64: expected = 11075114379; break;
    case 256: expected = 44386271116; break;
    case 1024: expected = 184774996089; break;
    default:
      FAIL() << "Invalid satd size (" << txfm_size_
             << ") valid: 16/64/256/1024";
  }
  FillRandom();
  Check(expected);
}

TEST_P(BlockErrorTestFP, DISABLED_Speed) {
  const int kCountSpeedTestBlock = 20000;
  vpx_usec_timer timer;
  DECLARE_ALIGNED(16, tran_low_t, coeff[1024]);
  DECLARE_ALIGNED(16, tran_low_t, dqcoeff[1024]);
  const int blocksize = GET_PARAM(0);

  vpx_usec_timer_start(&timer);
  for (int i = 0; i < kCountSpeedTestBlock; ++i) {
    GET_PARAM(1)(coeff, dqcoeff, blocksize);
  }
  vpx_usec_timer_mark(&timer);
  const int elapsed_time = static_cast<int>(vpx_usec_timer_elapsed(&timer));
  printf("blocksize: %4d time: %4d us\n", blocksize, elapsed_time);
}

using std::tr1::make_tuple;

INSTANTIATE_TEST_CASE_P(
    C, AverageTest,
    ::testing::Values(make_tuple(16, 16, 1, 8, &vpx_avg_8x8_c),
                      make_tuple(16, 16, 1, 4, &vpx_avg_4x4_c)));

INSTANTIATE_TEST_CASE_P(C, SatdTest,
                        ::testing::Values(make_tuple(16, &vpx_satd_c),
                                          make_tuple(64, &vpx_satd_c),
                                          make_tuple(256, &vpx_satd_c),
                                          make_tuple(1024, &vpx_satd_c)));

INSTANTIATE_TEST_CASE_P(
    C, BlockErrorTestFP,
    ::testing::Values(make_tuple(16, &vp9_block_error_fp_c),
                      make_tuple(64, &vp9_block_error_fp_c),
                      make_tuple(256, &vp9_block_error_fp_c),
                      make_tuple(1024, &vp9_block_error_fp_c)));

#if HAVE_SSE2
INSTANTIATE_TEST_CASE_P(
    SSE2, AverageTest,
    ::testing::Values(make_tuple(16, 16, 0, 8, &vpx_avg_8x8_sse2),
                      make_tuple(16, 16, 5, 8, &vpx_avg_8x8_sse2),
                      make_tuple(32, 32, 15, 8, &vpx_avg_8x8_sse2),
                      make_tuple(16, 16, 0, 4, &vpx_avg_4x4_sse2),
                      make_tuple(16, 16, 5, 4, &vpx_avg_4x4_sse2),
                      make_tuple(32, 32, 15, 4, &vpx_avg_4x4_sse2)));

INSTANTIATE_TEST_CASE_P(
    SSE2, IntProRowTest,
    ::testing::Values(make_tuple(16, &vpx_int_pro_row_sse2, &vpx_int_pro_row_c),
                      make_tuple(32, &vpx_int_pro_row_sse2, &vpx_int_pro_row_c),
                      make_tuple(64, &vpx_int_pro_row_sse2,
                                 &vpx_int_pro_row_c)));

INSTANTIATE_TEST_CASE_P(
    SSE2, IntProColTest,
    ::testing::Values(make_tuple(16, &vpx_int_pro_col_sse2, &vpx_int_pro_col_c),
                      make_tuple(32, &vpx_int_pro_col_sse2, &vpx_int_pro_col_c),
                      make_tuple(64, &vpx_int_pro_col_sse2,
                                 &vpx_int_pro_col_c)));

INSTANTIATE_TEST_CASE_P(SSE2, SatdTest,
                        ::testing::Values(make_tuple(16, &vpx_satd_sse2),
                                          make_tuple(64, &vpx_satd_sse2),
                                          make_tuple(256, &vpx_satd_sse2),
                                          make_tuple(1024, &vpx_satd_sse2)));

INSTANTIATE_TEST_CASE_P(
    SSE2, BlockErrorTestFP,
    ::testing::Values(make_tuple(16, &vp9_block_error_fp_sse2),
                      make_tuple(64, &vp9_block_error_fp_sse2),
                      make_tuple(256, &vp9_block_error_fp_sse2),
                      make_tuple(1024, &vp9_block_error_fp_sse2)));
#endif  // HAVE_SSE2

#if HAVE_AVX2
INSTANTIATE_TEST_CASE_P(AVX2, SatdTest,
                        ::testing::Values(make_tuple(16, &vpx_satd_avx2),
                                          make_tuple(64, &vpx_satd_avx2),
                                          make_tuple(256, &vpx_satd_avx2),
                                          make_tuple(1024, &vpx_satd_avx2)));

INSTANTIATE_TEST_CASE_P(
    AVX2, BlockErrorTestFP,
    ::testing::Values(make_tuple(16, &vp9_block_error_fp_avx2),
                      make_tuple(64, &vp9_block_error_fp_avx2),
                      make_tuple(256, &vp9_block_error_fp_avx2),
                      make_tuple(1024, &vp9_block_error_fp_avx2)));
#endif

#if HAVE_NEON
INSTANTIATE_TEST_CASE_P(
    NEON, AverageTest,
    ::testing::Values(make_tuple(16, 16, 0, 8, &vpx_avg_8x8_neon),
                      make_tuple(16, 16, 5, 8, &vpx_avg_8x8_neon),
                      make_tuple(32, 32, 15, 8, &vpx_avg_8x8_neon),
                      make_tuple(16, 16, 0, 4, &vpx_avg_4x4_neon),
                      make_tuple(16, 16, 5, 4, &vpx_avg_4x4_neon),
                      make_tuple(32, 32, 15, 4, &vpx_avg_4x4_neon)));

INSTANTIATE_TEST_CASE_P(
    NEON, IntProRowTest,
    ::testing::Values(make_tuple(16, &vpx_int_pro_row_neon, &vpx_int_pro_row_c),
                      make_tuple(32, &vpx_int_pro_row_neon, &vpx_int_pro_row_c),
                      make_tuple(64, &vpx_int_pro_row_neon,
                                 &vpx_int_pro_row_c)));

INSTANTIATE_TEST_CASE_P(
    NEON, IntProColTest,
    ::testing::Values(make_tuple(16, &vpx_int_pro_col_neon, &vpx_int_pro_col_c),
                      make_tuple(32, &vpx_int_pro_col_neon, &vpx_int_pro_col_c),
                      make_tuple(64, &vpx_int_pro_col_neon,
                                 &vpx_int_pro_col_c)));

INSTANTIATE_TEST_CASE_P(NEON, SatdTest,
                        ::testing::Values(make_tuple(16, &vpx_satd_neon),
                                          make_tuple(64, &vpx_satd_neon),
                                          make_tuple(256, &vpx_satd_neon),
                                          make_tuple(1024, &vpx_satd_neon)));

// TODO(jianj): Remove the highbitdepth flag once the SIMD functions are
// in place.
#if !CONFIG_VP9_HIGHBITDEPTH
INSTANTIATE_TEST_CASE_P(
    NEON, BlockErrorTestFP,
    ::testing::Values(make_tuple(16, &vp9_block_error_fp_neon),
                      make_tuple(64, &vp9_block_error_fp_neon),
                      make_tuple(256, &vp9_block_error_fp_neon),
                      make_tuple(1024, &vp9_block_error_fp_neon)));
#endif  // !CONFIG_VP9_HIGHBITDEPTH
#endif  // HAVE_NEON

#if HAVE_MSA
INSTANTIATE_TEST_CASE_P(
    MSA, AverageTest,
    ::testing::Values(make_tuple(16, 16, 0, 8, &vpx_avg_8x8_msa),
                      make_tuple(16, 16, 5, 8, &vpx_avg_8x8_msa),
                      make_tuple(32, 32, 15, 8, &vpx_avg_8x8_msa),
                      make_tuple(16, 16, 0, 4, &vpx_avg_4x4_msa),
                      make_tuple(16, 16, 5, 4, &vpx_avg_4x4_msa),
                      make_tuple(32, 32, 15, 4, &vpx_avg_4x4_msa)));

INSTANTIATE_TEST_CASE_P(
    MSA, IntProRowTest,
    ::testing::Values(make_tuple(16, &vpx_int_pro_row_msa, &vpx_int_pro_row_c),
                      make_tuple(32, &vpx_int_pro_row_msa, &vpx_int_pro_row_c),
                      make_tuple(64, &vpx_int_pro_row_msa,
                                 &vpx_int_pro_row_c)));

INSTANTIATE_TEST_CASE_P(
    MSA, IntProColTest,
    ::testing::Values(make_tuple(16, &vpx_int_pro_col_msa, &vpx_int_pro_col_c),
                      make_tuple(32, &vpx_int_pro_col_msa, &vpx_int_pro_col_c),
                      make_tuple(64, &vpx_int_pro_col_msa,
                                 &vpx_int_pro_col_c)));

// TODO(jingning): Remove the highbitdepth flag once the SIMD functions are
// in place.
#if !CONFIG_VP9_HIGHBITDEPTH
INSTANTIATE_TEST_CASE_P(MSA, SatdTest,
                        ::testing::Values(make_tuple(16, &vpx_satd_msa),
                                          make_tuple(64, &vpx_satd_msa),
                                          make_tuple(256, &vpx_satd_msa),
                                          make_tuple(1024, &vpx_satd_msa)));
#endif  // !CONFIG_VP9_HIGHBITDEPTH
#endif  // HAVE_MSA

}  // namespace
