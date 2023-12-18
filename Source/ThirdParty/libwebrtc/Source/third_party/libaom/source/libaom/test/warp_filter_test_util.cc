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
#include <memory>
#include <new>

#include "aom_ports/aom_timer.h"
#include "test/warp_filter_test_util.h"

using std::make_tuple;
using std::tuple;

namespace libaom_test {

int32_t random_warped_param(libaom_test::ACMRandom *rnd, int bits,
                            int rnd_gen_zeros) {
  // Avoid accidentally generating a zero in speed tests, they are set by the
  // is_*_zero parameters instead.
  if (rnd_gen_zeros) {
    // 1 in 8 chance of generating zero (arbitrarily chosen)
    if (((rnd->Rand8()) & 7) == 0) return 0;
  }
  // Otherwise, enerate uniform values in the range
  // [-(1 << bits), 1] U [1, 1<<bits]
  int32_t v = 1 + (rnd->Rand16() & ((1 << bits) - 1));
  if ((rnd->Rand8()) & 1) return -v;
  return v;
}

void generate_warped_model(libaom_test::ACMRandom *rnd, int32_t *mat,
                           int16_t *alpha, int16_t *beta, int16_t *gamma,
                           int16_t *delta, const int is_alpha_zero,
                           const int is_beta_zero, const int is_gamma_zero,
                           const int is_delta_zero, const int rnd_gen_zeros) {
  while (true) {
    int rnd8 = rnd->Rand8() & 3;
    mat[0] = random_warped_param(rnd, WARPEDMODEL_PREC_BITS + 6, rnd_gen_zeros);
    mat[1] = random_warped_param(rnd, WARPEDMODEL_PREC_BITS + 6, rnd_gen_zeros);
    mat[2] =
        (random_warped_param(rnd, WARPEDMODEL_PREC_BITS - 3, rnd_gen_zeros)) +
        (1 << WARPEDMODEL_PREC_BITS);
    mat[3] = random_warped_param(rnd, WARPEDMODEL_PREC_BITS - 3, rnd_gen_zeros);

    if (rnd8 <= 1) {
      // AFFINE
      mat[4] =
          random_warped_param(rnd, WARPEDMODEL_PREC_BITS - 3, rnd_gen_zeros);
      mat[5] =
          (random_warped_param(rnd, WARPEDMODEL_PREC_BITS - 3, rnd_gen_zeros)) +
          (1 << WARPEDMODEL_PREC_BITS);
    } else if (rnd8 == 2) {
      mat[4] = -mat[3];
      mat[5] = mat[2];
    } else {
      mat[4] =
          random_warped_param(rnd, WARPEDMODEL_PREC_BITS - 3, rnd_gen_zeros);
      mat[5] =
          (random_warped_param(rnd, WARPEDMODEL_PREC_BITS - 3, rnd_gen_zeros)) +
          (1 << WARPEDMODEL_PREC_BITS);
    }

    if (is_alpha_zero == 1) {
      mat[2] = 1 << WARPEDMODEL_PREC_BITS;
    }
    if (is_beta_zero == 1) {
      mat[3] = 0;
    }
    if (is_gamma_zero == 1) {
      mat[4] = 0;
    }
    if (is_delta_zero == 1) {
      mat[5] = static_cast<int32_t>(
          ((static_cast<int64_t>(mat[3]) * mat[4] + (mat[2] / 2)) / mat[2]) +
          (1 << WARPEDMODEL_PREC_BITS));
    }

    // Calculate the derived parameters and check that they are suitable
    // for the warp filter.
    assert(mat[2] != 0);

    *alpha = clamp(mat[2] - (1 << WARPEDMODEL_PREC_BITS), INT16_MIN, INT16_MAX);
    *beta = clamp(mat[3], INT16_MIN, INT16_MAX);
    *gamma = static_cast<int16_t>(clamp64(
        (static_cast<int64_t>(mat[4]) * (1 << WARPEDMODEL_PREC_BITS)) / mat[2],
        INT16_MIN, INT16_MAX));
    *delta = static_cast<int16_t>(clamp64(
        mat[5] -
            ((static_cast<int64_t>(mat[3]) * mat[4] + (mat[2] / 2)) / mat[2]) -
            (1 << WARPEDMODEL_PREC_BITS),
        INT16_MIN, INT16_MAX));

    if ((4 * abs(*alpha) + 7 * abs(*beta) >= (1 << WARPEDMODEL_PREC_BITS)) ||
        (4 * abs(*gamma) + 4 * abs(*delta) >= (1 << WARPEDMODEL_PREC_BITS)))
      continue;

    *alpha = ROUND_POWER_OF_TWO_SIGNED(*alpha, WARP_PARAM_REDUCE_BITS) *
             (1 << WARP_PARAM_REDUCE_BITS);
    *beta = ROUND_POWER_OF_TWO_SIGNED(*beta, WARP_PARAM_REDUCE_BITS) *
            (1 << WARP_PARAM_REDUCE_BITS);
    *gamma = ROUND_POWER_OF_TWO_SIGNED(*gamma, WARP_PARAM_REDUCE_BITS) *
             (1 << WARP_PARAM_REDUCE_BITS);
    *delta = ROUND_POWER_OF_TWO_SIGNED(*delta, WARP_PARAM_REDUCE_BITS) *
             (1 << WARP_PARAM_REDUCE_BITS);

    // We have a valid model, so finish
    return;
  }
}

namespace AV1WarpFilter {
::testing::internal::ParamGenerator<WarpTestParams> BuildParams(
    warp_affine_func filter) {
  WarpTestParam params[] = {
    make_tuple(4, 4, 5000, filter),  make_tuple(8, 8, 5000, filter),
    make_tuple(64, 64, 100, filter), make_tuple(4, 16, 2000, filter),
    make_tuple(32, 8, 1000, filter),
  };
  return ::testing::Combine(::testing::ValuesIn(params),
                            ::testing::Values(0, 1), ::testing::Values(0, 1),
                            ::testing::Values(0, 1), ::testing::Values(0, 1));
}

AV1WarpFilterTest::~AV1WarpFilterTest() = default;
void AV1WarpFilterTest::SetUp() { rnd_.Reset(ACMRandom::DeterministicSeed()); }

void AV1WarpFilterTest::RunSpeedTest(warp_affine_func test_impl) {
  const int w = 128, h = 128;
  const int border = 16;
  const int stride = w + 2 * border;
  WarpTestParam params = GET_PARAM(0);
  const int out_w = std::get<0>(params), out_h = std::get<1>(params);
  const int is_alpha_zero = GET_PARAM(1);
  const int is_beta_zero = GET_PARAM(2);
  const int is_gamma_zero = GET_PARAM(3);
  const int is_delta_zero = GET_PARAM(4);
  int sub_x, sub_y;
  const int bd = 8;

  std::unique_ptr<uint8_t[]> input_(new (std::nothrow) uint8_t[h * stride]);
  ASSERT_NE(input_, nullptr);
  uint8_t *input = input_.get() + border;

  // The warp functions always write rows with widths that are multiples of 8.
  // So to avoid a buffer overflow, we may need to pad rows to a multiple of 8.
  int output_n = ((out_w + 7) & ~7) * out_h;
  std::unique_ptr<uint8_t[]> output(new (std::nothrow) uint8_t[output_n]);
  ASSERT_NE(output, nullptr);
  int32_t mat[8];
  int16_t alpha, beta, gamma, delta;
  ConvolveParams conv_params = get_conv_params(0, 0, bd);
  std::unique_ptr<CONV_BUF_TYPE[]> dsta(new (std::nothrow)
                                            CONV_BUF_TYPE[output_n]);
  ASSERT_NE(dsta, nullptr);
  generate_warped_model(&rnd_, mat, &alpha, &beta, &gamma, &delta,
                        is_alpha_zero, is_beta_zero, is_gamma_zero,
                        is_delta_zero, 0);

  for (int r = 0; r < h; ++r)
    for (int c = 0; c < w; ++c) input[r * stride + c] = rnd_.Rand8();
  for (int r = 0; r < h; ++r) {
    memset(input + r * stride - border, input[r * stride], border);
    memset(input + r * stride + w, input[r * stride + (w - 1)], border);
  }

  sub_x = 0;
  sub_y = 0;
  int do_average = 0;

  conv_params =
      get_conv_params_no_round(do_average, 0, dsta.get(), out_w, 1, bd);
  conv_params.use_dist_wtd_comp_avg = 0;

  const int num_loops = 1000000000 / (out_w + out_h);
  aom_usec_timer timer;
  aom_usec_timer_start(&timer);
  for (int i = 0; i < num_loops; ++i)
    test_impl(mat, input, w, h, stride, output.get(), 32, 32, out_w, out_h,
              out_w, sub_x, sub_y, &conv_params, alpha, beta, gamma, delta);

  aom_usec_timer_mark(&timer);
  const int elapsed_time = static_cast<int>(aom_usec_timer_elapsed(&timer));
  printf("warp %3dx%-3d alpha=%d beta=%d gamma=%d delta=%d: %7.2f ns \n", out_w,
         out_h, alpha, beta, gamma, delta, 1000.0 * elapsed_time / num_loops);
}

void AV1WarpFilterTest::RunCheckOutput(warp_affine_func test_impl) {
  const int w = 128, h = 128;
  const int border = 16;
  const int stride = w + 2 * border;
  WarpTestParam params = GET_PARAM(0);
  const int is_alpha_zero = GET_PARAM(1);
  const int is_beta_zero = GET_PARAM(2);
  const int is_gamma_zero = GET_PARAM(3);
  const int is_delta_zero = GET_PARAM(4);
  const int out_w = std::get<0>(params), out_h = std::get<1>(params);
  const int num_iters = std::get<2>(params);
  const int bd = 8;

  // The warp functions always write rows with widths that are multiples of 8.
  // So to avoid a buffer overflow, we may need to pad rows to a multiple of 8.
  int output_n = ((out_w + 7) & ~7) * out_h;
  std::unique_ptr<uint8_t[]> input_(new (std::nothrow) uint8_t[h * stride]);
  ASSERT_NE(input_, nullptr);
  uint8_t *input = input_.get() + border;
  std::unique_ptr<uint8_t[]> output(new (std::nothrow) uint8_t[output_n]);
  ASSERT_NE(output, nullptr);
  std::unique_ptr<uint8_t[]> output2(new (std::nothrow) uint8_t[output_n]);
  ASSERT_NE(output2, nullptr);
  int32_t mat[8];
  int16_t alpha, beta, gamma, delta;
  ConvolveParams conv_params = get_conv_params(0, 0, bd);
  std::unique_ptr<CONV_BUF_TYPE[]> dsta(new (std::nothrow)
                                            CONV_BUF_TYPE[output_n]);
  ASSERT_NE(dsta, nullptr);
  std::unique_ptr<CONV_BUF_TYPE[]> dstb(new (std::nothrow)
                                            CONV_BUF_TYPE[output_n]);
  ASSERT_NE(dstb, nullptr);
  for (int i = 0; i < output_n; ++i) output[i] = output2[i] = rnd_.Rand8();

  for (int i = 0; i < num_iters; ++i) {
    // Generate an input block and extend its borders horizontally
    for (int r = 0; r < h; ++r)
      for (int c = 0; c < w; ++c) input[r * stride + c] = rnd_.Rand8();
    for (int r = 0; r < h; ++r) {
      memset(input + r * stride - border, input[r * stride], border);
      memset(input + r * stride + w, input[r * stride + (w - 1)], border);
    }
    const int use_no_round = rnd_.Rand8() & 1;
    for (int sub_x = 0; sub_x < 2; ++sub_x)
      for (int sub_y = 0; sub_y < 2; ++sub_y) {
        generate_warped_model(&rnd_, mat, &alpha, &beta, &gamma, &delta,
                              is_alpha_zero, is_beta_zero, is_gamma_zero,
                              is_delta_zero, 1);

        for (int ii = 0; ii < 2; ++ii) {
          for (int jj = 0; jj < 5; ++jj) {
            for (int do_average = 0; do_average <= 1; ++do_average) {
              if (use_no_round) {
                conv_params = get_conv_params_no_round(
                    do_average, 0, dsta.get(), out_w, 1, bd);
              } else {
                conv_params = get_conv_params(0, 0, bd);
              }
              if (jj >= 4) {
                conv_params.use_dist_wtd_comp_avg = 0;
              } else {
                conv_params.use_dist_wtd_comp_avg = 1;
                conv_params.fwd_offset = quant_dist_lookup_table[jj][ii];
                conv_params.bck_offset = quant_dist_lookup_table[jj][1 - ii];
              }
              av1_warp_affine_c(mat, input, w, h, stride, output.get(), 32, 32,
                                out_w, out_h, out_w, sub_x, sub_y, &conv_params,
                                alpha, beta, gamma, delta);
              if (use_no_round) {
                conv_params = get_conv_params_no_round(
                    do_average, 0, dstb.get(), out_w, 1, bd);
              }
              if (jj >= 4) {
                conv_params.use_dist_wtd_comp_avg = 0;
              } else {
                conv_params.use_dist_wtd_comp_avg = 1;
                conv_params.fwd_offset = quant_dist_lookup_table[jj][ii];
                conv_params.bck_offset = quant_dist_lookup_table[jj][1 - ii];
              }
              test_impl(mat, input, w, h, stride, output2.get(), 32, 32, out_w,
                        out_h, out_w, sub_x, sub_y, &conv_params, alpha, beta,
                        gamma, delta);
              if (use_no_round) {
                for (int j = 0; j < out_w * out_h; ++j)
                  ASSERT_EQ(dsta[j], dstb[j])
                      << "Pixel mismatch at index " << j << " = ("
                      << (j % out_w) << ", " << (j / out_w) << ") on iteration "
                      << i;
                for (int j = 0; j < out_w * out_h; ++j)
                  ASSERT_EQ(output[j], output2[j])
                      << "Pixel mismatch at index " << j << " = ("
                      << (j % out_w) << ", " << (j / out_w) << ") on iteration "
                      << i;
              } else {
                for (int j = 0; j < out_w * out_h; ++j)
                  ASSERT_EQ(output[j], output2[j])
                      << "Pixel mismatch at index " << j << " = ("
                      << (j % out_w) << ", " << (j / out_w) << ") on iteration "
                      << i;
              }
            }
          }
        }
      }
  }
}
}  // namespace AV1WarpFilter

#if CONFIG_AV1_HIGHBITDEPTH
namespace AV1HighbdWarpFilter {
::testing::internal::ParamGenerator<HighbdWarpTestParams> BuildParams(
    highbd_warp_affine_func filter) {
  const HighbdWarpTestParam params[] = {
    make_tuple(4, 4, 100, 8, filter),    make_tuple(8, 8, 100, 8, filter),
    make_tuple(64, 64, 100, 8, filter),  make_tuple(4, 16, 100, 8, filter),
    make_tuple(32, 8, 100, 8, filter),   make_tuple(4, 4, 100, 10, filter),
    make_tuple(8, 8, 100, 10, filter),   make_tuple(64, 64, 100, 10, filter),
    make_tuple(4, 16, 100, 10, filter),  make_tuple(32, 8, 100, 10, filter),
    make_tuple(4, 4, 100, 12, filter),   make_tuple(8, 8, 100, 12, filter),
    make_tuple(64, 64, 100, 12, filter), make_tuple(4, 16, 100, 12, filter),
    make_tuple(32, 8, 100, 12, filter),
  };
  return ::testing::Combine(::testing::ValuesIn(params),
                            ::testing::Values(0, 1), ::testing::Values(0, 1),
                            ::testing::Values(0, 1), ::testing::Values(0, 1));
}

AV1HighbdWarpFilterTest::~AV1HighbdWarpFilterTest() = default;
void AV1HighbdWarpFilterTest::SetUp() {
  rnd_.Reset(ACMRandom::DeterministicSeed());
}

void AV1HighbdWarpFilterTest::RunSpeedTest(highbd_warp_affine_func test_impl) {
  const int w = 128, h = 128;
  const int border = 16;
  const int stride = w + 2 * border;
  HighbdWarpTestParam param = GET_PARAM(0);
  const int is_alpha_zero = GET_PARAM(1);
  const int is_beta_zero = GET_PARAM(2);
  const int is_gamma_zero = GET_PARAM(3);
  const int is_delta_zero = GET_PARAM(4);
  const int out_w = std::get<0>(param), out_h = std::get<1>(param);
  const int bd = std::get<3>(param);
  const int mask = (1 << bd) - 1;
  int sub_x, sub_y;

  // The warp functions always write rows with widths that are multiples of 8.
  // So to avoid a buffer overflow, we may need to pad rows to a multiple of 8.
  int output_n = ((out_w + 7) & ~7) * out_h;
  std::unique_ptr<uint16_t[]> input_(new (std::nothrow) uint16_t[h * stride]);
  ASSERT_NE(input_, nullptr);
  uint16_t *input = input_.get() + border;
  std::unique_ptr<uint16_t[]> output(new (std::nothrow) uint16_t[output_n]);
  ASSERT_NE(output, nullptr);
  int32_t mat[8];
  int16_t alpha, beta, gamma, delta;
  ConvolveParams conv_params = get_conv_params(0, 0, bd);
  std::unique_ptr<CONV_BUF_TYPE[]> dsta(new (std::nothrow)
                                            CONV_BUF_TYPE[output_n]);
  ASSERT_NE(dsta, nullptr);

  generate_warped_model(&rnd_, mat, &alpha, &beta, &gamma, &delta,
                        is_alpha_zero, is_beta_zero, is_gamma_zero,
                        is_delta_zero, 0);
  // Generate an input block and extend its borders horizontally
  for (int r = 0; r < h; ++r)
    for (int c = 0; c < w; ++c) input[r * stride + c] = rnd_.Rand16() & mask;
  for (int r = 0; r < h; ++r) {
    for (int c = 0; c < border; ++c) {
      input[r * stride - border + c] = input[r * stride];
      input[r * stride + w + c] = input[r * stride + (w - 1)];
    }
  }

  sub_x = 0;
  sub_y = 0;
  int do_average = 0;
  conv_params.use_dist_wtd_comp_avg = 0;
  conv_params =
      get_conv_params_no_round(do_average, 0, dsta.get(), out_w, 1, bd);

  const int num_loops = 1000000000 / (out_w + out_h);
  aom_usec_timer timer;
  aom_usec_timer_start(&timer);

  for (int i = 0; i < num_loops; ++i)
    test_impl(mat, input, w, h, stride, output.get(), 32, 32, out_w, out_h,
              out_w, sub_x, sub_y, bd, &conv_params, alpha, beta, gamma, delta);

  aom_usec_timer_mark(&timer);
  const int elapsed_time = static_cast<int>(aom_usec_timer_elapsed(&timer));
  printf("highbd warp %3dx%-3d alpha=%d beta=%d gamma=%d delta=%d: %7.2f ns \n",
         out_w, out_h, alpha, beta, gamma, delta,
         1000.0 * elapsed_time / num_loops);
}

void AV1HighbdWarpFilterTest::RunCheckOutput(
    highbd_warp_affine_func test_impl) {
  const int w = 128, h = 128;
  const int border = 16;
  const int stride = w + 2 * border;
  HighbdWarpTestParam param = GET_PARAM(0);
  const int is_alpha_zero = GET_PARAM(1);
  const int is_beta_zero = GET_PARAM(2);
  const int is_gamma_zero = GET_PARAM(3);
  const int is_delta_zero = GET_PARAM(4);
  const int out_w = std::get<0>(param), out_h = std::get<1>(param);
  const int bd = std::get<3>(param);
  const int num_iters = std::get<2>(param);
  const int mask = (1 << bd) - 1;

  // The warp functions always write rows with widths that are multiples of 8.
  // So to avoid a buffer overflow, we may need to pad rows to a multiple of 8.
  int output_n = ((out_w + 7) & ~7) * out_h;
  std::unique_ptr<uint16_t[]> input_(new (std::nothrow) uint16_t[h * stride]);
  ASSERT_NE(input_, nullptr);
  uint16_t *input = input_.get() + border;
  std::unique_ptr<uint16_t[]> output(new (std::nothrow) uint16_t[output_n]);
  ASSERT_NE(output, nullptr);
  std::unique_ptr<uint16_t[]> output2(new (std::nothrow) uint16_t[output_n]);
  ASSERT_NE(output2, nullptr);
  int32_t mat[8];
  int16_t alpha, beta, gamma, delta;
  ConvolveParams conv_params = get_conv_params(0, 0, bd);
  std::unique_ptr<CONV_BUF_TYPE[]> dsta(new (std::nothrow)
                                            CONV_BUF_TYPE[output_n]);
  ASSERT_NE(dsta, nullptr);
  std::unique_ptr<CONV_BUF_TYPE[]> dstb(new (std::nothrow)
                                            CONV_BUF_TYPE[output_n]);
  ASSERT_NE(dstb, nullptr);
  for (int i = 0; i < output_n; ++i) output[i] = output2[i] = rnd_.Rand16();

  for (int i = 0; i < num_iters; ++i) {
    // Generate an input block and extend its borders horizontally
    for (int r = 0; r < h; ++r)
      for (int c = 0; c < w; ++c) input[r * stride + c] = rnd_.Rand16() & mask;
    for (int r = 0; r < h; ++r) {
      for (int c = 0; c < border; ++c) {
        input[r * stride - border + c] = input[r * stride];
        input[r * stride + w + c] = input[r * stride + (w - 1)];
      }
    }
    const int use_no_round = rnd_.Rand8() & 1;
    for (int sub_x = 0; sub_x < 2; ++sub_x)
      for (int sub_y = 0; sub_y < 2; ++sub_y) {
        generate_warped_model(&rnd_, mat, &alpha, &beta, &gamma, &delta,
                              is_alpha_zero, is_beta_zero, is_gamma_zero,
                              is_delta_zero, 1);
        for (int ii = 0; ii < 2; ++ii) {
          for (int jj = 0; jj < 5; ++jj) {
            for (int do_average = 0; do_average <= 1; ++do_average) {
              if (use_no_round) {
                conv_params = get_conv_params_no_round(
                    do_average, 0, dsta.get(), out_w, 1, bd);
              } else {
                conv_params = get_conv_params(0, 0, bd);
              }
              if (jj >= 4) {
                conv_params.use_dist_wtd_comp_avg = 0;
              } else {
                conv_params.use_dist_wtd_comp_avg = 1;
                conv_params.fwd_offset = quant_dist_lookup_table[jj][ii];
                conv_params.bck_offset = quant_dist_lookup_table[jj][1 - ii];
              }

              av1_highbd_warp_affine_c(mat, input, w, h, stride, output.get(),
                                       32, 32, out_w, out_h, out_w, sub_x,
                                       sub_y, bd, &conv_params, alpha, beta,
                                       gamma, delta);
              if (use_no_round) {
                // TODO(angiebird): Change this to test_impl once we have SIMD
                // implementation
                conv_params = get_conv_params_no_round(
                    do_average, 0, dstb.get(), out_w, 1, bd);
              }
              if (jj >= 4) {
                conv_params.use_dist_wtd_comp_avg = 0;
              } else {
                conv_params.use_dist_wtd_comp_avg = 1;
                conv_params.fwd_offset = quant_dist_lookup_table[jj][ii];
                conv_params.bck_offset = quant_dist_lookup_table[jj][1 - ii];
              }
              test_impl(mat, input, w, h, stride, output2.get(), 32, 32, out_w,
                        out_h, out_w, sub_x, sub_y, bd, &conv_params, alpha,
                        beta, gamma, delta);

              if (use_no_round) {
                for (int j = 0; j < out_w * out_h; ++j)
                  ASSERT_EQ(dsta[j], dstb[j])
                      << "Pixel mismatch at index " << j << " = ("
                      << (j % out_w) << ", " << (j / out_w) << ") on iteration "
                      << i;
                for (int j = 0; j < out_w * out_h; ++j)
                  ASSERT_EQ(output[j], output2[j])
                      << "Pixel mismatch at index " << j << " = ("
                      << (j % out_w) << ", " << (j / out_w) << ") on iteration "
                      << i;
              } else {
                for (int j = 0; j < out_w * out_h; ++j)
                  ASSERT_EQ(output[j], output2[j])
                      << "Pixel mismatch at index " << j << " = ("
                      << (j % out_w) << ", " << (j / out_w) << ") on iteration "
                      << i;
              }
            }
          }
        }
      }
  }
}
}  // namespace AV1HighbdWarpFilter
#endif  // CONFIG_AV1_HIGHBITDEPTH
}  // namespace libaom_test
