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

#include <string.h>
#include <limits.h>
#include <stdio.h>
#include <tuple>

#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"

#include "test/acm_random.h"
#include "test/register_state_check.h"
#include "test/util.h"
#include "aom/aom_codec.h"
#include "aom_mem/aom_mem.h"
#include "aom_ports/mem.h"

typedef unsigned int (*SadMxNFunc)(const uint8_t *src_ptr, int src_stride,
                                   const uint8_t *ref_ptr, int ref_stride);
typedef std::tuple<int, int, SadMxNFunc, int> SadMxNParam;

typedef unsigned int (*SadSkipMxNFunc)(const uint8_t *src_ptr, int src_stride,
                                       const uint8_t *ref_ptr, int ref_stride);
typedef std::tuple<int, int, SadSkipMxNFunc, int> SadSkipMxNParam;

typedef uint32_t (*SadMxNAvgFunc)(const uint8_t *src_ptr, int src_stride,
                                  const uint8_t *ref_ptr, int ref_stride,
                                  const uint8_t *second_pred);
typedef std::tuple<int, int, SadMxNAvgFunc, int> SadMxNAvgParam;

typedef void (*DistWtdCompAvgFunc)(uint8_t *comp_pred, const uint8_t *pred,
                                   int width, int height, const uint8_t *ref,
                                   int ref_stride,
                                   const DIST_WTD_COMP_PARAMS *jcp_param);
typedef std::tuple<int, int, DistWtdCompAvgFunc, int> DistWtdCompAvgParam;

typedef unsigned int (*DistWtdSadMxhFunc)(const uint8_t *src_ptr,
                                          int src_stride,
                                          const uint8_t *ref_ptr,
                                          int ref_stride, int width,
                                          int height);
typedef std::tuple<int, int, DistWtdSadMxhFunc, int> DistWtdSadMxhParam;

typedef uint32_t (*DistWtdSadMxNAvgFunc)(const uint8_t *src_ptr, int src_stride,
                                         const uint8_t *ref_ptr, int ref_stride,
                                         const uint8_t *second_pred,
                                         const DIST_WTD_COMP_PARAMS *jcp_param);
typedef std::tuple<int, int, DistWtdSadMxNAvgFunc, int> DistWtdSadMxNAvgParam;

typedef void (*SadMxNx4Func)(const uint8_t *src_ptr, int src_stride,
                             const uint8_t *const ref_ptr[], int ref_stride,
                             uint32_t *sad_array);
typedef std::tuple<int, int, SadMxNx4Func, int> SadMxNx4Param;

typedef void (*SadSkipMxNx4Func)(const uint8_t *src_ptr, int src_stride,
                                 const uint8_t *const ref_ptr[], int ref_stride,
                                 uint32_t *sad_array);
typedef std::tuple<int, int, SadSkipMxNx4Func, int> SadSkipMxNx4Param;

typedef void (*SadMxNx4AvgFunc)(const uint8_t *src_ptr, int src_stride,
                                const uint8_t *const ref_ptr[], int ref_stride,
                                const uint8_t *second_pred,
                                uint32_t *sad_array);
typedef std::tuple<int, int, SadMxNx4AvgFunc, int> SadMxNx4AvgParam;

using libaom_test::ACMRandom;

namespace {
class SADTestBase : public ::testing::Test {
 public:
  SADTestBase(int width, int height, int bit_depth)
      : width_(width), height_(height), bd_(bit_depth) {}

  static void SetUpTestSuite() {
    source_data8_ = reinterpret_cast<uint8_t *>(
        aom_memalign(kDataAlignment, kDataBlockSize));
    ASSERT_NE(source_data8_, nullptr);
    reference_data8_ = reinterpret_cast<uint8_t *>(
        aom_memalign(kDataAlignment, kDataBufferSize));
    ASSERT_NE(reference_data8_, nullptr);
    second_pred8_ =
        reinterpret_cast<uint8_t *>(aom_memalign(kDataAlignment, 128 * 128));
    ASSERT_NE(second_pred8_, nullptr);
    comp_pred8_ =
        reinterpret_cast<uint8_t *>(aom_memalign(kDataAlignment, 128 * 128));
    ASSERT_NE(comp_pred8_, nullptr);
    comp_pred8_test_ =
        reinterpret_cast<uint8_t *>(aom_memalign(kDataAlignment, 128 * 128));
    ASSERT_NE(comp_pred8_test_, nullptr);
    source_data16_ = reinterpret_cast<uint16_t *>(
        aom_memalign(kDataAlignment, kDataBlockSize * sizeof(uint16_t)));
    ASSERT_NE(source_data16_, nullptr);
    reference_data16_ = reinterpret_cast<uint16_t *>(
        aom_memalign(kDataAlignment, kDataBufferSize * sizeof(uint16_t)));
    ASSERT_NE(reference_data16_, nullptr);
    second_pred16_ = reinterpret_cast<uint16_t *>(
        aom_memalign(kDataAlignment, 128 * 128 * sizeof(uint16_t)));
    ASSERT_NE(second_pred16_, nullptr);
    comp_pred16_ = reinterpret_cast<uint16_t *>(
        aom_memalign(kDataAlignment, 128 * 128 * sizeof(uint16_t)));
    ASSERT_NE(comp_pred16_, nullptr);
    comp_pred16_test_ = reinterpret_cast<uint16_t *>(
        aom_memalign(kDataAlignment, 128 * 128 * sizeof(uint16_t)));
    ASSERT_NE(comp_pred16_test_, nullptr);
  }

  static void TearDownTestSuite() {
    aom_free(source_data8_);
    source_data8_ = nullptr;
    aom_free(reference_data8_);
    reference_data8_ = nullptr;
    aom_free(second_pred8_);
    second_pred8_ = nullptr;
    aom_free(comp_pred8_);
    comp_pred8_ = nullptr;
    aom_free(comp_pred8_test_);
    comp_pred8_test_ = nullptr;
    aom_free(source_data16_);
    source_data16_ = nullptr;
    aom_free(reference_data16_);
    reference_data16_ = nullptr;
    aom_free(second_pred16_);
    second_pred16_ = nullptr;
    aom_free(comp_pred16_);
    comp_pred16_ = nullptr;
    aom_free(comp_pred16_test_);
    comp_pred16_test_ = nullptr;
  }

  virtual void TearDown() {}

 protected:
  // Handle up to 4 128x128 blocks, with stride up to 256
  static const int kDataAlignment = 16;
  static const int kDataBlockSize = 128 * 256;
  static const int kDataBufferSize = 4 * kDataBlockSize;

  virtual void SetUp() {
    if (bd_ == -1) {
      use_high_bit_depth_ = false;
      bit_depth_ = AOM_BITS_8;
      source_data_ = source_data8_;
      reference_data_ = reference_data8_;
      second_pred_ = second_pred8_;
      comp_pred_ = comp_pred8_;
      comp_pred_test_ = comp_pred8_test_;
    } else {
      use_high_bit_depth_ = true;
      bit_depth_ = static_cast<aom_bit_depth_t>(bd_);
      source_data_ = CONVERT_TO_BYTEPTR(source_data16_);
      reference_data_ = CONVERT_TO_BYTEPTR(reference_data16_);
      second_pred_ = CONVERT_TO_BYTEPTR(second_pred16_);
      comp_pred_ = CONVERT_TO_BYTEPTR(comp_pred16_);
      comp_pred_test_ = CONVERT_TO_BYTEPTR(comp_pred16_test_);
    }
    mask_ = (1 << bit_depth_) - 1;
    source_stride_ = (width_ + 31) & ~31;
    reference_stride_ = width_ * 2;
    rnd_.Reset(ACMRandom::DeterministicSeed());
  }

  virtual uint8_t *GetReference(int block_idx) {
    if (use_high_bit_depth_)
      return CONVERT_TO_BYTEPTR(CONVERT_TO_SHORTPTR(reference_data_) +
                                block_idx * kDataBlockSize);
    return reference_data_ + block_idx * kDataBlockSize;
  }

  // Sum of Absolute Differences. Given two blocks, calculate the absolute
  // difference between two pixels in the same relative location; accumulate.
  unsigned int ReferenceSAD(int block_idx) {
    unsigned int sad = 0;
    const uint8_t *const reference8 = GetReference(block_idx);
    const uint8_t *const source8 = source_data_;
    const uint16_t *const reference16 =
        CONVERT_TO_SHORTPTR(GetReference(block_idx));
    const uint16_t *const source16 = CONVERT_TO_SHORTPTR(source_data_);
    for (int h = 0; h < height_; ++h) {
      for (int w = 0; w < width_; ++w) {
        if (!use_high_bit_depth_) {
          sad += abs(source8[h * source_stride_ + w] -
                     reference8[h * reference_stride_ + w]);
        } else {
          sad += abs(source16[h * source_stride_ + w] -
                     reference16[h * reference_stride_ + w]);
        }
      }
    }
    return sad;
  }

  // Sum of Absolute Differences Skip rows. Given two blocks,
  // calculate the absolute  difference between two pixels in the same
  // relative location every other row; accumulate and double the result at the
  // end.
  unsigned int ReferenceSADSkip(int block_idx) {
    unsigned int sad = 0;
    const uint8_t *const reference8 = GetReference(block_idx);
    const uint8_t *const source8 = source_data_;
    const uint16_t *const reference16 =
        CONVERT_TO_SHORTPTR(GetReference(block_idx));
    const uint16_t *const source16 = CONVERT_TO_SHORTPTR(source_data_);
    for (int h = 0; h < height_; h += 2) {
      for (int w = 0; w < width_; ++w) {
        if (!use_high_bit_depth_) {
          sad += abs(source8[h * source_stride_ + w] -
                     reference8[h * reference_stride_ + w]);
        } else {
          sad += abs(source16[h * source_stride_ + w] -
                     reference16[h * reference_stride_ + w]);
        }
      }
    }
    return sad * 2;
  }

  // Sum of Absolute Differences Average. Given two blocks, and a prediction
  // calculate the absolute difference between one pixel and average of the
  // corresponding and predicted pixels; accumulate.
  unsigned int ReferenceSADavg(int block_idx) {
    unsigned int sad = 0;
    const uint8_t *const reference8 = GetReference(block_idx);
    const uint8_t *const source8 = source_data_;
    const uint8_t *const second_pred8 = second_pred_;
    const uint16_t *const reference16 =
        CONVERT_TO_SHORTPTR(GetReference(block_idx));
    const uint16_t *const source16 = CONVERT_TO_SHORTPTR(source_data_);
    const uint16_t *const second_pred16 = CONVERT_TO_SHORTPTR(second_pred_);
    for (int h = 0; h < height_; ++h) {
      for (int w = 0; w < width_; ++w) {
        if (!use_high_bit_depth_) {
          const int tmp = second_pred8[h * width_ + w] +
                          reference8[h * reference_stride_ + w];
          const uint8_t comp_pred = ROUND_POWER_OF_TWO(tmp, 1);
          sad += abs(source8[h * source_stride_ + w] - comp_pred);
        } else {
          const int tmp = second_pred16[h * width_ + w] +
                          reference16[h * reference_stride_ + w];
          const uint16_t comp_pred = ROUND_POWER_OF_TWO(tmp, 1);
          sad += abs(source16[h * source_stride_ + w] - comp_pred);
        }
      }
    }
    return sad;
  }

  void ReferenceDistWtdCompAvg(int block_idx) {
    const uint8_t *const reference8 = GetReference(block_idx);
    const uint8_t *const second_pred8 = second_pred_;
    uint8_t *const comp_pred8 = comp_pred_;
    const uint16_t *const reference16 =
        CONVERT_TO_SHORTPTR(GetReference(block_idx));
    const uint16_t *const second_pred16 = CONVERT_TO_SHORTPTR(second_pred_);
    uint16_t *const comp_pred16 = CONVERT_TO_SHORTPTR(comp_pred_);
    for (int h = 0; h < height_; ++h) {
      for (int w = 0; w < width_; ++w) {
        if (!use_high_bit_depth_) {
          const int tmp =
              second_pred8[h * width_ + w] * jcp_param_.bck_offset +
              reference8[h * reference_stride_ + w] * jcp_param_.fwd_offset;
          comp_pred8[h * width_ + w] = ROUND_POWER_OF_TWO(tmp, 4);
        } else {
          const int tmp =
              second_pred16[h * width_ + w] * jcp_param_.bck_offset +
              reference16[h * reference_stride_ + w] * jcp_param_.fwd_offset;
          comp_pred16[h * width_ + w] = ROUND_POWER_OF_TWO(tmp, 4);
        }
      }
    }
  }

  unsigned int ReferenceDistWtdSADavg(int block_idx) {
    unsigned int sad = 0;
    const uint8_t *const reference8 = GetReference(block_idx);
    const uint8_t *const source8 = source_data_;
    const uint8_t *const second_pred8 = second_pred_;
    const uint16_t *const reference16 =
        CONVERT_TO_SHORTPTR(GetReference(block_idx));
    const uint16_t *const source16 = CONVERT_TO_SHORTPTR(source_data_);
    const uint16_t *const second_pred16 = CONVERT_TO_SHORTPTR(second_pred_);
    for (int h = 0; h < height_; ++h) {
      for (int w = 0; w < width_; ++w) {
        if (!use_high_bit_depth_) {
          const int tmp =
              second_pred8[h * width_ + w] * jcp_param_.bck_offset +
              reference8[h * reference_stride_ + w] * jcp_param_.fwd_offset;
          const uint8_t comp_pred = ROUND_POWER_OF_TWO(tmp, 4);
          sad += abs(source8[h * source_stride_ + w] - comp_pred);
        } else {
          const int tmp =
              second_pred16[h * width_ + w] * jcp_param_.bck_offset +
              reference16[h * reference_stride_ + w] * jcp_param_.fwd_offset;
          const uint16_t comp_pred = ROUND_POWER_OF_TWO(tmp, 4);
          sad += abs(source16[h * source_stride_ + w] - comp_pred);
        }
      }
    }
    return sad;
  }

  void FillConstant(uint8_t *data, int stride, uint16_t fill_constant) {
    uint8_t *data8 = data;
    uint16_t *data16 = CONVERT_TO_SHORTPTR(data);
    for (int h = 0; h < height_; ++h) {
      for (int w = 0; w < width_; ++w) {
        if (!use_high_bit_depth_) {
          data8[h * stride + w] = static_cast<uint8_t>(fill_constant);
        } else {
          data16[h * stride + w] = fill_constant;
        }
      }
    }
  }

  void FillRandom(uint8_t *data, int stride) {
    uint8_t *data8 = data;
    uint16_t *data16 = CONVERT_TO_SHORTPTR(data);
    for (int h = 0; h < height_; ++h) {
      for (int w = 0; w < width_; ++w) {
        if (!use_high_bit_depth_) {
          data8[h * stride + w] = rnd_.Rand8();
        } else {
          data16[h * stride + w] = rnd_.Rand16() & mask_;
        }
      }
    }
  }

  virtual void SADForSpeedTest(unsigned int *results,
                               const uint8_t *const *references) {
    (void)results;
    (void)references;
  }

  void SpeedSAD() {
    int test_count = 20000000;
    unsigned int exp_sad[4];
    const uint8_t *references[] = { GetReference(0), GetReference(1),
                                    GetReference(2), GetReference(3) };
    aom_usec_timer timer;
    aom_usec_timer_start(&timer);
    while (test_count > 0) {
      SADForSpeedTest(exp_sad, references);
      test_count -= 1;
    }
    aom_usec_timer_mark(&timer);
    const int64_t time = aom_usec_timer_elapsed(&timer) / 1000;
    std::cout << "BLOCK_" << width_ << "X" << height_
              << ", bit_depth:" << bit_depth_ << ",Time: " << time << "ms"
              << std::endl;
  }

  int width_, height_, mask_, bd_;
  aom_bit_depth_t bit_depth_;
  static uint8_t *source_data_;
  static uint8_t *reference_data_;
  static uint8_t *second_pred_;
  int source_stride_;
  bool use_high_bit_depth_;
  static uint8_t *source_data8_;
  static uint8_t *reference_data8_;
  static uint8_t *second_pred8_;
  static uint16_t *source_data16_;
  static uint16_t *reference_data16_;
  static uint16_t *second_pred16_;
  int reference_stride_;
  static uint8_t *comp_pred_;
  static uint8_t *comp_pred8_;
  static uint16_t *comp_pred16_;
  static uint8_t *comp_pred_test_;
  static uint8_t *comp_pred8_test_;
  static uint16_t *comp_pred16_test_;
  DIST_WTD_COMP_PARAMS jcp_param_;

