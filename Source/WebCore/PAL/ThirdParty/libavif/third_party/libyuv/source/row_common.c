/*
 *  Copyright 2011 The LibYuv Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS. All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "libyuv/row.h"

#include <assert.h>
#include <string.h>  // For memcpy and memset.

#include "libyuv/basic_types.h"

#define STATIC_CAST(type, expr) (type)(expr)

void CopyRow_C(const uint8_t* src, uint8_t* dst, int count) {
  memcpy(dst, src, count);
}

// Blend 2 rows into 1.
static void HalfRow_C(const uint8_t* src_uv,
                      ptrdiff_t src_uv_stride,
                      uint8_t* dst_uv,
                      int width) {
  int x;
  for (x = 0; x < width; ++x) {
    dst_uv[x] = (src_uv[x] + src_uv[src_uv_stride + x] + 1) >> 1;
  }
}

static void HalfRow_16_C(const uint16_t* src_uv,
                         ptrdiff_t src_uv_stride,
                         uint16_t* dst_uv,
                         int width) {
  int x;
  for (x = 0; x < width; ++x) {
    dst_uv[x] = (src_uv[x] + src_uv[src_uv_stride + x] + 1) >> 1;
  }
}

// C version 2x2 -> 2x1.
void InterpolateRow_C(uint8_t* dst_ptr,
                      const uint8_t* src_ptr,
                      ptrdiff_t src_stride,
                      int width,
                      int source_y_fraction) {
  int y1_fraction = source_y_fraction;
  int y0_fraction = 256 - y1_fraction;
  const uint8_t* src_ptr1 = src_ptr + src_stride;
  int x;
  assert(source_y_fraction >= 0);
  assert(source_y_fraction < 256);

  if (y1_fraction == 0) {
    memcpy(dst_ptr, src_ptr, width);
    return;
  }
  if (y1_fraction == 128) {
    HalfRow_C(src_ptr, src_stride, dst_ptr, width);
    return;
  }
  for (x = 0; x < width; ++x) {
    dst_ptr[0] = STATIC_CAST(
        uint8_t,
        (src_ptr[0] * y0_fraction + src_ptr1[0] * y1_fraction + 128) >> 8);
    ++src_ptr;
    ++src_ptr1;
    ++dst_ptr;
  }
}

// C version 2x2 -> 2x1.
void InterpolateRow_16_C(uint16_t* dst_ptr,
                         const uint16_t* src_ptr,
                         ptrdiff_t src_stride,
                         int width,
                         int source_y_fraction) {
  int y1_fraction = source_y_fraction;
  int y0_fraction = 256 - y1_fraction;
  const uint16_t* src_ptr1 = src_ptr + src_stride;
  int x;
  assert(source_y_fraction >= 0);
  assert(source_y_fraction < 256);

  if (y1_fraction == 0) {
    memcpy(dst_ptr, src_ptr, width * 2);
    return;
  }
  if (y1_fraction == 128) {
    HalfRow_16_C(src_ptr, src_stride, dst_ptr, width);
    return;
  }
  for (x = 0; x < width; ++x) {
    dst_ptr[0] = STATIC_CAST(
        uint16_t,
        (src_ptr[0] * y0_fraction + src_ptr1[0] * y1_fraction + 128) >> 8);
    ++src_ptr;
    ++src_ptr1;
    ++dst_ptr;
  }
}
