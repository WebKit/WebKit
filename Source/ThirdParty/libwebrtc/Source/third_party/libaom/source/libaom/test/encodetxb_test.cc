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

#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

#include "config/aom_config.h"
#include "config/av1_rtcd.h"

#include "aom_ports/aom_timer.h"
#include "aom_ports/mem.h"
#include "av1/common/av1_common_int.h"
#include "av1/common/idct.h"
#include "av1/common/scan.h"
#include "av1/common/txb_common.h"
#include "test/acm_random.h"
#include "test/register_state_check.h"
#include "test/util.h"

namespace {
using libaom_test::ACMRandom;

typedef void (*GetNzMapContextsFunc)(const uint8_t *const levels,
                                     const int16_t *const scan,
                                     const uint16_t eob, const TX_SIZE tx_size,
                                     const TX_CLASS tx_class,
                                     int8_t *const coeff_contexts);

class EncodeTxbTest : public ::testing::TestWithParam<GetNzMapContextsFunc> {
 public:
  EncodeTxbTest() : get_nz_map_contexts_func_(GetParam()) {}

  virtual ~EncodeTxbTest() {}

  virtual void SetUp() {
    coeff_contexts_ref_ = reinterpret_cast<int8_t *>(
        aom_memalign(16, sizeof(*coeff_contexts_ref_) * MAX_TX_SQUARE));
    ASSERT_NE(coeff_contexts_ref_, nullptr);
    coeff_contexts_ = reinterpret_cast<int8_t *>(
        aom_memalign(16, sizeof(*coeff_contexts_) * MAX_TX_SQUARE));
    ASSERT_NE(coeff_contexts_, nullptr);
  }

  virtual void TearDown() {
    aom_free(coeff_contexts_ref_);
    aom_free(coeff_contexts_);
  }

  void GetNzMapContextsRun() {
    const int kNumTests = 10;
    int result = 0;

    for (int is_inter = 0; is_inter < 2; ++is_inter) {
      for (int tx_type = DCT_DCT; tx_type < TX_TYPES; ++tx_type) {
        const TX_CLASS tx_class = tx_type_to_class[tx_type];
        for (int tx_size = TX_4X4; tx_size < TX_SIZES_ALL; ++tx_size) {
          const int bhl = get_txb_bhl((TX_SIZE)tx_size);
          const int width = get_txb_wide((TX_SIZE)tx_size);
          const int height = get_txb_high((TX_SIZE)tx_size);
          const int real_width = tx_size_wide[tx_size];
          const int real_height = tx_size_high[tx_size];
          const int16_t *const scan = av1_scan_orders[tx_size][tx_type].scan;

          levels_ = set_levels(levels_buf_, height);
          for (int i = 0; i < kNumTests && !result; ++i) {
            for (int eob = 1; eob <= width * height && !result; ++eob) {
              InitDataWithEob(scan, bhl, eob);

              av1_get_nz_map_contexts_c(levels_, scan, eob, (TX_SIZE)tx_size,
                                        tx_class, coeff_contexts_ref_);
              get_nz_map_contexts_func_(levels_, scan, eob, (TX_SIZE)tx_size,
                                        tx_class, coeff_contexts_);

              result = Compare(scan, eob);

              EXPECT_EQ(result, 0)
                  << " tx_class " << (int)tx_class << " width " << real_width
                  << " height " << real_height << " eob " << eob;
            }
          }
        }
      }
    }
  }

