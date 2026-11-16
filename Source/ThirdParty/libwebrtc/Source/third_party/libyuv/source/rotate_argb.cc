/*
 *  Copyright 2012 The LibYuv Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS. All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "libyuv/rotate_argb.h"

#include <limits.h>

#include "libyuv/convert.h"
#include "libyuv/cpu_id.h"
#include "libyuv/planar_functions.h"
#include "libyuv/rotate.h"
#include "libyuv/rotate_row.h"
#include "libyuv/row.h"

#ifdef __cplusplus
namespace libyuv {
extern "C" {
#endif

static int ARGBTranspose(const uint8_t* src_argb,
                         int src_stride_argb,
                         uint8_t* dst_argb,
                         int dst_stride_argb,
                         int width,
                         int height) {
#if defined(HAS_TRANSPOSEWXH_32_NEON) || defined(HAS_TRANSPOSEWXH_32_SME) || \
    defined(HAS_TRANSPOSEWXH_32_RVV)
  void (*TransposeWxH_32)(const uint8_t* src, int src_stride, uint8_t* dst,
                          int dst_stride, int width, int height) = NULL;
#endif
#if defined(HAS_TRANSPOSEWX16_32_AVX512BW)
  void (*TransposeWx16_32)(const uint8_t* src, int src_stride, uint8_t* dst,
                           int dst_stride, int width) = NULL;
#endif
#if defined(HAS_TRANSPOSEWX8_32_AVX2)
  void (*TransposeWx8_32)(const uint8_t* src, int src_stride, uint8_t* dst,
                          int dst_stride, int width) = NULL;
#endif
  void (*TransposeWx4_32)(const uint8_t* src, int src_stride, uint8_t* dst,
                          int dst_stride, int width) = NULL;
  // Check stride is a multiple of 4.
  if (src_stride_argb & 3) {
    return -1;
  }
#if defined(HAS_TRANSPOSEWX4_32_SSE2)
  if (TestCpuFlag(kCpuHasSSE2)) {
    TransposeWx4_32 = TransposeWx4_32_Any_SSE2;
    if (IS_ALIGNED(height, 4)) {  // Width of dest.
      TransposeWx4_32 = TransposeWx4_32_SSE2;
    }
  }
#endif
#if defined(HAS_TRANSPOSEWX8_32_AVX2)
  if (TestCpuFlag(kCpuHasAVX2)) {
    TransposeWx8_32 = TransposeWx8_32_Any_AVX2;
    if (IS_ALIGNED(width, 4)) {
      TransposeWx8_32 = TransposeWx8_32_AVX2;
    }
  }
#endif
#if defined(HAS_TRANSPOSEWX16_32_AVX512BW)
  if (TestCpuFlag(kCpuHasAVX512BW)) {
    TransposeWx16_32 = TransposeWx16_32_AVX512BW;
  }
#endif
#if defined(HAS_TRANSPOSEWXH_32_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    TransposeWxH_32 = TransposeWxH_32_NEON;
  }
#endif
#if defined(HAS_TRANSPOSEWX4_32_SVE2)
  if (TestCpuFlag(kCpuHasSVE2)) {
    TransposeWx4_32 = TransposeWx4_32_SVE2;
  }
#endif
#if defined(HAS_TRANSPOSEWXH_32_SME)
  if (TestCpuFlag(kCpuHasSME)) {
    TransposeWxH_32 = TransposeWxH_32_SME;
  }
#endif
#if defined(HAS_TRANSPOSEWXH_32_RVV)
  if (TestCpuFlag(kCpuHasRVV)) {
    TransposeWxH_32 = TransposeWxH_32_RVV;
  }
#endif
#if defined(HAS_TRANSPOSEWX4_32_LSX)
  if (TestCpuFlag(kCpuHasLSX)) {
    TransposeWx4_32 = TransposeWx4_32_Any_LSX;
    if (IS_ALIGNED(height, 4)) {  // Width of dest.
      TransposeWx4_32 = TransposeWx4_32_LSX;
    }
  }
#endif
#if defined(HAS_TRANSPOSEWX4_32_LASX)
  if (TestCpuFlag(kCpuHasLASX)) {
    TransposeWx4_32 = TransposeWx4_32_Any_LASX;
    if (IS_ALIGNED(height, 8)) {  // Width of dest.
      TransposeWx4_32 = TransposeWx4_32_LASX;
    }
  }
#endif
#if defined(HAS_TRANSPOSEWX4_32_WASMSIMD)
  if (TestCpuFlag(kCpuHasWASMSIMD)) {
    TransposeWx4_32 = TransposeWx4_32_Any_WASMSIMD;
    if (IS_ALIGNED(height, 4)) {  // Width of dest.
      TransposeWx4_32 = TransposeWx4_32_WASMSIMD;
    }
  }
#endif

#if defined(HAS_TRANSPOSEWXH_32_NEON) || defined(HAS_TRANSPOSEWXH_32_SME) || \
    defined(HAS_TRANSPOSEWXH_32_RVV)
  if (TransposeWxH_32) {
    TransposeWxH_32(src_argb, src_stride_argb, dst_argb, dst_stride_argb, width,
                    height);
    return 0;
  }
#endif
#if defined(HAS_TRANSPOSEWX16_32_AVX512BW)
  if (TransposeWx16_32) {
    while (height >= 16) {
      TransposeWx16_32(src_argb, src_stride_argb, dst_argb, dst_stride_argb,
                       width);
      src_argb += 16 * (ptrdiff_t)src_stride_argb;
      dst_argb += 16 * 4;
      height -= 16;
    }
  }
#endif
#if defined(HAS_TRANSPOSEWX8_32_AVX2)
  if (TransposeWx8_32) {
    while (height >= 8) {
      TransposeWx8_32(src_argb, src_stride_argb, dst_argb, dst_stride_argb,
                      width);
      src_argb += 8 * (ptrdiff_t)src_stride_argb;
      dst_argb += 8 * 4;
      height -= 8;
    }
  }
#endif
  if (height > 0) {
    if (TransposeWx4_32) {
      while (width >= 4) {
        TransposeWx4_32(src_argb, src_stride_argb, dst_argb, dst_stride_argb,
                        height);
        src_argb += 4 * 4;
        dst_argb += 4 * (ptrdiff_t)dst_stride_argb;
        width -= 4;
      }
    }
    if (width > 0) {
      TransposeWxH_32_C(src_argb, src_stride_argb, dst_argb, dst_stride_argb,
                        width, height);
    }
  }
  return 0;
}

static int ARGBRotate90(const uint8_t* src_argb,
                        int src_stride_argb,
                        uint8_t* dst_argb,
                        int dst_stride_argb,
                        int width,
                        int height) {
  // Rotate by 90 is a ARGBTranspose with the source read
  // from bottom to top. So set the source pointer to the end
  // of the buffer and flip the sign of the source stride.
  src_argb += (ptrdiff_t)src_stride_argb * (height - 1);
  src_stride_argb = -src_stride_argb;
  return ARGBTranspose(src_argb, src_stride_argb, dst_argb, dst_stride_argb,
                       width, height);
}

static int ARGBRotate270(const uint8_t* src_argb,
                         int src_stride_argb,
                         uint8_t* dst_argb,
                         int dst_stride_argb,
                         int width,
                         int height) {
  // Rotate by 270 is a ARGBTranspose with the destination written
  // from bottom to top. So set the destination pointer to the end
  // of the buffer and flip the sign of the destination stride.
  dst_argb += (ptrdiff_t)dst_stride_argb * (width - 1);
  dst_stride_argb = -dst_stride_argb;
  return ARGBTranspose(src_argb, src_stride_argb, dst_argb, dst_stride_argb,
                       width, height);
}

static int ARGBRotate180(const uint8_t* src_argb,
                         int src_stride_argb,
                         uint8_t* dst_argb,
                         int dst_stride_argb,
                         int width,
                         int height) {
  // Rotate by 180 is a vertical flip (negative height) and horizontal mirror.
  return ARGBMirror(src_argb, src_stride_argb, dst_argb, dst_stride_argb, width,
                    -height);
}

LIBYUV_API
int ARGBRotate(const uint8_t* src_argb,
               int src_stride_argb,
               uint8_t* dst_argb,
               int dst_stride_argb,
               int width,
               int height,
               enum RotationMode mode) {
  if (!src_argb || width <= 0 || height == 0 || height == INT_MIN ||
      !dst_argb) {
    return -1;
  }

  // Negative height means invert the image.
  if (height < 0) {
    height = -height;
    src_argb = src_argb + (ptrdiff_t)(height - 1) * src_stride_argb;
    src_stride_argb = -src_stride_argb;
  }

  switch (mode) {
    case kRotate0:
      // copy frame
      return ARGBCopy(src_argb, src_stride_argb, dst_argb, dst_stride_argb,
                      width, height);
    case kRotate90:
      return ARGBRotate90(src_argb, src_stride_argb, dst_argb, dst_stride_argb,
                          width, height);
    case kRotate270:
      return ARGBRotate270(src_argb, src_stride_argb, dst_argb, dst_stride_argb,
                           width, height);
    case kRotate180:
      return ARGBRotate180(src_argb, src_stride_argb, dst_argb, dst_stride_argb,
                           width, height);
    default:
      break;
  }
  return -1;
}

#ifdef __cplusplus
}  // extern "C"
}  // namespace libyuv
#endif
