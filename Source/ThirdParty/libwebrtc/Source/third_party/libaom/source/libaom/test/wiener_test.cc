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
#include <utility>
#include <vector>

#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

#include "test/register_state_check.h"
#include "test/acm_random.h"
#include "test/util.h"

#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"

#include "aom/aom_integer.h"
#include "aom_ports/aom_timer.h"
#include "av1/encoder/pickrst.h"

#define MAX_WIENER_BLOCK 384
#define MAX_DATA_BLOCK (MAX_WIENER_BLOCK + WIENER_WIN)

// 8-bit-depth tests
namespace wiener_lowbd {

// C implementation of the algorithm implmented by the SIMD code.
// This is a little more efficient than the version in av1_compute_stats_c().
static void compute_stats_win_opt_c(int wiener_win, const uint8_t *dgd,
                                    const uint8_t *src, int16_t *d, int16_t *s,
                                    int h_start, int h_end, int v_start,
                                    int v_end, int dgd_stride, int src_stride,
                                    int64_t *M, int64_t *H,
                                    int use_downsampled_wiener_stats) {
  ASSERT_TRUE(wiener_win == WIENER_WIN || wiener_win == WIENER_WIN_CHROMA);
  (void)d;
  (void)s;
  int i, j, k, l, m, n;
  const int pixel_count = (h_end - h_start) * (v_end - v_start);
  const int wiener_win2 = wiener_win * wiener_win;
  const int wiener_halfwin = (wiener_win >> 1);
  uint8_t avg = find_average(dgd, h_start, h_end, v_start, v_end, dgd_stride);
  int downsample_factor =
      use_downsampled_wiener_stats ? WIENER_STATS_DOWNSAMPLE_FACTOR : 1;

  std::vector<std::vector<int64_t> > M_int(wiener_win,
                                           std::vector<int64_t>(wiener_win, 0));
  std::vector<std::vector<int64_t> > H_int(
      wiener_win * wiener_win, std::vector<int64_t>(wiener_win * 8, 0));
  std::vector<std::vector<int32_t> > sumY(wiener_win,
                                          std::vector<int32_t>(wiener_win, 0));
  int32_t sumX = 0;
  const uint8_t *dgd_win = dgd - wiener_halfwin * dgd_stride - wiener_halfwin;

  // Main loop handles two pixels at a time
  // We can assume that h_start is even, since it will always be aligned to
  // a tile edge + some number of restoration units, and both of those will
  // be 64-pixel aligned.
  // However, at the edge of the image, h_end may be odd, so we need to handle
  // that case correctly.
  assert(h_start % 2 == 0);
  for (i = v_start; i < v_end; i = i + downsample_factor) {
    if (use_downsampled_wiener_stats &&
        (v_end - i < WIENER_STATS_DOWNSAMPLE_FACTOR)) {
      downsample_factor = v_end - i;
    }
    int32_t sumX_row_i32 = 0;
    std::vector<std::vector<int32_t> > sumY_row(
        wiener_win, std::vector<int32_t>(wiener_win, 0));
    std::vector<std::vector<int32_t> > M_row_i32(
        wiener_win, std::vector<int32_t>(wiener_win, 0));
    std::vector<std::vector<int32_t> > H_row_i32(
        wiener_win * wiener_win, std::vector<int32_t>(wiener_win * 8, 0));
    const int h_end_even = h_end & ~1;
    const int has_odd_pixel = h_end & 1;
    for (j = h_start; j < h_end_even; j += 2) {
      const uint8_t X1 = src[i * src_stride + j];
      const uint8_t X2 = src[i * src_stride + j + 1];
      sumX_row_i32 += X1 + X2;

      const uint8_t *dgd_ij = dgd_win + i * dgd_stride + j;
      for (k = 0; k < wiener_win; k++) {
        for (l = 0; l < wiener_win; l++) {
          const uint8_t *dgd_ijkl = dgd_ij + k * dgd_stride + l;
          int32_t *H_int_temp = &H_row_i32[(l * wiener_win + k)][0];
          const uint8_t D1 = dgd_ijkl[0];
          const uint8_t D2 = dgd_ijkl[1];
          sumY_row[k][l] += D1 + D2;
          M_row_i32[l][k] += D1 * X1 + D2 * X2;
          for (m = 0; m < wiener_win; m++) {
            for (n = 0; n < wiener_win; n++) {
              H_int_temp[m * 8 + n] += D1 * dgd_ij[n + dgd_stride * m] +
                                       D2 * dgd_ij[n + dgd_stride * m + 1];
            }
          }
        }
      }
    }
    // If the width is odd, add in the final pixel
    if (has_odd_pixel) {
      const uint8_t X1 = src[i * src_stride + j];
      sumX_row_i32 += X1;

      const uint8_t *dgd_ij = dgd_win + i * dgd_stride + j;
      for (k = 0; k < wiener_win; k++) {
        for (l = 0; l < wiener_win; l++) {
          const uint8_t *dgd_ijkl = dgd_ij + k * dgd_stride + l;
          int32_t *H_int_temp = &H_row_i32[(l * wiener_win + k)][0];
          const uint8_t D1 = dgd_ijkl[0];
          sumY_row[k][l] += D1;
          M_row_i32[l][k] += D1 * X1;
          for (m = 0; m < wiener_win; m++) {
            for (n = 0; n < wiener_win; n++) {
              H_int_temp[m * 8 + n] += D1 * dgd_ij[n + dgd_stride * m];
            }
          }
        }
      }
    }

    sumX += sumX_row_i32 * downsample_factor;
    // Scale M matrix based on the downsampling factor
    for (k = 0; k < wiener_win; ++k) {
      for (l = 0; l < wiener_win; ++l) {
        sumY[k][l] += sumY_row[k][l] * downsample_factor;
        M_int[k][l] += (int64_t)M_row_i32[k][l] * downsample_factor;
      }
    }
    // Scale H matrix based on the downsampling factor
    for (k = 0; k < wiener_win * wiener_win; ++k) {
      for (l = 0; l < wiener_win * 8; ++l) {
        H_int[k][l] += (int64_t)H_row_i32[k][l] * downsample_factor;
      }
    }
  }

  const int64_t avg_square_sum = (int64_t)avg * (int64_t)avg * pixel_count;
  for (k = 0; k < wiener_win; k++) {
    for (l = 0; l < wiener_win; l++) {
      M[l * wiener_win + k] =
          M_int[l][k] + avg_square_sum - (int64_t)avg * (sumX + sumY[k][l]);
      for (m = 0; m < wiener_win; m++) {
        for (n = 0; n < wiener_win; n++) {
          H[(l * wiener_win + k) * wiener_win2 + m * wiener_win + n] =
              H_int[(l * wiener_win + k)][n * 8 + m] + avg_square_sum -
              (int64_t)avg * (sumY[k][l] + sumY[n][m]);
        }
      }
    }
  }
}

void compute_stats_opt_c(int wiener_win, const uint8_t *dgd, const uint8_t *src,
                         int16_t *d, int16_t *s, int h_start, int h_end,
                         int v_start, int v_end, int dgd_stride, int src_stride,
                         int64_t *M, int64_t *H,
                         int use_downsampled_wiener_stats) {
  if (wiener_win == WIENER_WIN || wiener_win == WIENER_WIN_CHROMA) {
    compute_stats_win_opt_c(wiener_win, dgd, src, d, s, h_start, h_end, v_start,
                            v_end, dgd_stride, src_stride, M, H,
                            use_downsampled_wiener_stats);
  } else {
    av1_compute_stats_c(wiener_win, dgd, src, d, s, h_start, h_end, v_start,
                        v_end, dgd_stride, src_stride, M, H,
                        use_downsampled_wiener_stats);
  }
}

static const int kIterations = 100;
typedef void (*compute_stats_Func)(int wiener_win, const uint8_t *dgd,
                                   const uint8_t *src, int16_t *dgd_avg,
                                   int16_t *src_avg, int h_start, int h_end,
                                   int v_start, int v_end, int dgd_stride,
                                   int src_stride, int64_t *M, int64_t *H,
                                   int use_downsampled_wiener_stats);

////////////////////////////////////////////////////////////////////////////////
// 8 bit
////////////////////////////////////////////////////////////////////////////////

typedef std::tuple<const compute_stats_Func> WienerTestParam;

class WienerTest : public ::testing::TestWithParam<WienerTestParam> {
 public:
  virtual void SetUp() {
    src_buf = (uint8_t *)aom_memalign(
        32, MAX_DATA_BLOCK * MAX_DATA_BLOCK * sizeof(*src_buf));
    ASSERT_NE(src_buf, nullptr);
    dgd_buf = (uint8_t *)aom_memalign(
        32, MAX_DATA_BLOCK * MAX_DATA_BLOCK * sizeof(*dgd_buf));
    ASSERT_NE(dgd_buf, nullptr);
    const int buf_size =
        sizeof(*buf) * 6 * RESTORATION_UNITSIZE_MAX * RESTORATION_UNITSIZE_MAX;
    buf = (int16_t *)aom_memalign(32, buf_size);
    ASSERT_NE(buf, nullptr);
    memset(buf, 0, buf_size);
    target_func_ = GET_PARAM(0);
  }
  virtual void TearDown() {
    aom_free(src_buf);
    aom_free(dgd_buf);
    aom_free(buf);
  }
  void RunWienerTest(const int32_t wiener_win, int32_t run_times);
  void RunWienerTest_ExtremeValues(const int32_t wiener_win);

