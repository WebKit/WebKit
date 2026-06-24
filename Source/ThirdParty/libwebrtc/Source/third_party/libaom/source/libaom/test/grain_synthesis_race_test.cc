/*
 * Copyright (c) 2026, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

// Test that av1_add_film_grain is thread-safe when called concurrently
// with images of different chroma subsampling (4:2:0 vs 4:4:4).
// Reproduces the data race on static globals in grain_synthesis.c.

#include <cstring>
#include <vector>

#include "gtest/gtest.h"
#include "aom/aom_image.h"
#include "aom_dsp/grain_params.h"
#include "aom_util/aom_pthread.h"
#include "av1/decoder/grain_synthesis.h"

namespace {

// Minimal film grain params that trigger the overlap path.
aom_film_grain_t MakeGrainParams() {
  aom_film_grain_t params = {};
  params.apply_grain = 1;
  params.update_parameters = 1;
  params.num_y_points = 2;
  params.scaling_points_y[0][0] = 0;
  params.scaling_points_y[0][1] = 96;
  params.scaling_points_y[1][0] = 255;
  params.scaling_points_y[1][1] = 96;
  params.num_cb_points = 2;
  params.scaling_points_cb[0][0] = 0;
  params.scaling_points_cb[0][1] = 64;
  params.scaling_points_cb[1][0] = 255;
  params.scaling_points_cb[1][1] = 64;
  params.num_cr_points = 2;
  params.scaling_points_cr[0][0] = 0;
  params.scaling_points_cr[0][1] = 64;
  params.scaling_points_cr[1][0] = 255;
  params.scaling_points_cr[1][1] = 64;
  params.scaling_shift = 11;
  params.ar_coeff_lag = 1;
  params.ar_coeff_shift = 7;
  params.overlap_flag = 1;
  params.bit_depth = 8;
  params.random_seed = 7391;
  params.cb_mult = 128;
  params.cb_luma_mult = 192;
  params.cb_offset = 256;
  params.cr_mult = 128;
  params.cr_luma_mult = 192;
  params.cr_offset = 256;
  return params;
}

void RunGrain(aom_img_fmt_t fmt, int iterations) {
  constexpr int kWidth = 128;
  constexpr int kHeight = 128;

  aom_image_t src;
  ASSERT_NE(aom_img_alloc(&src, fmt, kWidth, kHeight, 32), nullptr);
  memset(src.planes[AOM_PLANE_Y], 128,
         (size_t)src.stride[AOM_PLANE_Y] * kHeight);
  const int chroma_h = (fmt == AOM_IMG_FMT_I420) ? kHeight / 2 : kHeight;
  memset(src.planes[AOM_PLANE_U], 128,
         (size_t)src.stride[AOM_PLANE_U] * chroma_h);
  memset(src.planes[AOM_PLANE_V], 128,
         (size_t)src.stride[AOM_PLANE_V] * chroma_h);
  src.bit_depth = 8;
  src.mc = AOM_CICP_MC_BT_709;

  aom_image_t dst;
  ASSERT_NE(aom_img_alloc(&dst, fmt, kWidth, kHeight, 32), nullptr);
  dst.bit_depth = 8;
  dst.mc = AOM_CICP_MC_BT_709;

  aom_film_grain_t params = MakeGrainParams();

  for (int i = 0; i < iterations; ++i) {
    params.random_seed = (uint16_t)(7391 + i);
    int ret = av1_add_film_grain(&params, &src, &dst);
    ASSERT_EQ(ret, 0) << "av1_add_film_grain failed on iteration " << i;
  }

  aom_img_free(&src);
  aom_img_free(&dst);
}

constexpr int kIterations = 200;

THREADFN RunGrain420(void * /*arg*/) {
  RunGrain(AOM_IMG_FMT_I420, kIterations);
  return THREAD_EXIT_SUCCESS;
}

THREADFN RunGrain444(void * /*arg*/) {
  RunGrain(AOM_IMG_FMT_I444, kIterations);
  return THREAD_EXIT_SUCCESS;
}

TEST(GrainSynthesisRaceTest, ConcurrentDifferentSubsampling) {
  constexpr int kThreads = 4;

  std::vector<pthread_t> threads;
  threads.reserve(kThreads * 2);

  for (int i = 0; i < kThreads; ++i) {
    pthread_t thread;
    ASSERT_EQ(pthread_create(&thread, nullptr, RunGrain420, nullptr), 0);
    threads.push_back(thread);
    ASSERT_EQ(pthread_create(&thread, nullptr, RunGrain444, nullptr), 0);
    threads.push_back(thread);
  }

  for (auto thread : threads) {
    ASSERT_EQ(pthread_join(thread, nullptr), 0);
  }
}

}  // namespace
