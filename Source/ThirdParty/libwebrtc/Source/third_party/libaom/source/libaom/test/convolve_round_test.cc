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

#include <assert.h>
#include <tuple>

#include "config/av1_rtcd.h"

#include "aom/aom_integer.h"
#include "aom_ports/aom_timer.h"
#include "test/acm_random.h"
#include "test/register_state_check.h"
#include "test/util.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

using libaom_test::ACMRandom;

namespace {
#define CONVOLVE_ROUNDING_PARAM                                            \
  const int32_t *src, int src_stride, uint8_t *dst, int dst_stride, int w, \
      int h, int bits

typedef void (*ConvolveRoundFunc)(CONVOLVE_ROUNDING_PARAM);

typedef void (*ConvolveRoundFuncHbd)(CONVOLVE_ROUNDING_PARAM, int bd);

template <ConvolveRoundFuncHbd fn>
void highbd_convolve_rounding_8(CONVOLVE_ROUNDING_PARAM) {
  const int bd = 8;
  fn(src, src_stride, dst, dst_stride, w, h, bits, bd);
}

template <ConvolveRoundFuncHbd fn>
void highbd_convolve_rounding_10(CONVOLVE_ROUNDING_PARAM) {
  const int bd = 10;
  fn(src, src_stride, dst, dst_stride, w, h, bits, bd);
}

template <ConvolveRoundFuncHbd fn>
void highbd_convolve_rounding_12(CONVOLVE_ROUNDING_PARAM) {
  const int bd = 12;
  fn(src, src_stride, dst, dst_stride, w, h, bits, bd);
}

typedef enum { LOWBITDEPTH_TEST, HIGHBITDEPTH_TEST } DataPathType;

using std::tuple;

typedef tuple<ConvolveRoundFunc, ConvolveRoundFunc, DataPathType>
    ConvolveRoundParam;

const int kTestNum = 5000;

class ConvolveRoundTest : public ::testing::TestWithParam<ConvolveRoundParam> {
 protected:
  ConvolveRoundTest()
      : func_ref_(GET_PARAM(0)), func_(GET_PARAM(1)), data_path_(GET_PARAM(2)) {
  }
  virtual ~ConvolveRoundTest() {}

  virtual void SetUp() {
    const size_t block_size = 128 * 128;
    src_ = reinterpret_cast<int32_t *>(
        aom_memalign(16, block_size * sizeof(*src_)));
    ASSERT_NE(src_, nullptr);
    dst_ref_ = reinterpret_cast<uint16_t *>(
        aom_memalign(16, block_size * sizeof(*dst_ref_)));
    ASSERT_NE(dst_ref_, nullptr);
    dst_ = reinterpret_cast<uint16_t *>(
        aom_memalign(16, block_size * sizeof(*dst_)));
    ASSERT_NE(dst_, nullptr);
  }

  virtual void TearDown() {
    aom_free(src_);
    aom_free(dst_ref_);
    aom_free(dst_);
  }

  void ConvolveRoundingRun() {
    int test_num = 0;
    const int src_stride = 128;
    const int dst_stride = 128;
    int bits = 13;
    uint8_t *dst = 0;
    uint8_t *dst_ref = 0;

    if (data_path_ == LOWBITDEPTH_TEST) {
      dst = reinterpret_cast<uint8_t *>(dst_);
      dst_ref = reinterpret_cast<uint8_t *>(dst_ref_);
    } else if (data_path_ == HIGHBITDEPTH_TEST) {
      dst = CONVERT_TO_BYTEPTR(dst_);
      dst_ref = CONVERT_TO_BYTEPTR(dst_ref_);
    } else {
      assert(0);
    }

    while (test_num < kTestNum) {
      int block_size = test_num % BLOCK_SIZES_ALL;
      int w = block_size_wide[block_size];
      int h = block_size_high[block_size];

      if (test_num % 2 == 0)
        bits -= 1;
      else
        bits += 1;

      GenerateBufferWithRandom(src_, src_stride, bits, w, h);

      func_ref_(src_, src_stride, dst_ref, dst_stride, w, h, bits);
      API_REGISTER_STATE_CHECK(
          func_(src_, src_stride, dst, dst_stride, w, h, bits));

      if (data_path_ == LOWBITDEPTH_TEST) {
        for (int r = 0; r < h; ++r) {
          for (int c = 0; c < w; ++c) {
            ASSERT_EQ(dst_ref[r * dst_stride + c], dst[r * dst_stride + c])
                << "Mismatch at r: " << r << " c: " << c << " w: " << w
                << " h: " << h << " test: " << test_num;
          }
        }
      } else {
        for (int r = 0; r < h; ++r) {
          for (int c = 0; c < w; ++c) {
            ASSERT_EQ(dst_ref_[r * dst_stride + c], dst_[r * dst_stride + c])
                << "Mismatch at r: " << r << " c: " << c << " w: " << w
                << " h: " << h << " test: " << test_num;
          }
        }
      }

      test_num++;
    }
  }

  void GenerateBufferWithRandom(int32_t *src, int src_stride, int bits, int w,
                                int h) {
    int32_t number;
    for (int r = 0; r < h; ++r) {
      for (int c = 0; c < w; ++c) {
        number = static_cast<int32_t>(rand_.Rand31());
        number %= 1 << (bits + 9);
        src[r * src_stride + c] = number;
      }
    }
  }

  ACMRandom rand_;
  int32_t *src_;
  uint16_t *dst_ref_;
  uint16_t *dst_;

  ConvolveRoundFunc func_ref_;
  ConvolveRoundFunc func_;
  DataPathType data_path_;
};

TEST_P(ConvolveRoundTest, BitExactCheck) { ConvolveRoundingRun(); }

using std::make_tuple;
#if HAVE_AVX2
const ConvolveRoundParam kConvRndParamArray[] = {
  make_tuple(&av1_convolve_rounding_c, &av1_convolve_rounding_avx2,
             LOWBITDEPTH_TEST),
  make_tuple(&highbd_convolve_rounding_8<av1_highbd_convolve_rounding_c>,
             &highbd_convolve_rounding_8<av1_highbd_convolve_rounding_avx2>,
             HIGHBITDEPTH_TEST),
  make_tuple(&highbd_convolve_rounding_10<av1_highbd_convolve_rounding_c>,
             &highbd_convolve_rounding_10<av1_highbd_convolve_rounding_avx2>,
             HIGHBITDEPTH_TEST),
  make_tuple(&highbd_convolve_rounding_12<av1_highbd_convolve_rounding_c>,
             &highbd_convolve_rounding_12<av1_highbd_convolve_rounding_avx2>,
             HIGHBITDEPTH_TEST)
};
INSTANTIATE_TEST_SUITE_P(AVX2, ConvolveRoundTest,
                         ::testing::ValuesIn(kConvRndParamArray));
#endif  // HAVE_AVX2
}  // namespace
