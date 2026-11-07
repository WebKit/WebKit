/*
 *  Copyright 2011 The LibYuv Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS. All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "libyuv/rotate_row.h"
#include "libyuv/row.h"

#ifdef __cplusplus
namespace libyuv {
extern "C" {
#endif

void TransposeWx8_C(const uint8_t* src,
                    int src_stride,
                    uint8_t* dst,
                    int dst_stride,
                    int width) {
  int i;
  for (i = 0; i < width; ++i) {
    dst[0] = src[0 * src_stride];
    dst[1] = src[1 * src_stride];
    dst[2] = src[2 * src_stride];
    dst[3] = src[3 * src_stride];
    dst[4] = src[4 * src_stride];
    dst[5] = src[5 * src_stride];
    dst[6] = src[6 * src_stride];
    dst[7] = src[7 * src_stride];
    ++src;
    dst += dst_stride;
  }
}

void TransposeWx16_C(const uint8_t* src,
                     int src_stride,
                     uint8_t* dst,
                     int dst_stride,
                     int width) {
  TransposeWx8_C(src, src_stride, dst, dst_stride, width);
  TransposeWx8_C((src + 8 * src_stride), src_stride, (dst + 8), dst_stride,
                 width);
}

void TransposeUVWx8_C(const uint8_t* src,
                      int src_stride,
                      uint8_t* dst_a,
                      int dst_stride_a,
                      uint8_t* dst_b,
                      int dst_stride_b,
                      int width) {
  int i;
  for (i = 0; i < width; ++i) {
    dst_a[0] = src[0 * src_stride + 0];
    dst_b[0] = src[0 * src_stride + 1];
    dst_a[1] = src[1 * src_stride + 0];
    dst_b[1] = src[1 * src_stride + 1];
    dst_a[2] = src[2 * src_stride + 0];
    dst_b[2] = src[2 * src_stride + 1];
    dst_a[3] = src[3 * src_stride + 0];
    dst_b[3] = src[3 * src_stride + 1];
    dst_a[4] = src[4 * src_stride + 0];
    dst_b[4] = src[4 * src_stride + 1];
    dst_a[5] = src[5 * src_stride + 0];
    dst_b[5] = src[5 * src_stride + 1];
    dst_a[6] = src[6 * src_stride + 0];
    dst_b[6] = src[6 * src_stride + 1];
    dst_a[7] = src[7 * src_stride + 0];
    dst_b[7] = src[7 * src_stride + 1];
    src += 2;
    dst_a += dst_stride_a;
    dst_b += dst_stride_b;
  }
}

void TransposeUVWx16_C(const uint8_t* src,
                       int src_stride,
                       uint8_t* dst_a,
                       int dst_stride_a,
                       uint8_t* dst_b,
                       int dst_stride_b,
                       int width) {
  TransposeUVWx8_C(src, src_stride, dst_a, dst_stride_a, dst_b, dst_stride_b,
                   width);
  TransposeUVWx8_C((src + 8 * src_stride), src_stride, (dst_a + 8),
                   dst_stride_a, (dst_b + 8), dst_stride_b, width);
}

void TransposeWxH_C(const uint8_t* src,
                    int src_stride,
                    uint8_t* dst,
                    int dst_stride,
                    int width,
                    int height) {
  int i;
  for (i = 0; i < width; ++i) {
    int j;
    for (j = 0; j < height; ++j) {
      dst[i * dst_stride + j] = src[j * src_stride + i];
    }
  }
}

void TransposeUVWxH_C(const uint8_t* src,
                      int src_stride,
                      uint8_t* dst_a,
                      int dst_stride_a,
                      uint8_t* dst_b,
                      int dst_stride_b,
                      int width,
                      int height) {
  int i;
  for (i = 0; i < width * 2; i += 2) {
    int j;
    for (j = 0; j < height; ++j) {
      dst_a[((i >> 1) * dst_stride_a) + j] = src[i + (j * src_stride)];
      dst_b[((i >> 1) * dst_stride_b) + j] = src[i + (j * src_stride) + 1];
    }
  }
}

void TransposeWx8_16_C(const uint16_t* src,
                       int src_stride,
                       uint16_t* dst,
                       int dst_stride,
                       int width) {
  int i;
  for (i = 0; i < width; ++i) {
    dst[0] = src[0 * src_stride];
    dst[1] = src[1 * src_stride];
    dst[2] = src[2 * src_stride];
    dst[3] = src[3 * src_stride];
    dst[4] = src[4 * src_stride];
    dst[5] = src[5 * src_stride];
    dst[6] = src[6 * src_stride];
    dst[7] = src[7 * src_stride];
    ++src;
    dst += dst_stride;
  }
}

void TransposeWxH_16_C(const uint16_t* src,
                       int src_stride,
                       uint16_t* dst,
                       int dst_stride,
                       int width,
                       int height) {
  int i;
  for (i = 0; i < width; ++i) {
    int j;
    for (j = 0; j < height; ++j) {
      dst[i * dst_stride + j] = src[j * src_stride + i];
    }
  }
}

static inline void Copy4(uint8_t* dst, const uint8_t* src) {
  dst[0] = src[0];
  dst[1] = src[1];
  dst[2] = src[2];
  dst[3] = src[3];
}

// Transpose 32 bit values (ARGB)
void TransposeWx4_32_C(const uint8_t* src,
                       int src_stride,
                       uint8_t* dst,
                       int dst_stride,
                       int width) {
  const uint8_t* src1 = src + src_stride;
  const uint8_t* src2 = src1 + src_stride;
  const uint8_t* src3 = src2 + src_stride;
  uint8_t* dst1 = dst + dst_stride;
  uint8_t* dst2 = dst1 + dst_stride;
  uint8_t* dst3 = dst2 + dst_stride;
  int i;
  for (i = 0; i < width; i += 4) {
    Copy4(dst + 0, src + 0);
    Copy4(dst + 4, src1 + 0);
    Copy4(dst + 8, src2 + 0);
    Copy4(dst + 12, src3 + 0);
    Copy4(dst1 + 0, src + 4);
    Copy4(dst1 + 4, src1 + 4);
    Copy4(dst1 + 8, src2 + 4);
    Copy4(dst1 + 12, src3 + 4);
    Copy4(dst2 + 0, src + 8);
    Copy4(dst2 + 4, src1 + 8);
    Copy4(dst2 + 8, src2 + 8);
    Copy4(dst2 + 12, src3 + 8);
    Copy4(dst3 + 0, src + 12);
    Copy4(dst3 + 4, src1 + 12);
    Copy4(dst3 + 8, src2 + 12);
    Copy4(dst3 + 12, src3 + 12);
    src += (ptrdiff_t)src_stride * 4;  // advance 4 rows
    src1 += (ptrdiff_t)src_stride * 4;
    src2 += (ptrdiff_t)src_stride * 4;
    src3 += (ptrdiff_t)src_stride * 4;
    dst += 4 * 4;  // advance 4 columns
    dst1 += 4 * 4;
    dst2 += 4 * 4;
    dst3 += 4 * 4;
  }
}

void TransposeWxH_32_C(const uint8_t* src,
                       int src_stride,
                       uint8_t* dst,
                       int dst_stride,
                       int width,
                       int height) {
  int i;
  for (i = 0; i < width; ++i) {
    int j;
    for (j = 0; j < height; ++j) {
      Copy4(dst + (ptrdiff_t)i * dst_stride + j * 4,
            src + (ptrdiff_t)j * src_stride + i * 4);
    }
  }
}

#ifdef __cplusplus
}  // extern "C"
}  // namespace libyuv
#endif
