/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <cmath>
#include <cstdlib>
#include <string>
#include <tuple>

#include "gtest/gtest.h"

#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"

#include "test/acm_random.h"
#include "test/register_state_check.h"
#include "test/util.h"
#include "av1/common/av1_loopfilter.h"
#include "av1/common/entropy.h"
#include "aom/aom_integer.h"

using libaom_test::ACMRandom;

namespace {
// Horizontally and Vertically need 32x32: 8  Coeffs preceeding filtered section
//                                         16 Coefs within filtered section
//                                         8  Coeffs following filtered section
const int kNumCoeffs = 1024;

const int number_of_iterations = 10000;

const int kSpeedTestNum = 500000;

#define LOOP_PARAM \
  int p, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh
#define DUAL_LOOP_PARAM                                                      \
  int p, const uint8_t *blimit0, const uint8_t *limit0,                      \
      const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, \
      const uint8_t *thresh1

typedef void (*loop_op_t)(uint8_t *s, LOOP_PARAM);
typedef void (*dual_loop_op_t)(uint8_t *s, DUAL_LOOP_PARAM);
typedef void (*hbdloop_op_t)(uint16_t *s, LOOP_PARAM, int bd);
typedef void (*hbddual_loop_op_t)(uint16_t *s, DUAL_LOOP_PARAM, int bd);

typedef std::tuple<hbdloop_op_t, hbdloop_op_t, int> hbdloop_param_t;
typedef std::tuple<hbddual_loop_op_t, hbddual_loop_op_t, int>
    hbddual_loop_param_t;
typedef std::tuple<loop_op_t, loop_op_t, int> loop_param_t;
typedef std::tuple<dual_loop_op_t, dual_loop_op_t, int> dual_loop_param_t;

template <typename Pixel_t, int PIXEL_WIDTH_t>
void InitInput(Pixel_t *s, Pixel_t *ref_s, ACMRandom *rnd, const uint8_t limit,
               const int mask, const int32_t p, const int i) {
  uint16_t tmp_s[kNumCoeffs];

  for (int j = 0; j < kNumCoeffs;) {
    const uint8_t val = rnd->Rand8();
    if (val & 0x80) {  // 50% chance to choose a new value.
      tmp_s[j] = rnd->Rand16();
      j++;
    } else {  // 50% chance to repeat previous value in row X times.
      int k = 0;
      while (k++ < ((val & 0x1f) + 1) && j < kNumCoeffs) {
        if (j < 1) {
          tmp_s[j] = rnd->Rand16();
        } else if (val & 0x20) {  // Increment by a value within the limit.
          tmp_s[j] = static_cast<uint16_t>(tmp_s[j - 1] + (limit - 1));
        } else {  // Decrement by a value within the limit.
          tmp_s[j] = static_cast<uint16_t>(tmp_s[j - 1] - (limit - 1));
        }
        j++;
      }
    }
  }

  for (int j = 0; j < kNumCoeffs;) {
    const uint8_t val = rnd->Rand8();
    if (val & 0x80) {
      j++;
    } else {  // 50% chance to repeat previous value in column X times.
      int k = 0;
      while (k++ < ((val & 0x1f) + 1) && j < kNumCoeffs) {
        if (j < 1) {
          tmp_s[j] = rnd->Rand16();
        } else if (val & 0x20) {  // Increment by a value within the limit.
          tmp_s[(j % 32) * 32 + j / 32] = static_cast<uint16_t>(
              tmp_s[((j - 1) % 32) * 32 + (j - 1) / 32] + (limit - 1));
        } else {  // Decrement by a value within the limit.
          tmp_s[(j % 32) * 32 + j / 32] = static_cast<uint16_t>(
              tmp_s[((j - 1) % 32) * 32 + (j - 1) / 32] - (limit - 1));
        }
        j++;
      }
    }
  }

  for (int j = 0; j < kNumCoeffs; j++) {
    if (i % 2) {
      s[j] = tmp_s[j] & mask;
    } else {
      s[j] = tmp_s[p * (j % p) + j / p] & mask;
    }
    ref_s[j] = s[j];
  }
}

uint8_t GetOuterThresh(ACMRandom *rnd) {
  return static_cast<uint8_t>(rnd->PseudoUniform(3 * MAX_LOOP_FILTER + 5));
}

uint8_t GetInnerThresh(ACMRandom *rnd) {
  return static_cast<uint8_t>(rnd->PseudoUniform(MAX_LOOP_FILTER + 1));
}

uint8_t GetHevThresh(ACMRandom *rnd) {
  return static_cast<uint8_t>(rnd->PseudoUniform(MAX_LOOP_FILTER + 1) >> 4);
}

template <typename func_type_t, typename params_t>
class LoopTestParam : public ::testing::TestWithParam<params_t> {
 public:
  ~LoopTestParam() override = default;
  void SetUp() override {
    loopfilter_op_ = std::get<0>(this->GetParam());
    ref_loopfilter_op_ = std::get<1>(this->GetParam());
    bit_depth_ = std::get<2>(this->GetParam());
    mask_ = (1 << bit_depth_) - 1;
  }

