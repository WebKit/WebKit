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
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <tuple>

#include "third_party/googletest/src/googletest/include/gtest/gtest.h"
#include "test/acm_random.h"
#include "test/register_state_check.h"
#include "test/util.h"

#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"

#include "aom/aom_integer.h"

using libaom_test::ACMRandom;

namespace {
const int number_of_iterations = 200;

typedef unsigned int (*MaskedSADFunc)(const uint8_t *src, int src_stride,
                                      const uint8_t *ref, int ref_stride,
                                      const uint8_t *second_pred,
                                      const uint8_t *msk, int msk_stride,
                                      int invert_mask);
typedef std::tuple<MaskedSADFunc, MaskedSADFunc> MaskedSADParam;

typedef void (*MaskedSADx4Func)(const uint8_t *src, int src_stride,
                                const uint8_t *ref[], int ref_stride,
                                const uint8_t *second_pred, const uint8_t *msk,
                                int msk_stride, int invert_mask,
                                unsigned sads[]);

typedef std::tuple<MaskedSADx4Func, MaskedSADx4Func> MaskedSADx4Param;

class MaskedSADTestBase : public ::testing::Test {
 public:
  ~MaskedSADTestBase() override = default;
  void SetUp() override = 0;
  virtual void runRef(const uint8_t *src_ptr, int src_stride,
                      const uint8_t *ref_ptr[], int ref_stride,
                      const uint8_t *second_pred, const uint8_t *msk,
                      int msk_stride, int inv_mask, unsigned sads[],
                      int times) = 0;
  virtual void runTest(const uint8_t *src_ptr, int src_stride,
                       const uint8_t *ref_ptr[], int ref_stride,
                       const uint8_t *second_pred, const uint8_t *msk,
                       int msk_stride, int inv_mask, unsigned sads[],
                       int times) = 0;

