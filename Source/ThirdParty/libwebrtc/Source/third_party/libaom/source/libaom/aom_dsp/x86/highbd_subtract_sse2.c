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

#include <assert.h>
#include <emmintrin.h>
#include <stddef.h>

#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"

typedef void (*SubtractWxHFuncType)(int16_t *diff, ptrdiff_t diff_stride,
                                    const uint16_t *src, ptrdiff_t src_stride,
                                    const uint16_t *pred,
                                    ptrdiff_t pred_stride);

static void subtract_4x4(int16_t *diff, ptrdiff_t diff_stride,
                         const uint16_t *src, ptrdiff_t src_stride,
                         const uint16_t *pred, ptrdiff_t pred_stride) {
  __m128i u0, u1, u2, u3;
  __m128i v0, v1, v2, v3;
  __m128i x0, x1, x2, x3;
  int64_t *store_diff = (int64_t *)(diff + 0 * diff_stride);

  u0 = _mm_loadl_epi64((__m128i const *)(src + 0 * src_stride));
  u1 = _mm_loadl_epi64((__m128i const *)(src + 1 * src_stride));
  u2 = _mm_loadl_epi64((__m128i const *)(src + 2 * src_stride));
  u3 = _mm_loadl_epi64((__m128i const *)(src + 3 * src_stride));

  v0 = _mm_loadl_epi64((__m128i const *)(pred + 0 * pred_stride));
  v1 = _mm_loadl_epi64((__m128i const *)(pred + 1 * pred_stride));
  v2 = _mm_loadl_epi64((__m128i const *)(pred + 2 * pred_stride));
  v3 = _mm_loadl_epi64((__m128i const *)(pred + 3 * pred_stride));

  x0 = _mm_sub_epi16(u0, v0);
  x1 = _mm_sub_epi16(u1, v1);
  x2 = _mm_sub_epi16(u2, v2);
  x3 = _mm_sub_epi16(u3, v3);

  _mm_storel_epi64((__m128i *)store_diff, x0);
  store_diff = (int64_t *)(diff + 1 * diff_stride);
  _mm_storel_epi64((__m128i *)store_diff, x1);
  store_diff = (int64_t *)(diff + 2 * diff_stride);
  _mm_storel_epi64((__m128i *)store_diff, x2);
  store_diff = (int64_t *)(diff + 3 * diff_stride);
  _mm_storel_epi64((__m128i *)store_diff, x3);
}

static void subtract_4x8(int16_t *diff, ptrdiff_t diff_stride,
                         const uint16_t *src, ptrdiff_t src_stride,
                         const uint16_t *pred, ptrdiff_t pred_stride) {
  __m128i u0, u1, u2, u3, u4, u5, u6, u7;
  __m128i v0, v1, v2, v3, v4, v5, v6, v7;
  __m128i x0, x1, x2, x3, x4, x5, x6, x7;
  int64_t *store_diff = (int64_t *)(diff + 0 * diff_stride);

  u0 = _mm_loadl_epi64((__m128i const *)(src + 0 * src_stride));
  u1 = _mm_loadl_epi64((__m128i const *)(src + 1 * src_stride));
  u2 = _mm_loadl_epi64((__m128i const *)(src + 2 * src_stride));
  u3 = _mm_loadl_epi64((__m128i const *)(src + 3 * src_stride));
  u4 = _mm_loadl_epi64((__m128i const *)(src + 4 * src_stride));
  u5 = _mm_loadl_epi64((__m128i const *)(src + 5 * src_stride));
  u6 = _mm_loadl_epi64((__m128i const *)(src + 6 * src_stride));
  u7 = _mm_loadl_epi64((__m128i const *)(src + 7 * src_stride));

  v0 = _mm_loadl_epi64((__m128i const *)(pred + 0 * pred_stride));
  v1 = _mm_loadl_epi64((__m128i const *)(pred + 1 * pred_stride));
  v2 = _mm_loadl_epi64((__m128i const *)(pred + 2 * pred_stride));
  v3 = _mm_loadl_epi64((__m128i const *)(pred + 3 * pred_stride));
  v4 = _mm_loadl_epi64((__m128i const *)(pred + 4 * pred_stride));
  v5 = _mm_loadl_epi64((__m128i const *)(pred + 5 * pred_stride));
  v6 = _mm_loadl_epi64((__m128i const *)(pred + 6 * pred_stride));
  v7 = _mm_loadl_epi64((__m128i const *)(pred + 7 * pred_stride));

  x0 = _mm_sub_epi16(u0, v0);
  x1 = _mm_sub_epi16(u1, v1);
  x2 = _mm_sub_epi16(u2, v2);
  x3 = _mm_sub_epi16(u3, v3);
  x4 = _mm_sub_epi16(u4, v4);
  x5 = _mm_sub_epi16(u5, v5);
  x6 = _mm_sub_epi16(u6, v6);
  x7 = _mm_sub_epi16(u7, v7);

  _mm_storel_epi64((__m128i *)store_diff, x0);
  store_diff = (int64_t *)(diff + 1 * diff_stride);
  _mm_storel_epi64((__m128i *)store_diff, x1);
  store_diff = (int64_t *)(diff + 2 * diff_stride);
  _mm_storel_epi64((__m128i *)store_diff, x2);
  store_diff = (int64_t *)(diff + 3 * diff_stride);
  _mm_storel_epi64((__m128i *)store_diff, x3);
  store_diff = (int64_t *)(diff + 4 * diff_stride);
  _mm_storel_epi64((__m128i *)store_diff, x4);
  store_diff = (int64_t *)(diff + 5 * diff_stride);
  _mm_storel_epi64((__m128i *)store_diff, x5);
  store_diff = (int64_t *)(diff + 6 * diff_stride);
  _mm_storel_epi64((__m128i *)store_diff, x6);
  store_diff = (int64_t *)(diff + 7 * diff_stride);
  _mm_storel_epi64((__m128i *)store_diff, x7);
}