 protected:
  int bit_depth_;
  int mask_;
  func_type_t loopfilter_op_;
  func_type_t ref_loopfilter_op_;
};

#if CONFIG_AV1_HIGHBITDEPTH
void call_filter(uint16_t *s, LOOP_PARAM, int bd, hbdloop_op_t op) {
  op(s, p, blimit, limit, thresh, bd);
}
void call_dualfilter(uint16_t *s, DUAL_LOOP_PARAM, int bd,
                     hbddual_loop_op_t op) {
  op(s, p, blimit0, limit0, thresh0, blimit1, limit1, thresh1, bd);
}
#endif
void call_filter(uint8_t *s, LOOP_PARAM, int bd, loop_op_t op) {
  (void)bd;
  op(s, p, blimit, limit, thresh);
}
void call_dualfilter(uint8_t *s, DUAL_LOOP_PARAM, int bd, dual_loop_op_t op) {
  (void)bd;
  op(s, p, blimit0, limit0, thresh0, blimit1, limit1, thresh1);
}

#if CONFIG_AV1_HIGHBITDEPTH
typedef LoopTestParam<hbdloop_op_t, hbdloop_param_t> Loop8Test6Param_hbd;
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(Loop8Test6Param_hbd);
typedef LoopTestParam<hbddual_loop_op_t, hbddual_loop_param_t>
    Loop8Test9Param_hbd;
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(Loop8Test9Param_hbd);
#endif
typedef LoopTestParam<loop_op_t, loop_param_t> Loop8Test6Param_lbd;
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(Loop8Test6Param_lbd);
typedef LoopTestParam<dual_loop_op_t, dual_loop_param_t> Loop8Test9Param_lbd;
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(Loop8Test9Param_lbd);

#define OPCHECK(a, b)                                                          \
  do {                                                                         \
    ACMRandom rnd(ACMRandom::DeterministicSeed());                             \
    const int count_test_block = number_of_iterations;                         \
    const int32_t p = kNumCoeffs / 32;                                         \
    DECLARE_ALIGNED(b, a, s[kNumCoeffs]);                                      \
    DECLARE_ALIGNED(b, a, ref_s[kNumCoeffs]);                                  \
    int err_count_total = 0;                                                   \
    int first_failure = -1;                                                    \
    for (int i = 0; i < count_test_block; ++i) {                               \
      int err_count = 0;                                                       \
      uint8_t tmp = GetOuterThresh(&rnd);                                      \
      DECLARE_ALIGNED(16, const uint8_t, blimit[16]) = { tmp, tmp, tmp, tmp,   \
                                                         tmp, tmp, tmp, tmp,   \
                                                         tmp, tmp, tmp, tmp,   \
                                                         tmp, tmp, tmp, tmp }; \
      tmp = GetInnerThresh(&rnd);                                              \
      DECLARE_ALIGNED(16, const uint8_t,                                       \
                      limit[16]) = { tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp,   \
                                     tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp }; \
      tmp = GetHevThresh(&rnd);                                                \
      DECLARE_ALIGNED(16, const uint8_t, thresh[16]) = { tmp, tmp, tmp, tmp,   \
                                                         tmp, tmp, tmp, tmp,   \
                                                         tmp, tmp, tmp, tmp,   \
                                                         tmp, tmp, tmp, tmp }; \
      InitInput<a, b>(s, ref_s, &rnd, *limit, mask_, p, i);                    \
      call_filter(ref_s + 8 + p * 8, p, blimit, limit, thresh, bit_depth_,     \
                  ref_loopfilter_op_);                                         \
      API_REGISTER_STATE_CHECK(call_filter(s + 8 + p * 8, p, blimit, limit,    \
                                           thresh, bit_depth_,                 \
                                           loopfilter_op_));                   \
      for (int j = 0; j < kNumCoeffs; ++j) {                                   \
        err_count += ref_s[j] != s[j];                                         \
      }                                                                        \
      if (err_count && !err_count_total) {                                     \
        first_failure = i;                                                     \
      }                                                                        \
      err_count_total += err_count;                                            \
    }                                                                          \
    EXPECT_EQ(0, err_count_total)                                              \
        << "Error: Loop8Test6Param, C output doesn't match SIMD "              \
           "loopfilter output. "                                               \
        << "First failed at test case " << first_failure;                      \
  } while (false)

#if CONFIG_AV1_HIGHBITDEPTH
TEST_P(Loop8Test6Param_hbd, OperationCheck) { OPCHECK(uint16_t, 16); }
#endif
TEST_P(Loop8Test6Param_lbd, OperationCheck) { OPCHECK(uint8_t, 8); }

#define VALCHECK(a, b)                                                         \
  do {                                                                         \
    ACMRandom rnd(ACMRandom::DeterministicSeed());                             \
    const int count_test_block = number_of_iterations;                         \
    DECLARE_ALIGNED(b, a, s[kNumCoeffs]);                                      \
    DECLARE_ALIGNED(b, a, ref_s[kNumCoeffs]);                                  \
    int err_count_total = 0;                                                   \
    int first_failure = -1;                                                    \
    for (int i = 0; i < count_test_block; ++i) {                               \
      int err_count = 0;                                                       \
      uint8_t tmp = GetOuterThresh(&rnd);                                      \
      DECLARE_ALIGNED(16, const uint8_t, blimit[16]) = { tmp, tmp, tmp, tmp,   \
                                                         tmp, tmp, tmp, tmp,   \
                                                         tmp, tmp, tmp, tmp,   \
                                                         tmp, tmp, tmp, tmp }; \
      tmp = GetInnerThresh(&rnd);                                              \
      DECLARE_ALIGNED(16, const uint8_t,                                       \
                      limit[16]) = { tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp,   \
                                     tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp }; \
      tmp = GetHevThresh(&rnd);                                                \
      DECLARE_ALIGNED(16, const uint8_t, thresh[16]) = { tmp, tmp, tmp, tmp,   \
                                                         tmp, tmp, tmp, tmp,   \
                                                         tmp, tmp, tmp, tmp,   \
                                                         tmp, tmp, tmp, tmp }; \
      int32_t p = kNumCoeffs / 32;                                             \
      for (int j = 0; j < kNumCoeffs; ++j) {                                   \
        s[j] = rnd.Rand16() & mask_;                                           \
        ref_s[j] = s[j];                                                       \
      }                                                                        \
      call_filter(ref_s + 8 + p * 8, p, blimit, limit, thresh, bit_depth_,     \
                  ref_loopfilter_op_);                                         \
      API_REGISTER_STATE_CHECK(call_filter(s + 8 + p * 8, p, blimit, limit,    \
                                           thresh, bit_depth_,                 \
                                           loopfilter_op_));                   \
      for (int j = 0; j < kNumCoeffs; ++j) {                                   \
        err_count += ref_s[j] != s[j];                                         \
      }                                                                        \
      if (err_count && !err_count_total) {                                     \
        first_failure = i;                                                     \
      }                                                                        \
      err_count_total += err_count;                                            \
    }                                                                          \
    EXPECT_EQ(0, err_count_total)                                              \
        << "Error: Loop8Test6Param, C output doesn't match SIMD "              \
           "loopfilter output. "                                               \
        << "First failed at test case " << first_failure;                      \
  } while (false)

#if CONFIG_AV1_HIGHBITDEPTH
TEST_P(Loop8Test6Param_hbd, ValueCheck) { VALCHECK(uint16_t, 16); }
#endif
TEST_P(Loop8Test6Param_lbd, ValueCheck) { VALCHECK(uint8_t, 8); }

#define SPEEDCHECK(a, b)                                                      \
  do {                                                                        \
    ACMRandom rnd(ACMRandom::DeterministicSeed());                            \
    const int count_test_block = kSpeedTestNum;                               \
    const int32_t bd = bit_depth_;                                            \
    DECLARE_ALIGNED(b, a, s[kNumCoeffs]);                                     \
    uint8_t tmp = GetOuterThresh(&rnd);                                       \
    DECLARE_ALIGNED(16, const uint8_t,                                        \
                    blimit[16]) = { tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp,   \
                                    tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp }; \
    tmp = GetInnerThresh(&rnd);                                               \
    DECLARE_ALIGNED(16, const uint8_t,                                        \
                    limit[16]) = { tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp,    \
                                   tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp };  \
    tmp = GetHevThresh(&rnd);                                                 \
    DECLARE_ALIGNED(16, const uint8_t,                                        \
                    thresh[16]) = { tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp,   \
                                    tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp }; \
    int32_t p = kNumCoeffs / 32;                                              \
    for (int j = 0; j < kNumCoeffs; ++j) {                                    \
      s[j] = rnd.Rand16() & mask_;                                            \
    }                                                                         \
    for (int i = 0; i < count_test_block; ++i) {                              \
      call_filter(s + 8 + p * 8, p, blimit, limit, thresh, bd,                \
                  loopfilter_op_);                                            \
    }                                                                         \
  } while (false)

#if CONFIG_AV1_HIGHBITDEPTH
TEST_P(Loop8Test6Param_hbd, DISABLED_Speed) { SPEEDCHECK(uint16_t, 16); }
#endif
TEST_P(Loop8Test6Param_lbd, DISABLED_Speed) { SPEEDCHECK(uint8_t, 8); }

#define OPCHECKd(a, b)                                                         \
  do {                                                                         \
    ACMRandom rnd(ACMRandom::DeterministicSeed());                             \
    const int count_test_block = number_of_iterations;                         \
    DECLARE_ALIGNED(b, a, s[kNumCoeffs]);                                      \
    DECLARE_ALIGNED(b, a, ref_s[kNumCoeffs]);                                  \
    int err_count_total = 0;                                                   \
    int first_failure = -1;                                                    \
    for (int i = 0; i < count_test_block; ++i) {                               \
      int err_count = 0;                                                       \
      uint8_t tmp = GetOuterThresh(&rnd);                                      \
      DECLARE_ALIGNED(                                                         \
          16, const uint8_t, blimit0[16]) = { tmp, tmp, tmp, tmp, tmp, tmp,    \
                                              tmp, tmp, tmp, tmp, tmp, tmp,    \
                                              tmp, tmp, tmp, tmp };            \
      tmp = GetInnerThresh(&rnd);                                              \
      DECLARE_ALIGNED(16, const uint8_t, limit0[16]) = { tmp, tmp, tmp, tmp,   \
                                                         tmp, tmp, tmp, tmp,   \
                                                         tmp, tmp, tmp, tmp,   \
                                                         tmp, tmp, tmp, tmp }; \
      tmp = GetHevThresh(&rnd);                                                \
      DECLARE_ALIGNED(                                                         \
          16, const uint8_t, thresh0[16]) = { tmp, tmp, tmp, tmp, tmp, tmp,    \
                                              tmp, tmp, tmp, tmp, tmp, tmp,    \
                                              tmp, tmp, tmp, tmp };            \
      tmp = GetOuterThresh(&rnd);                                              \
      DECLARE_ALIGNED(                                                         \
          16, const uint8_t, blimit1[16]) = { tmp, tmp, tmp, tmp, tmp, tmp,    \
                                              tmp, tmp, tmp, tmp, tmp, tmp,    \
                                              tmp, tmp, tmp, tmp };            \
      tmp = GetInnerThresh(&rnd);                                              \
      DECLARE_ALIGNED(16, const uint8_t, limit1[16]) = { tmp, tmp, tmp, tmp,   \
                                                         tmp, tmp, tmp, tmp,   \
                                                         tmp, tmp, tmp, tmp,   \
                                                         tmp, tmp, tmp, tmp }; \
      tmp = GetHevThresh(&rnd);                                                \
      DECLARE_ALIGNED(                                                         \
          16, const uint8_t, thresh1[16]) = { tmp, tmp, tmp, tmp, tmp, tmp,    \
                                              tmp, tmp, tmp, tmp, tmp, tmp,    \
                                              tmp, tmp, tmp, tmp };            \
      int32_t p = kNumCoeffs / 32;                                             \
      const uint8_t limit = *limit0 < *limit1 ? *limit0 : *limit1;             \
      InitInput<a, b>(s, ref_s, &rnd, limit, mask_, p, i);                     \
      call_dualfilter(ref_s + 8 + p * 8, p, blimit0, limit0, thresh0, blimit1, \
                      limit1, thresh1, bit_depth_, ref_loopfilter_op_);        \
      API_REGISTER_STATE_CHECK(                                                \
          call_dualfilter(s + 8 + p * 8, p, blimit0, limit0, thresh0, blimit1, \
                          limit1, thresh1, bit_depth_, loopfilter_op_));       \
      for (int j = 0; j < kNumCoeffs; ++j) {                                   \
        err_count += ref_s[j] != s[j];                                         \
      }                                                                        \
      if (err_count && !err_count_total) {                                     \
        first_failure = i;                                                     \
      }                                                                        \
      err_count_total += err_count;                                            \
    }                                                                          \
    EXPECT_EQ(0, err_count_total)                                              \
        << "Error: Loop8Test9Param, C output doesn't match SIMD "              \
           "loopfilter output. "                                               \
        << "First failed at test case " << first_failure;                      \
  } while (false)

#if CONFIG_AV1_HIGHBITDEPTH
TEST_P(Loop8Test9Param_hbd, OperationCheck) { OPCHECKd(uint16_t, 16); }
#endif
TEST_P(Loop8Test9Param_lbd, OperationCheck) { OPCHECKd(uint8_t, 8); }

#define VALCHECKd(a, b)                                                        \
  do {                                                                         \
    ACMRandom rnd(ACMRandom::DeterministicSeed());                             \
    const int count_test_block = number_of_iterations;                         \
    DECLARE_ALIGNED(b, a, s[kNumCoeffs]);                                      \
    DECLARE_ALIGNED(b, a, ref_s[kNumCoeffs]);                                  \
    int err_count_total = 0;                                                   \
    int first_failure = -1;                                                    \
    for (int i = 0; i < count_test_block; ++i) {                               \
      int err_count = 0;                                                       \
      uint8_t tmp = GetOuterThresh(&rnd);                                      \
      DECLARE_ALIGNED(                                                         \
          16, const uint8_t, blimit0[16]) = { tmp, tmp, tmp, tmp, tmp, tmp,    \
                                              tmp, tmp, tmp, tmp, tmp, tmp,    \
                                              tmp, tmp, tmp, tmp };            \
      tmp = GetInnerThresh(&rnd);                                              \
      DECLARE_ALIGNED(16, const uint8_t, limit0[16]) = { tmp, tmp, tmp, tmp,   \
                                                         tmp, tmp, tmp, tmp,   \
                                                         tmp, tmp, tmp, tmp,   \
                                                         tmp, tmp, tmp, tmp }; \
      tmp = GetHevThresh(&rnd);                                                \
      DECLARE_ALIGNED(                                                         \
          16, const uint8_t, thresh0[16]) = { tmp, tmp, tmp, tmp, tmp, tmp,    \
                                              tmp, tmp, tmp, tmp, tmp, tmp,    \
                                              tmp, tmp, tmp, tmp };            \
      tmp = GetOuterThresh(&rnd);                                              \
      DECLARE_ALIGNED(                                                         \
          16, const uint8_t, blimit1[16]) = { tmp, tmp, tmp, tmp, tmp, tmp,    \
                                              tmp, tmp, tmp, tmp, tmp, tmp,    \
                                              tmp, tmp, tmp, tmp };            \
      tmp = GetInnerThresh(&rnd);                                              \
      DECLARE_ALIGNED(16, const uint8_t, limit1[16]) = { tmp, tmp, tmp, tmp,   \
                                                         tmp, tmp, tmp, tmp,   \
                                                         tmp, tmp, tmp, tmp,   \
                                                         tmp, tmp, tmp, tmp }; \
      tmp = GetHevThresh(&rnd);                                                \
      DECLARE_ALIGNED(                                                         \
          16, const uint8_t, thresh1[16]) = { tmp, tmp, tmp, tmp, tmp, tmp,    \
                                              tmp, tmp, tmp, tmp, tmp, tmp,    \
                                              tmp, tmp, tmp, tmp };            \
      int32_t p = kNumCoeffs / 32;                                             \
      for (int j = 0; j < kNumCoeffs; ++j) {                                   \
        s[j] = rnd.Rand16() & mask_;                                           \
        ref_s[j] = s[j];                                                       \
      }                                                                        \
      call_dualfilter(ref_s + 8 + p * 8, p, blimit0, limit0, thresh0, blimit1, \
                      limit1, thresh1, bit_depth_, ref_loopfilter_op_);        \
      API_REGISTER_STATE_CHECK(                                                \
          call_dualfilter(s + 8 + p * 8, p, blimit0, limit0, thresh0, blimit1, \
                          limit1, thresh1, bit_depth_, loopfilter_op_));       \
      for (int j = 0; j < kNumCoeffs; ++j) {                                   \
        err_count += ref_s[j] != s[j];                                         \
      }                                                                        \
      if (err_count && !err_count_total) {                                     \
        first_failure = i;                                                     \
      }                                                                        \
      err_count_total += err_count;                                            \
    }                                                                          \
    EXPECT_EQ(0, err_count_total)                                              \
        << "Error: Loop8Test9Param, C output doesn't match SIMD "              \
           "loopfilter output. "                                               \
        << "First failed at test case " << first_failure;                      \
  } while (false)

#if CONFIG_AV1_HIGHBITDEPTH
TEST_P(Loop8Test9Param_hbd, ValueCheck) { VALCHECKd(uint16_t, 16); }
#endif
TEST_P(Loop8Test9Param_lbd, ValueCheck) { VALCHECKd(uint8_t, 8); }

#define SPEEDCHECKd(a, b)                                                      \
  do {                                                                         \
    ACMRandom rnd(ACMRandom::DeterministicSeed());                             \
    const int count_test_block = kSpeedTestNum;                                \
    DECLARE_ALIGNED(b, a, s[kNumCoeffs]);                                      \
    uint8_t tmp = GetOuterThresh(&rnd);                                        \
    DECLARE_ALIGNED(16, const uint8_t,                                         \
                    blimit0[16]) = { tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp,   \
                                     tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp }; \
    tmp = GetInnerThresh(&rnd);                                                \
    DECLARE_ALIGNED(16, const uint8_t,                                         \
                    limit0[16]) = { tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp,    \
                                    tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp };  \
    tmp = GetHevThresh(&rnd);                                                  \
    DECLARE_ALIGNED(16, const uint8_t,                                         \
                    thresh0[16]) = { tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp,   \
                                     tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp }; \
    tmp = GetOuterThresh(&rnd);                                                \
    DECLARE_ALIGNED(16, const uint8_t,                                         \
                    blimit1[16]) = { tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp,   \
                                     tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp }; \
    tmp = GetInnerThresh(&rnd);                                                \
    DECLARE_ALIGNED(16, const uint8_t,                                         \
                    limit1[16]) = { tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp,    \
                                    tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp };  \
    tmp = GetHevThresh(&rnd);                                                  \
    DECLARE_ALIGNED(16, const uint8_t,                                         \
                    thresh1[16]) = { tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp,   \
                                     tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp }; \
    int32_t p = kNumCoeffs / 32;                                               \
    for (int j = 0; j < kNumCoeffs; ++j) {                                     \
      s[j] = rnd.Rand16() & mask_;                                             \
    }                                                                          \
    for (int i = 0; i < count_test_block; ++i) {                               \
      call_dualfilter(s + 8 + p * 8, p, blimit0, limit0, thresh0, blimit1,     \
                      limit1, thresh1, bit_depth_, loopfilter_op_);            \
    }                                                                          \
  } while (false)

#if CONFIG_AV1_HIGHBITDEPTH
TEST_P(Loop8Test9Param_hbd, DISABLED_Speed) { SPEEDCHECKd(uint16_t, 16); }
#endif
TEST_P(Loop8Test9Param_lbd, DISABLED_Speed) { SPEEDCHECKd(uint8_t, 8); }

using std::make_tuple;

#if HAVE_SSE2
#if CONFIG_AV1_HIGHBITDEPTH
const hbdloop_param_t kHbdLoop8Test6[] = {
  make_tuple(&aom_highbd_lpf_horizontal_4_sse2, &aom_highbd_lpf_horizontal_4_c,
             8),
  make_tuple(&aom_highbd_lpf_vertical_4_sse2, &aom_highbd_lpf_vertical_4_c, 8),
  make_tuple(&aom_highbd_lpf_horizontal_6_sse2, &aom_highbd_lpf_horizontal_6_c,
             8),
  make_tuple(&aom_highbd_lpf_horizontal_8_sse2, &aom_highbd_lpf_horizontal_8_c,
             8),
  make_tuple(&aom_highbd_lpf_horizontal_14_sse2,
             &aom_highbd_lpf_horizontal_14_c, 8),
  make_tuple(&aom_highbd_lpf_vertical_6_sse2, &aom_highbd_lpf_vertical_6_c, 8),
  make_tuple(&aom_highbd_lpf_vertical_8_sse2, &aom_highbd_lpf_vertical_8_c, 8),

  make_tuple(&aom_highbd_lpf_vertical_14_sse2, &aom_highbd_lpf_vertical_14_c,
             8),
  make_tuple(&aom_highbd_lpf_horizontal_4_sse2, &aom_highbd_lpf_horizontal_4_c,
             10),
  make_tuple(&aom_highbd_lpf_vertical_4_sse2, &aom_highbd_lpf_vertical_4_c, 10),
  make_tuple(&aom_highbd_lpf_horizontal_6_sse2, &aom_highbd_lpf_horizontal_6_c,
             10),
  make_tuple(&aom_highbd_lpf_horizontal_8_sse2, &aom_highbd_lpf_horizontal_8_c,
             10),
  make_tuple(&aom_highbd_lpf_horizontal_14_sse2,
             &aom_highbd_lpf_horizontal_14_c, 10),
  make_tuple(&aom_highbd_lpf_vertical_6_sse2, &aom_highbd_lpf_vertical_6_c, 10),
  make_tuple(&aom_highbd_lpf_vertical_8_sse2, &aom_highbd_lpf_vertical_8_c, 10),
  make_tuple(&aom_highbd_lpf_vertical_14_sse2, &aom_highbd_lpf_vertical_14_c,
             10),
  make_tuple(&aom_highbd_lpf_horizontal_4_sse2, &aom_highbd_lpf_horizontal_4_c,
             12),
  make_tuple(&aom_highbd_lpf_vertical_4_sse2, &aom_highbd_lpf_vertical_4_c, 12),
  make_tuple(&aom_highbd_lpf_horizontal_6_sse2, &aom_highbd_lpf_horizontal_6_c,
             12),
  make_tuple(&aom_highbd_lpf_horizontal_8_sse2, &aom_highbd_lpf_horizontal_8_c,
             12),
  make_tuple(&aom_highbd_lpf_horizontal_14_sse2,
             &aom_highbd_lpf_horizontal_14_c, 12),
  make_tuple(&aom_highbd_lpf_vertical_14_sse2, &aom_highbd_lpf_vertical_14_c,
             12),
  make_tuple(&aom_highbd_lpf_vertical_6_sse2, &aom_highbd_lpf_vertical_6_c, 12),
  make_tuple(&aom_highbd_lpf_vertical_8_sse2, &aom_highbd_lpf_vertical_8_c, 12)
};

INSTANTIATE_TEST_SUITE_P(SSE2, Loop8Test6Param_hbd,
                         ::testing::ValuesIn(kHbdLoop8Test6));
#endif  // CONFIG_AV1_HIGHBITDEPTH

const loop_param_t kLoop8Test6[] = {
  make_tuple(&aom_lpf_horizontal_4_sse2, &aom_lpf_horizontal_4_c, 8),
  make_tuple(&aom_lpf_horizontal_8_sse2, &aom_lpf_horizontal_8_c, 8),
  make_tuple(&aom_lpf_horizontal_6_sse2, &aom_lpf_horizontal_6_c, 8),
  make_tuple(&aom_lpf_vertical_6_sse2, &aom_lpf_vertical_6_c, 8),
  make_tuple(&aom_lpf_horizontal_14_sse2, &aom_lpf_horizontal_14_c, 8),
  make_tuple(&aom_lpf_vertical_4_sse2, &aom_lpf_vertical_4_c, 8),
  make_tuple(&aom_lpf_vertical_8_sse2, &aom_lpf_vertical_8_c, 8),
  make_tuple(&aom_lpf_vertical_14_sse2, &aom_lpf_vertical_14_c, 8),
  make_tuple(&aom_lpf_horizontal_4_quad_sse2, &aom_lpf_horizontal_4_quad_c, 8),
  make_tuple(&aom_lpf_vertical_4_quad_sse2, &aom_lpf_vertical_4_quad_c, 8),
  make_tuple(&aom_lpf_horizontal_6_quad_sse2, &aom_lpf_horizontal_6_quad_c, 8),
  make_tuple(&aom_lpf_vertical_6_quad_sse2, &aom_lpf_vertical_6_quad_c, 8),
  make_tuple(&aom_lpf_horizontal_8_quad_sse2, &aom_lpf_horizontal_8_quad_c, 8),
  make_tuple(&aom_lpf_vertical_8_quad_sse2, &aom_lpf_vertical_8_quad_c, 8),
  make_tuple(&aom_lpf_horizontal_14_quad_sse2, &aom_lpf_horizontal_14_quad_c,
             8),
  make_tuple(&aom_lpf_vertical_14_quad_sse2, &aom_lpf_vertical_14_quad_c, 8)
};

INSTANTIATE_TEST_SUITE_P(SSE2, Loop8Test6Param_lbd,
                         ::testing::ValuesIn(kLoop8Test6));

const dual_loop_param_t kLoop8Test9[] = {
  make_tuple(&aom_lpf_horizontal_4_dual_sse2, &aom_lpf_horizontal_4_dual_c, 8),
  make_tuple(&aom_lpf_vertical_4_dual_sse2, &aom_lpf_vertical_4_dual_c, 8),
  make_tuple(&aom_lpf_horizontal_6_dual_sse2, &aom_lpf_horizontal_6_dual_c, 8),
  make_tuple(&aom_lpf_vertical_6_dual_sse2, &aom_lpf_vertical_6_dual_c, 8),
  make_tuple(&aom_lpf_horizontal_8_dual_sse2, &aom_lpf_horizontal_8_dual_c, 8),
  make_tuple(&aom_lpf_vertical_8_dual_sse2, &aom_lpf_vertical_8_dual_c, 8),
  make_tuple(&aom_lpf_horizontal_14_dual_sse2, &aom_lpf_horizontal_14_dual_c,
             8),
  make_tuple(&aom_lpf_vertical_14_dual_sse2, &aom_lpf_vertical_14_dual_c, 8)
};

INSTANTIATE_TEST_SUITE_P(SSE2, Loop8Test9Param_lbd,
                         ::testing::ValuesIn(kLoop8Test9));

#endif  // HAVE_SSE2

#if HAVE_AVX2
const loop_param_t kLoop8Test6Avx2[] = {
  make_tuple(&aom_lpf_horizontal_6_quad_avx2, &aom_lpf_horizontal_6_quad_c, 8),
  make_tuple(&aom_lpf_horizontal_8_quad_avx2, &aom_lpf_horizontal_8_quad_c, 8),
  make_tuple(&aom_lpf_horizontal_14_quad_avx2, &aom_lpf_horizontal_14_quad_c,
             8),
  make_tuple(&aom_lpf_vertical_14_quad_avx2, &aom_lpf_vertical_14_quad_c, 8),
};

INSTANTIATE_TEST_SUITE_P(AVX2, Loop8Test6Param_lbd,
                         ::testing::ValuesIn(kLoop8Test6Avx2));
#endif

#if HAVE_SSE2 && CONFIG_AV1_HIGHBITDEPTH
const hbddual_loop_param_t kHbdLoop8Test9[] = {
  make_tuple(&aom_highbd_lpf_horizontal_4_dual_sse2,
             &aom_highbd_lpf_horizontal_4_dual_c, 8),
  make_tuple(&aom_highbd_lpf_horizontal_6_dual_sse2,
             &aom_highbd_lpf_horizontal_6_dual_c, 8),
  make_tuple(&aom_highbd_lpf_horizontal_8_dual_sse2,
             &aom_highbd_lpf_horizontal_8_dual_c, 8),
  make_tuple(&aom_highbd_lpf_horizontal_14_dual_sse2,
             &aom_highbd_lpf_horizontal_14_dual_c, 8),
  make_tuple(&aom_highbd_lpf_vertical_4_dual_sse2,
             &aom_highbd_lpf_vertical_4_dual_c, 8),
  make_tuple(&aom_highbd_lpf_vertical_6_dual_sse2,
             &aom_highbd_lpf_vertical_6_dual_c, 8),
  make_tuple(&aom_highbd_lpf_vertical_8_dual_sse2,
             &aom_highbd_lpf_vertical_8_dual_c, 8),
  make_tuple(&aom_highbd_lpf_vertical_14_dual_sse2,
             &aom_highbd_lpf_vertical_14_dual_c, 8),
  make_tuple(&aom_highbd_lpf_horizontal_4_dual_sse2,
             &aom_highbd_lpf_horizontal_4_dual_c, 10),
  make_tuple(&aom_highbd_lpf_horizontal_6_dual_sse2,
             &aom_highbd_lpf_horizontal_6_dual_c, 10),
  make_tuple(&aom_highbd_lpf_horizontal_8_dual_sse2,
             &aom_highbd_lpf_horizontal_8_dual_c, 10),
  make_tuple(&aom_highbd_lpf_horizontal_14_dual_sse2,
             &aom_highbd_lpf_horizontal_14_dual_c, 10),
  make_tuple(&aom_highbd_lpf_vertical_4_dual_sse2,
             &aom_highbd_lpf_vertical_4_dual_c, 10),
  make_tuple(&aom_highbd_lpf_vertical_6_dual_sse2,
             &aom_highbd_lpf_vertical_6_dual_c, 10),
  make_tuple(&aom_highbd_lpf_vertical_8_dual_sse2,
             &aom_highbd_lpf_vertical_8_dual_c, 10),
  make_tuple(&aom_highbd_lpf_vertical_14_dual_sse2,
             &aom_highbd_lpf_vertical_14_dual_c, 10),
  make_tuple(&aom_highbd_lpf_horizontal_4_dual_sse2,
             &aom_highbd_lpf_horizontal_4_dual_c, 12),
  make_tuple(&aom_highbd_lpf_horizontal_6_dual_sse2,
             &aom_highbd_lpf_horizontal_6_dual_c, 12),
  make_tuple(&aom_highbd_lpf_horizontal_8_dual_sse2,
             &aom_highbd_lpf_horizontal_8_dual_c, 12),
  make_tuple(&aom_highbd_lpf_horizontal_14_dual_sse2,
             &aom_highbd_lpf_horizontal_14_dual_c, 12),
  make_tuple(&aom_highbd_lpf_vertical_4_dual_sse2,
             &aom_highbd_lpf_vertical_4_dual_c, 12),
  make_tuple(&aom_highbd_lpf_vertical_6_dual_sse2,
             &aom_highbd_lpf_vertical_6_dual_c, 12),
  make_tuple(&aom_highbd_lpf_vertical_8_dual_sse2,
             &aom_highbd_lpf_vertical_8_dual_c, 12),
  make_tuple(&aom_highbd_lpf_vertical_14_dual_sse2,
             &aom_highbd_lpf_vertical_14_dual_c, 12),
};

INSTANTIATE_TEST_SUITE_P(SSE2, Loop8Test9Param_hbd,
                         ::testing::ValuesIn(kHbdLoop8Test9));

#endif  // HAVE_SSE2 && CONFIG_AV1_HIGHBITDEPTH

#if HAVE_NEON
const loop_param_t kLoop8Test6[] = {
  make_tuple(&aom_lpf_vertical_14_neon, &aom_lpf_vertical_14_c, 8),
  make_tuple(&aom_lpf_vertical_8_neon, &aom_lpf_vertical_8_c, 8),
  make_tuple(&aom_lpf_vertical_6_neon, &aom_lpf_vertical_6_c, 8),
  make_tuple(&aom_lpf_vertical_4_neon, &aom_lpf_vertical_4_c, 8),
  make_tuple(&aom_lpf_horizontal_14_neon, &aom_lpf_horizontal_14_c, 8),
  make_tuple(&aom_lpf_horizontal_8_neon, &aom_lpf_horizontal_8_c, 8),
  make_tuple(&aom_lpf_horizontal_6_neon, &aom_lpf_horizontal_6_c, 8),
  make_tuple(&aom_lpf_horizontal_4_neon, &aom_lpf_horizontal_4_c, 8),
  make_tuple(&aom_lpf_horizontal_4_quad_neon, &aom_lpf_horizontal_4_quad_c, 8),
  make_tuple(&aom_lpf_vertical_4_quad_neon, &aom_lpf_vertical_4_quad_c, 8),
  make_tuple(&aom_lpf_horizontal_6_quad_neon, &aom_lpf_horizontal_6_quad_c, 8),
  make_tuple(&aom_lpf_vertical_6_quad_neon, &aom_lpf_vertical_6_quad_c, 8),
  make_tuple(&aom_lpf_horizontal_8_quad_neon, &aom_lpf_horizontal_8_quad_c, 8),
  make_tuple(&aom_lpf_vertical_8_quad_neon, &aom_lpf_vertical_8_quad_c, 8),
  make_tuple(&aom_lpf_horizontal_14_quad_neon, &aom_lpf_horizontal_14_quad_c,
             8),
  make_tuple(&aom_lpf_vertical_14_quad_neon, &aom_lpf_vertical_14_quad_c, 8)
};

INSTANTIATE_TEST_SUITE_P(NEON, Loop8Test6Param_lbd,
                         ::testing::ValuesIn(kLoop8Test6));

const dual_loop_param_t kLoop8Test9[] = {
  make_tuple(&aom_lpf_horizontal_4_dual_neon, &aom_lpf_horizontal_4_dual_c, 8),
  make_tuple(&aom_lpf_horizontal_6_dual_neon, &aom_lpf_horizontal_6_dual_c, 8),
  make_tuple(&aom_lpf_horizontal_8_dual_neon, &aom_lpf_horizontal_8_dual_c, 8),
  make_tuple(&aom_lpf_horizontal_14_dual_neon, &aom_lpf_horizontal_14_dual_c,
             8),
  make_tuple(&aom_lpf_vertical_4_dual_neon, &aom_lpf_vertical_4_dual_c, 8),
  make_tuple(&aom_lpf_vertical_6_dual_neon, &aom_lpf_vertical_6_dual_c, 8),
  make_tuple(&aom_lpf_vertical_8_dual_neon, &aom_lpf_vertical_8_dual_c, 8),
  make_tuple(&aom_lpf_vertical_14_dual_neon, &aom_lpf_vertical_14_dual_c, 8)
};

INSTANTIATE_TEST_SUITE_P(NEON, Loop8Test9Param_lbd,
                         ::testing::ValuesIn(kLoop8Test9));
#if CONFIG_AV1_HIGHBITDEPTH
const hbdloop_param_t kHbdLoop8Test6[] = {
  make_tuple(&aom_highbd_lpf_horizontal_4_neon, &aom_highbd_lpf_horizontal_4_c,
             8),
  make_tuple(&aom_highbd_lpf_horizontal_4_neon, &aom_highbd_lpf_horizontal_4_c,
             10),
  make_tuple(&aom_highbd_lpf_horizontal_4_neon, &aom_highbd_lpf_horizontal_4_c,
             12),
  make_tuple(&aom_highbd_lpf_horizontal_6_neon, &aom_highbd_lpf_horizontal_6_c,
             8),
  make_tuple(&aom_highbd_lpf_horizontal_6_neon, &aom_highbd_lpf_horizontal_6_c,
             10),
  make_tuple(&aom_highbd_lpf_horizontal_6_neon, &aom_highbd_lpf_horizontal_6_c,
             12),
  make_tuple(&aom_highbd_lpf_horizontal_8_neon, &aom_highbd_lpf_horizontal_8_c,
             8),
  make_tuple(&aom_highbd_lpf_horizontal_8_neon, &aom_highbd_lpf_horizontal_8_c,
             10),
  make_tuple(&aom_highbd_lpf_horizontal_8_neon, &aom_highbd_lpf_horizontal_8_c,
             12),
  make_tuple(&aom_highbd_lpf_horizontal_14_neon,
             &aom_highbd_lpf_horizontal_14_c, 8),
  make_tuple(&aom_highbd_lpf_horizontal_14_neon,
             &aom_highbd_lpf_horizontal_14_c, 10),
  make_tuple(&aom_highbd_lpf_horizontal_14_neon,
             &aom_highbd_lpf_horizontal_14_c, 12),
  make_tuple(&aom_highbd_lpf_vertical_4_neon, &aom_highbd_lpf_vertical_4_c, 8),
  make_tuple(&aom_highbd_lpf_vertical_4_neon, &aom_highbd_lpf_vertical_4_c, 10),
  make_tuple(&aom_highbd_lpf_vertical_4_neon, &aom_highbd_lpf_vertical_4_c, 12),
  make_tuple(&aom_highbd_lpf_vertical_6_neon, &aom_highbd_lpf_vertical_6_c, 8),
  make_tuple(&aom_highbd_lpf_vertical_6_neon, &aom_highbd_lpf_vertical_6_c, 10),
  make_tuple(&aom_highbd_lpf_vertical_6_neon, &aom_highbd_lpf_vertical_6_c, 12),
  make_tuple(&aom_highbd_lpf_vertical_8_neon, &aom_highbd_lpf_vertical_8_c, 8),
  make_tuple(&aom_highbd_lpf_vertical_8_neon, &aom_highbd_lpf_vertical_8_c, 10),
  make_tuple(&aom_highbd_lpf_vertical_8_neon, &aom_highbd_lpf_vertical_8_c, 12),
  make_tuple(&aom_highbd_lpf_vertical_14_neon, &aom_highbd_lpf_vertical_14_c,
             8),
  make_tuple(&aom_highbd_lpf_vertical_14_neon, &aom_highbd_lpf_vertical_14_c,
             10),
  make_tuple(&aom_highbd_lpf_vertical_14_neon, &aom_highbd_lpf_vertical_14_c,
             12),
};

INSTANTIATE_TEST_SUITE_P(NEON, Loop8Test6Param_hbd,
                         ::testing::ValuesIn(kHbdLoop8Test6));

const hbddual_loop_param_t kHbdLoop8Test9[] = {
  make_tuple(&aom_highbd_lpf_horizontal_4_dual_neon,
             &aom_highbd_lpf_horizontal_4_dual_c, 8),
  make_tuple(&aom_highbd_lpf_horizontal_6_dual_neon,
             &aom_highbd_lpf_horizontal_6_dual_c, 8),
  make_tuple(&aom_highbd_lpf_horizontal_8_dual_neon,
             &aom_highbd_lpf_horizontal_8_dual_c, 8),
  make_tuple(&aom_highbd_lpf_horizontal_14_dual_neon,
             &aom_highbd_lpf_horizontal_14_dual_c, 8),
  make_tuple(&aom_highbd_lpf_vertical_4_dual_neon,
             &aom_highbd_lpf_vertical_4_dual_c, 8),
  make_tuple(&aom_highbd_lpf_vertical_6_dual_neon,
             &aom_highbd_lpf_vertical_6_dual_c, 8),
  make_tuple(&aom_highbd_lpf_vertical_8_dual_neon,
             &aom_highbd_lpf_vertical_8_dual_c, 8),
  make_tuple(&aom_highbd_lpf_vertical_14_dual_neon,
             &aom_highbd_lpf_vertical_14_dual_c, 8),
  make_tuple(&aom_highbd_lpf_horizontal_4_dual_neon,
             &aom_highbd_lpf_horizontal_4_dual_c, 10),
  make_tuple(&aom_highbd_lpf_horizontal_6_dual_neon,
             &aom_highbd_lpf_horizontal_6_dual_c, 10),
  make_tuple(&aom_highbd_lpf_horizontal_8_dual_neon,
             &aom_highbd_lpf_horizontal_8_dual_c, 10),
  make_tuple(&aom_highbd_lpf_horizontal_14_dual_neon,
             &aom_highbd_lpf_horizontal_14_dual_c, 10),
  make_tuple(&aom_highbd_lpf_vertical_4_dual_neon,
             &aom_highbd_lpf_vertical_4_dual_c, 10),
  make_tuple(&aom_highbd_lpf_vertical_6_dual_neon,
             &aom_highbd_lpf_vertical_6_dual_c, 10),
  make_tuple(&aom_highbd_lpf_vertical_8_dual_neon,
             &aom_highbd_lpf_vertical_8_dual_c, 10),
  make_tuple(&aom_highbd_lpf_vertical_14_dual_neon,
             &aom_highbd_lpf_vertical_14_dual_c, 10),
  make_tuple(&aom_highbd_lpf_horizontal_4_dual_neon,
             &aom_highbd_lpf_horizontal_4_dual_c, 12),
  make_tuple(&aom_highbd_lpf_horizontal_6_dual_neon,
             &aom_highbd_lpf_horizontal_6_dual_c, 12),
  make_tuple(&aom_highbd_lpf_horizontal_8_dual_neon,
             &aom_highbd_lpf_horizontal_8_dual_c, 12),
  make_tuple(&aom_highbd_lpf_horizontal_14_dual_neon,
             &aom_highbd_lpf_horizontal_14_dual_c, 12),
  make_tuple(&aom_highbd_lpf_vertical_4_dual_neon,
             &aom_highbd_lpf_vertical_4_dual_c, 12),
  make_tuple(&aom_highbd_lpf_vertical_6_dual_neon,
             &aom_highbd_lpf_vertical_6_dual_c, 12),
  make_tuple(&aom_highbd_lpf_vertical_8_dual_neon,
             &aom_highbd_lpf_vertical_8_dual_c, 12),
  make_tuple(&aom_highbd_lpf_vertical_14_dual_neon,
             &aom_highbd_lpf_vertical_14_dual_c, 12),
};

INSTANTIATE_TEST_SUITE_P(NEON, Loop8Test9Param_hbd,
                         ::testing::ValuesIn(kHbdLoop8Test9));

#endif  // CONFIG_AV1_HIGHBITDEPTH
#endif  // HAVE_NEON

#if HAVE_AVX2 && CONFIG_AV1_HIGHBITDEPTH
const hbddual_loop_param_t kHbdLoop8Test9Avx2[] = {
  make_tuple(&aom_highbd_lpf_horizontal_4_dual_avx2,
             &aom_highbd_lpf_horizontal_4_dual_c, 8),
  make_tuple(&aom_highbd_lpf_horizontal_4_dual_avx2,
             &aom_highbd_lpf_horizontal_4_dual_c, 10),
  make_tuple(&aom_highbd_lpf_horizontal_4_dual_avx2,
             &aom_highbd_lpf_horizontal_4_dual_c, 12),
  make_tuple(&aom_highbd_lpf_horizontal_8_dual_avx2,
             &aom_highbd_lpf_horizontal_8_dual_c, 8),
  make_tuple(&aom_highbd_lpf_horizontal_8_dual_avx2,
             &aom_highbd_lpf_horizontal_8_dual_c, 10),
  make_tuple(&aom_highbd_lpf_horizontal_8_dual_avx2,
             &aom_highbd_lpf_horizontal_8_dual_c, 12),
  make_tuple(&aom_highbd_lpf_vertical_4_dual_avx2,
             &aom_highbd_lpf_vertical_4_dual_c, 8),
  make_tuple(&aom_highbd_lpf_vertical_4_dual_avx2,
             &aom_highbd_lpf_vertical_4_dual_c, 10),
  make_tuple(&aom_highbd_lpf_vertical_4_dual_avx2,
             &aom_highbd_lpf_vertical_4_dual_c, 12),
  make_tuple(&aom_highbd_lpf_vertical_8_dual_avx2,
             &aom_highbd_lpf_vertical_8_dual_c, 8),
  make_tuple(&aom_highbd_lpf_vertical_8_dual_avx2,
             &aom_highbd_lpf_vertical_8_dual_c, 10),
  make_tuple(&aom_highbd_lpf_vertical_8_dual_avx2,
             &aom_highbd_lpf_vertical_8_dual_c, 12),
};

INSTANTIATE_TEST_SUITE_P(AVX2, Loop8Test9Param_hbd,
                         ::testing::ValuesIn(kHbdLoop8Test9Avx2));
#endif
}  // namespace