  void SpeedTestGetNzMapContextsRun() {
    const int kNumTests = 2000000000;
    aom_usec_timer timer;
    aom_usec_timer timer_ref;

    printf("Note: Only test the largest possible eob case!\n");
    for (int tx_size = TX_4X4; tx_size < TX_SIZES_ALL; ++tx_size) {
      const int bhl = get_txb_bhl((TX_SIZE)tx_size);
      const int width = get_txb_wide((TX_SIZE)tx_size);
      const int height = get_txb_high((TX_SIZE)tx_size);
      const int real_width = tx_size_wide[tx_size];
      const int real_height = tx_size_high[tx_size];
      const TX_TYPE tx_type = DCT_DCT;
      const TX_CLASS tx_class = tx_type_to_class[tx_type];
      const int16_t *const scan = av1_scan_orders[tx_size][tx_type].scan;
      const int eob = width * height;
      const int numTests = kNumTests / (width * height);

      levels_ = set_levels(levels_buf_, height);
      InitDataWithEob(scan, bhl, eob);

      aom_usec_timer_start(&timer_ref);
      for (int i = 0; i < numTests; ++i) {
        av1_get_nz_map_contexts_c(levels_, scan, eob, (TX_SIZE)tx_size,
                                  tx_class, coeff_contexts_ref_);
      }
      aom_usec_timer_mark(&timer_ref);

      levels_ = set_levels(levels_buf_, height);
      InitDataWithEob(scan, bhl, eob);

      aom_usec_timer_start(&timer);
      for (int i = 0; i < numTests; ++i) {
        get_nz_map_contexts_func_(levels_, scan, eob, (TX_SIZE)tx_size,
                                  tx_class, coeff_contexts_);
      }
      aom_usec_timer_mark(&timer);

      const int elapsed_time_ref =
          static_cast<int>(aom_usec_timer_elapsed(&timer_ref));
      const int elapsed_time = static_cast<int>(aom_usec_timer_elapsed(&timer));

      printf("get_nz_map_contexts_%2dx%2d: %7.1f ms ref %7.1f ms gain %4.2f\n",
             real_width, real_height, elapsed_time / 1000.0,
             elapsed_time_ref / 1000.0,
             (elapsed_time_ref * 1.0) / (elapsed_time * 1.0));
    }
  }

 private:
  void InitDataWithEob(const int16_t *const scan, const int bhl,
                       const int eob) {
    memset(levels_buf_, 0, sizeof(levels_buf_));
    memset(coeff_contexts_, 0, sizeof(*coeff_contexts_) * MAX_TX_SQUARE);

    for (int c = 0; c < eob; ++c) {
      levels_[get_padded_idx(scan[c], bhl)] =
          static_cast<uint8_t>(clamp(rnd_.Rand8(), 0, INT8_MAX));
      coeff_contexts_[scan[c]] = static_cast<int8_t>(rnd_.Rand16() >> 1);
    }

    memcpy(coeff_contexts_ref_, coeff_contexts_,
           sizeof(*coeff_contexts_) * MAX_TX_SQUARE);
  }

  bool Compare(const int16_t *const scan, const int eob) const {
    bool result = false;
    if (memcmp(coeff_contexts_, coeff_contexts_ref_,
               sizeof(*coeff_contexts_ref_) * MAX_TX_SQUARE)) {
      for (int i = 0; i < eob; i++) {
        const int pos = scan[i];
        if (coeff_contexts_ref_[pos] != coeff_contexts_[pos]) {
          printf("coeff_contexts_[%d] diff:%6d (ref),%6d (opt)\n", pos,
                 coeff_contexts_ref_[pos], coeff_contexts_[pos]);
          result = true;
          break;
        }
      }
    }
    return result;
  }