static void subtract_8x4(int16_t *diff, ptrdiff_t diff_stride,
                         const uint16_t *src, ptrdiff_t src_stride,
                         const uint16_t *pred, ptrdiff_t pred_stride) {
  __m128i u0, u1, u2, u3;
  __m128i v0, v1, v2, v3;
  __m128i x0, x1, x2, x3;

  u0 = _mm_loadu_si128((__m128i const *)(src + 0 * src_stride));
  u1 = _mm_loadu_si128((__m128i const *)(src + 1 * src_stride));
  u2 = _mm_loadu_si128((__m128i const *)(src + 2 * src_stride));
  u3 = _mm_loadu_si128((__m128i const *)(src + 3 * src_stride));

  v0 = _mm_loadu_si128((__m128i const *)(pred + 0 * pred_stride));
  v1 = _mm_loadu_si128((__m128i const *)(pred + 1 * pred_stride));
  v2 = _mm_loadu_si128((__m128i const *)(pred + 2 * pred_stride));
  v3 = _mm_loadu_si128((__m128i const *)(pred + 3 * pred_stride));

  x0 = _mm_sub_epi16(u0, v0);
  x1 = _mm_sub_epi16(u1, v1);
  x2 = _mm_sub_epi16(u2, v2);
  x3 = _mm_sub_epi16(u3, v3);

  _mm_storeu_si128((__m128i *)(diff + 0 * diff_stride), x0);
  _mm_storeu_si128((__m128i *)(diff + 1 * diff_stride), x1);
  _mm_storeu_si128((__m128i *)(diff + 2 * diff_stride), x2);
  _mm_storeu_si128((__m128i *)(diff + 3 * diff_stride), x3);
}

static void subtract_8x8(int16_t *diff, ptrdiff_t diff_stride,
                         const uint16_t *src, ptrdiff_t src_stride,
                         const uint16_t *pred, ptrdiff_t pred_stride) {
  __m128i u0, u1, u2, u3, u4, u5, u6, u7;
  __m128i v0, v1, v2, v3, v4, v5, v6, v7;
  __m128i x0, x1, x2, x3, x4, x5, x6, x7;

  u0 = _mm_loadu_si128((__m128i const *)(src + 0 * src_stride));
  u1 = _mm_loadu_si128((__m128i const *)(src + 1 * src_stride));
  u2 = _mm_loadu_si128((__m128i const *)(src + 2 * src_stride));
  u3 = _mm_loadu_si128((__m128i const *)(src + 3 * src_stride));
  u4 = _mm_loadu_si128((__m128i const *)(src + 4 * src_stride));
  u5 = _mm_loadu_si128((__m128i const *)(src + 5 * src_stride));
  u6 = _mm_loadu_si128((__m128i const *)(src + 6 * src_stride));
  u7 = _mm_loadu_si128((__m128i const *)(src + 7 * src_stride));

  v0 = _mm_loadu_si128((__m128i const *)(pred + 0 * pred_stride));
  v1 = _mm_loadu_si128((__m128i const *)(pred + 1 * pred_stride));
  v2 = _mm_loadu_si128((__m128i const *)(pred + 2 * pred_stride));
  v3 = _mm_loadu_si128((__m128i const *)(pred + 3 * pred_stride));
  v4 = _mm_loadu_si128((__m128i const *)(pred + 4 * pred_stride));
  v5 = _mm_loadu_si128((__m128i const *)(pred + 5 * pred_stride));
  v6 = _mm_loadu_si128((__m128i const *)(pred + 6 * pred_stride));
  v7 = _mm_loadu_si128((__m128i const *)(pred + 7 * pred_stride));

  x0 = _mm_sub_epi16(u0, v0);
  x1 = _mm_sub_epi16(u1, v1);
  x2 = _mm_sub_epi16(u2, v2);
  x3 = _mm_sub_epi16(u3, v3);
  x4 = _mm_sub_epi16(u4, v4);
  x5 = _mm_sub_epi16(u5, v5);
  x6 = _mm_sub_epi16(u6, v6);
  x7 = _mm_sub_epi16(u7, v7);

  _mm_storeu_si128((__m128i *)(diff + 0 * diff_stride), x0);
  _mm_storeu_si128((__m128i *)(diff + 1 * diff_stride), x1);
  _mm_storeu_si128((__m128i *)(diff + 2 * diff_stride), x2);
  _mm_storeu_si128((__m128i *)(diff + 3 * diff_stride), x3);
  _mm_storeu_si128((__m128i *)(diff + 4 * diff_stride), x4);
  _mm_storeu_si128((__m128i *)(diff + 5 * diff_stride), x5);
  _mm_storeu_si128((__m128i *)(diff + 6 * diff_stride), x6);
  _mm_storeu_si128((__m128i *)(diff + 7 * diff_stride), x7);
}

