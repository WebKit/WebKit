/*
 * Copyright (c) 2018, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <tuple>

#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"

#include "aom_mem/aom_mem.h"
#include "aom_ports/aom_timer.h"
#include "av1/common/blockd.h"
#include "av1/common/pred_common.h"
#include "av1/common/reconintra.h"
#include "test/acm_random.h"
#include "test/register_state_check.h"
#include "test/util.h"

namespace {

const int kZ1Start = 0;
const int kZ2Start = 90;
const int kZ3Start = 180;

const TX_SIZE kTxSize[] = { TX_4X4,   TX_8X8,   TX_16X16, TX_32X32, TX_64X64,
                            TX_4X8,   TX_8X4,   TX_8X16,  TX_16X8,  TX_16X32,
                            TX_32X16, TX_32X64, TX_64X32, TX_4X16,  TX_16X4,
                            TX_8X32,  TX_32X8,  TX_16X64, TX_64X16 };

const char *const kTxSizeStrings[] = {
  "TX_4X4",   "TX_8X8",   "TX_16X16", "TX_32X32", "TX_64X64",
  "TX_4X8",   "TX_8X4",   "TX_8X16",  "TX_16X8",  "TX_16X32",
  "TX_32X16", "TX_32X64", "TX_64X32", "TX_4X16",  "TX_16X4",
  "TX_8X32",  "TX_32X8",  "TX_16X64", "TX_64X16"
};

using libaom_test::ACMRandom;

typedef void (*DrPred_Hbd)(uint16_t *dst, ptrdiff_t stride, int bw, int bh,
                           const uint16_t *above, const uint16_t *left,
                           int upsample_above, int upsample_left, int dx,
                           int dy, int bd);

typedef void (*DrPred)(uint8_t *dst, ptrdiff_t stride, int bw, int bh,
                       const uint8_t *above, const uint8_t *left,
                       int upsample_above, int upsample_left, int dx, int dy,
                       int bd);

typedef void (*Z1_Lbd)(uint8_t *dst, ptrdiff_t stride, int bw, int bh,
                       const uint8_t *above, const uint8_t *left,
                       int upsample_above, int dx, int dy);
template <Z1_Lbd fn>
void z1_wrapper(uint8_t *dst, ptrdiff_t stride, int bw, int bh,
                const uint8_t *above, const uint8_t *left, int upsample_above,
                int upsample_left, int dx, int dy, int bd) {
  (void)bd;
  (void)upsample_left;
  fn(dst, stride, bw, bh, above, left, upsample_above, dx, dy);
}

typedef void (*Z2_Lbd)(uint8_t *dst, ptrdiff_t stride, int bw, int bh,
                       const uint8_t *above, const uint8_t *left,
                       int upsample_above, int upsample_left, int dx, int dy);
template <Z2_Lbd fn>
void z2_wrapper(uint8_t *dst, ptrdiff_t stride, int bw, int bh,
                const uint8_t *above, const uint8_t *left, int upsample_above,
                int upsample_left, int dx, int dy, int bd) {
  (void)bd;
  (void)upsample_left;
  fn(dst, stride, bw, bh, above, left, upsample_above, upsample_left, dx, dy);
}

typedef void (*Z3_Lbd)(uint8_t *dst, ptrdiff_t stride, int bw, int bh,
                       const uint8_t *above, const uint8_t *left,
                       int upsample_left, int dx, int dy);
template <Z3_Lbd fn>
void z3_wrapper(uint8_t *dst, ptrdiff_t stride, int bw, int bh,
                const uint8_t *above, const uint8_t *left, int upsample_above,
                int upsample_left, int dx, int dy, int bd) {
  (void)bd;
  (void)upsample_above;
  fn(dst, stride, bw, bh, above, left, upsample_left, dx, dy);
}

typedef void (*Z1_Hbd)(uint16_t *dst, ptrdiff_t stride, int bw, int bh,
                       const uint16_t *above, const uint16_t *left,
                       int upsample_above, int dx, int dy, int bd);
template <Z1_Hbd fn>
void z1_wrapper_hbd(uint16_t *dst, ptrdiff_t stride, int bw, int bh,
                    const uint16_t *above, const uint16_t *left,
                    int upsample_above, int upsample_left, int dx, int dy,
                    int bd) {
  (void)bd;
  (void)upsample_left;
  fn(dst, stride, bw, bh, above, left, upsample_above, dx, dy, bd);
}

typedef void (*Z2_Hbd)(uint16_t *dst, ptrdiff_t stride, int bw, int bh,
                       const uint16_t *above, const uint16_t *left,
                       int upsample_above, int upsample_left, int dx, int dy,
                       int bd);
template <Z2_Hbd fn>
void z2_wrapper_hbd(uint16_t *dst, ptrdiff_t stride, int bw, int bh,
                    const uint16_t *above, const uint16_t *left,
                    int upsample_above, int upsample_left, int dx, int dy,
                    int bd) {
  (void)bd;
  fn(dst, stride, bw, bh, above, left, upsample_above, upsample_left, dx, dy,
     bd);
}

typedef void (*Z3_Hbd)(uint16_t *dst, ptrdiff_t stride, int bw, int bh,
                       const uint16_t *above, const uint16_t *left,
                       int upsample_left, int dx, int dy, int bd);
template <Z3_Hbd fn>
void z3_wrapper_hbd(uint16_t *dst, ptrdiff_t stride, int bw, int bh,
                    const uint16_t *above, const uint16_t *left,
                    int upsample_above, int upsample_left, int dx, int dy,
                    int bd) {
  (void)bd;
  (void)upsample_above;
  fn(dst, stride, bw, bh, above, left, upsample_left, dx, dy, bd);
}

template <typename FuncType>
struct DrPredFunc {
  DrPredFunc(FuncType pred = nullptr, FuncType tst = nullptr,
             int bit_depth_value = 0, int start_angle_value = 0)
      : ref_fn(pred), tst_fn(tst), bit_depth(bit_depth_value),
        start_angle(start_angle_value) {}

  FuncType ref_fn;
  FuncType tst_fn;
  int bit_depth;
  int start_angle;
};

template <typename Pixel, typename FuncType>
class DrPredTest : public ::testing::TestWithParam<DrPredFunc<FuncType> > {
 protected:
  static const int kMaxNumTests = 10000;
  static const int kIterations = 10;
  static const int kDstStride = 64;
  static const int kDstSize = kDstStride * kDstStride;
  static const int kOffset = 16;
  static const int kBufSize = ((2 * MAX_TX_SIZE) << 1) + 16;

  DrPredTest()
      : enable_upsample_(0), upsample_above_(0), upsample_left_(0), bw_(0),
        bh_(0), dx_(1), dy_(1), bd_(8), txsize_(TX_4X4) {
    params_ = this->GetParam();
    start_angle_ = params_.start_angle;
    stop_angle_ = start_angle_ + 90;

    dst_ref_ = &dst_ref_data_[0];
    dst_tst_ = &dst_tst_data_[0];
    dst_stride_ = kDstStride;
    above_ = &above_data_[kOffset];
    left_ = &left_data_[kOffset];

    for (int i = 0; i < kBufSize; ++i) {
      above_data_[i] = rng_.Rand8();
      left_data_[i] = rng_.Rand8();
    }

    for (int i = 0; i < kDstSize; ++i) {
      dst_ref_[i] = 0;
      dst_tst_[i] = 0;
    }
  }

  ~DrPredTest() override = default;

  void Predict(bool speedtest, int tx) {
    const int kNumTests = speedtest ? kMaxNumTests : 1;
    aom_usec_timer timer;
    int tst_time = 0;

    bd_ = params_.bit_depth;

    aom_usec_timer_start(&timer);
    for (int k = 0; k < kNumTests; ++k) {
      params_.ref_fn(dst_ref_, dst_stride_, bw_, bh_, above_, left_,
                     upsample_above_, upsample_left_, dx_, dy_, bd_);
    }
    aom_usec_timer_mark(&timer);
    const int ref_time = static_cast<int>(aom_usec_timer_elapsed(&timer));

    if (params_.tst_fn) {
      aom_usec_timer_start(&timer);
      for (int k = 0; k < kNumTests; ++k) {
        API_REGISTER_STATE_CHECK(params_.tst_fn(dst_tst_, dst_stride_, bw_, bh_,
                                                above_, left_, upsample_above_,
                                                upsample_left_, dx_, dy_, bd_));
      }
      aom_usec_timer_mark(&timer);
      tst_time = static_cast<int>(aom_usec_timer_elapsed(&timer));
    } else {
      for (int i = 0; i < kDstSize; ++i) {
        dst_ref_[i] = dst_tst_[i];
      }
    }

    OutputTimes(kNumTests, ref_time, tst_time, tx);
  }

  void RunTest(bool speedtest, bool needsaturation, int p_angle) {
    bd_ = params_.bit_depth;

    if (needsaturation) {
      for (int i = 0; i < kBufSize; ++i) {
        above_data_[i] = left_data_[i] = (1 << bd_) - 1;
      }
    }
    for (int tx = 0; tx < TX_SIZES_ALL; ++tx) {
      if (params_.tst_fn == nullptr) {
        for (int i = 0; i < kDstSize; ++i) {
          dst_tst_[i] = (1 << bd_) - 1;
          dst_ref_[i] = (1 << bd_) - 1;
        }
      } else {
        for (int i = 0; i < kDstSize; ++i) {
          dst_ref_[i] = 0;
          dst_tst_[i] = 0;
        }
      }

      bw_ = tx_size_wide[kTxSize[tx]];
      bh_ = tx_size_high[kTxSize[tx]];

      if (enable_upsample_) {
        upsample_above_ =
            av1_use_intra_edge_upsample(bw_, bh_, p_angle - 90, 0);
        upsample_left_ =
            av1_use_intra_edge_upsample(bw_, bh_, p_angle - 180, 0);
      } else {
        upsample_above_ = upsample_left_ = 0;
      }

      Predict(speedtest, tx);

      for (int r = 0; r < bh_; ++r) {
        for (int c = 0; c < bw_; ++c) {
          ASSERT_EQ(dst_ref_[r * dst_stride_ + c],
                    dst_tst_[r * dst_stride_ + c])
              << bw_ << "x" << bh_ << " r: " << r << " c: " << c
              << " dx: " << dx_ << " dy: " << dy_
              << " upsample_above: " << upsample_above_
              << " upsample_left: " << upsample_left_;
        }
      }
    }
  }

  void OutputTimes(int num_tests, int ref_time, int tst_time, int tx) {
    if (num_tests > 1) {
      if (params_.tst_fn) {
        const float x = static_cast<float>(ref_time) / tst_time;
        printf("\t[%8s] :: ref time %6d, tst time %6d     %3.2f\n",
               kTxSizeStrings[tx], ref_time, tst_time, x);
      } else {
        printf("\t[%8s] :: ref time %6d\n", kTxSizeStrings[tx], ref_time);
      }
    }
  }

  void RundrPredTest(const int speed) {
    if (params_.tst_fn == nullptr) return;
    const int angles[] = { 3, 45, 87 };
    const int start_angle = speed ? 0 : start_angle_;
    const int stop_angle = speed ? 3 : stop_angle_;
    for (enable_upsample_ = 0; enable_upsample_ < 2; ++enable_upsample_) {
      for (int i = start_angle; i < stop_angle; ++i) {
        const int angle = speed ? angles[i] + start_angle_ : i;
        dx_ = av1_get_dx(angle);
        dy_ = av1_get_dy(angle);
        if (speed) {
          printf("enable_upsample: %d angle: %d ~~~~~~~~~~~~~~~\n",
                 enable_upsample_, angle);
        }
        if (dx_ && dy_) RunTest(speed, false, angle);
      }
    }
  }

  Pixel dst_ref_data_[kDstSize];
  Pixel dst_tst_data_[kDstSize];

  Pixel left_data_[kBufSize];
  Pixel dummy_data_[kBufSize];
  Pixel above_data_[kBufSize];

  Pixel *dst_ref_;
  Pixel *dst_tst_;
  Pixel *above_;
  Pixel *left_;
  int dst_stride_;

  int enable_upsample_;
  int upsample_above_;
  int upsample_left_;
  int bw_;
  int bh_;
  int dx_;
  int dy_;
  int bd_;
  TX_SIZE txsize_;

  int start_angle_;
  int stop_angle_;

  ACMRandom rng_;

  DrPredFunc<FuncType> params_;
};

class LowbdDrPredTest : public DrPredTest<uint8_t, DrPred> {};

TEST_P(LowbdDrPredTest, SaturatedValues) {
  for (enable_upsample_ = 0; enable_upsample_ < 2; ++enable_upsample_) {
    for (int angle = start_angle_; angle < stop_angle_; ++angle) {
      dx_ = av1_get_dx(angle);
      dy_ = av1_get_dy(angle);
      if (dx_ && dy_) RunTest(false, true, angle);
    }
  }
}

using std::make_tuple;

INSTANTIATE_TEST_SUITE_P(
    C, LowbdDrPredTest,
    ::testing::Values(DrPredFunc<DrPred>(&z1_wrapper<av1_dr_prediction_z1_c>,
                                         nullptr, AOM_BITS_8, kZ1Start),
                      DrPredFunc<DrPred>(&z2_wrapper<av1_dr_prediction_z2_c>,
                                         nullptr, AOM_BITS_8, kZ2Start),
                      DrPredFunc<DrPred>(&z3_wrapper<av1_dr_prediction_z3_c>,
                                         nullptr, AOM_BITS_8, kZ3Start)));

#if CONFIG_AV1_HIGHBITDEPTH
class HighbdDrPredTest : public DrPredTest<uint16_t, DrPred_Hbd> {};

TEST_P(HighbdDrPredTest, SaturatedValues) {
  for (enable_upsample_ = 0; enable_upsample_ < 2; ++enable_upsample_) {
    for (int angle = start_angle_; angle < stop_angle_; ++angle) {
      dx_ = av1_get_dx(angle);
      dy_ = av1_get_dy(angle);
      if (dx_ && dy_) RunTest(false, true, angle);
    }
  }
}

INSTANTIATE_TEST_SUITE_P(
    C, HighbdDrPredTest,
    ::testing::Values(
        DrPredFunc<DrPred_Hbd>(&z1_wrapper_hbd<av1_highbd_dr_prediction_z1_c>,
                               nullptr, AOM_BITS_8, kZ1Start),
        DrPredFunc<DrPred_Hbd>(&z1_wrapper_hbd<av1_highbd_dr_prediction_z1_c>,
                               nullptr, AOM_BITS_10, kZ1Start),
        DrPredFunc<DrPred_Hbd>(&z1_wrapper_hbd<av1_highbd_dr_prediction_z1_c>,
                               nullptr, AOM_BITS_12, kZ1Start),
        DrPredFunc<DrPred_Hbd>(&z2_wrapper_hbd<av1_highbd_dr_prediction_z2_c>,
                               nullptr, AOM_BITS_8, kZ2Start),
        DrPredFunc<DrPred_Hbd>(&z2_wrapper_hbd<av1_highbd_dr_prediction_z2_c>,
                               nullptr, AOM_BITS_10, kZ2Start),
        DrPredFunc<DrPred_Hbd>(&z2_wrapper_hbd<av1_highbd_dr_prediction_z2_c>,
                               nullptr, AOM_BITS_12, kZ2Start),
        DrPredFunc<DrPred_Hbd>(&z3_wrapper_hbd<av1_highbd_dr_prediction_z3_c>,
                               nullptr, AOM_BITS_8, kZ3Start),
        DrPredFunc<DrPred_Hbd>(&z3_wrapper_hbd<av1_highbd_dr_prediction_z3_c>,
                               nullptr, AOM_BITS_10, kZ3Start),
        DrPredFunc<DrPred_Hbd>(&z3_wrapper_hbd<av1_highbd_dr_prediction_z3_c>,
                               nullptr, AOM_BITS_12, kZ3Start)));
#endif  // CONFIG_AV1_HIGHBITDEPTH

TEST_P(LowbdDrPredTest, OperationCheck) { RundrPredTest(0); }

TEST_P(LowbdDrPredTest, DISABLED_Speed) { RundrPredTest(1); }

#if HAVE_SSE4_1
INSTANTIATE_TEST_SUITE_P(
    SSE4_1, LowbdDrPredTest,
    ::testing::Values(
        DrPredFunc<DrPred>(&z1_wrapper<av1_dr_prediction_z1_c>,
                           &z1_wrapper<av1_dr_prediction_z1_sse4_1>, AOM_BITS_8,
                           kZ1Start),
        DrPredFunc<DrPred>(&z2_wrapper<av1_dr_prediction_z2_c>,
                           &z2_wrapper<av1_dr_prediction_z2_sse4_1>, AOM_BITS_8,
                           kZ2Start),
        DrPredFunc<DrPred>(&z3_wrapper<av1_dr_prediction_z3_c>,
                           &z3_wrapper<av1_dr_prediction_z3_sse4_1>, AOM_BITS_8,
                           kZ3Start)));
#endif  // HAVE_SSE4_1

#if HAVE_AVX2
INSTANTIATE_TEST_SUITE_P(
    AVX2, LowbdDrPredTest,
    ::testing::Values(DrPredFunc<DrPred>(&z1_wrapper<av1_dr_prediction_z1_c>,
                                         &z1_wrapper<av1_dr_prediction_z1_avx2>,
                                         AOM_BITS_8, kZ1Start),
                      DrPredFunc<DrPred>(&z2_wrapper<av1_dr_prediction_z2_c>,
                                         &z2_wrapper<av1_dr_prediction_z2_avx2>,
                                         AOM_BITS_8, kZ2Start),
                      DrPredFunc<DrPred>(&z3_wrapper<av1_dr_prediction_z3_c>,
                                         &z3_wrapper<av1_dr_prediction_z3_avx2>,
                                         AOM_BITS_8, kZ3Start)));

#if CONFIG_AV1_HIGHBITDEPTH
INSTANTIATE_TEST_SUITE_P(
    AVX2, HighbdDrPredTest,
    ::testing::Values(DrPredFunc<DrPred_Hbd>(
                          &z1_wrapper_hbd<av1_highbd_dr_prediction_z1_c>,
                          &z1_wrapper_hbd<av1_highbd_dr_prediction_z1_avx2>,
                          AOM_BITS_8, kZ1Start),
                      DrPredFunc<DrPred_Hbd>(
                          &z1_wrapper_hbd<av1_highbd_dr_prediction_z1_c>,
                          &z1_wrapper_hbd<av1_highbd_dr_prediction_z1_avx2>,
                          AOM_BITS_10, kZ1Start),
                      DrPredFunc<DrPred_Hbd>(
                          &z1_wrapper_hbd<av1_highbd_dr_prediction_z1_c>,
                          &z1_wrapper_hbd<av1_highbd_dr_prediction_z1_avx2>,
                          AOM_BITS_12, kZ1Start),
                      DrPredFunc<DrPred_Hbd>(
                          &z2_wrapper_hbd<av1_highbd_dr_prediction_z2_c>,
                          &z2_wrapper_hbd<av1_highbd_dr_prediction_z2_avx2>,
                          AOM_BITS_8, kZ2Start),
                      DrPredFunc<DrPred_Hbd>(
                          &z2_wrapper_hbd<av1_highbd_dr_prediction_z2_c>,
                          &z2_wrapper_hbd<av1_highbd_dr_prediction_z2_avx2>,
                          AOM_BITS_10, kZ2Start),
                      DrPredFunc<DrPred_Hbd>(
                          &z2_wrapper_hbd<av1_highbd_dr_prediction_z2_c>,
                          &z2_wrapper_hbd<av1_highbd_dr_prediction_z2_avx2>,
                          AOM_BITS_12, kZ2Start),
                      DrPredFunc<DrPred_Hbd>(
                          &z3_wrapper_hbd<av1_highbd_dr_prediction_z3_c>,
                          &z3_wrapper_hbd<av1_highbd_dr_prediction_z3_avx2>,
                          AOM_BITS_8, kZ3Start),
                      DrPredFunc<DrPred_Hbd>(
                          &z3_wrapper_hbd<av1_highbd_dr_prediction_z3_c>,
                          &z3_wrapper_hbd<av1_highbd_dr_prediction_z3_avx2>,
                          AOM_BITS_10, kZ3Start),
                      DrPredFunc<DrPred_Hbd>(
                          &z3_wrapper_hbd<av1_highbd_dr_prediction_z3_c>,
                          &z3_wrapper_hbd<av1_highbd_dr_prediction_z3_avx2>,
                          AOM_BITS_12, kZ3Start)));

TEST_P(HighbdDrPredTest, DISABLED_Speed) {
  const int angles[] = { 3, 45, 87 };
  for (enable_upsample_ = 0; enable_upsample_ < 2; ++enable_upsample_) {
    for (int i = 0; i < 3; ++i) {
      int angle = angles[i] + start_angle_;
      dx_ = av1_get_dx(angle);
      dy_ = av1_get_dy(angle);
      printf("enable_upsample: %d angle: %d ~~~~~~~~~~~~~~~\n",
             enable_upsample_, angle);
      if (dx_ && dy_) RunTest(true, false, angle);
    }
  }
}

TEST_P(HighbdDrPredTest, OperationCheck) {
  if (params_.tst_fn == nullptr) return;
  // const int angles[] = { 3, 45, 81, 87, 93, 100, 145, 187, 199, 260 };
  for (enable_upsample_ = 0; enable_upsample_ < 2; ++enable_upsample_) {
    for (int angle = start_angle_; angle < stop_angle_; angle++) {
      dx_ = av1_get_dx(angle);
      dy_ = av1_get_dy(angle);
      if (dx_ && dy_) RunTest(false, false, angle);
    }
  }
}
#endif  // CONFIG_AV1_HIGHBITDEPTH
#endif  // HAVE_AVX2

#if HAVE_NEON
INSTANTIATE_TEST_SUITE_P(
    NEON, LowbdDrPredTest,
    ::testing::Values(DrPredFunc<DrPred>(&z1_wrapper<av1_dr_prediction_z1_c>,
                                         &z1_wrapper<av1_dr_prediction_z1_neon>,
                                         AOM_BITS_8, kZ1Start),
                      DrPredFunc<DrPred>(&z2_wrapper<av1_dr_prediction_z2_c>,
                                         &z2_wrapper<av1_dr_prediction_z2_neon>,
                                         AOM_BITS_8, kZ2Start),
                      DrPredFunc<DrPred>(&z3_wrapper<av1_dr_prediction_z3_c>,
                                         &z3_wrapper<av1_dr_prediction_z3_neon>,
                                         AOM_BITS_8, kZ3Start)));

#endif  // HAVE_NEON

}  // namespace