  void runMaskedSADTest(int run_times);
};

class MaskedSADTest : public MaskedSADTestBase,
                      public ::testing::WithParamInterface<MaskedSADParam> {
 public:
  ~MaskedSADTest() override = default;
  void SetUp() override {
    maskedSAD_op_ = GET_PARAM(0);
    ref_maskedSAD_op_ = GET_PARAM(1);
  }

  void runRef(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr[],
              int ref_stride, const uint8_t *second_pred, const uint8_t *msk,
              int msk_stride, int inv_mask, unsigned sads[],
              int times) override;
  void runTest(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr[],
               int ref_stride, const uint8_t *second_pred, const uint8_t *msk,
               int msk_stride, int inv_mask, unsigned sads[],
               int times) override;

 protected:
  MaskedSADFunc maskedSAD_op_;
  MaskedSADFunc ref_maskedSAD_op_;
};
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(MaskedSADTest);

class MaskedSADx4Test : public MaskedSADTestBase,
                        public ::testing::WithParamInterface<MaskedSADx4Param> {
 public:
  ~MaskedSADx4Test() override = default;
  void SetUp() override {
    maskedSAD_op_ = GET_PARAM(0);
    ref_maskedSAD_op_ = GET_PARAM(1);
  }
  void runRef(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr[],
              int ref_stride, const uint8_t *second_pred, const uint8_t *msk,
              int msk_stride, int inv_mask, unsigned sads[],
              int times) override;
  void runTest(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr[],
               int ref_stride, const uint8_t *second_pred, const uint8_t *msk,
               int msk_stride, int inv_mask, unsigned sads[],
               int times) override;

 protected:
  MaskedSADx4Func maskedSAD_op_;
  MaskedSADx4Func ref_maskedSAD_op_;
};
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(MaskedSADx4Test);

void MaskedSADTest::runRef(const uint8_t *src_ptr, int src_stride,
                           const uint8_t *ref_ptr[], int ref_stride,
                           const uint8_t *second_pred, const uint8_t *msk,
                           int msk_stride, int invert_mask, unsigned sads[],
                           int times) {
  for (int repeat = 0; repeat < times; ++repeat) {
    sads[0] = ref_maskedSAD_op_(src_ptr, src_stride, ref_ptr[0], ref_stride,
                                second_pred, msk, msk_stride, invert_mask);
  }
}

void MaskedSADTest::runTest(const uint8_t *src_ptr, int src_stride,
                            const uint8_t *ref_ptr[], int ref_stride,
                            const uint8_t *second_pred, const uint8_t *msk,
                            int msk_stride, int invert_mask, unsigned sads[],
                            int times) {
  if (times == 1) {
    sads[0] = maskedSAD_op_(src_ptr, src_stride, ref_ptr[0], ref_stride,
                            second_pred, msk, msk_stride, invert_mask);
  } else {
    for (int repeat = 0; repeat < times; ++repeat) {
      API_REGISTER_STATE_CHECK(
          sads[0] = maskedSAD_op_(src_ptr, src_stride, ref_ptr[0], ref_stride,
                                  second_pred, msk, msk_stride, invert_mask));
    }
  }
}

void MaskedSADx4Test::runRef(const uint8_t *src_ptr, int src_stride,
                             const uint8_t *ref_ptr[], int ref_stride,
                             const uint8_t *second_pred, const uint8_t *msk,
                             int msk_stride, int invert_mask, unsigned sads[],
                             int times) {
  for (int repeat = 0; repeat < times; ++repeat) {
    ref_maskedSAD_op_(src_ptr, src_stride, ref_ptr, ref_stride, second_pred,
                      msk, msk_stride, invert_mask, sads);
  }
}

void MaskedSADx4Test::runTest(const uint8_t *src_ptr, int src_stride,
                              const uint8_t *ref_ptr[], int ref_stride,
                              const uint8_t *second_pred, const uint8_t *msk,
                              int msk_stride, int invert_mask, unsigned sads[],
                              int times) {
  if (times == 1) {
    API_REGISTER_STATE_CHECK(maskedSAD_op_(src_ptr, src_stride, ref_ptr,
                                           ref_stride, second_pred, msk,
                                           msk_stride, invert_mask, sads));
  } else {
    for (int repeat = 0; repeat < times; ++repeat) {
      maskedSAD_op_(src_ptr, src_stride, ref_ptr, ref_stride, second_pred, msk,
                    msk_stride, invert_mask, sads);
    }
  }
}

void MaskedSADTestBase::runMaskedSADTest(int run_times) {
  ACMRandom rnd(ACMRandom::DeterministicSeed());
  const unsigned kBlockSize = MAX_SB_SIZE * MAX_SB_SIZE;
  DECLARE_ALIGNED(16, uint8_t, src_ptr[MAX_SB_SIZE * MAX_SB_SIZE]);
  DECLARE_ALIGNED(16, uint8_t, ref_ptr[MAX_SB_SIZE * MAX_SB_SIZE * 4]);
  DECLARE_ALIGNED(16, uint8_t, second_pred_ptr[MAX_SB_SIZE * MAX_SB_SIZE]);
  DECLARE_ALIGNED(16, uint8_t, msk_ptr[MAX_SB_SIZE * MAX_SB_SIZE]);

  const uint8_t *refs[] = { ref_ptr, ref_ptr + kBlockSize,
                            ref_ptr + 2 * kBlockSize,
                            ref_ptr + 3 * kBlockSize };
  unsigned sads[] = { 0, 0, 0, 0 };
  unsigned sads_ref[] = { 0, 0, 0, 0 };
  int err_count = 0;
  int first_failure = -1;
  int src_stride = MAX_SB_SIZE;
  int ref_stride = MAX_SB_SIZE;
  int msk_stride = MAX_SB_SIZE;
  const int iters = run_times == 1 ? number_of_iterations : 1;
  for (int i = 0; i < iters; ++i) {
    if (run_times == 1 && i == 0) {
      // The maximum accumulator value occurs when src=0 and
      // ref/second_pref=255 (or vice-versa, since we take the absolute
      // difference). Check this case explicitly to ensure we do not overflow
      // during accumulation.
      for (int j = 0; j < MAX_SB_SIZE * MAX_SB_SIZE; j++) {
        src_ptr[j] = 0;
        ref_ptr[j] = 255;
        (ref_ptr + kBlockSize)[j] = 255;
        (ref_ptr + 2 * kBlockSize)[j] = 255;
        (ref_ptr + 3 * kBlockSize)[j] = 255;
        second_pred_ptr[j] = 255;
      }
    } else {
      for (int j = 0; j < MAX_SB_SIZE * MAX_SB_SIZE; j++) {
        src_ptr[j] = rnd.Rand8();
        ref_ptr[j] = rnd.Rand8();
        (ref_ptr + kBlockSize)[j] = rnd.Rand8();
        (ref_ptr + 2 * kBlockSize)[j] = rnd.Rand8();
        (ref_ptr + 3 * kBlockSize)[j] = rnd.Rand8();
        second_pred_ptr[j] = rnd.Rand8();
      }
    }
    for (int j = 0; j < MAX_SB_SIZE * MAX_SB_SIZE; j++) {
      msk_ptr[j] = ((rnd.Rand8() & 0x7f) > 64) ? rnd.Rand8() & 0x3f : 64;
      assert(msk_ptr[j] <= 64);
    }

    for (int invert_mask = 0; invert_mask < 2; ++invert_mask) {
      aom_usec_timer timer;
      aom_usec_timer_start(&timer);
      runRef(src_ptr, src_stride, refs, ref_stride, second_pred_ptr, msk_ptr,
             msk_stride, invert_mask, sads_ref, run_times);
      aom_usec_timer_mark(&timer);
      const double time1 = static_cast<double>(aom_usec_timer_elapsed(&timer));

      aom_usec_timer_start(&timer);
      runTest(src_ptr, src_stride, refs, ref_stride, second_pred_ptr, msk_ptr,
              msk_stride, invert_mask, sads, run_times);
      aom_usec_timer_mark(&timer);
      const double time2 = static_cast<double>(aom_usec_timer_elapsed(&timer));

      if (run_times > 10) {
        printf("%7.2f/%7.2fns", time1, time2);
        printf("(%3.2f)\n", time1 / time2);
      }
      if (sads_ref[0] != sads[0] || sads_ref[1] != sads[1] ||
          sads_ref[2] != sads[2] || sads_ref[3] != sads[3]) {
        err_count++;
        if (first_failure == -1) first_failure = i;
      }
    }
  }
  EXPECT_EQ(0, err_count) << "Error: Masked SAD Test,  output doesn't match. "
                          << "First failed at test case " << first_failure;
}

TEST_P(MaskedSADTest, OperationCheck) { runMaskedSADTest(1); }

TEST_P(MaskedSADTest, DISABLED_Speed) { runMaskedSADTest(2000000); }

TEST_P(MaskedSADx4Test, OperationCheck) { runMaskedSADTest(1); }

TEST_P(MaskedSADx4Test, DISABLED_Speed) { runMaskedSADTest(2000000); }

#if CONFIG_AV1_HIGHBITDEPTH
typedef unsigned int (*HighbdMaskedSADFunc)(const uint8_t *src, int src_stride,
                                            const uint8_t *ref, int ref_stride,
                                            const uint8_t *second_pred,
                                            const uint8_t *msk, int msk_stride,
                                            int invert_mask);
typedef std::tuple<HighbdMaskedSADFunc, HighbdMaskedSADFunc>
    HighbdMaskedSADParam;

class HighbdMaskedSADTest
    : public ::testing::TestWithParam<HighbdMaskedSADParam> {
 public:
  ~HighbdMaskedSADTest() override = default;
  void SetUp() override {
    maskedSAD_op_ = GET_PARAM(0);
    ref_maskedSAD_op_ = GET_PARAM(1);
  }

  void runHighbdMaskedSADTest(int run_times);

 protected:
  HighbdMaskedSADFunc maskedSAD_op_;
  HighbdMaskedSADFunc ref_maskedSAD_op_;
};
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(HighbdMaskedSADTest);

void HighbdMaskedSADTest::runHighbdMaskedSADTest(int run_times) {
  unsigned int ref_ret = 0, ret = 1;
  ACMRandom rnd(ACMRandom::DeterministicSeed());
  DECLARE_ALIGNED(16, uint16_t, src_ptr[MAX_SB_SIZE * MAX_SB_SIZE]);
  DECLARE_ALIGNED(16, uint16_t, ref_ptr[MAX_SB_SIZE * MAX_SB_SIZE]);
  DECLARE_ALIGNED(16, uint16_t, second_pred_ptr[MAX_SB_SIZE * MAX_SB_SIZE]);
  DECLARE_ALIGNED(16, uint8_t, msk_ptr[MAX_SB_SIZE * MAX_SB_SIZE]);
  uint8_t *src8_ptr = CONVERT_TO_BYTEPTR(src_ptr);
  uint8_t *ref8_ptr = CONVERT_TO_BYTEPTR(ref_ptr);
  uint8_t *second_pred8_ptr = CONVERT_TO_BYTEPTR(second_pred_ptr);
  int err_count = 0;
  int first_failure = -1;
  int src_stride = MAX_SB_SIZE;
  int ref_stride = MAX_SB_SIZE;
  int msk_stride = MAX_SB_SIZE;
  const int iters = run_times == 1 ? number_of_iterations : 1;
  for (int i = 0; i < iters; ++i) {
    for (int j = 0; j < MAX_SB_SIZE * MAX_SB_SIZE; j++) {
      src_ptr[j] = rnd.Rand16() & 0xfff;
      ref_ptr[j] = rnd.Rand16() & 0xfff;
      second_pred_ptr[j] = rnd.Rand16() & 0xfff;
      msk_ptr[j] = ((rnd.Rand8() & 0x7f) > 64) ? rnd.Rand8() & 0x3f : 64;
    }

    for (int invert_mask = 0; invert_mask < 2; ++invert_mask) {
      aom_usec_timer timer;
      aom_usec_timer_start(&timer);
      for (int repeat = 0; repeat < run_times; ++repeat) {
        ref_ret = ref_maskedSAD_op_(src8_ptr, src_stride, ref8_ptr, ref_stride,
                                    second_pred8_ptr, msk_ptr, msk_stride,
                                    invert_mask);
      }
      aom_usec_timer_mark(&timer);
      const double time1 = static_cast<double>(aom_usec_timer_elapsed(&timer));
      aom_usec_timer_start(&timer);
      if (run_times == 1) {
        API_REGISTER_STATE_CHECK(ret = maskedSAD_op_(src8_ptr, src_stride,
                                                     ref8_ptr, ref_stride,
                                                     second_pred8_ptr, msk_ptr,
                                                     msk_stride, invert_mask));
      } else {
        for (int repeat = 0; repeat < run_times; ++repeat) {
          ret =
              maskedSAD_op_(src8_ptr, src_stride, ref8_ptr, ref_stride,
                            second_pred8_ptr, msk_ptr, msk_stride, invert_mask);
        }
      }
      aom_usec_timer_mark(&timer);
      const double time2 = static_cast<double>(aom_usec_timer_elapsed(&timer));
      if (run_times > 10) {
        printf("%7.2f/%7.2fns", time1, time2);
        printf("(%3.2f)\n", time1 / time2);
      }
      if (ret != ref_ret) {
        err_count++;
        if (first_failure == -1) first_failure = i;
      }
    }
  }
  EXPECT_EQ(0, err_count)
      << "Error: High BD Masked SAD Test, output doesn't match. "
      << "First failed at test case " << first_failure;
}

TEST_P(HighbdMaskedSADTest, OperationCheck) { runHighbdMaskedSADTest(1); }

TEST_P(HighbdMaskedSADTest, DISABLED_Speed) { runHighbdMaskedSADTest(1000000); }
#endif  // CONFIG_AV1_HIGHBITDEPTH

using std::make_tuple;

#if HAVE_SSSE3
const MaskedSADParam msad_test[] = {
  make_tuple(&aom_masked_sad4x4_ssse3, &aom_masked_sad4x4_c),
  make_tuple(&aom_masked_sad4x8_ssse3, &aom_masked_sad4x8_c),
  make_tuple(&aom_masked_sad8x4_ssse3, &aom_masked_sad8x4_c),
  make_tuple(&aom_masked_sad8x8_ssse3, &aom_masked_sad8x8_c),
  make_tuple(&aom_masked_sad8x16_ssse3, &aom_masked_sad8x16_c),
  make_tuple(&aom_masked_sad16x8_ssse3, &aom_masked_sad16x8_c),
  make_tuple(&aom_masked_sad16x16_ssse3, &aom_masked_sad16x16_c),
  make_tuple(&aom_masked_sad16x32_ssse3, &aom_masked_sad16x32_c),
  make_tuple(&aom_masked_sad32x16_ssse3, &aom_masked_sad32x16_c),
  make_tuple(&aom_masked_sad32x32_ssse3, &aom_masked_sad32x32_c),
  make_tuple(&aom_masked_sad32x64_ssse3, &aom_masked_sad32x64_c),
  make_tuple(&aom_masked_sad64x32_ssse3, &aom_masked_sad64x32_c),
  make_tuple(&aom_masked_sad64x64_ssse3, &aom_masked_sad64x64_c),
  make_tuple(&aom_masked_sad64x128_ssse3, &aom_masked_sad64x128_c),
  make_tuple(&aom_masked_sad128x64_ssse3, &aom_masked_sad128x64_c),
  make_tuple(&aom_masked_sad128x128_ssse3, &aom_masked_sad128x128_c),
#if !CONFIG_REALTIME_ONLY
  make_tuple(&aom_masked_sad4x16_ssse3, &aom_masked_sad4x16_c),
  make_tuple(&aom_masked_sad16x4_ssse3, &aom_masked_sad16x4_c),
  make_tuple(&aom_masked_sad8x32_ssse3, &aom_masked_sad8x32_c),
  make_tuple(&aom_masked_sad32x8_ssse3, &aom_masked_sad32x8_c),
  make_tuple(&aom_masked_sad16x64_ssse3, &aom_masked_sad16x64_c),
  make_tuple(&aom_masked_sad64x16_ssse3, &aom_masked_sad64x16_c),
#endif
};

INSTANTIATE_TEST_SUITE_P(SSSE3, MaskedSADTest, ::testing::ValuesIn(msad_test));

const MaskedSADx4Param msadx4_test[] = {
  make_tuple(&aom_masked_sad4x4x4d_ssse3, &aom_masked_sad4x4x4d_c),
  make_tuple(&aom_masked_sad4x8x4d_ssse3, &aom_masked_sad4x8x4d_c),
  make_tuple(&aom_masked_sad8x4x4d_ssse3, &aom_masked_sad8x4x4d_c),
  make_tuple(&aom_masked_sad8x8x4d_ssse3, &aom_masked_sad8x8x4d_c),
  make_tuple(&aom_masked_sad8x16x4d_ssse3, &aom_masked_sad8x16x4d_c),
  make_tuple(&aom_masked_sad16x8x4d_ssse3, &aom_masked_sad16x8x4d_c),
  make_tuple(&aom_masked_sad16x16x4d_ssse3, &aom_masked_sad16x16x4d_c),
  make_tuple(&aom_masked_sad16x32x4d_ssse3, &aom_masked_sad16x32x4d_c),
  make_tuple(&aom_masked_sad32x16x4d_ssse3, &aom_masked_sad32x16x4d_c),
  make_tuple(&aom_masked_sad32x32x4d_ssse3, &aom_masked_sad32x32x4d_c),
  make_tuple(&aom_masked_sad32x64x4d_ssse3, &aom_masked_sad32x64x4d_c),
  make_tuple(&aom_masked_sad64x32x4d_ssse3, &aom_masked_sad64x32x4d_c),
  make_tuple(&aom_masked_sad64x64x4d_ssse3, &aom_masked_sad64x64x4d_c),
  make_tuple(&aom_masked_sad64x128x4d_ssse3, &aom_masked_sad64x128x4d_c),
  make_tuple(&aom_masked_sad128x64x4d_ssse3, &aom_masked_sad128x64x4d_c),
  make_tuple(&aom_masked_sad128x128x4d_ssse3, &aom_masked_sad128x128x4d_c),
#if !CONFIG_REALTIME_ONLY
  make_tuple(&aom_masked_sad4x16x4d_ssse3, &aom_masked_sad4x16x4d_c),
  make_tuple(&aom_masked_sad16x4x4d_ssse3, &aom_masked_sad16x4x4d_c),
  make_tuple(&aom_masked_sad8x32x4d_ssse3, &aom_masked_sad8x32x4d_c),
  make_tuple(&aom_masked_sad32x8x4d_ssse3, &aom_masked_sad32x8x4d_c),
  make_tuple(&aom_masked_sad16x64x4d_ssse3, &aom_masked_sad16x64x4d_c),
  make_tuple(&aom_masked_sad64x16x4d_ssse3, &aom_masked_sad64x16x4d_c),
#endif
};

INSTANTIATE_TEST_SUITE_P(SSSE3, MaskedSADx4Test,
                         ::testing::ValuesIn(msadx4_test));

#if CONFIG_AV1_HIGHBITDEPTH
const HighbdMaskedSADParam hbd_msad_test[] = {
  make_tuple(&aom_highbd_masked_sad4x4_ssse3, &aom_highbd_masked_sad4x4_c),
  make_tuple(&aom_highbd_masked_sad4x8_ssse3, &aom_highbd_masked_sad4x8_c),
  make_tuple(&aom_highbd_masked_sad8x4_ssse3, &aom_highbd_masked_sad8x4_c),
  make_tuple(&aom_highbd_masked_sad8x8_ssse3, &aom_highbd_masked_sad8x8_c),
  make_tuple(&aom_highbd_masked_sad8x16_ssse3, &aom_highbd_masked_sad8x16_c),
  make_tuple(&aom_highbd_masked_sad16x8_ssse3, &aom_highbd_masked_sad16x8_c),
  make_tuple(&aom_highbd_masked_sad16x16_ssse3, &aom_highbd_masked_sad16x16_c),
  make_tuple(&aom_highbd_masked_sad16x32_ssse3, &aom_highbd_masked_sad16x32_c),
  make_tuple(&aom_highbd_masked_sad32x16_ssse3, &aom_highbd_masked_sad32x16_c),
  make_tuple(&aom_highbd_masked_sad32x32_ssse3, &aom_highbd_masked_sad32x32_c),
  make_tuple(&aom_highbd_masked_sad32x64_ssse3, &aom_highbd_masked_sad32x64_c),
  make_tuple(&aom_highbd_masked_sad64x32_ssse3, &aom_highbd_masked_sad64x32_c),
  make_tuple(&aom_highbd_masked_sad64x64_ssse3, &aom_highbd_masked_sad64x64_c),
  make_tuple(&aom_highbd_masked_sad64x128_ssse3,
             &aom_highbd_masked_sad64x128_c),
  make_tuple(&aom_highbd_masked_sad128x64_ssse3,
             &aom_highbd_masked_sad128x64_c),
  make_tuple(&aom_highbd_masked_sad128x128_ssse3,
             &aom_highbd_masked_sad128x128_c),
#if !CONFIG_REALTIME_ONLY
  make_tuple(&aom_highbd_masked_sad4x16_ssse3, &aom_highbd_masked_sad4x16_c),
  make_tuple(&aom_highbd_masked_sad16x4_ssse3, &aom_highbd_masked_sad16x4_c),
  make_tuple(&aom_highbd_masked_sad8x32_ssse3, &aom_highbd_masked_sad8x32_c),
  make_tuple(&aom_highbd_masked_sad32x8_ssse3, &aom_highbd_masked_sad32x8_c),
  make_tuple(&aom_highbd_masked_sad16x64_ssse3, &aom_highbd_masked_sad16x64_c),
  make_tuple(&aom_highbd_masked_sad64x16_ssse3, &aom_highbd_masked_sad64x16_c),
#endif
};

INSTANTIATE_TEST_SUITE_P(SSSE3, HighbdMaskedSADTest,
                         ::testing::ValuesIn(hbd_msad_test));
#endif  // CONFIG_AV1_HIGHBITDEPTH
#endif  // HAVE_SSSE3

#if HAVE_AVX2
const MaskedSADParam msad_avx2_test[] = {
  make_tuple(&aom_masked_sad4x4_avx2, &aom_masked_sad4x4_ssse3),
  make_tuple(&aom_masked_sad4x8_avx2, &aom_masked_sad4x8_ssse3),
  make_tuple(&aom_masked_sad8x4_avx2, &aom_masked_sad8x4_ssse3),
  make_tuple(&aom_masked_sad8x8_avx2, &aom_masked_sad8x8_ssse3),
  make_tuple(&aom_masked_sad8x16_avx2, &aom_masked_sad8x16_ssse3),
  make_tuple(&aom_masked_sad16x8_avx2, &aom_masked_sad16x8_ssse3),
  make_tuple(&aom_masked_sad16x16_avx2, &aom_masked_sad16x16_ssse3),
  make_tuple(&aom_masked_sad16x32_avx2, &aom_masked_sad16x32_ssse3),
  make_tuple(&aom_masked_sad32x16_avx2, &aom_masked_sad32x16_ssse3),
  make_tuple(&aom_masked_sad32x32_avx2, &aom_masked_sad32x32_ssse3),
  make_tuple(&aom_masked_sad32x64_avx2, &aom_masked_sad32x64_ssse3),
  make_tuple(&aom_masked_sad64x32_avx2, &aom_masked_sad64x32_ssse3),
  make_tuple(&aom_masked_sad64x64_avx2, &aom_masked_sad64x64_ssse3),
  make_tuple(&aom_masked_sad64x128_avx2, &aom_masked_sad64x128_ssse3),
  make_tuple(&aom_masked_sad128x64_avx2, &aom_masked_sad128x64_ssse3),
  make_tuple(&aom_masked_sad128x128_avx2, &aom_masked_sad128x128_ssse3),
#if !CONFIG_REALTIME_ONLY
  make_tuple(&aom_masked_sad4x16_avx2, &aom_masked_sad4x16_ssse3),
  make_tuple(&aom_masked_sad16x4_avx2, &aom_masked_sad16x4_ssse3),
  make_tuple(&aom_masked_sad8x32_avx2, &aom_masked_sad8x32_ssse3),
  make_tuple(&aom_masked_sad32x8_avx2, &aom_masked_sad32x8_ssse3),
  make_tuple(&aom_masked_sad16x64_avx2, &aom_masked_sad16x64_ssse3),
  make_tuple(&aom_masked_sad64x16_avx2, &aom_masked_sad64x16_ssse3)
#endif
};

INSTANTIATE_TEST_SUITE_P(AVX2, MaskedSADTest,
                         ::testing::ValuesIn(msad_avx2_test));

#if CONFIG_AV1_HIGHBITDEPTH
const HighbdMaskedSADParam hbd_msad_avx2_test[] = {
  make_tuple(&aom_highbd_masked_sad4x4_avx2, &aom_highbd_masked_sad4x4_ssse3),
  make_tuple(&aom_highbd_masked_sad4x8_avx2, &aom_highbd_masked_sad4x8_ssse3),
  make_tuple(&aom_highbd_masked_sad8x4_avx2, &aom_highbd_masked_sad8x4_ssse3),
  make_tuple(&aom_highbd_masked_sad8x8_avx2, &aom_highbd_masked_sad8x8_ssse3),
  make_tuple(&aom_highbd_masked_sad8x16_avx2, &aom_highbd_masked_sad8x16_ssse3),
  make_tuple(&aom_highbd_masked_sad16x8_avx2, &aom_highbd_masked_sad16x8_ssse3),
  make_tuple(&aom_highbd_masked_sad16x16_avx2,
             &aom_highbd_masked_sad16x16_ssse3),
  make_tuple(&aom_highbd_masked_sad16x32_avx2,
             &aom_highbd_masked_sad16x32_ssse3),
  make_tuple(&aom_highbd_masked_sad32x16_avx2,
             &aom_highbd_masked_sad32x16_ssse3),
  make_tuple(&aom_highbd_masked_sad32x32_avx2,
             &aom_highbd_masked_sad32x32_ssse3),
  make_tuple(&aom_highbd_masked_sad32x64_avx2,
             &aom_highbd_masked_sad32x64_ssse3),
  make_tuple(&aom_highbd_masked_sad64x32_avx2,
             &aom_highbd_masked_sad64x32_ssse3),
  make_tuple(&aom_highbd_masked_sad64x64_avx2,
             &aom_highbd_masked_sad64x64_ssse3),
  make_tuple(&aom_highbd_masked_sad64x128_avx2,
             &aom_highbd_masked_sad64x128_ssse3),
  make_tuple(&aom_highbd_masked_sad128x64_avx2,
             &aom_highbd_masked_sad128x64_ssse3),
  make_tuple(&aom_highbd_masked_sad128x128_avx2,
             &aom_highbd_masked_sad128x128_ssse3),
#if !CONFIG_REALTIME_ONLY
  make_tuple(&aom_highbd_masked_sad4x16_avx2, &aom_highbd_masked_sad4x16_ssse3),
  make_tuple(&aom_highbd_masked_sad16x4_avx2, &aom_highbd_masked_sad16x4_ssse3),
  make_tuple(&aom_highbd_masked_sad8x32_avx2, &aom_highbd_masked_sad8x32_ssse3),
  make_tuple(&aom_highbd_masked_sad32x8_avx2, &aom_highbd_masked_sad32x8_ssse3),
  make_tuple(&aom_highbd_masked_sad16x64_avx2,
             &aom_highbd_masked_sad16x64_ssse3),
  make_tuple(&aom_highbd_masked_sad64x16_avx2,
             &aom_highbd_masked_sad64x16_ssse3)
#endif
};

INSTANTIATE_TEST_SUITE_P(AVX2, HighbdMaskedSADTest,
                         ::testing::ValuesIn(hbd_msad_avx2_test));
#endif  // CONFIG_AV1_HIGHBITDEPTH
#endif  // HAVE_AVX2

#if HAVE_NEON
const MaskedSADParam msad_test[] = {
  make_tuple(&aom_masked_sad4x4_neon, &aom_masked_sad4x4_c),
  make_tuple(&aom_masked_sad4x8_neon, &aom_masked_sad4x8_c),
  make_tuple(&aom_masked_sad8x4_neon, &aom_masked_sad8x4_c),
  make_tuple(&aom_masked_sad8x8_neon, &aom_masked_sad8x8_c),
  make_tuple(&aom_masked_sad8x16_neon, &aom_masked_sad8x16_c),
  make_tuple(&aom_masked_sad16x8_neon, &aom_masked_sad16x8_c),
  make_tuple(&aom_masked_sad16x16_neon, &aom_masked_sad16x16_c),
  make_tuple(&aom_masked_sad16x32_neon, &aom_masked_sad16x32_c),
  make_tuple(&aom_masked_sad32x16_neon, &aom_masked_sad32x16_c),
  make_tuple(&aom_masked_sad32x32_neon, &aom_masked_sad32x32_c),
  make_tuple(&aom_masked_sad32x64_neon, &aom_masked_sad32x64_c),
  make_tuple(&aom_masked_sad64x32_neon, &aom_masked_sad64x32_c),
  make_tuple(&aom_masked_sad64x64_neon, &aom_masked_sad64x64_c),
  make_tuple(&aom_masked_sad64x128_neon, &aom_masked_sad64x128_c),
  make_tuple(&aom_masked_sad128x64_neon, &aom_masked_sad128x64_c),
  make_tuple(&aom_masked_sad128x128_neon, &aom_masked_sad128x128_c),
#if !CONFIG_REALTIME_ONLY
  make_tuple(&aom_masked_sad4x16_neon, &aom_masked_sad4x16_c),
  make_tuple(&aom_masked_sad16x4_neon, &aom_masked_sad16x4_c),
  make_tuple(&aom_masked_sad8x32_neon, &aom_masked_sad8x32_c),
  make_tuple(&aom_masked_sad32x8_neon, &aom_masked_sad32x8_c),
  make_tuple(&aom_masked_sad16x64_neon, &aom_masked_sad16x64_c),
  make_tuple(&aom_masked_sad64x16_neon, &aom_masked_sad64x16_c),
#endif
};

INSTANTIATE_TEST_SUITE_P(NEON, MaskedSADTest, ::testing::ValuesIn(msad_test));

const MaskedSADx4Param msadx4_test[] = {
  make_tuple(&aom_masked_sad4x4x4d_neon, &aom_masked_sad4x4x4d_c),
  make_tuple(&aom_masked_sad4x8x4d_neon, &aom_masked_sad4x8x4d_c),
  make_tuple(&aom_masked_sad8x4x4d_neon, &aom_masked_sad8x4x4d_c),
  make_tuple(&aom_masked_sad8x8x4d_neon, &aom_masked_sad8x8x4d_c),
  make_tuple(&aom_masked_sad8x16x4d_neon, &aom_masked_sad8x16x4d_c),
  make_tuple(&aom_masked_sad16x8x4d_neon, &aom_masked_sad16x8x4d_c),
  make_tuple(&aom_masked_sad16x16x4d_neon, &aom_masked_sad16x16x4d_c),
  make_tuple(&aom_masked_sad16x32x4d_neon, &aom_masked_sad16x32x4d_c),
  make_tuple(&aom_masked_sad32x16x4d_neon, &aom_masked_sad32x16x4d_c),
  make_tuple(&aom_masked_sad32x32x4d_neon, &aom_masked_sad32x32x4d_c),
  make_tuple(&aom_masked_sad32x64x4d_neon, &aom_masked_sad32x64x4d_c),
  make_tuple(&aom_masked_sad64x32x4d_neon, &aom_masked_sad64x32x4d_c),
  make_tuple(&aom_masked_sad64x64x4d_neon, &aom_masked_sad64x64x4d_c),
  make_tuple(&aom_masked_sad64x128x4d_neon, &aom_masked_sad64x128x4d_c),
  make_tuple(&aom_masked_sad128x64x4d_neon, &aom_masked_sad128x64x4d_c),
  make_tuple(&aom_masked_sad128x128x4d_neon, &aom_masked_sad128x128x4d_c),
#if !CONFIG_REALTIME_ONLY
  make_tuple(&aom_masked_sad4x16x4d_neon, &aom_masked_sad4x16x4d_c),
  make_tuple(&aom_masked_sad16x4x4d_neon, &aom_masked_sad16x4x4d_c),
  make_tuple(&aom_masked_sad8x32x4d_neon, &aom_masked_sad8x32x4d_c),
  make_tuple(&aom_masked_sad32x8x4d_neon, &aom_masked_sad32x8x4d_c),
  make_tuple(&aom_masked_sad16x64x4d_neon, &aom_masked_sad16x64x4d_c),
  make_tuple(&aom_masked_sad64x16x4d_neon, &aom_masked_sad64x16x4d_c),
#endif
};

INSTANTIATE_TEST_SUITE_P(NEON, MaskedSADx4Test,
                         ::testing::ValuesIn(msadx4_test));

#if CONFIG_AV1_HIGHBITDEPTH
const MaskedSADParam hbd_msad_neon_test[] = {
  make_tuple(&aom_highbd_masked_sad4x4_neon, &aom_highbd_masked_sad4x4_c),
  make_tuple(&aom_highbd_masked_sad4x8_neon, &aom_highbd_masked_sad4x8_c),
  make_tuple(&aom_highbd_masked_sad8x4_neon, &aom_highbd_masked_sad8x4_c),
  make_tuple(&aom_highbd_masked_sad8x8_neon, &aom_highbd_masked_sad8x8_c),
  make_tuple(&aom_highbd_masked_sad8x16_neon, &aom_highbd_masked_sad8x16_c),
  make_tuple(&aom_highbd_masked_sad16x8_neon, &aom_highbd_masked_sad16x8_c),
  make_tuple(&aom_highbd_masked_sad16x16_neon, &aom_highbd_masked_sad16x16_c),
  make_tuple(&aom_highbd_masked_sad16x32_neon, &aom_highbd_masked_sad16x32_c),
  make_tuple(&aom_highbd_masked_sad32x16_neon, &aom_highbd_masked_sad32x16_c),
  make_tuple(&aom_highbd_masked_sad32x32_neon, &aom_highbd_masked_sad32x32_c),
  make_tuple(&aom_highbd_masked_sad32x64_neon, &aom_highbd_masked_sad32x64_c),
  make_tuple(&aom_highbd_masked_sad64x32_neon, &aom_highbd_masked_sad64x32_c),
  make_tuple(&aom_highbd_masked_sad64x64_neon, &aom_highbd_masked_sad64x64_c),
  make_tuple(&aom_highbd_masked_sad64x128_neon, &aom_highbd_masked_sad64x128_c),
  make_tuple(&aom_highbd_masked_sad128x64_neon, &aom_highbd_masked_sad128x64_c),
  make_tuple(&aom_highbd_masked_sad128x128_neon,
             &aom_highbd_masked_sad128x128_c),
#if !CONFIG_REALTIME_ONLY
  make_tuple(&aom_highbd_masked_sad4x16_neon, &aom_highbd_masked_sad4x16_c),
  make_tuple(&aom_highbd_masked_sad16x4_neon, &aom_highbd_masked_sad16x4_c),
  make_tuple(&aom_highbd_masked_sad8x32_neon, &aom_highbd_masked_sad8x32_c),
  make_tuple(&aom_highbd_masked_sad32x8_neon, &aom_highbd_masked_sad32x8_c),
  make_tuple(&aom_highbd_masked_sad16x64_neon, &aom_highbd_masked_sad16x64_c),
  make_tuple(&aom_highbd_masked_sad64x16_neon, &aom_highbd_masked_sad64x16_c),
#endif  // !CONFIG_REALTIME_ONLY
};

INSTANTIATE_TEST_SUITE_P(NEON, HighbdMaskedSADTest,
                         ::testing::ValuesIn(hbd_msad_neon_test));

#endif  // CONFIG_AV1_HIGHBITDEPTH

#endif  // HAVE_NEON

}  // namespace
