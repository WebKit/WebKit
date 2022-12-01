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

#include "test/hiprec_convolve_test_util.h"

#include <memory>
#include <new>

#include "av1/common/restoration.h"

using std::make_tuple;
using std::tuple;

namespace libaom_test {

// Generate a random pair of filter kernels, using the ranges
// of possible values from the loop-restoration experiment
static void generate_kernels(ACMRandom *rnd, InterpKernel hkernel,
                             InterpKernel vkernel, int kernel_type = 2) {
  if (kernel_type == 0) {
    // Low possible values for filter coefficients
    hkernel[0] = hkernel[6] = vkernel[0] = vkernel[6] = WIENER_FILT_TAP0_MINV;
    hkernel[1] = hkernel[5] = vkernel[1] = vkernel[5] = WIENER_FILT_TAP1_MINV;
    hkernel[2] = hkernel[4] = vkernel[2] = vkernel[4] = WIENER_FILT_TAP2_MINV;
    hkernel[3] = vkernel[3] = -2 * (hkernel[0] + hkernel[1] + hkernel[2]);
    hkernel[7] = vkernel[7] = 0;
  } else if (kernel_type == 1) {
    // Max possible values for filter coefficients
    hkernel[0] = hkernel[6] = vkernel[0] = vkernel[6] = WIENER_FILT_TAP0_MAXV;
    hkernel[1] = hkernel[5] = vkernel[1] = vkernel[5] = WIENER_FILT_TAP1_MAXV;
    hkernel[2] = hkernel[4] = vkernel[2] = vkernel[4] = WIENER_FILT_TAP2_MAXV;
    hkernel[3] = vkernel[3] = -2 * (hkernel[0] + hkernel[1] + hkernel[2]);
    hkernel[7] = vkernel[7] = 0;
  } else {
    // Randomly generated values for filter coefficients
    hkernel[0] = hkernel[6] =
        WIENER_FILT_TAP0_MINV +
        rnd->PseudoUniform(WIENER_FILT_TAP0_MAXV + 1 - WIENER_FILT_TAP0_MINV);
    hkernel[1] = hkernel[5] =
        WIENER_FILT_TAP1_MINV +
        rnd->PseudoUniform(WIENER_FILT_TAP1_MAXV + 1 - WIENER_FILT_TAP1_MINV);
    hkernel[2] = hkernel[4] =
        WIENER_FILT_TAP2_MINV +
        rnd->PseudoUniform(WIENER_FILT_TAP2_MAXV + 1 - WIENER_FILT_TAP2_MINV);
    hkernel[3] = -2 * (hkernel[0] + hkernel[1] + hkernel[2]);
    hkernel[7] = 0;

    vkernel[0] = vkernel[6] =
        WIENER_FILT_TAP0_MINV +
        rnd->PseudoUniform(WIENER_FILT_TAP0_MAXV + 2 - WIENER_FILT_TAP0_MINV);
    vkernel[1] = vkernel[5] =
        WIENER_FILT_TAP1_MINV +
        rnd->PseudoUniform(WIENER_FILT_TAP1_MAXV + 2 - WIENER_FILT_TAP1_MINV);
    vkernel[2] = vkernel[4] =
        WIENER_FILT_TAP2_MINV +
        rnd->PseudoUniform(WIENER_FILT_TAP2_MAXV + 2 - WIENER_FILT_TAP2_MINV);
    vkernel[3] = -2 * (vkernel[0] + vkernel[1] + vkernel[2]);
    vkernel[7] = 0;
  }
}

namespace AV1HiprecConvolve {

::testing::internal::ParamGenerator<HiprecConvolveParam> BuildParams(
    hiprec_convolve_func filter) {
  const HiprecConvolveParam params[] = {
    make_tuple(8, 8, 50000, filter),   make_tuple(8, 4, 50000, filter),
    make_tuple(64, 24, 1000, filter),  make_tuple(64, 64, 1000, filter),
    make_tuple(64, 56, 1000, filter),  make_tuple(32, 8, 10000, filter),
    make_tuple(32, 28, 10000, filter), make_tuple(32, 32, 10000, filter),
    make_tuple(16, 34, 10000, filter), make_tuple(32, 34, 10000, filter),
    make_tuple(64, 34, 1000, filter),  make_tuple(8, 17, 10000, filter),
    make_tuple(16, 17, 10000, filter), make_tuple(32, 17, 10000, filter)
  };
  return ::testing::ValuesIn(params);
}

AV1HiprecConvolveTest::~AV1HiprecConvolveTest() {}
void AV1HiprecConvolveTest::SetUp() {
  rnd_.Reset(ACMRandom::DeterministicSeed());
}

void AV1HiprecConvolveTest::TearDown() {}

void AV1HiprecConvolveTest::RunCheckOutput(hiprec_convolve_func test_impl) {
  const int w = 128, h = 128;
  const int out_w = GET_PARAM(0), out_h = GET_PARAM(1);
  const int num_iters = GET_PARAM(2);
  int i, j, k, m;
  const ConvolveParams conv_params = get_conv_params_wiener(8);

  std::unique_ptr<uint8_t[]> input_(new (std::nothrow) uint8_t[h * w]);
  ASSERT_NE(input_, nullptr);
  uint8_t *input = input_.get();

  // The AVX2 convolve functions always write rows with widths that are
  // multiples of 16. So to avoid a buffer overflow, we may need to pad
  // rows to a multiple of 16.
  int output_n = ALIGN_POWER_OF_TWO(out_w, 4) * out_h;
  std::unique_ptr<uint8_t[]> output(new (std::nothrow) uint8_t[output_n]);
  ASSERT_NE(output, nullptr);
  std::unique_ptr<uint8_t[]> output2(new (std::nothrow) uint8_t[output_n]);
  ASSERT_NE(output2, nullptr);

  // Generate random filter kernels
  DECLARE_ALIGNED(16, InterpKernel, hkernel);
  DECLARE_ALIGNED(16, InterpKernel, vkernel);

  for (int kernel_type = 0; kernel_type < 3; kernel_type++) {
    generate_kernels(&rnd_, hkernel, vkernel, kernel_type);
    for (i = 0; i < num_iters; ++i) {
      for (k = 0; k < h; ++k)
        for (m = 0; m < w; ++m) input[k * w + m] = rnd_.Rand8();
      // Choose random locations within the source block
      int offset_r = 3 + rnd_.PseudoUniform(h - out_h - 7);
      int offset_c = 3 + rnd_.PseudoUniform(w - out_w - 7);
      av1_wiener_convolve_add_src_c(input + offset_r * w + offset_c, w,
                                    output.get(), out_w, hkernel, 16, vkernel,
                                    16, out_w, out_h, &conv_params);
      test_impl(input + offset_r * w + offset_c, w, output2.get(), out_w,
                hkernel, 16, vkernel, 16, out_w, out_h, &conv_params);

      for (j = 0; j < out_w * out_h; ++j)
        ASSERT_EQ(output[j], output2[j])
            << "Pixel mismatch at index " << j << " = (" << (j % out_w) << ", "
            << (j / out_w) << ") on iteration " << i;
    }
  }
}

void AV1HiprecConvolveTest::RunSpeedTest(hiprec_convolve_func test_impl) {
  const int w = 128, h = 128;
  const int out_w = GET_PARAM(0), out_h = GET_PARAM(1);
  const int num_iters = GET_PARAM(2) / 500;
  int i, j, k;
  const ConvolveParams conv_params = get_conv_params_wiener(8);

  std::unique_ptr<uint8_t[]> input_(new (std::nothrow) uint8_t[h * w]);
  ASSERT_NE(input_, nullptr);
  uint8_t *input = input_.get();

  // The AVX2 convolve functions always write rows with widths that are
  // multiples of 16. So to avoid a buffer overflow, we may need to pad
  // rows to a multiple of 16.
  int output_n = ALIGN_POWER_OF_TWO(out_w, 4) * out_h;
  std::unique_ptr<uint8_t[]> output(new (std::nothrow) uint8_t[output_n]);
  ASSERT_NE(output, nullptr);
  std::unique_ptr<uint8_t[]> output2(new (std::nothrow) uint8_t[output_n]);
  ASSERT_NE(output2, nullptr);

  // Generate random filter kernels
  DECLARE_ALIGNED(16, InterpKernel, hkernel);
  DECLARE_ALIGNED(16, InterpKernel, vkernel);

  generate_kernels(&rnd_, hkernel, vkernel);

  for (i = 0; i < h; ++i)
    for (j = 0; j < w; ++j) input[i * w + j] = rnd_.Rand8();

  aom_usec_timer ref_timer;
  aom_usec_timer_start(&ref_timer);
  for (i = 0; i < num_iters; ++i) {
    for (j = 3; j < h - out_h - 4; j++) {
      for (k = 3; k < w - out_w - 4; k++) {
        av1_wiener_convolve_add_src_c(input + j * w + k, w, output.get(), out_w,
                                      hkernel, 16, vkernel, 16, out_w, out_h,
                                      &conv_params);
      }
    }
  }
  aom_usec_timer_mark(&ref_timer);
  const int64_t ref_time = aom_usec_timer_elapsed(&ref_timer);

  aom_usec_timer tst_timer;
  aom_usec_timer_start(&tst_timer);
  for (i = 0; i < num_iters; ++i) {
    for (j = 3; j < h - out_h - 4; j++) {
      for (k = 3; k < w - out_w - 4; k++) {
        test_impl(input + j * w + k, w, output2.get(), out_w, hkernel, 16,
                  vkernel, 16, out_w, out_h, &conv_params);
      }
    }
  }
  aom_usec_timer_mark(&tst_timer);
  const int64_t tst_time = aom_usec_timer_elapsed(&tst_timer);

  std::cout << "[          ] C time = " << ref_time / 1000
            << " ms, SIMD time = " << tst_time / 1000 << " ms\n";

  EXPECT_GT(ref_time, tst_time)
      << "Error: AV1HiprecConvolveTest.SpeedTest, SIMD slower than C.\n"
      << "C time: " << ref_time << " us\n"
      << "SIMD time: " << tst_time << " us\n";
}
}  // namespace AV1HiprecConvolve

#if CONFIG_AV1_HIGHBITDEPTH
namespace AV1HighbdHiprecConvolve {

::testing::internal::ParamGenerator<HighbdHiprecConvolveParam> BuildParams(
    highbd_hiprec_convolve_func filter) {
  const HighbdHiprecConvolveParam params[] = {
    make_tuple(8, 8, 50000, 8, filter),   make_tuple(64, 64, 1000, 8, filter),
    make_tuple(32, 8, 10000, 8, filter),  make_tuple(8, 8, 50000, 10, filter),
    make_tuple(64, 64, 1000, 10, filter), make_tuple(32, 8, 10000, 10, filter),
    make_tuple(8, 8, 50000, 12, filter),  make_tuple(64, 64, 1000, 12, filter),
    make_tuple(32, 8, 10000, 12, filter),
  };
  return ::testing::ValuesIn(params);
}

AV1HighbdHiprecConvolveTest::~AV1HighbdHiprecConvolveTest() {}
void AV1HighbdHiprecConvolveTest::SetUp() {
  rnd_.Reset(ACMRandom::DeterministicSeed());
}

void AV1HighbdHiprecConvolveTest::TearDown() {}

void AV1HighbdHiprecConvolveTest::RunCheckOutput(
    highbd_hiprec_convolve_func test_impl) {
  const int w = 128, h = 128;
  const int out_w = GET_PARAM(0), out_h = GET_PARAM(1);
  const int num_iters = GET_PARAM(2);
  const int bd = GET_PARAM(3);
  int i, j;
  const ConvolveParams conv_params = get_conv_params_wiener(bd);

  std::unique_ptr<uint16_t[]> input(new (std::nothrow) uint16_t[h * w]);
  ASSERT_NE(input, nullptr);

  // The AVX2 convolve functions always write rows with widths that are
  // multiples of 16. So to avoid a buffer overflow, we may need to pad
  // rows to a multiple of 16.
  int output_n = ALIGN_POWER_OF_TWO(out_w, 4) * out_h;
  std::unique_ptr<uint16_t[]> output(new (std::nothrow) uint16_t[output_n]);
  ASSERT_NE(output, nullptr);
  std::unique_ptr<uint16_t[]> output2(new (std::nothrow) uint16_t[output_n]);
  ASSERT_NE(output2, nullptr);

  // Generate random filter kernels
  DECLARE_ALIGNED(16, InterpKernel, hkernel);
  DECLARE_ALIGNED(16, InterpKernel, vkernel);

  for (i = 0; i < h; ++i)
    for (j = 0; j < w; ++j) input[i * w + j] = rnd_.Rand16() & ((1 << bd) - 1);

  uint8_t *input_ptr = CONVERT_TO_BYTEPTR(input.get());
  uint8_t *output_ptr = CONVERT_TO_BYTEPTR(output.get());
  uint8_t *output2_ptr = CONVERT_TO_BYTEPTR(output2.get());
  for (int kernel_type = 0; kernel_type < 3; kernel_type++) {
    generate_kernels(&rnd_, hkernel, vkernel, kernel_type);
    for (i = 0; i < num_iters; ++i) {
      // Choose random locations within the source block
      int offset_r = 3 + rnd_.PseudoUniform(h - out_h - 7);
      int offset_c = 3 + rnd_.PseudoUniform(w - out_w - 7);
      av1_highbd_wiener_convolve_add_src_c(
          input_ptr + offset_r * w + offset_c, w, output_ptr, out_w, hkernel,
          16, vkernel, 16, out_w, out_h, &conv_params, bd);
      test_impl(input_ptr + offset_r * w + offset_c, w, output2_ptr, out_w,
                hkernel, 16, vkernel, 16, out_w, out_h, &conv_params, bd);

      for (j = 0; j < out_w * out_h; ++j)
        ASSERT_EQ(output[j], output2[j])
            << "Pixel mismatch at index " << j << " = (" << (j % out_w) << ", "
            << (j / out_w) << ") on iteration " << i;
    }
  }
}

void AV1HighbdHiprecConvolveTest::RunSpeedTest(
    highbd_hiprec_convolve_func test_impl) {
  const int w = 128, h = 128;
  const int out_w = GET_PARAM(0), out_h = GET_PARAM(1);
  const int num_iters = GET_PARAM(2) / 500;
  const int bd = GET_PARAM(3);
  int i, j, k;
  const ConvolveParams conv_params = get_conv_params_wiener(bd);

  std::unique_ptr<uint16_t[]> input(new (std::nothrow) uint16_t[h * w]);
  ASSERT_NE(input, nullptr);

  // The AVX2 convolve functions always write rows with widths that are
  // multiples of 16. So to avoid a buffer overflow, we may need to pad
  // rows to a multiple of 16.
  int output_n = ALIGN_POWER_OF_TWO(out_w, 4) * out_h;
  std::unique_ptr<uint16_t[]> output(new (std::nothrow) uint16_t[output_n]);
  ASSERT_NE(output, nullptr);
  std::unique_ptr<uint16_t[]> output2(new (std::nothrow) uint16_t[output_n]);
  ASSERT_NE(output2, nullptr);

  // Generate random filter kernels
  DECLARE_ALIGNED(16, InterpKernel, hkernel);
  DECLARE_ALIGNED(16, InterpKernel, vkernel);

  generate_kernels(&rnd_, hkernel, vkernel);

  for (i = 0; i < h; ++i)
    for (j = 0; j < w; ++j) input[i * w + j] = rnd_.Rand16() & ((1 << bd) - 1);

  uint8_t *input_ptr = CONVERT_TO_BYTEPTR(input.get());
  uint8_t *output_ptr = CONVERT_TO_BYTEPTR(output.get());
  uint8_t *output2_ptr = CONVERT_TO_BYTEPTR(output2.get());

  aom_usec_timer ref_timer;
  aom_usec_timer_start(&ref_timer);
  for (i = 0; i < num_iters; ++i) {
    for (j = 3; j < h - out_h - 4; j++) {
      for (k = 3; k < w - out_w - 4; k++) {
        av1_highbd_wiener_convolve_add_src_c(
            input_ptr + j * w + k, w, output_ptr, out_w, hkernel, 16, vkernel,
            16, out_w, out_h, &conv_params, bd);
      }
    }
  }
  aom_usec_timer_mark(&ref_timer);
  const int64_t ref_time = aom_usec_timer_elapsed(&ref_timer);

  aom_usec_timer tst_timer;
  aom_usec_timer_start(&tst_timer);
  for (i = 0; i < num_iters; ++i) {
    for (j = 3; j < h - out_h - 4; j++) {
      for (k = 3; k < w - out_w - 4; k++) {
        test_impl(input_ptr + j * w + k, w, output2_ptr, out_w, hkernel, 16,
                  vkernel, 16, out_w, out_h, &conv_params, bd);
      }
    }
  }
  aom_usec_timer_mark(&tst_timer);
  const int64_t tst_time = aom_usec_timer_elapsed(&tst_timer);

  std::cout << "[          ] C time = " << ref_time / 1000
            << " ms, SIMD time = " << tst_time / 1000 << " ms\n";

  EXPECT_GT(ref_time, tst_time)
      << "Error: AV1HighbdHiprecConvolveTest.SpeedTest, SIMD slower than C.\n"
      << "C time: " << ref_time << " us\n"
      << "SIMD time: " << tst_time << " us\n";
}
}  // namespace AV1HighbdHiprecConvolve
#endif  // CONFIG_AV1_HIGHBITDEPTH
}  // namespace libaom_test