  ACMRandom rnd_;
};

class SADx4Test : public ::testing::WithParamInterface<SadMxNx4Param>,
                  public SADTestBase {
 public:
  SADx4Test() : SADTestBase(GET_PARAM(0), GET_PARAM(1), GET_PARAM(3)) {}

 protected:
  void SADs(unsigned int *results) {
    const uint8_t *references[] = { GetReference(0), GetReference(1),
                                    GetReference(2), GetReference(3) };

    API_REGISTER_STATE_CHECK(GET_PARAM(2)(
        source_data_, source_stride_, references, reference_stride_, results));
  }

  void SADForSpeedTest(unsigned int *results,
                       const uint8_t *const *references) {
    GET_PARAM(2)
    (source_data_, source_stride_, references, reference_stride_, results);
  }

  void CheckSADs() {
    unsigned int reference_sad, exp_sad[4];
    SADs(exp_sad);
    for (int block = 0; block < 4; ++block) {
      reference_sad = ReferenceSAD(block);

      EXPECT_EQ(reference_sad, exp_sad[block]) << "block " << block;
    }
  }
};

class SADx3Test : public ::testing::WithParamInterface<SadMxNx4Param>,
                  public SADTestBase {
 public:
  SADx3Test() : SADTestBase(GET_PARAM(0), GET_PARAM(1), GET_PARAM(3)) {}

 protected:
  void SADs(unsigned int *results) {
    const uint8_t *references[] = { GetReference(0), GetReference(1),
                                    GetReference(2), GetReference(3) };

    GET_PARAM(2)
    (source_data_, source_stride_, references, reference_stride_, results);
  }

  void SADForSpeedTest(unsigned int *results,
                       const uint8_t *const *references) {
    GET_PARAM(2)
    (source_data_, source_stride_, references, reference_stride_, results);
  }

  void CheckSADs() {
    unsigned int reference_sad, exp_sad[4];

    SADs(exp_sad);
    for (int block = 0; block < 3; ++block) {
      reference_sad = ReferenceSAD(block);

      EXPECT_EQ(reference_sad, exp_sad[block]) << "block " << block;
    }
  }
};

class SADSkipx4Test : public ::testing::WithParamInterface<SadMxNx4Param>,
                      public SADTestBase {
 public:
  SADSkipx4Test() : SADTestBase(GET_PARAM(0), GET_PARAM(1), GET_PARAM(3)) {}

 protected:
  void SADs(unsigned int *results) {
    const uint8_t *references[] = { GetReference(0), GetReference(1),
                                    GetReference(2), GetReference(3) };

    API_REGISTER_STATE_CHECK(GET_PARAM(2)(
        source_data_, source_stride_, references, reference_stride_, results));
  }

  void CheckSADs() {
    unsigned int reference_sad, exp_sad[4];

    SADs(exp_sad);
    for (int block = 0; block < 4; ++block) {
      reference_sad = ReferenceSADSkip(block);

      EXPECT_EQ(reference_sad, exp_sad[block]) << "block " << block;
    }
  }

  void SADForSpeedTest(unsigned int *results,
                       const uint8_t *const *references) {
    GET_PARAM(2)
    (source_data_, source_stride_, references, reference_stride_, results);
  }
};

class SADTest : public ::testing::WithParamInterface<SadMxNParam>,
                public SADTestBase {
 public:
  SADTest() : SADTestBase(GET_PARAM(0), GET_PARAM(1), GET_PARAM(3)) {}

 protected:
  unsigned int SAD(int block_idx) {
    unsigned int ret;
    const uint8_t *const reference = GetReference(block_idx);

    API_REGISTER_STATE_CHECK(ret = GET_PARAM(2)(source_data_, source_stride_,
                                                reference, reference_stride_));
    return ret;
  }

  void CheckSAD() {
    const unsigned int reference_sad = ReferenceSAD(0);
    const unsigned int exp_sad = SAD(0);

    ASSERT_EQ(reference_sad, exp_sad);
  }

  void SADForSpeedTest(unsigned int *results,
                       const uint8_t *const *references) {
    GET_PARAM(2)
    (source_data_, source_stride_, references[0], reference_stride_);
    (void)results;
  }
};

class SADSkipTest : public ::testing::WithParamInterface<SadMxNParam>,
                    public SADTestBase {
 public:
  SADSkipTest() : SADTestBase(GET_PARAM(0), GET_PARAM(1), GET_PARAM(3)) {}

 protected:
  unsigned int SAD(int block_idx) {
    unsigned int ret;
    const uint8_t *const reference = GetReference(block_idx);

    API_REGISTER_STATE_CHECK(ret = GET_PARAM(2)(source_data_, source_stride_,
                                                reference, reference_stride_));
    return ret;
  }

  void CheckSAD() {
    const unsigned int reference_sad = ReferenceSADSkip(0);
    const unsigned int exp_sad = SAD(0);

    ASSERT_EQ(reference_sad, exp_sad);
  }

  void SADForSpeedTest(unsigned int *results,
                       const uint8_t *const *references) {
    GET_PARAM(2)
    (source_data_, source_stride_, references[0], reference_stride_);
    (void)results;
  }
};

class SADavgTest : public ::testing::WithParamInterface<SadMxNAvgParam>,
                   public SADTestBase {
 public:
  SADavgTest() : SADTestBase(GET_PARAM(0), GET_PARAM(1), GET_PARAM(3)) {}

 protected:
  unsigned int SAD_avg(int block_idx) {
    unsigned int ret;
    const uint8_t *const reference = GetReference(block_idx);

    API_REGISTER_STATE_CHECK(ret = GET_PARAM(2)(source_data_, source_stride_,
                                                reference, reference_stride_,
                                                second_pred_));
    return ret;
  }

  void CheckSAD() {
    const unsigned int reference_sad = ReferenceSADavg(0);
    const unsigned int exp_sad = SAD_avg(0);

    ASSERT_EQ(reference_sad, exp_sad);
  }
};

class DistWtdCompAvgTest
    : public ::testing::WithParamInterface<DistWtdCompAvgParam>,
      public SADTestBase {
 public:
  DistWtdCompAvgTest()
      : SADTestBase(GET_PARAM(0), GET_PARAM(1), GET_PARAM(3)) {}

 protected:
  void dist_wtd_comp_avg(int block_idx) {
    const uint8_t *const reference = GetReference(block_idx);

    API_REGISTER_STATE_CHECK(GET_PARAM(2)(comp_pred_test_, second_pred_, width_,
                                          height_, reference, reference_stride_,
                                          &jcp_param_));
  }

  void CheckCompAvg() {
    for (int j = 0; j < 2; ++j) {
      for (int i = 0; i < 4; ++i) {
        jcp_param_.fwd_offset = quant_dist_lookup_table[i][j];
        jcp_param_.bck_offset = quant_dist_lookup_table[i][1 - j];

        ReferenceDistWtdCompAvg(0);
        dist_wtd_comp_avg(0);

        for (int y = 0; y < height_; ++y)
          for (int x = 0; x < width_; ++x)
            ASSERT_EQ(comp_pred_[y * width_ + x],
                      comp_pred_test_[y * width_ + x]);
      }
    }
  }
};

class DistWtdSADavgTest
    : public ::testing::WithParamInterface<DistWtdSadMxNAvgParam>,
      public SADTestBase {
 public:
  DistWtdSADavgTest() : SADTestBase(GET_PARAM(0), GET_PARAM(1), GET_PARAM(3)) {}

 protected:
  unsigned int dist_wtd_SAD_avg(int block_idx) {
    unsigned int ret;
    const uint8_t *const reference = GetReference(block_idx);

    API_REGISTER_STATE_CHECK(ret = GET_PARAM(2)(source_data_, source_stride_,
                                                reference, reference_stride_,
                                                second_pred_, &jcp_param_));
    return ret;
  }

  void CheckSAD() {
    for (int j = 0; j < 2; ++j) {
      for (int i = 0; i < 4; ++i) {
        jcp_param_.fwd_offset = quant_dist_lookup_table[i][j];
        jcp_param_.bck_offset = quant_dist_lookup_table[i][1 - j];

        const unsigned int reference_sad = ReferenceDistWtdSADavg(0);
        const unsigned int exp_sad = dist_wtd_SAD_avg(0);

        ASSERT_EQ(reference_sad, exp_sad);
      }
    }
  }
};

uint8_t *SADTestBase::source_data_ = nullptr;
uint8_t *SADTestBase::reference_data_ = nullptr;
uint8_t *SADTestBase::second_pred_ = nullptr;
uint8_t *SADTestBase::comp_pred_ = nullptr;
uint8_t *SADTestBase::comp_pred_test_ = nullptr;
uint8_t *SADTestBase::source_data8_ = nullptr;
uint8_t *SADTestBase::reference_data8_ = nullptr;
uint8_t *SADTestBase::second_pred8_ = nullptr;
uint8_t *SADTestBase::comp_pred8_ = nullptr;
uint8_t *SADTestBase::comp_pred8_test_ = nullptr;
uint16_t *SADTestBase::source_data16_ = nullptr;
uint16_t *SADTestBase::reference_data16_ = nullptr;
uint16_t *SADTestBase::second_pred16_ = nullptr;
uint16_t *SADTestBase::comp_pred16_ = nullptr;
uint16_t *SADTestBase::comp_pred16_test_ = nullptr;

TEST_P(SADTest, MaxRef) {
  FillConstant(source_data_, source_stride_, 0);
  FillConstant(reference_data_, reference_stride_, mask_);
  CheckSAD();
}

TEST_P(SADTest, MaxSrc) {
  FillConstant(source_data_, source_stride_, mask_);
  FillConstant(reference_data_, reference_stride_, 0);
  CheckSAD();
}

TEST_P(SADTest, ShortRef) {
  const int tmp_stride = reference_stride_;
  reference_stride_ >>= 1;
  FillRandom(source_data_, source_stride_);
  FillRandom(reference_data_, reference_stride_);
  CheckSAD();
  reference_stride_ = tmp_stride;
}

TEST_P(SADTest, UnalignedRef) {
  // The reference frame, but not the source frame, may be unaligned for
  // certain types of searches.
  const int tmp_stride = reference_stride_;
  reference_stride_ -= 1;
  FillRandom(source_data_, source_stride_);
  FillRandom(reference_data_, reference_stride_);
  CheckSAD();
  reference_stride_ = tmp_stride;
}

TEST_P(SADTest, ShortSrc) {
  const int tmp_stride = source_stride_;
  source_stride_ >>= 1;
  int test_count = 2000;
  while (test_count > 0) {
    FillRandom(source_data_, source_stride_);
    FillRandom(reference_data_, reference_stride_);
    CheckSAD();
    if (testing::Test::HasFatalFailure()) break;
    test_count -= 1;
  }
  source_stride_ = tmp_stride;
}

TEST_P(SADTest, DISABLED_Speed) {
  const int tmp_stride = source_stride_;
  source_stride_ >>= 1;
  FillRandom(source_data_, source_stride_);
  FillRandom(reference_data_, reference_stride_);
  SpeedSAD();
  source_stride_ = tmp_stride;
}

TEST_P(SADSkipTest, MaxRef) {
  FillConstant(source_data_, source_stride_, 0);
  FillConstant(reference_data_, reference_stride_, mask_);
  CheckSAD();
}

TEST_P(SADSkipTest, MaxSrc) {
  FillConstant(source_data_, source_stride_, mask_);
  FillConstant(reference_data_, reference_stride_, 0);
  CheckSAD();
}

TEST_P(SADSkipTest, ShortRef) {
  const int tmp_stride = reference_stride_;
  reference_stride_ >>= 1;
  FillRandom(source_data_, source_stride_);
  FillRandom(reference_data_, reference_stride_);
  CheckSAD();
  reference_stride_ = tmp_stride;
}

TEST_P(SADSkipTest, UnalignedRef) {
  // The reference frame, but not the source frame, may be unaligned for
  // certain types of searches.
  const int tmp_stride = reference_stride_;
  reference_stride_ -= 1;
  FillRandom(source_data_, source_stride_);
  FillRandom(reference_data_, reference_stride_);
  CheckSAD();
  reference_stride_ = tmp_stride;
}

TEST_P(SADSkipTest, ShortSrc) {
  const int tmp_stride = source_stride_;
  source_stride_ >>= 1;
  int test_count = 2000;
  while (test_count > 0) {
    FillRandom(source_data_, source_stride_);
    FillRandom(reference_data_, reference_stride_);
    CheckSAD();
    if (testing::Test::HasFatalFailure()) break;
    test_count -= 1;
  }
  source_stride_ = tmp_stride;
}

TEST_P(SADSkipTest, DISABLED_Speed) {
  const int tmp_stride = source_stride_;
  source_stride_ >>= 1;
  FillRandom(source_data_, source_stride_);
  FillRandom(reference_data_, reference_stride_);
  SpeedSAD();
  source_stride_ = tmp_stride;
}

TEST_P(SADavgTest, MaxRef) {
  FillConstant(source_data_, source_stride_, 0);
  FillConstant(reference_data_, reference_stride_, mask_);
  FillConstant(second_pred_, width_, 0);
  CheckSAD();
}
TEST_P(SADavgTest, MaxSrc) {
  FillConstant(source_data_, source_stride_, mask_);
  FillConstant(reference_data_, reference_stride_, 0);
  FillConstant(second_pred_, width_, 0);
  CheckSAD();
}

TEST_P(SADavgTest, ShortRef) {
  const int tmp_stride = reference_stride_;
  reference_stride_ >>= 1;
  FillRandom(source_data_, source_stride_);
  FillRandom(reference_data_, reference_stride_);
  FillRandom(second_pred_, width_);
  CheckSAD();
  reference_stride_ = tmp_stride;
}

TEST_P(SADavgTest, UnalignedRef) {
  // The reference frame, but not the source frame, may be unaligned for
  // certain types of searches.
  const int tmp_stride = reference_stride_;
  reference_stride_ -= 1;
  FillRandom(source_data_, source_stride_);
  FillRandom(reference_data_, reference_stride_);
  FillRandom(second_pred_, width_);
  CheckSAD();
  reference_stride_ = tmp_stride;
}

TEST_P(SADavgTest, ShortSrc) {
  const int tmp_stride = source_stride_;
  source_stride_ >>= 1;
  int test_count = 2000;
  while (test_count > 0) {
    FillRandom(source_data_, source_stride_);
    FillRandom(reference_data_, reference_stride_);
    FillRandom(second_pred_, width_);
    CheckSAD();
    if (testing::Test::HasFatalFailure()) break;
    test_count -= 1;
  }
  source_stride_ = tmp_stride;
}

TEST_P(DistWtdCompAvgTest, MaxRef) {
  FillConstant(reference_data_, reference_stride_, mask_);
  FillConstant(second_pred_, width_, 0);
  CheckCompAvg();
}

TEST_P(DistWtdCompAvgTest, MaxSecondPred) {
  FillConstant(reference_data_, reference_stride_, 0);
  FillConstant(second_pred_, width_, mask_);
  CheckCompAvg();
}

TEST_P(DistWtdCompAvgTest, ShortRef) {
  const int tmp_stride = reference_stride_;
  reference_stride_ >>= 1;
  FillRandom(reference_data_, reference_stride_);
  FillRandom(second_pred_, width_);
  CheckCompAvg();
  reference_stride_ = tmp_stride;
}

TEST_P(DistWtdCompAvgTest, UnalignedRef) {
  // The reference frame, but not the source frame, may be unaligned for
  // certain types of searches.
  const int tmp_stride = reference_stride_;
  reference_stride_ -= 1;
  FillRandom(reference_data_, reference_stride_);
  FillRandom(second_pred_, width_);
  CheckCompAvg();
  reference_stride_ = tmp_stride;
}

TEST_P(DistWtdSADavgTest, MaxRef) {
  FillConstant(source_data_, source_stride_, 0);
  FillConstant(reference_data_, reference_stride_, mask_);
  FillConstant(second_pred_, width_, 0);
  CheckSAD();
}
TEST_P(DistWtdSADavgTest, MaxSrc) {
  FillConstant(source_data_, source_stride_, mask_);
  FillConstant(reference_data_, reference_stride_, 0);
  FillConstant(second_pred_, width_, 0);
  CheckSAD();
}

TEST_P(DistWtdSADavgTest, ShortRef) {
  const int tmp_stride = reference_stride_;
  reference_stride_ >>= 1;
  FillRandom(source_data_, source_stride_);
  FillRandom(reference_data_, reference_stride_);
  FillRandom(second_pred_, width_);
  CheckSAD();
  reference_stride_ = tmp_stride;
}

TEST_P(DistWtdSADavgTest, UnalignedRef) {
  // The reference frame, but not the source frame, may be unaligned for
  // certain types of searches.
  const int tmp_stride = reference_stride_;
  reference_stride_ -= 1;
  FillRandom(source_data_, source_stride_);
  FillRandom(reference_data_, reference_stride_);
  FillRandom(second_pred_, width_);
  CheckSAD();
  reference_stride_ = tmp_stride;
}

TEST_P(DistWtdSADavgTest, ShortSrc) {
  const int tmp_stride = source_stride_;
  source_stride_ >>= 1;
  int test_count = 2000;
  while (test_count > 0) {
    FillRandom(source_data_, source_stride_);
    FillRandom(reference_data_, reference_stride_);
    FillRandom(second_pred_, width_);
    CheckSAD();
    if (testing::Test::HasFatalFailure()) break;
    test_count -= 1;
  }
  source_stride_ = tmp_stride;
}

// SADx4
TEST_P(SADx4Test, MaxRef) {
  FillConstant(source_data_, source_stride_, 0);
  FillConstant(GetReference(0), reference_stride_, mask_);
  FillConstant(GetReference(1), reference_stride_, mask_);
  FillConstant(GetReference(2), reference_stride_, mask_);
  FillConstant(GetReference(3), reference_stride_, mask_);
  CheckSADs();
}

TEST_P(SADx4Test, MaxSrc) {
  FillConstant(source_data_, source_stride_, mask_);
  FillConstant(GetReference(0), reference_stride_, 0);
  FillConstant(GetReference(1), reference_stride_, 0);
  FillConstant(GetReference(2), reference_stride_, 0);
  FillConstant(GetReference(3), reference_stride_, 0);
  CheckSADs();
}

TEST_P(SADx4Test, ShortRef) {
  int tmp_stride = reference_stride_;
  reference_stride_ >>= 1;
  FillRandom(source_data_, source_stride_);
  FillRandom(GetReference(0), reference_stride_);
  FillRandom(GetReference(1), reference_stride_);
  FillRandom(GetReference(2), reference_stride_);
  FillRandom(GetReference(3), reference_stride_);
  CheckSADs();
  reference_stride_ = tmp_stride;
}

TEST_P(SADx4Test, UnalignedRef) {
  // The reference frame, but not the source frame, may be unaligned for
  // certain types of searches.
  int tmp_stride = reference_stride_;
  reference_stride_ -= 1;
  FillRandom(source_data_, source_stride_);
  FillRandom(GetReference(0), reference_stride_);
  FillRandom(GetReference(1), reference_stride_);
  FillRandom(GetReference(2), reference_stride_);
  FillRandom(GetReference(3), reference_stride_);
  CheckSADs();
  reference_stride_ = tmp_stride;
}

TEST_P(SADx4Test, ShortSrc) {
  int tmp_stride = source_stride_;
  source_stride_ >>= 1;
  int test_count = 1000;
  while (test_count > 0) {
    FillRandom(source_data_, source_stride_);
    FillRandom(GetReference(0), reference_stride_);
    FillRandom(GetReference(1), reference_stride_);
    FillRandom(GetReference(2), reference_stride_);
    FillRandom(GetReference(3), reference_stride_);
    CheckSADs();
    test_count -= 1;
  }
  source_stride_ = tmp_stride;
}

TEST_P(SADx4Test, SrcAlignedByWidth) {
  uint8_t *tmp_source_data = source_data_;
  source_data_ += width_;
  FillRandom(source_data_, source_stride_);
  FillRandom(GetReference(0), reference_stride_);
  FillRandom(GetReference(1), reference_stride_);
  FillRandom(GetReference(2), reference_stride_);
  FillRandom(GetReference(3), reference_stride_);
  CheckSADs();
  source_data_ = tmp_source_data;
}

TEST_P(SADx4Test, DISABLED_Speed) {
  FillRandom(source_data_, source_stride_);
  FillRandom(GetReference(0), reference_stride_);
  FillRandom(GetReference(1), reference_stride_);
  FillRandom(GetReference(2), reference_stride_);
  FillRandom(GetReference(3), reference_stride_);
  SpeedSAD();
}

// SADx3
TEST_P(SADx3Test, MaxRef) {
  FillConstant(source_data_, source_stride_, 0);
  FillConstant(GetReference(0), reference_stride_, mask_);
  FillConstant(GetReference(1), reference_stride_, mask_);
  FillConstant(GetReference(2), reference_stride_, mask_);
  FillConstant(GetReference(3), reference_stride_, mask_);
  CheckSADs();
}

TEST_P(SADx3Test, MaxSrc) {
  FillConstant(source_data_, source_stride_, mask_);
  FillConstant(GetReference(0), reference_stride_, 0);
  FillConstant(GetReference(1), reference_stride_, 0);
  FillConstant(GetReference(2), reference_stride_, 0);
  FillConstant(GetReference(3), reference_stride_, 0);
  CheckSADs();
}

TEST_P(SADx3Test, ShortRef) {
  int tmp_stride = reference_stride_;
  reference_stride_ >>= 1;
  FillRandom(source_data_, source_stride_);
  FillRandom(GetReference(0), reference_stride_);
  FillRandom(GetReference(1), reference_stride_);
  FillRandom(GetReference(2), reference_stride_);
  FillRandom(GetReference(3), reference_stride_);
  CheckSADs();
  reference_stride_ = tmp_stride;
}

TEST_P(SADx3Test, UnalignedRef) {
  // The reference frame, but not the source frame, may be unaligned for
  // certain types of searches.
  int tmp_stride = reference_stride_;
  reference_stride_ -= 1;
  FillRandom(source_data_, source_stride_);
  FillRandom(GetReference(0), reference_stride_);
  FillRandom(GetReference(1), reference_stride_);
  FillRandom(GetReference(2), reference_stride_);
  FillRandom(GetReference(3), reference_stride_);
  CheckSADs();
  reference_stride_ = tmp_stride;
}

TEST_P(SADx3Test, ShortSrc) {
  int tmp_stride = source_stride_;
  source_stride_ >>= 1;
  int test_count = 1000;
  while (test_count > 0) {
    FillRandom(source_data_, source_stride_);
    FillRandom(GetReference(0), reference_stride_);
    FillRandom(GetReference(1), reference_stride_);
    FillRandom(GetReference(2), reference_stride_);
    FillRandom(GetReference(3), reference_stride_);
    CheckSADs();
    test_count -= 1;
  }
  source_stride_ = tmp_stride;
}

TEST_P(SADx3Test, SrcAlignedByWidth) {
  uint8_t *tmp_source_data = source_data_;
  source_data_ += width_;
  FillRandom(source_data_, source_stride_);
  FillRandom(GetReference(0), reference_stride_);
  FillRandom(GetReference(1), reference_stride_);
  FillRandom(GetReference(2), reference_stride_);
  FillRandom(GetReference(3), reference_stride_);
  CheckSADs();
  source_data_ = tmp_source_data;
}

TEST_P(SADx3Test, DISABLED_Speed) {
  FillRandom(source_data_, source_stride_);
  FillRandom(GetReference(0), reference_stride_);
  FillRandom(GetReference(1), reference_stride_);
  FillRandom(GetReference(2), reference_stride_);
  FillRandom(GetReference(3), reference_stride_);
  SpeedSAD();
}

// SADSkipx4
TEST_P(SADSkipx4Test, MaxRef) {
  FillConstant(source_data_, source_stride_, 0);
  FillConstant(GetReference(0), reference_stride_, mask_);
  FillConstant(GetReference(1), reference_stride_, mask_);
  FillConstant(GetReference(2), reference_stride_, mask_);
  FillConstant(GetReference(3), reference_stride_, mask_);
  CheckSADs();
}

TEST_P(SADSkipx4Test, MaxSrc) {
  FillConstant(source_data_, source_stride_, mask_);
  FillConstant(GetReference(0), reference_stride_, 0);
  FillConstant(GetReference(1), reference_stride_, 0);
  FillConstant(GetReference(2), reference_stride_, 0);
  FillConstant(GetReference(3), reference_stride_, 0);
  CheckSADs();
}

TEST_P(SADSkipx4Test, ShortRef) {
  int tmp_stride = reference_stride_;
  reference_stride_ >>= 1;
  FillRandom(source_data_, source_stride_);
  FillRandom(GetReference(0), reference_stride_);
  FillRandom(GetReference(1), reference_stride_);
  FillRandom(GetReference(2), reference_stride_);
  FillRandom(GetReference(3), reference_stride_);
  CheckSADs();
  reference_stride_ = tmp_stride;
}

TEST_P(SADSkipx4Test, UnalignedRef) {
  // The reference frame, but not the source frame, may be unaligned for
  // certain types of searches.
  int tmp_stride = reference_stride_;
  reference_stride_ -= 1;
  FillRandom(source_data_, source_stride_);
  FillRandom(GetReference(0), reference_stride_);
  FillRandom(GetReference(1), reference_stride_);
  FillRandom(GetReference(2), reference_stride_);
  FillRandom(GetReference(3), reference_stride_);
  CheckSADs();
  reference_stride_ = tmp_stride;
}

TEST_P(SADSkipx4Test, ShortSrc) {
  int tmp_stride = source_stride_;
  source_stride_ >>= 1;
  int test_count = 1000;
  while (test_count > 0) {
    FillRandom(source_data_, source_stride_);
    FillRandom(GetReference(0), reference_stride_);
    FillRandom(GetReference(1), reference_stride_);
    FillRandom(GetReference(2), reference_stride_);
    FillRandom(GetReference(3), reference_stride_);
    CheckSADs();
    test_count -= 1;
  }
  source_stride_ = tmp_stride;
}

TEST_P(SADSkipx4Test, SrcAlignedByWidth) {
  uint8_t *tmp_source_data = source_data_;
  source_data_ += width_;
  FillRandom(source_data_, source_stride_);
  FillRandom(GetReference(0), reference_stride_);
  FillRandom(GetReference(1), reference_stride_);
  FillRandom(GetReference(2), reference_stride_);
  FillRandom(GetReference(3), reference_stride_);
  CheckSADs();
  source_data_ = tmp_source_data;
}

TEST_P(SADSkipx4Test, DISABLED_Speed) {
  FillRandom(source_data_, source_stride_);
  FillRandom(GetReference(0), reference_stride_);
  FillRandom(GetReference(1), reference_stride_);
  FillRandom(GetReference(2), reference_stride_);
  FillRandom(GetReference(3), reference_stride_);
  SpeedSAD();
}

using std::make_tuple;

//------------------------------------------------------------------------------
// C functions
const SadMxNParam c_tests[] = {
  make_tuple(128, 128, &aom_sad128x128_c, -1),
  make_tuple(128, 64, &aom_sad128x64_c, -1),
  make_tuple(64, 128, &aom_sad64x128_c, -1),
  make_tuple(64, 64, &aom_sad64x64_c, -1),
  make_tuple(64, 32, &aom_sad64x32_c, -1),
  make_tuple(32, 64, &aom_sad32x64_c, -1),
  make_tuple(32, 32, &aom_sad32x32_c, -1),
  make_tuple(32, 16, &aom_sad32x16_c, -1),
  make_tuple(16, 32, &aom_sad16x32_c, -1),
  make_tuple(16, 16, &aom_sad16x16_c, -1),
  make_tuple(16, 8, &aom_sad16x8_c, -1),
  make_tuple(8, 16, &aom_sad8x16_c, -1),
  make_tuple(8, 8, &aom_sad8x8_c, -1),
  make_tuple(8, 4, &aom_sad8x4_c, -1),
  make_tuple(4, 8, &aom_sad4x8_c, -1),
  make_tuple(4, 4, &aom_sad4x4_c, -1),
#if CONFIG_AV1_HIGHBITDEPTH
  make_tuple(128, 128, &aom_highbd_sad128x128_c, 8),
  make_tuple(128, 64, &aom_highbd_sad128x64_c, 8),
  make_tuple(64, 128, &aom_highbd_sad64x128_c, 8),
  make_tuple(64, 64, &aom_highbd_sad64x64_c, 8),
  make_tuple(64, 32, &aom_highbd_sad64x32_c, 8),
  make_tuple(32, 64, &aom_highbd_sad32x64_c, 8),
  make_tuple(32, 32, &aom_highbd_sad32x32_c, 8),
  make_tuple(32, 16, &aom_highbd_sad32x16_c, 8),
  make_tuple(16, 32, &aom_highbd_sad16x32_c, 8),
  make_tuple(16, 16, &aom_highbd_sad16x16_c, 8),
  make_tuple(16, 8, &aom_highbd_sad16x8_c, 8),
  make_tuple(8, 16, &aom_highbd_sad8x16_c, 8),
  make_tuple(8, 8, &aom_highbd_sad8x8_c, 8),
  make_tuple(8, 4, &aom_highbd_sad8x4_c, 8),
  make_tuple(4, 8, &aom_highbd_sad4x8_c, 8),
  make_tuple(4, 4, &aom_highbd_sad4x4_c, 8),
  make_tuple(128, 128, &aom_highbd_sad128x128_c, 10),
  make_tuple(128, 64, &aom_highbd_sad128x64_c, 10),
  make_tuple(64, 128, &aom_highbd_sad64x128_c, 10),
  make_tuple(64, 64, &aom_highbd_sad64x64_c, 10),
  make_tuple(64, 32, &aom_highbd_sad64x32_c, 10),
  make_tuple(32, 64, &aom_highbd_sad32x64_c, 10),
  make_tuple(32, 32, &aom_highbd_sad32x32_c, 10),
  make_tuple(32, 16, &aom_highbd_sad32x16_c, 10),
  make_tuple(16, 32, &aom_highbd_sad16x32_c, 10),
  make_tuple(16, 16, &aom_highbd_sad16x16_c, 10),
  make_tuple(16, 8, &aom_highbd_sad16x8_c, 10),
  make_tuple(8, 16, &aom_highbd_sad8x16_c, 10),
  make_tuple(8, 8, &aom_highbd_sad8x8_c, 10),
  make_tuple(8, 4, &aom_highbd_sad8x4_c, 10),
  make_tuple(4, 8, &aom_highbd_sad4x8_c, 10),
  make_tuple(4, 4, &aom_highbd_sad4x4_c, 10),
  make_tuple(128, 128, &aom_highbd_sad128x128_c, 12),
  make_tuple(128, 64, &aom_highbd_sad128x64_c, 12),
  make_tuple(64, 128, &aom_highbd_sad64x128_c, 12),
  make_tuple(64, 64, &aom_highbd_sad64x64_c, 12),
  make_tuple(64, 32, &aom_highbd_sad64x32_c, 12),
  make_tuple(32, 64, &aom_highbd_sad32x64_c, 12),
  make_tuple(32, 32, &aom_highbd_sad32x32_c, 12),
  make_tuple(32, 16, &aom_highbd_sad32x16_c, 12),
  make_tuple(16, 32, &aom_highbd_sad16x32_c, 12),
  make_tuple(16, 16, &aom_highbd_sad16x16_c, 12),
  make_tuple(16, 8, &aom_highbd_sad16x8_c, 12),
  make_tuple(8, 16, &aom_highbd_sad8x16_c, 12),
  make_tuple(8, 8, &aom_highbd_sad8x8_c, 12),
  make_tuple(8, 4, &aom_highbd_sad8x4_c, 12),
  make_tuple(4, 8, &aom_highbd_sad4x8_c, 12),
  make_tuple(4, 4, &aom_highbd_sad4x4_c, 12),
#endif  // CONFIG_AV1_HIGHBITDEPTH
#if !CONFIG_REALTIME_ONLY
  make_tuple(64, 16, &aom_sad64x16_c, -1),
  make_tuple(16, 64, &aom_sad16x64_c, -1),
#if CONFIG_AV1_HIGHBITDEPTH
  make_tuple(64, 16, &aom_highbd_sad64x16_c, 8),
  make_tuple(16, 64, &aom_highbd_sad16x64_c, 8),
  make_tuple(64, 16, &aom_highbd_sad64x16_c, 10),
  make_tuple(16, 64, &aom_highbd_sad16x64_c, 10),
  make_tuple(64, 16, &aom_highbd_sad64x16_c, 12),
  make_tuple(16, 64, &aom_highbd_sad16x64_c, 12),
#endif
  make_tuple(32, 8, &aom_sad32x8_c, -1),
  make_tuple(8, 32, &aom_sad8x32_c, -1),
#if CONFIG_AV1_HIGHBITDEPTH
  make_tuple(32, 8, &aom_highbd_sad32x8_c, 8),
  make_tuple(8, 32, &aom_highbd_sad8x32_c, 8),
  make_tuple(32, 8, &aom_highbd_sad32x8_c, 10),
  make_tuple(8, 32, &aom_highbd_sad8x32_c, 10),
  make_tuple(32, 8, &aom_highbd_sad32x8_c, 12),
  make_tuple(8, 32, &aom_highbd_sad8x32_c, 12),
#endif
  make_tuple(16, 4, &aom_sad16x4_c, -1),
  make_tuple(4, 16, &aom_sad4x16_c, -1),
#if CONFIG_AV1_HIGHBITDEPTH
  make_tuple(16, 4, &aom_highbd_sad16x4_c, 8),
  make_tuple(4, 16, &aom_highbd_sad4x16_c, 8),
  make_tuple(16, 4, &aom_highbd_sad16x4_c, 10),
  make_tuple(4, 16, &aom_highbd_sad4x16_c, 10),
  make_tuple(16, 4, &aom_highbd_sad16x4_c, 12),
  make_tuple(4, 16, &aom_highbd_sad4x16_c, 12),
#endif
#endif  // !CONFIG_REALTIME_ONLY
};
INSTANTIATE_TEST_SUITE_P(C, SADTest, ::testing::ValuesIn(c_tests));

const SadSkipMxNParam skip_c_tests[] = {
  make_tuple(128, 128, &aom_sad_skip_128x128_c, -1),
  make_tuple(128, 64, &aom_sad_skip_128x64_c, -1),
  make_tuple(64, 128, &aom_sad_skip_64x128_c, -1),
  make_tuple(64, 64, &aom_sad_skip_64x64_c, -1),
  make_tuple(64, 32, &aom_sad_skip_64x32_c, -1),
  make_tuple(32, 64, &aom_sad_skip_32x64_c, -1),
  make_tuple(32, 32, &aom_sad_skip_32x32_c, -1),
  make_tuple(32, 16, &aom_sad_skip_32x16_c, -1),
  make_tuple(16, 32, &aom_sad_skip_16x32_c, -1),
  make_tuple(16, 16, &aom_sad_skip_16x16_c, -1),
  make_tuple(16, 8, &aom_sad_skip_16x8_c, -1),
  make_tuple(8, 16, &aom_sad_skip_8x16_c, -1),
  make_tuple(8, 8, &aom_sad_skip_8x8_c, -1),
  make_tuple(8, 4, &aom_sad_skip_8x4_c, -1),
  make_tuple(4, 8, &aom_sad_skip_4x8_c, -1),
  make_tuple(4, 4, &aom_sad_skip_4x4_c, -1),
#if !CONFIG_REALTIME_ONLY
  make_tuple(64, 16, &aom_sad_skip_64x16_c, -1),
  make_tuple(16, 64, &aom_sad_skip_16x64_c, -1),
  make_tuple(32, 8, &aom_sad_skip_32x8_c, -1),
  make_tuple(8, 32, &aom_sad_skip_8x32_c, -1),
  make_tuple(16, 4, &aom_sad_skip_16x4_c, -1),
  make_tuple(4, 16, &aom_sad_skip_4x16_c, -1),
#endif
#if CONFIG_AV1_HIGHBITDEPTH
  make_tuple(128, 128, &aom_highbd_sad_skip_128x128_c, 8),
  make_tuple(128, 64, &aom_highbd_sad_skip_128x64_c, 8),
  make_tuple(64, 128, &aom_highbd_sad_skip_64x128_c, 8),
  make_tuple(64, 64, &aom_highbd_sad_skip_64x64_c, 8),
  make_tuple(64, 32, &aom_highbd_sad_skip_64x32_c, 8),
  make_tuple(32, 64, &aom_highbd_sad_skip_32x64_c, 8),
  make_tuple(32, 32, &aom_highbd_sad_skip_32x32_c, 8),
  make_tuple(32, 16, &aom_highbd_sad_skip_32x16_c, 8),
  make_tuple(16, 32, &aom_highbd_sad_skip_16x32_c, 8),
  make_tuple(16, 16, &aom_highbd_sad_skip_16x16_c, 8),
  make_tuple(16, 8, &aom_highbd_sad_skip_16x8_c, 8),
  make_tuple(8, 16, &aom_highbd_sad_skip_8x16_c, 8),
  make_tuple(8, 8, &aom_highbd_sad_skip_8x8_c, 8),
  make_tuple(8, 4, &aom_highbd_sad_skip_8x4_c, 8),
  make_tuple(4, 8, &aom_highbd_sad_skip_4x8_c, 8),
  make_tuple(4, 4, &aom_highbd_sad_skip_4x4_c, 8),
#if !CONFIG_REALTIME_ONLY
  make_tuple(64, 16, &aom_highbd_sad_skip_64x16_c, 8),
  make_tuple(16, 64, &aom_highbd_sad_skip_16x64_c, 8),
  make_tuple(32, 8, &aom_highbd_sad_skip_32x8_c, 8),
  make_tuple(8, 32, &aom_highbd_sad_skip_8x32_c, 8),
  make_tuple(16, 4, &aom_highbd_sad_skip_16x4_c, 8),
  make_tuple(4, 16, &aom_highbd_sad_skip_4x16_c, 8),
#endif
  make_tuple(128, 128, &aom_highbd_sad_skip_128x128_c, 10),
  make_tuple(128, 64, &aom_highbd_sad_skip_128x64_c, 10),
  make_tuple(64, 128, &aom_highbd_sad_skip_64x128_c, 10),
  make_tuple(64, 64, &aom_highbd_sad_skip_64x64_c, 10),
  make_tuple(64, 32, &aom_highbd_sad_skip_64x32_c, 10),
  make_tuple(32, 64, &aom_highbd_sad_skip_32x64_c, 10),
  make_tuple(32, 32, &aom_highbd_sad_skip_32x32_c, 10),
  make_tuple(32, 16, &aom_highbd_sad_skip_32x16_c, 10),
  make_tuple(16, 32, &aom_highbd_sad_skip_16x32_c, 10),
  make_tuple(16, 16, &aom_highbd_sad_skip_16x16_c, 10),
  make_tuple(16, 8, &aom_highbd_sad_skip_16x8_c, 10),
  make_tuple(8, 16, &aom_highbd_sad_skip_8x16_c, 10),
  make_tuple(8, 8, &aom_highbd_sad_skip_8x8_c, 10),
  make_tuple(8, 4, &aom_highbd_sad_skip_8x4_c, 10),
  make_tuple(4, 8, &aom_highbd_sad_skip_4x8_c, 10),
  make_tuple(4, 4, &aom_highbd_sad_skip_4x4_c, 10),
#if !CONFIG_REALTIME_ONLY
  make_tuple(64, 16, &aom_highbd_sad_skip_64x16_c, 10),
  make_tuple(16, 64, &aom_highbd_sad_skip_16x64_c, 10),
  make_tuple(32, 8, &aom_highbd_sad_skip_32x8_c, 10),
  make_tuple(8, 32, &aom_highbd_sad_skip_8x32_c, 10),
  make_tuple(16, 4, &aom_highbd_sad_skip_16x4_c, 10),
  make_tuple(4, 16, &aom_highbd_sad_skip_4x16_c, 10),
#endif
  make_tuple(128, 128, &aom_highbd_sad_skip_128x128_c, 12),
  make_tuple(128, 64, &aom_highbd_sad_skip_128x64_c, 12),
  make_tuple(64, 128, &aom_highbd_sad_skip_64x128_c, 12),
  make_tuple(64, 64, &aom_highbd_sad_skip_64x64_c, 12),
  make_tuple(64, 32, &aom_highbd_sad_skip_64x32_c, 12),
  make_tuple(32, 64, &aom_highbd_sad_skip_32x64_c, 12),
  make_tuple(32, 32, &aom_highbd_sad_skip_32x32_c, 12),
  make_tuple(32, 16, &aom_highbd_sad_skip_32x16_c, 12),
  make_tuple(16, 32, &aom_highbd_sad_skip_16x32_c, 12),
  make_tuple(16, 16, &aom_highbd_sad_skip_16x16_c, 12),
  make_tuple(16, 8, &aom_highbd_sad_skip_16x8_c, 12),
  make_tuple(8, 16, &aom_highbd_sad_skip_8x16_c, 12),
  make_tuple(8, 8, &aom_highbd_sad_skip_8x8_c, 12),
  make_tuple(8, 4, &aom_highbd_sad_skip_8x4_c, 12),
  make_tuple(4, 8, &aom_highbd_sad_skip_4x8_c, 12),
  make_tuple(4, 4, &aom_highbd_sad_skip_4x4_c, 12),
#if !CONFIG_REALTIME_ONLY
  make_tuple(64, 16, &aom_highbd_sad_skip_64x16_c, 12),
  make_tuple(16, 64, &aom_highbd_sad_skip_16x64_c, 12),
  make_tuple(32, 8, &aom_highbd_sad_skip_32x8_c, 12),
  make_tuple(8, 32, &aom_highbd_sad_skip_8x32_c, 12),
  make_tuple(16, 4, &aom_highbd_sad_skip_16x4_c, 12),
  make_tuple(4, 16, &aom_highbd_sad_skip_4x16_c, 12),
#endif  // !CONFIG_REALTIME_ONLY
#endif  // CONFIG_AV1_HIGHBITDEPTH
};
INSTANTIATE_TEST_SUITE_P(C, SADSkipTest, ::testing::ValuesIn(skip_c_tests));

const SadMxNAvgParam avg_c_tests[] = {
  make_tuple(128, 128, &aom_sad128x128_avg_c, -1),
  make_tuple(128, 64, &aom_sad128x64_avg_c, -1),
  make_tuple(64, 128, &aom_sad64x128_avg_c, -1),
  make_tuple(64, 64, &aom_sad64x64_avg_c, -1),
  make_tuple(64, 32, &aom_sad64x32_avg_c, -1),
  make_tuple(32, 64, &aom_sad32x64_avg_c, -1),
  make_tuple(32, 32, &aom_sad32x32_avg_c, -1),
  make_tuple(32, 16, &aom_sad32x16_avg_c, -1),
  make_tuple(16, 32, &aom_sad16x32_avg_c, -1),
  make_tuple(16, 16, &aom_sad16x16_avg_c, -1),
  make_tuple(16, 8, &aom_sad16x8_avg_c, -1),
  make_tuple(8, 16, &aom_sad8x16_avg_c, -1),
  make_tuple(8, 8, &aom_sad8x8_avg_c, -1),
  make_tuple(8, 4, &aom_sad8x4_avg_c, -1),
  make_tuple(4, 8, &aom_sad4x8_avg_c, -1),
  make_tuple(4, 4, &aom_sad4x4_avg_c, -1),
#if CONFIG_AV1_HIGHBITDEPTH
  make_tuple(128, 128, &aom_highbd_sad128x128_avg_c, 8),
  make_tuple(128, 64, &aom_highbd_sad128x64_avg_c, 8),
  make_tuple(64, 128, &aom_highbd_sad64x128_avg_c, 8),
  make_tuple(64, 64, &aom_highbd_sad64x64_avg_c, 8),
  make_tuple(64, 32, &aom_highbd_sad64x32_avg_c, 8),
  make_tuple(32, 64, &aom_highbd_sad32x64_avg_c, 8),
  make_tuple(32, 32, &aom_highbd_sad32x32_avg_c, 8),
  make_tuple(32, 16, &aom_highbd_sad32x16_avg_c, 8),
  make_tuple(16, 32, &aom_highbd_sad16x32_avg_c, 8),
  make_tuple(16, 16, &aom_highbd_sad16x16_avg_c, 8),
  make_tuple(16, 8, &aom_highbd_sad16x8_avg_c, 8),
  make_tuple(8, 16, &aom_highbd_sad8x16_avg_c, 8),
  make_tuple(8, 8, &aom_highbd_sad8x8_avg_c, 8),
  make_tuple(8, 4, &aom_highbd_sad8x4_avg_c, 8),
  make_tuple(4, 8, &aom_highbd_sad4x8_avg_c, 8),
  make_tuple(4, 4, &aom_highbd_sad4x4_avg_c, 8),
  make_tuple(128, 128, &aom_highbd_sad128x128_avg_c, 10),
  make_tuple(128, 64, &aom_highbd_sad128x64_avg_c, 10),
  make_tuple(64, 128, &aom_highbd_sad64x128_avg_c, 10),
  make_tuple(64, 64, &aom_highbd_sad64x64_avg_c, 10),
  make_tuple(64, 32, &aom_highbd_sad64x32_avg_c, 10),
  make_tuple(32, 64, &aom_highbd_sad32x64_avg_c, 10),
  make_tuple(32, 32, &aom_highbd_sad32x32_avg_c, 10),
  make_tuple(32, 16, &aom_highbd_sad32x16_avg_c, 10),
  make_tuple(16, 32, &aom_highbd_sad16x32_avg_c, 10),
  make_tuple(16, 16, &aom_highbd_sad16x16_avg_c, 10),
  make_tuple(16, 8, &aom_highbd_sad16x8_avg_c, 10),
  make_tuple(8, 16, &aom_highbd_sad8x16_avg_c, 10),
  make_tuple(8, 8, &aom_highbd_sad8x8_avg_c, 10),
  make_tuple(8, 4, &aom_highbd_sad8x4_avg_c, 10),
  make_tuple(4, 8, &aom_highbd_sad4x8_avg_c, 10),
  make_tuple(4, 4, &aom_highbd_sad4x4_avg_c, 10),
  make_tuple(128, 128, &aom_highbd_sad128x128_avg_c, 12),
  make_tuple(128, 64, &aom_highbd_sad128x64_avg_c, 12),
  make_tuple(64, 128, &aom_highbd_sad64x128_avg_c, 12),
  make_tuple(64, 64, &aom_highbd_sad64x64_avg_c, 12),
  make_tuple(64, 32, &aom_highbd_sad64x32_avg_c, 12),
  make_tuple(32, 64, &aom_highbd_sad32x64_avg_c, 12),
  make_tuple(32, 32, &aom_highbd_sad32x32_avg_c, 12),
  make_tuple(32, 16, &aom_highbd_sad32x16_avg_c, 12),
  make_tuple(16, 32, &aom_highbd_sad16x32_avg_c, 12),
  make_tuple(16, 16, &aom_highbd_sad16x16_avg_c, 12),
  make_tuple(16, 8, &aom_highbd_sad16x8_avg_c, 12),
  make_tuple(8, 16, &aom_highbd_sad8x16_avg_c, 12),
  make_tuple(8, 8, &aom_highbd_sad8x8_avg_c, 12),
  make_tuple(8, 4, &aom_highbd_sad8x4_avg_c, 12),
  make_tuple(4, 8, &aom_highbd_sad4x8_avg_c, 12),
  make_tuple(4, 4, &aom_highbd_sad4x4_avg_c, 12),
#endif  // CONFIG_AV1_HIGHBITDEPTH
#if !CONFIG_REALTIME_ONLY
  make_tuple(64, 16, &aom_sad64x16_avg_c, -1),
  make_tuple(16, 64, &aom_sad16x64_avg_c, -1),
#if CONFIG_AV1_HIGHBITDEPTH
  make_tuple(64, 16, &aom_highbd_sad64x16_avg_c, 8),
  make_tuple(16, 64, &aom_highbd_sad16x64_avg_c, 8),
  make_tuple(64, 16, &aom_highbd_sad64x16_avg_c, 10),
  make_tuple(16, 64, &aom_highbd_sad16x64_avg_c, 10),
  make_tuple(64, 16, &aom_highbd_sad64x16_avg_c, 12),
  make_tuple(16, 64, &aom_highbd_sad16x64_avg_c, 12),
#endif
  make_tuple(32, 8, &aom_sad32x8_avg_c, -1),
  make_tuple(8, 32, &aom_sad8x32_avg_c, -1),
#if CONFIG_AV1_HIGHBITDEPTH
  make_tuple(32, 8, &aom_highbd_sad32x8_avg_c, 8),
  make_tuple(8, 32, &aom_highbd_sad8x32_avg_c, 8),
  make_tuple(32, 8, &aom_highbd_sad32x8_avg_c, 10),
  make_tuple(8, 32, &aom_highbd_sad8x32_avg_c, 10),
  make_tuple(32, 8, &aom_highbd_sad32x8_avg_c, 12),
  make_tuple(8, 32, &aom_highbd_sad8x32_avg_c, 12),
#endif
  make_tuple(16, 4, &aom_sad16x4_avg_c, -1),
  make_tuple(4, 16, &aom_sad4x16_avg_c, -1),
#if CONFIG_AV1_HIGHBITDEPTH
  make_tuple(16, 4, &aom_highbd_sad16x4_avg_c, 8),
  make_tuple(4, 16, &aom_highbd_sad4x16_avg_c, 8),
  make_tuple(16, 4, &aom_highbd_sad16x4_avg_c, 10),
  make_tuple(4, 16, &aom_highbd_sad4x16_avg_c, 10),
  make_tuple(16, 4, &aom_highbd_sad16x4_avg_c, 12),
  make_tuple(4, 16, &aom_highbd_sad4x16_avg_c, 12),
#endif
#endif  // !CONFIG_REALTIME_ONLY
};
INSTANTIATE_TEST_SUITE_P(C, SADavgTest, ::testing::ValuesIn(avg_c_tests));

// TODO(chengchen): add highbd tests
const DistWtdCompAvgParam dist_wtd_comp_avg_c_tests[] = {
  make_tuple(128, 128, &aom_dist_wtd_comp_avg_pred_c, -1),
  make_tuple(128, 64, &aom_dist_wtd_comp_avg_pred_c, -1),
  make_tuple(64, 128, &aom_dist_wtd_comp_avg_pred_c, -1),
  make_tuple(64, 64, &aom_dist_wtd_comp_avg_pred_c, -1),
  make_tuple(64, 32, &aom_dist_wtd_comp_avg_pred_c, -1),
  make_tuple(32, 64, &aom_dist_wtd_comp_avg_pred_c, -1),
  make_tuple(32, 32, &aom_dist_wtd_comp_avg_pred_c, -1),
  make_tuple(32, 16, &aom_dist_wtd_comp_avg_pred_c, -1),
  make_tuple(16, 32, &aom_dist_wtd_comp_avg_pred_c, -1),
  make_tuple(16, 16, &aom_dist_wtd_comp_avg_pred_c, -1),
  make_tuple(16, 8, &aom_dist_wtd_comp_avg_pred_c, -1),
  make_tuple(8, 16, &aom_dist_wtd_comp_avg_pred_c, -1),
  make_tuple(8, 8, &aom_dist_wtd_comp_avg_pred_c, -1),
  make_tuple(8, 4, &aom_dist_wtd_comp_avg_pred_c, -1),
  make_tuple(4, 8, &aom_dist_wtd_comp_avg_pred_c, -1),
  make_tuple(4, 4, &aom_dist_wtd_comp_avg_pred_c, -1),

#if !CONFIG_REALTIME_ONLY
  make_tuple(64, 16, &aom_dist_wtd_comp_avg_pred_c, -1),
  make_tuple(16, 64, &aom_dist_wtd_comp_avg_pred_c, -1),
  make_tuple(32, 8, &aom_dist_wtd_comp_avg_pred_c, -1),
  make_tuple(8, 32, &aom_dist_wtd_comp_avg_pred_c, -1),
  make_tuple(16, 4, &aom_dist_wtd_comp_avg_pred_c, -1),
  make_tuple(4, 16, &aom_dist_wtd_comp_avg_pred_c, -1),
#endif
};

INSTANTIATE_TEST_SUITE_P(C, DistWtdCompAvgTest,
                         ::testing::ValuesIn(dist_wtd_comp_avg_c_tests));

const DistWtdSadMxNAvgParam dist_wtd_avg_c_tests[] = {
  make_tuple(128, 128, &aom_dist_wtd_sad128x128_avg_c, -1),
  make_tuple(128, 64, &aom_dist_wtd_sad128x64_avg_c, -1),
  make_tuple(64, 128, &aom_dist_wtd_sad64x128_avg_c, -1),
  make_tuple(64, 64, &aom_dist_wtd_sad64x64_avg_c, -1),
  make_tuple(64, 32, &aom_dist_wtd_sad64x32_avg_c, -1),
  make_tuple(32, 64, &aom_dist_wtd_sad32x64_avg_c, -1),
  make_tuple(32, 32, &aom_dist_wtd_sad32x32_avg_c, -1),
  make_tuple(32, 16, &aom_dist_wtd_sad32x16_avg_c, -1),
  make_tuple(16, 32, &aom_dist_wtd_sad16x32_avg_c, -1),
  make_tuple(16, 16, &aom_dist_wtd_sad16x16_avg_c, -1),
  make_tuple(16, 8, &aom_dist_wtd_sad16x8_avg_c, -1),
  make_tuple(8, 16, &aom_dist_wtd_sad8x16_avg_c, -1),
  make_tuple(8, 8, &aom_dist_wtd_sad8x8_avg_c, -1),
  make_tuple(8, 4, &aom_dist_wtd_sad8x4_avg_c, -1),
  make_tuple(4, 8, &aom_dist_wtd_sad4x8_avg_c, -1),
  make_tuple(4, 4, &aom_dist_wtd_sad4x4_avg_c, -1),

#if !CONFIG_REALTIME_ONLY
  make_tuple(64, 16, &aom_dist_wtd_sad64x16_avg_c, -1),
  make_tuple(16, 64, &aom_dist_wtd_sad16x64_avg_c, -1),
  make_tuple(32, 8, &aom_dist_wtd_sad32x8_avg_c, -1),
  make_tuple(8, 32, &aom_dist_wtd_sad8x32_avg_c, -1),
  make_tuple(16, 4, &aom_dist_wtd_sad16x4_avg_c, -1),
  make_tuple(4, 16, &aom_dist_wtd_sad4x16_avg_c, -1),
#endif
};

INSTANTIATE_TEST_SUITE_P(C, DistWtdSADavgTest,
                         ::testing::ValuesIn(dist_wtd_avg_c_tests));

const SadMxNx4Param x4d_c_tests[] = {
  make_tuple(128, 128, &aom_sad128x128x4d_c, -1),
  make_tuple(128, 64, &aom_sad128x64x4d_c, -1),
  make_tuple(64, 128, &aom_sad64x128x4d_c, -1),
  make_tuple(64, 64, &aom_sad64x64x4d_c, -1),
  make_tuple(64, 32, &aom_sad64x32x4d_c, -1),
  make_tuple(32, 64, &aom_sad32x64x4d_c, -1),
  make_tuple(32, 32, &aom_sad32x32x4d_c, -1),
  make_tuple(32, 16, &aom_sad32x16x4d_c, -1),
  make_tuple(16, 32, &aom_sad16x32x4d_c, -1),
  make_tuple(16, 16, &aom_sad16x16x4d_c, -1),
  make_tuple(16, 8, &aom_sad16x8x4d_c, -1),
  make_tuple(8, 16, &aom_sad8x16x4d_c, -1),
  make_tuple(8, 8, &aom_sad8x8x4d_c, -1),
  make_tuple(8, 4, &aom_sad8x4x4d_c, -1),
  make_tuple(4, 8, &aom_sad4x8x4d_c, -1),
  make_tuple(4, 4, &aom_sad4x4x4d_c, -1),
#if CONFIG_AV1_HIGHBITDEPTH
  make_tuple(128, 128, &aom_highbd_sad128x128x4d_c, 8),
  make_tuple(128, 64, &aom_highbd_sad128x64x4d_c, 8),
  make_tuple(64, 128, &aom_highbd_sad64x128x4d_c, 8),
  make_tuple(64, 64, &aom_highbd_sad64x64x4d_c, 8),
  make_tuple(64, 32, &aom_highbd_sad64x32x4d_c, 8),
  make_tuple(32, 64, &aom_highbd_sad32x64x4d_c, 8),
  make_tuple(32, 32, &aom_highbd_sad32x32x4d_c, 8),
  make_tuple(32, 16, &aom_highbd_sad32x16x4d_c, 8),
  make_tuple(16, 32, &aom_highbd_sad16x32x4d_c, 8),
  make_tuple(16, 16, &aom_highbd_sad16x16x4d_c, 8),
  make_tuple(16, 8, &aom_highbd_sad16x8x4d_c, 8),
  make_tuple(8, 16, &aom_highbd_sad8x16x4d_c, 8),
  make_tuple(8, 8, &aom_highbd_sad8x8x4d_c, 8),
  make_tuple(8, 4, &aom_highbd_sad8x4x4d_c, 8),
  make_tuple(4, 8, &aom_highbd_sad4x8x4d_c, 8),
  make_tuple(4, 4, &aom_highbd_sad4x4x4d_c, 8),
  make_tuple(128, 128, &aom_highbd_sad128x128x4d_c, 10),
  make_tuple(128, 64, &aom_highbd_sad128x64x4d_c, 10),
  make_tuple(64, 128, &aom_highbd_sad64x128x4d_c, 10),
  make_tuple(64, 64, &aom_highbd_sad64x64x4d_c, 10),
  make_tuple(64, 32, &aom_highbd_sad64x32x4d_c, 10),
  make_tuple(32, 64, &aom_highbd_sad32x64x4d_c, 10),
  make_tuple(32, 32, &aom_highbd_sad32x32x4d_c, 10),
  make_tuple(32, 16, &aom_highbd_sad32x16x4d_c, 10),
  make_tuple(16, 32, &aom_highbd_sad16x32x4d_c, 10),
  make_tuple(16, 16, &aom_highbd_sad16x16x4d_c, 10),
  make_tuple(16, 8, &aom_highbd_sad16x8x4d_c, 10),
  make_tuple(8, 16, &aom_highbd_sad8x16x4d_c, 10),
  make_tuple(8, 8, &aom_highbd_sad8x8x4d_c, 10),
  make_tuple(8, 4, &aom_highbd_sad8x4x4d_c, 10),
  make_tuple(4, 8, &aom_highbd_sad4x8x4d_c, 10),
  make_tuple(4, 4, &aom_highbd_sad4x4x4d_c, 10),
  make_tuple(128, 128, &aom_highbd_sad128x128x4d_c, 12),
  make_tuple(128, 64, &aom_highbd_sad128x64x4d_c, 12),
  make_tuple(64, 128, &aom_highbd_sad64x128x4d_c, 12),
  make_tuple(64, 64, &aom_highbd_sad64x64x4d_c, 12),
  make_tuple(64, 32, &aom_highbd_sad64x32x4d_c, 12),
  make_tuple(32, 64, &aom_highbd_sad32x64x4d_c, 12),
  make_tuple(32, 32, &aom_highbd_sad32x32x4d_c, 12),
  make_tuple(32, 16, &aom_highbd_sad32x16x4d_c, 12),
  make_tuple(16, 32, &aom_highbd_sad16x32x4d_c, 12),
  make_tuple(16, 16, &aom_highbd_sad16x16x4d_c, 12),
  make_tuple(16, 8, &aom_highbd_sad16x8x4d_c, 12),
  make_tuple(8, 16, &aom_highbd_sad8x16x4d_c, 12),
  make_tuple(8, 8, &aom_highbd_sad8x8x4d_c, 12),
  make_tuple(8, 4, &aom_highbd_sad8x4x4d_c, 12),
  make_tuple(4, 8, &aom_highbd_sad4x8x4d_c, 12),
  make_tuple(4, 4, &aom_highbd_sad4x4x4d_c, 12),
#endif
#if !CONFIG_REALTIME_ONLY
  make_tuple(64, 16, &aom_sad64x16x4d_c, -1),
  make_tuple(16, 64, &aom_sad16x64x4d_c, -1),
#if CONFIG_AV1_HIGHBITDEPTH
  make_tuple(64, 16, &aom_highbd_sad64x16x4d_c, 8),
  make_tuple(16, 64, &aom_highbd_sad16x64x4d_c, 8),
  make_tuple(64, 16, &aom_highbd_sad64x16x4d_c, 10),
  make_tuple(16, 64, &aom_highbd_sad16x64x4d_c, 10),
  make_tuple(64, 16, &aom_highbd_sad64x16x4d_c, 12),
  make_tuple(16, 64, &aom_highbd_sad16x64x4d_c, 12),
#endif
  make_tuple(32, 8, &aom_sad32x8x4d_c, -1),
  make_tuple(8, 32, &aom_sad8x32x4d_c, -1),
#if CONFIG_AV1_HIGHBITDEPTH
  make_tuple(32, 8, &aom_highbd_sad32x8x4d_c, 8),
  make_tuple(8, 32, &aom_highbd_sad8x32x4d_c, 8),
  make_tuple(32, 8, &aom_highbd_sad32x8x4d_c, 10),
  make_tuple(8, 32, &aom_highbd_sad8x32x4d_c, 10),
  make_tuple(32, 8, &aom_highbd_sad32x8x4d_c, 12),
  make_tuple(8, 32, &aom_highbd_sad8x32x4d_c, 12),
#endif
  make_tuple(16, 4, &aom_sad16x4x4d_c, -1),
  make_tuple(4, 16, &aom_sad4x16x4d_c, -1),
#if CONFIG_AV1_HIGHBITDEPTH
  make_tuple(16, 4, &aom_highbd_sad16x4x4d_c, 8),
  make_tuple(4, 16, &aom_highbd_sad4x16x4d_c, 8),
  make_tuple(16, 4, &aom_highbd_sad16x4x4d_c, 10),
  make_tuple(4, 16, &aom_highbd_sad4x16x4d_c, 10),
  make_tuple(16, 4, &aom_highbd_sad16x4x4d_c, 12),
  make_tuple(4, 16, &aom_highbd_sad4x16x4d_c, 12),
#endif
#endif  // !CONFIG_REALTIME_ONLY
};
INSTANTIATE_TEST_SUITE_P(C, SADx4Test, ::testing::ValuesIn(x4d_c_tests));

const SadMxNx4Param x3d_c_tests[] = {
  make_tuple(128, 128, &aom_sad128x128x3d_c, -1),
  make_tuple(128, 64, &aom_sad128x64x3d_c, -1),
  make_tuple(64, 128, &aom_sad64x128x3d_c, -1),
  make_tuple(64, 64, &aom_sad64x64x3d_c, -1),
  make_tuple(64, 32, &aom_sad64x32x3d_c, -1),
  make_tuple(32, 64, &aom_sad32x64x3d_c, -1),
  make_tuple(32, 32, &aom_sad32x32x3d_c, -1),
  make_tuple(32, 16, &aom_sad32x16x3d_c, -1),
  make_tuple(16, 32, &aom_sad16x32x3d_c, -1),
  make_tuple(16, 16, &aom_sad16x16x3d_c, -1),
  make_tuple(16, 8, &aom_sad16x8x3d_c, -1),
  make_tuple(8, 16, &aom_sad8x16x3d_c, -1),
  make_tuple(8, 8, &aom_sad8x8x3d_c, -1),
  make_tuple(8, 4, &aom_sad8x4x3d_c, -1),
  make_tuple(4, 8, &aom_sad4x8x3d_c, -1),
  make_tuple(4, 4, &aom_sad4x4x3d_c, -1),
#if CONFIG_AV1_HIGHBITDEPTH
  make_tuple(128, 128, &aom_highbd_sad128x128x3d_c, 8),
  make_tuple(128, 64, &aom_highbd_sad128x64x3d_c, 8),
  make_tuple(64, 128, &aom_highbd_sad64x128x3d_c, 8),
  make_tuple(64, 64, &aom_highbd_sad64x64x3d_c, 8),
  make_tuple(64, 32, &aom_highbd_sad64x32x3d_c, 8),
  make_tuple(32, 64, &aom_highbd_sad32x64x3d_c, 8),
  make_tuple(32, 32, &aom_highbd_sad32x32x3d_c, 8),
  make_tuple(32, 16, &aom_highbd_sad32x16x3d_c, 8),
  make_tuple(16, 32, &aom_highbd_sad16x32x3d_c, 8),
  make_tuple(16, 16, &aom_highbd_sad16x16x3d_c, 8),
  make_tuple(16, 8, &aom_highbd_sad16x8x3d_c, 8),
  make_tuple(8, 16, &aom_highbd_sad8x16x3d_c, 8),
  make_tuple(8, 8, &aom_highbd_sad8x8x3d_c, 8),
  make_tuple(8, 4, &aom_highbd_sad8x4x3d_c, 8),
  make_tuple(4, 8, &aom_highbd_sad4x8x3d_c, 8),
  make_tuple(4, 4, &aom_highbd_sad4x4x3d_c, 8),
  make_tuple(128, 128, &aom_highbd_sad128x128x3d_c, 10),
  make_tuple(128, 64, &aom_highbd_sad128x64x3d_c, 10),
  make_tuple(64, 128, &aom_highbd_sad64x128x3d_c, 10),
  make_tuple(64, 64, &aom_highbd_sad64x64x3d_c, 10),
  make_tuple(64, 32, &aom_highbd_sad64x32x3d_c, 10),
  make_tuple(32, 64, &aom_highbd_sad32x64x3d_c, 10),
  make_tuple(32, 32, &aom_highbd_sad32x32x3d_c, 10),
  make_tuple(32, 16, &aom_highbd_sad32x16x3d_c, 10),
  make_tuple(16, 32, &aom_highbd_sad16x32x3d_c, 10),
  make_tuple(16, 16, &aom_highbd_sad16x16x3d_c, 10),
  make_tuple(16, 8, &aom_highbd_sad16x8x3d_c, 10),
  make_tuple(8, 16, &aom_highbd_sad8x16x3d_c, 10),
  make_tuple(8, 8, &aom_highbd_sad8x8x3d_c, 10),
  make_tuple(8, 4, &aom_highbd_sad8x4x3d_c, 10),
  make_tuple(4, 8, &aom_highbd_sad4x8x3d_c, 10),
  make_tuple(4, 4, &aom_highbd_sad4x4x3d_c, 10),
  make_tuple(128, 128, &aom_highbd_sad128x128x3d_c, 12),
  make_tuple(128, 64, &aom_highbd_sad128x64x3d_c, 12),
  make_tuple(64, 128, &aom_highbd_sad64x128x3d_c, 12),
  make_tuple(64, 64, &aom_highbd_sad64x64x3d_c, 12),
  make_tuple(64, 32, &aom_highbd_sad64x32x3d_c, 12),
  make_tuple(32, 64, &aom_highbd_sad32x64x3d_c, 12),
  make_tuple(32, 32, &aom_highbd_sad32x32x3d_c, 12),
  make_tuple(32, 16, &aom_highbd_sad32x16x3d_c, 12),
  make_tuple(16, 32, &aom_highbd_sad16x32x3d_c, 12),
  make_tuple(16, 16, &aom_highbd_sad16x16x3d_c, 12),
  make_tuple(16, 8, &aom_highbd_sad16x8x3d_c, 12),
  make_tuple(8, 16, &aom_highbd_sad8x16x3d_c, 12),
  make_tuple(8, 8, &aom_highbd_sad8x8x3d_c, 12),
  make_tuple(8, 4, &aom_highbd_sad8x4x3d_c, 12),
  make_tuple(4, 8, &aom_highbd_sad4x8x3d_c, 12),
  make_tuple(4, 4, &aom_highbd_sad4x4x3d_c, 12),
#endif
#if !CONFIG_REALTIME_ONLY
  make_tuple(64, 16, &aom_sad64x16x3d_c, -1),
  make_tuple(16, 64, &aom_sad16x64x3d_c, -1),
#if CONFIG_AV1_HIGHBITDEPTH
  make_tuple(64, 16, &aom_highbd_sad64x16x3d_c, 8),
  make_tuple(16, 64, &aom_highbd_sad16x64x3d_c, 8),
  make_tuple(64, 16, &aom_highbd_sad64x16x3d_c, 10),
  make_tuple(16, 64, &aom_highbd_sad16x64x3d_c, 10),
  make_tuple(64, 16, &aom_highbd_sad64x16x3d_c, 12),
  make_tuple(16, 64, &aom_highbd_sad16x64x3d_c, 12),
#endif
  make_tuple(32, 8, &aom_sad32x8x3d_c, -1),
  make_tuple(8, 32, &aom_sad8x32x3d_c, -1),
#if CONFIG_AV1_HIGHBITDEPTH
  make_tuple(32, 8, &aom_highbd_sad32x8x3d_c, 8),
  make_tuple(8, 32, &aom_highbd_sad8x32x3d_c, 8),
  make_tuple(32, 8, &aom_highbd_sad32x8x3d_c, 10),
  make_tuple(8, 32, &aom_highbd_sad8x32x3d_c, 10),
  make_tuple(32, 8, &aom_highbd_sad32x8x3d_c, 12),
  make_tuple(8, 32, &aom_highbd_sad8x32x3d_c, 12),
#endif
  make_tuple(16, 4, &aom_sad16x4x3d_c, -1),
  make_tuple(4, 16, &aom_sad4x16x3d_c, -1),
#if CONFIG_AV1_HIGHBITDEPTH
  make_tuple(16, 4, &aom_highbd_sad16x4x3d_c, 8),
  make_tuple(4, 16, &aom_highbd_sad4x16x3d_c, 8),
  make_tuple(16, 4, &aom_highbd_sad16x4x3d_c, 10),
  make_tuple(4, 16, &aom_highbd_sad4x16x3d_c, 10),
  make_tuple(16, 4, &aom_highbd_sad16x4x3d_c, 12),
  make_tuple(4, 16, &aom_highbd_sad4x16x3d_c, 12),
#endif
#endif  // !CONFIG_REALTIME_ONLY
};
INSTANTIATE_TEST_SUITE_P(C, SADx3Test, ::testing::ValuesIn(x3d_c_tests));

const SadMxNx4Param skip_x4d_c_tests[] = {
  make_tuple(128, 128, &aom_sad_skip_128x128x4d_c, -1),
  make_tuple(128, 64, &aom_sad_skip_128x64x4d_c, -1),
  make_tuple(64, 128, &aom_sad_skip_64x128x4d_c, -1),
  make_tuple(64, 64, &aom_sad_skip_64x64x4d_c, -1),
  make_tuple(64, 32, &aom_sad_skip_64x32x4d_c, -1),
  make_tuple(32, 64, &aom_sad_skip_32x64x4d_c, -1),
  make_tuple(32, 32, &aom_sad_skip_32x32x4d_c, -1),
  make_tuple(32, 16, &aom_sad_skip_32x16x4d_c, -1),
  make_tuple(16, 32, &aom_sad_skip_16x32x4d_c, -1),
  make_tuple(16, 16, &aom_sad_skip_16x16x4d_c, -1),
  make_tuple(16, 8, &aom_sad_skip_16x8x4d_c, -1),
  make_tuple(8, 16, &aom_sad_skip_8x16x4d_c, -1),
  make_tuple(8, 8, &aom_sad_skip_8x8x4d_c, -1),
  make_tuple(4, 8, &aom_sad_skip_4x8x4d_c, -1),
#if !CONFIG_REALTIME_ONLY
  make_tuple(64, 16, &aom_sad_skip_64x16x4d_c, -1),
  make_tuple(16, 64, &aom_sad_skip_16x64x4d_c, -1),
  make_tuple(32, 8, &aom_sad_skip_32x8x4d_c, -1),
  make_tuple(8, 32, &aom_sad_skip_8x32x4d_c, -1),
  make_tuple(4, 16, &aom_sad_skip_4x16x4d_c, -1),
#endif
#if CONFIG_AV1_HIGHBITDEPTH
  make_tuple(128, 128, &aom_highbd_sad_skip_128x128x4d_c, 8),
  make_tuple(128, 64, &aom_highbd_sad_skip_128x64x4d_c, 8),
  make_tuple(64, 128, &aom_highbd_sad_skip_64x128x4d_c, 8),
  make_tuple(64, 64, &aom_highbd_sad_skip_64x64x4d_c, 8),
  make_tuple(64, 32, &aom_highbd_sad_skip_64x32x4d_c, 8),
  make_tuple(32, 64, &aom_highbd_sad_skip_32x64x4d_c, 8),
  make_tuple(32, 32, &aom_highbd_sad_skip_32x32x4d_c, 8),
  make_tuple(32, 16, &aom_highbd_sad_skip_32x16x4d_c, 8),
  make_tuple(16, 32, &aom_highbd_sad_skip_16x32x4d_c, 8),
  make_tuple(16, 16, &aom_highbd_sad_skip_16x16x4d_c, 8),
  make_tuple(16, 8, &aom_highbd_sad_skip_16x8x4d_c, 8),
  make_tuple(8, 16, &aom_highbd_sad_skip_8x16x4d_c, 8),
  make_tuple(8, 8, &aom_highbd_sad_skip_8x8x4d_c, 8),
  make_tuple(8, 4, &aom_highbd_sad_skip_8x4x4d_c, 8),
  make_tuple(4, 8, &aom_highbd_sad_skip_4x8x4d_c, 8),
  make_tuple(4, 4, &aom_highbd_sad_skip_4x4x4d_c, 8),
#if !CONFIG_REALTIME_ONLY
  make_tuple(64, 16, &aom_highbd_sad_skip_64x16x4d_c, 8),
  make_tuple(16, 64, &aom_highbd_sad_skip_16x64x4d_c, 8),
  make_tuple(32, 8, &aom_highbd_sad_skip_32x8x4d_c, 8),
  make_tuple(8, 32, &aom_highbd_sad_skip_8x32x4d_c, 8),
  make_tuple(16, 4, &aom_highbd_sad_skip_16x4x4d_c, 8),
  make_tuple(4, 16, &aom_highbd_sad_skip_4x16x4d_c, 8),
#endif

  make_tuple(128, 128, &aom_highbd_sad_skip_128x128x4d_c, 10),
  make_tuple(128, 64, &aom_highbd_sad_skip_128x64x4d_c, 10),
  make_tuple(64, 128, &aom_highbd_sad_skip_64x128x4d_c, 10),
  make_tuple(64, 64, &aom_highbd_sad_skip_64x64x4d_c, 10),
  make_tuple(64, 32, &aom_highbd_sad_skip_64x32x4d_c, 10),
  make_tuple(32, 64, &aom_highbd_sad_skip_32x64x4d_c, 10),
  make_tuple(32, 32, &aom_highbd_sad_skip_32x32x4d_c, 10),
  make_tuple(32, 16, &aom_highbd_sad_skip_32x16x4d_c, 10),
  make_tuple(16, 32, &aom_highbd_sad_skip_16x32x4d_c, 10),
  make_tuple(16, 16, &aom_highbd_sad_skip_16x16x4d_c, 10),
  make_tuple(16, 8, &aom_highbd_sad_skip_16x8x4d_c, 10),
  make_tuple(8, 16, &aom_highbd_sad_skip_8x16x4d_c, 10),
  make_tuple(8, 8, &aom_highbd_sad_skip_8x8x4d_c, 10),
  make_tuple(8, 4, &aom_highbd_sad_skip_8x4x4d_c, 10),
  make_tuple(4, 8, &aom_highbd_sad_skip_4x8x4d_c, 10),
  make_tuple(4, 4, &aom_highbd_sad_skip_4x4x4d_c, 10),
#if !CONFIG_REALTIME_ONLY
  make_tuple(64, 16, &aom_highbd_sad_skip_64x16x4d_c, 10),
  make_tuple(16, 64, &aom_highbd_sad_skip_16x64x4d_c, 10),
  make_tuple(32, 8, &aom_highbd_sad_skip_32x8x4d_c, 10),
  make_tuple(8, 32, &aom_highbd_sad_skip_8x32x4d_c, 10),
  make_tuple(16, 4, &aom_highbd_sad_skip_16x4x4d_c, 10),
  make_tuple(4, 16, &aom_highbd_sad_skip_4x16x4d_c, 10),
#endif

  make_tuple(128, 128, &aom_highbd_sad_skip_128x128x4d_c, 12),
  make_tuple(128, 64, &aom_highbd_sad_skip_128x64x4d_c, 12),
  make_tuple(64, 128, &aom_highbd_sad_skip_64x128x4d_c, 12),
  make_tuple(64, 64, &aom_highbd_sad_skip_64x64x4d_c, 12),
  make_tuple(64, 32, &aom_highbd_sad_skip_64x32x4d_c, 12),
  make_tuple(32, 64, &aom_highbd_sad_skip_32x64x4d_c, 12),
  make_tuple(32, 32, &aom_highbd_sad_skip_32x32x4d_c, 12),
  make_tuple(32, 16, &aom_highbd_sad_skip_32x16x4d_c, 12),
  make_tuple(16, 32, &aom_highbd_sad_skip_16x32x4d_c, 12),
  make_tuple(16, 16, &aom_highbd_sad_skip_16x16x4d_c, 12),
  make_tuple(16, 8, &aom_highbd_sad_skip_16x8x4d_c, 12),
  make_tuple(8, 16, &aom_highbd_sad_skip_8x16x4d_c, 12),
  make_tuple(8, 8, &aom_highbd_sad_skip_8x8x4d_c, 12),
  make_tuple(8, 4, &aom_highbd_sad_skip_8x4x4d_c, 12),
  make_tuple(4, 8, &aom_highbd_sad_skip_4x8x4d_c, 12),
  make_tuple(4, 4, &aom_highbd_sad_skip_4x4x4d_c, 12),
#if !CONFIG_REALTIME_ONLY
  make_tuple(64, 16, &aom_highbd_sad_skip_64x16x4d_c, 12),
  make_tuple(16, 64, &aom_highbd_sad_skip_16x64x4d_c, 12),
  make_tuple(32, 8, &aom_highbd_sad_skip_32x8x4d_c, 12),
  make_tuple(8, 32, &aom_highbd_sad_skip_8x32x4d_c, 12),
  make_tuple(16, 4, &aom_highbd_sad_skip_16x4x4d_c, 12),
  make_tuple(4, 16, &aom_highbd_sad_skip_4x16x4d_c, 12),
#endif
#endif  // CONFIG_AV1_HIGHBITDEPTH
};
INSTANTIATE_TEST_SUITE_P(C, SADSkipx4Test,
                         ::testing::ValuesIn(skip_x4d_c_tests));

//------------------------------------------------------------------------------
// ARM functions
#if HAVE_NEON
const SadMxNParam neon_tests[] = {
  make_tuple(128, 128, &aom_sad128x128_neon, -1),
  make_tuple(128, 64, &aom_sad128x64_neon, -1),
  make_tuple(64, 128, &aom_sad64x128_neon, -1),
  make_tuple(64, 64, &aom_sad64x64_neon, -1),
  make_tuple(64, 32, &aom_sad64x32_neon, -1),
  make_tuple(32, 64, &aom_sad32x64_neon, -1),
  make_tuple(32, 32, &aom_sad32x32_neon, -1),
  make_tuple(32, 16, &aom_sad32x16_neon, -1),
  make_tuple(16, 32, &aom_sad16x32_neon, -1),
  make_tuple(16, 16, &aom_sad16x16_neon, -1),
  make_tuple(16, 8, &aom_sad16x8_neon, -1),
  make_tuple(8, 16, &aom_sad8x16_neon, -1),
  make_tuple(8, 8, &aom_sad8x8_neon, -1),
  make_tuple(8, 4, &aom_sad8x4_neon, -1),
  make_tuple(4, 8, &aom_sad4x8_neon, -1),
  make_tuple(4, 4, &aom_sad4x4_neon, -1),
#if !CONFIG_REALTIME_ONLY
  make_tuple(64, 16, &aom_sad64x16_neon, -1),
  make_tuple(32, 8, &aom_sad32x8_neon, -1),
  make_tuple(16, 64, &aom_sad16x64_neon, -1),
  make_tuple(16, 4, &aom_sad16x4_neon, -1),
  make_tuple(8, 32, &aom_sad8x32_neon, -1),
  make_tuple(4, 16, &aom_sad4x16_neon, -1),
#endif
};
INSTANTIATE_TEST_SUITE_P(NEON, SADTest, ::testing::ValuesIn(neon_tests));

const SadMxNx4Param x4d_neon_tests[] = {
  make_tuple(128, 128, &aom_sad128x128x4d_neon, -1),
  make_tuple(128, 64, &aom_sad128x64x4d_neon, -1),
  make_tuple(64, 128, &aom_sad64x128x4d_neon, -1),
  make_tuple(64, 64, &aom_sad64x64x4d_neon, -1),
  make_tuple(64, 32, &aom_sad64x32x4d_neon, -1),
  make_tuple(32, 64, &aom_sad32x64x4d_neon, -1),
  make_tuple(32, 32, &aom_sad32x32x4d_neon, -1),
  make_tuple(32, 16, &aom_sad32x16x4d_neon, -1),
  make_tuple(16, 32, &aom_sad16x32x4d_neon, -1),
  make_tuple(16, 16, &aom_sad16x16x4d_neon, -1),
  make_tuple(16, 8, &aom_sad16x8x4d_neon, -1),
  make_tuple(8, 16, &aom_sad8x16x4d_neon, -1),
  make_tuple(8, 8, &aom_sad8x8x4d_neon, -1),
  make_tuple(8, 4, &aom_sad8x4x4d_neon, -1),
  make_tuple(4, 8, &aom_sad4x8x4d_neon, -1),
  make_tuple(4, 4, &aom_sad4x4x4d_neon, -1),
#if !CONFIG_REALTIME_ONLY
  make_tuple(64, 16, &aom_sad64x16x4d_neon, -1),
  make_tuple(32, 8, &aom_sad32x8x4d_neon, -1),
  make_tuple(16, 64, &aom_sad16x64x4d_neon, -1),
  make_tuple(16, 4, &aom_sad16x4x4d_neon, -1),
  make_tuple(8, 32, &aom_sad8x32x4d_neon, -1),
  make_tuple(4, 16, &aom_sad4x16x4d_neon, -1),
#endif
};
INSTANTIATE_TEST_SUITE_P(NEON, SADx4Test, ::testing::ValuesIn(x4d_neon_tests));
const SadSkipMxNParam skip_neon_tests[] = {
  make_tuple(128, 128, &aom_sad_skip_128x128_neon, -1),
  make_tuple(128, 64, &aom_sad_skip_128x64_neon, -1),
  make_tuple(64, 128, &aom_sad_skip_64x128_neon, -1),
  make_tuple(64, 64, &aom_sad_skip_64x64_neon, -1),
  make_tuple(64, 32, &aom_sad_skip_64x32_neon, -1),
  make_tuple(32, 64, &aom_sad_skip_32x64_neon, -1),
  make_tuple(32, 32, &aom_sad_skip_32x32_neon, -1),
  make_tuple(32, 16, &aom_sad_skip_32x16_neon, -1),
  make_tuple(16, 32, &aom_sad_skip_16x32_neon, -1),
  make_tuple(16, 16, &aom_sad_skip_16x16_neon, -1),
  make_tuple(16, 8, &aom_sad_skip_16x8_neon, -1),
  make_tuple(8, 16, &aom_sad_skip_8x16_neon, -1),
  make_tuple(8, 8, &aom_sad_skip_8x8_neon, -1),
  make_tuple(4, 8, &aom_sad_skip_4x8_neon, -1),
#if !CONFIG_REALTIME_ONLY
  make_tuple(64, 16, &aom_sad_skip_64x16_neon, -1),
  make_tuple(32, 8, &aom_sad_skip_32x8_neon, -1),
  make_tuple(16, 64, &aom_sad_skip_16x64_neon, -1),
  make_tuple(8, 32, &aom_sad_skip_8x32_neon, -1),
  make_tuple(4, 16, &aom_sad_skip_4x16_neon, -1),
#endif
};
INSTANTIATE_TEST_SUITE_P(NEON, SADSkipTest,
                         ::testing::ValuesIn(skip_neon_tests));

const SadSkipMxNx4Param skip_x4d_neon_tests[] = {
  make_tuple(128, 128, &aom_sad_skip_128x128x4d_neon, -1),
  make_tuple(128, 64, &aom_sad_skip_128x64x4d_neon, -1),
  make_tuple(64, 128, &aom_sad_skip_64x128x4d_neon, -1),
  make_tuple(64, 64, &aom_sad_skip_64x64x4d_neon, -1),
  make_tuple(64, 32, &aom_sad_skip_64x32x4d_neon, -1),
  make_tuple(32, 64, &aom_sad_skip_32x64x4d_neon, -1),
  make_tuple(32, 32, &aom_sad_skip_32x32x4d_neon, -1),
  make_tuple(32, 16, &aom_sad_skip_32x16x4d_neon, -1),
  make_tuple(16, 32, &aom_sad_skip_16x32x4d_neon, -1),
  make_tuple(16, 16, &aom_sad_skip_16x16x4d_neon, -1),
  make_tuple(16, 8, &aom_sad_skip_16x8x4d_neon, -1),
  make_tuple(8, 8, &aom_sad_skip_8x8x4d_neon, -1),
  make_tuple(8, 16, &aom_sad_skip_8x16x4d_neon, -1),
  make_tuple(4, 8, &aom_sad_skip_4x8x4d_neon, -1),
#if !CONFIG_REALTIME_ONLY
  make_tuple(64, 16, &aom_sad_skip_64x16x4d_neon, -1),
  make_tuple(32, 8, &aom_sad_skip_32x8x4d_neon, -1),
  make_tuple(16, 64, &aom_sad_skip_16x64x4d_neon, -1),
  make_tuple(8, 32, &aom_sad_skip_8x32x4d_neon, -1),
  make_tuple(4, 16, &aom_sad_skip_4x16x4d_neon, -1),
#endif
};
INSTANTIATE_TEST_SUITE_P(NEON, SADSkipx4Test,
                         ::testing::ValuesIn(skip_x4d_neon_tests));

const SadMxNAvgParam avg_neon_tests[] = {
  make_tuple(128, 128, &aom_sad128x128_avg_neon, -1),
  make_tuple(128, 64, &aom_sad128x64_avg_neon, -1),
  make_tuple(64, 128, &aom_sad64x128_avg_neon, -1),
  make_tuple(64, 64, &aom_sad64x64_avg_neon, -1),
  make_tuple(64, 32, &aom_sad64x32_avg_neon, -1),
  make_tuple(32, 64, &aom_sad32x64_avg_neon, -1),
  make_tuple(32, 32, &aom_sad32x32_avg_neon, -1),
  make_tuple(32, 16, &aom_sad32x16_avg_neon, -1),
  make_tuple(16, 32, &aom_sad16x32_avg_neon, -1),
  make_tuple(16, 16, &aom_sad16x16_avg_neon, -1),
  make_tuple(16, 8, &aom_sad16x8_avg_neon, -1),
  make_tuple(8, 16, &aom_sad8x16_avg_neon, -1),
  make_tuple(8, 8, &aom_sad8x8_avg_neon, -1),
  make_tuple(8, 4, &aom_sad8x4_avg_neon, -1),
  make_tuple(4, 8, &aom_sad4x8_avg_neon, -1),
  make_tuple(4, 4, &aom_sad4x4_avg_neon, -1),
#if !CONFIG_REALTIME_ONLY
  make_tuple(64, 16, &aom_sad64x16_avg_neon, -1),
  make_tuple(32, 8, &aom_sad32x8_avg_neon, -1),
  make_tuple(16, 64, &aom_sad16x64_avg_neon, -1),
  make_tuple(16, 4, &aom_sad16x4_avg_neon, -1),
  make_tuple(8, 32, &aom_sad8x32_avg_neon, -1),
  make_tuple(4, 16, &aom_sad4x16_avg_neon, -1),
#endif
};
INSTANTIATE_TEST_SUITE_P(NEON, SADavgTest, ::testing::ValuesIn(avg_neon_tests));

#endif  // HAVE_NEON

//------------------------------------------------------------------------------
// x86 functions
#if HAVE_SSE2
const SadMxNParam sse2_tests[] = {
  make_tuple(128, 128, &aom_sad128x128_sse2, -1),
  make_tuple(128, 64, &aom_sad128x64_sse2, -1),
  make_tuple(64, 128, &aom_sad64x128_sse2, -1),
  make_tuple(64, 64, &aom_sad64x64_sse2, -1),
  make_tuple(64, 32, &aom_sad64x32_sse2, -1),
  make_tuple(32, 64, &aom_sad32x64_sse2, -1),
  make_tuple(32, 32, &aom_sad32x32_sse2, -1),
  make_tuple(32, 16, &aom_sad32x16_sse2, -1),
  make_tuple(16, 32, &aom_sad16x32_sse2, -1),
  make_tuple(16, 16, &aom_sad16x16_sse2, -1),
  make_tuple(16, 8, &aom_sad16x8_sse2, -1),
  make_tuple(8, 16, &aom_sad8x16_sse2, -1),
  make_tuple(8, 8, &aom_sad8x8_sse2, -1),
  make_tuple(8, 4, &aom_sad8x4_sse2, -1),
  make_tuple(4, 8, &aom_sad4x8_sse2, -1),
  make_tuple(4, 4, &aom_sad4x4_sse2, -1),
#if CONFIG_AV1_HIGHBITDEPTH
  make_tuple(64, 64, &aom_highbd_sad64x64_sse2, 8),
  make_tuple(64, 32, &aom_highbd_sad64x32_sse2, 8),
  make_tuple(32, 64, &aom_highbd_sad32x64_sse2, 8),
  make_tuple(32, 32, &aom_highbd_sad32x32_sse2, 8),
  make_tuple(32, 16, &aom_highbd_sad32x16_sse2, 8),
  make_tuple(16, 32, &aom_highbd_sad16x32_sse2, 8),
  make_tuple(16, 16, &aom_highbd_sad16x16_sse2, 8),
  make_tuple(16, 8, &aom_highbd_sad16x8_sse2, 8),
  make_tuple(8, 16, &aom_highbd_sad8x16_sse2, 8),
  make_tuple(8, 8, &aom_highbd_sad8x8_sse2, 8),
  make_tuple(8, 4, &aom_highbd_sad8x4_sse2, 8),
  make_tuple(4, 8, &aom_highbd_sad4x8_sse2, 8),
  make_tuple(4, 4, &aom_highbd_sad4x4_sse2, 8),
  make_tuple(64, 64, &aom_highbd_sad64x64_sse2, 10),
  make_tuple(64, 32, &aom_highbd_sad64x32_sse2, 10),
  make_tuple(32, 64, &aom_highbd_sad32x64_sse2, 10),
  make_tuple(32, 32, &aom_highbd_sad32x32_sse2, 10),
  make_tuple(32, 16, &aom_highbd_sad32x16_sse2, 10),
  make_tuple(16, 32, &aom_highbd_sad16x32_sse2, 10),
  make_tuple(16, 16, &aom_highbd_sad16x16_sse2, 10),
  make_tuple(16, 8, &aom_highbd_sad16x8_sse2, 10),
  make_tuple(8, 16, &aom_highbd_sad8x16_sse2, 10),
  make_tuple(8, 8, &aom_highbd_sad8x8_sse2, 10),
  make_tuple(8, 4, &aom_highbd_sad8x4_sse2, 10),
  make_tuple(4, 8, &aom_highbd_sad4x8_sse2, 10),
  make_tuple(4, 4, &aom_highbd_sad4x4_sse2, 10),
  make_tuple(64, 64, &aom_highbd_sad64x64_sse2, 12),
  make_tuple(64, 32, &aom_highbd_sad64x32_sse2, 12),
  make_tuple(32, 64, &aom_highbd_sad32x64_sse2, 12),
  make_tuple(32, 32, &aom_highbd_sad32x32_sse2, 12),
  make_tuple(32, 16, &aom_highbd_sad32x16_sse2, 12),
  make_tuple(16, 32, &aom_highbd_sad16x32_sse2, 12),
  make_tuple(16, 16, &aom_highbd_sad16x16_sse2, 12),
  make_tuple(16, 8, &aom_highbd_sad16x8_sse2, 12),
  make_tuple(8, 16, &aom_highbd_sad8x16_sse2, 12),
  make_tuple(8, 8, &aom_highbd_sad8x8_sse2, 12),
  make_tuple(8, 4, &aom_highbd_sad8x4_sse2, 12),
  make_tuple(4, 8, &aom_highbd_sad4x8_sse2, 12),
  make_tuple(4, 4, &aom_highbd_sad4x4_sse2, 12),
#endif
#if !CONFIG_REALTIME_ONLY
  make_tuple(64, 16, &aom_sad64x16_sse2, -1),
  make_tuple(16, 64, &aom_sad16x64_sse2, -1),
#if CONFIG_AV1_HIGHBITDEPTH
  make_tuple(64, 16, &aom_highbd_sad64x16_sse2, 8),
  make_tuple(16, 64, &aom_highbd_sad16x64_sse2, 8),
  make_tuple(64, 16, &aom_highbd_sad64x16_sse2, 10),
  make_tuple(16, 64, &aom_highbd_sad16x64_sse2, 10),
  make_tuple(64, 16, &aom_highbd_sad64x16_sse2, 12),
  make_tuple(16, 64, &aom_highbd_sad16x64_sse2, 12),
#endif
  make_tuple(32, 8, &aom_sad32x8_sse2, -1),
  make_tuple(8, 32, &aom_sad8x32_sse2, -1),
#if CONFIG_AV1_HIGHBITDEPTH
  make_tuple(32, 8, &aom_highbd_sad32x8_sse2, 8),
  make_tuple(8, 32, &aom_highbd_sad8x32_sse2, 8),
  make_tuple(32, 8, &aom_highbd_sad32x8_sse2, 10),
  make_tuple(8, 32, &aom_highbd_sad8x32_sse2, 10),
  make_tuple(32, 8, &aom_highbd_sad32x8_sse2, 12),
  make_tuple(8, 32, &aom_highbd_sad8x32_sse2, 12),
#endif
  make_tuple(16, 4, &aom_sad16x4_sse2, -1),
  make_tuple(4, 16, &aom_sad4x16_sse2, -1),
#if CONFIG_AV1_HIGHBITDEPTH
  make_tuple(16, 4, &aom_highbd_sad16x4_sse2, 8),
  make_tuple(4, 16, &aom_highbd_sad4x16_sse2, 8),
  make_tuple(16, 4, &aom_highbd_sad16x4_sse2, 10),
  make_tuple(4, 16, &aom_highbd_sad4x16_sse2, 10),
  make_tuple(16, 4, &aom_highbd_sad16x4_sse2, 12),
  make_tuple(4, 16, &aom_highbd_sad4x16_sse2, 12),
#endif
#endif  // !CONFIG_REALTIME_ONLY
};
INSTANTIATE_TEST_SUITE_P(SSE2, SADTest, ::testing::ValuesIn(sse2_tests));

const SadSkipMxNParam skip_sse2_tests[] = {
  make_tuple(128, 128, &aom_sad_skip_128x128_sse2, -1),
  make_tuple(128, 64, &aom_sad_skip_128x64_sse2, -1),
  make_tuple(64, 128, &aom_sad_skip_64x128_sse2, -1),
  make_tuple(64, 64, &aom_sad_skip_64x64_sse2, -1),
  make_tuple(64, 32, &aom_sad_skip_64x32_sse2, -1),
  make_tuple(32, 64, &aom_sad_skip_32x64_sse2, -1),
  make_tuple(32, 32, &aom_sad_skip_32x32_sse2, -1),
  make_tuple(32, 16, &aom_sad_skip_32x16_sse2, -1),
  make_tuple(16, 32, &aom_sad_skip_16x32_sse2, -1),
  make_tuple(16, 16, &aom_sad_skip_16x16_sse2, -1),
  make_tuple(16, 8, &aom_sad_skip_16x8_sse2, -1),
  make_tuple(8, 16, &aom_sad_skip_8x16_sse2, -1),
  make_tuple(8, 8, &aom_sad_skip_8x8_sse2, -1),
  make_tuple(4, 8, &aom_sad_skip_4x8_sse2, -1),
#if !CONFIG_REALTIME_ONLY
  make_tuple(64, 16, &aom_sad_skip_64x16_sse2, -1),
  make_tuple(16, 64, &aom_sad_skip_16x64_sse2, -1),
  make_tuple(32, 8, &aom_sad_skip_32x8_sse2, -1),
  make_tuple(8, 32, &aom_sad_skip_8x32_sse2, -1),
  make_tuple(4, 16, &aom_sad_skip_4x16_sse2, -1),
#endif
#if CONFIG_AV1_HIGHBITDEPTH
  make_tuple(64, 64, &aom_highbd_sad_skip_64x64_sse2, 8),
  make_tuple(64, 32, &aom_highbd_sad_skip_64x32_sse2, 8),
  make_tuple(32, 64, &aom_highbd_sad_skip_32x64_sse2, 8),
  make_tuple(32, 32, &aom_highbd_sad_skip_32x32_sse2, 8),
  make_tuple(32, 16, &aom_highbd_sad_skip_32x16_sse2, 8),
  make_tuple(16, 32, &aom_highbd_sad_skip_16x32_sse2, 8),
  make_tuple(16, 16, &aom_highbd_sad_skip_16x16_sse2, 8),
  make_tuple(16, 8, &aom_highbd_sad_skip_16x8_sse2, 8),
  make_tuple(8, 16, &aom_highbd_sad_skip_8x16_sse2, 8),
  make_tuple(8, 8, &aom_highbd_sad_skip_8x8_sse2, 8),
  make_tuple(4, 8, &aom_highbd_sad_skip_4x8_sse2, 8),
#if !CONFIG_REALTIME_ONLY
  make_tuple(64, 16, &aom_highbd_sad_skip_64x16_sse2, 8),
  make_tuple(16, 64, &aom_highbd_sad_skip_16x64_sse2, 8),
  make_tuple(32, 8, &aom_highbd_sad_skip_32x8_sse2, 8),
  make_tuple(8, 32, &aom_highbd_sad_skip_8x32_sse2, 8),
  make_tuple(4, 16, &aom_highbd_sad_skip_4x16_sse2, 8),
#endif

  make_tuple(64, 64, &aom_highbd_sad_skip_64x64_sse2, 10),
  make_tuple(64, 32, &aom_highbd_sad_skip_64x32_sse2, 10),
  make_tuple(32, 64, &aom_highbd_sad_skip_32x64_sse2, 10),
  make_tuple(32, 32, &aom_highbd_sad_skip_32x32_sse2, 10),
  make_tuple(32, 16, &aom_highbd_sad_skip_32x16_sse2, 10),
  make_tuple(16, 32, &aom_highbd_sad_skip_16x32_sse2, 10),
  make_tuple(16, 16, &aom_highbd_sad_skip_16x16_sse2, 10),
  make_tuple(16, 8, &aom_highbd_sad_skip_16x8_sse2, 10),
  make_tuple(8, 16, &aom_highbd_sad_skip_8x16_sse2, 10),
  make_tuple(8, 8, &aom_highbd_sad_skip_8x8_sse2, 10),
  make_tuple(4, 8, &aom_highbd_sad_skip_4x8_sse2, 10),
#if !CONFIG_REALTIME_ONLY
  make_tuple(64, 16, &aom_highbd_sad_skip_64x16_sse2, 10),
  make_tuple(16, 64, &aom_highbd_sad_skip_16x64_sse2, 10),
  make_tuple(32, 8, &aom_highbd_sad_skip_32x8_sse2, 10),
  make_tuple(8, 32, &aom_highbd_sad_skip_8x32_sse2, 10),
  make_tuple(4, 16, &aom_highbd_sad_skip_4x16_sse2, 10),
#endif

  make_tuple(64, 64, &aom_highbd_sad_skip_64x64_sse2, 12),
  make_tuple(64, 32, &aom_highbd_sad_skip_64x32_sse2, 12),
  make_tuple(32, 64, &aom_highbd_sad_skip_32x64_sse2, 12),
  make_tuple(32, 32, &aom_highbd_sad_skip_32x32_sse2, 12),
  make_tuple(32, 16, &aom_highbd_sad_skip_32x16_sse2, 12),
  make_tuple(16, 32, &aom_highbd_sad_skip_16x32_sse2, 12),
  make_tuple(16, 16, &aom_highbd_sad_skip_16x16_sse2, 12),
  make_tuple(16, 8, &aom_highbd_sad_skip_16x8_sse2, 12),
  make_tuple(8, 16, &aom_highbd_sad_skip_8x16_sse2, 12),
  make_tuple(8, 8, &aom_highbd_sad_skip_8x8_sse2, 12),
  make_tuple(4, 8, &aom_highbd_sad_skip_4x8_sse2, 12),
#if !CONFIG_REALTIME_ONLY
  make_tuple(64, 16, &aom_highbd_sad_skip_64x16_sse2, 12),
  make_tuple(16, 64, &aom_highbd_sad_skip_16x64_sse2, 12),
  make_tuple(32, 8, &aom_highbd_sad_skip_32x8_sse2, 12),
  make_tuple(8, 32, &aom_highbd_sad_skip_8x32_sse2, 12),
  make_tuple(4, 16, &aom_highbd_sad_skip_4x16_sse2, 12),
#endif
#endif  // CONFIG_AV1_HIGHBITDEPTH
};
INSTANTIATE_TEST_SUITE_P(SSE2, SADSkipTest,
                         ::testing::ValuesIn(skip_sse2_tests));

const SadMxNAvgParam avg_sse2_tests[] = {
  make_tuple(128, 128, &aom_sad128x128_avg_sse2, -1),
  make_tuple(128, 64, &aom_sad128x64_avg_sse2, -1),
  make_tuple(64, 128, &aom_sad64x128_avg_sse2, -1),
  make_tuple(64, 64, &aom_sad64x64_avg_sse2, -1),
  make_tuple(64, 32, &aom_sad64x32_avg_sse2, -1),
  make_tuple(32, 64, &aom_sad32x64_avg_sse2, -1),
  make_tuple(32, 32, &aom_sad32x32_avg_sse2, -1),
  make_tuple(32, 16, &aom_sad32x16_avg_sse2, -1),
  make_tuple(16, 32, &aom_sad16x32_avg_sse2, -1),
  make_tuple(16, 16, &aom_sad16x16_avg_sse2, -1),
  make_tuple(16, 8, &aom_sad16x8_avg_sse2, -1),
  make_tuple(8, 16, &aom_sad8x16_avg_sse2, -1),
  make_tuple(8, 8, &aom_sad8x8_avg_sse2, -1),
  make_tuple(8, 4, &aom_sad8x4_avg_sse2, -1),
  make_tuple(4, 8, &aom_sad4x8_avg_sse2, -1),
  make_tuple(4, 4, &aom_sad4x4_avg_sse2, -1),
#if CONFIG_AV1_HIGHBITDEPTH
  make_tuple(64, 64, &aom_highbd_sad64x64_avg_sse2, 8),
  make_tuple(64, 32, &aom_highbd_sad64x32_avg_sse2, 8),
  make_tuple(32, 64, &aom_highbd_sad32x64_avg_sse2, 8),
  make_tuple(32, 32, &aom_highbd_sad32x32_avg_sse2, 8),
  make_tuple(32, 16, &aom_highbd_sad32x16_avg_sse2, 8),
  make_tuple(16, 32, &aom_highbd_sad16x32_avg_sse2, 8),
  make_tuple(16, 16, &aom_highbd_sad16x16_avg_sse2, 8),
  make_tuple(16, 8, &aom_highbd_sad16x8_avg_sse2, 8),
  make_tuple(8, 16, &aom_highbd_sad8x16_avg_sse2, 8),
  make_tuple(8, 8, &aom_highbd_sad8x8_avg_sse2, 8),
  make_tuple(8, 4, &aom_highbd_sad8x4_avg_sse2, 8),
  make_tuple(4, 8, &aom_highbd_sad4x8_avg_sse2, 8),
  make_tuple(4, 4, &aom_highbd_sad4x4_avg_sse2, 8),
  make_tuple(64, 64, &aom_highbd_sad64x64_avg_sse2, 10),
  make_tuple(64, 32, &aom_highbd_sad64x32_avg_sse2, 10),
  make_tuple(32, 64, &aom_highbd_sad32x64_avg_sse2, 10),
  make_tuple(32, 32, &aom_highbd_sad32x32_avg_sse2, 10),
  make_tuple(32, 16, &aom_highbd_sad32x16_avg_sse2, 10),
  make_tuple(16, 32, &aom_highbd_sad16x32_avg_sse2, 10),
  make_tuple(16, 16, &aom_highbd_sad16x16_avg_sse2, 10),
  make_tuple(16, 8, &aom_highbd_sad16x8_avg_sse2, 10),
  make_tuple(8, 16, &aom_highbd_sad8x16_avg_sse2, 10),
  make_tuple(8, 8, &aom_highbd_sad8x8_avg_sse2, 10),
  make_tuple(8, 4, &aom_highbd_sad8x4_avg_sse2, 10),
  make_tuple(4, 8, &aom_highbd_sad4x8_avg_sse2, 10),
  make_tuple(4, 4, &aom_highbd_sad4x4_avg_sse2, 10),
  make_tuple(64, 64, &aom_highbd_sad64x64_avg_sse2, 12),
  make_tuple(64, 32, &aom_highbd_sad64x32_avg_sse2, 12),
  make_tuple(32, 64, &aom_highbd_sad32x64_avg_sse2, 12),
  make_tuple(32, 32, &aom_highbd_sad32x32_avg_sse2, 12),
  make_tuple(32, 16, &aom_highbd_sad32x16_avg_sse2, 12),
  make_tuple(16, 32, &aom_highbd_sad16x32_avg_sse2, 12),
  make_tuple(16, 16, &aom_highbd_sad16x16_avg_sse2, 12),
  make_tuple(16, 8, &aom_highbd_sad16x8_avg_sse2, 12),
  make_tuple(8, 16, &aom_highbd_sad8x16_avg_sse2, 12),
  make_tuple(8, 8, &aom_highbd_sad8x8_avg_sse2, 12),
  make_tuple(8, 4, &aom_highbd_sad8x4_avg_sse2, 12),
  make_tuple(4, 8, &aom_highbd_sad4x8_avg_sse2, 12),
  make_tuple(4, 4, &aom_highbd_sad4x4_avg_sse2, 12),
#endif
#if !CONFIG_REALTIME_ONLY
  make_tuple(64, 16, &aom_sad64x16_avg_sse2, -1),
  make_tuple(16, 64, &aom_sad16x64_avg_sse2, -1),
#if CONFIG_AV1_HIGHBITDEPTH
  make_tuple(64, 16, &aom_highbd_sad64x16_avg_sse2, 8),
  make_tuple(16, 64, &aom_highbd_sad16x64_avg_sse2, 8),
  make_tuple(64, 16, &aom_highbd_sad64x16_avg_sse2, 10),
  make_tuple(16, 64, &aom_highbd_sad16x64_avg_sse2, 10),
  make_tuple(64, 16, &aom_highbd_sad64x16_avg_sse2, 12),
  make_tuple(16, 64, &aom_highbd_sad16x64_avg_sse2, 12),
#endif
  make_tuple(32, 8, &aom_sad32x8_avg_sse2, -1),
  make_tuple(8, 32, &aom_sad8x32_avg_sse2, -1),
#if CONFIG_AV1_HIGHBITDEPTH
  make_tuple(32, 8, &aom_highbd_sad32x8_avg_sse2, 8),
  make_tuple(8, 32, &aom_highbd_sad8x32_avg_sse2, 8),
  make_tuple(32, 8, &aom_highbd_sad32x8_avg_sse2, 10),
  make_tuple(8, 32, &aom_highbd_sad8x32_avg_sse2, 10),
  make_tuple(32, 8, &aom_highbd_sad32x8_avg_sse2, 12),
  make_tuple(8, 32, &aom_highbd_sad8x32_avg_sse2, 12),
#endif
  make_tuple(16, 4, &aom_sad16x4_avg_sse2, -1),
  make_tuple(4, 16, &aom_sad4x16_avg_sse2, -1),
#if CONFIG_AV1_HIGHBITDEPTH
  make_tuple(16, 4, &aom_highbd_sad16x4_avg_sse2, 8),
  make_tuple(4, 16, &aom_highbd_sad4x16_avg_sse2, 8),
  make_tuple(16, 4, &aom_highbd_sad16x4_avg_sse2, 10),
  make_tuple(4, 16, &aom_highbd_sad4x16_avg_sse2, 10),
  make_tuple(16, 4, &aom_highbd_sad16x4_avg_sse2, 12),
  make_tuple(4, 16, &aom_highbd_sad4x16_avg_sse2, 12),
#endif
#endif  // !CONFIG_REALTIME_ONLY
};
INSTANTIATE_TEST_SUITE_P(SSE2, SADavgTest, ::testing::ValuesIn(avg_sse2_tests));

const SadMxNx4Param x4d_sse2_tests[] = {
  make_tuple(128, 128, &aom_sad128x128x4d_sse2, -1),
  make_tuple(128, 64, &aom_sad128x64x4d_sse2, -1),
  make_tuple(64, 128, &aom_sad64x128x4d_sse2, -1),
  make_tuple(64, 64, &aom_sad64x64x4d_sse2, -1),
  make_tuple(64, 32, &aom_sad64x32x4d_sse2, -1),
  make_tuple(32, 64, &aom_sad32x64x4d_sse2, -1),
  make_tuple(32, 32, &aom_sad32x32x4d_sse2, -1),
  make_tuple(32, 16, &aom_sad32x16x4d_sse2, -1),
  make_tuple(16, 32, &aom_sad16x32x4d_sse2, -1),
  make_tuple(16, 16, &aom_sad16x16x4d_sse2, -1),
  make_tuple(16, 8, &aom_sad16x8x4d_sse2, -1),
  make_tuple(8, 16, &aom_sad8x16x4d_sse2, -1),
  make_tuple(8, 8, &aom_sad8x8x4d_sse2, -1),
  make_tuple(8, 4, &aom_sad8x4x4d_sse2, -1),
  make_tuple(4, 8, &aom_sad4x8x4d_sse2, -1),
  make_tuple(4, 4, &aom_sad4x4x4d_sse2, -1),
#if CONFIG_AV1_HIGHBITDEPTH
  make_tuple(64, 64, &aom_highbd_sad64x64x4d_sse2, 8),
  make_tuple(64, 32, &aom_highbd_sad64x32x4d_sse2, 8),
  make_tuple(32, 64, &aom_highbd_sad32x64x4d_sse2, 8),
  make_tuple(32, 32, &aom_highbd_sad32x32x4d_sse2, 8),
  make_tuple(32, 16, &aom_highbd_sad32x16x4d_sse2, 8),
  make_tuple(16, 32, &aom_highbd_sad16x32x4d_sse2, 8),
  make_tuple(16, 16, &aom_highbd_sad16x16x4d_sse2, 8),
  make_tuple(16, 8, &aom_highbd_sad16x8x4d_sse2, 8),
  make_tuple(8, 16, &aom_highbd_sad8x16x4d_sse2, 8),
  make_tuple(8, 8, &aom_highbd_sad8x8x4d_sse2, 8),
  make_tuple(8, 4, &aom_highbd_sad8x4x4d_sse2, 8),
  make_tuple(4, 8, &aom_highbd_sad4x8x4d_sse2, 8),
  make_tuple(4, 4, &aom_highbd_sad4x4x4d_sse2, 8),
  make_tuple(64, 64, &aom_highbd_sad64x64x4d_sse2, 10),
  make_tuple(64, 32, &aom_highbd_sad64x32x4d_sse2, 10),
  make_tuple(32, 64, &aom_highbd_sad32x64x4d_sse2, 10),
  make_tuple(32, 32, &aom_highbd_sad32x32x4d_sse2, 10),
  make_tuple(32, 16, &aom_highbd_sad32x16x4d_sse2, 10),
  make_tuple(16, 32, &aom_highbd_sad16x32x4d_sse2, 10),
  make_tuple(16, 16, &aom_highbd_sad16x16x4d_sse2, 10),
  make_tuple(16, 8, &aom_highbd_sad16x8x4d_sse2, 10),
  make_tuple(8, 16, &aom_highbd_sad8x16x4d_sse2, 10),
  make_tuple(8, 8, &aom_highbd_sad8x8x4d_sse2, 10),
  make_tuple(8, 4, &aom_highbd_sad8x4x4d_sse2, 10),
  make_tuple(4, 8, &aom_highbd_sad4x8x4d_sse2, 10),
  make_tuple(4, 4, &aom_highbd_sad4x4x4d_sse2, 10),
  make_tuple(64, 64, &aom_highbd_sad64x64x4d_sse2, 12),
  make_tuple(64, 32, &aom_highbd_sad64x32x4d_sse2, 12),
  make_tuple(32, 64, &aom_highbd_sad32x64x4d_sse2, 12),
  make_tuple(32, 32, &aom_highbd_sad32x32x4d_sse2, 12),
  make_tuple(32, 16, &aom_highbd_sad32x16x4d_sse2, 12),
  make_tuple(16, 32, &aom_highbd_sad16x32x4d_sse2, 12),
  make_tuple(16, 16, &aom_highbd_sad16x16x4d_sse2, 12),
  make_tuple(16, 8, &aom_highbd_sad16x8x4d_sse2, 12),
  make_tuple(8, 16, &aom_highbd_sad8x16x4d_sse2, 12),
  make_tuple(8, 8, &aom_highbd_sad8x8x4d_sse2, 12),
  make_tuple(8, 4, &aom_highbd_sad8x4x4d_sse2, 12),
  make_tuple(4, 8, &aom_highbd_sad4x8x4d_sse2, 12),
  make_tuple(4, 4, &aom_highbd_sad4x4x4d_sse2, 12),
#endif
#if !CONFIG_REALTIME_ONLY
  make_tuple(64, 16, &aom_sad64x16x4d_sse2, -1),
  make_tuple(16, 64, &aom_sad16x64x4d_sse2, -1),
#if CONFIG_AV1_HIGHBITDEPTH
  make_tuple(64, 16, &aom_highbd_sad64x16x4d_sse2, 8),
  make_tuple(16, 64, &aom_highbd_sad16x64x4d_sse2, 8),
  make_tuple(64, 16, &aom_highbd_sad64x16x4d_sse2, 10),
  make_tuple(16, 64, &aom_highbd_sad16x64x4d_sse2, 10),
  make_tuple(64, 16, &aom_highbd_sad64x16x4d_sse2, 12),
  make_tuple(16, 64, &aom_highbd_sad16x64x4d_sse2, 12),
#endif
  make_tuple(32, 8, &aom_sad32x8x4d_sse2, -1),
  make_tuple(8, 32, &aom_sad8x32x4d_sse2, -1),
#if CONFIG_AV1_HIGHBITDEPTH
  make_tuple(32, 8, &aom_highbd_sad32x8x4d_sse2, 8),
  make_tuple(8, 32, &aom_highbd_sad8x32x4d_sse2, 8),
  make_tuple(32, 8, &aom_highbd_sad32x8x4d_sse2, 10),
  make_tuple(8, 32, &aom_highbd_sad8x32x4d_sse2, 10),
  make_tuple(32, 8, &aom_highbd_sad32x8x4d_sse2, 12),
  make_tuple(8, 32, &aom_highbd_sad8x32x4d_sse2, 12),
#endif
  make_tuple(16, 4, &aom_sad16x4x4d_sse2, -1),
  make_tuple(4, 16, &aom_sad4x16x4d_sse2, -1),
#if CONFIG_AV1_HIGHBITDEPTH
  make_tuple(16, 4, &aom_highbd_sad16x4x4d_sse2, 8),
  make_tuple(4, 16, &aom_highbd_sad4x16x4d_sse2, 8),
  make_tuple(16, 4, &aom_highbd_sad16x4x4d_sse2, 10),
  make_tuple(4, 16, &aom_highbd_sad4x16x4d_sse2, 10),
  make_tuple(16, 4, &aom_highbd_sad16x4x4d_sse2, 12),
  make_tuple(4, 16, &aom_highbd_sad4x16x4d_sse2, 12),
#endif
#endif
};
INSTANTIATE_TEST_SUITE_P(SSE2, SADx4Test, ::testing::ValuesIn(x4d_sse2_tests));

const SadSkipMxNx4Param skip_x4d_sse2_tests[] = {
  make_tuple(128, 128, &aom_sad_skip_128x128x4d_sse2, -1),
  make_tuple(128, 64, &aom_sad_skip_128x64x4d_sse2, -1),
  make_tuple(64, 128, &aom_sad_skip_64x128x4d_sse2, -1),
  make_tuple(64, 64, &aom_sad_skip_64x64x4d_sse2, -1),
  make_tuple(64, 32, &aom_sad_skip_64x32x4d_sse2, -1),
  make_tuple(32, 64, &aom_sad_skip_32x64x4d_sse2, -1),
  make_tuple(32, 32, &aom_sad_skip_32x32x4d_sse2, -1),
  make_tuple(32, 16, &aom_sad_skip_32x16x4d_sse2, -1),
  make_tuple(16, 32, &aom_sad_skip_16x32x4d_sse2, -1),
  make_tuple(16, 16, &aom_sad_skip_16x16x4d_sse2, -1),
  make_tuple(16, 8, &aom_sad_skip_16x8x4d_sse2, -1),
  make_tuple(8, 16, &aom_sad_skip_8x16x4d_sse2, -1),
  make_tuple(8, 8, &aom_sad_skip_8x8x4d_sse2, -1),
  make_tuple(4, 8, &aom_sad_skip_4x8x4d_sse2, -1),
#if !CONFIG_REALTIME_ONLY
  make_tuple(64, 16, &aom_sad_skip_64x16x4d_sse2, -1),
  make_tuple(16, 64, &aom_sad_skip_16x64x4d_sse2, -1),
  make_tuple(32, 8, &aom_sad_skip_32x8x4d_sse2, -1),
  make_tuple(8, 32, &aom_sad_skip_8x32x4d_sse2, -1),
  make_tuple(4, 16, &aom_sad_skip_4x16x4d_sse2, -1),
#endif
#if CONFIG_AV1_HIGHBITDEPTH
  make_tuple(64, 64, &aom_highbd_sad_skip_64x64x4d_sse2, 8),
  make_tuple(64, 32, &aom_highbd_sad_skip_64x32x4d_sse2, 8),
  make_tuple(32, 64, &aom_highbd_sad_skip_32x64x4d_sse2, 8),
  make_tuple(32, 32, &aom_highbd_sad_skip_32x32x4d_sse2, 8),
  make_tuple(32, 16, &aom_highbd_sad_skip_32x16x4d_sse2, 8),
  make_tuple(16, 32, &aom_highbd_sad_skip_16x32x4d_sse2, 8),
  make_tuple(16, 16, &aom_highbd_sad_skip_16x16x4d_sse2, 8),
  make_tuple(16, 8, &aom_highbd_sad_skip_16x8x4d_sse2, 8),
  make_tuple(8, 16, &aom_highbd_sad_skip_8x16x4d_sse2, 8),
  make_tuple(8, 8, &aom_highbd_sad_skip_8x8x4d_sse2, 8),
  make_tuple(4, 8, &aom_highbd_sad_skip_4x8x4d_sse2, 8),
#if !CONFIG_REALTIME_ONLY
  make_tuple(64, 16, &aom_highbd_sad_skip_64x16x4d_sse2, 8),
  make_tuple(16, 64, &aom_highbd_sad_skip_16x64x4d_sse2, 8),
  make_tuple(32, 8, &aom_highbd_sad_skip_32x8x4d_sse2, 8),
  make_tuple(8, 32, &aom_highbd_sad_skip_8x32x4d_sse2, 8),
  make_tuple(4, 16, &aom_highbd_sad_skip_4x16x4d_sse2, 8),
#endif
  make_tuple(64, 64, &aom_highbd_sad_skip_64x64x4d_sse2, 10),
  make_tuple(64, 32, &aom_highbd_sad_skip_64x32x4d_sse2, 10),
  make_tuple(32, 64, &aom_highbd_sad_skip_32x64x4d_sse2, 10),
  make_tuple(32, 32, &aom_highbd_sad_skip_32x32x4d_sse2, 10),
  make_tuple(32, 16, &aom_highbd_sad_skip_32x16x4d_sse2, 10),
  make_tuple(16, 32, &aom_highbd_sad_skip_16x32x4d_sse2, 10),
  make_tuple(16, 16, &aom_highbd_sad_skip_16x16x4d_sse2, 10),
  make_tuple(16, 8, &aom_highbd_sad_skip_16x8x4d_sse2, 10),
  make_tuple(8, 16, &aom_highbd_sad_skip_8x16x4d_sse2, 10),
  make_tuple(8, 8, &aom_highbd_sad_skip_8x8x4d_sse2, 10),
  make_tuple(4, 8, &aom_highbd_sad_skip_4x8x4d_sse2, 10),
#if !CONFIG_REALTIME_ONLY
  make_tuple(64, 16, &aom_highbd_sad_skip_64x16x4d_sse2, 10),
  make_tuple(16, 64, &aom_highbd_sad_skip_16x64x4d_sse2, 10),
  make_tuple(32, 8, &aom_highbd_sad_skip_32x8x4d_sse2, 10),
  make_tuple(8, 32, &aom_highbd_sad_skip_8x32x4d_sse2, 10),
  make_tuple(4, 16, &aom_highbd_sad_skip_4x16x4d_sse2, 10),
#endif
  make_tuple(64, 64, &aom_highbd_sad_skip_64x64x4d_sse2, 12),
  make_tuple(64, 32, &aom_highbd_sad_skip_64x32x4d_sse2, 12),
  make_tuple(32, 64, &aom_highbd_sad_skip_32x64x4d_sse2, 12),
  make_tuple(32, 32, &aom_highbd_sad_skip_32x32x4d_sse2, 12),
  make_tuple(32, 16, &aom_highbd_sad_skip_32x16x4d_sse2, 12),
  make_tuple(16, 32, &aom_highbd_sad_skip_16x32x4d_sse2, 12),
  make_tuple(16, 16, &aom_highbd_sad_skip_16x16x4d_sse2, 12),
  make_tuple(16, 8, &aom_highbd_sad_skip_16x8x4d_sse2, 12),
  make_tuple(8, 16, &aom_highbd_sad_skip_8x16x4d_sse2, 12),
  make_tuple(8, 8, &aom_highbd_sad_skip_8x8x4d_sse2, 12),
  make_tuple(4, 8, &aom_highbd_sad_skip_4x8x4d_sse2, 12),
#if !CONFIG_REALTIME_ONLY
  make_tuple(64, 16, &aom_highbd_sad_skip_64x16x4d_sse2, 12),
  make_tuple(16, 64, &aom_highbd_sad_skip_16x64x4d_sse2, 12),
  make_tuple(32, 8, &aom_highbd_sad_skip_32x8x4d_sse2, 12),
  make_tuple(8, 32, &aom_highbd_sad_skip_8x32x4d_sse2, 12),
  make_tuple(4, 16, &aom_highbd_sad_skip_4x16x4d_sse2, 12),
#endif
#endif  // CONFIG_AV1_HIGHBITDEPTH
};
INSTANTIATE_TEST_SUITE_P(SSE2, SADSkipx4Test,
                         ::testing::ValuesIn(skip_x4d_sse2_tests));

const DistWtdSadMxNAvgParam dist_wtd_avg_sse2_tests[] = {
  make_tuple(128, 128, &aom_dist_wtd_sad128x128_avg_sse2, -1),
  make_tuple(128, 64, &aom_dist_wtd_sad128x64_avg_sse2, -1),
  make_tuple(64, 128, &aom_dist_wtd_sad64x128_avg_sse2, -1),
  make_tuple(64, 64, &aom_dist_wtd_sad64x64_avg_sse2, -1),
  make_tuple(64, 32, &aom_dist_wtd_sad64x32_avg_sse2, -1),
  make_tuple(32, 64, &aom_dist_wtd_sad32x64_avg_sse2, -1),
  make_tuple(32, 32, &aom_dist_wtd_sad32x32_avg_sse2, -1),
  make_tuple(32, 16, &aom_dist_wtd_sad32x16_avg_sse2, -1),
  make_tuple(16, 32, &aom_dist_wtd_sad16x32_avg_sse2, -1),
  make_tuple(16, 16, &aom_dist_wtd_sad16x16_avg_sse2, -1),
  make_tuple(16, 8, &aom_dist_wtd_sad16x8_avg_sse2, -1),
  make_tuple(8, 16, &aom_dist_wtd_sad8x16_avg_sse2, -1),
  make_tuple(8, 8, &aom_dist_wtd_sad8x8_avg_sse2, -1),
  make_tuple(8, 4, &aom_dist_wtd_sad8x4_avg_sse2, -1),
  make_tuple(4, 8, &aom_dist_wtd_sad4x8_avg_sse2, -1),
  make_tuple(4, 4, &aom_dist_wtd_sad4x4_avg_sse2, -1),
#if !CONFIG_REALTIME_ONLY
  make_tuple(64, 16, &aom_dist_wtd_sad64x16_avg_sse2, -1),
  make_tuple(16, 64, &aom_dist_wtd_sad16x64_avg_sse2, -1),
  make_tuple(32, 8, &aom_dist_wtd_sad32x8_avg_sse2, -1),
  make_tuple(8, 32, &aom_dist_wtd_sad8x32_avg_sse2, -1),
  make_tuple(16, 4, &aom_dist_wtd_sad16x4_avg_sse2, -1),
  make_tuple(4, 16, &aom_dist_wtd_sad4x16_avg_sse2, -1),
#endif
};
INSTANTIATE_TEST_SUITE_P(sse2, DistWtdSADavgTest,
                         ::testing::ValuesIn(dist_wtd_avg_sse2_tests));
#endif  // HAVE_SSE2

#if HAVE_SSE3
// Only functions are x3, which do not have tests.
#endif  // HAVE_SSE3

#if HAVE_SSSE3
const DistWtdCompAvgParam dist_wtd_comp_avg_ssse3_tests[] = {
  make_tuple(128, 128, &aom_dist_wtd_comp_avg_pred_ssse3, -1),
  make_tuple(128, 64, &aom_dist_wtd_comp_avg_pred_ssse3, -1),
  make_tuple(64, 128, &aom_dist_wtd_comp_avg_pred_ssse3, -1),
  make_tuple(64, 64, &aom_dist_wtd_comp_avg_pred_ssse3, -1),
  make_tuple(64, 32, &aom_dist_wtd_comp_avg_pred_ssse3, -1),
  make_tuple(32, 64, &aom_dist_wtd_comp_avg_pred_ssse3, -1),
  make_tuple(32, 32, &aom_dist_wtd_comp_avg_pred_ssse3, -1),
  make_tuple(32, 16, &aom_dist_wtd_comp_avg_pred_ssse3, -1),
  make_tuple(16, 32, &aom_dist_wtd_comp_avg_pred_ssse3, -1),
  make_tuple(16, 16, &aom_dist_wtd_comp_avg_pred_ssse3, -1),
  make_tuple(16, 8, &aom_dist_wtd_comp_avg_pred_ssse3, -1),
  make_tuple(8, 16, &aom_dist_wtd_comp_avg_pred_ssse3, -1),
  make_tuple(8, 8, &aom_dist_wtd_comp_avg_pred_ssse3, -1),
  make_tuple(8, 4, &aom_dist_wtd_comp_avg_pred_ssse3, -1),
  make_tuple(4, 8, &aom_dist_wtd_comp_avg_pred_ssse3, -1),
  make_tuple(4, 4, &aom_dist_wtd_comp_avg_pred_ssse3, -1),
  make_tuple(16, 16, &aom_dist_wtd_comp_avg_pred_ssse3, -1),
#if !CONFIG_REALTIME_ONLY
  make_tuple(64, 16, &aom_dist_wtd_comp_avg_pred_ssse3, -1),
  make_tuple(16, 64, &aom_dist_wtd_comp_avg_pred_ssse3, -1),
  make_tuple(32, 8, &aom_dist_wtd_comp_avg_pred_ssse3, -1),
  make_tuple(8, 32, &aom_dist_wtd_comp_avg_pred_ssse3, -1),
  make_tuple(16, 4, &aom_dist_wtd_comp_avg_pred_ssse3, -1),
  make_tuple(4, 16, &aom_dist_wtd_comp_avg_pred_ssse3, -1),
#endif
};

INSTANTIATE_TEST_SUITE_P(SSSE3, DistWtdCompAvgTest,
                         ::testing::ValuesIn(dist_wtd_comp_avg_ssse3_tests));
#endif  // HAVE_SSSE3

#if HAVE_SSE4_1
// Only functions are x8, which do not have tests.
#endif  // HAVE_SSE4_1

#if HAVE_AVX2
const SadMxNParam avx2_tests[] = {
  make_tuple(64, 128, &aom_sad64x128_avx2, -1),
  make_tuple(128, 64, &aom_sad128x64_avx2, -1),
  make_tuple(128, 128, &aom_sad128x128_avx2, -1),
  make_tuple(64, 64, &aom_sad64x64_avx2, -1),
  make_tuple(64, 32, &aom_sad64x32_avx2, -1),
  make_tuple(32, 64, &aom_sad32x64_avx2, -1),
  make_tuple(32, 32, &aom_sad32x32_avx2, -1),
  make_tuple(32, 16, &aom_sad32x16_avx2, -1),
#if CONFIG_AV1_HIGHBITDEPTH
  make_tuple(128, 128, &aom_highbd_sad128x128_avx2, 8),
  make_tuple(128, 128, &aom_highbd_sad128x128_avx2, 10),
  make_tuple(128, 128, &aom_highbd_sad128x128_avx2, 12),
  make_tuple(128, 64, &aom_highbd_sad128x64_avx2, 8),
  make_tuple(128, 64, &aom_highbd_sad128x64_avx2, 10),
  make_tuple(128, 64, &aom_highbd_sad128x64_avx2, 12),
  make_tuple(64, 128, &aom_highbd_sad64x128_avx2, 8),
  make_tuple(64, 128, &aom_highbd_sad64x128_avx2, 10),
  make_tuple(64, 128, &aom_highbd_sad64x128_avx2, 12),
  make_tuple(64, 64, &aom_highbd_sad64x64_avx2, 8),
  make_tuple(64, 64, &aom_highbd_sad64x64_avx2, 10),
  make_tuple(64, 64, &aom_highbd_sad64x64_avx2, 12),
  make_tuple(64, 32, &aom_highbd_sad64x32_avx2, 8),
  make_tuple(64, 32, &aom_highbd_sad64x32_avx2, 10),
  make_tuple(64, 32, &aom_highbd_sad64x32_avx2, 12),
  make_tuple(32, 64, &aom_highbd_sad32x64_avx2, 8),
  make_tuple(32, 64, &aom_highbd_sad32x64_avx2, 10),
  make_tuple(32, 64, &aom_highbd_sad32x64_avx2, 12),
  make_tuple(32, 32, &aom_highbd_sad32x32_avx2, 8),
  make_tuple(32, 32, &aom_highbd_sad32x32_avx2, 10),
  make_tuple(32, 32, &aom_highbd_sad32x32_avx2, 12),
  make_tuple(32, 16, &aom_highbd_sad32x16_avx2, 8),
  make_tuple(32, 16, &aom_highbd_sad32x16_avx2, 10),
  make_tuple(32, 16, &aom_highbd_sad32x16_avx2, 12),
  make_tuple(16, 32, &aom_highbd_sad16x32_avx2, 8),
  make_tuple(16, 32, &aom_highbd_sad16x32_avx2, 10),
  make_tuple(16, 32, &aom_highbd_sad16x32_avx2, 12),
  make_tuple(16, 16, &aom_highbd_sad16x16_avx2, 8),
  make_tuple(16, 16, &aom_highbd_sad16x16_avx2, 10),
  make_tuple(16, 16, &aom_highbd_sad16x16_avx2, 12),
  make_tuple(16, 8, &aom_highbd_sad16x8_avx2, 8),
  make_tuple(16, 8, &aom_highbd_sad16x8_avx2, 10),
  make_tuple(16, 8, &aom_highbd_sad16x8_avx2, 12),

#if !CONFIG_REALTIME_ONLY
  make_tuple(64, 16, &aom_highbd_sad64x16_avx2, 8),
  make_tuple(64, 16, &aom_highbd_sad64x16_avx2, 10),
  make_tuple(64, 16, &aom_highbd_sad64x16_avx2, 12),
  make_tuple(16, 64, &aom_highbd_sad16x64_avx2, 8),
  make_tuple(16, 64, &aom_highbd_sad16x64_avx2, 10),
  make_tuple(16, 64, &aom_highbd_sad16x64_avx2, 12),
  make_tuple(32, 8, &aom_highbd_sad32x8_avx2, 8),
  make_tuple(32, 8, &aom_highbd_sad32x8_avx2, 10),
  make_tuple(32, 8, &aom_highbd_sad32x8_avx2, 12),
  make_tuple(16, 4, &aom_highbd_sad16x4_avx2, 8),
  make_tuple(16, 4, &aom_highbd_sad16x4_avx2, 10),
  make_tuple(16, 4, &aom_highbd_sad16x4_avx2, 12),
#endif
#endif
};
INSTANTIATE_TEST_SUITE_P(AVX2, SADTest, ::testing::ValuesIn(avx2_tests));

const SadSkipMxNParam skip_avx2_tests[] = {
  make_tuple(128, 128, &aom_sad_skip_128x128_avx2, -1),
  make_tuple(128, 64, &aom_sad_skip_128x64_avx2, -1),
  make_tuple(64, 128, &aom_sad_skip_64x128_avx2, -1),
  make_tuple(64, 64, &aom_sad_skip_64x64_avx2, -1),
  make_tuple(64, 32, &aom_sad_skip_64x32_avx2, -1),
  make_tuple(32, 64, &aom_sad_skip_32x64_avx2, -1),
  make_tuple(32, 32, &aom_sad_skip_32x32_avx2, -1),
  make_tuple(32, 16, &aom_sad_skip_32x16_avx2, -1),
#if CONFIG_AV1_HIGHBITDEPTH
  make_tuple(128, 128, &aom_highbd_sad_skip_128x128_avx2, 8),
  make_tuple(128, 64, &aom_highbd_sad_skip_128x64_avx2, 8),
  make_tuple(64, 128, &aom_highbd_sad_skip_64x128_avx2, 8),
  make_tuple(64, 64, &aom_highbd_sad_skip_64x64_avx2, 8),
  make_tuple(64, 32, &aom_highbd_sad_skip_64x32_avx2, 8),
  make_tuple(32, 64, &aom_highbd_sad_skip_32x64_avx2, 8),
  make_tuple(32, 32, &aom_highbd_sad_skip_32x32_avx2, 8),
  make_tuple(32, 16, &aom_highbd_sad_skip_32x16_avx2, 8),
  make_tuple(16, 32, &aom_highbd_sad_skip_16x32_avx2, 8),
  make_tuple(16, 16, &aom_highbd_sad_skip_16x16_avx2, 8),
  make_tuple(16, 8, &aom_highbd_sad_skip_16x8_avx2, 8),

  make_tuple(128, 128, &aom_highbd_sad_skip_128x128_avx2, 10),
  make_tuple(128, 64, &aom_highbd_sad_skip_128x64_avx2, 10),
  make_tuple(64, 128, &aom_highbd_sad_skip_64x128_avx2, 10),
  make_tuple(64, 64, &aom_highbd_sad_skip_64x64_avx2, 10),
  make_tuple(64, 32, &aom_highbd_sad_skip_64x32_avx2, 10),
  make_tuple(32, 64, &aom_highbd_sad_skip_32x64_avx2, 10),
  make_tuple(32, 32, &aom_highbd_sad_skip_32x32_avx2, 10),
  make_tuple(32, 16, &aom_highbd_sad_skip_32x16_avx2, 10),
  make_tuple(16, 32, &aom_highbd_sad_skip_16x32_avx2, 10),
  make_tuple(16, 16, &aom_highbd_sad_skip_16x16_avx2, 10),
  make_tuple(16, 8, &aom_highbd_sad_skip_16x8_avx2, 10),

  make_tuple(128, 128, &aom_highbd_sad_skip_128x128_avx2, 12),
  make_tuple(128, 64, &aom_highbd_sad_skip_128x64_avx2, 12),
  make_tuple(64, 128, &aom_highbd_sad_skip_64x128_avx2, 12),
  make_tuple(64, 64, &aom_highbd_sad_skip_64x64_avx2, 12),
  make_tuple(64, 32, &aom_highbd_sad_skip_64x32_avx2, 12),
  make_tuple(32, 64, &aom_highbd_sad_skip_32x64_avx2, 12),
  make_tuple(32, 32, &aom_highbd_sad_skip_32x32_avx2, 12),
  make_tuple(32, 16, &aom_highbd_sad_skip_32x16_avx2, 12),
  make_tuple(16, 32, &aom_highbd_sad_skip_16x32_avx2, 12),
  make_tuple(16, 16, &aom_highbd_sad_skip_16x16_avx2, 12),
  make_tuple(16, 8, &aom_highbd_sad_skip_16x8_avx2, 12),

#if !CONFIG_REALTIME_ONLY
  make_tuple(16, 64, &aom_highbd_sad_skip_16x64_avx2, 8),
  make_tuple(16, 64, &aom_highbd_sad_skip_16x64_avx2, 10),
  make_tuple(16, 64, &aom_highbd_sad_skip_16x64_avx2, 12),
#endif
#endif
};
INSTANTIATE_TEST_SUITE_P(AVX2, SADSkipTest,
                         ::testing::ValuesIn(skip_avx2_tests));

const SadMxNAvgParam avg_avx2_tests[] = {
  make_tuple(64, 128, &aom_sad64x128_avg_avx2, -1),
  make_tuple(128, 64, &aom_sad128x64_avg_avx2, -1),
  make_tuple(128, 128, &aom_sad128x128_avg_avx2, -1),
  make_tuple(64, 64, &aom_sad64x64_avg_avx2, -1),
  make_tuple(64, 32, &aom_sad64x32_avg_avx2, -1),
  make_tuple(32, 64, &aom_sad32x64_avg_avx2, -1),
  make_tuple(32, 32, &aom_sad32x32_avg_avx2, -1),
  make_tuple(32, 16, &aom_sad32x16_avg_avx2, -1),
#if CONFIG_AV1_HIGHBITDEPTH
  make_tuple(128, 128, &aom_highbd_sad128x128_avg_avx2, 8),
  make_tuple(128, 128, &aom_highbd_sad128x128_avg_avx2, 10),
  make_tuple(128, 128, &aom_highbd_sad128x128_avg_avx2, 12),
  make_tuple(128, 64, &aom_highbd_sad128x64_avg_avx2, 8),
  make_tuple(128, 64, &aom_highbd_sad128x64_avg_avx2, 10),
  make_tuple(128, 64, &aom_highbd_sad128x64_avg_avx2, 12),
  make_tuple(64, 128, &aom_highbd_sad64x128_avg_avx2, 8),
  make_tuple(64, 128, &aom_highbd_sad64x128_avg_avx2, 10),
  make_tuple(64, 128, &aom_highbd_sad64x128_avg_avx2, 12),
  make_tuple(64, 64, &aom_highbd_sad64x64_avg_avx2, 8),
  make_tuple(64, 64, &aom_highbd_sad64x64_avg_avx2, 10),
  make_tuple(64, 64, &aom_highbd_sad64x64_avg_avx2, 12),
  make_tuple(64, 32, &aom_highbd_sad64x32_avg_avx2, 8),
  make_tuple(64, 32, &aom_highbd_sad64x32_avg_avx2, 10),
  make_tuple(64, 32, &aom_highbd_sad64x32_avg_avx2, 12),
  make_tuple(32, 64, &aom_highbd_sad32x64_avg_avx2, 8),
  make_tuple(32, 64, &aom_highbd_sad32x64_avg_avx2, 10),
  make_tuple(32, 64, &aom_highbd_sad32x64_avg_avx2, 12),
  make_tuple(32, 32, &aom_highbd_sad32x32_avg_avx2, 8),
  make_tuple(32, 32, &aom_highbd_sad32x32_avg_avx2, 10),
  make_tuple(32, 32, &aom_highbd_sad32x32_avg_avx2, 12),
  make_tuple(32, 16, &aom_highbd_sad32x16_avg_avx2, 8),
  make_tuple(32, 16, &aom_highbd_sad32x16_avg_avx2, 10),
  make_tuple(32, 16, &aom_highbd_sad32x16_avg_avx2, 12),
  make_tuple(16, 32, &aom_highbd_sad16x32_avg_avx2, 8),
  make_tuple(16, 32, &aom_highbd_sad16x32_avg_avx2, 10),
  make_tuple(16, 32, &aom_highbd_sad16x32_avg_avx2, 12),
  make_tuple(16, 16, &aom_highbd_sad16x16_avg_avx2, 8),
  make_tuple(16, 16, &aom_highbd_sad16x16_avg_avx2, 10),
  make_tuple(16, 16, &aom_highbd_sad16x16_avg_avx2, 12),
  make_tuple(16, 8, &aom_highbd_sad16x8_avg_avx2, 8),
  make_tuple(16, 8, &aom_highbd_sad16x8_avg_avx2, 10),
  make_tuple(16, 8, &aom_highbd_sad16x8_avg_avx2, 12),

#if !CONFIG_REALTIME_ONLY
  make_tuple(64, 16, &aom_highbd_sad64x16_avg_avx2, 8),
  make_tuple(64, 16, &aom_highbd_sad64x16_avg_avx2, 10),
  make_tuple(64, 16, &aom_highbd_sad64x16_avg_avx2, 12),
  make_tuple(16, 64, &aom_highbd_sad16x64_avg_avx2, 8),
  make_tuple(16, 64, &aom_highbd_sad16x64_avg_avx2, 10),
  make_tuple(16, 64, &aom_highbd_sad16x64_avg_avx2, 12),
  make_tuple(32, 8, &aom_highbd_sad32x8_avg_avx2, 8),
  make_tuple(32, 8, &aom_highbd_sad32x8_avg_avx2, 10),
  make_tuple(32, 8, &aom_highbd_sad32x8_avg_avx2, 12),
  make_tuple(16, 4, &aom_highbd_sad16x4_avg_avx2, 8),
  make_tuple(16, 4, &aom_highbd_sad16x4_avg_avx2, 10),
  make_tuple(16, 4, &aom_highbd_sad16x4_avg_avx2, 12),
#endif
#endif
};
INSTANTIATE_TEST_SUITE_P(AVX2, SADavgTest, ::testing::ValuesIn(avg_avx2_tests));

const SadSkipMxNx4Param skip_x4d_avx2_tests[] = {
  make_tuple(128, 128, &aom_sad_skip_128x128x4d_avx2, -1),
  make_tuple(128, 64, &aom_sad_skip_128x64x4d_avx2, -1),
  make_tuple(64, 128, &aom_sad_skip_64x128x4d_avx2, -1),
  make_tuple(64, 64, &aom_sad_skip_64x64x4d_avx2, -1),
  make_tuple(64, 32, &aom_sad_skip_64x32x4d_avx2, -1),
  make_tuple(32, 64, &aom_sad_skip_32x64x4d_avx2, -1),
  make_tuple(32, 32, &aom_sad_skip_32x32x4d_avx2, -1),
  make_tuple(32, 16, &aom_sad_skip_32x16x4d_avx2, -1),
  make_tuple(16, 32, &aom_sad_skip_16x32x4d_avx2, -1),
  make_tuple(16, 16, &aom_sad_skip_16x16x4d_avx2, -1),
  make_tuple(16, 8, &aom_sad_skip_16x8x4d_avx2, -1),

#if CONFIG_AV1_HIGHBITDEPTH
  make_tuple(128, 128, &aom_highbd_sad_skip_128x128x4d_avx2, 8),
  make_tuple(128, 64, &aom_highbd_sad_skip_128x64x4d_avx2, 8),
  make_tuple(64, 128, &aom_highbd_sad_skip_64x128x4d_avx2, 8),
  make_tuple(64, 64, &aom_highbd_sad_skip_64x64x4d_avx2, 8),
  make_tuple(64, 32, &aom_highbd_sad_skip_64x32x4d_avx2, 8),
  make_tuple(32, 64, &aom_highbd_sad_skip_32x64x4d_avx2, 8),
  make_tuple(32, 32, &aom_highbd_sad_skip_32x32x4d_avx2, 8),
  make_tuple(32, 16, &aom_highbd_sad_skip_32x16x4d_avx2, 8),
  make_tuple(16, 32, &aom_highbd_sad_skip_16x32x4d_avx2, 8),
  make_tuple(16, 16, &aom_highbd_sad_skip_16x16x4d_avx2, 8),
  make_tuple(16, 8, &aom_highbd_sad_skip_16x8x4d_avx2, 8),

  make_tuple(128, 128, &aom_highbd_sad_skip_128x128x4d_avx2, 10),
  make_tuple(128, 64, &aom_highbd_sad_skip_128x64x4d_avx2, 10),
  make_tuple(64, 128, &aom_highbd_sad_skip_64x128x4d_avx2, 10),
  make_tuple(64, 64, &aom_highbd_sad_skip_64x64x4d_avx2, 10),
  make_tuple(64, 32, &aom_highbd_sad_skip_64x32x4d_avx2, 10),
  make_tuple(32, 64, &aom_highbd_sad_skip_32x64x4d_avx2, 10),
  make_tuple(32, 32, &aom_highbd_sad_skip_32x32x4d_avx2, 10),
  make_tuple(32, 16, &aom_highbd_sad_skip_32x16x4d_avx2, 10),
  make_tuple(16, 32, &aom_highbd_sad_skip_16x32x4d_avx2, 10),
  make_tuple(16, 16, &aom_highbd_sad_skip_16x16x4d_avx2, 10),
  make_tuple(16, 8, &aom_highbd_sad_skip_16x8x4d_avx2, 10),

  make_tuple(128, 128, &aom_highbd_sad_skip_128x128x4d_avx2, 12),
  make_tuple(128, 64, &aom_highbd_sad_skip_128x64x4d_avx2, 12),
  make_tuple(64, 128, &aom_highbd_sad_skip_64x128x4d_avx2, 12),
  make_tuple(64, 64, &aom_highbd_sad_skip_64x64x4d_avx2, 12),
  make_tuple(64, 32, &aom_highbd_sad_skip_64x32x4d_avx2, 12),
  make_tuple(32, 64, &aom_highbd_sad_skip_32x64x4d_avx2, 12),
  make_tuple(32, 32, &aom_highbd_sad_skip_32x32x4d_avx2, 12),
  make_tuple(32, 16, &aom_highbd_sad_skip_32x16x4d_avx2, 12),
  make_tuple(16, 32, &aom_highbd_sad_skip_16x32x4d_avx2, 12),
  make_tuple(16, 16, &aom_highbd_sad_skip_16x16x4d_avx2, 12),
  make_tuple(16, 8, &aom_highbd_sad_skip_16x8x4d_avx2, 12),

#if !CONFIG_REALTIME_ONLY
  make_tuple(64, 16, &aom_highbd_sad_skip_64x16x4d_avx2, 8),
  make_tuple(32, 8, &aom_highbd_sad_skip_32x8x4d_avx2, 8),
  make_tuple(16, 64, &aom_highbd_sad_skip_16x64x4d_avx2, 8),

  make_tuple(64, 16, &aom_highbd_sad_skip_64x16x4d_avx2, 10),
  make_tuple(32, 8, &aom_highbd_sad_skip_32x8x4d_avx2, 10),
  make_tuple(16, 64, &aom_highbd_sad_skip_16x64x4d_avx2, 10),

  make_tuple(64, 16, &aom_highbd_sad_skip_64x16x4d_avx2, 12),
  make_tuple(32, 8, &aom_highbd_sad_skip_32x8x4d_avx2, 12),
  make_tuple(16, 64, &aom_highbd_sad_skip_16x64x4d_avx2, 12),
#endif
#endif

#if !CONFIG_REALTIME_ONLY
  make_tuple(64, 16, &aom_sad_skip_64x16x4d_avx2, -1),
  make_tuple(32, 8, &aom_sad_skip_32x8x4d_avx2, -1),

  make_tuple(16, 64, &aom_sad_skip_16x64x4d_avx2, -1),
#endif
};

INSTANTIATE_TEST_SUITE_P(AVX2, SADSkipx4Test,
                         ::testing::ValuesIn(skip_x4d_avx2_tests));

const SadMxNx4Param x4d_avx2_tests[] = {
  make_tuple(16, 32, &aom_sad16x32x4d_avx2, -1),
  make_tuple(16, 16, &aom_sad16x16x4d_avx2, -1),
  make_tuple(16, 8, &aom_sad16x8x4d_avx2, -1),
  make_tuple(32, 64, &aom_sad32x64x4d_avx2, -1),
  make_tuple(32, 32, &aom_sad32x32x4d_avx2, -1),
  make_tuple(32, 16, &aom_sad32x16x4d_avx2, -1),
  make_tuple(64, 128, &aom_sad64x128x4d_avx2, -1),
  make_tuple(64, 64, &aom_sad64x64x4d_avx2, -1),
  make_tuple(64, 32, &aom_sad64x32x4d_avx2, -1),
  make_tuple(128, 128, &aom_sad128x128x4d_avx2, -1),
  make_tuple(128, 64, &aom_sad128x64x4d_avx2, -1),

#if !CONFIG_REALTIME_ONLY
  make_tuple(16, 64, &aom_sad16x64x4d_avx2, -1),
  make_tuple(16, 4, &aom_sad16x4x4d_avx2, -1),
  make_tuple(32, 8, &aom_sad32x8x4d_avx2, -1),
  make_tuple(64, 16, &aom_sad64x16x4d_avx2, -1),
#endif

#if CONFIG_AV1_HIGHBITDEPTH
  make_tuple(128, 128, &aom_highbd_sad128x128x4d_avx2, 8),
  make_tuple(128, 128, &aom_highbd_sad128x128x4d_avx2, 10),
  make_tuple(128, 128, &aom_highbd_sad128x128x4d_avx2, 12),
  make_tuple(128, 64, &aom_highbd_sad128x64x4d_avx2, 8),
  make_tuple(128, 64, &aom_highbd_sad128x64x4d_avx2, 10),
  make_tuple(128, 64, &aom_highbd_sad128x64x4d_avx2, 12),
  make_tuple(64, 128, &aom_highbd_sad64x128x4d_avx2, 8),
  make_tuple(64, 128, &aom_highbd_sad64x128x4d_avx2, 10),
  make_tuple(64, 128, &aom_highbd_sad64x128x4d_avx2, 12),
  make_tuple(64, 64, &aom_highbd_sad64x64x4d_avx2, 8),
  make_tuple(64, 64, &aom_highbd_sad64x64x4d_avx2, 10),
  make_tuple(64, 64, &aom_highbd_sad64x64x4d_avx2, 12),
  make_tuple(64, 32, &aom_highbd_sad64x32x4d_avx2, 8),
  make_tuple(64, 32, &aom_highbd_sad64x32x4d_avx2, 10),
  make_tuple(64, 32, &aom_highbd_sad64x32x4d_avx2, 12),
  make_tuple(32, 64, &aom_highbd_sad32x64x4d_avx2, 8),
  make_tuple(32, 64, &aom_highbd_sad32x64x4d_avx2, 10),
  make_tuple(32, 64, &aom_highbd_sad32x64x4d_avx2, 12),
  make_tuple(32, 32, &aom_highbd_sad32x32x4d_avx2, 8),
  make_tuple(32, 32, &aom_highbd_sad32x32x4d_avx2, 10),
  make_tuple(32, 32, &aom_highbd_sad32x32x4d_avx2, 12),
  make_tuple(32, 16, &aom_highbd_sad32x16x4d_avx2, 8),
  make_tuple(32, 16, &aom_highbd_sad32x16x4d_avx2, 10),
  make_tuple(32, 16, &aom_highbd_sad32x16x4d_avx2, 12),
  make_tuple(16, 32, &aom_highbd_sad16x32x4d_avx2, 8),
  make_tuple(16, 32, &aom_highbd_sad16x32x4d_avx2, 10),
  make_tuple(16, 32, &aom_highbd_sad16x32x4d_avx2, 12),
  make_tuple(16, 16, &aom_highbd_sad16x16x4d_avx2, 8),
  make_tuple(16, 16, &aom_highbd_sad16x16x4d_avx2, 10),
  make_tuple(16, 16, &aom_highbd_sad16x16x4d_avx2, 12),
  make_tuple(16, 8, &aom_highbd_sad16x8x4d_avx2, 8),
  make_tuple(16, 8, &aom_highbd_sad16x8x4d_avx2, 10),
  make_tuple(16, 8, &aom_highbd_sad16x8x4d_avx2, 12),

#if !CONFIG_REALTIME_ONLY
  make_tuple(16, 64, &aom_highbd_sad16x64x4d_avx2, 8),
  make_tuple(16, 64, &aom_highbd_sad16x64x4d_avx2, 10),
  make_tuple(16, 64, &aom_highbd_sad16x64x4d_avx2, 12),
  make_tuple(64, 16, &aom_highbd_sad64x16x4d_avx2, 8),
  make_tuple(64, 16, &aom_highbd_sad64x16x4d_avx2, 10),
  make_tuple(64, 16, &aom_highbd_sad64x16x4d_avx2, 12),
  make_tuple(32, 8, &aom_highbd_sad32x8x4d_avx2, 8),
  make_tuple(32, 8, &aom_highbd_sad32x8x4d_avx2, 10),
  make_tuple(32, 8, &aom_highbd_sad32x8x4d_avx2, 12),
  make_tuple(16, 4, &aom_highbd_sad16x4x4d_avx2, 8),
  make_tuple(16, 4, &aom_highbd_sad16x4x4d_avx2, 10),
  make_tuple(16, 4, &aom_highbd_sad16x4x4d_avx2, 12),
#endif
#endif
};
INSTANTIATE_TEST_SUITE_P(AVX2, SADx4Test, ::testing::ValuesIn(x4d_avx2_tests));

const SadMxNx4Param x3d_avx2_tests[] = {
  make_tuple(32, 64, &aom_sad32x64x3d_avx2, -1),
  make_tuple(32, 32, &aom_sad32x32x3d_avx2, -1),
  make_tuple(32, 16, &aom_sad32x16x3d_avx2, -1),
  make_tuple(64, 128, &aom_sad64x128x3d_avx2, -1),
  make_tuple(64, 64, &aom_sad64x64x3d_avx2, -1),
  make_tuple(64, 32, &aom_sad64x32x3d_avx2, -1),
  make_tuple(128, 128, &aom_sad128x128x3d_avx2, -1),
  make_tuple(128, 64, &aom_sad128x64x3d_avx2, -1),

#if !CONFIG_REALTIME_ONLY
  make_tuple(32, 8, &aom_sad32x8x3d_avx2, -1),
  make_tuple(64, 16, &aom_sad64x16x3d_avx2, -1),
#endif  // !CONFIG_REALTIME_ONLY

#if CONFIG_AV1_HIGHBITDEPTH
  make_tuple(128, 128, &aom_highbd_sad128x128x3d_avx2, 8),
  make_tuple(128, 128, &aom_highbd_sad128x128x3d_avx2, 10),
  make_tuple(128, 128, &aom_highbd_sad128x128x3d_avx2, 12),
  make_tuple(128, 64, &aom_highbd_sad128x64x3d_avx2, 8),
  make_tuple(128, 64, &aom_highbd_sad128x64x3d_avx2, 10),
  make_tuple(128, 64, &aom_highbd_sad128x64x3d_avx2, 12),
  make_tuple(64, 128, &aom_highbd_sad64x128x3d_avx2, 8),
  make_tuple(64, 128, &aom_highbd_sad64x128x3d_avx2, 10),
  make_tuple(64, 128, &aom_highbd_sad64x128x3d_avx2, 12),
  make_tuple(64, 64, &aom_highbd_sad64x64x3d_avx2, 8),
  make_tuple(64, 64, &aom_highbd_sad64x64x3d_avx2, 10),
  make_tuple(64, 64, &aom_highbd_sad64x64x3d_avx2, 12),
  make_tuple(64, 32, &aom_highbd_sad64x32x3d_avx2, 8),
  make_tuple(64, 32, &aom_highbd_sad64x32x3d_avx2, 10),
  make_tuple(64, 32, &aom_highbd_sad64x32x3d_avx2, 12),
  make_tuple(32, 64, &aom_highbd_sad32x64x3d_avx2, 8),
  make_tuple(32, 64, &aom_highbd_sad32x64x3d_avx2, 10),
  make_tuple(32, 64, &aom_highbd_sad32x64x3d_avx2, 12),
  make_tuple(32, 32, &aom_highbd_sad32x32x3d_avx2, 8),
  make_tuple(32, 32, &aom_highbd_sad32x32x3d_avx2, 10),
  make_tuple(32, 32, &aom_highbd_sad32x32x3d_avx2, 12),
  make_tuple(32, 16, &aom_highbd_sad32x16x3d_avx2, 8),
  make_tuple(32, 16, &aom_highbd_sad32x16x3d_avx2, 10),
  make_tuple(32, 16, &aom_highbd_sad32x16x3d_avx2, 12),
  make_tuple(16, 32, &aom_highbd_sad16x32x3d_avx2, 8),
  make_tuple(16, 32, &aom_highbd_sad16x32x3d_avx2, 10),
  make_tuple(16, 32, &aom_highbd_sad16x32x3d_avx2, 12),
  make_tuple(16, 16, &aom_highbd_sad16x16x3d_avx2, 8),
  make_tuple(16, 16, &aom_highbd_sad16x16x3d_avx2, 10),
  make_tuple(16, 16, &aom_highbd_sad16x16x3d_avx2, 12),
  make_tuple(16, 8, &aom_highbd_sad16x8x3d_avx2, 8),
  make_tuple(16, 8, &aom_highbd_sad16x8x3d_avx2, 10),
  make_tuple(16, 8, &aom_highbd_sad16x8x3d_avx2, 12),

#if !CONFIG_REALTIME_ONLY
  make_tuple(16, 64, &aom_highbd_sad16x64x3d_avx2, 8),
  make_tuple(16, 64, &aom_highbd_sad16x64x3d_avx2, 10),
  make_tuple(16, 64, &aom_highbd_sad16x64x3d_avx2, 12),
  make_tuple(64, 16, &aom_highbd_sad64x16x3d_avx2, 8),
  make_tuple(64, 16, &aom_highbd_sad64x16x3d_avx2, 10),
  make_tuple(64, 16, &aom_highbd_sad64x16x3d_avx2, 12),
  make_tuple(32, 8, &aom_highbd_sad32x8x3d_avx2, 8),
  make_tuple(32, 8, &aom_highbd_sad32x8x3d_avx2, 10),
  make_tuple(32, 8, &aom_highbd_sad32x8x3d_avx2, 12),
  make_tuple(16, 4, &aom_highbd_sad16x4x3d_avx2, 8),
  make_tuple(16, 4, &aom_highbd_sad16x4x3d_avx2, 10),
  make_tuple(16, 4, &aom_highbd_sad16x4x3d_avx2, 12),
#endif  // !CONFIG_REALTIME_ONLY
#endif  // CONFIG_AV1_HIGHBITDEPTH
};
INSTANTIATE_TEST_SUITE_P(AVX2, SADx3Test, ::testing::ValuesIn(x3d_avx2_tests));
#endif  // HAVE_AVX2

}  // namespace
