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

#ifndef AOM_TEST_COMP_AVG_PRED_TEST_H_
#define AOM_TEST_COMP_AVG_PRED_TEST_H_

#include <tuple>

#include "config/aom_dsp_rtcd.h"
#include "config/av1_rtcd.h"

#include "third_party/googletest/src/googletest/include/gtest/gtest.h"
#include "test/acm_random.h"
#include "test/util.h"
#include "test/register_state_check.h"
#include "av1/common/common_data.h"
#include "aom_ports/aom_timer.h"

namespace libaom_test {
const int kMaxSize = 128 + 32;  // padding

namespace AV1DISTWTDCOMPAVG {

typedef void (*distwtdcompavg_func)(uint8_t *comp_pred, const uint8_t *pred,
                                    int width, int height, const uint8_t *ref,
                                    int ref_stride,
                                    const DIST_WTD_COMP_PARAMS *jcp_param);

typedef void (*distwtdcompavgupsampled_func)(
    MACROBLOCKD *xd, const struct AV1Common *const cm, int mi_row, int mi_col,
    const MV *const mv, uint8_t *comp_pred, const uint8_t *pred, int width,
    int height, int subpel_x_q3, int subpel_y_q3, const uint8_t *ref,
    int ref_stride, const DIST_WTD_COMP_PARAMS *jcp_param, int subpel_search);

typedef std::tuple<distwtdcompavg_func, BLOCK_SIZE> DISTWTDCOMPAVGParam;

typedef std::tuple<distwtdcompavgupsampled_func, BLOCK_SIZE>
    DISTWTDCOMPAVGUPSAMPLEDParam;

#if CONFIG_AV1_HIGHBITDEPTH
typedef void (*highbddistwtdcompavgupsampled_func)(
    MACROBLOCKD *xd, const struct AV1Common *const cm, int mi_row, int mi_col,
    const MV *const mv, uint8_t *comp_pred8, const uint8_t *pred8, int width,
    int height, int subpel_x_q3, int subpel_y_q3, const uint8_t *ref8,
    int ref_stride, int bd, const DIST_WTD_COMP_PARAMS *jcp_param,
    int subpel_search);

typedef std::tuple<int, highbddistwtdcompavgupsampled_func, BLOCK_SIZE>
    HighbdDISTWTDCOMPAVGUPSAMPLEDParam;

typedef std::tuple<int, distwtdcompavg_func, BLOCK_SIZE>
    HighbdDISTWTDCOMPAVGParam;

::testing::internal::ParamGenerator<HighbdDISTWTDCOMPAVGParam> BuildParams(
    distwtdcompavg_func filter, int is_hbd) {
  (void)is_hbd;
  return ::testing::Combine(::testing::Range(8, 13, 2),
                            ::testing::Values(filter),
                            ::testing::Range(BLOCK_4X4, BLOCK_SIZES_ALL));
}

::testing::internal::ParamGenerator<HighbdDISTWTDCOMPAVGUPSAMPLEDParam>
BuildParams(highbddistwtdcompavgupsampled_func filter) {
  return ::testing::Combine(::testing::Range(8, 13, 2),
                            ::testing::Values(filter),
                            ::testing::Range(BLOCK_4X4, BLOCK_SIZES_ALL));
}
#endif  // CONFIG_AV1_HIGHBITDEPTH

::testing::internal::ParamGenerator<DISTWTDCOMPAVGParam> BuildParams(
    distwtdcompavg_func filter) {
  return ::testing::Combine(::testing::Values(filter),
                            ::testing::Range(BLOCK_4X4, BLOCK_SIZES_ALL));
}

::testing::internal::ParamGenerator<DISTWTDCOMPAVGUPSAMPLEDParam> BuildParams(
    distwtdcompavgupsampled_func filter) {
  return ::testing::Combine(::testing::Values(filter),
                            ::testing::Range(BLOCK_4X4, BLOCK_SIZES_ALL));
}

class AV1DISTWTDCOMPAVGTest
    : public ::testing::TestWithParam<DISTWTDCOMPAVGParam> {
 public:
  ~AV1DISTWTDCOMPAVGTest() {}
  void SetUp() { rnd_.Reset(ACMRandom::DeterministicSeed()); }

 protected:
  void RunCheckOutput(distwtdcompavg_func test_impl) {
    const int w = kMaxSize, h = kMaxSize;
    const int block_idx = GET_PARAM(1);

    uint8_t pred8[kMaxSize * kMaxSize];
    uint8_t ref8[kMaxSize * kMaxSize];
    uint8_t output[kMaxSize * kMaxSize];
    uint8_t output2[kMaxSize * kMaxSize];

    for (int i = 0; i < h; ++i)
      for (int j = 0; j < w; ++j) {
        pred8[i * w + j] = rnd_.Rand8();
        ref8[i * w + j] = rnd_.Rand8();
      }
    const int in_w = block_size_wide[block_idx];
    const int in_h = block_size_high[block_idx];

    DIST_WTD_COMP_PARAMS dist_wtd_comp_params;
    dist_wtd_comp_params.use_dist_wtd_comp_avg = 1;

    for (int ii = 0; ii < 2; ii++) {
      for (int jj = 0; jj < 4; jj++) {
        dist_wtd_comp_params.fwd_offset = quant_dist_lookup_table[jj][ii];
        dist_wtd_comp_params.bck_offset = quant_dist_lookup_table[jj][1 - ii];

        const int offset_r = 3 + rnd_.PseudoUniform(h - in_h - 7);
        const int offset_c = 3 + rnd_.PseudoUniform(w - in_w - 7);
        aom_dist_wtd_comp_avg_pred_c(output, pred8 + offset_r * w + offset_c,
                                     in_w, in_h, ref8 + offset_r * w + offset_c,
                                     in_w, &dist_wtd_comp_params);
        test_impl(output2, pred8 + offset_r * w + offset_c, in_w, in_h,
                  ref8 + offset_r * w + offset_c, in_w, &dist_wtd_comp_params);

        for (int i = 0; i < in_h; ++i) {
          for (int j = 0; j < in_w; ++j) {
            int idx = i * in_w + j;
            ASSERT_EQ(output[idx], output2[idx])
                << "Mismatch at unit tests for AV1DISTWTDCOMPAVGTest\n"
                << in_w << "x" << in_h << " Pixel mismatch at index " << idx
                << " = (" << i << ", " << j << ")";
          }
        }
      }
    }
  }
  void RunSpeedTest(distwtdcompavg_func test_impl) {
    const int w = kMaxSize, h = kMaxSize;
    const int block_idx = GET_PARAM(1);

    uint8_t pred8[kMaxSize * kMaxSize];
    uint8_t ref8[kMaxSize * kMaxSize];
    uint8_t output[kMaxSize * kMaxSize];
    uint8_t output2[kMaxSize * kMaxSize];

    for (int i = 0; i < h; ++i)
      for (int j = 0; j < w; ++j) {
        pred8[i * w + j] = rnd_.Rand8();
        ref8[i * w + j] = rnd_.Rand8();
      }
    const int in_w = block_size_wide[block_idx];
    const int in_h = block_size_high[block_idx];

    DIST_WTD_COMP_PARAMS dist_wtd_comp_params;
    dist_wtd_comp_params.use_dist_wtd_comp_avg = 1;

    dist_wtd_comp_params.fwd_offset = quant_dist_lookup_table[0][0];
    dist_wtd_comp_params.bck_offset = quant_dist_lookup_table[0][1];

    const int num_loops = 1000000000 / (in_w + in_h);
    aom_usec_timer timer;
    aom_usec_timer_start(&timer);

    for (int i = 0; i < num_loops; ++i)
      aom_dist_wtd_comp_avg_pred_c(output, pred8, in_w, in_h, ref8, in_w,
                                   &dist_wtd_comp_params);

    aom_usec_timer_mark(&timer);
    const int elapsed_time = static_cast<int>(aom_usec_timer_elapsed(&timer));
    printf("distwtdcompavg c_code %3dx%-3d: %7.2f us\n", in_w, in_h,
           1000.0 * elapsed_time / num_loops);

    aom_usec_timer timer1;
    aom_usec_timer_start(&timer1);

    for (int i = 0; i < num_loops; ++i)
      test_impl(output2, pred8, in_w, in_h, ref8, in_w, &dist_wtd_comp_params);

    aom_usec_timer_mark(&timer1);
    const int elapsed_time1 = static_cast<int>(aom_usec_timer_elapsed(&timer1));
    printf("distwtdcompavg test_code %3dx%-3d: %7.2f us\n", in_w, in_h,
           1000.0 * elapsed_time1 / num_loops);
  }

  libaom_test::ACMRandom rnd_;
};  // class AV1DISTWTDCOMPAVGTest

class AV1DISTWTDCOMPAVGUPSAMPLEDTest
    : public ::testing::TestWithParam<DISTWTDCOMPAVGUPSAMPLEDParam> {
 public:
  ~AV1DISTWTDCOMPAVGUPSAMPLEDTest() {}
  void SetUp() { rnd_.Reset(ACMRandom::DeterministicSeed()); }

 protected:
  void RunCheckOutput(distwtdcompavgupsampled_func test_impl) {
    const int w = kMaxSize, h = kMaxSize;
    const int block_idx = GET_PARAM(1);

    uint8_t pred8[kMaxSize * kMaxSize];
    uint8_t ref8[kMaxSize * kMaxSize];
    DECLARE_ALIGNED(16, uint8_t, output[MAX_SB_SQUARE]);
    DECLARE_ALIGNED(16, uint8_t, output2[MAX_SB_SQUARE]);

    for (int i = 0; i < h; ++i)
      for (int j = 0; j < w; ++j) {
        pred8[i * w + j] = rnd_.Rand8();
        ref8[i * w + j] = rnd_.Rand8();
      }
    const int in_w = block_size_wide[block_idx];
    const int in_h = block_size_high[block_idx];

    DIST_WTD_COMP_PARAMS dist_wtd_comp_params;
    dist_wtd_comp_params.use_dist_wtd_comp_avg = 1;
    int sub_x_q3, sub_y_q3;
    int subpel_search;
    for (subpel_search = USE_4_TAPS; subpel_search <= USE_8_TAPS;
         ++subpel_search) {
      for (sub_x_q3 = 0; sub_x_q3 < 8; ++sub_x_q3) {
        for (sub_y_q3 = 0; sub_y_q3 < 8; ++sub_y_q3) {
          for (int ii = 0; ii < 2; ii++) {
            for (int jj = 0; jj < 4; jj++) {
              dist_wtd_comp_params.fwd_offset = quant_dist_lookup_table[jj][ii];
              dist_wtd_comp_params.bck_offset =
                  quant_dist_lookup_table[jj][1 - ii];

              const int offset_r = 3 + rnd_.PseudoUniform(h - in_h - 7);
              const int offset_c = 3 + rnd_.PseudoUniform(w - in_w - 7);

              aom_dist_wtd_comp_avg_upsampled_pred_c(
                  nullptr, nullptr, 0, 0, nullptr, output,
                  pred8 + offset_r * w + offset_c, in_w, in_h, sub_x_q3,
                  sub_y_q3, ref8 + offset_r * w + offset_c, in_w,
                  &dist_wtd_comp_params, subpel_search);
              test_impl(nullptr, nullptr, 0, 0, nullptr, output2,
                        pred8 + offset_r * w + offset_c, in_w, in_h, sub_x_q3,
                        sub_y_q3, ref8 + offset_r * w + offset_c, in_w,
                        &dist_wtd_comp_params, subpel_search);

              for (int i = 0; i < in_h; ++i) {
                for (int j = 0; j < in_w; ++j) {
                  int idx = i * in_w + j;
                  ASSERT_EQ(output[idx], output2[idx])
                      << "Mismatch at unit tests for "
                         "AV1DISTWTDCOMPAVGUPSAMPLEDTest\n"
                      << in_w << "x" << in_h << " Pixel mismatch at index "
                      << idx << " = (" << i << ", " << j
                      << "), sub pixel offset = (" << sub_y_q3 << ", "
                      << sub_x_q3 << ")";
                }
              }
            }
          }
        }
      }
    }
  }
  void RunSpeedTest(distwtdcompavgupsampled_func test_impl) {
    const int w = kMaxSize, h = kMaxSize;
    const int block_idx = GET_PARAM(1);

    uint8_t pred8[kMaxSize * kMaxSize];
    uint8_t ref8[kMaxSize * kMaxSize];
    DECLARE_ALIGNED(16, uint8_t, output[MAX_SB_SQUARE]);
    DECLARE_ALIGNED(16, uint8_t, output2[MAX_SB_SQUARE]);

    for (int i = 0; i < h; ++i)
      for (int j = 0; j < w; ++j) {
        pred8[i * w + j] = rnd_.Rand8();
        ref8[i * w + j] = rnd_.Rand8();
      }
    const int in_w = block_size_wide[block_idx];
    const int in_h = block_size_high[block_idx];

    DIST_WTD_COMP_PARAMS dist_wtd_comp_params;
    dist_wtd_comp_params.use_dist_wtd_comp_avg = 1;

    dist_wtd_comp_params.fwd_offset = quant_dist_lookup_table[0][0];
    dist_wtd_comp_params.bck_offset = quant_dist_lookup_table[0][1];

    int sub_x_q3 = 0;
    int sub_y_q3 = 0;

    const int num_loops = 1000000000 / (in_w + in_h);
    aom_usec_timer timer;
    aom_usec_timer_start(&timer);
    int subpel_search = USE_8_TAPS;  // set to USE_4_TAPS to test 4-tap filter.

    for (int i = 0; i < num_loops; ++i)
      aom_dist_wtd_comp_avg_upsampled_pred_c(
          nullptr, nullptr, 0, 0, nullptr, output, pred8, in_w, in_h, sub_x_q3,
          sub_y_q3, ref8, in_w, &dist_wtd_comp_params, subpel_search);

    aom_usec_timer_mark(&timer);
    const int elapsed_time = static_cast<int>(aom_usec_timer_elapsed(&timer));
    printf("distwtdcompavgupsampled c_code %3dx%-3d: %7.2f us\n", in_w, in_h,
           1000.0 * elapsed_time / num_loops);

    aom_usec_timer timer1;
    aom_usec_timer_start(&timer1);

    for (int i = 0; i < num_loops; ++i)
      test_impl(nullptr, nullptr, 0, 0, nullptr, output2, pred8, in_w, in_h,
                sub_x_q3, sub_y_q3, ref8, in_w, &dist_wtd_comp_params,
                subpel_search);

    aom_usec_timer_mark(&timer1);
    const int elapsed_time1 = static_cast<int>(aom_usec_timer_elapsed(&timer1));
    printf("distwtdcompavgupsampled test_code %3dx%-3d: %7.2f us\n", in_w, in_h,
           1000.0 * elapsed_time1 / num_loops);
  }

  libaom_test::ACMRandom rnd_;
};  // class AV1DISTWTDCOMPAVGUPSAMPLEDTest

#if CONFIG_AV1_HIGHBITDEPTH
class AV1HighBDDISTWTDCOMPAVGTest
    : public ::testing::TestWithParam<HighbdDISTWTDCOMPAVGParam> {
 public:
  ~AV1HighBDDISTWTDCOMPAVGTest() {}
  void SetUp() { rnd_.Reset(ACMRandom::DeterministicSeed()); }

 protected:
  void RunCheckOutput(distwtdcompavg_func test_impl) {
    const int w = kMaxSize, h = kMaxSize;
    const int block_idx = GET_PARAM(2);
    const int bd = GET_PARAM(0);
    uint16_t pred8[kMaxSize * kMaxSize];
    uint16_t ref8[kMaxSize * kMaxSize];
    uint16_t output[kMaxSize * kMaxSize];
    uint16_t output2[kMaxSize * kMaxSize];

    for (int i = 0; i < h; ++i)
      for (int j = 0; j < w; ++j) {
        pred8[i * w + j] = rnd_.Rand16() & ((1 << bd) - 1);
        ref8[i * w + j] = rnd_.Rand16() & ((1 << bd) - 1);
      }
    const int in_w = block_size_wide[block_idx];
    const int in_h = block_size_high[block_idx];

    DIST_WTD_COMP_PARAMS dist_wtd_comp_params;
    dist_wtd_comp_params.use_dist_wtd_comp_avg = 1;

    for (int ii = 0; ii < 2; ii++) {
      for (int jj = 0; jj < 4; jj++) {
        dist_wtd_comp_params.fwd_offset = quant_dist_lookup_table[jj][ii];
        dist_wtd_comp_params.bck_offset = quant_dist_lookup_table[jj][1 - ii];

        const int offset_r = 3 + rnd_.PseudoUniform(h - in_h - 7);
        const int offset_c = 3 + rnd_.PseudoUniform(w - in_w - 7);
        aom_highbd_dist_wtd_comp_avg_pred_c(
            CONVERT_TO_BYTEPTR(output),
            CONVERT_TO_BYTEPTR(pred8) + offset_r * w + offset_c, in_w, in_h,
            CONVERT_TO_BYTEPTR(ref8) + offset_r * w + offset_c, in_w,
            &dist_wtd_comp_params);
        test_impl(CONVERT_TO_BYTEPTR(output2),
                  CONVERT_TO_BYTEPTR(pred8) + offset_r * w + offset_c, in_w,
                  in_h, CONVERT_TO_BYTEPTR(ref8) + offset_r * w + offset_c,
                  in_w, &dist_wtd_comp_params);

        for (int i = 0; i < in_h; ++i) {
          for (int j = 0; j < in_w; ++j) {
            int idx = i * in_w + j;
            ASSERT_EQ(output[idx], output2[idx])
                << "Mismatch at unit tests for AV1HighBDDISTWTDCOMPAVGTest\n"
                << in_w << "x" << in_h << " Pixel mismatch at index " << idx
                << " = (" << i << ", " << j << ")";
          }
        }
      }
    }
  }
  void RunSpeedTest(distwtdcompavg_func test_impl) {
    const int w = kMaxSize, h = kMaxSize;
    const int block_idx = GET_PARAM(2);
    const int bd = GET_PARAM(0);
    uint16_t pred8[kMaxSize * kMaxSize];
    uint16_t ref8[kMaxSize * kMaxSize];
    uint16_t output[kMaxSize * kMaxSize];
    uint16_t output2[kMaxSize * kMaxSize];

    for (int i = 0; i < h; ++i)
      for (int j = 0; j < w; ++j) {
        pred8[i * w + j] = rnd_.Rand16() & ((1 << bd) - 1);
        ref8[i * w + j] = rnd_.Rand16() & ((1 << bd) - 1);
      }
    const int in_w = block_size_wide[block_idx];
    const int in_h = block_size_high[block_idx];

    DIST_WTD_COMP_PARAMS dist_wtd_comp_params;
    dist_wtd_comp_params.use_dist_wtd_comp_avg = 1;

    dist_wtd_comp_params.fwd_offset = quant_dist_lookup_table[0][0];
    dist_wtd_comp_params.bck_offset = quant_dist_lookup_table[0][1];

    const int num_loops = 1000000000 / (in_w + in_h);
    aom_usec_timer timer;
    aom_usec_timer_start(&timer);

    for (int i = 0; i < num_loops; ++i)
      aom_highbd_dist_wtd_comp_avg_pred_c(
          CONVERT_TO_BYTEPTR(output), CONVERT_TO_BYTEPTR(pred8), in_w, in_h,
          CONVERT_TO_BYTEPTR(ref8), in_w, &dist_wtd_comp_params);

    aom_usec_timer_mark(&timer);
    const int elapsed_time = static_cast<int>(aom_usec_timer_elapsed(&timer));
    printf("highbddistwtdcompavg c_code %3dx%-3d: %7.2f us\n", in_w, in_h,
           1000.0 * elapsed_time / num_loops);

    aom_usec_timer timer1;
    aom_usec_timer_start(&timer1);

    for (int i = 0; i < num_loops; ++i)
      test_impl(CONVERT_TO_BYTEPTR(output2), CONVERT_TO_BYTEPTR(pred8), in_w,
                in_h, CONVERT_TO_BYTEPTR(ref8), in_w, &dist_wtd_comp_params);

    aom_usec_timer_mark(&timer1);
    const int elapsed_time1 = static_cast<int>(aom_usec_timer_elapsed(&timer1));
    printf("highbddistwtdcompavg test_code %3dx%-3d: %7.2f us\n", in_w, in_h,
           1000.0 * elapsed_time1 / num_loops);
  }

  libaom_test::ACMRandom rnd_;
};  // class AV1HighBDDISTWTDCOMPAVGTest

class AV1HighBDDISTWTDCOMPAVGUPSAMPLEDTest
    : public ::testing::TestWithParam<HighbdDISTWTDCOMPAVGUPSAMPLEDParam> {
 public:
  ~AV1HighBDDISTWTDCOMPAVGUPSAMPLEDTest() {}
  void SetUp() { rnd_.Reset(ACMRandom::DeterministicSeed()); }

 protected:
  void RunCheckOutput(highbddistwtdcompavgupsampled_func test_impl) {
    const int w = kMaxSize, h = kMaxSize;
    const int block_idx = GET_PARAM(2);
    const int bd = GET_PARAM(0);
    uint16_t pred8[kMaxSize * kMaxSize];
    uint16_t ref8[kMaxSize * kMaxSize];
    DECLARE_ALIGNED(16, uint16_t, output[kMaxSize * kMaxSize]);
    DECLARE_ALIGNED(16, uint16_t, output2[kMaxSize * kMaxSize]);

    for (int i = 0; i < h; ++i)
      for (int j = 0; j < w; ++j) {
        pred8[i * w + j] = rnd_.Rand16() & ((1 << bd) - 1);
        ref8[i * w + j] = rnd_.Rand16() & ((1 << bd) - 1);
      }
    const int in_w = block_size_wide[block_idx];
    const int in_h = block_size_high[block_idx];

    DIST_WTD_COMP_PARAMS dist_wtd_comp_params;
    dist_wtd_comp_params.use_dist_wtd_comp_avg = 1;
    int sub_x_q3, sub_y_q3;
    int subpel_search;
    for (subpel_search = USE_4_TAPS; subpel_search <= USE_8_TAPS;
         ++subpel_search) {
      for (sub_x_q3 = 0; sub_x_q3 < 8; ++sub_x_q3) {
        for (sub_y_q3 = 0; sub_y_q3 < 8; ++sub_y_q3) {
          for (int ii = 0; ii < 2; ii++) {
            for (int jj = 0; jj < 4; jj++) {
              dist_wtd_comp_params.fwd_offset = quant_dist_lookup_table[jj][ii];
              dist_wtd_comp_params.bck_offset =
                  quant_dist_lookup_table[jj][1 - ii];

              const int offset_r = 3 + rnd_.PseudoUniform(h - in_h - 7);
              const int offset_c = 3 + rnd_.PseudoUniform(w - in_w - 7);

              aom_highbd_dist_wtd_comp_avg_upsampled_pred_c(
                  nullptr, nullptr, 0, 0, nullptr, CONVERT_TO_BYTEPTR(output),
                  CONVERT_TO_BYTEPTR(pred8) + offset_r * w + offset_c, in_w,
                  in_h, sub_x_q3, sub_y_q3,
                  CONVERT_TO_BYTEPTR(ref8) + offset_r * w + offset_c, in_w, bd,
                  &dist_wtd_comp_params, subpel_search);
              test_impl(nullptr, nullptr, 0, 0, nullptr,
                        CONVERT_TO_BYTEPTR(output2),
                        CONVERT_TO_BYTEPTR(pred8) + offset_r * w + offset_c,
                        in_w, in_h, sub_x_q3, sub_y_q3,
                        CONVERT_TO_BYTEPTR(ref8) + offset_r * w + offset_c,
                        in_w, bd, &dist_wtd_comp_params, subpel_search);

              for (int i = 0; i < in_h; ++i) {
                for (int j = 0; j < in_w; ++j) {
                  int idx = i * in_w + j;
                  ASSERT_EQ(output[idx], output2[idx])
                      << "Mismatch at unit tests for "
                         "AV1HighBDDISTWTDCOMPAVGUPSAMPLEDTest\n"
                      << in_w << "x" << in_h << " Pixel mismatch at index "
                      << idx << " = (" << i << ", " << j
                      << "), sub pixel offset = (" << sub_y_q3 << ", "
                      << sub_x_q3 << ")";
                }
              }
            }
          }
        }
      }
    }
  }
  void RunSpeedTest(highbddistwtdcompavgupsampled_func test_impl) {
    const int w = kMaxSize, h = kMaxSize;
    const int block_idx = GET_PARAM(2);
    const int bd = GET_PARAM(0);
    uint16_t pred8[kMaxSize * kMaxSize];
    uint16_t ref8[kMaxSize * kMaxSize];
    DECLARE_ALIGNED(16, uint16_t, output[kMaxSize * kMaxSize]);
    DECLARE_ALIGNED(16, uint16_t, output2[kMaxSize * kMaxSize]);

    for (int i = 0; i < h; ++i)
      for (int j = 0; j < w; ++j) {
        pred8[i * w + j] = rnd_.Rand16() & ((1 << bd) - 1);
        ref8[i * w + j] = rnd_.Rand16() & ((1 << bd) - 1);
      }
    const int in_w = block_size_wide[block_idx];
    const int in_h = block_size_high[block_idx];

    DIST_WTD_COMP_PARAMS dist_wtd_comp_params;
    dist_wtd_comp_params.use_dist_wtd_comp_avg = 1;

    dist_wtd_comp_params.fwd_offset = quant_dist_lookup_table[0][0];
    dist_wtd_comp_params.bck_offset = quant_dist_lookup_table[0][1];
    int sub_x_q3 = 0;
    int sub_y_q3 = 0;
    const int num_loops = 1000000000 / (in_w + in_h);
    aom_usec_timer timer;
    aom_usec_timer_start(&timer);
    int subpel_search = USE_8_TAPS;  // set to USE_4_TAPS to test 4-tap filter.
    for (int i = 0; i < num_loops; ++i)
      aom_highbd_dist_wtd_comp_avg_upsampled_pred_c(
          nullptr, nullptr, 0, 0, nullptr, CONVERT_TO_BYTEPTR(output),
          CONVERT_TO_BYTEPTR(pred8), in_w, in_h, sub_x_q3, sub_y_q3,
          CONVERT_TO_BYTEPTR(ref8), in_w, bd, &dist_wtd_comp_params,
          subpel_search);

    aom_usec_timer_mark(&timer);
    const int elapsed_time = static_cast<int>(aom_usec_timer_elapsed(&timer));
    printf("highbddistwtdcompavgupsampled c_code %3dx%-3d: %7.2f us\n", in_w,
           in_h, 1000.0 * elapsed_time / num_loops);

    aom_usec_timer timer1;
    aom_usec_timer_start(&timer1);

    for (int i = 0; i < num_loops; ++i)
      test_impl(nullptr, nullptr, 0, 0, nullptr, CONVERT_TO_BYTEPTR(output2),
                CONVERT_TO_BYTEPTR(pred8), in_w, in_h, sub_x_q3, sub_y_q3,
                CONVERT_TO_BYTEPTR(ref8), in_w, bd, &dist_wtd_comp_params,
                subpel_search);

    aom_usec_timer_mark(&timer1);
    const int elapsed_time1 = static_cast<int>(aom_usec_timer_elapsed(&timer1));
    printf("highbddistwtdcompavgupsampled test_code %3dx%-3d: %7.2f us\n", in_w,
           in_h, 1000.0 * elapsed_time1 / num_loops);
  }

  libaom_test::ACMRandom rnd_;
};      // class AV1HighBDDISTWTDCOMPAVGUPSAMPLEDTest
#endif  // CONFIG_AV1_HIGHBITDEPTH

}  // namespace AV1DISTWTDCOMPAVG
}  // namespace libaom_test

#endif  // AOM_TEST_COMP_AVG_PRED_TEST_H_