 private:
  compute_stats_Func target_func_;
  libaom_test::ACMRandom rng_;
  uint8_t *src_buf;
  uint8_t *dgd_buf;
  int16_t *buf;
};

void WienerTest::RunWienerTest(const int32_t wiener_win, int32_t run_times) {
  const int32_t wiener_halfwin = wiener_win >> 1;
  const int32_t wiener_win2 = wiener_win * wiener_win;
  DECLARE_ALIGNED(32, int64_t, M_ref[WIENER_WIN2]);
  DECLARE_ALIGNED(32, int64_t, H_ref[WIENER_WIN2 * WIENER_WIN2]);
  DECLARE_ALIGNED(32, int64_t, M_test[WIENER_WIN2]);
  DECLARE_ALIGNED(32, int64_t, H_test[WIENER_WIN2 * WIENER_WIN2]);
  // Note(rachelbarker):
  // The SIMD code requires `h_start` to be even, but can otherwise
  // deal with any values of `h_end`, `v_start`, `v_end`. We cover this
  // entire range, even though (at the time of writing) `h_start` and `v_start`
  // will always be multiples of 64 when called from non-test code.
  // If in future any new requirements are added, these lines will
  // need changing.
  int h_start = (rng_.Rand16() % (MAX_WIENER_BLOCK / 2)) & ~1;
  int h_end = run_times != 1 ? 256 : (rng_.Rand16() % MAX_WIENER_BLOCK);
  if (h_start > h_end) std::swap(h_start, h_end);
  int v_start = rng_.Rand16() % (MAX_WIENER_BLOCK / 2);
  int v_end = run_times != 1 ? 256 : (rng_.Rand16() % MAX_WIENER_BLOCK);
  if (v_start > v_end) std::swap(v_start, v_end);
  const int dgd_stride = h_end;
  const int src_stride = MAX_DATA_BLOCK;
  const int iters = run_times == 1 ? kIterations : 2;
  const int max_value_downsample_stats = 1;
  int16_t *dgd_avg = buf;
  int16_t *src_avg =
      buf + (3 * RESTORATION_UNITSIZE_MAX * RESTORATION_UNITSIZE_MAX);

  for (int iter = 0; iter < iters && !HasFatalFailure(); ++iter) {
    for (int i = 0; i < MAX_DATA_BLOCK * MAX_DATA_BLOCK; ++i) {
      dgd_buf[i] = rng_.Rand8();
      src_buf[i] = rng_.Rand8();
    }
    uint8_t *dgd = dgd_buf + wiener_halfwin * MAX_DATA_BLOCK + wiener_halfwin;
    uint8_t *src = src_buf;
    for (int use_downsampled_stats = 0;
         use_downsampled_stats <= max_value_downsample_stats;
         use_downsampled_stats++) {
      aom_usec_timer timer;
      aom_usec_timer_start(&timer);
      for (int i = 0; i < run_times; ++i) {
        av1_compute_stats_c(wiener_win, dgd, src, dgd_avg, src_avg, h_start,
                            h_end, v_start, v_end, dgd_stride, src_stride,
                            M_ref, H_ref, use_downsampled_stats);
      }
      aom_usec_timer_mark(&timer);
      const double time1 = static_cast<double>(aom_usec_timer_elapsed(&timer));
      aom_usec_timer_start(&timer);
      for (int i = 0; i < run_times; ++i) {
        target_func_(wiener_win, dgd, src, dgd_avg, src_avg, h_start, h_end,
                     v_start, v_end, dgd_stride, src_stride, M_test, H_test,
                     use_downsampled_stats);
      }
      aom_usec_timer_mark(&timer);
      const double time2 = static_cast<double>(aom_usec_timer_elapsed(&timer));
      if (run_times > 10) {
        printf("win %d %3dx%-3d:%7.2f/%7.2fns", wiener_win, h_end, v_end, time1,
               time2);
        printf("(%3.2f)\n", time1 / time2);
      }
      int failed = 0;
      for (int i = 0; i < wiener_win2; ++i) {
        if (M_ref[i] != M_test[i]) {
          failed = 1;
          printf("win %d M iter %d [%4d] ref %6" PRId64 " test %6" PRId64 " \n",
                 wiener_win, iter, i, M_ref[i], M_test[i]);
          break;
        }
      }
      for (int i = 0; i < wiener_win2 * wiener_win2; ++i) {
        if (H_ref[i] != H_test[i]) {
          failed = 1;
          printf("win %d H iter %d [%4d] ref %6" PRId64 " test %6" PRId64 " \n",
                 wiener_win, iter, i, H_ref[i], H_test[i]);
          break;
        }
      }
      ASSERT_EQ(failed, 0);
    }
  }
}

void WienerTest::RunWienerTest_ExtremeValues(const int32_t wiener_win) {
  const int32_t wiener_halfwin = wiener_win >> 1;
  const int32_t wiener_win2 = wiener_win * wiener_win;
  DECLARE_ALIGNED(32, int64_t, M_ref[WIENER_WIN2]);
  DECLARE_ALIGNED(32, int64_t, H_ref[WIENER_WIN2 * WIENER_WIN2]);
  DECLARE_ALIGNED(32, int64_t, M_test[WIENER_WIN2]);
  DECLARE_ALIGNED(32, int64_t, H_test[WIENER_WIN2 * WIENER_WIN2]);
  const int h_start = 16;
  const int h_end = MAX_WIENER_BLOCK;
  const int v_start = 16;
  const int v_end = MAX_WIENER_BLOCK;
  const int dgd_stride = h_end;
  const int src_stride = MAX_DATA_BLOCK;
  const int iters = 1;
  const int max_value_downsample_stats = 1;
  int16_t *dgd_avg = buf;
  int16_t *src_avg =
      buf + (3 * RESTORATION_UNITSIZE_MAX * RESTORATION_UNITSIZE_MAX);

  for (int iter = 0; iter < iters && !HasFatalFailure(); ++iter) {
    for (int i = 0; i < MAX_DATA_BLOCK * MAX_DATA_BLOCK; ++i) {
      dgd_buf[i] = 255;
      src_buf[i] = 255;
    }
    uint8_t *dgd = dgd_buf + wiener_halfwin * MAX_DATA_BLOCK + wiener_halfwin;
    uint8_t *src = src_buf;
    for (int use_downsampled_stats = 0;
         use_downsampled_stats <= max_value_downsample_stats;
         use_downsampled_stats++) {
      av1_compute_stats_c(wiener_win, dgd, src, dgd_avg, src_avg, h_start,
                          h_end, v_start, v_end, dgd_stride, src_stride, M_ref,
                          H_ref, use_downsampled_stats);

      target_func_(wiener_win, dgd, src, dgd_avg, src_avg, h_start, h_end,
                   v_start, v_end, dgd_stride, src_stride, M_test, H_test,
                   use_downsampled_stats);

      int failed = 0;
      for (int i = 0; i < wiener_win2; ++i) {
        if (M_ref[i] != M_test[i]) {
          failed = 1;
          printf("win %d M iter %d [%4d] ref %6" PRId64 " test %6" PRId64 " \n",
                 wiener_win, iter, i, M_ref[i], M_test[i]);
          break;
        }
      }
      for (int i = 0; i < wiener_win2 * wiener_win2; ++i) {
        if (H_ref[i] != H_test[i]) {
          failed = 1;
          printf("win %d H iter %d [%4d] ref %6" PRId64 " test %6" PRId64 " \n",
                 wiener_win, iter, i, H_ref[i], H_test[i]);
          break;
        }
      }
      ASSERT_EQ(failed, 0);
    }
  }
}

TEST_P(WienerTest, RandomValues) {
  RunWienerTest(WIENER_WIN, 1);
  RunWienerTest(WIENER_WIN_CHROMA, 1);
}

TEST_P(WienerTest, ExtremeValues) {
  RunWienerTest_ExtremeValues(WIENER_WIN);
  RunWienerTest_ExtremeValues(WIENER_WIN_CHROMA);
}

TEST_P(WienerTest, DISABLED_Speed) {
  RunWienerTest(WIENER_WIN, 200);
  RunWienerTest(WIENER_WIN_CHROMA, 200);
}

INSTANTIATE_TEST_SUITE_P(C, WienerTest, ::testing::Values(compute_stats_opt_c));

#if HAVE_SSE4_1
INSTANTIATE_TEST_SUITE_P(SSE4_1, WienerTest,
                         ::testing::Values(av1_compute_stats_sse4_1));
#endif  // HAVE_SSE4_1

#if HAVE_AVX2

INSTANTIATE_TEST_SUITE_P(AVX2, WienerTest,
                         ::testing::Values(av1_compute_stats_avx2));
#endif  // HAVE_AVX2

}  // namespace wiener_lowbd

