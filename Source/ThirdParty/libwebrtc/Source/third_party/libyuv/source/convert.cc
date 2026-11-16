/*
 *  Copyright 2011 The LibYuv Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS. All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "libyuv/convert.h"

#include <limits.h>

#include "libyuv/convert_from_argb.h"
#include "libyuv/cpu_id.h"
#include "libyuv/planar_functions.h"
#include "libyuv/rotate.h"
#include "libyuv/row.h"
#include "libyuv/scale.h"      // For ScalePlane()
#include "libyuv/scale_row.h"  // For FixedDiv
#include "libyuv/scale_uv.h"   // For UVScale()

#ifdef __cplusplus
namespace libyuv {

extern const struct ArgbConstants kArgbI601Constants;
extern const struct ArgbConstants kArgbJPEGConstants;
extern "C" {
#endif

// Subsample amount uses a shift.
//   v is value
//   a is amount to add to round up
//   s is shift to subsample down
#define SUBSAMPLE(v, a, s) (v < 0) ? (-((-v + a) >> s)) : ((v + a) >> s)
static __inline int Abs(int v) {
  return v >= 0 ? v : -v;
}

// Any I4xx To I420 format
static int I4xxToI420(const uint8_t* src_y,
                      int src_stride_y,
                      const uint8_t* src_u,
                      int src_stride_u,
                      const uint8_t* src_v,
                      int src_stride_v,
                      uint8_t* dst_y,
                      int dst_stride_y,
                      uint8_t* dst_u,
                      int dst_stride_u,
                      uint8_t* dst_v,
                      int dst_stride_v,
                      int src_y_width,
                      int src_y_height,
                      int src_uv_width,
                      int src_uv_height) {
  int r;
  if ((!src_y && dst_y) || !src_u || !src_v || !dst_u || !dst_v ||
      src_y_width <= 0 || src_y_height == 0 || src_y_height == INT_MIN ||
      src_uv_width <= 0 || src_uv_height == 0) {
    return -1;
  }
  const int dst_y_width = src_y_width;
  const int dst_y_height = Abs(src_y_height);
  const int dst_uv_width = SUBSAMPLE(dst_y_width, 1, 1);
  const int dst_uv_height = SUBSAMPLE(dst_y_height, 1, 1);
  if (dst_y) {
    CopyPlane(src_y, src_stride_y, dst_y, dst_stride_y, src_y_width,
              src_y_height);
  }
  r = ScalePlane(src_u, src_stride_u, src_uv_width, src_uv_height, dst_u,
                 dst_stride_u, dst_uv_width, dst_uv_height, kFilterBilinear);
  if (r != 0) {
    return r;
  }
  r = ScalePlane(src_v, src_stride_v, src_uv_width, src_uv_height, dst_v,
                 dst_stride_v, dst_uv_width, dst_uv_height, kFilterBilinear);
  return r;
}

// Copy I420 with optional vertical flipping using negative height.
LIBYUV_API
int I420Copy(const uint8_t* src_y,
             int src_stride_y,
             const uint8_t* src_u,
             int src_stride_u,
             const uint8_t* src_v,
             int src_stride_v,
             uint8_t* dst_y,
             int dst_stride_y,
             uint8_t* dst_u,
             int dst_stride_u,
             uint8_t* dst_v,
             int dst_stride_v,
             int width,
             int height) {
  int halfwidth = (width + 1) >> 1;
  int halfheight = (height + 1) >> 1;
  if ((!src_y && dst_y) || !src_u || !src_v || !dst_u || !dst_v || width <= 0 ||
      height == 0 || height == INT_MIN) {
    return -1;
  }
  // Negative height means invert the image.
  if (height < 0) {
    height = -height;
    halfheight = (height + 1) >> 1;
    src_y = src_y + (ptrdiff_t)(height - 1) * src_stride_y;
    src_u = src_u + (ptrdiff_t)(halfheight - 1) * src_stride_u;
    src_v = src_v + (ptrdiff_t)(halfheight - 1) * src_stride_v;
    src_stride_y = -src_stride_y;
    src_stride_u = -src_stride_u;
    src_stride_v = -src_stride_v;
  }

  if (dst_y) {
    CopyPlane(src_y, src_stride_y, dst_y, dst_stride_y, width, height);
  }
  // Copy UV planes.
  CopyPlane(src_u, src_stride_u, dst_u, dst_stride_u, halfwidth, halfheight);
  CopyPlane(src_v, src_stride_v, dst_v, dst_stride_v, halfwidth, halfheight);
  return 0;
}

// Copy I010 with optional flipping.
LIBYUV_API
int I010Copy(const uint16_t* src_y,
             int src_stride_y,
             const uint16_t* src_u,
             int src_stride_u,
             const uint16_t* src_v,
             int src_stride_v,
             uint16_t* dst_y,
             int dst_stride_y,
             uint16_t* dst_u,
             int dst_stride_u,
             uint16_t* dst_v,
             int dst_stride_v,
             int width,
             int height) {
  int halfwidth = (width + 1) >> 1;
  int halfheight = (height + 1) >> 1;
  if ((!src_y && dst_y) || !src_u || !src_v || !dst_u || !dst_v || width <= 0 ||
      height == 0 || height == INT_MIN) {
    return -1;
  }
  // Negative height means invert the image.
  if (height < 0) {
    height = -height;
    halfheight = (height + 1) >> 1;
    src_y = src_y + (ptrdiff_t)(height - 1) * src_stride_y;
    src_u = src_u + (ptrdiff_t)(halfheight - 1) * src_stride_u;
    src_v = src_v + (ptrdiff_t)(halfheight - 1) * src_stride_v;
    src_stride_y = -src_stride_y;
    src_stride_u = -src_stride_u;
    src_stride_v = -src_stride_v;
  }

  if (dst_y) {
    CopyPlane_16(src_y, src_stride_y, dst_y, dst_stride_y, width, height);
  }
  // Copy UV planes.
  CopyPlane_16(src_u, src_stride_u, dst_u, dst_stride_u, halfwidth, halfheight);
  CopyPlane_16(src_v, src_stride_v, dst_v, dst_stride_v, halfwidth, halfheight);
  return 0;
}

static int Planar16bitTo8bit(const uint16_t* src_y,
                             int src_stride_y,
                             const uint16_t* src_u,
                             int src_stride_u,
                             const uint16_t* src_v,
                             int src_stride_v,
                             uint8_t* dst_y,
                             int dst_stride_y,
                             uint8_t* dst_u,
                             int dst_stride_u,
                             uint8_t* dst_v,
                             int dst_stride_v,
                             int width,
                             int height,
                             int subsample_x,
                             int subsample_y,
                             int depth) {
  if ((!src_y && dst_y) || !src_u || !src_v || !dst_u || !dst_v || width <= 0 ||
      height == 0 || height == INT_MIN) {
    return -1;
  }
  int uv_width = SUBSAMPLE(width, subsample_x, subsample_x);
  int uv_height = SUBSAMPLE(height, subsample_y, subsample_y);
  int scale = 1 << (24 - depth);
  // Negative height means invert the image.
  if (height < 0) {
    height = -height;
    uv_height = -uv_height;
    src_y = src_y + (ptrdiff_t)(height - 1) * src_stride_y;
    src_u = src_u + (ptrdiff_t)(uv_height - 1) * src_stride_u;
    src_v = src_v + (ptrdiff_t)(uv_height - 1) * src_stride_v;
    src_stride_y = -src_stride_y;
    src_stride_u = -src_stride_u;
    src_stride_v = -src_stride_v;
  }

  // Convert Y plane.
  if (dst_y) {
    Convert16To8Plane(src_y, src_stride_y, dst_y, dst_stride_y, scale, width,
                      height);
  }
  // Convert UV planes.
  Convert16To8Plane(src_u, src_stride_u, dst_u, dst_stride_u, scale, uv_width,
                    uv_height);
  Convert16To8Plane(src_v, src_stride_v, dst_v, dst_stride_v, scale, uv_width,
                    uv_height);
  return 0;
}

static int I41xToI420(const uint16_t* src_y,
                      int src_stride_y,
                      const uint16_t* src_u,
                      int src_stride_u,
                      const uint16_t* src_v,
                      int src_stride_v,
                      uint8_t* dst_y,
                      int dst_stride_y,
                      uint8_t* dst_u,
                      int dst_stride_u,
                      uint8_t* dst_v,
                      int dst_stride_v,
                      int width,
                      int height,
                      int depth) {
  int y;
  const int scale = 1 << (24 - depth);
  const int uv_width = SUBSAMPLE(width, 1, 1);
  void (*Convert16To8Row)(const uint16_t* src_y, uint8_t* dst_y, int scale,
                          int width) = Convert16To8Row_C;
  void (*HalfWidthRow_16To8)(const uint16_t* src_uv, ptrdiff_t src_uv_stride,
                             uint8_t* dst_uv, int scale, int width) =
      (width & 1) ? HalfWidthRow_16To8_Odd_C : HalfWidthRow_16To8_C;

  if ((!src_y && dst_y) || !src_u || !src_v || !dst_u || !dst_v || width <= 0 ||
      height == 0 || height == INT_MIN) {
    return -1;
  }
  // Negative height means invert the image.
  if (height < 0) {
    height = -height;
    if (src_y) {
      src_y = src_y + (ptrdiff_t)(height - 1) * src_stride_y;
      src_stride_y = -src_stride_y;
    }
    src_u = src_u + (ptrdiff_t)(height - 1) * src_stride_u;
    src_v = src_v + (ptrdiff_t)(height - 1) * src_stride_v;
    src_stride_u = -src_stride_u;
    src_stride_v = -src_stride_v;
  }

#if defined(HAS_CONVERT16TO8ROW_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    Convert16To8Row = Convert16To8Row_Any_NEON;
    if (IS_ALIGNED(width, 16)) {
      Convert16To8Row = Convert16To8Row_NEON;
    }
  }
#endif
#if defined(HAS_CONVERT16TO8ROW_SVE2)
  if (TestCpuFlag(kCpuHasSVE2)) {
    Convert16To8Row = Convert16To8Row_SVE2;
  }
#endif
#if defined(HAS_CONVERT16TO8ROW_SME)
  if (TestCpuFlag(kCpuHasSME)) {
    Convert16To8Row = Convert16To8Row_SME;
  }
#endif
#if defined(HAS_CONVERT16TO8ROW_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3)) {
    Convert16To8Row = Convert16To8Row_Any_SSSE3;
    if (IS_ALIGNED(width, 16)) {
      Convert16To8Row = Convert16To8Row_SSSE3;
    }
  }
#endif
#if defined(HAS_CONVERT16TO8ROW_AVX2)
  if (TestCpuFlag(kCpuHasAVX2)) {
    Convert16To8Row = Convert16To8Row_Any_AVX2;
    if (IS_ALIGNED(width, 32)) {
      Convert16To8Row = Convert16To8Row_AVX2;
    }
  }
#endif
#if defined(HAS_CONVERT16TO8ROW_AVX512BW)
  if (TestCpuFlag(kCpuHasAVX512BW)) {
    Convert16To8Row = Convert16To8Row_Any_AVX512BW;
    if (IS_ALIGNED(width, 64)) {
      Convert16To8Row = Convert16To8Row_AVX512BW;
    }
  }
#endif
#if defined(HAS_CONVERT16TO8ROW_RVV)
  if (TestCpuFlag(kCpuHasRVV)) {
    Convert16To8Row = Convert16To8Row_RVV;
  }
#endif

  if (IS_ALIGNED(width, 2)) {
#if defined(HAS_HALFWIDTHROW_16TO8_NEON)
    if (TestCpuFlag(kCpuHasNEON)) {
      HalfWidthRow_16To8 = HalfWidthRow_16To8_Any_NEON;
      if (IS_ALIGNED(uv_width, 8)) {
        HalfWidthRow_16To8 = HalfWidthRow_16To8_NEON;
      }
    }
#endif
#if defined(HAS_HALFWIDTHROW_16TO8_SVE2)
    if (TestCpuFlag(kCpuHasSVE2)) {
      HalfWidthRow_16To8 = HalfWidthRow_16To8_SVE2;
    }
#endif
#if defined(HAS_HALFWIDTHROW_16TO8_SME)
    if (TestCpuFlag(kCpuHasSME)) {
      HalfWidthRow_16To8 = HalfWidthRow_16To8_SME;
    }
#endif
#if defined(HAS_HALFWIDTHROW_16TO8_SSSE3)
    if (TestCpuFlag(kCpuHasSSSE3)) {
      HalfWidthRow_16To8 = HalfWidthRow_16To8_Any_SSSE3;
      if (IS_ALIGNED(uv_width, 8)) {
        HalfWidthRow_16To8 = HalfWidthRow_16To8_SSSE3;
      }
    }
#endif
#if defined(HAS_HALFWIDTHROW_16TO8_AVX2)
    if (TestCpuFlag(kCpuHasAVX2)) {
      HalfWidthRow_16To8 = HalfWidthRow_16To8_Any_AVX2;
      if (IS_ALIGNED(uv_width, 16)) {
        HalfWidthRow_16To8 = HalfWidthRow_16To8_AVX2;
      }
    }
#endif
#if defined(HAS_HALFWIDTHROW_16TO8_AVX512BW)
    if (TestCpuFlag(kCpuHasAVX512BW)) {
      HalfWidthRow_16To8 = HalfWidthRow_16To8_Any_AVX512BW;
      if (IS_ALIGNED(uv_width, 32)) {
        HalfWidthRow_16To8 = HalfWidthRow_16To8_AVX512BW;
      }
    }
#endif
#if defined(HAS_HALFWIDTHROW_16TO8_RVV)
    if (TestCpuFlag(kCpuHasRVV)) {
      HalfWidthRow_16To8 = HalfWidthRow_16To8_RVV;
    }
#endif
  }

  for (y = 0; y < height - 1; y += 2) {
    HalfWidthRow_16To8(src_u, src_stride_u, dst_u, scale, uv_width);
    HalfWidthRow_16To8(src_v, src_stride_v, dst_v, scale, uv_width);
    if (dst_y) {
      Convert16To8Row(src_y, dst_y, scale, width);
      Convert16To8Row(src_y + src_stride_y, dst_y + dst_stride_y, scale, width);
      src_y += src_stride_y * 2;
      dst_y += dst_stride_y * 2;
    }
    src_u += src_stride_u * 2;
    src_v += src_stride_v * 2;
    dst_u += dst_stride_u;
    dst_v += dst_stride_v;
  }
  if (height & 1) {
    HalfWidthRow_16To8(src_u, 0, dst_u, scale, uv_width);
    HalfWidthRow_16To8(src_v, 0, dst_v, scale, uv_width);
    if (dst_y) {
      Convert16To8Row(src_y, dst_y, scale, width);
    }
  }
  return 0;
}

static int I21xToI420(const uint16_t* src_y,
                      int src_stride_y,
                      const uint16_t* src_u,
                      int src_stride_u,
                      const uint16_t* src_v,
                      int src_stride_v,
                      uint8_t* dst_y,
                      int dst_stride_y,
                      uint8_t* dst_u,
                      int dst_stride_u,
                      uint8_t* dst_v,
                      int dst_stride_v,
                      int width,
                      int height,
                      int depth) {
  int y;
  const int scale = 1 << (24 - depth);
  const int uv_width = SUBSAMPLE(width, 1, 1);
  void (*Convert16To8Row)(const uint16_t* src_y, uint8_t* dst_y, int scale,
                          int width) = Convert16To8Row_C;
  void (*HalfRow_16To8)(const uint16_t* src_uv, ptrdiff_t src_uv_stride,
                        uint8_t* dst_uv, int scale, int width) =
      HalfRow_16To8_C;

  if ((!src_y && dst_y) || !src_u || !src_v || !dst_u || !dst_v || width <= 0 ||
      height == 0 || height == INT_MIN) {
    return -1;
  }
  // Negative height means invert the image.
  if (height < 0) {
    height = -height;
    if (src_y) {
      src_y = src_y + (ptrdiff_t)(height - 1) * src_stride_y;
      src_stride_y = -src_stride_y;
    }
    src_u = src_u + (ptrdiff_t)(height - 1) * src_stride_u;
    src_v = src_v + (ptrdiff_t)(height - 1) * src_stride_v;
    src_stride_u = -src_stride_u;
    src_stride_v = -src_stride_v;
  }

#if defined(HAS_CONVERT16TO8ROW_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    Convert16To8Row = Convert16To8Row_Any_NEON;
    if (IS_ALIGNED(width, 16)) {
      Convert16To8Row = Convert16To8Row_NEON;
    }
  }
#endif
#if defined(HAS_CONVERT16TO8ROW_SVE2)
  if (TestCpuFlag(kCpuHasSVE2)) {
    Convert16To8Row = Convert16To8Row_SVE2;
  }
#endif
#if defined(HAS_CONVERT16TO8ROW_SME)
  if (TestCpuFlag(kCpuHasSME)) {
    Convert16To8Row = Convert16To8Row_SME;
  }
#endif
#if defined(HAS_CONVERT16TO8ROW_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3)) {
    Convert16To8Row = Convert16To8Row_Any_SSSE3;
    if (IS_ALIGNED(width, 16)) {
      Convert16To8Row = Convert16To8Row_SSSE3;
    }
  }
#endif
#if defined(HAS_CONVERT16TO8ROW_AVX2)
  if (TestCpuFlag(kCpuHasAVX2)) {
    Convert16To8Row = Convert16To8Row_Any_AVX2;
    if (IS_ALIGNED(width, 32)) {
      Convert16To8Row = Convert16To8Row_AVX2;
    }
  }
#endif
#if defined(HAS_CONVERT16TO8ROW_AVX512BW)
  if (TestCpuFlag(kCpuHasAVX512BW)) {
    Convert16To8Row = Convert16To8Row_Any_AVX512BW;
    if (IS_ALIGNED(width, 64)) {
      Convert16To8Row = Convert16To8Row_AVX512BW;
    }
  }
#endif
#if defined(HAS_CONVERT16TO8ROW_RVV)
  if (TestCpuFlag(kCpuHasRVV)) {
    Convert16To8Row = Convert16To8Row_RVV;
  }
#endif

#if defined(HAS_HALFROW_16TO8_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    HalfRow_16To8 = HalfRow_16To8_Any_NEON;
    if (IS_ALIGNED(uv_width, 16)) {
      HalfRow_16To8 = HalfRow_16To8_NEON;
    }
  }
#endif
#if defined(HAS_HALFROW_16TO8_SVE2)
  if (TestCpuFlag(kCpuHasSVE2)) {
    HalfRow_16To8 = HalfRow_16To8_SVE2;
  }
#endif
#if defined(HAS_HALFROW_16TO8_SME)
  if (TestCpuFlag(kCpuHasSME)) {
    HalfRow_16To8 = HalfRow_16To8_SME;
  }
#endif
#if defined(HAS_HALFROW_16TO8_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3)) {
    HalfRow_16To8 = HalfRow_16To8_Any_SSSE3;
    if (IS_ALIGNED(uv_width, 16)) {
      HalfRow_16To8 = HalfRow_16To8_SSSE3;
    }
  }
#endif
#if defined(HAS_HALFROW_16TO8_AVX2)
  if (TestCpuFlag(kCpuHasAVX2)) {
    HalfRow_16To8 = HalfRow_16To8_Any_AVX2;
    if (IS_ALIGNED(uv_width, 32)) {
      HalfRow_16To8 = HalfRow_16To8_AVX2;
    }
  }
#endif
#if defined(HAS_HALFROW_16TO8_AVX512BW)
  if (TestCpuFlag(kCpuHasAVX512BW)) {
    HalfRow_16To8 = HalfRow_16To8_Any_AVX512BW;
    if (IS_ALIGNED(uv_width, 64)) {
      HalfRow_16To8 = HalfRow_16To8_AVX512BW;
    }
  }
#endif
#if defined(HAS_HALFROW_16TO8_RVV)
  if (TestCpuFlag(kCpuHasRVV)) {
    HalfRow_16To8 = HalfRow_16To8_RVV;
  }
#endif

  for (y = 0; y < height - 1; y += 2) {
    HalfRow_16To8(src_u, src_stride_u, dst_u, scale, uv_width);
    HalfRow_16To8(src_v, src_stride_v, dst_v, scale, uv_width);
    if (dst_y) {
      Convert16To8Row(src_y, dst_y, scale, width);
      Convert16To8Row(src_y + src_stride_y, dst_y + dst_stride_y, scale, width);
      src_y += src_stride_y * 2;
      dst_y += dst_stride_y * 2;
    }
    src_u += src_stride_u * 2;
    src_v += src_stride_v * 2;
    dst_u += dst_stride_u;
    dst_v += dst_stride_v;
  }
  if (height & 1) {
    HalfRow_16To8(src_u, 0, dst_u, scale, uv_width);
    HalfRow_16To8(src_v, 0, dst_v, scale, uv_width);
    if (dst_y) {
      Convert16To8Row(src_y, dst_y, scale, width);
    }
  }
  return 0;
}

// Convert 10 bit YUV to 8 bit.
LIBYUV_API
int I010ToI420(const uint16_t* src_y,
               int src_stride_y,
               const uint16_t* src_u,
               int src_stride_u,
               const uint16_t* src_v,
               int src_stride_v,
               uint8_t* dst_y,
               int dst_stride_y,
               uint8_t* dst_u,
               int dst_stride_u,
               uint8_t* dst_v,
               int dst_stride_v,
               int width,
               int height) {
  return Planar16bitTo8bit(src_y, src_stride_y, src_u, src_stride_u, src_v,
                           src_stride_v, dst_y, dst_stride_y, dst_u,
                           dst_stride_u, dst_v, dst_stride_v, width, height, 1,
                           1, 10);
}

LIBYUV_API
int I210ToI420(const uint16_t* src_y,
               int src_stride_y,
               const uint16_t* src_u,
               int src_stride_u,
               const uint16_t* src_v,
               int src_stride_v,
               uint8_t* dst_y,
               int dst_stride_y,
               uint8_t* dst_u,
               int dst_stride_u,
               uint8_t* dst_v,
               int dst_stride_v,
               int width,
               int height) {
  return I21xToI420(src_y, src_stride_y, src_u, src_stride_u, src_v,
                    src_stride_v, dst_y, dst_stride_y, dst_u, dst_stride_u,
                    dst_v, dst_stride_v, width, height, 10);
}

LIBYUV_API
int I210ToI422(const uint16_t* src_y,
               int src_stride_y,
               const uint16_t* src_u,
               int src_stride_u,
               const uint16_t* src_v,
               int src_stride_v,
               uint8_t* dst_y,
               int dst_stride_y,
               uint8_t* dst_u,
               int dst_stride_u,
               uint8_t* dst_v,
               int dst_stride_v,
               int width,
               int height) {
  return Planar16bitTo8bit(src_y, src_stride_y, src_u, src_stride_u, src_v,
                           src_stride_v, dst_y, dst_stride_y, dst_u,
                           dst_stride_u, dst_v, dst_stride_v, width, height, 1,
                           0, 10);
}

LIBYUV_API
int I410ToI420(const uint16_t* src_y,
               int src_stride_y,
               const uint16_t* src_u,
               int src_stride_u,
               const uint16_t* src_v,
               int src_stride_v,
               uint8_t* dst_y,
               int dst_stride_y,
               uint8_t* dst_u,
               int dst_stride_u,
               uint8_t* dst_v,
               int dst_stride_v,
               int width,
               int height) {
  return I41xToI420(src_y, src_stride_y, src_u, src_stride_u, src_v,
                    src_stride_v, dst_y, dst_stride_y, dst_u, dst_stride_u,
                    dst_v, dst_stride_v, width, height, 10);
}

LIBYUV_API
int I410ToI444(const uint16_t* src_y,
               int src_stride_y,
               const uint16_t* src_u,
               int src_stride_u,
               const uint16_t* src_v,
               int src_stride_v,
               uint8_t* dst_y,
               int dst_stride_y,
               uint8_t* dst_u,
               int dst_stride_u,
               uint8_t* dst_v,
               int dst_stride_v,
               int width,
               int height) {
  return Planar16bitTo8bit(src_y, src_stride_y, src_u, src_stride_u, src_v,
                           src_stride_v, dst_y, dst_stride_y, dst_u,
                           dst_stride_u, dst_v, dst_stride_v, width, height, 0,
                           0, 10);
}

LIBYUV_API
int I012ToI420(const uint16_t* src_y,
               int src_stride_y,
               const uint16_t* src_u,
               int src_stride_u,
               const uint16_t* src_v,
               int src_stride_v,
               uint8_t* dst_y,
               int dst_stride_y,
               uint8_t* dst_u,
               int dst_stride_u,
               uint8_t* dst_v,
               int dst_stride_v,
               int width,
               int height) {
  return Planar16bitTo8bit(src_y, src_stride_y, src_u, src_stride_u, src_v,
                           src_stride_v, dst_y, dst_stride_y, dst_u,
                           dst_stride_u, dst_v, dst_stride_v, width, height, 1,
                           1, 12);
}

LIBYUV_API
int I212ToI422(const uint16_t* src_y,
               int src_stride_y,
               const uint16_t* src_u,
               int src_stride_u,
               const uint16_t* src_v,
               int src_stride_v,
               uint8_t* dst_y,
               int dst_stride_y,
               uint8_t* dst_u,
               int dst_stride_u,
               uint8_t* dst_v,
               int dst_stride_v,
               int width,
               int height) {
  return Planar16bitTo8bit(src_y, src_stride_y, src_u, src_stride_u, src_v,
                           src_stride_v, dst_y, dst_stride_y, dst_u,
                           dst_stride_u, dst_v, dst_stride_v, width, height, 1,
                           0, 12);
}

LIBYUV_API
int I212ToI420(const uint16_t* src_y,
               int src_stride_y,
               const uint16_t* src_u,
               int src_stride_u,
               const uint16_t* src_v,
               int src_stride_v,
               uint8_t* dst_y,
               int dst_stride_y,
               uint8_t* dst_u,
               int dst_stride_u,
               uint8_t* dst_v,
               int dst_stride_v,
               int width,
               int height) {
  return I21xToI420(src_y, src_stride_y, src_u, src_stride_u, src_v,
                    src_stride_v, dst_y, dst_stride_y, dst_u, dst_stride_u,
                    dst_v, dst_stride_v, width, height, 12);
}

LIBYUV_API
int I412ToI444(const uint16_t* src_y,
               int src_stride_y,
               const uint16_t* src_u,
               int src_stride_u,
               const uint16_t* src_v,
               int src_stride_v,
               uint8_t* dst_y,
               int dst_stride_y,
               uint8_t* dst_u,
               int dst_stride_u,
               uint8_t* dst_v,
               int dst_stride_v,
               int width,
               int height) {
  return Planar16bitTo8bit(src_y, src_stride_y, src_u, src_stride_u, src_v,
                           src_stride_v, dst_y, dst_stride_y, dst_u,
                           dst_stride_u, dst_v, dst_stride_v, width, height, 0,
                           0, 12);
}

LIBYUV_API
int I412ToI420(const uint16_t* src_y,
               int src_stride_y,
               const uint16_t* src_u,
               int src_stride_u,
               const uint16_t* src_v,
               int src_stride_v,
               uint8_t* dst_y,
               int dst_stride_y,
               uint8_t* dst_u,
               int dst_stride_u,
               uint8_t* dst_v,
               int dst_stride_v,
               int width,
               int height) {
  return I41xToI420(src_y, src_stride_y, src_u, src_stride_u, src_v,
                    src_stride_v, dst_y, dst_stride_y, dst_u, dst_stride_u,
                    dst_v, dst_stride_v, width, height, 12);
}

// Any Ix10 To I010 format
static int Ix10ToI010(const uint16_t* src_y,
                      int src_stride_y,
                      const uint16_t* src_u,
                      int src_stride_u,
                      const uint16_t* src_v,
                      int src_stride_v,
                      uint16_t* dst_y,
                      int dst_stride_y,
                      uint16_t* dst_u,
                      int dst_stride_u,
                      uint16_t* dst_v,
                      int dst_stride_v,
                      int width,
                      int height,
                      int subsample_x,
                      int subsample_y) {
  int r;
  if ((!src_y && dst_y) || !src_u || !src_v || !dst_u || !dst_v || width <= 0 ||
      height == 0 || height == INT_MIN) {
    return -1;
  }
  const int dst_y_width = width;
  const int dst_y_height = Abs(height);
  const int src_uv_width = SUBSAMPLE(width, subsample_x, subsample_x);
  const int src_uv_height = SUBSAMPLE(height, subsample_y, subsample_y);
  const int dst_uv_width = SUBSAMPLE(dst_y_width, 1, 1);
  const int dst_uv_height = SUBSAMPLE(dst_y_height, 1, 1);
  if (dst_y) {
    CopyPlane_16(src_y, src_stride_y, dst_y, dst_stride_y, width, height);
  }
  r = ScalePlane_12(src_u, src_stride_u, src_uv_width, src_uv_height, dst_u,
                    dst_stride_u, dst_uv_width, dst_uv_height, kFilterBilinear);
  if (r != 0) {
    return r;
  }
  r = ScalePlane_12(src_v, src_stride_v, src_uv_width, src_uv_height, dst_v,
                    dst_stride_v, dst_uv_width, dst_uv_height, kFilterBilinear);
  return r;
}

LIBYUV_API
int I410ToI010(const uint16_t* src_y,
               int src_stride_y,
               const uint16_t* src_u,
               int src_stride_u,
               const uint16_t* src_v,
               int src_stride_v,
               uint16_t* dst_y,
               int dst_stride_y,
               uint16_t* dst_u,
               int dst_stride_u,
               uint16_t* dst_v,
               int dst_stride_v,
               int width,
               int height) {
  return Ix10ToI010(src_y, src_stride_y, src_u, src_stride_u, src_v,
                    src_stride_v, dst_y, dst_stride_y, dst_u, dst_stride_u,
                    dst_v, dst_stride_v, width, height, 0, 0);
}

LIBYUV_API
int I210ToI010(const uint16_t* src_y,
               int src_stride_y,
               const uint16_t* src_u,
               int src_stride_u,
               const uint16_t* src_v,
               int src_stride_v,
               uint16_t* dst_y,
               int dst_stride_y,
               uint16_t* dst_u,
               int dst_stride_u,
               uint16_t* dst_v,
               int dst_stride_v,
               int width,
               int height) {
  return Ix10ToI010(src_y, src_stride_y, src_u, src_stride_u, src_v,
                    src_stride_v, dst_y, dst_stride_y, dst_u, dst_stride_u,
                    dst_v, dst_stride_v, width, height, 1, 0);
}

// Any I[420]1[02] to P[420]1[02] format
static int IxxxToPxxx(const uint16_t* src_y,
                      int src_stride_y,
                      const uint16_t* src_u,
                      int src_stride_u,
                      const uint16_t* src_v,
                      int src_stride_v,
                      uint16_t* dst_y,
                      int dst_stride_y,
                      uint16_t* dst_uv,
                      int dst_stride_uv,
                      int width,
                      int height,
                      int subsample_x,
                      int subsample_y,
                      int depth) {
  if (width <= 0 || height == 0 || height == INT_MIN) {
    return -1;
  }
  const int uv_width = SUBSAMPLE(width, subsample_x, subsample_x);
  const int uv_height = SUBSAMPLE(height, subsample_y, subsample_y);

  ConvertToMSBPlane_16(src_y, src_stride_y, dst_y, dst_stride_y, width, height,
                       depth);
  MergeUVPlane_16(src_u, src_stride_u, src_v, src_stride_v, dst_uv,
                  dst_stride_uv, uv_width, uv_height, depth);
  return 0;
}

LIBYUV_API
int I010ToP010(const uint16_t* src_y,
               int src_stride_y,
               const uint16_t* src_u,
               int src_stride_u,
               const uint16_t* src_v,
               int src_stride_v,
               uint16_t* dst_y,
               int dst_stride_y,
               uint16_t* dst_uv,
               int dst_stride_uv,
               int width,
               int height) {
  return IxxxToPxxx(src_y, src_stride_y, src_u, src_stride_u, src_v,
                    src_stride_v, dst_y, dst_stride_y, dst_uv, dst_stride_uv,
                    width, height, 1, 1, 10);
}

LIBYUV_API
int I010ToNV12(const uint16_t* src_y,
               int src_stride_y,
               const uint16_t* src_u,
               int src_stride_u,
               const uint16_t* src_v,
               int src_stride_v,
               uint8_t* dst_y,
               int dst_stride_y,
               uint8_t* dst_uv,
               int dst_stride_uv,
               int width,
               int height) {
  int y;
  int halfwidth = (width + 1) >> 1;
  int halfheight = (height + 1) >> 1;
  const int scale = 16385;  // 16384 for 10 bits
  void (*Convert16To8Row)(const uint16_t* src_y, uint8_t* dst_y, int scale,
                          int width) = Convert16To8Row_C;
  void (*MergeUVRow)(const uint8_t* src_u, const uint8_t* src_v,
                     uint8_t* dst_uv, int width) = MergeUVRow_C;
  if ((!src_y && dst_y) || !src_u || !src_v || !dst_uv || width <= 0 ||
      height == 0 || height == INT_MIN) {
    return -1;
  }
  // Negative height means invert the image.
  if (height < 0) {
    height = -height;
    halfheight = (height + 1) >> 1;
    src_y = src_y + (ptrdiff_t)(height - 1) * src_stride_y;
    src_u = src_u + (ptrdiff_t)(halfheight - 1) * src_stride_u;
    src_v = src_v + (ptrdiff_t)(halfheight - 1) * src_stride_v;
    src_stride_y = -src_stride_y;
    src_stride_u = -src_stride_u;
    src_stride_v = -src_stride_v;
  }
#if defined(HAS_CONVERT16TO8ROW_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    Convert16To8Row = Convert16To8Row_Any_NEON;
    if (IS_ALIGNED(halfwidth, 16)) {
      Convert16To8Row = Convert16To8Row_NEON;
    }
  }
#endif
#if defined(HAS_CONVERT16TO8ROW_SME)
  if (TestCpuFlag(kCpuHasSME)) {
    Convert16To8Row = Convert16To8Row_SME;
  }
#endif
#if defined(HAS_CONVERT16TO8ROW_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3)) {
    Convert16To8Row = Convert16To8Row_Any_SSSE3;
    if (IS_ALIGNED(halfwidth, 16)) {
      Convert16To8Row = Convert16To8Row_SSSE3;
    }
  }
#endif
#if defined(HAS_CONVERT16TO8ROW_AVX2)
  if (TestCpuFlag(kCpuHasAVX2)) {
    Convert16To8Row = Convert16To8Row_Any_AVX2;
    if (IS_ALIGNED(halfwidth, 32)) {
      Convert16To8Row = Convert16To8Row_AVX2;
    }
  }
#endif
#if defined(HAS_CONVERT16TO8ROW_AVX512BW)
  if (TestCpuFlag(kCpuHasAVX512BW)) {
    Convert16To8Row = Convert16To8Row_Any_AVX512BW;
    if (IS_ALIGNED(halfwidth, 64)) {
      Convert16To8Row = Convert16To8Row_AVX512BW;
    }
  }
#endif

#if defined(HAS_MERGEUVROW_SSE2)
  if (TestCpuFlag(kCpuHasSSE2)) {
    MergeUVRow = MergeUVRow_Any_SSE2;
    if (IS_ALIGNED(halfwidth, 16)) {
      MergeUVRow = MergeUVRow_SSE2;
    }
  }
#endif
#if defined(HAS_MERGEUVROW_AVX2)
  if (TestCpuFlag(kCpuHasAVX2)) {
    MergeUVRow = MergeUVRow_Any_AVX2;
    if (IS_ALIGNED(halfwidth, 32)) {
      MergeUVRow = MergeUVRow_AVX2;
    }
  }
#endif
#if defined(HAS_MERGEUVROW_AVX512BW)
  if (TestCpuFlag(kCpuHasAVX512BW)) {
    MergeUVRow = MergeUVRow_Any_AVX512BW;
    if (IS_ALIGNED(halfwidth, 32)) {
      MergeUVRow = MergeUVRow_AVX512BW;
    }
  }
#endif
#if defined(HAS_MERGEUVROW_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    MergeUVRow = MergeUVRow_Any_NEON;
    if (IS_ALIGNED(halfwidth, 16)) {
      MergeUVRow = MergeUVRow_NEON;
    }
  }
#endif
#if defined(HAS_MERGEUVROW_SVE2)
  if (TestCpuFlag(kCpuHasSVE2)) {
    MergeUVRow = MergeUVRow_SVE2;
  }
#endif
#if defined(HAS_MERGEUVROW_SME)
  if (TestCpuFlag(kCpuHasSME)) {
    MergeUVRow = MergeUVRow_SME;
  }
#endif
#if defined(HAS_MERGEUVROW_LSX)
  if (TestCpuFlag(kCpuHasLSX)) {
    MergeUVRow = MergeUVRow_Any_LSX;
    if (IS_ALIGNED(halfwidth, 16)) {
      MergeUVRow = MergeUVRow_LSX;
    }
  }
#endif
#if defined(HAS_MERGEUVROW_RVV)
  if (TestCpuFlag(kCpuHasRVV)) {
    MergeUVRow = MergeUVRow_RVV;
  }
#endif
#if defined(HAS_MERGEUVROW_WASMSIMD)
  if (TestCpuFlag(kCpuHasWASMSIMD)) {
    MergeUVRow = MergeUVRow_Any_WASMSIMD;
    if (IS_ALIGNED(halfwidth, 16)) {
      MergeUVRow = MergeUVRow_WASMSIMD;
    }
  }
#endif

  // Convert Y plane.
  if (dst_y) {
    Convert16To8Plane(src_y, src_stride_y, dst_y, dst_stride_y, scale, width,
                      height);
  }

  {
    // Allocate a row of uv.
    align_buffer_64(row_u, ((halfwidth + 31) & ~31) * 2);
    uint8_t* row_v = row_u + ((halfwidth + 31) & ~31);
    if (!row_u)
      return 1;

    for (y = 0; y < halfheight; ++y) {
      Convert16To8Row(src_u, row_u, scale, halfwidth);
      Convert16To8Row(src_v, row_v, scale, halfwidth);
      MergeUVRow(row_u, row_v, dst_uv, halfwidth);
      src_u += src_stride_u;
      src_v += src_stride_v;
      dst_uv += dst_stride_uv;
    }
    free_aligned_buffer_64(row_u);
  }
  return 0;
}

LIBYUV_API
int I210ToP210(const uint16_t* src_y,
               int src_stride_y,
               const uint16_t* src_u,
               int src_stride_u,
               const uint16_t* src_v,
               int src_stride_v,
               uint16_t* dst_y,
               int dst_stride_y,
               uint16_t* dst_uv,
               int dst_stride_uv,
               int width,
               int height) {
  return IxxxToPxxx(src_y, src_stride_y, src_u, src_stride_u, src_v,
                    src_stride_v, dst_y, dst_stride_y, dst_uv, dst_stride_uv,
                    width, height, 1, 0, 10);
}

LIBYUV_API
int I410ToP410(const uint16_t* src_y,
               int src_stride_y,
               const uint16_t* src_u,
               int src_stride_u,
               const uint16_t* src_v,
               int src_stride_v,
               uint16_t* dst_y,
               int dst_stride_y,
               uint16_t* dst_uv,
               int dst_stride_uv,
               int width,
               int height) {
  return IxxxToPxxx(src_y, src_stride_y, src_u, src_stride_u, src_v,
                    src_stride_v, dst_y, dst_stride_y, dst_uv, dst_stride_uv,
                    width, height, 0, 0, 10);
}

LIBYUV_API
int I012ToP012(const uint16_t* src_y,
               int src_stride_y,
               const uint16_t* src_u,
               int src_stride_u,
               const uint16_t* src_v,
               int src_stride_v,
               uint16_t* dst_y,
               int dst_stride_y,
               uint16_t* dst_uv,
               int dst_stride_uv,
               int width,
               int height) {
  return IxxxToPxxx(src_y, src_stride_y, src_u, src_stride_u, src_v,
                    src_stride_v, dst_y, dst_stride_y, dst_uv, dst_stride_uv,
                    width, height, 1, 1, 12);
}

LIBYUV_API
int I212ToP212(const uint16_t* src_y,
               int src_stride_y,
               const uint16_t* src_u,
               int src_stride_u,
               const uint16_t* src_v,
               int src_stride_v,
               uint16_t* dst_y,
               int dst_stride_y,
               uint16_t* dst_uv,
               int dst_stride_uv,
               int width,
               int height) {
  return IxxxToPxxx(src_y, src_stride_y, src_u, src_stride_u, src_v,
                    src_stride_v, dst_y, dst_stride_y, dst_uv, dst_stride_uv,
                    width, height, 1, 0, 12);
}

LIBYUV_API
int I412ToP412(const uint16_t* src_y,
               int src_stride_y,
               const uint16_t* src_u,
               int src_stride_u,
               const uint16_t* src_v,
               int src_stride_v,
               uint16_t* dst_y,
               int dst_stride_y,
               uint16_t* dst_uv,
               int dst_stride_uv,
               int width,
               int height) {
  return IxxxToPxxx(src_y, src_stride_y, src_u, src_stride_u, src_v,
                    src_stride_v, dst_y, dst_stride_y, dst_uv, dst_stride_uv,
                    width, height, 0, 0, 12);
}

// 422 chroma is 1/2 width, 1x height
// 420 chroma is 1/2 width, 1/2 height
LIBYUV_API
int I422ToI420(const uint8_t* src_y,
               int src_stride_y,
               const uint8_t* src_u,
               int src_stride_u,
               const uint8_t* src_v,
               int src_stride_v,
               uint8_t* dst_y,
               int dst_stride_y,
               uint8_t* dst_u,
               int dst_stride_u,
               uint8_t* dst_v,
               int dst_stride_v,
               int width,
               int height) {
  const int src_uv_width = SUBSAMPLE(width, 1, 1);
  return I4xxToI420(src_y, src_stride_y, src_u, src_stride_u, src_v,
                    src_stride_v, dst_y, dst_stride_y, dst_u, dst_stride_u,
                    dst_v, dst_stride_v, width, height, src_uv_width, height);
}

LIBYUV_API
int I422ToI210(const uint8_t* src_y,
               int src_stride_y,
               const uint8_t* src_u,
               int src_stride_u,
               const uint8_t* src_v,
               int src_stride_v,
               uint16_t* dst_y,
               int dst_stride_y,
               uint16_t* dst_u,
               int dst_stride_u,
               uint16_t* dst_v,
               int dst_stride_v,
               int width,
               int height) {
  int halfwidth = (width + 1) >> 1;
  if ((!src_y && dst_y) || !src_u || !src_v || !dst_u || !dst_v || width <= 0 ||
      height == 0 || height == INT_MIN) {
    return -1;
  }
  // Negative height means invert the image.
  if (height < 0) {
    height = -height;
    src_y = src_y + (ptrdiff_t)(height - 1) * src_stride_y;
    src_u = src_u + (ptrdiff_t)(height - 1) * src_stride_u;
    src_v = src_v + (ptrdiff_t)(height - 1) * src_stride_v;
    src_stride_y = -src_stride_y;
    src_stride_u = -src_stride_u;
    src_stride_v = -src_stride_v;
  }

  // Convert Y plane.
  Convert8To16Plane(src_y, src_stride_y, dst_y, dst_stride_y, 1024, width,
                    height);
  // Convert UV planes.
  Convert8To16Plane(src_u, src_stride_u, dst_u, dst_stride_u, 1024, halfwidth,
                    height);
  Convert8To16Plane(src_v, src_stride_v, dst_v, dst_stride_v, 1024, halfwidth,
                    height);
  return 0;
}

// TODO(fbarchard): Implement row conversion.
LIBYUV_API
int I422ToNV21(const uint8_t* src_y,
               int src_stride_y,
               const uint8_t* src_u,
               int src_stride_u,
               const uint8_t* src_v,
               int src_stride_v,
               uint8_t* dst_y,
               int dst_stride_y,
               uint8_t* dst_vu,
               int dst_stride_vu,
               int width,
               int height) {
  int r;
  if (width <= 0 || height == 0 || height == INT_MIN) {
    return -1;
  }
  int halfwidth = (width + 1) >> 1;
  int halfheight = (height + 1) >> 1;
  // Negative height means invert the image.
  if (height < 0) {
    height = -height;
    halfheight = (height + 1) >> 1;
    src_y = src_y + (ptrdiff_t)(height - 1) * src_stride_y;
    src_u = src_u + (ptrdiff_t)(height - 1) * src_stride_u;
    src_v = src_v + (ptrdiff_t)(height - 1) * src_stride_v;
    src_stride_y = -src_stride_y;
    src_stride_u = -src_stride_u;
    src_stride_v = -src_stride_v;
  }

  // Allocate u and v buffers
  const uint64_t plane_size = (uint64_t)halfwidth * halfheight;
  if (plane_size > SIZE_MAX / 2)
    return 1;
  align_buffer_64(plane_u, (size_t)plane_size * 2);
  if (!plane_u)
    return 1;
  uint8_t* plane_v = plane_u + (size_t)plane_size;

  r = I422ToI420(src_y, src_stride_y, src_u, src_stride_u, src_v, src_stride_v,
                 dst_y, dst_stride_y, plane_u, halfwidth, plane_v, halfwidth,
                 width, height);
  if (r != 0) {
    return r;
  }
  MergeUVPlane(plane_v, halfwidth, plane_u, halfwidth, dst_vu, dst_stride_vu,
               halfwidth, halfheight);
  free_aligned_buffer_64(plane_u);
  return 0;
}

LIBYUV_API
int MM21ToNV12(const uint8_t* src_y,
               int src_stride_y,
               const uint8_t* src_uv,
               int src_stride_uv,
               uint8_t* dst_y,
               int dst_stride_y,
               uint8_t* dst_uv,
               int dst_stride_uv,
               int width,
               int height) {
  if (!src_uv || !dst_uv || width <= 0) {
    return -1;
  }

  int sign = height < 0 ? -1 : 1;

  if (dst_y) {
    DetilePlane(src_y, src_stride_y, dst_y, dst_stride_y, width, height, 32);
  }
  DetilePlane(src_uv, src_stride_uv, dst_uv, dst_stride_uv, (width + 1) & ~1,
              (height + sign) / 2, 16);

  return 0;
}

LIBYUV_API
int MM21ToI420(const uint8_t* src_y,
               int src_stride_y,
               const uint8_t* src_uv,
               int src_stride_uv,
               uint8_t* dst_y,
               int dst_stride_y,
               uint8_t* dst_u,
               int dst_stride_u,
               uint8_t* dst_v,
               int dst_stride_v,
               int width,
               int height) {
  int sign = height < 0 ? -1 : 1;

  if (!src_uv || !dst_u || !dst_v || width <= 0) {
    return -1;
  }

  if (dst_y) {
    DetilePlane(src_y, src_stride_y, dst_y, dst_stride_y, width, height, 32);
  }
  DetileSplitUVPlane(src_uv, src_stride_uv, dst_u, dst_stride_u, dst_v,
                     dst_stride_v, (width + 1) & ~1, (height + sign) / 2, 16);

  return 0;
}

LIBYUV_API
int MM21ToYUY2(const uint8_t* src_y,
               int src_stride_y,
               const uint8_t* src_uv,
               int src_stride_uv,
               uint8_t* dst_yuy2,
               int dst_stride_yuy2,
               int width,
               int height) {
  if (!src_y || !src_uv || !dst_yuy2 || width <= 0) {
    return -1;
  }

  DetileToYUY2(src_y, src_stride_y, src_uv, src_stride_uv, dst_yuy2,
               dst_stride_yuy2, width, height, 32);

  return 0;
}

// Convert MT2T into P010. See tinyurl.com/mtk-10bit-video-format for format
// documentation.
// TODO(greenjustin): Add an MT2T to I420 conversion.
LIBYUV_API
int MT2TToP010(const uint8_t* src_y,
               int src_stride_y,
               const uint8_t* src_uv,
               int src_stride_uv,
               uint16_t* dst_y,
               int dst_stride_y,
               uint16_t* dst_uv,
               int dst_stride_uv,
               int width,
               int height) {
  if (width <= 0 || height == 0 || height == INT_MIN || !src_uv || !dst_uv) {
    return -1;
  }

  {
    int uv_width = (width + 1) & ~1;
    int uv_height = (height + 1) / 2;
    int y = 0;
    const int tile_width = 16;
    const int y_tile_height = 32;
    const int uv_tile_height = 16;
    int padded_width = (width + tile_width - 1) & ~(tile_width - 1);
    int y_tile_row_size = padded_width * y_tile_height * 10 / 8;
    int uv_tile_row_size = padded_width * uv_tile_height * 10 / 8;
    size_t row_buf_size = padded_width * y_tile_height * sizeof(uint16_t);
    void (*UnpackMT2T)(const uint8_t* src, uint16_t* dst, size_t size) =
        UnpackMT2T_C;
    align_buffer_64(row_buf, row_buf_size);
    if (!row_buf)
      return 1;

#if defined(HAS_UNPACKMT2T_NEON)
    if (TestCpuFlag(kCpuHasNEON)) {
      UnpackMT2T = UnpackMT2T_NEON;
    }
#endif
    // Negative height means invert the image.
    if (height < 0) {
      height = -height;
      uv_height = (height + 1) / 2;
      if (dst_y) {
        dst_y = dst_y + (ptrdiff_t)(height - 1) * dst_stride_y;
        dst_stride_y = -dst_stride_y;
      }
      dst_uv = dst_uv + (ptrdiff_t)(uv_height - 1) * dst_stride_uv;
      dst_stride_uv = -dst_stride_uv;
    }

    // Unpack and detile Y in rows of tiles
    if (src_y && dst_y) {
      for (y = 0; y < (height & ~(y_tile_height - 1)); y += y_tile_height) {
        UnpackMT2T(src_y, (uint16_t*)row_buf, y_tile_row_size);
        DetilePlane_16((uint16_t*)row_buf, padded_width, dst_y, dst_stride_y,
                       width, y_tile_height, y_tile_height);
        src_y += src_stride_y * y_tile_height;
        dst_y += dst_stride_y * y_tile_height;
      }
      if (height & (y_tile_height - 1)) {
        UnpackMT2T(src_y, (uint16_t*)row_buf, y_tile_row_size);
        DetilePlane_16((uint16_t*)row_buf, padded_width, dst_y, dst_stride_y,
                       width, height & (y_tile_height - 1), y_tile_height);
      }
    }

    // Unpack and detile UV plane
    for (y = 0; y < (uv_height & ~(uv_tile_height - 1)); y += uv_tile_height) {
      UnpackMT2T(src_uv, (uint16_t*)row_buf, uv_tile_row_size);
      DetilePlane_16((uint16_t*)row_buf, padded_width, dst_uv, dst_stride_uv,
                     uv_width, uv_tile_height, uv_tile_height);
      src_uv += src_stride_uv * uv_tile_height;
      dst_uv += dst_stride_uv * uv_tile_height;
    }
    if (uv_height & (uv_tile_height - 1)) {
      UnpackMT2T(src_uv, (uint16_t*)row_buf, uv_tile_row_size);
      DetilePlane_16((uint16_t*)row_buf, padded_width, dst_uv, dst_stride_uv,
                     uv_width, uv_height & (uv_tile_height - 1),
                     uv_tile_height);
    }
    free_aligned_buffer_64(row_buf);
  }
  return 0;
}

#ifdef I422TONV21_ROW_VERSION
// Unittest fails for this version.
// 422 chroma is 1/2 width, 1x height
// 420 chroma is 1/2 width, 1/2 height
// Swap src_u and src_v to implement I422ToNV12
LIBYUV_API
int I422ToNV21(const uint8_t* src_y,
               int src_stride_y,
               const uint8_t* src_u,
               int src_stride_u,
               const uint8_t* src_v,
               int src_stride_v,
               uint8_t* dst_y,
               int dst_stride_y,
               uint8_t* dst_vu,
               int dst_stride_vu,
               int width,
               int height) {
  int y;
  void (*MergeUVRow)(const uint8_t* src_u, const uint8_t* src_v,
                     uint8_t* dst_uv, int width) = MergeUVRow_C;
  void (*InterpolateRow)(uint8_t* dst_ptr, const uint8_t* src_ptr,
                         ptrdiff_t src_stride, int dst_width,
                         int source_y_fraction) = InterpolateRow_C;
  int halfwidth = (width + 1) >> 1;
  int halfheight = (height + 1) >> 1;
  if ((!src_y && dst_y) || !src_u || !src_v || !dst_vu || width <= 0 ||
      height == 0 || height == INT_MIN) {
    return -1;
  }
  // Negative height means invert the image.
  if (height < 0) {
    height = -height;
    halfheight = (height + 1) >> 1;
    src_y = src_y + (ptrdiff_t)(height - 1) * src_stride_y;
    src_u = src_u + (ptrdiff_t)(halfheight - 1) * src_stride_u;
    src_v = src_v + (ptrdiff_t)(halfheight - 1) * src_stride_v;
    src_stride_y = -src_stride_y;
    src_stride_u = -src_stride_u;
    src_stride_v = -src_stride_v;
  }
#if defined(HAS_MERGEUVROW_SSE2)
  if (TestCpuFlag(kCpuHasSSE2)) {
    MergeUVRow = MergeUVRow_Any_SSE2;
    if (IS_ALIGNED(halfwidth, 16)) {
      MergeUVRow = MergeUVRow_SSE2;
    }
  }
#endif
#if defined(HAS_MERGEUVROW_AVX2)
  if (TestCpuFlag(kCpuHasAVX2)) {
    MergeUVRow = MergeUVRow_Any_AVX2;
    if (IS_ALIGNED(halfwidth, 32)) {
      MergeUVRow = MergeUVRow_AVX2;
    }
  }
#endif
#if defined(HAS_MERGEUVROW_AVX512BW)
  if (TestCpuFlag(kCpuHasAVX512BW)) {
    MergeUVRow = MergeUVRow_Any_AVX512BW;
    if (IS_ALIGNED(halfwidth, 32)) {
      MergeUVRow = MergeUVRow_AVX512BW;
    }
  }
#endif
#if defined(HAS_MERGEUVROW_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    MergeUVRow = MergeUVRow_Any_NEON;
    if (IS_ALIGNED(halfwidth, 16)) {
      MergeUVRow = MergeUVRow_NEON;
    }
  }
#endif
#if defined(HAS_MERGEUVROW_SVE2)
  if (TestCpuFlag(kCpuHasSVE2)) {
    MergeUVRow = MergeUVRow_SVE2;
  }
#endif
#if defined(HAS_MERGEUVROW_SME)
  if (TestCpuFlag(kCpuHasSME)) {
    MergeUVRow = MergeUVRow_SME;
  }
#endif
#if defined(HAS_MERGEUVROW_LSX)
  if (TestCpuFlag(kCpuHasLSX)) {
    MergeUVRow = MergeUVRow_Any_LSX;
    if (IS_ALIGNED(halfwidth, 16)) {
      MergeUVRow = MergeUVRow_LSX;
    }
  }
#endif
#if defined(HAS_MERGEUVROW_RVV)
  if (TestCpuFlag(kCpuHasRVV)) {
    MergeUVRow = MergeUVRow_RVV;
  }
#endif
#if defined(HAS_MERGEUVROW_WASMSIMD)
  if (TestCpuFlag(kCpuHasWASMSIMD)) {
    MergeUVRow = MergeUVRow_Any_WASMSIMD;
    if (IS_ALIGNED(halfwidth, 16)) {
      MergeUVRow = MergeUVRow_WASMSIMD;
    }
  }
#endif
#if defined(HAS_INTERPOLATEROW_AVX2)
  if (TestCpuFlag(kCpuHasAVX2)) {
    InterpolateRow = InterpolateRow_Any_AVX2;
    if (IS_ALIGNED(width, 32)) {
      InterpolateRow = InterpolateRow_AVX2;
    }
  }
#endif
#if defined(HAS_INTERPOLATEROW_AVX512BW)
  if (TestCpuFlag(kCpuHasAVX512BW)) {
    InterpolateRow = InterpolateRow_AVX512BW;
  }
#endif
#if defined(HAS_INTERPOLATEROW_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    InterpolateRow = InterpolateRow_Any_NEON;
    if (IS_ALIGNED(width, 16)) {
      InterpolateRow = InterpolateRow_NEON;
    }
  }
#endif
#if defined(HAS_INTERPOLATEROW_SVE2)
  if (TestCpuFlag(kCpuHasSVE2)) {
    InterpolateRow = InterpolateRow_SVE2;
  }
#endif
#if defined(HAS_INTERPOLATEROW_SME)
  if (TestCpuFlag(kCpuHasSME)) {
    InterpolateRow = InterpolateRow_SME;
  }
#endif
#if defined(HAS_INTERPOLATEROW_LSX)
  if (TestCpuFlag(kCpuHasLSX)) {
    InterpolateRow = InterpolateRow_Any_LSX;
    if (IS_ALIGNED(width, 32)) {
      InterpolateRow = InterpolateRow_LSX;
    }
  }
#endif
#if defined(HAS_INTERPOLATEROW_RVV)
  if (TestCpuFlag(kCpuHasRVV)) {
    InterpolateRow = InterpolateRow_RVV;
  }
#endif
#if defined(HAS_INTERPOLATEROW_WASMSIMD)
  if (TestCpuFlag(kCpuHasWASMSIMD)) {
    InterpolateRow = InterpolateRow_Any_WASMSIMD;
    if (IS_ALIGNED(width, 16)) {
      InterpolateRow = InterpolateRow_WASMSIMD;
    }
  }
#endif

  if (dst_y) {
    CopyPlane(src_y, src_stride_y, dst_y, dst_stride_y, halfwidth, height);
  }
  {
    // Allocate 2 rows of vu.
    int awidth = halfwidth * 2;
    align_buffer_64(row_vu_0, awidth * 2);
    uint8_t* row_vu_1 = row_vu_0 + awidth;
    if (!row_vu_0)
      return 1;

    for (y = 0; y < height - 1; y += 2) {
      MergeUVRow(src_v, src_u, row_vu_0, halfwidth);
      MergeUVRow(src_v + src_stride_v, src_u + src_stride_u, row_vu_1,
                 halfwidth);
      InterpolateRow(dst_vu, row_vu_0, awidth, awidth, 128);
      src_u += src_stride_u * 2;
      src_v += src_stride_v * 2;
      dst_vu += dst_stride_vu;
    }
    if (height & 1) {
      MergeUVRow(src_v, src_u, dst_vu, halfwidth);
    }
    free_aligned_buffer_64(row_vu_0);
  }
  return 0;
}
#endif  // I422TONV21_ROW_VERSION

// 444 chroma is 1x width, 1x height
// 420 chroma is 1/2 width, 1/2 height
LIBYUV_API
int I444ToI420(const uint8_t* src_y,
               int src_stride_y,
               const uint8_t* src_u,
               int src_stride_u,
               const uint8_t* src_v,
               int src_stride_v,
               uint8_t* dst_y,
               int dst_stride_y,
               uint8_t* dst_u,
               int dst_stride_u,
               uint8_t* dst_v,
               int dst_stride_v,
               int width,
               int height) {
  return I4xxToI420(src_y, src_stride_y, src_u, src_stride_u, src_v,
                    src_stride_v, dst_y, dst_stride_y, dst_u, dst_stride_u,
                    dst_v, dst_stride_v, width, height, width, height);
}

LIBYUV_API
int I444ToNV12(const uint8_t* src_y,
               int src_stride_y,
               const uint8_t* src_u,
               int src_stride_u,
               const uint8_t* src_v,
               int src_stride_v,
               uint8_t* dst_y,
               int dst_stride_y,
               uint8_t* dst_uv,
               int dst_stride_uv,
               int width,
               int height) {
  if ((!src_y && dst_y) || !src_u || !src_v || !dst_uv || width <= 0 ||
      height == 0 || height == INT_MIN) {
    return -1;
  }
  // Negative height means invert the image.
  if (height < 0) {
    height = -height;
    src_y = src_y + (ptrdiff_t)(height - 1) * src_stride_y;
    src_u = src_u + (ptrdiff_t)(height - 1) * src_stride_u;
    src_v = src_v + (ptrdiff_t)(height - 1) * src_stride_v;
    src_stride_y = -src_stride_y;
    src_stride_u = -src_stride_u;
    src_stride_v = -src_stride_v;
  }
  if (dst_y) {
    CopyPlane(src_y, src_stride_y, dst_y, dst_stride_y, width, height);
  }
  HalfMergeUVPlane(src_u, src_stride_u, src_v, src_stride_v, dst_uv,
                   dst_stride_uv, width, height);
  return 0;
}

LIBYUV_API
int I444ToNV21(const uint8_t* src_y,
               int src_stride_y,
               const uint8_t* src_u,
               int src_stride_u,
               const uint8_t* src_v,
               int src_stride_v,
               uint8_t* dst_y,
               int dst_stride_y,
               uint8_t* dst_vu,
               int dst_stride_vu,
               int width,
               int height) {
  return I444ToNV12(src_y, src_stride_y, src_v, src_stride_v, src_u,
                    src_stride_u, dst_y, dst_stride_y, dst_vu, dst_stride_vu,
                    width, height);
}

// I400 is greyscale typically used in MJPG
LIBYUV_API
int I400ToI420(const uint8_t* src_y,
               int src_stride_y,
               uint8_t* dst_y,
               int dst_stride_y,
               uint8_t* dst_u,
               int dst_stride_u,
               uint8_t* dst_v,
               int dst_stride_v,
               int width,
               int height) {
  int halfwidth = (width + 1) >> 1;
  int halfheight = (height + 1) >> 1;
  if ((!src_y && dst_y) || !dst_u || !dst_v || width <= 0 || height == 0 ||
      height == INT_MIN) {
    return -1;
  }
  // Negative height means invert the image.
  if (height < 0) {
    height = -height;
    halfheight = (height + 1) >> 1;
    src_y = src_y + (ptrdiff_t)(height - 1) * src_stride_y;
    src_stride_y = -src_stride_y;
  }
  if (dst_y) {
    CopyPlane(src_y, src_stride_y, dst_y, dst_stride_y, width, height);
  }
  SetPlane(dst_u, dst_stride_u, halfwidth, halfheight, 128);
  SetPlane(dst_v, dst_stride_v, halfwidth, halfheight, 128);
  return 0;
}

// I400 is greyscale typically used in MJPG
LIBYUV_API
int I400ToNV21(const uint8_t* src_y,
               int src_stride_y,
               uint8_t* dst_y,
               int dst_stride_y,
               uint8_t* dst_vu,
               int dst_stride_vu,
               int width,
               int height) {
  int halfwidth = (width + 1) >> 1;
  int halfheight = (height + 1) >> 1;
  if ((!src_y && dst_y) || !dst_vu || width <= 0 || height == 0 ||
      height == INT_MIN) {
    return -1;
  }
  // Negative height means invert the image.
  if (height < 0) {
    height = -height;
    halfheight = (height + 1) >> 1;
    src_y = src_y + (ptrdiff_t)(height - 1) * src_stride_y;
    src_stride_y = -src_stride_y;
  }
  if (dst_y) {
    CopyPlane(src_y, src_stride_y, dst_y, dst_stride_y, width, height);
  }
  SetPlane(dst_vu, dst_stride_vu, halfwidth * 2, halfheight, 128);
  return 0;
}

// Convert NV12 to I420.
// TODO(fbarchard): Consider inverting destination. Faster on ARM with prfm.
LIBYUV_API
int NV12ToI420(const uint8_t* src_y,
               int src_stride_y,
               const uint8_t* src_uv,
               int src_stride_uv,
               uint8_t* dst_y,
               int dst_stride_y,
               uint8_t* dst_u,
               int dst_stride_u,
               uint8_t* dst_v,
               int dst_stride_v,
               int width,
               int height) {
  int halfwidth = (width + 1) >> 1;
  int halfheight = (height + 1) >> 1;
  if ((!src_y && dst_y) || !src_uv || !dst_u || !dst_v || width <= 0 ||
      height == 0 || height == INT_MIN) {
    return -1;
  }
  // Negative height means invert the image.
  if (height < 0) {
    height = -height;
    halfheight = (height + 1) >> 1;
    src_y = src_y + (ptrdiff_t)(height - 1) * src_stride_y;
    src_uv = src_uv + (ptrdiff_t)(halfheight - 1) * src_stride_uv;
    src_stride_y = -src_stride_y;
    src_stride_uv = -src_stride_uv;
  }
  // Coalesce rows.
  if (src_stride_y == width && dst_stride_y == width &&
      (ptrdiff_t)width * height <= INT_MAX) {
    width *= height;
    height = 1;
    src_stride_y = dst_stride_y = 0;
  }
  // Coalesce rows.
  if (src_stride_uv == halfwidth * 2 && dst_stride_u == halfwidth &&
      dst_stride_v == halfwidth &&
      (ptrdiff_t)halfwidth * halfheight <= INT_MAX) {
    halfwidth *= halfheight;
    halfheight = 1;
    src_stride_uv = dst_stride_u = dst_stride_v = 0;
  }

  if (dst_y) {
    CopyPlane(src_y, src_stride_y, dst_y, dst_stride_y, width, height);
  }

  // Split UV plane - NV12 / NV21
  SplitUVPlane(src_uv, src_stride_uv, dst_u, dst_stride_u, dst_v, dst_stride_v,
               halfwidth, halfheight);

  return 0;
}

// Convert NV21 to I420.  Same as NV12 but u and v pointers swapped.
LIBYUV_API
int NV21ToI420(const uint8_t* src_y,
               int src_stride_y,
               const uint8_t* src_vu,
               int src_stride_vu,
               uint8_t* dst_y,
               int dst_stride_y,
               uint8_t* dst_u,
               int dst_stride_u,
               uint8_t* dst_v,
               int dst_stride_v,
               int width,
               int height) {
  return NV12ToI420(src_y, src_stride_y, src_vu, src_stride_vu, dst_y,
                    dst_stride_y, dst_v, dst_stride_v, dst_u, dst_stride_u,
                    width, height);
}

LIBYUV_API
int NV12ToNV24(const uint8_t* src_y,
               int src_stride_y,
               const uint8_t* src_uv,
               int src_stride_uv,
               uint8_t* dst_y,
               int dst_stride_y,
               uint8_t* dst_uv,
               int dst_stride_uv,
               int width,
               int height) {
  int r;
  if ((!src_y && dst_y) || !src_uv || !dst_uv || width <= 0 || height == 0 ||
      height == INT_MIN) {
    return -1;
  }

  if (dst_y) {
    CopyPlane(src_y, src_stride_y, dst_y, dst_stride_y, width, height);
  }
  r = UVScale(src_uv, src_stride_uv, SUBSAMPLE(width, 1, 1),
              SUBSAMPLE(height, 1, 1), dst_uv, dst_stride_uv, Abs(width),
              Abs(height), kFilterBilinear);
  return r;
}

LIBYUV_API
int NV16ToNV24(const uint8_t* src_y,
               int src_stride_y,
               const uint8_t* src_uv,
               int src_stride_uv,
               uint8_t* dst_y,
               int dst_stride_y,
               uint8_t* dst_uv,
               int dst_stride_uv,
               int width,
               int height) {
  int r;
  if ((!src_y && dst_y) || !src_uv || !dst_uv || width <= 0 || height == 0 ||
      height == INT_MIN) {
    return -1;
  }

  if (dst_y) {
    CopyPlane(src_y, src_stride_y, dst_y, dst_stride_y, width, height);
  }
  r = UVScale(src_uv, src_stride_uv, SUBSAMPLE(width, 1, 1), height, dst_uv,
              dst_stride_uv, Abs(width), Abs(height), kFilterBilinear);
  return r;
}

// Any P[420]1[02] to I[420]1[02] format
static int PxxxToIxxx(const uint16_t* src_y,
                      int src_stride_y,
                      const uint16_t* src_uv,
                      int src_stride_uv,
                      uint16_t* dst_y,
                      int dst_stride_y,
                      uint16_t* dst_u,
                      int dst_stride_u,
                      uint16_t* dst_v,
                      int dst_stride_v,
                      int width,
                      int height,
                      int subsample_x,
                      int subsample_y,
                      int depth) {
  const int uv_width = SUBSAMPLE(width, subsample_x, subsample_x);
  const int uv_height = SUBSAMPLE(height, subsample_y, subsample_y);
  if (!src_y || !dst_y || !src_uv || !dst_u || !dst_v || width <= 0 ||
      height == 0 || height == INT_MIN) {
    return -1;
  }
  ConvertToLSBPlane_16(src_y, src_stride_y, dst_y, dst_stride_y, width, height,
                       depth);
  SplitUVPlane_16(src_uv, src_stride_uv, dst_u, dst_stride_u, dst_v,
                  dst_stride_v, uv_width, uv_height, depth);
  return 0;
}

LIBYUV_API
int P010ToI010(const uint16_t* src_y,
               int src_stride_y,
               const uint16_t* src_uv,
               int src_stride_uv,
               uint16_t* dst_y,
               int dst_stride_y,
               uint16_t* dst_u,
               int dst_stride_u,
               uint16_t* dst_v,
               int dst_stride_v,
               int width,
               int height) {
  return PxxxToIxxx(src_y, src_stride_y, src_uv, src_stride_uv, dst_y,
                    dst_stride_y, dst_u, dst_stride_u, dst_v, dst_stride_v,
                    width, height, 1, 1, 10);
}

LIBYUV_API
int P210ToI210(const uint16_t* src_y,
               int src_stride_y,
               const uint16_t* src_uv,
               int src_stride_uv,
               uint16_t* dst_y,
               int dst_stride_y,
               uint16_t* dst_u,
               int dst_stride_u,
               uint16_t* dst_v,
               int dst_stride_v,
               int width,
               int height) {
  return PxxxToIxxx(src_y, src_stride_y, src_uv, src_stride_uv, dst_y,
                    dst_stride_y, dst_u, dst_stride_u, dst_v, dst_stride_v,
                    width, height, 1, 0, 10);
}

LIBYUV_API
int P410ToI410(const uint16_t* src_y,
               int src_stride_y,
               const uint16_t* src_uv,
               int src_stride_uv,
               uint16_t* dst_y,
               int dst_stride_y,
               uint16_t* dst_u,
               int dst_stride_u,
               uint16_t* dst_v,
               int dst_stride_v,
               int width,
               int height) {
  return PxxxToIxxx(src_y, src_stride_y, src_uv, src_stride_uv, dst_y,
                    dst_stride_y, dst_u, dst_stride_u, dst_v, dst_stride_v,
                    width, height, 0, 0, 10);
}

static void InvertPx10(const uint16_t** src_y,
                       int* src_stride_y,
                       const uint16_t** src_uv,
                       int* src_stride_uv,
                       int* height,
                       int subsample_y) {
  const int abs_height = Abs(*height);
  const int src_uv_height = SUBSAMPLE(abs_height, subsample_y, subsample_y);
  if (*src_y) {
    *src_y = *src_y + (ptrdiff_t)(abs_height - 1) * *src_stride_y;
    *src_stride_y = -*src_stride_y;
  }
  *src_uv = *src_uv + (ptrdiff_t)(src_uv_height - 1) * *src_stride_uv;
  *src_stride_uv = -*src_stride_uv;
  *height = abs_height;
}

static int Px10ToI420(const uint16_t* src_y,
                      int src_stride_y,
                      const uint16_t* src_uv,
                      int src_stride_uv,
                      uint8_t* dst_y,
                      int dst_stride_y,
                      uint8_t* dst_u,
                      int dst_stride_u,
                      uint8_t* dst_v,
                      int dst_stride_v,
                      int width,
                      int height,
                      int subsample_x,
                      int subsample_y) {
  const int scale16 = 256;  // P*10 stores 10-bit in the MSBs.
  int src_uv_width;
  int dst_uv_width;
  if ((!src_y && dst_y) || !src_uv || !dst_u || !dst_v || width <= 0 ||
      height == 0 || height == INT_MIN) {
    return -1;
  }
  if (height < 0) {
    InvertPx10(&src_y, &src_stride_y, &src_uv, &src_stride_uv, &height,
               subsample_y);
  }
  src_uv_width = SUBSAMPLE(width, subsample_x, subsample_x);
  dst_uv_width = SUBSAMPLE(width, 1, 1);

  if (dst_y) {
    Convert16To8Plane(src_y, src_stride_y, dst_y, dst_stride_y, scale16, width,
                      height);
  }

  if (subsample_x && subsample_y) {
    int y;
    const int uv_packed = src_uv_width * 2;
    const int uv_height = SUBSAMPLE(height, 1, 1);
    void (*Convert16To8Row)(const uint16_t* src_y, uint8_t* dst_y, int scale,
                            int width) = Convert16To8Row_C;
    void (*SplitUVRow)(const uint8_t* src_uv, uint8_t* dst_u, uint8_t* dst_v,
                       int width) = SplitUVRow_C;

#if defined(HAS_CONVERT16TO8ROW_NEON)
    if (TestCpuFlag(kCpuHasNEON)) {
      Convert16To8Row = Convert16To8Row_Any_NEON;
      if (IS_ALIGNED(uv_packed, 16)) {
        Convert16To8Row = Convert16To8Row_NEON;
      }
    }
#endif
#if defined(HAS_CONVERT16TO8ROW_SVE2)
    if (TestCpuFlag(kCpuHasSVE2)) {
      Convert16To8Row = Convert16To8Row_SVE2;
    }
#endif
#if defined(HAS_CONVERT16TO8ROW_SME)
    if (TestCpuFlag(kCpuHasSME)) {
      Convert16To8Row = Convert16To8Row_SME;
    }
#endif
#if defined(HAS_CONVERT16TO8ROW_SSSE3)
    if (TestCpuFlag(kCpuHasSSSE3)) {
      Convert16To8Row = Convert16To8Row_Any_SSSE3;
      if (IS_ALIGNED(uv_packed, 16)) {
        Convert16To8Row = Convert16To8Row_SSSE3;
      }
    }
#endif
#if defined(HAS_CONVERT16TO8ROW_AVX2)
    if (TestCpuFlag(kCpuHasAVX2)) {
      Convert16To8Row = Convert16To8Row_Any_AVX2;
      if (IS_ALIGNED(uv_packed, 32)) {
        Convert16To8Row = Convert16To8Row_AVX2;
      }
    }
#endif
#if defined(HAS_CONVERT16TO8ROW_AVX512BW)
    if (TestCpuFlag(kCpuHasAVX512BW)) {
      Convert16To8Row = Convert16To8Row_Any_AVX512BW;
      if (IS_ALIGNED(uv_packed, 64)) {
        Convert16To8Row = Convert16To8Row_AVX512BW;
      }
    }
#endif
#if defined(HAS_CONVERT16TO8ROW_RVV)
    if (TestCpuFlag(kCpuHasRVV)) {
      Convert16To8Row = Convert16To8Row_RVV;
    }
#endif
#if defined(HAS_SPLITUVROW_SSE2)
    if (TestCpuFlag(kCpuHasSSE2)) {
      SplitUVRow = SplitUVRow_Any_SSE2;
      if (IS_ALIGNED(src_uv_width, 16)) {
        SplitUVRow = SplitUVRow_SSE2;
      }
    }
#endif
#if defined(HAS_SPLITUVROW_AVX2)
    if (TestCpuFlag(kCpuHasAVX2)) {
      SplitUVRow = SplitUVRow_Any_AVX2;
      if (IS_ALIGNED(src_uv_width, 32)) {
        SplitUVRow = SplitUVRow_AVX2;
      }
    }
#endif
#if defined(HAS_SPLITUVROW_AVX512BW)
    if (TestCpuFlag(kCpuHasAVX512BW)) {
      SplitUVRow = SplitUVRow_Any_AVX512BW;
      if (IS_ALIGNED(src_uv_width, 64)) {
        SplitUVRow = SplitUVRow_AVX512BW;
      }
    }
#endif
#if defined(HAS_SPLITUVROW_NEON)
    if (TestCpuFlag(kCpuHasNEON)) {
      SplitUVRow = SplitUVRow_Any_NEON;
      if (IS_ALIGNED(src_uv_width, 16)) {
        SplitUVRow = SplitUVRow_NEON;
      }
    }
#endif
#if defined(HAS_SPLITUVROW_SVE2)
    if (TestCpuFlag(kCpuHasSVE2)) {
      SplitUVRow = SplitUVRow_SVE2;
    }
#endif
#if defined(HAS_SPLITUVROW_LSX)
    if (TestCpuFlag(kCpuHasLSX)) {
      SplitUVRow = SplitUVRow_Any_LSX;
      if (IS_ALIGNED(src_uv_width, 32)) {
        SplitUVRow = SplitUVRow_LSX;
      }
    }
#endif
#if defined(HAS_SPLITUVROW_RVV)
    if (TestCpuFlag(kCpuHasRVV)) {
      SplitUVRow = SplitUVRow_RVV;
    }
#endif

    align_buffer_64(row_uv, ((uv_packed + 63) & ~63));
    if (!row_uv) {
      return 1;
    }
    for (y = 0; y < uv_height; ++y) {
      Convert16To8Row(src_uv, row_uv, scale16, uv_packed);
      SplitUVRow(row_uv, dst_u, dst_v, src_uv_width);
      src_uv += src_stride_uv;
      dst_u += dst_stride_u;
      dst_v += dst_stride_v;
    }
    free_aligned_buffer_64(row_uv);
    return 0;
  }

  {
    int y;
    const int scale = 16384;  // 10-bit LSB after SplitUVRow_16.
    int row_stride;
    void (*SplitUVRow_16)(const uint16_t* src_uv, uint16_t* dst_u,
                          uint16_t* dst_v, int depth, int width) =
        SplitUVRow_16_C;
    void (*Half16To8)(const uint16_t* src_uv, ptrdiff_t src_uv_stride,
                      uint8_t* dst_uv, int scale, int width) =
        subsample_x
            ? HalfRow_16To8_C
            : ((width & 1) ? HalfWidthRow_16To8_Odd_C : HalfWidthRow_16To8_C);

#if defined(HAS_SPLITUVROW_16_AVX2)
    if (TestCpuFlag(kCpuHasAVX2)) {
      SplitUVRow_16 = SplitUVRow_16_Any_AVX2;
      if (IS_ALIGNED(src_uv_width, 16)) {
        SplitUVRow_16 = SplitUVRow_16_AVX2;
      }
    }
#endif
#if defined(HAS_SPLITUVROW_16_NEON)
    if (TestCpuFlag(kCpuHasNEON)) {
      SplitUVRow_16 = SplitUVRow_16_Any_NEON;
      if (IS_ALIGNED(src_uv_width, 8)) {
        SplitUVRow_16 = SplitUVRow_16_NEON;
      }
    }
#endif
    if (subsample_x) {
#if defined(HAS_HALFROW_16TO8_NEON)
      if (TestCpuFlag(kCpuHasNEON)) {
        Half16To8 = HalfRow_16To8_Any_NEON;
        if (IS_ALIGNED(dst_uv_width, 16)) {
          Half16To8 = HalfRow_16To8_NEON;
        }
      }
#endif
#if defined(HAS_HALFROW_16TO8_SVE2)
      if (TestCpuFlag(kCpuHasSVE2)) {
        Half16To8 = HalfRow_16To8_SVE2;
      }
#endif
#if defined(HAS_HALFROW_16TO8_SME)
      if (TestCpuFlag(kCpuHasSME)) {
        Half16To8 = HalfRow_16To8_SME;
      }
#endif
#if defined(HAS_HALFROW_16TO8_SSSE3)
      if (TestCpuFlag(kCpuHasSSSE3)) {
        Half16To8 = HalfRow_16To8_Any_SSSE3;
        if (IS_ALIGNED(dst_uv_width, 16)) {
          Half16To8 = HalfRow_16To8_SSSE3;
        }
      }
#endif
#if defined(HAS_HALFROW_16TO8_AVX2)
      if (TestCpuFlag(kCpuHasAVX2)) {
        Half16To8 = HalfRow_16To8_Any_AVX2;
        if (IS_ALIGNED(dst_uv_width, 32)) {
          Half16To8 = HalfRow_16To8_AVX2;
        }
      }
#endif
#if defined(HAS_HALFROW_16TO8_AVX512BW)
      if (TestCpuFlag(kCpuHasAVX512BW)) {
        Half16To8 = HalfRow_16To8_Any_AVX512BW;
        if (IS_ALIGNED(dst_uv_width, 64)) {
          Half16To8 = HalfRow_16To8_AVX512BW;
        }
      }
#endif
#if defined(HAS_HALFROW_16TO8_RVV)
      if (TestCpuFlag(kCpuHasRVV)) {
        Half16To8 = HalfRow_16To8_RVV;
      }
#endif
    } else if (IS_ALIGNED(width, 2)) {
#if defined(HAS_HALFWIDTHROW_16TO8_NEON)
      if (TestCpuFlag(kCpuHasNEON)) {
        Half16To8 = HalfWidthRow_16To8_Any_NEON;
        if (IS_ALIGNED(dst_uv_width, 8)) {
          Half16To8 = HalfWidthRow_16To8_NEON;
        }
      }
#endif
#if defined(HAS_HALFWIDTHROW_16TO8_SVE2)
      if (TestCpuFlag(kCpuHasSVE2)) {
        Half16To8 = HalfWidthRow_16To8_SVE2;
      }
#endif
#if defined(HAS_HALFWIDTHROW_16TO8_SME)
      if (TestCpuFlag(kCpuHasSME)) {
        Half16To8 = HalfWidthRow_16To8_SME;
      }
#endif
#if defined(HAS_HALFWIDTHROW_16TO8_SSSE3)
      if (TestCpuFlag(kCpuHasSSSE3)) {
        Half16To8 = HalfWidthRow_16To8_Any_SSSE3;
        if (IS_ALIGNED(dst_uv_width, 8)) {
          Half16To8 = HalfWidthRow_16To8_SSSE3;
        }
      }
#endif
#if defined(HAS_HALFWIDTHROW_16TO8_AVX2)
      if (TestCpuFlag(kCpuHasAVX2)) {
        Half16To8 = HalfWidthRow_16To8_Any_AVX2;
        if (IS_ALIGNED(dst_uv_width, 16)) {
          Half16To8 = HalfWidthRow_16To8_AVX2;
        }
      }
#endif
#if defined(HAS_HALFWIDTHROW_16TO8_AVX512BW)
      if (TestCpuFlag(kCpuHasAVX512BW)) {
        Half16To8 = HalfWidthRow_16To8_Any_AVX512BW;
        if (IS_ALIGNED(dst_uv_width, 32)) {
          Half16To8 = HalfWidthRow_16To8_AVX512BW;
        }
      }
#endif
#if defined(HAS_HALFWIDTHROW_16TO8_RVV)
      if (TestCpuFlag(kCpuHasRVV)) {
        Half16To8 = HalfWidthRow_16To8_RVV;
      }
#endif
    }

    row_stride = (src_uv_width + 31) & ~31;
    align_buffer_64(row_u, (size_t)row_stride * sizeof(uint16_t) * 4u);
    if (!row_u) {
      return 1;
    }
    {
      uint16_t* row_u0 = (uint16_t*)row_u;
      uint16_t* row_u1 = row_u0 + row_stride;
      uint16_t* row_v0 = row_u1 + row_stride;
      uint16_t* row_v1 = row_v0 + row_stride;
      for (y = 0; y < height - 1; y += 2) {
        SplitUVRow_16(src_uv, row_u0, row_v0, 10, src_uv_width);
        SplitUVRow_16(src_uv + src_stride_uv, row_u1, row_v1, 10, src_uv_width);
        Half16To8(row_u0, row_stride, dst_u, scale, dst_uv_width);
        Half16To8(row_v0, row_stride, dst_v, scale, dst_uv_width);
        src_uv += src_stride_uv * 2;
        dst_u += dst_stride_u;
        dst_v += dst_stride_v;
      }
      if (height & 1) {
        SplitUVRow_16(src_uv, row_u0, row_v0, 10, src_uv_width);
        Half16To8(row_u0, 0, dst_u, scale, dst_uv_width);
        Half16To8(row_v0, 0, dst_v, scale, dst_uv_width);
      }
    }
    free_aligned_buffer_64(row_u);
    return 0;
  }
}

LIBYUV_API
int P010ToI420(const uint16_t* src_y,
               int src_stride_y,
               const uint16_t* src_uv,
               int src_stride_uv,
               uint8_t* dst_y,
               int dst_stride_y,
               uint8_t* dst_u,
               int dst_stride_u,
               uint8_t* dst_v,
               int dst_stride_v,
               int width,
               int height) {
  return Px10ToI420(src_y, src_stride_y, src_uv, src_stride_uv, dst_y,
                    dst_stride_y, dst_u, dst_stride_u, dst_v, dst_stride_v,
                    width, height, 1, 1);
}

LIBYUV_API
int P210ToI420(const uint16_t* src_y,
               int src_stride_y,
               const uint16_t* src_uv,
               int src_stride_uv,
               uint8_t* dst_y,
               int dst_stride_y,
               uint8_t* dst_u,
               int dst_stride_u,
               uint8_t* dst_v,
               int dst_stride_v,
               int width,
               int height) {
  return Px10ToI420(src_y, src_stride_y, src_uv, src_stride_uv, dst_y,
                    dst_stride_y, dst_u, dst_stride_u, dst_v, dst_stride_v,
                    width, height, 1, 0);
}

LIBYUV_API
int P410ToI420(const uint16_t* src_y,
               int src_stride_y,
               const uint16_t* src_uv,
               int src_stride_uv,
               uint8_t* dst_y,
               int dst_stride_y,
               uint8_t* dst_u,
               int dst_stride_u,
               uint8_t* dst_v,
               int dst_stride_v,
               int width,
               int height) {
  return Px10ToI420(src_y, src_stride_y, src_uv, src_stride_uv, dst_y,
                    dst_stride_y, dst_u, dst_stride_u, dst_v, dst_stride_v,
                    width, height, 0, 0);
}

LIBYUV_API
int P012ToI012(const uint16_t* src_y,
               int src_stride_y,
               const uint16_t* src_uv,
               int src_stride_uv,
               uint16_t* dst_y,
               int dst_stride_y,
               uint16_t* dst_u,
               int dst_stride_u,
               uint16_t* dst_v,
               int dst_stride_v,
               int width,
               int height) {
  return PxxxToIxxx(src_y, src_stride_y, src_uv, src_stride_uv, dst_y,
                    dst_stride_y, dst_u, dst_stride_u, dst_v, dst_stride_v,
                    width, height, 1, 1, 12);
}

LIBYUV_API
int P010ToP410(const uint16_t* src_y,
               int src_stride_y,
               const uint16_t* src_uv,
               int src_stride_uv,
               uint16_t* dst_y,
               int dst_stride_y,
               uint16_t* dst_uv,
               int dst_stride_uv,
               int width,
               int height) {
  int r;
  if ((!src_y && dst_y) || !src_uv || !dst_uv || width <= 0 || height == 0 ||
      height == INT_MIN) {
    return -1;
  }

  if (dst_y) {
    CopyPlane_16(src_y, src_stride_y, dst_y, dst_stride_y, width, height);
  }
  r = UVScale_16(src_uv, src_stride_uv, SUBSAMPLE(width, 1, 1),
                 SUBSAMPLE(height, 1, 1), dst_uv, dst_stride_uv, Abs(width),
                 Abs(height), kFilterBilinear);
  return r;
}

LIBYUV_API
int P210ToP410(const uint16_t* src_y,
               int src_stride_y,
               const uint16_t* src_uv,
               int src_stride_uv,
               uint16_t* dst_y,
               int dst_stride_y,
               uint16_t* dst_uv,
               int dst_stride_uv,
               int width,
               int height) {
  int r;
  if ((!src_y && dst_y) || !src_uv || !dst_uv || width <= 0 || height == 0 ||
      height == INT_MIN) {
    return -1;
  }

  if (dst_y) {
    CopyPlane_16(src_y, src_stride_y, dst_y, dst_stride_y, width, height);
  }
  r = UVScale_16(src_uv, src_stride_uv, SUBSAMPLE(width, 1, 1), height, dst_uv,
                 dst_stride_uv, Abs(width), Abs(height), kFilterBilinear);
  return r;
}

// Convert YUY2 to I420.
LIBYUV_API
int YUY2ToI420(const uint8_t* src_yuy2,
               int src_stride_yuy2,
               uint8_t* dst_y,
               int dst_stride_y,
               uint8_t* dst_u,
               int dst_stride_u,
               uint8_t* dst_v,
               int dst_stride_v,
               int width,
               int height) {
  int y;
  void (*YUY2ToUVRow)(const uint8_t* src_yuy2, int src_stride_yuy2,
                      uint8_t* dst_u, uint8_t* dst_v, int width) =
      YUY2ToUVRow_C;
  void (*YUY2ToYRow)(const uint8_t* src_yuy2, uint8_t* dst_y, int width) =
      YUY2ToYRow_C;
  if (width <= 0 || height == 0 || height == INT_MIN) {
    return -1;
  }
  // Negative height means invert the image.
  if (height < 0) {
    height = -height;
    src_yuy2 = src_yuy2 + (ptrdiff_t)(height - 1) * src_stride_yuy2;
    src_stride_yuy2 = -src_stride_yuy2;
  }
#if defined(HAS_YUY2TOYROW_SSE2)
  if (TestCpuFlag(kCpuHasSSE2)) {
    YUY2ToUVRow = YUY2ToUVRow_Any_SSE2;
    YUY2ToYRow = YUY2ToYRow_Any_SSE2;
    if (IS_ALIGNED(width, 16)) {
      YUY2ToUVRow = YUY2ToUVRow_SSE2;
      YUY2ToYRow = YUY2ToYRow_SSE2;
    }
  }
#endif
#if defined(HAS_YUY2TOYROW_AVX2)
  if (TestCpuFlag(kCpuHasAVX2)) {
    YUY2ToUVRow = YUY2ToUVRow_Any_AVX2;
    YUY2ToYRow = YUY2ToYRow_Any_AVX2;
    if (IS_ALIGNED(width, 32)) {
      YUY2ToUVRow = YUY2ToUVRow_AVX2;
      YUY2ToYRow = YUY2ToYRow_AVX2;
    }
  }
#endif
#if defined(HAS_YUY2TOYROW_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    YUY2ToYRow = YUY2ToYRow_Any_NEON;
    YUY2ToUVRow = YUY2ToUVRow_Any_NEON;
    if (IS_ALIGNED(width, 16)) {
      YUY2ToYRow = YUY2ToYRow_NEON;
      YUY2ToUVRow = YUY2ToUVRow_NEON;
    }
  }
#endif
#if defined(HAS_YUY2TOYROW_SVE2) && defined(HAS_YUY2TOUVROW_SVE2)
  if (TestCpuFlag(kCpuHasSVE2)) {
    YUY2ToYRow = YUY2ToYRow_SVE2;
    YUY2ToUVRow = YUY2ToUVRow_SVE2;
  }
#endif
#if defined(HAS_YUY2TOYROW_LSX) && defined(HAS_YUY2TOUVROW_LSX)
  if (TestCpuFlag(kCpuHasLSX)) {
    YUY2ToYRow = YUY2ToYRow_Any_LSX;
    YUY2ToUVRow = YUY2ToUVRow_Any_LSX;
    if (IS_ALIGNED(width, 16)) {
      YUY2ToYRow = YUY2ToYRow_LSX;
      YUY2ToUVRow = YUY2ToUVRow_LSX;
    }
  }
#endif
#if defined(HAS_YUY2TOYROW_LASX) && defined(HAS_YUY2TOUVROW_LASX)
  if (TestCpuFlag(kCpuHasLASX)) {
    YUY2ToYRow = YUY2ToYRow_Any_LASX;
    YUY2ToUVRow = YUY2ToUVRow_Any_LASX;
    if (IS_ALIGNED(width, 32)) {
      YUY2ToYRow = YUY2ToYRow_LASX;
      YUY2ToUVRow = YUY2ToUVRow_LASX;
    }
  }
#endif

  for (y = 0; y < height - 1; y += 2) {
    YUY2ToUVRow(src_yuy2, src_stride_yuy2, dst_u, dst_v, width);
    YUY2ToYRow(src_yuy2, dst_y, width);
    YUY2ToYRow(src_yuy2 + src_stride_yuy2, dst_y + dst_stride_y, width);
    src_yuy2 += src_stride_yuy2 * 2;
    dst_y += dst_stride_y * 2;
    dst_u += dst_stride_u;
    dst_v += dst_stride_v;
  }
  if (height & 1) {
    YUY2ToUVRow(src_yuy2, 0, dst_u, dst_v, width);
    YUY2ToYRow(src_yuy2, dst_y, width);
  }
  return 0;
}

// Convert UYVY to I420.
LIBYUV_API
int UYVYToI420(const uint8_t* src_uyvy,
               int src_stride_uyvy,
               uint8_t* dst_y,
               int dst_stride_y,
               uint8_t* dst_u,
               int dst_stride_u,
               uint8_t* dst_v,
               int dst_stride_v,
               int width,
               int height) {
  int y;
  void (*UYVYToUVRow)(const uint8_t* src_uyvy, int src_stride_uyvy,
                      uint8_t* dst_u, uint8_t* dst_v, int width) =
      UYVYToUVRow_C;
  void (*UYVYToYRow)(const uint8_t* src_uyvy, uint8_t* dst_y, int width) =
      UYVYToYRow_C;
  if (width <= 0 || height == 0 || height == INT_MIN) {
    return -1;
  }
  // Negative height means invert the image.
  if (height < 0) {
    height = -height;
    src_uyvy = src_uyvy + (ptrdiff_t)(height - 1) * src_stride_uyvy;
    src_stride_uyvy = -src_stride_uyvy;
  }
#if defined(HAS_UYVYTOYROW_SSE2)
  if (TestCpuFlag(kCpuHasSSE2)) {
    UYVYToUVRow = UYVYToUVRow_Any_SSE2;
    UYVYToYRow = UYVYToYRow_Any_SSE2;
    if (IS_ALIGNED(width, 16)) {
      UYVYToUVRow = UYVYToUVRow_SSE2;
      UYVYToYRow = UYVYToYRow_SSE2;
    }
  }
#endif
#if defined(HAS_UYVYTOYROW_AVX2)
  if (TestCpuFlag(kCpuHasAVX2)) {
    UYVYToUVRow = UYVYToUVRow_Any_AVX2;
    UYVYToYRow = UYVYToYRow_Any_AVX2;
    if (IS_ALIGNED(width, 32)) {
      UYVYToUVRow = UYVYToUVRow_AVX2;
      UYVYToYRow = UYVYToYRow_AVX2;
    }
  }
#endif
#if defined(HAS_UYVYTOYROW_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    UYVYToYRow = UYVYToYRow_Any_NEON;
    UYVYToUVRow = UYVYToUVRow_Any_NEON;
    if (IS_ALIGNED(width, 16)) {
      UYVYToYRow = UYVYToYRow_NEON;
      UYVYToUVRow = UYVYToUVRow_NEON;
    }
  }
#endif
#if defined(HAS_UYVYTOYROW_SVE2) && defined(HAS_UYVYTOUVROW_SVE2)
  if (TestCpuFlag(kCpuHasSVE2)) {
    UYVYToYRow = UYVYToYRow_SVE2;
    UYVYToUVRow = UYVYToUVRow_SVE2;
  }
#endif
#if defined(HAS_UYVYTOYROW_LSX)
  if (TestCpuFlag(kCpuHasLSX)) {
    UYVYToYRow = UYVYToYRow_Any_LSX;
    UYVYToUVRow = UYVYToUVRow_Any_LSX;
    if (IS_ALIGNED(width, 16)) {
      UYVYToYRow = UYVYToYRow_LSX;
      UYVYToUVRow = UYVYToUVRow_LSX;
    }
  }
#endif
#if defined(HAS_UYVYTOYROW_LSX)
  if (TestCpuFlag(kCpuHasLSX)) {
    UYVYToYRow = UYVYToYRow_Any_LSX;
    UYVYToUVRow = UYVYToUVRow_Any_LSX;
    if (IS_ALIGNED(width, 16)) {
      UYVYToYRow = UYVYToYRow_LSX;
      UYVYToUVRow = UYVYToUVRow_LSX;
    }
  }
#endif
#if defined(HAS_UYVYTOYROW_LASX)
  if (TestCpuFlag(kCpuHasLASX)) {
    UYVYToYRow = UYVYToYRow_Any_LASX;
    UYVYToUVRow = UYVYToUVRow_Any_LASX;
    if (IS_ALIGNED(width, 32)) {
      UYVYToYRow = UYVYToYRow_LASX;
      UYVYToUVRow = UYVYToUVRow_LASX;
    }
  }
#endif

  for (y = 0; y < height - 1; y += 2) {
    UYVYToUVRow(src_uyvy, src_stride_uyvy, dst_u, dst_v, width);
    UYVYToYRow(src_uyvy, dst_y, width);
    UYVYToYRow(src_uyvy + src_stride_uyvy, dst_y + dst_stride_y, width);
    src_uyvy += src_stride_uyvy * 2;
    dst_y += dst_stride_y * 2;
    dst_u += dst_stride_u;
    dst_v += dst_stride_v;
  }
  if (height & 1) {
    UYVYToUVRow(src_uyvy, 0, dst_u, dst_v, width);
    UYVYToYRow(src_uyvy, dst_y, width);
  }
  return 0;
}

// Convert AYUV to NV12.
LIBYUV_API
int AYUVToNV12(const uint8_t* src_ayuv,
               int src_stride_ayuv,
               uint8_t* dst_y,
               int dst_stride_y,
               uint8_t* dst_uv,
               int dst_stride_uv,
               int width,
               int height) {
  int y;
  void (*AYUVToUVRow)(const uint8_t* src_ayuv, int src_stride_ayuv,
                      uint8_t* dst_uv, int width) = AYUVToUVRow_C;
  void (*AYUVToYRow)(const uint8_t* src_ayuv, uint8_t* dst_y, int width) =
      AYUVToYRow_C;
  if (width <= 0 || height == 0 || height == INT_MIN) {
    return -1;
  }
  // Negative height means invert the image.
  if (height < 0) {
    height = -height;
    src_ayuv = src_ayuv + (ptrdiff_t)(height - 1) * src_stride_ayuv;
    src_stride_ayuv = -src_stride_ayuv;
  }
// place holders for future intel code
#if defined(HAS_AYUVTOYROW_SSE2)
  if (TestCpuFlag(kCpuHasSSE2)) {
    AYUVToUVRow = AYUVToUVRow_Any_SSE2;
    AYUVToYRow = AYUVToYRow_Any_SSE2;
    if (IS_ALIGNED(width, 16)) {
      AYUVToUVRow = AYUVToUVRow_SSE2;
      AYUVToYRow = AYUVToYRow_SSE2;
    }
  }
#endif
#if defined(HAS_AYUVTOYROW_AVX2)
  if (TestCpuFlag(kCpuHasAVX2)) {
    AYUVToUVRow = AYUVToUVRow_Any_AVX2;
    AYUVToYRow = AYUVToYRow_Any_AVX2;
    if (IS_ALIGNED(width, 32)) {
      AYUVToUVRow = AYUVToUVRow_AVX2;
      AYUVToYRow = AYUVToYRow_AVX2;
    }
  }
#endif

#if defined(HAS_AYUVTOYROW_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    AYUVToYRow = AYUVToYRow_Any_NEON;
    AYUVToUVRow = AYUVToUVRow_Any_NEON;
    if (IS_ALIGNED(width, 16)) {
      AYUVToYRow = AYUVToYRow_NEON;
      AYUVToUVRow = AYUVToUVRow_NEON;
    }
  }
#endif
#if defined(HAS_AYUVTOUVROW_SVE2)
  if (TestCpuFlag(kCpuHasSVE2)) {
    AYUVToUVRow = AYUVToUVRow_Any_SVE2;
    if (IS_ALIGNED(width, 2)) {
      AYUVToUVRow = AYUVToUVRow_SVE2;
    }
  }
#endif

  for (y = 0; y < height - 1; y += 2) {
    AYUVToUVRow(src_ayuv, src_stride_ayuv, dst_uv, width);
    AYUVToYRow(src_ayuv, dst_y, width);
    AYUVToYRow(src_ayuv + src_stride_ayuv, dst_y + dst_stride_y, width);
    src_ayuv += src_stride_ayuv * 2;
    dst_y += dst_stride_y * 2;
    dst_uv += dst_stride_uv;
  }
  if (height & 1) {
    AYUVToUVRow(src_ayuv, 0, dst_uv, width);
    AYUVToYRow(src_ayuv, dst_y, width);
  }
  return 0;
}

// Convert AYUV to NV21.
LIBYUV_API
int AYUVToNV21(const uint8_t* src_ayuv,
               int src_stride_ayuv,
               uint8_t* dst_y,
               int dst_stride_y,
               uint8_t* dst_vu,
               int dst_stride_vu,
               int width,
               int height) {
  int y;
  void (*AYUVToVURow)(const uint8_t* src_ayuv, int src_stride_ayuv,
                      uint8_t* dst_vu, int width) = AYUVToVURow_C;
  void (*AYUVToYRow)(const uint8_t* src_ayuv, uint8_t* dst_y, int width) =
      AYUVToYRow_C;
  if (width <= 0 || height == 0 || height == INT_MIN) {
    return -1;
  }
  // Negative height means invert the image.
  if (height < 0) {
    height = -height;
    src_ayuv = src_ayuv + (ptrdiff_t)(height - 1) * src_stride_ayuv;
    src_stride_ayuv = -src_stride_ayuv;
  }
// place holders for future intel code
#if defined(HAS_AYUVTOYROW_SSE2)
  if (TestCpuFlag(kCpuHasSSE2)) {
    AYUVToVURow = AYUVToVURow_Any_SSE2;
    AYUVToYRow = AYUVToYRow_Any_SSE2;
    if (IS_ALIGNED(width, 16)) {
      AYUVToVURow = AYUVToVURow_SSE2;
      AYUVToYRow = AYUVToYRow_SSE2;
    }
  }
#endif
#if defined(HAS_AYUVTOYROW_AVX2)
  if (TestCpuFlag(kCpuHasAVX2)) {
    AYUVToVURow = AYUVToVURow_Any_AVX2;
    AYUVToYRow = AYUVToYRow_Any_AVX2;
    if (IS_ALIGNED(width, 32)) {
      AYUVToVURow = AYUVToVURow_AVX2;
      AYUVToYRow = AYUVToYRow_AVX2;
    }
  }
#endif

#if defined(HAS_AYUVTOYROW_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    AYUVToYRow = AYUVToYRow_Any_NEON;
    AYUVToVURow = AYUVToVURow_Any_NEON;
    if (IS_ALIGNED(width, 16)) {
      AYUVToYRow = AYUVToYRow_NEON;
      AYUVToVURow = AYUVToVURow_NEON;
    }
  }
#endif
#if defined(HAS_AYUVTOVUROW_SVE2)
  if (TestCpuFlag(kCpuHasSVE2)) {
    AYUVToVURow = AYUVToVURow_Any_SVE2;
    if (IS_ALIGNED(width, 2)) {
      AYUVToVURow = AYUVToVURow_SVE2;
    }
  }
#endif

  for (y = 0; y < height - 1; y += 2) {
    AYUVToVURow(src_ayuv, src_stride_ayuv, dst_vu, width);
    AYUVToYRow(src_ayuv, dst_y, width);
    AYUVToYRow(src_ayuv + src_stride_ayuv, dst_y + dst_stride_y, width);
    src_ayuv += src_stride_ayuv * 2;
    dst_y += dst_stride_y * 2;
    dst_vu += dst_stride_vu;
  }
  if (height & 1) {
    AYUVToVURow(src_ayuv, 0, dst_vu, width);
    AYUVToYRow(src_ayuv, dst_y, width);
  }
  return 0;
}

// Convert ARGB to I420.
LIBYUV_API
int ARGBToI420(const uint8_t* src_argb,
               int src_stride_argb,
               uint8_t* dst_y,
               int dst_stride_y,
               uint8_t* dst_u,
               int dst_stride_u,
               uint8_t* dst_v,
               int dst_stride_v,
               int width,
               int height) {
  return ARGBToI420Matrix(src_argb, src_stride_argb, dst_y, dst_stride_y, dst_u,
                          dst_stride_u, dst_v, dst_stride_v,
                          &kArgbI601Constants, width, height);
}

LIBYUV_API
int ARGBToI420Matrix(const uint8_t* src_argb,
                     int src_stride_argb,
                     uint8_t* dst_y,
                     int dst_stride_y,
                     uint8_t* dst_u,
                     int dst_stride_u,
                     uint8_t* dst_v,
                     int dst_stride_v,
                     const struct ArgbConstants* argbconstants,
                     int width,
                     int height) {
  int y;
  void (*ARGBToYMatrixRow)(const uint8_t* src_argb, uint8_t* dst_y, int width,
                           const struct ArgbConstants* c) = ARGBToYMatrixRow_C;
  void (*ARGBToUVMatrixRow)(const uint8_t* src_argb, int src_stride_argb,
                            uint8_t* dst_u, uint8_t* dst_v, int width,
                            const struct ArgbConstants* c) =
      ARGBToUVMatrixRow_C;

#if defined(HAS_ARGBTOYMATRIXROW_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3)) {
    ARGBToYMatrixRow = ARGBToYMatrixRow_Any_SSSE3;
    if (IS_ALIGNED(width, 16)) {
      ARGBToYMatrixRow = ARGBToYMatrixRow_SSSE3;
    }
  }
#endif
#if defined(HAS_ARGBTOYMATRIXROW_AVX2)
  if (TestCpuFlag(kCpuHasAVX2)) {
    ARGBToYMatrixRow = ARGBToYMatrixRow_Any_AVX2;
    if (IS_ALIGNED(width, 32)) {
      ARGBToYMatrixRow = ARGBToYMatrixRow_AVX2;
    }
  }
#endif
#if defined(HAS_ARGBTOYMATRIXROW_AVX512BW)
  if (TestCpuFlag(kCpuHasAVX512BW)) {
    ARGBToYMatrixRow = ARGBToYMatrixRow_Any_AVX512BW;
    if (IS_ALIGNED(width, 64)) {
      ARGBToYMatrixRow = ARGBToYMatrixRow_AVX512BW;
    }
  }
#endif
#if defined(HAS_ARGBTOYMATRIXROW_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    ARGBToYMatrixRow = ARGBToYMatrixRow_Any_NEON;
    if (IS_ALIGNED(width, 16)) {
      ARGBToYMatrixRow = ARGBToYMatrixRow_NEON;
    }
  }
#endif
#if defined(HAS_ARGBTOYMATRIXROW_NEON_DOTPROD)
  if (TestCpuFlag(kCpuHasNeonDotProd)) {
    ARGBToYMatrixRow = ARGBToYMatrixRow_Any_NEON_DotProd;
    if (IS_ALIGNED(width, 16)) {
      ARGBToYMatrixRow = ARGBToYMatrixRow_NEON_DotProd;
    }
  }
#endif
#if defined(HAS_ARGBTOYMATRIXROW_SVE2)
  if (TestCpuFlag(kCpuHasSVE2)) {
    ARGBToYMatrixRow = ARGBToYMatrixRow_SVE2;
  }
#endif
#if defined(HAS_ARGBTOYMATRIXROW_SME)
  if (TestCpuFlag(kCpuHasSME)) {
    ARGBToYMatrixRow = ARGBToYMatrixRow_SME;
  }
#endif
#if defined(HAS_ARGBTOYMATRIXROW_LSX)
  if (TestCpuFlag(kCpuHasLSX)) {
    ARGBToYMatrixRow = ARGBToYMatrixRow_Any_LSX;
    if (IS_ALIGNED(width, 16)) {
      ARGBToYMatrixRow = ARGBToYMatrixRow_LSX;
    }
  }
#endif
#if defined(HAS_ARGBTOYMATRIXROW_LASX)
  if (TestCpuFlag(kCpuHasLASX)) {
    ARGBToYMatrixRow = ARGBToYMatrixRow_Any_LASX;
    if (IS_ALIGNED(width, 32)) {
      ARGBToYMatrixRow = ARGBToYMatrixRow_LASX;
    }
  }
#endif
#if defined(HAS_ARGBTOYMATRIXROW_RVV)
  if (TestCpuFlag(kCpuHasRVV)) {
    ARGBToYMatrixRow = ARGBToYMatrixRow_RVV;
  }
#endif
#if defined(HAS_ARGBTOYMATRIXROW_WASMSIMD)
  if (TestCpuFlag(kCpuHasWASMSIMD)) {
    ARGBToYMatrixRow = ARGBToYMatrixRow_Any_WASMSIMD;
    if (IS_ALIGNED(width, 16)) {
      ARGBToYMatrixRow = ARGBToYMatrixRow_WASMSIMD;
    }
  }
#endif

#if defined(HAS_ARGBTOUVMATRIXROW_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    ARGBToUVMatrixRow = ARGBToUVMatrixRow_Any_NEON;
    if (IS_ALIGNED(width, 16)) {
      ARGBToUVMatrixRow = ARGBToUVMatrixRow_NEON;
    }
  }
#endif
#if defined(HAS_ARGBTOUVMATRIXROW_NEON_I8MM)
  if (TestCpuFlag(kCpuHasNeonI8MM)) {
    ARGBToUVMatrixRow = ARGBToUVMatrixRow_Any_NEON_I8MM;
    if (IS_ALIGNED(width, 16)) {
      ARGBToUVMatrixRow = ARGBToUVMatrixRow_NEON_I8MM;
    }
  }
#endif
#if defined(HAS_ARGBTOUVMATRIXROW_SVE2)
  if (TestCpuFlag(kCpuHasSVE2)) {
    if (IS_ALIGNED(width, 2)) {
      ARGBToUVMatrixRow = ARGBToUVMatrixRow_SVE2;
    }
  }
#endif
#if defined(HAS_ARGBTOUVMATRIXROW_SME)
  if (TestCpuFlag(kCpuHasSME)) {
    if (IS_ALIGNED(width, 2)) {
      ARGBToUVMatrixRow = ARGBToUVMatrixRow_SME;
    }
  }
#endif
#if defined(HAS_ARGBTOUVMATRIXROW_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3)) {
    ARGBToUVMatrixRow = ARGBToUVMatrixRow_Any_SSSE3;
    if (IS_ALIGNED(width, 8)) {
      ARGBToUVMatrixRow = ARGBToUVMatrixRow_SSSE3;
    }
  }
#endif
#if defined(HAS_ARGBTOUVMATRIXROW_AVX2)
  if (TestCpuFlag(kCpuHasAVX2)) {
    ARGBToUVMatrixRow = ARGBToUVMatrixRow_Any_AVX2;
    if (IS_ALIGNED(width, 32)) {
      ARGBToUVMatrixRow = ARGBToUVMatrixRow_AVX2;
    }
  }
#endif
#if defined(HAS_ARGBTOUVMATRIXROW_AVX512BW)
  if (TestCpuFlag(kCpuHasAVX512BW)) {
    ARGBToUVMatrixRow = ARGBToUVMatrixRow_Any_AVX512BW;
    if (IS_ALIGNED(width, 64)) {
      ARGBToUVMatrixRow = ARGBToUVMatrixRow_AVX512BW;
    }
  }
#endif
#if defined(HAS_ARGBTOUVMATRIXROW_RVV)
  if (TestCpuFlag(kCpuHasRVV)) {
    ARGBToUVMatrixRow = ARGBToUVMatrixRow_RVV;
  }
#endif
#if defined(HAS_ARGBTOUVMATRIXROW_WASMSIMD)
  if (TestCpuFlag(kCpuHasWASMSIMD)) {
    ARGBToUVMatrixRow = ARGBToUVMatrixRow_Any_WASMSIMD;
    if (IS_ALIGNED(width, 16)) {
      ARGBToUVMatrixRow = ARGBToUVMatrixRow_WASMSIMD;
    }
  }
#endif
  if (!src_argb || !dst_y || !dst_u || !dst_v || !argbconstants || width <= 0 ||
      height == 0 || height == INT_MIN) {
    return -1;
  }
  // Negative height means invert the image.
  if (height < 0) {
    height = -height;
    src_argb = src_argb + (ptrdiff_t)(height - 1) * src_stride_argb;
    src_stride_argb = -src_stride_argb;
  }

  for (y = 0; y < height - 1; y += 2) {
    ARGBToUVMatrixRow(src_argb, src_stride_argb, dst_u, dst_v, width,
                      argbconstants);
    ARGBToYMatrixRow(src_argb, dst_y, width, argbconstants);
    ARGBToYMatrixRow(src_argb + src_stride_argb, dst_y + dst_stride_y, width,
                     argbconstants);
    src_argb += src_stride_argb * 2;
    dst_y += dst_stride_y * 2;
    dst_u += dst_stride_u;
    dst_v += dst_stride_v;
  }
  if (height & 1) {
    ARGBToUVMatrixRow(src_argb, 0, dst_u, dst_v, width, argbconstants);
    ARGBToYMatrixRow(src_argb, dst_y, width, argbconstants);
  }
  return 0;
}

#ifdef USE_EXTRACTALPHA
// Convert ARGB to I420 with Alpha
// The following version calls ARGBExtractAlpha on the full image.
LIBYUV_API
int ARGBToI420Alpha(const uint8_t* src_argb,
                    int src_stride_argb,
                    uint8_t* dst_y,
                    int dst_stride_y,
                    uint8_t* dst_u,
                    int dst_stride_u,
                    uint8_t* dst_v,
                    int dst_stride_v,
                    uint8_t* dst_a,
                    int dst_stride_a,
                    int width,
                    int height) {
  int r = ARGBToI420(src_argb, src_stride_argb, dst_y, dst_stride_y, dst_u,
                     dst_stride_u, dst_v, dst_stride_v, width, height);
  if (r == 0) {
    r = ARGBExtractAlpha(src_argb, src_stride_argb, dst_a, dst_stride_a, width,
                         height);
  }
  return r;
}
#else  // USE_EXTRACTALPHA
// Convert ARGB to I420 with Alpha
LIBYUV_API
int ARGBToI420Alpha(const uint8_t* src_argb,
                    int src_stride_argb,
                    uint8_t* dst_y,
                    int dst_stride_y,
                    uint8_t* dst_u,
                    int dst_stride_u,
                    uint8_t* dst_v,
                    int dst_stride_v,
                    uint8_t* dst_a,
                    int dst_stride_a,
                    int width,
                    int height) {
  int y;
  void (*ARGBToYMatrixRow)(const uint8_t* src_argb, uint8_t* dst_y, int width,
                           const struct ArgbConstants* c) = ARGBToYMatrixRow_C;
  void (*ARGBToUVMatrixRow)(const uint8_t* src_argb, int src_stride_argb,
                            uint8_t* dst_u, uint8_t* dst_v, int width,
                            const struct ArgbConstants* c) =
      ARGBToUVMatrixRow_C;
  void (*ARGBExtractAlphaRow)(const uint8_t* src_argb, uint8_t* dst_a,
                              int width) = ARGBExtractAlphaRow_C;
  if (!src_argb || !dst_y || !dst_u || !dst_v || !dst_a || width <= 0 ||
      height == 0 || height == INT_MIN) {
    return -1;
  }
  // Negative height means invert the image.
  if (height < 0) {
    height = -height;
    src_argb = src_argb + (ptrdiff_t)(height - 1) * src_stride_argb;
    src_stride_argb = -src_stride_argb;
  }

#if defined(HAS_ARGBTOYMATRIXROW_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3)) {
    ARGBToYMatrixRow = ARGBToYMatrixRow_Any_SSSE3;
    if (IS_ALIGNED(width, 16)) {
      ARGBToYMatrixRow = ARGBToYMatrixRow_SSSE3;
    }
  }
#endif
#if defined(HAS_ARGBTOYMATRIXROW_AVX2)
  if (TestCpuFlag(kCpuHasAVX2)) {
    ARGBToYMatrixRow = ARGBToYMatrixRow_Any_AVX2;
    if (IS_ALIGNED(width, 32)) {
      ARGBToYMatrixRow = ARGBToYMatrixRow_AVX2;
    }
  }
#endif
#if defined(HAS_ARGBTOYMATRIXROW_AVX512BW)
  if (TestCpuFlag(kCpuHasAVX512BW)) {
    ARGBToYMatrixRow = ARGBToYMatrixRow_Any_AVX512BW;
    if (IS_ALIGNED(width, 64)) {
      ARGBToYMatrixRow = ARGBToYMatrixRow_AVX512BW;
    }
  }
#endif
#if defined(HAS_ARGBTOYMATRIXROW_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    ARGBToYMatrixRow = ARGBToYMatrixRow_Any_NEON;
    if (IS_ALIGNED(width, 16)) {
      ARGBToYMatrixRow = ARGBToYMatrixRow_NEON;
    }
  }
#endif
#if defined(HAS_ARGBTOYMATRIXROW_NEON_DOTPROD)
  if (TestCpuFlag(kCpuHasNeonDotProd)) {
    ARGBToYMatrixRow = ARGBToYMatrixRow_Any_NEON_DotProd;
    if (IS_ALIGNED(width, 16)) {
      ARGBToYMatrixRow = ARGBToYMatrixRow_NEON_DotProd;
    }
  }
#endif
#if defined(HAS_ARGBTOYMATRIXROW_SVE2)
  if (TestCpuFlag(kCpuHasSVE2)) {
    ARGBToYMatrixRow = ARGBToYMatrixRow_SVE2;
  }
#endif
#if defined(HAS_ARGBTOYMATRIXROW_SME)
  if (TestCpuFlag(kCpuHasSME)) {
    ARGBToYMatrixRow = ARGBToYMatrixRow_SME;
  }
#endif
#if defined(HAS_ARGBTOYMATRIXROW_LSX)
  if (TestCpuFlag(kCpuHasLSX)) {
    ARGBToYMatrixRow = ARGBToYMatrixRow_Any_LSX;
    if (IS_ALIGNED(width, 16)) {
      ARGBToYMatrixRow = ARGBToYMatrixRow_LSX;
    }
  }
#endif
#if defined(HAS_ARGBTOYMATRIXROW_LASX)
  if (TestCpuFlag(kCpuHasLASX)) {
    ARGBToYMatrixRow = ARGBToYMatrixRow_Any_LASX;
    if (IS_ALIGNED(width, 32)) {
      ARGBToYMatrixRow = ARGBToYMatrixRow_LASX;
    }
  }
#endif
#if defined(HAS_ARGBTOYMATRIXROW_RVV)
  if (TestCpuFlag(kCpuHasRVV)) {
    ARGBToYMatrixRow = ARGBToYMatrixRow_RVV;
  }
#endif
#if defined(HAS_ARGBTOYMATRIXROW_WASMSIMD)
  if (TestCpuFlag(kCpuHasWASMSIMD)) {
    ARGBToYMatrixRow = ARGBToYMatrixRow_Any_WASMSIMD;
    if (IS_ALIGNED(width, 16)) {
      ARGBToYMatrixRow = ARGBToYMatrixRow_WASMSIMD;
    }
  }
#endif

#if defined(HAS_ARGBTOUVMATRIXROW_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    ARGBToUVMatrixRow = ARGBToUVMatrixRow_Any_NEON;
    if (IS_ALIGNED(width, 16)) {
      ARGBToUVMatrixRow = ARGBToUVMatrixRow_NEON;
    }
  }
#endif
#if defined(HAS_ARGBTOUVMATRIXROW_NEON_I8MM)
  if (TestCpuFlag(kCpuHasNeonI8MM)) {
    ARGBToUVMatrixRow = ARGBToUVMatrixRow_Any_NEON_I8MM;
    if (IS_ALIGNED(width, 16)) {
      ARGBToUVMatrixRow = ARGBToUVMatrixRow_NEON_I8MM;
    }
  }
#endif
#if defined(HAS_ARGBTOUVMATRIXROW_SVE2)
  if (TestCpuFlag(kCpuHasSVE2)) {
    if (IS_ALIGNED(width, 2)) {
      ARGBToUVMatrixRow = ARGBToUVMatrixRow_SVE2;
    }
  }
#endif
#if defined(HAS_ARGBTOUVMATRIXROW_SME)
  if (TestCpuFlag(kCpuHasSME)) {
    if (IS_ALIGNED(width, 2)) {
      ARGBToUVMatrixRow = ARGBToUVMatrixRow_SME;
    }
  }
#endif
#if defined(HAS_ARGBTOUVMATRIXROW_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3)) {
    ARGBToUVMatrixRow = ARGBToUVMatrixRow_Any_SSSE3;
    if (IS_ALIGNED(width, 8)) {
      ARGBToUVMatrixRow = ARGBToUVMatrixRow_SSSE3;
    }
  }
#endif
#if defined(HAS_ARGBTOUVMATRIXROW_AVX2)
  if (TestCpuFlag(kCpuHasAVX2)) {
    ARGBToUVMatrixRow = ARGBToUVMatrixRow_Any_AVX2;
    if (IS_ALIGNED(width, 32)) {
      ARGBToUVMatrixRow = ARGBToUVMatrixRow_AVX2;
    }
  }
#endif
#if defined(HAS_ARGBTOUVMATRIXROW_AVX512BW)
  if (TestCpuFlag(kCpuHasAVX512BW)) {
    ARGBToUVMatrixRow = ARGBToUVMatrixRow_Any_AVX512BW;
    if (IS_ALIGNED(width, 64)) {
      ARGBToUVMatrixRow = ARGBToUVMatrixRow_AVX512BW;
    }
  }
#endif
#if defined(HAS_ARGBTOUVMATRIXROW_RVV)
  if (TestCpuFlag(kCpuHasRVV)) {
    ARGBToUVMatrixRow = ARGBToUVMatrixRow_RVV;
  }
#endif
#if defined(HAS_ARGBTOUVMATRIXROW_WASMSIMD)
  if (TestCpuFlag(kCpuHasWASMSIMD)) {
    ARGBToUVMatrixRow = ARGBToUVMatrixRow_Any_WASMSIMD;
    if (IS_ALIGNED(width, 16)) {
      ARGBToUVMatrixRow = ARGBToUVMatrixRow_WASMSIMD;
    }
  }
#endif

#if defined(HAS_ARGBEXTRACTALPHAROW_SSE2)
  if (TestCpuFlag(kCpuHasSSE2)) {
    ARGBExtractAlphaRow = IS_ALIGNED(width, 8) ? ARGBExtractAlphaRow_SSE2
                                               : ARGBExtractAlphaRow_Any_SSE2;
  }
#endif
#if defined(HAS_ARGBEXTRACTALPHAROW_AVX2)
  if (TestCpuFlag(kCpuHasAVX2)) {
    ARGBExtractAlphaRow = IS_ALIGNED(width, 32) ? ARGBExtractAlphaRow_AVX2
                                                : ARGBExtractAlphaRow_Any_AVX2;
  }
#endif
#if defined(HAS_ARGBEXTRACTALPHAROW_AVX512BW)
  if (TestCpuFlag(kCpuHasAVX512BW)) {
    ARGBExtractAlphaRow = ARGBExtractAlphaRow_AVX512BW;
  }
#endif
#if defined(HAS_ARGBEXTRACTALPHAROW_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    ARGBExtractAlphaRow = IS_ALIGNED(width, 16) ? ARGBExtractAlphaRow_NEON
                                                : ARGBExtractAlphaRow_Any_NEON;
  }
#endif
#if defined(HAS_ARGBEXTRACTALPHAROW_LSX)
  if (TestCpuFlag(kCpuHasLSX)) {
    ARGBExtractAlphaRow = IS_ALIGNED(width, 16) ? ARGBExtractAlphaRow_LSX
                                                : ARGBExtractAlphaRow_Any_LSX;
  }
#endif
#if defined(HAS_ARGBEXTRACTALPHAROW_WASMSIMD)
  if (TestCpuFlag(kCpuHasWASMSIMD)) {
    ARGBExtractAlphaRow = IS_ALIGNED(width, 16)
                              ? ARGBExtractAlphaRow_WASMSIMD
                              : ARGBExtractAlphaRow_Any_WASMSIMD;
  }
#endif
#if defined(HAS_ARGBEXTRACTALPHAROW_RVV)
  if (TestCpuFlag(kCpuHasRVV)) {
    ARGBExtractAlphaRow = ARGBExtractAlphaRow_RVV;
  }
#endif

  for (y = 0; y < height - 1; y += 2) {
    ARGBToUVMatrixRow(src_argb, src_stride_argb, dst_u, dst_v, width,
                      &kArgbI601Constants);
    ARGBToYMatrixRow(src_argb, dst_y, width, &kArgbI601Constants);
    ARGBToYMatrixRow(src_argb + src_stride_argb, dst_y + dst_stride_y, width,
                     &kArgbI601Constants);
    ARGBExtractAlphaRow(src_argb, dst_a, width);
    ARGBExtractAlphaRow(src_argb + src_stride_argb, dst_a + dst_stride_a,
                        width);
    src_argb += src_stride_argb * 2;
    dst_y += dst_stride_y * 2;
    dst_u += dst_stride_u;
    dst_v += dst_stride_v;
    dst_a += dst_stride_a * 2;
  }
  if (height & 1) {
    ARGBToUVMatrixRow(src_argb, 0, dst_u, dst_v, width, &kArgbI601Constants);
    ARGBToYMatrixRow(src_argb, dst_y, width, &kArgbI601Constants);
    ARGBExtractAlphaRow(src_argb, dst_a, width);
  }
  return 0;
}
#endif  // USE_EXTRACTALPHA

// Convert BGRA to I420.
LIBYUV_API
int BGRAToI420(const uint8_t* src_bgra,
               int src_stride_bgra,
               uint8_t* dst_y,
               int dst_stride_y,
               uint8_t* dst_u,
               int dst_stride_u,
               uint8_t* dst_v,
               int dst_stride_v,
               int width,
               int height) {
  return ARGBToI420Matrix(src_bgra, src_stride_bgra, dst_y, dst_stride_y, dst_u,
                          dst_stride_u, dst_v, dst_stride_v,
                          &kBgraI601Constants, width, height);
}

// Convert BGRA to I422.
LIBYUV_API
int BGRAToI422(const uint8_t* src_bgra,
               int src_stride_bgra,
               uint8_t* dst_y,
               int dst_stride_y,
               uint8_t* dst_u,
               int dst_stride_u,
               uint8_t* dst_v,
               int dst_stride_v,
               int width,
               int height) {
  return ARGBToI422Matrix(src_bgra, src_stride_bgra, dst_y, dst_stride_y, dst_u,
                          dst_stride_u, dst_v, dst_stride_v,
                          &kBgraI601Constants, width, height);
}

// Convert ABGR to I422.
LIBYUV_API
int ABGRToI422(const uint8_t* src_abgr,
               int src_stride_abgr,
               uint8_t* dst_y,
               int dst_stride_y,
               uint8_t* dst_u,
               int dst_stride_u,
               uint8_t* dst_v,
               int dst_stride_v,
               int width,
               int height) {
  return ARGBToI422Matrix(src_abgr, src_stride_abgr, dst_y, dst_stride_y, dst_u,
                          dst_stride_u, dst_v, dst_stride_v,
                          &kAbgrI601Constants, width, height);
}

// Convert RGBA to I422.
LIBYUV_API
int RGBAToI422(const uint8_t* src_rgba,
               int src_stride_rgba,
               uint8_t* dst_y,
               int dst_stride_y,
               uint8_t* dst_u,
               int dst_stride_u,
               uint8_t* dst_v,
               int dst_stride_v,
               int width,
               int height) {
  return ARGBToI422Matrix(src_rgba, src_stride_rgba, dst_y, dst_stride_y, dst_u,
                          dst_stride_u, dst_v, dst_stride_v,
                          &kRgbaI601Constants, width, height);
}

// Convert ABGR to I420.
LIBYUV_API
int ABGRToI420(const uint8_t* src_abgr,
               int src_stride_abgr,
               uint8_t* dst_y,
               int dst_stride_y,
               uint8_t* dst_u,
               int dst_stride_u,
               uint8_t* dst_v,
               int dst_stride_v,
               int width,
               int height) {
  return ARGBToI420Matrix(src_abgr, src_stride_abgr, dst_y, dst_stride_y, dst_u,
                          dst_stride_u, dst_v, dst_stride_v,
                          &kAbgrI601Constants, width, height);
}

// Convert RGBA to I420.
LIBYUV_API
int RGBAToI420(const uint8_t* src_rgba,
               int src_stride_rgba,
               uint8_t* dst_y,
               int dst_stride_y,
               uint8_t* dst_u,
               int dst_stride_u,
               uint8_t* dst_v,
               int dst_stride_v,
               int width,
               int height) {
  return ARGBToI420Matrix(src_rgba, src_stride_rgba, dst_y, dst_stride_y, dst_u,
                          dst_stride_u, dst_v, dst_stride_v,
                          &kRgbaI601Constants, width, height);
}

// Enabled if 1 pass is available
#if (defined(HAS_RGB24TOYROW_NEON) || defined(HAS_RGB24TOYROW_LSX) || \
     defined(HAS_RGB24TOYROW_RVV))
#define HAS_RGB24TOYROW
#endif

// Convert RGB24 to I420.
LIBYUV_API
int RGB24ToI420(const uint8_t* src_rgb24,
                int src_stride_rgb24,
                uint8_t* dst_y,
                int dst_stride_y,
                uint8_t* dst_u,
                int dst_stride_u,
                uint8_t* dst_v,
                int dst_stride_v,
                int width,
                int height) {
  int y;
  void (*RGBToUVMatrixRow)(const uint8_t* src_rgb, int src_stride_rgb,
                           uint8_t* dst_u, uint8_t* dst_v, int width,
                           const struct ArgbConstants* c) = RGBToUVMatrixRow_C;
  void (*RGBToYMatrixRow)(const uint8_t* src_rgb, uint8_t* dst_y, int width,
                          const struct ArgbConstants* c) = RGBToYMatrixRow_C;

#if defined(HAS_RGBTOYMATRIXROW_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3)) {
    RGBToYMatrixRow = RGBToYMatrixRow_Any_SSSE3;
    if (IS_ALIGNED(width, 16)) {
      RGBToYMatrixRow = RGBToYMatrixRow_SSSE3;
    }
  }
#endif
#if defined(HAS_RGBTOYMATRIXROW_AVX2)
  if (TestCpuFlag(kCpuHasAVX2)) {
    RGBToYMatrixRow = RGBToYMatrixRow_Any_AVX2;
    if (IS_ALIGNED(width, 32)) {
      RGBToYMatrixRow = RGBToYMatrixRow_AVX2;
    }
  }
#endif
#if defined(HAS_RGBTOYMATRIXROW_AVX512BW)
  if (TestCpuFlag(kCpuHasAVX512BW)) {
    RGBToYMatrixRow = RGBToYMatrixRow_Any_AVX512BW;
    if (IS_ALIGNED(width, 64)) {
      RGBToYMatrixRow = RGBToYMatrixRow_AVX512BW;
    }
  }
#endif
#if defined(HAS_RGBTOUVMATRIXROW_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3)) {
    RGBToUVMatrixRow = RGBToUVMatrixRow_Any_SSSE3;
    if (IS_ALIGNED(width, 16)) {
      RGBToUVMatrixRow = RGBToUVMatrixRow_SSSE3;
    }
  }
#endif
#if defined(HAS_RGBTOUVMATRIXROW_AVX2)
  if (TestCpuFlag(kCpuHasAVX2)) {
    RGBToUVMatrixRow = RGBToUVMatrixRow_Any_AVX2;
    if (IS_ALIGNED(width, 32)) {
      RGBToUVMatrixRow = RGBToUVMatrixRow_AVX2;
    }
  }
#endif
#if defined(HAS_RGBTOUVMATRIXROW_AVX512BW)
  if (TestCpuFlag(kCpuHasAVX512BW)) {
    RGBToUVMatrixRow = RGBToUVMatrixRow_Any_AVX512BW;
    if (IS_ALIGNED(width, 64)) {
      RGBToUVMatrixRow = RGBToUVMatrixRow_AVX512BW;
    }
  }
#endif
#if defined(HAS_RGBTOUVMATRIXROW_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    RGBToUVMatrixRow = RGBToUVMatrixRow_Any_NEON;
    if (IS_ALIGNED(width, 16)) {
      RGBToUVMatrixRow = RGBToUVMatrixRow_NEON;
    }
  }
#endif
#if defined(HAS_RGBTOYMATRIXROW_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    RGBToYMatrixRow = RGBToYMatrixRow_Any_NEON;
    if (IS_ALIGNED(width, 16)) {
      RGBToYMatrixRow = RGBToYMatrixRow_NEON;
    }
  }
#endif
#if defined(HAS_RGBTOYMATRIXROW_NEON_DOTPROD)
  if (TestCpuFlag(kCpuHasNeonDotProd)) {
    RGBToYMatrixRow = RGBToYMatrixRow_Any_NEON_DotProd;
    if (IS_ALIGNED(width, 16)) {
      RGBToYMatrixRow = RGBToYMatrixRow_NEON_DotProd;
    }
  }
#endif
#if defined(HAS_RGBTOUVMATRIXROW_RVV)
  if (TestCpuFlag(kCpuHasRVV)) {
    RGBToUVMatrixRow = RGBToUVMatrixRow_RVV;
  }
#endif
#if defined(HAS_RGBTOYMATRIXROW_RVV)
  if (TestCpuFlag(kCpuHasRVV)) {
    RGBToYMatrixRow = RGBToYMatrixRow_RVV;
  }
#endif
#if defined(HAS_RGBTOUVMATRIXROW_WASMSIMD)
  if (TestCpuFlag(kCpuHasWASMSIMD)) {
    RGBToUVMatrixRow = RGBToUVMatrixRow_Any_WASMSIMD;
    if (IS_ALIGNED(width, 16)) {
      RGBToUVMatrixRow = RGBToUVMatrixRow_WASMSIMD;
    }
  }
#endif
#if defined(HAS_RGBTOYMATRIXROW_WASMSIMD)
  if (TestCpuFlag(kCpuHasWASMSIMD)) {
    RGBToYMatrixRow = RGBToYMatrixRow_Any_WASMSIMD;
    if (IS_ALIGNED(width, 16)) {
      RGBToYMatrixRow = RGBToYMatrixRow_WASMSIMD;
    }
  }
#endif

  if (!src_rgb24 || !dst_y || !dst_u || !dst_v || width <= 0 || height == 0 ||
      height == INT_MIN) {
    return -1;
  }
  // Negative height means invert the image.
  if (height < 0) {
    height = -height;
    src_rgb24 = src_rgb24 + (ptrdiff_t)(height - 1) * src_stride_rgb24;
    src_stride_rgb24 = -src_stride_rgb24;
  }

  for (y = 0; y < height - 1; y += 2) {
    RGBToUVMatrixRow(src_rgb24, src_stride_rgb24, dst_u, dst_v, width,
                     &kArgbI601Constants);
    RGBToYMatrixRow(src_rgb24, dst_y, width, &kArgbI601Constants);
    RGBToYMatrixRow(src_rgb24 + src_stride_rgb24, dst_y + dst_stride_y, width,
                    &kArgbI601Constants);
    src_rgb24 += src_stride_rgb24 * 2;
    dst_y += dst_stride_y * 2;
    dst_u += dst_stride_u;
    dst_v += dst_stride_v;
  }
  if (height & 1) {
    RGBToYMatrixRow(src_rgb24, dst_y, width, &kArgbI601Constants);
    RGBToUVMatrixRow(src_rgb24, 0, dst_u, dst_v, width, &kArgbI601Constants);
  }
  return 0;
}
#undef HAS_RGB24TOYROW

/// Convert RGB24 to J420.
LIBYUV_API
int RGB24ToJ420(const uint8_t* src_rgb24,
                int src_stride_rgb24,
                uint8_t* dst_y,
                int dst_stride_y,
                uint8_t* dst_u,
                int dst_stride_u,
                uint8_t* dst_v,
                int dst_stride_v,
                int width,
                int height) {
  int y;
  void (*RGBToUVMatrixRow)(const uint8_t* src_rgb, int src_stride_rgb,
                           uint8_t* dst_u, uint8_t* dst_v, int width,
                           const struct ArgbConstants* c) = RGBToUVMatrixRow_C;
  void (*RGBToYMatrixRow)(const uint8_t* src_rgb, uint8_t* dst_y, int width,
                          const struct ArgbConstants* c) = RGBToYMatrixRow_C;

#if defined(HAS_RGBTOYMATRIXROW_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3)) {
    RGBToYMatrixRow = RGBToYMatrixRow_Any_SSSE3;
    if (IS_ALIGNED(width, 16)) {
      RGBToYMatrixRow = RGBToYMatrixRow_SSSE3;
    }
  }
#endif
#if defined(HAS_RGBTOYMATRIXROW_AVX2)
  if (TestCpuFlag(kCpuHasAVX2)) {
    RGBToYMatrixRow = RGBToYMatrixRow_Any_AVX2;
    if (IS_ALIGNED(width, 32)) {
      RGBToYMatrixRow = RGBToYMatrixRow_AVX2;
    }
  }
#endif
#if defined(HAS_RGBTOYMATRIXROW_AVX512BW)
  if (TestCpuFlag(kCpuHasAVX512BW)) {
    RGBToYMatrixRow = RGBToYMatrixRow_Any_AVX512BW;
    if (IS_ALIGNED(width, 64)) {
      RGBToYMatrixRow = RGBToYMatrixRow_AVX512BW;
    }
  }
#endif
#if defined(HAS_RGBTOUVMATRIXROW_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3)) {
    RGBToUVMatrixRow = RGBToUVMatrixRow_Any_SSSE3;
    if (IS_ALIGNED(width, 16)) {
      RGBToUVMatrixRow = RGBToUVMatrixRow_SSSE3;
    }
  }
#endif
#if defined(HAS_RGBTOUVMATRIXROW_AVX2)
  if (TestCpuFlag(kCpuHasAVX2)) {
    RGBToUVMatrixRow = RGBToUVMatrixRow_Any_AVX2;
    if (IS_ALIGNED(width, 32)) {
      RGBToUVMatrixRow = RGBToUVMatrixRow_AVX2;
    }
  }
#endif
#if defined(HAS_RGBTOUVMATRIXROW_AVX512BW)
  if (TestCpuFlag(kCpuHasAVX512BW)) {
    RGBToUVMatrixRow = RGBToUVMatrixRow_Any_AVX512BW;
    if (IS_ALIGNED(width, 64)) {
      RGBToUVMatrixRow = RGBToUVMatrixRow_AVX512BW;
    }
  }
#endif
#if defined(HAS_RGBTOUVMATRIXROW_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    RGBToUVMatrixRow = RGBToUVMatrixRow_Any_NEON;
    if (IS_ALIGNED(width, 16)) {
      RGBToUVMatrixRow = RGBToUVMatrixRow_NEON;
    }
  }
#endif
#if defined(HAS_RGBTOYMATRIXROW_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    RGBToYMatrixRow = RGBToYMatrixRow_Any_NEON;
    if (IS_ALIGNED(width, 16)) {
      RGBToYMatrixRow = RGBToYMatrixRow_NEON;
    }
  }
#endif
#if defined(HAS_RGBTOYMATRIXROW_NEON_DOTPROD)
  if (TestCpuFlag(kCpuHasNeonDotProd)) {
    RGBToYMatrixRow = RGBToYMatrixRow_Any_NEON_DotProd;
    if (IS_ALIGNED(width, 16)) {
      RGBToYMatrixRow = RGBToYMatrixRow_NEON_DotProd;
    }
  }
#endif
#if defined(HAS_RGBTOUVMATRIXROW_RVV)
  if (TestCpuFlag(kCpuHasRVV)) {
    RGBToUVMatrixRow = RGBToUVMatrixRow_RVV;
  }
#endif
#if defined(HAS_RGBTOYMATRIXROW_RVV)
  if (TestCpuFlag(kCpuHasRVV)) {
    RGBToYMatrixRow = RGBToYMatrixRow_RVV;
  }
#endif
#if defined(HAS_RGBTOUVMATRIXROW_WASMSIMD)
  if (TestCpuFlag(kCpuHasWASMSIMD)) {
    RGBToUVMatrixRow = RGBToUVMatrixRow_Any_WASMSIMD;
    if (IS_ALIGNED(width, 16)) {
      RGBToUVMatrixRow = RGBToUVMatrixRow_WASMSIMD;
    }
  }
#endif
#if defined(HAS_RGBTOYMATRIXROW_WASMSIMD)
  if (TestCpuFlag(kCpuHasWASMSIMD)) {
    RGBToYMatrixRow = RGBToYMatrixRow_Any_WASMSIMD;
    if (IS_ALIGNED(width, 16)) {
      RGBToYMatrixRow = RGBToYMatrixRow_WASMSIMD;
    }
  }
#endif

  if (!src_rgb24 || !dst_y || !dst_u || !dst_v || width <= 0 || height == 0 ||
      height == INT_MIN) {
    return -1;
  }
  // Negative height means invert the image.
  if (height < 0) {
    height = -height;
    src_rgb24 = src_rgb24 + (ptrdiff_t)(height - 1) * src_stride_rgb24;
    src_stride_rgb24 = -src_stride_rgb24;
  }

  for (y = 0; y < height - 1; y += 2) {
    RGBToUVMatrixRow(src_rgb24, src_stride_rgb24, dst_u, dst_v, width,
                     &kArgbJPEGConstants);
    RGBToYMatrixRow(src_rgb24, dst_y, width, &kArgbJPEGConstants);
    RGBToYMatrixRow(src_rgb24 + src_stride_rgb24, dst_y + dst_stride_y, width,
                    &kArgbJPEGConstants);
    src_rgb24 += src_stride_rgb24 * 2;
    dst_y += dst_stride_y * 2;
    dst_u += dst_stride_u;
    dst_v += dst_stride_v;
  }
  if (height & 1) {
    RGBToYMatrixRow(src_rgb24, dst_y, width, &kArgbJPEGConstants);
    RGBToUVMatrixRow(src_rgb24, 0, dst_u, dst_v, width, &kArgbJPEGConstants);
  }
  return 0;
}

// Enabled if 1 pass is available
#if (defined(HAS_RAWTOYROW_NEON) || defined(HAS_RAWTOYROW_LSX) || \
     defined(HAS_RAWTOYROW_RVV))
#define HAS_RAWTOYROW
#endif

// Convert RAW to I420.
LIBYUV_API
int RAWToI420(const uint8_t* src_raw,
              int src_stride_raw,
              uint8_t* dst_y,
              int dst_stride_y,
              uint8_t* dst_u,
              int dst_stride_u,
              uint8_t* dst_v,
              int dst_stride_v,
              int width,
              int height) {
  int y;
  void (*RGBToUVMatrixRow)(const uint8_t* src_rgb, int src_stride_rgb,
                           uint8_t* dst_u, uint8_t* dst_v, int width,
                           const struct ArgbConstants* c) = RGBToUVMatrixRow_C;
  void (*RGBToYMatrixRow)(const uint8_t* src_rgb, uint8_t* dst_y, int width,
                          const struct ArgbConstants* c) = RGBToYMatrixRow_C;

#if defined(HAS_RGBTOYMATRIXROW_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3)) {
    RGBToYMatrixRow = RGBToYMatrixRow_Any_SSSE3;
    if (IS_ALIGNED(width, 16)) {
      RGBToYMatrixRow = RGBToYMatrixRow_SSSE3;
    }
  }
#endif
#if defined(HAS_RGBTOYMATRIXROW_AVX2)
  if (TestCpuFlag(kCpuHasAVX2)) {
    RGBToYMatrixRow = RGBToYMatrixRow_Any_AVX2;
    if (IS_ALIGNED(width, 32)) {
      RGBToYMatrixRow = RGBToYMatrixRow_AVX2;
    }
  }
#endif
#if defined(HAS_RGBTOYMATRIXROW_AVX512BW)
  if (TestCpuFlag(kCpuHasAVX512BW)) {
    RGBToYMatrixRow = RGBToYMatrixRow_Any_AVX512BW;
    if (IS_ALIGNED(width, 64)) {
      RGBToYMatrixRow = RGBToYMatrixRow_AVX512BW;
    }
  }
#endif
#if defined(HAS_RGBTOUVMATRIXROW_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3)) {
    RGBToUVMatrixRow = RGBToUVMatrixRow_Any_SSSE3;
    if (IS_ALIGNED(width, 16)) {
      RGBToUVMatrixRow = RGBToUVMatrixRow_SSSE3;
    }
  }
#endif
#if defined(HAS_RGBTOUVMATRIXROW_AVX2)
  if (TestCpuFlag(kCpuHasAVX2)) {
    RGBToUVMatrixRow = RGBToUVMatrixRow_Any_AVX2;
    if (IS_ALIGNED(width, 32)) {
      RGBToUVMatrixRow = RGBToUVMatrixRow_AVX2;
    }
  }
#endif
#if defined(HAS_RGBTOUVMATRIXROW_AVX512BW)
  if (TestCpuFlag(kCpuHasAVX512BW)) {
    RGBToUVMatrixRow = RGBToUVMatrixRow_Any_AVX512BW;
    if (IS_ALIGNED(width, 64)) {
      RGBToUVMatrixRow = RGBToUVMatrixRow_AVX512BW;
    }
  }
#endif
#if defined(HAS_RGBTOUVMATRIXROW_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    RGBToUVMatrixRow = RGBToUVMatrixRow_Any_NEON;
    if (IS_ALIGNED(width, 16)) {
      RGBToUVMatrixRow = RGBToUVMatrixRow_NEON;
    }
  }
#endif
#if defined(HAS_RGBTOYMATRIXROW_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    RGBToYMatrixRow = RGBToYMatrixRow_Any_NEON;
    if (IS_ALIGNED(width, 16)) {
      RGBToYMatrixRow = RGBToYMatrixRow_NEON;
    }
  }
#endif
#if defined(HAS_RGBTOYMATRIXROW_NEON_DOTPROD)
  if (TestCpuFlag(kCpuHasNeonDotProd)) {
    RGBToYMatrixRow = RGBToYMatrixRow_Any_NEON_DotProd;
    if (IS_ALIGNED(width, 16)) {
      RGBToYMatrixRow = RGBToYMatrixRow_NEON_DotProd;
    }
  }
#endif
#if defined(HAS_RGBTOUVMATRIXROW_WASMSIMD)
  if (TestCpuFlag(kCpuHasWASMSIMD)) {
    RGBToUVMatrixRow = RGBToUVMatrixRow_Any_WASMSIMD;
    if (IS_ALIGNED(width, 16)) {
      RGBToUVMatrixRow = RGBToUVMatrixRow_WASMSIMD;
    }
  }
#endif
#if defined(HAS_RGBTOYMATRIXROW_WASMSIMD)
  if (TestCpuFlag(kCpuHasWASMSIMD)) {
    RGBToYMatrixRow = RGBToYMatrixRow_Any_WASMSIMD;
    if (IS_ALIGNED(width, 16)) {
      RGBToYMatrixRow = RGBToYMatrixRow_WASMSIMD;
    }
  }
#endif

  if (!src_raw || !dst_y || !dst_u || !dst_v || width <= 0 || height == 0 ||
      height == INT_MIN) {
    return -1;
  }
  // Negative height means invert the image.
  if (height < 0) {
    height = -height;
    src_raw = src_raw + (ptrdiff_t)(height - 1) * src_stride_raw;
    src_stride_raw = -src_stride_raw;
  }

  for (y = 0; y < height - 1; y += 2) {
    RGBToUVMatrixRow(src_raw, src_stride_raw, dst_u, dst_v, width,
                     &kAbgrI601Constants);
    RGBToYMatrixRow(src_raw, dst_y, width, &kAbgrI601Constants);
    RGBToYMatrixRow(src_raw + src_stride_raw, dst_y + dst_stride_y, width,
                    &kAbgrI601Constants);
    src_raw += src_stride_raw * 2;
    dst_y += dst_stride_y * 2;
    dst_u += dst_stride_u;
    dst_v += dst_stride_v;
  }
  if (height & 1) {
    RGBToYMatrixRow(src_raw, dst_y, width, &kAbgrI601Constants);
    RGBToUVMatrixRow(src_raw, 0, dst_u, dst_v, width, &kAbgrI601Constants);
  }
  return 0;
}
#undef HAS_RAWTOYROW

// Convert RAW to J420.
LIBYUV_API
int RAWToJ420(const uint8_t* src_raw,
              int src_stride_raw,
              uint8_t* dst_y,
              int dst_stride_y,
              uint8_t* dst_u,
              int dst_stride_u,
              uint8_t* dst_v,
              int dst_stride_v,
              int width,
              int height) {
  int y;
  void (*RGBToUVMatrixRow)(const uint8_t* src_rgb, int src_stride_rgb,
                           uint8_t* dst_u, uint8_t* dst_v, int width,
                           const struct ArgbConstants* c) = RGBToUVMatrixRow_C;
  void (*RGBToYMatrixRow)(const uint8_t* src_rgb, uint8_t* dst_y, int width,
                          const struct ArgbConstants* c) = RGBToYMatrixRow_C;

#if defined(HAS_RGBTOYMATRIXROW_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3)) {
    RGBToYMatrixRow = RGBToYMatrixRow_Any_SSSE3;
    if (IS_ALIGNED(width, 16)) {
      RGBToYMatrixRow = RGBToYMatrixRow_SSSE3;
    }
  }
#endif
#if defined(HAS_RGBTOYMATRIXROW_AVX2)
  if (TestCpuFlag(kCpuHasAVX2)) {
    RGBToYMatrixRow = RGBToYMatrixRow_Any_AVX2;
    if (IS_ALIGNED(width, 32)) {
      RGBToYMatrixRow = RGBToYMatrixRow_AVX2;
    }
  }
#endif
#if defined(HAS_RGBTOYMATRIXROW_AVX512BW)
  if (TestCpuFlag(kCpuHasAVX512BW)) {
    RGBToYMatrixRow = RGBToYMatrixRow_Any_AVX512BW;
    if (IS_ALIGNED(width, 64)) {
      RGBToYMatrixRow = RGBToYMatrixRow_AVX512BW;
    }
  }
#endif
#if defined(HAS_RGBTOUVMATRIXROW_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3)) {
    RGBToUVMatrixRow = RGBToUVMatrixRow_Any_SSSE3;
    if (IS_ALIGNED(width, 16)) {
      RGBToUVMatrixRow = RGBToUVMatrixRow_SSSE3;
    }
  }
#endif
#if defined(HAS_RGBTOUVMATRIXROW_AVX2)
  if (TestCpuFlag(kCpuHasAVX2)) {
    RGBToUVMatrixRow = RGBToUVMatrixRow_Any_AVX2;
    if (IS_ALIGNED(width, 32)) {
      RGBToUVMatrixRow = RGBToUVMatrixRow_AVX2;
    }
  }
#endif
#if defined(HAS_RGBTOUVMATRIXROW_AVX512BW)
  if (TestCpuFlag(kCpuHasAVX512BW)) {
    RGBToUVMatrixRow = RGBToUVMatrixRow_Any_AVX512BW;
    if (IS_ALIGNED(width, 64)) {
      RGBToUVMatrixRow = RGBToUVMatrixRow_AVX512BW;
    }
  }
#endif
#if defined(HAS_RGBTOUVMATRIXROW_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    RGBToUVMatrixRow = RGBToUVMatrixRow_Any_NEON;
    if (IS_ALIGNED(width, 16)) {
      RGBToUVMatrixRow = RGBToUVMatrixRow_NEON;
    }
  }
#endif
#if defined(HAS_RGBTOYMATRIXROW_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    RGBToYMatrixRow = RGBToYMatrixRow_Any_NEON;
    if (IS_ALIGNED(width, 16)) {
      RGBToYMatrixRow = RGBToYMatrixRow_NEON;
    }
  }
#endif
#if defined(HAS_RGBTOYMATRIXROW_NEON_DOTPROD)
  if (TestCpuFlag(kCpuHasNeonDotProd)) {
    RGBToYMatrixRow = RGBToYMatrixRow_Any_NEON_DotProd;
    if (IS_ALIGNED(width, 16)) {
      RGBToYMatrixRow = RGBToYMatrixRow_NEON_DotProd;
    }
  }
#endif
#if defined(HAS_RGBTOUVMATRIXROW_WASMSIMD)
  if (TestCpuFlag(kCpuHasWASMSIMD)) {
    RGBToUVMatrixRow = RGBToUVMatrixRow_Any_WASMSIMD;
    if (IS_ALIGNED(width, 16)) {
      RGBToUVMatrixRow = RGBToUVMatrixRow_WASMSIMD;
    }
  }
#endif
#if defined(HAS_RGBTOYMATRIXROW_WASMSIMD)
  if (TestCpuFlag(kCpuHasWASMSIMD)) {
    RGBToYMatrixRow = RGBToYMatrixRow_Any_WASMSIMD;
    if (IS_ALIGNED(width, 16)) {
      RGBToYMatrixRow = RGBToYMatrixRow_WASMSIMD;
    }
  }
#endif

  if (!src_raw || !dst_y || !dst_u || !dst_v || width <= 0 || height == 0 ||
      height == INT_MIN) {
    return -1;
  }
  // Negative height means invert the image.
  if (height < 0) {
    height = -height;
    src_raw = src_raw + (ptrdiff_t)(height - 1) * src_stride_raw;
    src_stride_raw = -src_stride_raw;
  }

  for (y = 0; y < height - 1; y += 2) {
    RGBToUVMatrixRow(src_raw, src_stride_raw, dst_u, dst_v, width,
                     &kAbgrJPEGConstants);
    RGBToYMatrixRow(src_raw, dst_y, width, &kAbgrJPEGConstants);
    RGBToYMatrixRow(src_raw + src_stride_raw, dst_y + dst_stride_y, width,
                    &kAbgrJPEGConstants);
    src_raw += src_stride_raw * 2;
    dst_y += dst_stride_y * 2;
    dst_u += dst_stride_u;
    dst_v += dst_stride_v;
  }
  if (height & 1) {
    RGBToYMatrixRow(src_raw, dst_y, width, &kAbgrJPEGConstants);
    RGBToUVMatrixRow(src_raw, 0, dst_u, dst_v, width, &kAbgrJPEGConstants);
  }
  return 0;
}

// Convert RAW to I444 with matrix.
static int RAWToI444Matrix(const uint8_t* src_raw,
                           int src_stride_raw,
                           uint8_t* dst_y,
                           int dst_stride_y,
                           uint8_t* dst_u,
                           int dst_stride_u,
                           uint8_t* dst_v,
                           int dst_stride_v,
                           const struct ArgbConstants* argbconstants,
                           int width,
                           int height) {
  int y;
  void (*RGBToUV444MatrixRow)(
      const uint8_t* src_rgb, uint8_t* dst_u, uint8_t* dst_v, int width,
      const struct ArgbConstants* c) = RGBToUV444MatrixRow_C;
  void (*RGBToYMatrixRow)(const uint8_t* src_rgb, uint8_t* dst_y, int width,
                          const struct ArgbConstants* c) = RGBToYMatrixRow_C;

#if defined(HAS_RGBTOYMATRIXROW_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3)) {
    RGBToYMatrixRow = RGBToYMatrixRow_Any_SSSE3;
    if (IS_ALIGNED(width, 16)) {
      RGBToYMatrixRow = RGBToYMatrixRow_SSSE3;
    }
  }
#endif
#if defined(HAS_RGBTOYMATRIXROW_AVX2)
  if (TestCpuFlag(kCpuHasAVX2)) {
    RGBToYMatrixRow = RGBToYMatrixRow_Any_AVX2;
    if (IS_ALIGNED(width, 32)) {
      RGBToYMatrixRow = RGBToYMatrixRow_AVX2;
    }
  }
#endif
#if defined(HAS_RGBTOYMATRIXROW_AVX512BW)
  if (TestCpuFlag(kCpuHasAVX512BW)) {
    RGBToYMatrixRow = RGBToYMatrixRow_Any_AVX512BW;
    if (IS_ALIGNED(width, 64)) {
      RGBToYMatrixRow = RGBToYMatrixRow_AVX512BW;
    }
  }
#endif
#if defined(HAS_RGBTOYMATRIXROW_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    RGBToYMatrixRow = RGBToYMatrixRow_Any_NEON;
    if (IS_ALIGNED(width, 16)) {
      RGBToYMatrixRow = RGBToYMatrixRow_NEON;
    }
  }
#endif
#if defined(HAS_RGBTOYMATRIXROW_NEON_DOTPROD)
  if (TestCpuFlag(kCpuHasNeonDotProd)) {
    RGBToYMatrixRow = RGBToYMatrixRow_Any_NEON_DotProd;
    if (IS_ALIGNED(width, 16)) {
      RGBToYMatrixRow = RGBToYMatrixRow_NEON_DotProd;
    }
  }
#endif
#if defined(HAS_RGBTOYMATRIXROW_LSX)
  if (TestCpuFlag(kCpuHasLSX)) {
    RGBToYMatrixRow = RGBToYMatrixRow_Any_LSX;
    if (IS_ALIGNED(width, 16)) {
      RGBToYMatrixRow = RGBToYMatrixRow_LSX;
    }
  }
#endif
#if defined(HAS_RGBTOYMATRIXROW_LASX)
  if (TestCpuFlag(kCpuHasLASX)) {
    RGBToYMatrixRow = RGBToYMatrixRow_Any_LASX;
    if (IS_ALIGNED(width, 32)) {
      RGBToYMatrixRow = RGBToYMatrixRow_LASX;
    }
  }
#endif
#if defined(HAS_RGBTOYMATRIXROW_RVV)
  if (TestCpuFlag(kCpuHasRVV)) {
    RGBToYMatrixRow = RGBToYMatrixRow_RVV;
  }
#endif
#if defined(HAS_RGBTOYMATRIXROW_WASMSIMD)
  if (TestCpuFlag(kCpuHasWASMSIMD)) {
    RGBToYMatrixRow = RGBToYMatrixRow_Any_WASMSIMD;
    if (IS_ALIGNED(width, 16)) {
      RGBToYMatrixRow = RGBToYMatrixRow_WASMSIMD;
    }
  }
#endif

#if defined(HAS_RGBTOUV444MATRIXROW_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3)) {
    RGBToUV444MatrixRow = RGBToUV444MatrixRow_Any_SSSE3;
    if (IS_ALIGNED(width, 16)) {
      RGBToUV444MatrixRow = RGBToUV444MatrixRow_SSSE3;
    }
  }
#endif
#if defined(HAS_RGBTOUV444MATRIXROW_AVX2)
  if (TestCpuFlag(kCpuHasAVX2)) {
    RGBToUV444MatrixRow = RGBToUV444MatrixRow_Any_AVX2;
    if (IS_ALIGNED(width, 32)) {
      RGBToUV444MatrixRow = RGBToUV444MatrixRow_AVX2;
    }
  }
#endif
#if defined(HAS_RGBTOUV444MATRIXROW_AVX512BW)
  if (TestCpuFlag(kCpuHasAVX512BW)) {
    RGBToUV444MatrixRow = RGBToUV444MatrixRow_Any_AVX512BW;
    if (IS_ALIGNED(width, 64)) {
      RGBToUV444MatrixRow = RGBToUV444MatrixRow_AVX512BW;
    }
  }
#endif
#if defined(HAS_RGBTOUV444MATRIXROW_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    RGBToUV444MatrixRow = RGBToUV444MatrixRow_Any_NEON;
    if (IS_ALIGNED(width, 8)) {
      RGBToUV444MatrixRow = RGBToUV444MatrixRow_NEON;
    }
  }
#endif
#if defined(HAS_RGBTOUV444MATRIXROW_RVV)
  if (TestCpuFlag(kCpuHasRVV)) {
    RGBToUV444MatrixRow = RGBToUV444MatrixRow_RVV;
  }
#endif
#if defined(HAS_RGBTOUV444MATRIXROW_WASMSIMD)
  if (TestCpuFlag(kCpuHasWASMSIMD)) {
    RGBToUV444MatrixRow = RGBToUV444MatrixRow_Any_WASMSIMD;
    if (IS_ALIGNED(width, 16)) {
      RGBToUV444MatrixRow = RGBToUV444MatrixRow_WASMSIMD;
    }
  }
#endif

  if (!src_raw || !dst_y || !dst_u || !dst_v || !argbconstants || width <= 0 ||
      height == 0 || height == INT_MIN) {
    return -1;
  }
  if (height < 0) {
    height = -height;
    src_raw = src_raw + (ptrdiff_t)(height - 1) * src_stride_raw;
    src_stride_raw = -src_stride_raw;
  }

  for (y = 0; y < height; ++y) {
    RGBToYMatrixRow(src_raw, dst_y, width, argbconstants);
    RGBToUV444MatrixRow(src_raw, dst_u, dst_v, width, argbconstants);
    src_raw += src_stride_raw;
    dst_y += dst_stride_y;
    dst_u += dst_stride_u;
    dst_v += dst_stride_v;
  }
  return 0;
}

// Convert RAW to I444.
LIBYUV_API
int RAWToI444(const uint8_t* src_raw,
              int src_stride_raw,
              uint8_t* dst_y,
              int dst_stride_y,
              uint8_t* dst_u,
              int dst_stride_u,
              uint8_t* dst_v,
              int dst_stride_v,
              int width,
              int height) {
  return RAWToI444Matrix(src_raw, src_stride_raw, dst_y, dst_stride_y, dst_u,
                         dst_stride_u, dst_v, dst_stride_v, &kAbgrI601Constants,
                         width, height);
}

// Convert RAW to J444.
LIBYUV_API
int RAWToJ444(const uint8_t* src_raw,
              int src_stride_raw,
              uint8_t* dst_y,
              int dst_stride_y,
              uint8_t* dst_u,
              int dst_stride_u,
              uint8_t* dst_v,
              int dst_stride_v,
              int width,
              int height) {
  return RAWToI444Matrix(src_raw, src_stride_raw, dst_y, dst_stride_y, dst_u,
                         dst_stride_u, dst_v, dst_stride_v, &kAbgrJPEGConstants,
                         width, height);
}

// Convert RGB565 to I420.
LIBYUV_API
int RGB565ToI420(const uint8_t* src_rgb565,
                 int src_stride_rgb565,
                 uint8_t* dst_y,
                 int dst_stride_y,
                 uint8_t* dst_u,
                 int dst_stride_u,
                 uint8_t* dst_v,
                 int dst_stride_v,
                 int width,
                 int height) {
  int y;
  void (*RGB565ToUVMatrixRow)(const uint8_t* src_rgb565, int src_stride_rgb565,
                              uint8_t* dst_u, uint8_t* dst_v, int width,
                              const struct ArgbConstants* c) =
      RGB565ToUVMatrixRow_C;
  void (*RGB565ToYMatrixRow)(const uint8_t* src_rgb565, uint8_t* dst_y,
                             int width, const struct ArgbConstants* c) =
      RGB565ToYMatrixRow_C;

#if defined(HAS_RGB565TOYMATRIXROW_AVX2)
  if (TestCpuFlag(kCpuHasAVX2)) {
    RGB565ToYMatrixRow = RGB565ToYMatrixRow_Any_AVX2;
    if (IS_ALIGNED(width, 32)) {
      RGB565ToYMatrixRow = RGB565ToYMatrixRow_AVX2;
    }
  }
#endif
#if defined(HAS_RGB565TOUVMATRIXROW_AVX2)
  if (TestCpuFlag(kCpuHasAVX2)) {
    RGB565ToUVMatrixRow = RGB565ToUVMatrixRow_Any_AVX2;
    if (IS_ALIGNED(width, 16)) {
      RGB565ToUVMatrixRow = RGB565ToUVMatrixRow_AVX2;
    }
  }
#endif
#if defined(HAS_RGB565TOUVMATRIXROW_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    RGB565ToUVMatrixRow = RGB565ToUVMatrixRow_Any_NEON;
    if (IS_ALIGNED(width, 16)) {
      RGB565ToUVMatrixRow = RGB565ToUVMatrixRow_NEON;
    }
  }
#endif
#if defined(HAS_RGB565TOYMATRIXROW_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    RGB565ToYMatrixRow = RGB565ToYMatrixRow_Any_NEON;
    if (IS_ALIGNED(width, 16)) {
      RGB565ToYMatrixRow = RGB565ToYMatrixRow_NEON;
    }
  }
#endif
#if defined(HAS_RGB565TOUVMATRIXROW_WASMSIMD)
  if (TestCpuFlag(kCpuHasWASMSIMD)) {
    RGB565ToUVMatrixRow = RGB565ToUVMatrixRow_Any_WASMSIMD;
    if (IS_ALIGNED(width, 16)) {
      RGB565ToUVMatrixRow = RGB565ToUVMatrixRow_WASMSIMD;
    }
  }
#endif
#if defined(HAS_RGB565TOYMATRIXROW_WASMSIMD)
  if (TestCpuFlag(kCpuHasWASMSIMD)) {
    RGB565ToYMatrixRow = RGB565ToYMatrixRow_Any_WASMSIMD;
    if (IS_ALIGNED(width, 16)) {
      RGB565ToYMatrixRow = RGB565ToYMatrixRow_WASMSIMD;
    }
  }
#endif

  if (!src_rgb565 || !dst_y || !dst_u || !dst_v || width <= 0 || height == 0 ||
      height == INT_MIN) {
    return -1;
  }
  // Negative height means invert the image.
  if (height < 0) {
    height = -height;
    src_rgb565 = src_rgb565 + (ptrdiff_t)(height - 1) * src_stride_rgb565;
    src_stride_rgb565 = -src_stride_rgb565;
  }

  for (y = 0; y < height - 1; y += 2) {
    RGB565ToUVMatrixRow(src_rgb565, src_stride_rgb565, dst_u, dst_v, width,
                        &kArgbI601Constants);
    RGB565ToYMatrixRow(src_rgb565, dst_y, width, &kArgbI601Constants);
    RGB565ToYMatrixRow(src_rgb565 + src_stride_rgb565, dst_y + dst_stride_y,
                       width, &kArgbI601Constants);
    src_rgb565 += src_stride_rgb565 * 2;
    dst_y += dst_stride_y * 2;
    dst_u += dst_stride_u;
    dst_v += dst_stride_v;
  }
  if (height & 1) {
    RGB565ToYMatrixRow(src_rgb565, dst_y, width, &kArgbI601Constants);
    RGB565ToUVMatrixRow(src_rgb565, 0, dst_u, dst_v, width,
                        &kArgbI601Constants);
  }
  return 0;
}
// Convert ARGB1555 to I420.
LIBYUV_API
int ARGB1555ToI420(const uint8_t* src_argb1555,
                   int src_stride_argb1555,
                   uint8_t* dst_y,
                   int dst_stride_y,
                   uint8_t* dst_u,
                   int dst_stride_u,
                   uint8_t* dst_v,
                   int dst_stride_v,
                   int width,
                   int height) {
  int y;
  void (*ARGB1555ToUVMatrixRow)(
      const uint8_t* src_argb1555, int src_stride_argb1555, uint8_t* dst_u,
      uint8_t* dst_v, int width, const struct ArgbConstants* c) =
      ARGB1555ToUVMatrixRow_C;
  void (*ARGB1555ToYMatrixRow)(const uint8_t* src_argb1555, uint8_t* dst_y,
                               int width, const struct ArgbConstants* c) =
      ARGB1555ToYMatrixRow_C;

#if defined(HAS_ARGB1555TOYMATRIXROW_AVX2)
  if (TestCpuFlag(kCpuHasAVX2)) {
    ARGB1555ToYMatrixRow = ARGB1555ToYMatrixRow_Any_AVX2;
    if (IS_ALIGNED(width, 32)) {
      ARGB1555ToYMatrixRow = ARGB1555ToYMatrixRow_AVX2;
    }
  }
#endif
#if defined(HAS_ARGB1555TOUVMATRIXROW_AVX2)
  if (TestCpuFlag(kCpuHasAVX2)) {
    ARGB1555ToUVMatrixRow = ARGB1555ToUVMatrixRow_Any_AVX2;
    if (IS_ALIGNED(width, 16)) {
      ARGB1555ToUVMatrixRow = ARGB1555ToUVMatrixRow_AVX2;
    }
  }
#endif
#if defined(HAS_ARGB1555TOUVMATRIXROW_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    ARGB1555ToUVMatrixRow = ARGB1555ToUVMatrixRow_Any_NEON;
    if (IS_ALIGNED(width, 16)) {
      ARGB1555ToUVMatrixRow = ARGB1555ToUVMatrixRow_NEON;
    }
  }
#endif
#if defined(HAS_ARGB1555TOYMATRIXROW_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    ARGB1555ToYMatrixRow = ARGB1555ToYMatrixRow_Any_NEON;
    if (IS_ALIGNED(width, 16)) {
      ARGB1555ToYMatrixRow = ARGB1555ToYMatrixRow_NEON;
    }
  }
#endif
#if defined(HAS_ARGB1555TOUVMATRIXROW_WASMSIMD)
  if (TestCpuFlag(kCpuHasWASMSIMD)) {
    ARGB1555ToUVMatrixRow = ARGB1555ToUVMatrixRow_Any_WASMSIMD;
    if (IS_ALIGNED(width, 16)) {
      ARGB1555ToUVMatrixRow = ARGB1555ToUVMatrixRow_WASMSIMD;
    }
  }
#endif
#if defined(HAS_ARGB1555TOYMATRIXROW_WASMSIMD)
  if (TestCpuFlag(kCpuHasWASMSIMD)) {
    ARGB1555ToYMatrixRow = ARGB1555ToYMatrixRow_Any_WASMSIMD;
    if (IS_ALIGNED(width, 16)) {
      ARGB1555ToYMatrixRow = ARGB1555ToYMatrixRow_WASMSIMD;
    }
  }
#endif

  if (!src_argb1555 || !dst_y || !dst_u || !dst_v || width <= 0 ||
      height == 0 || height == INT_MIN) {
    return -1;
  }
  // Negative height means invert the image.
  if (height < 0) {
    height = -height;
    src_argb1555 = src_argb1555 + (ptrdiff_t)(height - 1) * src_stride_argb1555;
    src_stride_argb1555 = -src_stride_argb1555;
  }

  for (y = 0; y < height - 1; y += 2) {
    ARGB1555ToUVMatrixRow(src_argb1555, src_stride_argb1555, dst_u, dst_v,
                          width, &kArgbI601Constants);
    ARGB1555ToYMatrixRow(src_argb1555, dst_y, width, &kArgbI601Constants);
    ARGB1555ToYMatrixRow(src_argb1555 + src_stride_argb1555,
                         dst_y + dst_stride_y, width, &kArgbI601Constants);
    src_argb1555 += src_stride_argb1555 * 2;
    dst_y += dst_stride_y * 2;
    dst_u += dst_stride_u;
    dst_v += dst_stride_v;
  }
  if (height & 1) {
    ARGB1555ToYMatrixRow(src_argb1555, dst_y, width, &kArgbI601Constants);
    ARGB1555ToUVMatrixRow(src_argb1555, 0, dst_u, dst_v, width,
                          &kArgbI601Constants);
  }
  return 0;
}
// Convert ARGB4444 to I420.
LIBYUV_API
int ARGB4444ToI420(const uint8_t* src_argb4444,
                   int src_stride_argb4444,
                   uint8_t* dst_y,
                   int dst_stride_y,
                   uint8_t* dst_u,
                   int dst_stride_u,
                   uint8_t* dst_v,
                   int dst_stride_v,
                   int width,
                   int height) {
  int y;
  void (*ARGB4444ToUVMatrixRow)(
      const uint8_t* src_argb4444, int src_stride_argb4444, uint8_t* dst_u,
      uint8_t* dst_v, int width, const struct ArgbConstants* c) =
      ARGB4444ToUVMatrixRow_C;
  void (*ARGB4444ToYMatrixRow)(const uint8_t* src_argb4444, uint8_t* dst_y,
                               int width, const struct ArgbConstants* c) =
      ARGB4444ToYMatrixRow_C;

#if defined(HAS_ARGB4444TOYMATRIXROW_AVX2)
  if (TestCpuFlag(kCpuHasAVX2)) {
    ARGB4444ToYMatrixRow = ARGB4444ToYMatrixRow_Any_AVX2;
    if (IS_ALIGNED(width, 32)) {
      ARGB4444ToYMatrixRow = ARGB4444ToYMatrixRow_AVX2;
    }
  }
#endif
#if defined(HAS_ARGB4444TOUVMATRIXROW_AVX2)
  if (TestCpuFlag(kCpuHasAVX2)) {
    ARGB4444ToUVMatrixRow = ARGB4444ToUVMatrixRow_Any_AVX2;
    if (IS_ALIGNED(width, 16)) {
      ARGB4444ToUVMatrixRow = ARGB4444ToUVMatrixRow_AVX2;
    }
  }
#endif
#if defined(HAS_ARGB4444TOUVMATRIXROW_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    ARGB4444ToUVMatrixRow = ARGB4444ToUVMatrixRow_Any_NEON;
    if (IS_ALIGNED(width, 16)) {
      ARGB4444ToUVMatrixRow = ARGB4444ToUVMatrixRow_NEON;
    }
  }
#endif
#if defined(HAS_ARGB4444TOYMATRIXROW_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    ARGB4444ToYMatrixRow = ARGB4444ToYMatrixRow_Any_NEON;
    if (IS_ALIGNED(width, 16)) {
      ARGB4444ToYMatrixRow = ARGB4444ToYMatrixRow_NEON;
    }
  }
#endif
#if defined(HAS_ARGB4444TOUVMATRIXROW_WASMSIMD)
  if (TestCpuFlag(kCpuHasWASMSIMD)) {
    ARGB4444ToUVMatrixRow = ARGB4444ToUVMatrixRow_Any_WASMSIMD;
    if (IS_ALIGNED(width, 16)) {
      ARGB4444ToUVMatrixRow = ARGB4444ToUVMatrixRow_WASMSIMD;
    }
  }
#endif
#if defined(HAS_ARGB4444TOYMATRIXROW_WASMSIMD)
  if (TestCpuFlag(kCpuHasWASMSIMD)) {
    ARGB4444ToYMatrixRow = ARGB4444ToYMatrixRow_Any_WASMSIMD;
    if (IS_ALIGNED(width, 16)) {
      ARGB4444ToYMatrixRow = ARGB4444ToYMatrixRow_WASMSIMD;
    }
  }
#endif

  if (!src_argb4444 || !dst_y || !dst_u || !dst_v || width <= 0 ||
      height == 0 || height == INT_MIN) {
    return -1;
  }
  // Negative height means invert the image.
  if (height < 0) {
    height = -height;
    src_argb4444 = src_argb4444 + (ptrdiff_t)(height - 1) * src_stride_argb4444;
    src_stride_argb4444 = -src_stride_argb4444;
  }

  for (y = 0; y < height - 1; y += 2) {
    ARGB4444ToUVMatrixRow(src_argb4444, src_stride_argb4444, dst_u, dst_v,
                          width, &kArgbI601Constants);
    ARGB4444ToYMatrixRow(src_argb4444, dst_y, width, &kArgbI601Constants);
    ARGB4444ToYMatrixRow(src_argb4444 + src_stride_argb4444,
                         dst_y + dst_stride_y, width, &kArgbI601Constants);
    src_argb4444 += src_stride_argb4444 * 2;
    dst_y += dst_stride_y * 2;
    dst_u += dst_stride_u;
    dst_v += dst_stride_v;
  }
  if (height & 1) {
    ARGB4444ToYMatrixRow(src_argb4444, dst_y, width, &kArgbI601Constants);
    ARGB4444ToUVMatrixRow(src_argb4444, 0, dst_u, dst_v, width,
                          &kArgbI601Constants);
  }
  return 0;
}
// Convert RGB24 to J400.
LIBYUV_API
int RGB24ToJ400(const uint8_t* src_rgb24,
                int src_stride_rgb24,
                uint8_t* dst_yj,
                int dst_stride_yj,
                int width,
                int height) {
  int y;
  void (*RGBToYMatrixRow)(const uint8_t* src_rgb, uint8_t* dst_y, int width,
                          const struct ArgbConstants* c) = RGBToYMatrixRow_C;
  if (!src_rgb24 || !dst_yj || width <= 0 || height == 0 || height == INT_MIN) {
    return -1;
  }
  if (height < 0) {
    height = -height;
    src_rgb24 = src_rgb24 + (ptrdiff_t)(height - 1) * src_stride_rgb24;
    src_stride_rgb24 = -src_stride_rgb24;
  }
  // Coalesce rows.
  if (src_stride_rgb24 == width * 3 && dst_stride_yj == width &&
      (ptrdiff_t)width * height <= INT_MAX) {
    width *= height;
    height = 1;
    src_stride_rgb24 = dst_stride_yj = 0;
  }

#if defined(HAS_RGBTOYMATRIXROW_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3)) {
    RGBToYMatrixRow = RGBToYMatrixRow_Any_SSSE3;
    if (IS_ALIGNED(width, 16)) {
      RGBToYMatrixRow = RGBToYMatrixRow_SSSE3;
    }
  }
#endif
#if defined(HAS_RGBTOYMATRIXROW_AVX2)
  if (TestCpuFlag(kCpuHasAVX2)) {
    RGBToYMatrixRow = RGBToYMatrixRow_Any_AVX2;
    if (IS_ALIGNED(width, 32)) {
      RGBToYMatrixRow = RGBToYMatrixRow_AVX2;
    }
  }
#endif
#if defined(HAS_RGBTOYMATRIXROW_AVX512BW)
  if (TestCpuFlag(kCpuHasAVX512BW)) {
    RGBToYMatrixRow = RGBToYMatrixRow_Any_AVX512BW;
    if (IS_ALIGNED(width, 64)) {
      RGBToYMatrixRow = RGBToYMatrixRow_AVX512BW;
    }
  }
#endif
#if defined(HAS_RGBTOYMATRIXROW_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    RGBToYMatrixRow = RGBToYMatrixRow_Any_NEON;
    if (IS_ALIGNED(width, 16)) {
      RGBToYMatrixRow = RGBToYMatrixRow_NEON;
    }
  }
#endif
#if defined(HAS_RGBTOYMATRIXROW_NEON_DOTPROD)
  if (TestCpuFlag(kCpuHasNeonDotProd)) {
    RGBToYMatrixRow = RGBToYMatrixRow_Any_NEON_DotProd;
    if (IS_ALIGNED(width, 16)) {
      RGBToYMatrixRow = RGBToYMatrixRow_NEON_DotProd;
    }
  }
#endif
#if defined(HAS_RGBTOYMATRIXROW_LSX)
  if (TestCpuFlag(kCpuHasLSX)) {
    RGBToYMatrixRow = RGBToYMatrixRow_Any_LSX;
    if (IS_ALIGNED(width, 16)) {
      RGBToYMatrixRow = RGBToYMatrixRow_LSX;
    }
  }
#endif
#if defined(HAS_RGBTOYMATRIXROW_LASX)
  if (TestCpuFlag(kCpuHasLASX)) {
    RGBToYMatrixRow = RGBToYMatrixRow_Any_LASX;
    if (IS_ALIGNED(width, 32)) {
      RGBToYMatrixRow = RGBToYMatrixRow_LASX;
    }
  }
#endif
#if defined(HAS_RGBTOYMATRIXROW_RVV)
  if (TestCpuFlag(kCpuHasRVV)) {
    RGBToYMatrixRow = RGBToYMatrixRow_RVV;
  }
#endif
#if defined(HAS_RGBTOYMATRIXROW_WASMSIMD)
  if (TestCpuFlag(kCpuHasWASMSIMD)) {
    RGBToYMatrixRow = RGBToYMatrixRow_Any_WASMSIMD;
    if (IS_ALIGNED(width, 16)) {
      RGBToYMatrixRow = RGBToYMatrixRow_WASMSIMD;
    }
  }
#endif

  for (y = 0; y < height; ++y) {
    RGBToYMatrixRow(src_rgb24, dst_yj, width, &kArgbJPEGConstants);
    src_rgb24 += src_stride_rgb24;
    dst_yj += dst_stride_yj;
  }
  return 0;
}

// Convert RAW to J400.
LIBYUV_API
int RAWToJ400(const uint8_t* src_raw,
              int src_stride_raw,
              uint8_t* dst_yj,
              int dst_stride_yj,
              int width,
              int height) {
  int y;
  void (*RGBToYMatrixRow)(const uint8_t* src_rgb, uint8_t* dst_y, int width,
                          const struct ArgbConstants* c) = RGBToYMatrixRow_C;
  if (!src_raw || !dst_yj || width <= 0 || height == 0 || height == INT_MIN) {
    return -1;
  }
  if (height < 0) {
    height = -height;
    src_raw = src_raw + (ptrdiff_t)(height - 1) * src_stride_raw;
    src_stride_raw = -src_stride_raw;
  }
  // Coalesce rows.
  if (src_stride_raw == width * 3 && dst_stride_yj == width &&
      (ptrdiff_t)width * height <= INT_MAX) {
    width *= height;
    height = 1;
    src_stride_raw = dst_stride_yj = 0;
  }

#if defined(HAS_RGBTOYMATRIXROW_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3)) {
    RGBToYMatrixRow = RGBToYMatrixRow_Any_SSSE3;
    if (IS_ALIGNED(width, 16)) {
      RGBToYMatrixRow = RGBToYMatrixRow_SSSE3;
    }
  }
#endif
#if defined(HAS_RGBTOYMATRIXROW_AVX2)
  if (TestCpuFlag(kCpuHasAVX2)) {
    RGBToYMatrixRow = RGBToYMatrixRow_Any_AVX2;
    if (IS_ALIGNED(width, 32)) {
      RGBToYMatrixRow = RGBToYMatrixRow_AVX2;
    }
  }
#endif
#if defined(HAS_RGBTOYMATRIXROW_AVX512BW)
  if (TestCpuFlag(kCpuHasAVX512BW)) {
    RGBToYMatrixRow = RGBToYMatrixRow_Any_AVX512BW;
    if (IS_ALIGNED(width, 64)) {
      RGBToYMatrixRow = RGBToYMatrixRow_AVX512BW;
    }
  }
#endif
#if defined(HAS_RGBTOYMATRIXROW_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    RGBToYMatrixRow = RGBToYMatrixRow_Any_NEON;
    if (IS_ALIGNED(width, 16)) {
      RGBToYMatrixRow = RGBToYMatrixRow_NEON;
    }
  }
#endif
#if defined(HAS_RGBTOYMATRIXROW_NEON_DOTPROD)
  if (TestCpuFlag(kCpuHasNeonDotProd)) {
    RGBToYMatrixRow = RGBToYMatrixRow_Any_NEON_DotProd;
    if (IS_ALIGNED(width, 16)) {
      RGBToYMatrixRow = RGBToYMatrixRow_NEON_DotProd;
    }
  }
#endif
#if defined(HAS_RGBTOYMATRIXROW_LSX)
  if (TestCpuFlag(kCpuHasLSX)) {
    RGBToYMatrixRow = RGBToYMatrixRow_Any_LSX;
    if (IS_ALIGNED(width, 16)) {
      RGBToYMatrixRow = RGBToYMatrixRow_LSX;
    }
  }
#endif
#if defined(HAS_RGBTOYMATRIXROW_LASX)
  if (TestCpuFlag(kCpuHasLASX)) {
    RGBToYMatrixRow = RGBToYMatrixRow_Any_LASX;
    if (IS_ALIGNED(width, 32)) {
      RGBToYMatrixRow = RGBToYMatrixRow_LASX;
    }
  }
#endif
#if defined(HAS_RGBTOYMATRIXROW_RVV)
  if (TestCpuFlag(kCpuHasRVV)) {
    RGBToYMatrixRow = RGBToYMatrixRow_RVV;
  }
#endif
#if defined(HAS_RGBTOYMATRIXROW_WASMSIMD)
  if (TestCpuFlag(kCpuHasWASMSIMD)) {
    RGBToYMatrixRow = RGBToYMatrixRow_Any_WASMSIMD;
    if (IS_ALIGNED(width, 16)) {
      RGBToYMatrixRow = RGBToYMatrixRow_WASMSIMD;
    }
  }
#endif

  for (y = 0; y < height; ++y) {
    RGBToYMatrixRow(src_raw, dst_yj, width, &kAbgrJPEGConstants);
    src_raw += src_stride_raw;
    dst_yj += dst_stride_yj;
  }
  return 0;
}

// Convert Android420 to I420.
LIBYUV_API
int Android420ToI420(const uint8_t* src_y,
                     int src_stride_y,
                     const uint8_t* src_u,
                     int src_stride_u,
                     const uint8_t* src_v,
                     int src_stride_v,
                     int src_pixel_stride_uv,
                     uint8_t* dst_y,
                     int dst_stride_y,
                     uint8_t* dst_u,
                     int dst_stride_u,
                     uint8_t* dst_v,
                     int dst_stride_v,
                     int width,
                     int height) {
  return Android420ToI420Rotate(src_y, src_stride_y, src_u, src_stride_u, src_v,
                                src_stride_v, src_pixel_stride_uv, dst_y,
                                dst_stride_y, dst_u, dst_stride_u, dst_v,
                                dst_stride_v, width, height, kRotate0);
}

// depth is source bits measured from lsb; For msb use 16
static int Biplanar16bitTo8bit(const uint16_t* src_y,
                               int src_stride_y,
                               const uint16_t* src_uv,
                               int src_stride_uv,
                               uint8_t* dst_y,
                               int dst_stride_y,
                               uint8_t* dst_uv,
                               int dst_stride_uv,
                               int width,
                               int height,
                               int subsample_x,
                               int subsample_y,
                               int depth) {
  if ((!src_y && dst_y) || !src_uv || !dst_uv || width <= 0 || height == 0 ||
      height == INT_MIN) {
    return -1;
  }
  int uv_width = SUBSAMPLE(width, subsample_x, subsample_x);
  int uv_height = SUBSAMPLE(height, subsample_y, subsample_y);
  int scale = 1 << (24 - depth);
  // Negative height means invert the image.
  if (height < 0) {
    height = -height;
    uv_height = -uv_height;
    src_y = src_y + (ptrdiff_t)(height - 1) * src_stride_y;
    src_uv = src_uv + (ptrdiff_t)(uv_height - 1) * src_stride_uv;
    src_stride_y = -src_stride_y;
    src_stride_uv = -src_stride_uv;
  }

  // Convert Y plane.
  if (dst_y) {
    Convert16To8Plane(src_y, src_stride_y, dst_y, dst_stride_y, scale, width,
                      height);
  }
  // Convert UV planes.
  Convert16To8Plane(src_uv, src_stride_uv, dst_uv, dst_stride_uv, scale,
                    uv_width * 2, uv_height);
  return 0;
}

// Convert 10 bit P010 to 8 bit NV12.
// Depth set to 16 because P010 uses 10 msb and this function keeps the upper 8
// bits of the specified number of bits.
LIBYUV_API
int P010ToNV12(const uint16_t* src_y,
               int src_stride_y,
               const uint16_t* src_uv,
               int src_stride_uv,
               uint8_t* dst_y,
               int dst_stride_y,
               uint8_t* dst_uv,
               int dst_stride_uv,
               int width,
               int height) {
  return Biplanar16bitTo8bit(src_y, src_stride_y, src_uv, src_stride_uv, dst_y,
                             dst_stride_y, dst_uv, dst_stride_uv, width, height,
                             1, 1, 16);
}

static int Planar8bitTo8bit(const uint8_t* src_y,
                            int src_stride_y,
                            const uint8_t* src_u,
                            int src_stride_u,
                            const uint8_t* src_v,
                            int src_stride_v,
                            uint8_t* dst_y,
                            int dst_stride_y,
                            uint8_t* dst_u,
                            int dst_stride_u,
                            uint8_t* dst_v,
                            int dst_stride_v,
                            int width,
                            int height,
                            int subsample_x,
                            int subsample_y,
                            int scale_y,
                            int bias_y,
                            int scale_uv,
                            int bias_uv) {
  if ((!src_y && dst_y) || !src_u || !src_v || !dst_u || !dst_v || width <= 0 ||
      height == 0 || height == INT_MIN) {
    return -1;
  }
  int uv_width = SUBSAMPLE(width, subsample_x, subsample_x);
  int uv_height = SUBSAMPLE(height, subsample_y, subsample_y);
  // Negative height means invert the image.
  if (height < 0) {
    height = -height;
    uv_height = -uv_height;
    src_y = src_y + (ptrdiff_t)(height - 1) * src_stride_y;
    src_u = src_u + (ptrdiff_t)(uv_height - 1) * src_stride_u;
    src_v = src_v + (ptrdiff_t)(uv_height - 1) * src_stride_v;
    src_stride_y = -src_stride_y;
    src_stride_u = -src_stride_u;
    src_stride_v = -src_stride_v;
  }

  // Convert Y plane.
  if (dst_y) {
    Convert8To8Plane(src_y, src_stride_y, dst_y, dst_stride_y, scale_y, bias_y,
                     width, height);
  }
  // Convert UV planes.
  Convert8To8Plane(src_u, src_stride_u, dst_u, dst_stride_u, scale_uv, bias_uv,
                   uv_width, uv_height);
  Convert8To8Plane(src_v, src_stride_v, dst_v, dst_stride_v, scale_uv, bias_uv,
                   uv_width, uv_height);
  return 0;
}

LIBYUV_API
int J420ToI420(const uint8_t* src_y,
               int src_stride_y,
               const uint8_t* src_u,
               int src_stride_u,
               const uint8_t* src_v,
               int src_stride_v,
               uint8_t* dst_y,
               int dst_stride_y,
               uint8_t* dst_u,
               int dst_stride_u,
               uint8_t* dst_v,
               int dst_stride_v,
               int width,
               int height) {
  return Planar8bitTo8bit(src_y, src_stride_y, src_u, src_stride_u, src_v,
                          src_stride_v, dst_y, dst_stride_y, dst_u,
                          dst_stride_u, dst_v, dst_stride_v, width, height, 1,
                          1, 220, 16, 225, 16);
}

#ifdef __cplusplus
}  // extern "C"
}  // namespace libyuv
#endif
