/*
 *  Copyright (c) 2023 The WebM project authors. All Rights Reserved.
 *  Copyright (c) 2023, Alliance for Open Media. All Rights Reserved.
 *
 *  This source code is subject to the terms of the BSD 2 Clause License and
 *  the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 *  was not distributed with this source code in the LICENSE file, you can
 *  obtain it at www.aomedia.org/license/software. If the Alliance for Open
 *  Media Patent License 1.0 was not distributed with this source code in the
 *  PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <stdlib.h>
#include <string.h>

#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"
#include "aom_ports/mem.h"
#include "test/acm_random.h"
#include "test/register_state_check.h"
#include "test/util.h"

namespace {

using ::libaom_test::ACMRandom;

typedef void (*MinMaxFunc)(const uint8_t *a, int a_stride, const uint8_t *b,
                           int b_stride, int *min, int *max);

class MinMaxTest : public ::testing::TestWithParam<MinMaxFunc> {
 public:
  void SetUp() override {
    mm_func_ = GetParam();
    rnd_.Reset(ACMRandom::DeterministicSeed());
  }

 protected:
  MinMaxFunc mm_func_;
  ACMRandom rnd_;
};

void reference_minmax(const uint8_t *a, int a_stride, const uint8_t *b,
                      int b_stride, int *min_ret, int *max_ret) {
  int min = 255;
  int max = 0;
  for (int i = 0; i < 8; i++) {
    for (int j = 0; j < 8; j++) {
      const int diff = abs(a[i * a_stride + j] - b[i * b_stride + j]);
      if (min > diff) min = diff;
      if (max < diff) max = diff;
    }
  }

  *min_ret = min;
  *max_ret = max;
}

TEST_P(MinMaxTest, MinValue) {
  for (int i = 0; i < 64; i++) {
    uint8_t a[64], b[64];
    memset(a, 0, sizeof(a));
    memset(b, 255, sizeof(b));
    b[i] = i;  // Set a minimum difference of i.

    int min, max;
    API_REGISTER_STATE_CHECK(mm_func_(a, 8, b, 8, &min, &max));
    EXPECT_EQ(255, max);
    EXPECT_EQ(i, min);
  }
}

TEST_P(MinMaxTest, MaxValue) {
  for (int i = 0; i < 64; i++) {
    uint8_t a[64], b[64];
    memset(a, 0, sizeof(a));
    memset(b, 0, sizeof(b));
    b[i] = i;  // Set a maximum difference of i.

    int min, max;
    API_REGISTER_STATE_CHECK(mm_func_(a, 8, b, 8, &min, &max));
    EXPECT_EQ(i, max);
    EXPECT_EQ(0, min);
  }
}

TEST_P(MinMaxTest, CompareReference) {
  uint8_t a[64], b[64];
  for (int j = 0; j < 64; j++) {
    a[j] = rnd_.Rand8();
    b[j] = rnd_.Rand8();
  }

  int min_ref, max_ref, min, max;
  reference_minmax(a, 8, b, 8, &min_ref, &max_ref);
  API_REGISTER_STATE_CHECK(mm_func_(a, 8, b, 8, &min, &max));
  EXPECT_EQ(max_ref, max);
  EXPECT_EQ(min_ref, min);
}

TEST_P(MinMaxTest, CompareReferenceAndVaryStride) {
  uint8_t a[8 * 64], b[8 * 64];
  for (int i = 0; i < 8 * 64; i++) {
    a[i] = rnd_.Rand8();
    b[i] = rnd_.Rand8();
  }
  for (int a_stride = 8; a_stride <= 64; a_stride += 8) {
    for (int b_stride = 8; b_stride <= 64; b_stride += 8) {
      int min_ref, max_ref, min, max;
      reference_minmax(a, a_stride, b, b_stride, &min_ref, &max_ref);
      API_REGISTER_STATE_CHECK(mm_func_(a, a_stride, b, b_stride, &min, &max));
      EXPECT_EQ(max_ref, max)
          << "when a_stride = " << a_stride << " and b_stride = " << b_stride;
      EXPECT_EQ(min_ref, min)
          << "when a_stride = " << a_stride << " and b_stride = " << b_stride;
    }
  }
}

#if CONFIG_AV1_HIGHBITDEPTH

using HBDMinMaxTest = MinMaxTest;

void highbd_reference_minmax(const uint8_t *a, int a_stride, const uint8_t *b,
                             int b_stride, int *min_ret, int *max_ret) {
  int min = 65535;
  int max = 0;
  const uint16_t *a_ptr = CONVERT_TO_SHORTPTR(a);
  const uint16_t *b_ptr = CONVERT_TO_SHORTPTR(b);
  for (int i = 0; i < 8; i++) {
    for (int j = 0; j < 8; j++) {
      const int diff = abs(a_ptr[i * a_stride + j] - b_ptr[i * b_stride + j]);
      if (min > diff) min = diff;
      if (max < diff) max = diff;
    }
  }

  *min_ret = min;
  *max_ret = max;
}

TEST_P(HBDMinMaxTest, MinValue) {
  uint8_t *a = CONVERT_TO_BYTEPTR(
      reinterpret_cast<uint16_t *>(aom_malloc(64 * sizeof(uint16_t))));
  uint8_t *b = CONVERT_TO_BYTEPTR(
      reinterpret_cast<uint16_t *>(aom_malloc(64 * sizeof(uint16_t))));
  for (int i = 0; i < 64; i++) {
    aom_memset16(CONVERT_TO_SHORTPTR(a), 0, 64);
    aom_memset16(CONVERT_TO_SHORTPTR(b), 65535, 64);
    CONVERT_TO_SHORTPTR(b)[i] = i;  // Set a minimum difference of i.

    int min, max;
    API_REGISTER_STATE_CHECK(mm_func_(a, 8, b, 8, &min, &max));
    EXPECT_EQ(65535, max);
    EXPECT_EQ(i, min);
  }
  aom_free(CONVERT_TO_SHORTPTR(a));
  aom_free(CONVERT_TO_SHORTPTR(b));
}

TEST_P(HBDMinMaxTest, MaxValue) {
  uint8_t *a = CONVERT_TO_BYTEPTR(
      reinterpret_cast<uint16_t *>(aom_malloc(64 * sizeof(uint16_t))));
  uint8_t *b = CONVERT_TO_BYTEPTR(
      reinterpret_cast<uint16_t *>(aom_malloc(64 * sizeof(uint16_t))));
  for (int i = 0; i < 64; i++) {
    aom_memset16(CONVERT_TO_SHORTPTR(a), 0, 64);
    aom_memset16(CONVERT_TO_SHORTPTR(b), 0, 64);
    CONVERT_TO_SHORTPTR(b)[i] = i;  // Set a minimum difference of i.

    int min, max;
    API_REGISTER_STATE_CHECK(mm_func_(a, 8, b, 8, &min, &max));
    EXPECT_EQ(i, max);
    EXPECT_EQ(0, min);
  }
  aom_free(CONVERT_TO_SHORTPTR(a));
  aom_free(CONVERT_TO_SHORTPTR(b));
}

TEST_P(HBDMinMaxTest, CompareReference) {
  uint8_t *a = CONVERT_TO_BYTEPTR(
      reinterpret_cast<uint16_t *>(aom_malloc(64 * sizeof(uint16_t))));
  uint8_t *b = CONVERT_TO_BYTEPTR(
      reinterpret_cast<uint16_t *>(aom_malloc(64 * sizeof(uint16_t))));
  for (int j = 0; j < 64; j++) {
    CONVERT_TO_SHORTPTR(a)[j] = rnd_.Rand16();
    CONVERT_TO_SHORTPTR(b)[j] = rnd_.Rand16();
  }

  int min_ref, max_ref, min, max;
  highbd_reference_minmax(a, 8, b, 8, &min_ref, &max_ref);
  API_REGISTER_STATE_CHECK(mm_func_(a, 8, b, 8, &min, &max));
  aom_free(CONVERT_TO_SHORTPTR(a));
  aom_free(CONVERT_TO_SHORTPTR(b));
  EXPECT_EQ(max_ref, max);
  EXPECT_EQ(min_ref, min);
}

TEST_P(HBDMinMaxTest, CompareReferenceAndVaryStride) {
  uint8_t *a = CONVERT_TO_BYTEPTR(
      reinterpret_cast<uint16_t *>(aom_malloc((8 * 64) * sizeof(uint16_t))));
  uint8_t *b = CONVERT_TO_BYTEPTR(
      reinterpret_cast<uint16_t *>(aom_malloc((8 * 64) * sizeof(uint16_t))));
  for (int i = 0; i < 8 * 64; i++) {
    CONVERT_TO_SHORTPTR(a)[i] = rnd_.Rand16();
    CONVERT_TO_SHORTPTR(b)[i] = rnd_.Rand16();
  }
  for (int a_stride = 8; a_stride <= 64; a_stride += 8) {
    for (int b_stride = 8; b_stride <= 64; b_stride += 8) {
      int min_ref, max_ref, min, max;
      highbd_reference_minmax(a, a_stride, b, b_stride, &min_ref, &max_ref);
      API_REGISTER_STATE_CHECK(mm_func_(a, a_stride, b, b_stride, &min, &max));
      EXPECT_EQ(max_ref, max)
          << "when a_stride = " << a_stride << " and b_stride = " << b_stride;
      EXPECT_EQ(min_ref, min)
          << "when a_stride = " << a_stride << " and b_stride = " << b_stride;
    }
  }
  aom_free(CONVERT_TO_SHORTPTR(a));
  aom_free(CONVERT_TO_SHORTPTR(b));
}
#endif  // CONFIG_AV1_HIGHBITDEPTH

INSTANTIATE_TEST_SUITE_P(C, MinMaxTest, ::testing::Values(&aom_minmax_8x8_c));
#if CONFIG_AV1_HIGHBITDEPTH
INSTANTIATE_TEST_SUITE_P(C, HBDMinMaxTest,
                         ::testing::Values(&aom_highbd_minmax_8x8_c));
#if HAVE_NEON
INSTANTIATE_TEST_SUITE_P(NEON, HBDMinMaxTest,
                         ::testing::Values(&aom_highbd_minmax_8x8_neon));
#endif
#endif

#if HAVE_SSE2
INSTANTIATE_TEST_SUITE_P(SSE2, MinMaxTest,
                         ::testing::Values(&aom_minmax_8x8_sse2));
#endif

#if HAVE_NEON
INSTANTIATE_TEST_SUITE_P(NEON, MinMaxTest,
                         ::testing::Values(&aom_minmax_8x8_neon));
#endif
}  // namespace