#if CONFIG_AV1_HIGHBITDEPTH
// High bit-depth tests:
namespace wiener_highbd {

static void compute_stats_highbd_win_opt_c(int wiener_win, const uint8_t *dgd8,
                                           const uint8_t *src8, int h_start,
                                           int h_end, int v_start, int v_end,
                                           int dgd_stride, int src_stride,
                                           int64_t *M, int64_t *H,
                                           aom_bit_depth_t bit_depth) {
  ASSERT_TRUE(wiener_win == WIENER_WIN || wiener_win == WIENER_WIN_CHROMA);
  int i, j, k, l, m, n;
  const int pixel_count = (h_end - h_start) * (v_end - v_start);
  const int wiener_win2 = wiener_win * wiener_win;
  const int wiener_halfwin = (wiener_win >> 1);
  const uint16_t *src = CONVERT_TO_SHORTPTR(src8);
  const uint16_t *dgd = CONVERT_TO_SHORTPTR(dgd8);
  const uint16_t avg =
      find_average_highbd(dgd, h_start, h_end, v_start, v_end, dgd_stride);

  std::vector<std::vector<int64_t> > M_int(wiener_win,
                                           std::vector<int64_t>(wiener_win, 0));
  std::vector<std::vector<int64_t> > H_int(
      wiener_win * wiener_win, std::vector<int64_t>(wiener_win * 8, 0));
  std::vector<std::vector<int32_t> > sumY(wiener_win,
                                          std::vector<int32_t>(wiener_win, 0));

  memset(M, 0, sizeof(*M) * wiener_win2);
  memset(H, 0, sizeof(*H) * wiener_win2 * wiener_win2);

  int64_t sumX = 0;
  const uint16_t *dgd_win = dgd - wiener_halfwin * dgd_stride - wiener_halfwin;

  // Main loop handles two pixels at a time
  // We can assume that h_start is even, since it will always be aligned to
  // a tile edge + some number of restoration units, and both of those will
  // be 64-pixel aligned.
  // However, at the edge of the image, h_end may be odd, so we need to handle
  // that case correctly.
  assert(h_start % 2 == 0);
  for (i = v_start; i < v_end; i++) {
    const int h_end_even = h_end & ~1;
    const int has_odd_pixel = h_end & 1;
    for (j = h_start; j < h_end_even; j += 2) {
      const uint16_t X1 = src[i * src_stride + j];
      const uint16_t X2 = src[i * src_stride + j + 1];
      sumX += X1 + X2;

      const uint16_t *dgd_ij = dgd_win + i * dgd_stride + j;
      for (k = 0; k < wiener_win; k++) {
        for (l = 0; l < wiener_win; l++) {
          const uint16_t *dgd_ijkl = dgd_ij + k * dgd_stride + l;
          int64_t *H_int_temp = &H_int[(l * wiener_win + k)][0];
          const uint16_t D1 = dgd_ijkl[0];
          const uint16_t D2 = dgd_ijkl[1];
          sumY[k][l] += D1 + D2;
          M_int[l][k] += D1 * X1 + D2 * X2;
          for (m = 0; m < wiener_win; m++) {
            for (n = 0; n < wiener_win; n++) {
              H_int_temp[m * 8 + n] += D1 * dgd_ij[n + dgd_stride * m] +
                                       D2 * dgd_ij[n + dgd_stride * m + 1];
            }
          }
        }
      }
    }
    // If the width is odd, add in the final pixel
    if (has_odd_pixel) {
      const uint16_t X1 = src[i * src_stride + j];
      sumX += X1;

      const uint16_t *dgd_ij = dgd_win + i * dgd_stride + j;
      for (k = 0; k < wiener_win; k++) {
        for (l = 0; l < wiener_win; l++) {
          const uint16_t *dgd_ijkl = dgd_ij + k * dgd_stride + l;
          int64_t *H_int_temp = &H_int[(l * wiener_win + k)][0];
          const uint16_t D1 = dgd_ijkl[0];
          sumY[k][l] += D1;
          M_int[l][k] += D1 * X1;
          for (m = 0; m < wiener_win; m++) {
            for (n = 0; n < wiener_win; n++) {
              H_int_temp[m * 8 + n] += D1 * dgd_ij[n + dgd_stride * m];
            }
          }
        }
      }
    }
  }

  uint8_t bit_depth_divider = 1;
  if (bit_depth == AOM_BITS_12)
    bit_depth_divider = 16;
  else if (bit_depth == AOM_BITS_10)
    bit_depth_divider = 4;

  const int64_t avg_square_sum = (int64_t)avg * (int64_t)avg * pixel_count;
  for (k = 0; k < wiener_win; k++) {
    for (l = 0; l < wiener_win; l++) {
      M[l * wiener_win + k] =
          (M_int[l][k] +
           (avg_square_sum - (int64_t)avg * (sumX + sumY[k][l]))) /
          bit_depth_divider;
      for (m = 0; m < wiener_win; m++) {
        for (n = 0; n < wiener_win; n++) {
          H[(l * wiener_win + k) * wiener_win2 + m * wiener_win + n] =
              (H_int[(l * wiener_win + k)][n * 8 + m] +
               (avg_square_sum - (int64_t)avg * (sumY[k][l] + sumY[n][m]))) /
              bit_depth_divider;
        }
      }
    }
  }
}

void compute_stats_highbd_opt_c(int wiener_win, const uint8_t *dgd,
                                const uint8_t *src, int h_start, int h_end,
                                int v_start, int v_end, int dgd_stride,
                                int src_stride, int64_t *M, int64_t *H,
                                aom_bit_depth_t bit_depth) {
  if (wiener_win == WIENER_WIN || wiener_win == WIENER_WIN_CHROMA) {
    compute_stats_highbd_win_opt_c(wiener_win, dgd, src, h_start, h_end,
                                   v_start, v_end, dgd_stride, src_stride, M, H,
                                   bit_depth);
  } else {
    av1_compute_stats_highbd_c(wiener_win, dgd, src, h_start, h_end, v_start,
                               v_end, dgd_stride, src_stride, M, H, bit_depth);
  }
}

static const int kIterations = 100;
typedef void (*compute_stats_Func)(int wiener_win, const uint8_t *dgd,
                                   const uint8_t *src, int h_start, int h_end,
                                   int v_start, int v_end, int dgd_stride,
                                   int src_stride, int64_t *M, int64_t *H,
                                   aom_bit_depth_t bit_depth);

typedef std::tuple<const compute_stats_Func> WienerTestParam;

class WienerTestHighbd : public ::testing::TestWithParam<WienerTestParam> {
 public:
  virtual void SetUp() {
    src_buf = (uint16_t *)aom_memalign(
        32, MAX_DATA_BLOCK * MAX_DATA_BLOCK * sizeof(*src_buf));
    ASSERT_NE(src_buf, nullptr);
    dgd_buf = (uint16_t *)aom_memalign(
        32, MAX_DATA_BLOCK * MAX_DATA_BLOCK * sizeof(*dgd_buf));
    ASSERT_NE(dgd_buf, nullptr);
    target_func_ = GET_PARAM(0);
  }
  virtual void TearDown() {
    aom_free(src_buf);
    aom_free(dgd_buf);
  }
  void RunWienerTest(const int32_t wiener_win, int32_t run_times,
                     aom_bit_depth_t bit_depth);
  void RunWienerTest_ExtremeValues(const int32_t wiener_win,
                                   aom_bit_depth_t bit_depth);

