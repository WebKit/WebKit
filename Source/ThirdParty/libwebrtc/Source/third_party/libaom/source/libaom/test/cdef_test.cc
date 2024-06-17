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

#include <array>
#include <cstdlib>
#include <iostream>
#include <string>
#include <tuple>

#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

#include "config/aom_config.h"
#include "config/av1_rtcd.h"

#include "aom_ports/aom_timer.h"
#include "av1/common/cdef_block.h"
#include "test/acm_random.h"
#include "test/register_state_check.h"
#include "test/util.h"

using libaom_test::ACMRandom;

namespace {

using CdefFilterBlockFunctions = std::array<cdef_filter_block_func, 4>;

typedef std::tuple<CdefFilterBlockFunctions, CdefFilterBlockFunctions,
                   BLOCK_SIZE, int, int>
    cdef_dir_param_t;

class CDEFBlockTest : public ::testing::TestWithParam<cdef_dir_param_t> {
 public:
  ~CDEFBlockTest() override = default;
  void SetUp() override {
    cdef = GET_PARAM(0);
    ref_cdef = GET_PARAM(1);
    bsize = GET_PARAM(2);
    boundary = GET_PARAM(3);
    depth = GET_PARAM(4);
  }

 protected:
  BLOCK_SIZE bsize;
  int boundary;
  int depth;
  CdefFilterBlockFunctions cdef;
  CdefFilterBlockFunctions ref_cdef;
};
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(CDEFBlockTest);

typedef CDEFBlockTest CDEFBlockHighbdTest;
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(CDEFBlockHighbdTest);

typedef CDEFBlockTest CDEFSpeedTest;
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(CDEFSpeedTest);

typedef CDEFBlockTest CDEFSpeedHighbdTest;
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(CDEFSpeedHighbdTest);

int64_t test_cdef(BLOCK_SIZE bsize, int iterations,
                  CdefFilterBlockFunctions cdef,
                  CdefFilterBlockFunctions ref_cdef, int boundary, int depth) {
  aom_usec_timer ref_timer;
  int64_t ref_elapsed_time = 0;
  const int size = 8;
  const int ysize = size + 2 * CDEF_VBORDER;
  ACMRandom rnd(ACMRandom::DeterministicSeed());
  DECLARE_ALIGNED(16, uint16_t, s[ysize * CDEF_BSTRIDE]);
  DECLARE_ALIGNED(16, static uint16_t, d[size * size]);
  DECLARE_ALIGNED(16, static uint16_t, ref_d[size * size]);
  memset(ref_d, 0, sizeof(ref_d));
  memset(d, 0, sizeof(d));

  int error = 0, pristrength = 0, secstrength, dir;
  int pridamping, secdamping, bits, level, count,
      errdepth = 0, errpristrength = 0, errsecstrength = 0, errboundary = 0,
      errpridamping = 0, errsecdamping = 0;
  unsigned int pos = 0;

  const int block_width =
      ((bsize == BLOCK_8X8) || (bsize == BLOCK_8X4)) ? 8 : 4;
  const int block_height =
      ((bsize == BLOCK_8X8) || (bsize == BLOCK_4X8)) ? 8 : 4;
  const unsigned int max_pos = size * size >> static_cast<int>(depth == 8);
  for (pridamping = 3 + depth - 8; pridamping < 7 - 3 * !!boundary + depth - 8;
       pridamping++) {
    for (secdamping = 3 + depth - 8;
         secdamping < 7 - 3 * !!boundary + depth - 8; secdamping++) {
      for (count = 0; count < iterations; count++) {
        for (level = 0; level < (1 << depth) && !error;
             level += (2 + 6 * !!boundary) << (depth - 8)) {
          for (bits = 1; bits <= depth && !error; bits += 1 + 3 * !!boundary) {
            for (unsigned int i = 0; i < sizeof(s) / sizeof(*s); i++)
              s[i] = clamp((rnd.Rand16() & ((1 << bits) - 1)) + level, 0,
                           (1 << depth) - 1);
            if (boundary) {
              if (boundary & 1) {  // Left
                for (int i = 0; i < ysize; i++)
                  for (int j = 0; j < CDEF_HBORDER; j++)
                    s[i * CDEF_BSTRIDE + j] = CDEF_VERY_LARGE;
              }
              if (boundary & 2) {  // Right
                for (int i = 0; i < ysize; i++)
                  for (int j = CDEF_HBORDER + size; j < CDEF_BSTRIDE; j++)
                    s[i * CDEF_BSTRIDE + j] = CDEF_VERY_LARGE;
              }
              if (boundary & 4) {  // Above
                for (int i = 0; i < CDEF_VBORDER; i++)
                  for (int j = 0; j < CDEF_BSTRIDE; j++)
                    s[i * CDEF_BSTRIDE + j] = CDEF_VERY_LARGE;
              }
              if (boundary & 8) {  // Below
                for (int i = CDEF_VBORDER + size; i < ysize; i++)
                  for (int j = 0; j < CDEF_BSTRIDE; j++)
                    s[i * CDEF_BSTRIDE + j] = CDEF_VERY_LARGE;
              }
            }
            for (dir = 0; dir < 8; dir++) {
              for (pristrength = 0; pristrength <= 19 << (depth - 8) && !error;
                   pristrength += (1 + 4 * !!boundary) << (depth - 8)) {
                if (pristrength == 16) pristrength = 19;
                for (secstrength = 0; secstrength <= 4 << (depth - 8) && !error;
                     secstrength += 1 << (depth - 8)) {
                  if (secstrength == 3 << (depth - 8)) continue;

                  const int strength_index =
                      (secstrength == 0) | ((pristrength == 0) << 1);

                  aom_usec_timer_start(&ref_timer);
                  ref_cdef[strength_index](
                      ref_d, size,
                      s + CDEF_HBORDER + CDEF_VBORDER * CDEF_BSTRIDE,
                      pristrength, secstrength, dir, pridamping, secdamping,
                      depth - 8, block_width, block_height);
                  aom_usec_timer_mark(&ref_timer);
                  ref_elapsed_time += aom_usec_timer_elapsed(&ref_timer);
                  // If cdef and ref_cdef are the same, we're just testing
                  // speed
                  if (cdef[0] != ref_cdef[0])
                    API_REGISTER_STATE_CHECK(cdef[strength_index](
                        d, size, s + CDEF_HBORDER + CDEF_VBORDER * CDEF_BSTRIDE,
                        pristrength, secstrength, dir, pridamping, secdamping,
                        depth - 8, block_width, block_height));
                  if (ref_cdef[0] != cdef[0]) {
                    for (pos = 0; pos < max_pos && !error; pos++) {
                      error = ref_d[pos] != d[pos];
                      errdepth = depth;
                      errpristrength = pristrength;
                      errsecstrength = secstrength;
                      errboundary = boundary;
                      errpridamping = pridamping;
                      errsecdamping = secdamping;
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
  }

  pos--;
  EXPECT_EQ(0, error) << "Error: CDEFBlockTest, SIMD and C mismatch."
                      << std::endl
                      << "First error at " << pos % size << "," << pos / size
                      << " (" << (int16_t)ref_d[pos] << " : " << (int16_t)d[pos]
                      << ") " << std::endl
                      << "pristrength: " << errpristrength << std::endl
                      << "pridamping: " << errpridamping << std::endl
                      << "secstrength: " << errsecstrength << std::endl
                      << "secdamping: " << errsecdamping << std::endl
                      << "depth: " << errdepth << std::endl
                      << "size: " << bsize << std::endl
                      << "boundary: " << errboundary << std::endl
                      << std::endl;

  return ref_elapsed_time;
}

void test_cdef_speed(BLOCK_SIZE bsize, int iterations,
                     CdefFilterBlockFunctions cdef,
                     CdefFilterBlockFunctions ref_cdef, int boundary,
                     int depth) {
  int64_t ref_elapsed_time =
      test_cdef(bsize, iterations, ref_cdef, ref_cdef, boundary, depth);

  int64_t elapsed_time =
      test_cdef(bsize, iterations, cdef, cdef, boundary, depth);

  std::cout << "C time: " << ref_elapsed_time << " us" << std::endl
            << "SIMD time: " << elapsed_time << " us" << std::endl;

  EXPECT_GT(ref_elapsed_time, elapsed_time)
      << "Error: CDEFSpeedTest, SIMD slower than C." << std::endl
      << "C time: " << ref_elapsed_time << " us" << std::endl
      << "SIMD time: " << elapsed_time << " us" << std::endl;
}

typedef int (*find_dir_t)(const uint16_t *img, int stride, int32_t *var,
                          int coeff_shift);

typedef std::tuple<find_dir_t, find_dir_t> find_dir_param_t;

class CDEFFindDirTest : public ::testing::TestWithParam<find_dir_param_t> {
 public:
  ~CDEFFindDirTest() override = default;
  void SetUp() override {
    finddir = GET_PARAM(0);
    ref_finddir = GET_PARAM(1);
  }

 protected:
  find_dir_t finddir;
  find_dir_t ref_finddir;
};
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(CDEFFindDirTest);

typedef CDEFFindDirTest CDEFFindDirSpeedTest;
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(CDEFFindDirSpeedTest);

void test_finddir(int (*finddir)(const uint16_t *img, int stride, int32_t *var,
                                 int coeff_shift),
                  int (*ref_finddir)(const uint16_t *img, int stride,
                                     int32_t *var, int coeff_shift)) {
  const int size = 8;
  ACMRandom rnd(ACMRandom::DeterministicSeed());
  DECLARE_ALIGNED(16, uint16_t, s[size * size]);

  int error = 0;
  int depth, bits, level, count, errdepth = 0;
  int ref_res = 0, res = 0;
  int32_t ref_var = 0, var = 0;

  for (depth = 8; depth <= 12 && !error; depth += 2) {
    for (count = 0; count < 512 && !error; count++) {
      for (level = 0; level < (1 << depth) && !error;
           level += 1 << (depth - 8)) {
        for (bits = 1; bits <= depth && !error; bits++) {
          for (unsigned int i = 0; i < sizeof(s) / sizeof(*s); i++)
            s[i] = clamp((rnd.Rand16() & ((1 << bits) - 1)) + level, 0,
                         (1 << depth) - 1);
          for (int c = 0; c < 1 + 9 * (finddir == ref_finddir); c++)
            ref_res = ref_finddir(s, size, &ref_var, depth - 8);
          if (finddir != ref_finddir)
            API_REGISTER_STATE_CHECK(res = finddir(s, size, &var, depth - 8));
          if (ref_finddir != finddir) {
            if (res != ref_res || var != ref_var) error = 1;
            errdepth = depth;
          }
        }
      }
    }
  }

  EXPECT_EQ(0, error) << "Error: CDEFFindDirTest, SIMD and C mismatch."
                      << std::endl
                      << "return: " << res << " : " << ref_res << std::endl
                      << "var: " << var << " : " << ref_var << std::endl
                      << "depth: " << errdepth << std::endl
                      << std::endl;
}

void test_finddir_speed(int (*finddir)(const uint16_t *img, int stride,
                                       int32_t *var, int coeff_shift),
                        int (*ref_finddir)(const uint16_t *img, int stride,
                                           int32_t *var, int coeff_shift)) {
  aom_usec_timer ref_timer;
  aom_usec_timer timer;

  aom_usec_timer_start(&ref_timer);
  test_finddir(ref_finddir, ref_finddir);
  aom_usec_timer_mark(&ref_timer);
  int64_t ref_elapsed_time = aom_usec_timer_elapsed(&ref_timer);

  aom_usec_timer_start(&timer);
  test_finddir(finddir, finddir);
  aom_usec_timer_mark(&timer);
  int64_t elapsed_time = aom_usec_timer_elapsed(&timer);

  EXPECT_GT(ref_elapsed_time, elapsed_time)
      << "Error: CDEFFindDirSpeedTest, SIMD slower than C." << std::endl
      << "C time: " << ref_elapsed_time << " us" << std::endl
      << "SIMD time: " << elapsed_time << " us" << std::endl;
}

typedef void (*find_dir_dual_t)(const uint16_t *img1, const uint16_t *img2,
                                int stride, int32_t *var1, int32_t *var2,
                                int coeff_shift, int *out1, int *out2);

typedef std::tuple<find_dir_dual_t, find_dir_dual_t> find_dir_dual_param_t;

class CDEFFindDirDualTest
    : public ::testing::TestWithParam<find_dir_dual_param_t> {
 public:
  ~CDEFFindDirDualTest() override = default;
  void SetUp() override {
    finddir = GET_PARAM(0);
    ref_finddir = GET_PARAM(1);
  }

 protected:
  find_dir_dual_t finddir;
  find_dir_dual_t ref_finddir;
};
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(CDEFFindDirDualTest);

typedef CDEFFindDirDualTest CDEFFindDirDualSpeedTest;
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(CDEFFindDirDualSpeedTest);

void test_finddir_dual(
    void (*finddir)(const uint16_t *img1, const uint16_t *img2, int stride,
                    int32_t *var1, int32_t *var2, int coeff_shift, int *out1,
                    int *out2),
    void (*ref_finddir)(const uint16_t *img1, const uint16_t *img2, int stride,
                        int32_t *var1, int32_t *var2, int coeff_shift,
                        int *out1, int *out2)) {
  const int size_wd = 16;
  const int size_ht = 8;
  ACMRandom rnd(ACMRandom::DeterministicSeed());
  DECLARE_ALIGNED(16, uint16_t, s[size_ht * size_wd]);

  int error = 0, errdepth = 0;
  int32_t ref_var[2] = { 0 };
  int ref_dir[2] = { 0 };
  int32_t var[2] = { 0 };
  int dir[2] = { 0 };

  for (int depth = 8; depth <= 12 && !error; depth += 2) {
    for (int count = 0; count < 512 && !error; count++) {
      for (int level = 0; level < (1 << depth) && !error;
           level += 1 << (depth - 8)) {
        for (int bits = 1; bits <= depth && !error; bits++) {
          for (unsigned int i = 0; i < sizeof(s) / sizeof(*s); i++)
            s[i] = clamp((rnd.Rand16() & ((1 << bits) - 1)) + level, 0,
                         (1 << depth) - 1);
          for (int c = 0; c < 1 + 9 * (finddir == ref_finddir); c++)
            ref_finddir(s, s + 8, size_wd, &ref_var[0], &ref_var[1], depth - 8,
                        &ref_dir[0], &ref_dir[1]);
          if (finddir != ref_finddir)
            API_REGISTER_STATE_CHECK(finddir(s, s + 8, size_wd, &var[0],
                                             &var[1], depth - 8, &dir[0],
                                             &dir[1]));
          if (ref_finddir != finddir) {
            for (int j = 0; j < 2; j++) {
              if (ref_dir[j] != dir[j] || ref_var[j] != var[j]) error = 1;
            }
            errdepth = depth;
          }
        }
      }
    }
  }

  for (int j = 0; j < 2; j++) {
    EXPECT_EQ(0, error) << "Error: CDEFFindDirTest, SIMD and C mismatch."
                        << std::endl
                        << "direction: " << dir[j] << " : " << ref_dir[j]
                        << std::endl
                        << "variance: " << var[j] << " : " << ref_var[j]
                        << std::endl
                        << "depth: " << errdepth << std::endl
                        << std::endl;
  }
}

void test_finddir_dual_speed(
    void (*finddir)(const uint16_t *img1, const uint16_t *img2, int stride,
                    int32_t *var1, int32_t *var2, int coeff_shift, int *out1,
                    int *out2),
    void (*ref_finddir)(const uint16_t *img1, const uint16_t *img2, int stride,
                        int32_t *var1, int32_t *var2, int coeff_shift,
                        int *out1, int *out2)) {
  aom_usec_timer ref_timer;
  aom_usec_timer timer;

  aom_usec_timer_start(&ref_timer);
  test_finddir_dual(ref_finddir, ref_finddir);
  aom_usec_timer_mark(&ref_timer);
  const double ref_elapsed_time =
      static_cast<double>(aom_usec_timer_elapsed(&ref_timer));

  aom_usec_timer_start(&timer);
  test_finddir_dual(finddir, finddir);
  aom_usec_timer_mark(&timer);
  const double elapsed_time =
      static_cast<double>(aom_usec_timer_elapsed(&timer));

  printf(
      "ref_time=%lf \t simd_time=%lf \t "
      "gain=%lf \n",
      ref_elapsed_time, elapsed_time, ref_elapsed_time / elapsed_time);
}

#define MAX_CDEF_BLOCK 256

constexpr int kIterations = 100;

using CDEFCopyRect8To16 = void (*)(uint16_t *dst, int dstride,
                                   const uint8_t *src, int sstride, int width,
                                   int height);

using CDEFCopyRect8To16Param = std::tuple<CDEFCopyRect8To16, CDEFCopyRect8To16>;

class CDEFCopyRect8to16Test
    : public ::testing::TestWithParam<CDEFCopyRect8To16Param> {
 public:
  CDEFCopyRect8to16Test()
      : rnd_(libaom_test::ACMRandom::DeterministicSeed()),
        test_func_(GET_PARAM(0)), ref_func_(GET_PARAM(1)) {}
  ~CDEFCopyRect8to16Test() override = default;
  void SetUp() override {
    src_ = reinterpret_cast<uint8_t *>(
        aom_memalign(8, sizeof(uint8_t) * MAX_CDEF_BLOCK * MAX_CDEF_BLOCK));
    ASSERT_NE(src_, nullptr);
    ref_dst_ = reinterpret_cast<uint16_t *>(
        aom_memalign(16, sizeof(uint16_t) * MAX_CDEF_BLOCK * MAX_CDEF_BLOCK));
    ASSERT_NE(ref_dst_, nullptr);
    test_dst_ = reinterpret_cast<uint16_t *>(
        aom_memalign(16, sizeof(uint16_t) * MAX_CDEF_BLOCK * MAX_CDEF_BLOCK));
    ASSERT_NE(test_dst_, nullptr);
  }

  void TearDown() override {
    aom_free(src_);
    aom_free(ref_dst_);
    aom_free(test_dst_);
  }

  void test_copy_rect_8_to_16(CDEFCopyRect8To16 test_func,
                              CDEFCopyRect8To16 ref_func) {
    constexpr int stride = MAX_CDEF_BLOCK;
    int error = 0;
    for (int k = 0; k < kIterations && !error; k++) {
      // This function operates on values of width that are either 4 or a
      // multiple of 8. For height, generate a random value between 1 and 256,
      // making sure it is even.
      const int width = k == 0 ? 4 : (rnd_.Rand8() % 32 + 1) * 8;
      const int height = k == 0 ? 4 : (rnd_.Rand8() % 128 + 1) * 2;
      for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
          src_[i * stride + j] = rnd_.Rand8();
        }
      }

      ref_func(ref_dst_, stride, src_, stride, width, height);
      test_func(test_dst_, stride, src_, stride, width, height);

      int i, j;
      for (i = 0; i < height; i++) {
        for (j = 0; j < width; j++) {
          if (test_dst_[i * stride + j] != ref_dst_[i * stride + j]) {
            error = 1;
            break;
          }
        }
        if (error) {
          break;
        }
      }
      EXPECT_EQ(0, error)
          << "Error: CDEFCopyRect8to16Test, SIMD and C mismatch." << std::endl
          << "First error at " << i << "," << j << " ("
          << ref_dst_[i * stride + j] << " : " << test_dst_[i * stride + j]
          << ") " << std::endl
          << "width: " << width << std::endl
          << "height: " << height << std::endl
          << std::endl;
    }
  }

 protected:
  libaom_test::ACMRandom rnd_;
  uint8_t *src_;
  uint16_t *ref_dst_;
  uint16_t *test_dst_;
  CDEFCopyRect8To16 test_func_;
  CDEFCopyRect8To16 ref_func_;
};
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(CDEFCopyRect8to16Test);

using CDEFCopyRect16To16 = void (*)(uint16_t *dst, int dstride,
                                    const uint16_t *src, int sstride, int width,
                                    int height);

using CDEFCopyRect16To16Param =
    std::tuple<CDEFCopyRect16To16, CDEFCopyRect16To16>;

class CDEFCopyRect16to16Test
    : public ::testing::TestWithParam<CDEFCopyRect16To16Param> {
 public:
  CDEFCopyRect16to16Test()
      : rnd_(libaom_test::ACMRandom::DeterministicSeed()),
        test_func_(GET_PARAM(0)), ref_func_(GET_PARAM(1)) {}
  ~CDEFCopyRect16to16Test() override = default;
  void SetUp() override {
    src_ = reinterpret_cast<uint16_t *>(
        aom_memalign(16, sizeof(uint16_t) * MAX_CDEF_BLOCK * MAX_CDEF_BLOCK));
    ASSERT_NE(src_, nullptr);
    ref_dst_ = reinterpret_cast<uint16_t *>(
        aom_memalign(16, sizeof(uint16_t) * MAX_CDEF_BLOCK * MAX_CDEF_BLOCK));
    ASSERT_NE(ref_dst_, nullptr);
    test_dst_ = reinterpret_cast<uint16_t *>(
        aom_memalign(16, sizeof(uint16_t) * MAX_CDEF_BLOCK * MAX_CDEF_BLOCK));
    ASSERT_NE(test_dst_, nullptr);
  }

  void TearDown() override {
    aom_free(src_);
    aom_free(ref_dst_);
    aom_free(test_dst_);
  }

  void test_copy_rect_16_to_16(CDEFCopyRect16To16 test_func,
                               CDEFCopyRect16To16 ref_func) {
    constexpr int stride = MAX_CDEF_BLOCK;
    int error = 0;
    for (int k = 0; k < kIterations && !error; k++) {
      // This function operates on values of width that are either 4 or a
      // multiple of 8. For height, generate a random value between 1 and 256,
      // making sure it is even.
      const int width = k == 0 ? 4 : (rnd_.Rand8() % 32 + 1) * 8;
      const int height = k == 0 ? 4 : (rnd_.Rand8() % 128 + 1) * 2;
      for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
          src_[i * stride + j] = rnd_.Rand16();
        }
      }

      ref_func(ref_dst_, stride, src_, stride, width, height);
      test_func(test_dst_, stride, src_, stride, width, height);

      int i, j;
      for (i = 0; i < height; i++) {
        for (j = 0; j < width; j++) {
          if (test_dst_[i * stride + j] != ref_dst_[i * stride + j]) {
            error = 1;
            break;
          }
        }
        if (error) {
          break;
        }
      }
      EXPECT_EQ(0, error)
          << "Error: CDEFCopyRect16to16Test, SIMD and C mismatch." << std::endl
          << "First error at " << i << "," << j << " ("
          << ref_dst_[i * stride + j] << " : " << test_dst_[i * stride + j]
          << ") " << std::endl
          << "width: " << width << std::endl
          << "height: " << height << std::endl
          << std::endl;
    }
  }

 protected:
  libaom_test::ACMRandom rnd_;
  uint16_t *src_;
  uint16_t *ref_dst_;
  uint16_t *test_dst_;
  CDEFCopyRect16To16 test_func_;
  CDEFCopyRect16To16 ref_func_;
};
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(CDEFCopyRect16to16Test);

TEST_P(CDEFBlockTest, TestSIMDNoMismatch) {
  test_cdef(bsize, 1, cdef, ref_cdef, boundary, depth);
}

TEST_P(CDEFBlockHighbdTest, TestSIMDHighbdNoMismatch) {
  test_cdef(bsize, 1, cdef, ref_cdef, boundary, depth);
}

TEST_P(CDEFSpeedTest, DISABLED_TestSpeed) {
  test_cdef_speed(bsize, 4, cdef, ref_cdef, boundary, depth);
}

TEST_P(CDEFSpeedHighbdTest, DISABLED_TestSpeed) {
  test_cdef_speed(bsize, 4, cdef, ref_cdef, boundary, depth);
}

TEST_P(CDEFFindDirTest, TestSIMDNoMismatch) {
  test_finddir(finddir, ref_finddir);
}

TEST_P(CDEFFindDirSpeedTest, DISABLED_TestSpeed) {
  test_finddir_speed(finddir, ref_finddir);
}

TEST_P(CDEFFindDirDualTest, TestSIMDNoMismatch) {
  test_finddir_dual(finddir, ref_finddir);
}

TEST_P(CDEFFindDirDualSpeedTest, DISABLED_TestSpeed) {
  test_finddir_dual_speed(finddir, ref_finddir);
}

TEST_P(CDEFCopyRect8to16Test, TestSIMDNoMismatch) {
  test_copy_rect_8_to_16(test_func_, ref_func_);
}

TEST_P(CDEFCopyRect16to16Test, TestSIMDNoMismatch) {
  test_copy_rect_16_to_16(test_func_, ref_func_);
}

using std::make_tuple;

#if ((AOM_ARCH_X86 && HAVE_SSSE3) || HAVE_SSE4_1 || HAVE_AVX2 || HAVE_NEON)
static const CdefFilterBlockFunctions kCdefFilterFuncC[] = {
  { &cdef_filter_8_0_c, &cdef_filter_8_1_c, &cdef_filter_8_2_c,
    &cdef_filter_8_3_c }
};

static const CdefFilterBlockFunctions kCdefFilterHighbdFuncC[] = {
  { &cdef_filter_16_0_c, &cdef_filter_16_0_c, &cdef_filter_16_0_c,
    &cdef_filter_16_0_c }
};
#endif

#if AOM_ARCH_X86 && HAVE_SSSE3
static const CdefFilterBlockFunctions kCdefFilterFuncSsse3[] = {
  { &cdef_filter_8_0_ssse3, &cdef_filter_8_1_ssse3, &cdef_filter_8_2_ssse3,
    &cdef_filter_8_3_ssse3 }
};

static const CdefFilterBlockFunctions kCdefFilterHighbdFuncSsse3[] = {
  { &cdef_filter_16_0_ssse3, &cdef_filter_16_1_ssse3, &cdef_filter_16_2_ssse3,
    &cdef_filter_16_3_ssse3 }
};

INSTANTIATE_TEST_SUITE_P(
    SSSE3, CDEFBlockTest,
    ::testing::Combine(::testing::ValuesIn(kCdefFilterFuncSsse3),
                       ::testing::ValuesIn(kCdefFilterFuncC),
                       ::testing::Values(BLOCK_4X4, BLOCK_4X8, BLOCK_8X4,
                                         BLOCK_8X8),
                       ::testing::Range(0, 16), ::testing::Values(8)));
INSTANTIATE_TEST_SUITE_P(
    SSSE3, CDEFBlockHighbdTest,
    ::testing::Combine(::testing::ValuesIn(kCdefFilterHighbdFuncSsse3),
                       ::testing::ValuesIn(kCdefFilterHighbdFuncC),
                       ::testing::Values(BLOCK_4X4, BLOCK_4X8, BLOCK_8X4,
                                         BLOCK_8X8),
                       ::testing::Range(0, 16), ::testing::Range(10, 13, 2)));
INSTANTIATE_TEST_SUITE_P(SSSE3, CDEFFindDirTest,
                         ::testing::Values(make_tuple(&cdef_find_dir_ssse3,
                                                      &cdef_find_dir_c)));
INSTANTIATE_TEST_SUITE_P(SSSE3, CDEFFindDirDualTest,
                         ::testing::Values(make_tuple(&cdef_find_dir_dual_ssse3,
                                                      &cdef_find_dir_dual_c)));

INSTANTIATE_TEST_SUITE_P(
    SSSE3, CDEFCopyRect8to16Test,
    ::testing::Values(make_tuple(&cdef_copy_rect8_8bit_to_16bit_c,
                                 &cdef_copy_rect8_8bit_to_16bit_ssse3)));

INSTANTIATE_TEST_SUITE_P(
    SSSE3, CDEFCopyRect16to16Test,
    ::testing::Values(make_tuple(&cdef_copy_rect8_16bit_to_16bit_c,
                                 &cdef_copy_rect8_16bit_to_16bit_ssse3)));
#endif

#if HAVE_SSE4_1
static const CdefFilterBlockFunctions kCdefFilterFuncSse4_1[] = {
  { &cdef_filter_8_0_sse4_1, &cdef_filter_8_1_sse4_1, &cdef_filter_8_2_sse4_1,
    &cdef_filter_8_3_sse4_1 }
};

static const CdefFilterBlockFunctions kCdefFilterHighbdFuncSse4_1[] = {
  { &cdef_filter_16_0_sse4_1, &cdef_filter_16_1_sse4_1,
    &cdef_filter_16_2_sse4_1, &cdef_filter_16_3_sse4_1 }
};

INSTANTIATE_TEST_SUITE_P(
    SSE4_1, CDEFBlockTest,
    ::testing::Combine(::testing::ValuesIn(kCdefFilterFuncSse4_1),
                       ::testing::ValuesIn(kCdefFilterFuncC),
                       ::testing::Values(BLOCK_4X4, BLOCK_4X8, BLOCK_8X4,
                                         BLOCK_8X8),
                       ::testing::Range(0, 16), ::testing::Values(8)));
INSTANTIATE_TEST_SUITE_P(
    SSE4_1, CDEFBlockHighbdTest,
    ::testing::Combine(::testing::ValuesIn(kCdefFilterHighbdFuncSse4_1),
                       ::testing::ValuesIn(kCdefFilterHighbdFuncC),
                       ::testing::Values(BLOCK_4X4, BLOCK_4X8, BLOCK_8X4,
                                         BLOCK_8X8),
                       ::testing::Range(0, 16), ::testing::Range(10, 13, 2)));
INSTANTIATE_TEST_SUITE_P(SSE4_1, CDEFFindDirTest,
                         ::testing::Values(make_tuple(&cdef_find_dir_sse4_1,
                                                      &cdef_find_dir_c)));
INSTANTIATE_TEST_SUITE_P(
    SSE4_1, CDEFFindDirDualTest,
    ::testing::Values(make_tuple(&cdef_find_dir_dual_sse4_1,
                                 &cdef_find_dir_dual_c)));

INSTANTIATE_TEST_SUITE_P(
    SSE4_1, CDEFCopyRect8to16Test,
    ::testing::Values(make_tuple(&cdef_copy_rect8_8bit_to_16bit_c,
                                 &cdef_copy_rect8_8bit_to_16bit_sse4_1)));

INSTANTIATE_TEST_SUITE_P(
    SSE4_1, CDEFCopyRect16to16Test,
    ::testing::Values(make_tuple(&cdef_copy_rect8_16bit_to_16bit_c,
                                 &cdef_copy_rect8_16bit_to_16bit_sse4_1)));
#endif

#if HAVE_AVX2
static const CdefFilterBlockFunctions kCdefFilterFuncAvx2[] = {
  { &cdef_filter_8_0_avx2, &cdef_filter_8_1_avx2, &cdef_filter_8_2_avx2,
    &cdef_filter_8_3_avx2 }
};

static const CdefFilterBlockFunctions kCdefFilterHighbdFuncAvx2[] = {
  { &cdef_filter_16_0_avx2, &cdef_filter_16_1_avx2, &cdef_filter_16_2_avx2,
    &cdef_filter_16_3_avx2 }
};

INSTANTIATE_TEST_SUITE_P(
    AVX2, CDEFBlockTest,
    ::testing::Combine(::testing::ValuesIn(kCdefFilterFuncAvx2),
                       ::testing::ValuesIn(kCdefFilterFuncC),
                       ::testing::Values(BLOCK_4X4, BLOCK_4X8, BLOCK_8X4,
                                         BLOCK_8X8),
                       ::testing::Range(0, 16), ::testing::Values(8)));
INSTANTIATE_TEST_SUITE_P(
    AVX2, CDEFBlockHighbdTest,
    ::testing::Combine(::testing::ValuesIn(kCdefFilterHighbdFuncAvx2),
                       ::testing::ValuesIn(kCdefFilterHighbdFuncC),
                       ::testing::Values(BLOCK_4X4, BLOCK_4X8, BLOCK_8X4,
                                         BLOCK_8X8),
                       ::testing::Range(0, 16), ::testing::Range(10, 13, 2)));
INSTANTIATE_TEST_SUITE_P(AVX2, CDEFFindDirTest,
                         ::testing::Values(make_tuple(&cdef_find_dir_avx2,
                                                      &cdef_find_dir_c)));
INSTANTIATE_TEST_SUITE_P(AVX2, CDEFFindDirDualTest,
                         ::testing::Values(make_tuple(&cdef_find_dir_dual_avx2,
                                                      &cdef_find_dir_dual_c)));

INSTANTIATE_TEST_SUITE_P(
    AVX2, CDEFCopyRect8to16Test,
    ::testing::Values(make_tuple(&cdef_copy_rect8_8bit_to_16bit_c,
                                 &cdef_copy_rect8_8bit_to_16bit_avx2)));

INSTANTIATE_TEST_SUITE_P(
    AVX2, CDEFCopyRect16to16Test,
    ::testing::Values(make_tuple(&cdef_copy_rect8_16bit_to_16bit_c,
                                 &cdef_copy_rect8_16bit_to_16bit_avx2)));
#endif

#if HAVE_NEON
static const CdefFilterBlockFunctions kCdefFilterFuncNeon[] = {
  { &cdef_filter_8_0_neon, &cdef_filter_8_1_neon, &cdef_filter_8_2_neon,
    &cdef_filter_8_3_neon }
};

static const CdefFilterBlockFunctions kCdefFilterHighbdFuncNeon[] = {
  { &cdef_filter_16_0_neon, &cdef_filter_16_1_neon, &cdef_filter_16_2_neon,
    &cdef_filter_16_3_neon }
};

INSTANTIATE_TEST_SUITE_P(
    NEON, CDEFBlockTest,
    ::testing::Combine(::testing::ValuesIn(kCdefFilterFuncNeon),
                       ::testing::ValuesIn(kCdefFilterFuncC),
                       ::testing::Values(BLOCK_4X4, BLOCK_4X8, BLOCK_8X4,
                                         BLOCK_8X8),
                       ::testing::Range(0, 16), ::testing::Values(8)));
INSTANTIATE_TEST_SUITE_P(
    NEON, CDEFBlockHighbdTest,
    ::testing::Combine(::testing::ValuesIn(kCdefFilterHighbdFuncNeon),
                       ::testing::ValuesIn(kCdefFilterHighbdFuncC),
                       ::testing::Values(BLOCK_4X4, BLOCK_4X8, BLOCK_8X4,
                                         BLOCK_8X8),
                       ::testing::Range(0, 16), ::testing::Range(10, 13, 2)));
INSTANTIATE_TEST_SUITE_P(NEON, CDEFFindDirTest,
                         ::testing::Values(make_tuple(&cdef_find_dir_neon,
                                                      &cdef_find_dir_c)));
INSTANTIATE_TEST_SUITE_P(NEON, CDEFFindDirDualTest,
                         ::testing::Values(make_tuple(&cdef_find_dir_dual_neon,
                                                      &cdef_find_dir_dual_c)));

INSTANTIATE_TEST_SUITE_P(
    NEON, CDEFCopyRect8to16Test,
    ::testing::Values(make_tuple(&cdef_copy_rect8_8bit_to_16bit_c,
                                 &cdef_copy_rect8_8bit_to_16bit_neon)));

INSTANTIATE_TEST_SUITE_P(
    NEON, CDEFCopyRect16to16Test,
    ::testing::Values(make_tuple(&cdef_copy_rect8_16bit_to_16bit_c,
                                 &cdef_copy_rect8_16bit_to_16bit_neon)));
#endif

// Test speed for all supported architectures
#if AOM_ARCH_X86 && HAVE_SSSE3
INSTANTIATE_TEST_SUITE_P(
    SSSE3, CDEFSpeedTest,
    ::testing::Combine(::testing::ValuesIn(kCdefFilterFuncSsse3),
                       ::testing::ValuesIn(kCdefFilterFuncC),
                       ::testing::Values(BLOCK_4X4, BLOCK_4X8, BLOCK_8X4,
                                         BLOCK_8X8),
                       ::testing::Range(0, 16), ::testing::Values(8)));
INSTANTIATE_TEST_SUITE_P(
    SSSE3, CDEFSpeedHighbdTest,
    ::testing::Combine(::testing::ValuesIn(kCdefFilterHighbdFuncSsse3),
                       ::testing::ValuesIn(kCdefFilterHighbdFuncC),
                       ::testing::Values(BLOCK_4X4, BLOCK_4X8, BLOCK_8X4,
                                         BLOCK_8X8),
                       ::testing::Range(0, 16), ::testing::Values(10)));
INSTANTIATE_TEST_SUITE_P(SSSE3, CDEFFindDirSpeedTest,
                         ::testing::Values(make_tuple(&cdef_find_dir_ssse3,
                                                      &cdef_find_dir_c)));
INSTANTIATE_TEST_SUITE_P(SSSE3, CDEFFindDirDualSpeedTest,
                         ::testing::Values(make_tuple(&cdef_find_dir_dual_ssse3,
                                                      &cdef_find_dir_dual_c)));
#endif

#if HAVE_SSE4_1
INSTANTIATE_TEST_SUITE_P(
    SSE4_1, CDEFSpeedTest,
    ::testing::Combine(::testing::ValuesIn(kCdefFilterFuncSse4_1),
                       ::testing::ValuesIn(kCdefFilterFuncC),
                       ::testing::Values(BLOCK_4X4, BLOCK_4X8, BLOCK_8X4,
                                         BLOCK_8X8),
                       ::testing::Range(0, 16), ::testing::Values(8)));
INSTANTIATE_TEST_SUITE_P(
    SSE4_1, CDEFSpeedHighbdTest,
    ::testing::Combine(::testing::ValuesIn(kCdefFilterHighbdFuncSse4_1),
                       ::testing::ValuesIn(kCdefFilterHighbdFuncC),
                       ::testing::Values(BLOCK_4X4, BLOCK_4X8, BLOCK_8X4,
                                         BLOCK_8X8),
                       ::testing::Range(0, 16), ::testing::Values(10)));
INSTANTIATE_TEST_SUITE_P(SSE4_1, CDEFFindDirSpeedTest,
                         ::testing::Values(make_tuple(&cdef_find_dir_sse4_1,
                                                      &cdef_find_dir_c)));
INSTANTIATE_TEST_SUITE_P(
    SSE4_1, CDEFFindDirDualSpeedTest,
    ::testing::Values(make_tuple(&cdef_find_dir_dual_sse4_1,
                                 &cdef_find_dir_dual_c)));
#endif

#if HAVE_AVX2
INSTANTIATE_TEST_SUITE_P(
    AVX2, CDEFSpeedTest,
    ::testing::Combine(::testing::ValuesIn(kCdefFilterFuncAvx2),
                       ::testing::ValuesIn(kCdefFilterFuncC),
                       ::testing::Values(BLOCK_4X4, BLOCK_4X8, BLOCK_8X4,
                                         BLOCK_8X8),
                       ::testing::Range(0, 16), ::testing::Values(8)));
INSTANTIATE_TEST_SUITE_P(
    AVX2, CDEFSpeedHighbdTest,
    ::testing::Combine(::testing::ValuesIn(kCdefFilterHighbdFuncAvx2),
                       ::testing::ValuesIn(kCdefFilterHighbdFuncC),
                       ::testing::Values(BLOCK_4X4, BLOCK_4X8, BLOCK_8X4,
                                         BLOCK_8X8),
                       ::testing::Range(0, 16), ::testing::Values(10)));
INSTANTIATE_TEST_SUITE_P(AVX2, CDEFFindDirSpeedTest,
                         ::testing::Values(make_tuple(&cdef_find_dir_avx2,
                                                      &cdef_find_dir_c)));
INSTANTIATE_TEST_SUITE_P(AVX2, CDEFFindDirDualSpeedTest,
                         ::testing::Values(make_tuple(&cdef_find_dir_dual_avx2,
                                                      &cdef_find_dir_dual_c)));
#endif

#if HAVE_NEON
INSTANTIATE_TEST_SUITE_P(
    NEON, CDEFSpeedTest,
    ::testing::Combine(::testing::ValuesIn(kCdefFilterFuncNeon),
                       ::testing::ValuesIn(kCdefFilterFuncC),
                       ::testing::Values(BLOCK_4X4, BLOCK_4X8, BLOCK_8X4,
                                         BLOCK_8X8),
                       ::testing::Range(0, 16), ::testing::Values(8)));
INSTANTIATE_TEST_SUITE_P(
    NEON, CDEFSpeedHighbdTest,
    ::testing::Combine(::testing::ValuesIn(kCdefFilterHighbdFuncNeon),
                       ::testing::ValuesIn(kCdefFilterHighbdFuncC),
                       ::testing::Values(BLOCK_4X4, BLOCK_4X8, BLOCK_8X4,
                                         BLOCK_8X8),
                       ::testing::Range(0, 16), ::testing::Values(10)));
INSTANTIATE_TEST_SUITE_P(NEON, CDEFFindDirSpeedTest,
                         ::testing::Values(make_tuple(&cdef_find_dir_neon,
                                                      &cdef_find_dir_c)));
INSTANTIATE_TEST_SUITE_P(NEON, CDEFFindDirDualSpeedTest,
                         ::testing::Values(make_tuple(&cdef_find_dir_dual_neon,
                                                      &cdef_find_dir_dual_c)));
#endif

}  // namespace