  GetNzMapContextsFunc get_nz_map_contexts_func_;
  ACMRandom rnd_;
  uint8_t levels_buf_[TX_PAD_2D];
  uint8_t *levels_;
  int8_t *coeff_contexts_ref_;
  int8_t *coeff_contexts_;
};
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(EncodeTxbTest);

TEST_P(EncodeTxbTest, GetNzMapContexts) { GetNzMapContextsRun(); }

TEST_P(EncodeTxbTest, DISABLED_SpeedTestGetNzMapContexts) {
  SpeedTestGetNzMapContextsRun();
}

#if HAVE_SSE2
INSTANTIATE_TEST_SUITE_P(SSE2, EncodeTxbTest,
                         ::testing::Values(av1_get_nz_map_contexts_sse2));
#endif

#if HAVE_NEON
INSTANTIATE_TEST_SUITE_P(NEON, EncodeTxbTest,
                         ::testing::Values(av1_get_nz_map_contexts_neon));
#endif

typedef void (*av1_txb_init_levels_func)(const tran_low_t *const coeff,
                                         const int width, const int height,
                                         uint8_t *const levels);

typedef std::tuple<av1_txb_init_levels_func, int> TxbInitLevelParam;

class EncodeTxbInitLevelTest
    : public ::testing::TestWithParam<TxbInitLevelParam> {
 public:
  virtual ~EncodeTxbInitLevelTest() {}
  virtual void TearDown() {}
  void RunTest(av1_txb_init_levels_func test_func, int tx_size, int is_speed);
};
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(EncodeTxbInitLevelTest);

void EncodeTxbInitLevelTest::RunTest(av1_txb_init_levels_func test_func,
                                     int tx_size, int is_speed) {
  const int width = get_txb_wide((TX_SIZE)tx_size);
  const int height = get_txb_high((TX_SIZE)tx_size);
  tran_low_t coeff[MAX_TX_SQUARE];

  uint8_t levels_buf[2][TX_PAD_2D];
  uint8_t *const levels0 = set_levels(levels_buf[0], height);
  uint8_t *const levels1 = set_levels(levels_buf[1], height);

  ACMRandom rnd(ACMRandom::DeterministicSeed());
  for (int i = 0; i < width * height; i++) {
    coeff[i] = rnd.Rand16Signed();
  }
  for (int i = 0; i < TX_PAD_2D; i++) {
    levels_buf[0][i] = rnd.Rand8();
    levels_buf[1][i] = rnd.Rand8();
  }
  const int run_times = is_speed ? (width * height) * 10000 : 1;
  aom_usec_timer timer;
  aom_usec_timer_start(&timer);
  for (int i = 0; i < run_times; ++i) {
    av1_txb_init_levels_c(coeff, width, height, levels0);
  }
  const double t1 = get_time_mark(&timer);
  aom_usec_timer_start(&timer);
  for (int i = 0; i < run_times; ++i) {
    test_func(coeff, width, height, levels1);
  }
  const double t2 = get_time_mark(&timer);
  if (is_speed) {
    printf("init %3dx%-3d:%7.2f/%7.2fns", width, height, t1, t2);
    printf("(%3.2f)\n", t1 / t2);
  }
  const int stride = width + TX_PAD_HOR;
  for (int r = 0; r < height + TX_PAD_VER; ++r) {
    for (int c = 0; c < stride; ++c) {
      ASSERT_EQ(levels_buf[0][c + r * stride], levels_buf[1][c + r * stride])
          << "[" << r << "," << c << "] " << run_times << width << "x"
          << height;
    }
  }
}

TEST_P(EncodeTxbInitLevelTest, match) {
  RunTest(GET_PARAM(0), GET_PARAM(1), 0);
}

TEST_P(EncodeTxbInitLevelTest, DISABLED_Speed) {
  RunTest(GET_PARAM(0), GET_PARAM(1), 1);
}

#if HAVE_SSE4_1
INSTANTIATE_TEST_SUITE_P(
    SSE4_1, EncodeTxbInitLevelTest,
    ::testing::Combine(::testing::Values(&av1_txb_init_levels_sse4_1),
                       ::testing::Range(0, static_cast<int>(TX_SIZES_ALL), 1)));
#endif
#if HAVE_AVX2
INSTANTIATE_TEST_SUITE_P(
    AVX2, EncodeTxbInitLevelTest,
    ::testing::Combine(::testing::Values(&av1_txb_init_levels_avx2),
                       ::testing::Range(0, static_cast<int>(TX_SIZES_ALL), 1)));
#endif
#if HAVE_NEON
INSTANTIATE_TEST_SUITE_P(
    NEON, EncodeTxbInitLevelTest,
    ::testing::Combine(::testing::Values(&av1_txb_init_levels_neon),
                       ::testing::Range(0, static_cast<int>(TX_SIZES_ALL), 1)));
#endif
}  // namespace