 private:
  compute_stats_Func target_func_;
  libaom_test::ACMRandom rng_;
  uint16_t *src_buf;
  uint16_t *dgd_buf;
};

void WienerTestHighbd::RunWienerTest(const int32_t wiener_win,
                                     int32_t run_times,
                                     aom_bit_depth_t bit_depth) {
  const int32_t wiener_halfwin = wiener_win >> 1;
  const int32_t wiener_win2 = wiener_win * wiener_win;
  DECLARE_ALIGNED(32, int64_t, M_ref[WIENER_WIN2]);
  DECLARE_ALIGNED(32, int64_t, H_ref[WIENER_WIN2 * WIENER_WIN2]);
  DECLARE_ALIGNED(32, int64_t, M_test[WIENER_WIN2]);
  DECLARE_ALIGNED(32, int64_t, H_test[WIENER_WIN2 * WIENER_WIN2]);
  // Note(rachelbarker):
  // The SIMD code requires `h_start` to be even, but can otherwise
  // deal with any values of `h_end`, `v_start`, `v_end`. We cover this
  // entire range, even though (at the time of writing) `h_start` and `v_start`
  // will always be multiples of 64 when called from non-test code.
  // If in future any new requirements are added, these lines will
  // need changing.
  int h_start = (rng_.Rand16() % (MAX_WIENER_BLOCK / 2)) & ~1;
  int h_end = run_times != 1 ? 256 : (rng_.Rand16() % MAX_WIENER_BLOCK);
  if (h_start > h_end) std::swap(h_start, h_end);
  int v_start = rng_.Rand16() % (MAX_WIENER_BLOCK / 2);
  int v_end = run_times != 1 ? 256 : (rng_.Rand16() % MAX_WIENER_BLOCK);
  if (v_start > v_end) std::swap(v_start, v_end);
  const int dgd_stride = h_end;
  const int src_stride = MAX_DATA_BLOCK;
  const int iters = run_times == 1 ? kIterations : 2;
  for (int iter = 0; iter < iters && !HasFatalFailure(); ++iter) {
    for (int i = 0; i < MAX_DATA_BLOCK * MAX_DATA_BLOCK; ++i) {
      dgd_buf[i] = rng_.Rand16() % (1 << bit_depth);
      src_buf[i] = rng_.Rand16() % (1 << bit_depth);
    }
    const uint8_t *dgd8 = CONVERT_TO_BYTEPTR(
        dgd_buf + wiener_halfwin * MAX_DATA_BLOCK + wiener_halfwin);
    const uint8_t *src8 = CONVERT_TO_BYTEPTR(src_buf);

    aom_usec_timer timer;
    aom_usec_timer_start(&timer);
    for (int i = 0; i < run_times; ++i) {
      av1_compute_stats_highbd_c(wiener_win, dgd8, src8, h_start, h_end,
                                 v_start, v_end, dgd_stride, src_stride, M_ref,
                                 H_ref, bit_depth);
    }
    aom_usec_timer_mark(&timer);
    const double time1 = static_cast<double>(aom_usec_timer_elapsed(&timer));
    aom_usec_timer_start(&timer);
    for (int i = 0; i < run_times; ++i) {
      target_func_(wiener_win, dgd8, src8, h_start, h_end, v_start, v_end,
                   dgd_stride, src_stride, M_test, H_test, bit_depth);
    }
    aom_usec_timer_mark(&timer);
    const double time2 = static_cast<double>(aom_usec_timer_elapsed(&timer));
    if (run_times > 10) {
      printf("win %d bd %d %3dx%-3d:%7.2f/%7.2fns", wiener_win, bit_depth,
             h_end, v_end, time1, time2);
      printf("(%3.2f)\n", time1 / time2);
    }
    int failed = 0;
    for (int i = 0; i < wiener_win2; ++i) {
      if (M_ref[i] != M_test[i]) {
        failed = 1;
        printf("win %d bd %d M iter %d [%4d] ref %6" PRId64 " test %6" PRId64
               " \n",
               wiener_win, bit_depth, iter, i, M_ref[i], M_test[i]);
        break;
      }
    }
    for (int i = 0; i < wiener_win2 * wiener_win2; ++i) {
      if (H_ref[i] != H_test[i]) {
        failed = 1;
        printf("win %d bd %d H iter %d [%4d] ref %6" PRId64 " test %6" PRId64
               " \n",
               wiener_win, bit_depth, iter, i, H_ref[i], H_test[i]);
        break;
      }
    }
    ASSERT_EQ(failed, 0);
  }
}

void WienerTestHighbd::RunWienerTest_ExtremeValues(const int32_t wiener_win,
                                                   aom_bit_depth_t bit_depth) {
  const int32_t wiener_halfwin = wiener_win >> 1;
  const int32_t wiener_win2 = wiener_win * wiener_win;
  DECLARE_ALIGNED(32, int64_t, M_ref[WIENER_WIN2]);
  DECLARE_ALIGNED(32, int64_t, H_ref[WIENER_WIN2 * WIENER_WIN2]);
  DECLARE_ALIGNED(32, int64_t, M_test[WIENER_WIN2]);
  DECLARE_ALIGNED(32, int64_t, H_test[WIENER_WIN2 * WIENER_WIN2]);
  const int h_start = 16;
  const int h_end = MAX_WIENER_BLOCK;
  const int v_start = 16;
  const int v_end = MAX_WIENER_BLOCK;
  const int dgd_stride = h_end;
  const int src_stride = MAX_DATA_BLOCK;
  const int iters = 1;
  for (int iter = 0; iter < iters && !HasFatalFailure(); ++iter) {
    for (int i = 0; i < MAX_DATA_BLOCK * MAX_DATA_BLOCK; ++i) {
      dgd_buf[i] = ((uint16_t)1 << bit_depth) - 1;
      src_buf[i] = ((uint16_t)1 << bit_depth) - 1;
    }
    const uint8_t *dgd8 = CONVERT_TO_BYTEPTR(
        dgd_buf + wiener_halfwin * MAX_DATA_BLOCK + wiener_halfwin);
    const uint8_t *src8 = CONVERT_TO_BYTEPTR(src_buf);

    av1_compute_stats_highbd_c(wiener_win, dgd8, src8, h_start, h_end, v_start,
                               v_end, dgd_stride, src_stride, M_ref, H_ref,
                               bit_depth);

    target_func_(wiener_win, dgd8, src8, h_start, h_end, v_start, v_end,
                 dgd_stride, src_stride, M_test, H_test, bit_depth);

    int failed = 0;
    for (int i = 0; i < wiener_win2; ++i) {
      if (M_ref[i] != M_test[i]) {
        failed = 1;
        printf("win %d bd %d M iter %d [%4d] ref %6" PRId64 " test %6" PRId64
               " \n",
               wiener_win, bit_depth, iter, i, M_ref[i], M_test[i]);
        break;
      }
    }
    for (int i = 0; i < wiener_win2 * wiener_win2; ++i) {
      if (H_ref[i] != H_test[i]) {
        failed = 1;
        printf("win %d bd %d H iter %d [%4d] ref %6" PRId64 " test %6" PRId64
               " \n",
               wiener_win, bit_depth, iter, i, H_ref[i], H_test[i]);
        break;
      }
    }
    ASSERT_EQ(failed, 0);
  }
}

TEST_P(WienerTestHighbd, RandomValues) {
  RunWienerTest(WIENER_WIN, 1, AOM_BITS_8);
  RunWienerTest(WIENER_WIN_CHROMA, 1, AOM_BITS_8);
  RunWienerTest(WIENER_WIN, 1, AOM_BITS_10);
  RunWienerTest(WIENER_WIN_CHROMA, 1, AOM_BITS_10);
  RunWienerTest(WIENER_WIN, 1, AOM_BITS_12);
  RunWienerTest(WIENER_WIN_CHROMA, 1, AOM_BITS_12);
}

TEST_P(WienerTestHighbd, ExtremeValues) {
  RunWienerTest_ExtremeValues(WIENER_WIN, AOM_BITS_8);
  RunWienerTest_ExtremeValues(WIENER_WIN_CHROMA, AOM_BITS_8);
  RunWienerTest_ExtremeValues(WIENER_WIN, AOM_BITS_10);
  RunWienerTest_ExtremeValues(WIENER_WIN_CHROMA, AOM_BITS_10);
  RunWienerTest_ExtremeValues(WIENER_WIN, AOM_BITS_12);
  RunWienerTest_ExtremeValues(WIENER_WIN_CHROMA, AOM_BITS_12);
}

TEST_P(WienerTestHighbd, DISABLED_Speed) {
  RunWienerTest(WIENER_WIN, 200, AOM_BITS_8);
  RunWienerTest(WIENER_WIN_CHROMA, 200, AOM_BITS_8);
  RunWienerTest(WIENER_WIN, 200, AOM_BITS_10);
  RunWienerTest(WIENER_WIN_CHROMA, 200, AOM_BITS_10);
  RunWienerTest(WIENER_WIN, 200, AOM_BITS_12);
  RunWienerTest(WIENER_WIN_CHROMA, 200, AOM_BITS_12);
}

INSTANTIATE_TEST_SUITE_P(C, WienerTestHighbd,
                         ::testing::Values(compute_stats_highbd_opt_c));

#if HAVE_SSE4_1
INSTANTIATE_TEST_SUITE_P(SSE4_1, WienerTestHighbd,
                         ::testing::Values(av1_compute_stats_highbd_sse4_1));
#endif  // HAVE_SSE4_1

#if HAVE_AVX2
INSTANTIATE_TEST_SUITE_P(AVX2, WienerTestHighbd,
                         ::testing::Values(av1_compute_stats_highbd_avx2));
#endif  // HAVE_AVX2

// A test that reproduces b/274668506: signed integer overflow in
// update_a_sep_sym().
TEST(SearchWienerTest, 10bitSignedIntegerOverflowInUpdateASepSym) {
  constexpr int kWidth = 427;
  constexpr int kHeight = 1;
  std::vector<uint16_t> buffer(3 * kWidth * kHeight);
  // The values in the buffer alternate between 0 and 1023.
  uint16_t value = 0;
  for (size_t i = 0; i < buffer.size(); ++i) {
    buffer[i] = value;
    value = 1023 - value;
  }
  unsigned char *img_data = reinterpret_cast<unsigned char *>(buffer.data());

  aom_image_t img;
  EXPECT_EQ(
      aom_img_wrap(&img, AOM_IMG_FMT_I44416, kWidth, kHeight, 1, img_data),
      &img);
  img.cp = AOM_CICP_CP_UNSPECIFIED;
  img.tc = AOM_CICP_TC_UNSPECIFIED;
  img.mc = AOM_CICP_MC_UNSPECIFIED;
  img.range = AOM_CR_FULL_RANGE;

  aom_codec_iface_t *iface = aom_codec_av1_cx();
  aom_codec_enc_cfg_t cfg;
  EXPECT_EQ(aom_codec_enc_config_default(iface, &cfg, AOM_USAGE_ALL_INTRA),
            AOM_CODEC_OK);
  cfg.rc_end_usage = AOM_Q;
  cfg.g_profile = 1;
  cfg.g_bit_depth = AOM_BITS_10;
  cfg.g_input_bit_depth = 10;
  cfg.g_w = kWidth;
  cfg.g_h = kHeight;
  cfg.g_limit = 1;
  cfg.g_lag_in_frames = 0;
  cfg.kf_mode = AOM_KF_DISABLED;
  cfg.kf_max_dist = 0;
  cfg.g_threads = 61;
  cfg.rc_min_quantizer = 2;
  cfg.rc_max_quantizer = 20;
  aom_codec_ctx_t enc;
  EXPECT_EQ(aom_codec_enc_init(&enc, iface, &cfg, AOM_CODEC_USE_HIGHBITDEPTH),
            AOM_CODEC_OK);
  EXPECT_EQ(aom_codec_control(&enc, AOME_SET_CQ_LEVEL, 11), AOM_CODEC_OK);
  EXPECT_EQ(aom_codec_control(&enc, AV1E_SET_ROW_MT, 1), AOM_CODEC_OK);
  EXPECT_EQ(aom_codec_control(&enc, AV1E_SET_TILE_ROWS, 4), AOM_CODEC_OK);
  EXPECT_EQ(aom_codec_control(&enc, AOME_SET_CPUUSED, 3), AOM_CODEC_OK);
  EXPECT_EQ(aom_codec_control(&enc, AV1E_SET_COLOR_RANGE, AOM_CR_FULL_RANGE),
            AOM_CODEC_OK);
  EXPECT_EQ(aom_codec_control(&enc, AV1E_SET_SKIP_POSTPROC_FILTERING, 1),
            AOM_CODEC_OK);
  EXPECT_EQ(aom_codec_control(&enc, AOME_SET_TUNING, AOM_TUNE_SSIM),
            AOM_CODEC_OK);

  // Encode frame
  EXPECT_EQ(aom_codec_encode(&enc, &img, 0, 1, 0), AOM_CODEC_OK);
  aom_codec_iter_t iter = nullptr;
  const aom_codec_cx_pkt_t *pkt = aom_codec_get_cx_data(&enc, &iter);
  ASSERT_NE(pkt, nullptr);
  EXPECT_EQ(pkt->kind, AOM_CODEC_CX_FRAME_PKT);
  // pkt->data.frame.flags is 0x1f0011.
  EXPECT_EQ(pkt->data.frame.flags & AOM_FRAME_IS_KEY, AOM_FRAME_IS_KEY);
  pkt = aom_codec_get_cx_data(&enc, &iter);
  EXPECT_EQ(pkt, nullptr);

  // Flush encoder
  EXPECT_EQ(aom_codec_encode(&enc, nullptr, 0, 1, 0), AOM_CODEC_OK);
  iter = nullptr;
  pkt = aom_codec_get_cx_data(&enc, &iter);
  EXPECT_EQ(pkt, nullptr);

  EXPECT_EQ(aom_codec_destroy(&enc), AOM_CODEC_OK);
}

// A test that reproduces b/272139363: signed integer overflow in
// update_b_sep_sym().
TEST(SearchWienerTest, 10bitSignedIntegerOverflowInUpdateBSepSym) {
  constexpr int kWidth = 34;
  constexpr int kHeight = 3;
  static const uint16_t buffer[3 * kWidth * kHeight] = {
    // Y plane:
    61, 765, 674, 188, 367, 944, 153, 275, 906, 433, 154, 51, 8, 855, 186, 154,
    392, 0, 634, 3, 690, 1023, 1023, 1023, 1023, 1023, 1023, 8, 1, 64, 426, 0,
    100, 344, 944, 816, 816, 33, 1023, 1023, 1023, 1023, 295, 1023, 1023, 1023,
    1023, 1023, 1023, 1015, 1023, 231, 1020, 254, 439, 439, 894, 439, 150, 1019,
    1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023, 385, 320, 575,
    682, 1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023, 511, 699, 987, 3, 140,
    661, 120, 33, 143, 0, 0, 0, 3, 40, 625, 585, 16, 579, 160, 867,
    // U plane:
    739, 646, 13, 603, 7, 328, 91, 32, 488, 870, 330, 330, 330, 330, 330, 330,
    109, 330, 330, 330, 3, 545, 945, 249, 35, 561, 801, 32, 931, 639, 801, 91,
    1023, 827, 844, 948, 631, 894, 854, 601, 432, 504, 85, 1, 0, 0, 89, 89, 0,
    0, 0, 0, 0, 0, 432, 801, 382, 4, 0, 0, 2, 89, 89, 89, 89, 89, 89, 384, 0, 0,
    0, 0, 0, 0, 0, 1023, 1019, 1, 3, 691, 575, 691, 691, 691, 691, 691, 691,
    691, 691, 691, 691, 691, 84, 527, 4, 485, 8, 682, 698, 340, 1015, 706,
    // V plane:
    49, 10, 28, 1023, 1023, 1023, 0, 32, 32, 872, 114, 1003, 1023, 57, 477, 999,
    1023, 309, 309, 309, 309, 309, 309, 309, 309, 309, 309, 309, 309, 309, 309,
    9, 418, 418, 418, 418, 418, 418, 0, 0, 0, 1023, 4, 5, 0, 0, 1023, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 64, 0, 155, 709, 3, 331, 807, 633, 1023,
    1018, 646, 886, 991, 692, 915, 294, 0, 35, 2, 0, 471, 643, 770, 346, 176,
    32, 329, 322, 302, 61, 765, 674, 188, 367, 944, 153, 275, 906, 433, 154
  };
  unsigned char *img_data =
      reinterpret_cast<unsigned char *>(const_cast<uint16_t *>(buffer));

  aom_image_t img;
  EXPECT_EQ(&img, aom_img_wrap(&img, AOM_IMG_FMT_I44416, kWidth, kHeight, 1,
                               img_data));
  img.cp = AOM_CICP_CP_UNSPECIFIED;
  img.tc = AOM_CICP_TC_UNSPECIFIED;
  img.mc = AOM_CICP_MC_UNSPECIFIED;
  img.range = AOM_CR_FULL_RANGE;

  aom_codec_iface_t *iface = aom_codec_av1_cx();
  aom_codec_enc_cfg_t cfg;
  EXPECT_EQ(AOM_CODEC_OK,
            aom_codec_enc_config_default(iface, &cfg, AOM_USAGE_ALL_INTRA));
  cfg.rc_end_usage = AOM_Q;
  cfg.g_profile = 1;
  cfg.g_bit_depth = AOM_BITS_10;
  cfg.g_input_bit_depth = 10;
  cfg.g_w = kWidth;
  cfg.g_h = kHeight;
  cfg.g_limit = 1;
  cfg.g_lag_in_frames = 0;
  cfg.kf_mode = AOM_KF_DISABLED;
  cfg.kf_max_dist = 0;
  cfg.rc_min_quantizer = 3;
  cfg.rc_max_quantizer = 54;
  aom_codec_ctx_t enc;
  EXPECT_EQ(AOM_CODEC_OK,
            aom_codec_enc_init(&enc, iface, &cfg, AOM_CODEC_USE_HIGHBITDEPTH));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_control(&enc, AOME_SET_CQ_LEVEL, 28));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_control(&enc, AV1E_SET_TILE_COLUMNS, 3));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_control(&enc, AOME_SET_CPUUSED, 0));
  EXPECT_EQ(AOM_CODEC_OK,
            aom_codec_control(&enc, AV1E_SET_COLOR_RANGE, AOM_CR_FULL_RANGE));
  EXPECT_EQ(AOM_CODEC_OK,
            aom_codec_control(&enc, AV1E_SET_SKIP_POSTPROC_FILTERING, 1));
  EXPECT_EQ(AOM_CODEC_OK,
            aom_codec_control(&enc, AOME_SET_TUNING, AOM_TUNE_SSIM));

  // Encode frame
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_encode(&enc, &img, 0, 1, 0));
  aom_codec_iter_t iter = nullptr;
  const aom_codec_cx_pkt_t *pkt = aom_codec_get_cx_data(&enc, &iter);
  ASSERT_NE(pkt, nullptr);
  EXPECT_EQ(pkt->kind, AOM_CODEC_CX_FRAME_PKT);
  // pkt->data.frame.flags is 0x1f0011.
  EXPECT_EQ(pkt->data.frame.flags & AOM_FRAME_IS_KEY, AOM_FRAME_IS_KEY);
  pkt = aom_codec_get_cx_data(&enc, &iter);
  EXPECT_EQ(pkt, nullptr);

  // Flush encoder
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_encode(&enc, nullptr, 0, 1, 0));
  iter = nullptr;
  pkt = aom_codec_get_cx_data(&enc, &iter);
  EXPECT_EQ(pkt, nullptr);

  EXPECT_EQ(AOM_CODEC_OK, aom_codec_destroy(&enc));
}