#define STACK_V(h, fun)                                                        \
  do {                                                                         \
    fun(diff, diff_stride, src, src_stride, pred, pred_stride);                \
    fun(diff + diff_stride * h, diff_stride, src + src_stride * h, src_stride, \
        pred + pred_stride * h, pred_stride);                                  \
  } while (0)

#define STACK_H(w, fun)                                                     \
  do {                                                                      \
    fun(diff, diff_stride, src, src_stride, pred, pred_stride);             \
    fun(diff + w, diff_stride, src + w, src_stride, pred + w, pred_stride); \
  } while (0)

#define SUBTRACT_FUN(size)                                               \
  static void subtract_##size(int16_t *diff, ptrdiff_t diff_stride,      \
                              const uint16_t *src, ptrdiff_t src_stride, \
                              const uint16_t *pred, ptrdiff_t pred_stride)

SUBTRACT_FUN(8x16) { STACK_V(8, subtract_8x8); }
SUBTRACT_FUN(16x8) { STACK_H(8, subtract_8x8); }
SUBTRACT_FUN(16x16) { STACK_V(8, subtract_16x8); }
SUBTRACT_FUN(16x32) { STACK_V(16, subtract_16x16); }
SUBTRACT_FUN(32x16) { STACK_H(16, subtract_16x16); }
SUBTRACT_FUN(32x32) { STACK_V(16, subtract_32x16); }
SUBTRACT_FUN(32x64) { STACK_V(32, subtract_32x32); }
SUBTRACT_FUN(64x32) { STACK_H(32, subtract_32x32); }
SUBTRACT_FUN(64x64) { STACK_V(32, subtract_64x32); }
SUBTRACT_FUN(64x128) { STACK_V(64, subtract_64x64); }
SUBTRACT_FUN(128x64) { STACK_H(64, subtract_64x64); }
SUBTRACT_FUN(128x128) { STACK_V(64, subtract_128x64); }
SUBTRACT_FUN(4x16) { STACK_V(8, subtract_4x8); }
SUBTRACT_FUN(16x4) { STACK_H(8, subtract_8x4); }
SUBTRACT_FUN(8x32) { STACK_V(16, subtract_8x16); }
SUBTRACT_FUN(32x8) { STACK_H(16, subtract_16x8); }
SUBTRACT_FUN(16x64) { STACK_V(32, subtract_16x32); }
SUBTRACT_FUN(64x16) { STACK_H(32, subtract_32x16); }

static SubtractWxHFuncType getSubtractFunc(int rows, int cols) {
  if (rows == 4) {
    if (cols == 4) return subtract_4x4;
    if (cols == 8) return subtract_8x4;
    if (cols == 16) return subtract_16x4;
  }
  if (rows == 8) {
    if (cols == 4) return subtract_4x8;
    if (cols == 8) return subtract_8x8;
    if (cols == 16) return subtract_16x8;
    if (cols == 32) return subtract_32x8;
  }
  if (rows == 16) {
    if (cols == 4) return subtract_4x16;
    if (cols == 8) return subtract_8x16;
    if (cols == 16) return subtract_16x16;
    if (cols == 32) return subtract_32x16;
    if (cols == 64) return subtract_64x16;
  }
  if (rows == 32) {
    if (cols == 8) return subtract_8x32;
    if (cols == 16) return subtract_16x32;
    if (cols == 32) return subtract_32x32;
    if (cols == 64) return subtract_64x32;
  }
  if (rows == 64) {
    if (cols == 16) return subtract_16x64;
    if (cols == 32) return subtract_32x64;
    if (cols == 64) return subtract_64x64;
    if (cols == 128) return subtract_128x64;
  }
  if (rows == 128) {
    if (cols == 64) return subtract_64x128;
    if (cols == 128) return subtract_128x128;
  }
  assert(0);
  return NULL;
}

void aom_highbd_subtract_block_sse2(int rows, int cols, int16_t *diff,
                                    ptrdiff_t diff_stride, const uint8_t *src8,
                                    ptrdiff_t src_stride, const uint8_t *pred8,
                                    ptrdiff_t pred_stride) {
  uint16_t *src = CONVERT_TO_SHORTPTR(src8);
  uint16_t *pred = CONVERT_TO_SHORTPTR(pred8);
  SubtractWxHFuncType func;

  func = getSubtractFunc(rows, cols);
  func(diff, diff_stride, src, src_stride, pred, pred_stride);
}