// A test that reproduces b/277121724: signed integer overflow in
// update_b_sep_sym().
TEST(SearchWienerTest, 8bitSignedIntegerOverflowInUpdateBSepSym) {
  constexpr int kWidth = 198;
  constexpr int kHeight = 3;
  // 8-bit YUV 4:2:2
  static const unsigned char buffer[2 * kWidth * kHeight] = {
    // Y plane:
    35, 225, 56, 91, 8, 142, 137, 143, 224, 49, 217, 57, 202, 163, 159, 246,
    232, 134, 135, 14, 76, 101, 239, 88, 186, 159, 118, 23, 114, 20, 108, 41,
    72, 17, 58, 242, 45, 146, 230, 14, 135, 140, 34, 61, 189, 181, 222, 71, 98,
    221, 5, 199, 244, 85, 229, 163, 105, 87, 144, 105, 64, 150, 36, 233, 235, 1,
    179, 190, 50, 222, 176, 109, 166, 18, 80, 129, 45, 9, 218, 144, 234, 10,
    148, 117, 37, 10, 232, 139, 206, 92, 208, 247, 128, 79, 202, 79, 212, 89,
    185, 152, 206, 182, 83, 105, 21, 86, 150, 84, 21, 165, 34, 251, 174, 240,
    172, 155, 254, 85, 98, 25, 96, 78, 230, 253, 36, 19, 247, 155, 112, 216,
    166, 114, 229, 118, 197, 149, 186, 194, 128, 45, 219, 26, 36, 77, 110, 45,
    252, 238, 183, 161, 171, 96, 232, 108, 73, 61, 243, 58, 155, 38, 91, 209,
    187, 206, 16, 165, 236, 145, 69, 126, 102, 10, 4, 43, 191, 106, 193, 240,
    132, 226, 38, 78, 7, 152, 101, 255, 254, 39, 33, 86, 35, 247, 199, 179, 239,
    198, 165, 58, 190, 171, 226, 94, 158, 21, 190, 151, 75, 176, 11, 53, 199,
    87, 91, 1, 226, 20, 117, 96, 75, 192, 101, 200, 125, 106, 233, 176, 63, 204,
    114, 16, 31, 222, 15, 14, 71, 2, 25, 47, 100, 174, 26, 209, 138, 138, 211,
    147, 164, 204, 9, 104, 135, 250, 9, 201, 88, 218, 71, 251, 61, 199, 0, 34,
    59, 115, 228, 161, 100, 132, 50, 4, 117, 100, 191, 126, 53, 28, 193, 42,
    155, 206, 79, 80, 117, 11, 3, 253, 181, 181, 138, 239, 107, 142, 216, 57,
    202, 126, 229, 250, 60, 62, 150, 128, 95, 32, 251, 207, 236, 208, 247, 183,
    59, 19, 117, 40, 106, 87, 140, 57, 109, 190, 51, 105, 226, 116, 156, 3, 35,
    86, 255, 138, 52, 211, 245, 76, 83, 109, 113, 77, 106, 77, 18, 56, 235, 158,
    24, 53, 151, 104, 152, 21, 15, 46, 163, 144, 217, 168, 154, 44, 80, 25, 11,
    37, 100, 235, 145, 154, 113, 0, 140, 153, 80, 64, 19, 121, 185, 144, 43,
    206, 16, 16, 72, 189, 175, 231, 177, 40, 177, 206, 116, 4, 82, 43, 244, 237,
    22, 252, 71, 194, 106, 4, 112, 0, 108, 137, 126, 80, 122, 142, 43, 205, 22,
    209, 217, 165, 32, 208, 100, 70, 3, 120, 159, 203, 7, 233, 152, 37, 96, 212,
    177, 1, 133, 218, 161, 172, 202, 192, 186, 114, 150, 121, 177, 227, 175, 64,
    127, 153, 113, 91, 198, 0, 111, 227, 226, 218, 71, 62, 5, 43, 128, 27, 3,
    82, 5, 10, 68, 153, 215, 181, 138, 246, 224, 170, 1, 241, 191, 181, 151,
    167, 14, 80, 45, 4, 252, 29, 66, 125, 58, 225, 253, 255, 248, 224, 40, 24,
    236, 46, 11, 219, 154, 134, 12, 76, 72, 97, 239, 50, 39, 85, 182, 55, 219,
    19, 109, 81, 119, 125, 206, 159, 239, 67, 193, 180, 132, 80, 127, 2, 169,
    99, 53, 47, 5, 100, 174, 151, 124, 246, 202, 93, 82, 65, 53, 214, 238, 32,
    218, 15, 254, 153, 95, 79, 189, 67, 233, 47, 83, 48, 125, 144, 206, 82, 69,
    186, 112, 134, 244, 96, 21, 143, 187, 248, 8, 224, 161, 227, 185, 236, 6,
    175, 237, 169, 154, 89, 143, 106, 205, 26, 47, 155, 42, 28, 162, 7, 8, 45,
    // U plane:
    55, 165, 203, 139, 152, 208, 36, 177, 61, 49, 129, 211, 140, 71, 253, 250,
    120, 167, 238, 67, 255, 223, 104, 32, 240, 179, 28, 41, 86, 84, 61, 243,
    169, 212, 201, 0, 9, 236, 89, 194, 204, 75, 228, 250, 27, 81, 137, 29, 255,
    131, 194, 241, 76, 133, 186, 135, 212, 197, 150, 145, 203, 96, 86, 231, 91,
    119, 197, 67, 226, 2, 118, 66, 181, 86, 219, 86, 132, 137, 156, 161, 221,
    18, 55, 170, 35, 206, 201, 193, 38, 63, 229, 29, 110, 96, 14, 135, 229, 99,
    106, 108, 167, 110, 50, 32, 144, 113, 48, 29, 57, 29, 20, 199, 145, 245, 9,
    183, 88, 174, 114, 237, 29, 40, 99, 117, 233, 6, 51, 227, 2, 28, 76, 149,
    190, 23, 240, 73, 113, 10, 73, 240, 105, 220, 129, 26, 144, 214, 34, 4, 24,
    219, 24, 156, 198, 214, 244, 143, 106, 255, 204, 93, 2, 88, 107, 211, 241,
    242, 86, 189, 219, 164, 132, 149, 32, 228, 219, 60, 202, 218, 189, 34, 250,
    160, 158, 36, 212, 212, 41, 233, 61, 92, 121, 170, 220, 192, 232, 255, 124,
    249, 231, 55, 196, 219, 196, 62, 238, 187, 76, 33, 138, 67, 82, 159, 169,
    196, 66, 196, 110, 194, 64, 35, 205, 64, 218, 12, 41, 188, 195, 244, 178,
    17, 80, 8, 149, 39, 110, 146, 164, 162, 215, 227, 107, 103, 47, 52, 95, 3,
    181, 90, 255, 80, 83, 206, 66, 153, 112, 72, 109, 235, 69, 105, 57, 75, 145,
    186, 16, 87, 73, 61, 98, 197, 237, 17, 32, 207, 220, 246, 188, 46, 73, 121,
    84, 252, 164, 111, 21, 98, 13, 170, 174, 170, 231, 77, 10, 113, 9, 217, 11,
    // V plane:
    124, 94, 69, 212, 107, 223, 228, 96, 56, 2, 158, 49, 251, 217, 143, 107,
    113, 17, 84, 169, 208, 43, 28, 37, 176, 54, 235, 150, 135, 135, 221, 94, 50,
    131, 251, 78, 38, 254, 129, 200, 207, 55, 111, 110, 144, 109, 228, 65, 70,
    39, 170, 5, 208, 151, 87, 86, 255, 74, 155, 153, 250, 15, 35, 33, 201, 226,
    117, 119, 220, 238, 133, 229, 69, 122, 160, 114, 245, 182, 13, 65, 2, 228,
    205, 174, 128, 248, 4, 139, 178, 227, 204, 243, 249, 253, 119, 253, 107,
    234, 39, 15, 173, 47, 93, 12, 222, 238, 30, 121, 124, 167, 27, 40, 215, 84,
    172, 130, 66, 43, 165, 55, 225, 79, 84, 153, 59, 110, 64, 176, 54, 123, 82,
    128, 189, 150, 52, 202, 102, 133, 199, 197, 253, 180, 221, 127, 144, 124,
    255, 224, 52, 149, 88, 166, 39, 38, 78, 114, 44, 242, 233, 40, 132, 142,
    152, 213, 112, 244, 221, 7, 52, 206, 246, 51, 182, 160, 247, 154, 183, 209,
    81, 70, 56, 186, 63, 182, 2, 82, 202, 178, 233, 52, 198, 241, 175, 38, 165,
    9, 231, 150, 114, 43, 159, 200, 42, 173, 217, 25, 233, 214, 210, 50, 43,
    159, 231, 102, 241, 246, 77, 76, 115, 77, 81, 114, 194, 182, 236, 0, 236,
    198, 197, 180, 176, 148, 48, 177, 106, 180, 150, 158, 237, 130, 242, 109,
    174, 247, 57, 230, 184, 64, 245, 251, 123, 169, 122, 156, 125, 123, 104,
    238, 1, 235, 187, 53, 67, 38, 50, 139, 123, 149, 111, 72, 80, 17, 175, 186,
    98, 153, 247, 97, 218, 141, 38, 0, 171, 254, 180, 81, 233, 71, 156, 48, 14,
    62, 210, 161, 124, 203, 92
  };
  unsigned char *img_data = const_cast<unsigned char *>(buffer);

  aom_image_t img;
  EXPECT_EQ(aom_img_wrap(&img, AOM_IMG_FMT_I422, kWidth, kHeight, 1, img_data),
            &img);
  img.cp = AOM_CICP_CP_UNSPECIFIED;
  img.tc = AOM_CICP_TC_UNSPECIFIED;
  img.mc = AOM_CICP_MC_UNSPECIFIED;
  img.range = AOM_CR_FULL_RANGE;

  aom_codec_iface_t *iface = aom_codec_av1_cx();
  aom_codec_enc_cfg_t cfg;
  EXPECT_EQ(aom_codec_enc_config_default(iface, &cfg, AOM_USAGE_ALL_INTRA),
            AOM_CODEC_OK);
  cfg.rc_end_usage = AOM_Q;
  cfg.g_profile = 2;
  cfg.g_bit_depth = AOM_BITS_8;
  cfg.g_input_bit_depth = 8;
  cfg.g_w = kWidth;
  cfg.g_h = kHeight;
  cfg.g_limit = 1;
  cfg.g_lag_in_frames = 0;
  cfg.kf_mode = AOM_KF_DISABLED;
  cfg.kf_max_dist = 0;
  cfg.g_threads = 43;
  cfg.rc_min_quantizer = 30;
  cfg.rc_max_quantizer = 50;
  aom_codec_ctx_t enc;
  EXPECT_EQ(aom_codec_enc_init(&enc, iface, &cfg, 0), AOM_CODEC_OK);
  EXPECT_EQ(aom_codec_control(&enc, AOME_SET_CQ_LEVEL, 40), AOM_CODEC_OK);
  EXPECT_EQ(aom_codec_control(&enc, AV1E_SET_ROW_MT, 1), AOM_CODEC_OK);
  EXPECT_EQ(aom_codec_control(&enc, AV1E_SET_TILE_ROWS, 4), AOM_CODEC_OK);
  EXPECT_EQ(aom_codec_control(&enc, AV1E_SET_TILE_COLUMNS, 1), AOM_CODEC_OK);
  EXPECT_EQ(aom_codec_control(&enc, AOME_SET_CPUUSED, 2), AOM_CODEC_OK);
  EXPECT_EQ(aom_codec_control(&enc, AV1E_SET_COLOR_RANGE, AOM_CR_FULL_RANGE),
            AOM_CODEC_OK);
  EXPECT_EQ(aom_codec_control(&enc, AV1E_SET_SKIP_POSTPROC_FILTERING, 1),
            AOM_CODEC_OK);
  EXPECT_EQ(aom_codec_control(&enc, AOME_SET_TUNING, AOM_TUNE_SSIM),
            AOM_CODEC_OK);

  // Encode frame
  EXPECT_EQ(aom_codec_encode(&enc, &img, 0, 1, 0), AOM_CODEC_OK);
  aom_codec_iter_t iter = nullptr;
  const aom_codec_cx_pkt_t *pkt = aom_codec_get_cx_data(&enc, &iter);
  ASSERT_NE(pkt, nullptr);
  EXPECT_EQ(pkt->kind, AOM_CODEC_CX_FRAME_PKT);
  // pkt->data.frame.flags is 0x1f0011.
  EXPECT_EQ(pkt->data.frame.flags & AOM_FRAME_IS_KEY, AOM_FRAME_IS_KEY);
  pkt = aom_codec_get_cx_data(&enc, &iter);
  EXPECT_EQ(pkt, nullptr);

  // Flush encoder
  EXPECT_EQ(aom_codec_encode(&enc, nullptr, 0, 1, 0), AOM_CODEC_OK);
  iter = nullptr;
  pkt = aom_codec_get_cx_data(&enc, &iter);
  EXPECT_EQ(pkt, nullptr);

  EXPECT_EQ(aom_codec_destroy(&enc), AOM_CODEC_OK);
}

// A test that reproduces b/259173819: signed integer overflow in
// linsolve_wiener().
TEST(SearchWienerTest, 10bitSignedIntegerOverflowInLinsolveWiener) {
  constexpr int kWidth = 3;
  constexpr int kHeight = 3;
  static const uint16_t buffer[3 * kWidth * kHeight] = {
    // Y plane:
    81, 81, 1023, 1020, 81, 1023, 81, 128, 0,
    // U plane:
    273, 273, 273, 273, 273, 273, 273, 273, 273,
    // V plane:
    273, 273, 273, 273, 273, 273, 516, 81, 81
  };
  unsigned char *img_data =
      reinterpret_cast<unsigned char *>(const_cast<uint16_t *>(buffer));

  aom_image_t img;
  EXPECT_EQ(
      aom_img_wrap(&img, AOM_IMG_FMT_I44416, kWidth, kHeight, 1, img_data),
      &img);
  img.cp = AOM_CICP_CP_UNSPECIFIED;
  img.tc = AOM_CICP_TC_UNSPECIFIED;
  img.mc = AOM_CICP_MC_UNSPECIFIED;
  img.range = AOM_CR_FULL_RANGE;

  aom_codec_iface_t *iface = aom_codec_av1_cx();
  aom_codec_enc_cfg_t cfg;
  EXPECT_EQ(aom_codec_enc_config_default(iface, &cfg, AOM_USAGE_ALL_INTRA),
            AOM_CODEC_OK);
  cfg.rc_end_usage = AOM_Q;
  cfg.g_profile = 1;
  cfg.g_bit_depth = AOM_BITS_10;
  cfg.g_input_bit_depth = 10;
  cfg.g_w = kWidth;
  cfg.g_h = kHeight;
  cfg.g_limit = 1;
  cfg.g_lag_in_frames = 0;
  cfg.kf_mode = AOM_KF_DISABLED;
  cfg.kf_max_dist = 0;
  cfg.g_threads = 21;
  cfg.rc_min_quantizer = 16;
  cfg.rc_max_quantizer = 54;
  aom_codec_ctx_t enc;
  EXPECT_EQ(aom_codec_enc_init(&enc, iface, &cfg, AOM_CODEC_USE_HIGHBITDEPTH),
            AOM_CODEC_OK);
  EXPECT_EQ(aom_codec_control(&enc, AOME_SET_CQ_LEVEL, 35), AOM_CODEC_OK);
  EXPECT_EQ(aom_codec_control(&enc, AV1E_SET_ROW_MT, 1), AOM_CODEC_OK);
  EXPECT_EQ(aom_codec_control(&enc, AV1E_SET_TILE_ROWS, 2), AOM_CODEC_OK);
  EXPECT_EQ(aom_codec_control(&enc, AV1E_SET_TILE_COLUMNS, 5), AOM_CODEC_OK);
  EXPECT_EQ(aom_codec_control(&enc, AOME_SET_CPUUSED, 1), AOM_CODEC_OK);
  EXPECT_EQ(aom_codec_control(&enc, AV1E_SET_COLOR_RANGE, AOM_CR_FULL_RANGE),
            AOM_CODEC_OK);
  EXPECT_EQ(aom_codec_control(&enc, AV1E_SET_SKIP_POSTPROC_FILTERING, 1),
            AOM_CODEC_OK);
  EXPECT_EQ(aom_codec_control(&enc, AOME_SET_TUNING, AOM_TUNE_SSIM),
            AOM_CODEC_OK);

  // Encode frame
  EXPECT_EQ(aom_codec_encode(&enc, &img, 0, 1, 0), AOM_CODEC_OK);
  aom_codec_iter_t iter = nullptr;
  const aom_codec_cx_pkt_t *pkt = aom_codec_get_cx_data(&enc, &iter);
  ASSERT_NE(pkt, nullptr);
  EXPECT_EQ(pkt->kind, AOM_CODEC_CX_FRAME_PKT);
  // pkt->data.frame.flags is 0x1f0011.
  EXPECT_EQ(pkt->data.frame.flags & AOM_FRAME_IS_KEY, AOM_FRAME_IS_KEY);
  pkt = aom_codec_get_cx_data(&enc, &iter);
  EXPECT_EQ(pkt, nullptr);

  // Flush encoder
  EXPECT_EQ(aom_codec_encode(&enc, nullptr, 0, 1, 0), AOM_CODEC_OK);
  iter = nullptr;
  pkt = aom_codec_get_cx_data(&enc, &iter);
  EXPECT_EQ(pkt, nullptr);

  EXPECT_EQ(aom_codec_destroy(&enc), AOM_CODEC_OK);
}

}  // namespace wiener_highbd
#endif  // CONFIG_AV1_HIGHBITDEPTH
