/*
 *  Copyright 2012 The LibYuv Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS. All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "libyuv/convert_from_argb.h"

#include <limits.h>

#include "libyuv/basic_types.h"
#include "libyuv/cpu_id.h"
#include "libyuv/planar_functions.h"
#include "libyuv/row.h"

#ifdef __cplusplus
namespace libyuv {
extern "C" {
#endif

// ARGB little endian (bgra in memory) to I444
LIBYUV_API
int ARGBToI444(const uint8_t* src_argb,
               int src_stride_argb,
               uint8_t* dst_y,
               int dst_stride_y,
               uint8_t* dst_u,
               int dst_stride_u,
               uint8_t* dst_v,
               int dst_stride_v,
               int width,
               int height) {
  return ARGBToI444Matrix(src_argb, src_stride_argb, dst_y, dst_stride_y, dst_u,
                          dst_stride_u, dst_v, dst_stride_v, &kArgbI601Constants,
                          width, height);
}

LIBYUV_API
int ARGBToI444Matrix(const uint8_t* src_argb,
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
  void (*ARGBToUV444MatrixRow)(const uint8_t* src_argb, uint8_t* dst_u,
                               uint8_t* dst_v, int width,
                               const struct ArgbConstants* c) =
ARGBToUV444MatrixRow_C;

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

#if defined(HAS_ARGBTOUV444MATRIXROW_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3)) {
    ARGBToUV444MatrixRow = ARGBToUV444MatrixRow_Any_SSSE3;
    if (IS_ALIGNED(width, 16)) {
      ARGBToUV444MatrixRow = ARGBToUV444MatrixRow_SSSE3;
    }
  }
#endif
#if defined(HAS_ARGBTOUV444MATRIXROW_AVX2)
  if (TestCpuFlag(kCpuHasAVX2)) {
    ARGBToUV444MatrixRow = ARGBToUV444MatrixRow_Any_AVX2;
    if (IS_ALIGNED(width, 32)) {
      ARGBToUV444MatrixRow = ARGBToUV444MatrixRow_AVX2;
    }
  }
#endif
#if defined(HAS_ARGBTOUV444MATRIXROW_AVX512BW)
  if (TestCpuFlag(kCpuHasAVX512BW)) {
    ARGBToUV444MatrixRow = ARGBToUV444MatrixRow_Any_AVX512BW;
    if (IS_ALIGNED(width, 64)) {
      ARGBToUV444MatrixRow = ARGBToUV444MatrixRow_AVX512BW;
    }
  }
#endif
#if defined(HAS_ARGBTOUV444MATRIXROW_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    ARGBToUV444MatrixRow = ARGBToUV444MatrixRow_Any_NEON;
    if (IS_ALIGNED(width, 8)) {
      ARGBToUV444MatrixRow = ARGBToUV444MatrixRow_NEON;
    }
  }
#endif
  if (!src_argb || !dst_y || !dst_u || !dst_v || !argbconstants || width <= 0 ||
      height == 0) {
    return -1;
  }
  // Negative height means invert the image.
  if (height < 0) {
    height = -height;
    src_argb = src_argb + (height - 1) * src_stride_argb;
    src_stride_argb = -src_stride_argb;
  }

  for (y = 0; y < height; ++y) {
    ARGBToYMatrixRow(src_argb, dst_y, width, argbconstants);
    ARGBToUV444MatrixRow(src_argb, dst_u, dst_v, width, argbconstants);
    src_argb += src_stride_argb;
    dst_y += dst_stride_y;
    dst_u += dst_stride_u;
    dst_v += dst_stride_v;
  }
  return 0;
}

// ARGB little endian (bgra in memory) to I422
LIBYUV_API
int ARGBToI422(const uint8_t* src_argb,
               int src_stride_argb,
               uint8_t* dst_y,
               int dst_stride_y,
               uint8_t* dst_u,
               int dst_stride_u,
               uint8_t* dst_v,
               int dst_stride_v,
               int width,
               int height) {
  return ARGBToI422Matrix(src_argb, src_stride_argb, dst_y, dst_stride_y, dst_u,
                          dst_stride_u, dst_v, dst_stride_v, &kArgbI601Constants,
                          width, height);
}

LIBYUV_API
int ARGBToI422Matrix(const uint8_t* src_argb,
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

#if defined(HAS_ARGBTOUVMATRIXROW_NEON)
    if (TestCpuFlag(kCpuHasNEON)) {
      ARGBToUVMatrixRow = ARGBToUVMatrixRow_Any_NEON;
      if (IS_ALIGNED(width, 16)) {
        ARGBToUVMatrixRow = ARGBToUVMatrixRow_NEON;
      }
    }
#endif
#if defined(HAS_ARGBTOUVMATRIXROW_NEON_I8MM)
    if (TestCpuFlag(kCpuHasNEON) && TestCpuFlag(kCpuHasNeonI8MM)) {
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
    if (IS_ALIGNED(width, 16)) {
      ARGBToUVMatrixRow = ARGBToUVMatrixRow_AVX2;
    }
  }
#endif
  if (!src_argb || !dst_y || !dst_u || !dst_v || !argbconstants || width <= 0 ||
      height == 0) {
    return -1;
  }
  // Negative height means invert the image.
  if (height < 0) {
    height = -height;
    src_argb = src_argb + (height - 1) * src_stride_argb;
    src_stride_argb = -src_stride_argb;
  }

  for (y = 0; y < height; ++y) {
    ARGBToUVMatrixRow(src_argb, 0, dst_u, dst_v, width, argbconstants);
    ARGBToYMatrixRow(src_argb, dst_y, width, argbconstants);
    src_argb += src_stride_argb;
    dst_y += dst_stride_y;
    dst_u += dst_stride_u;
    dst_v += dst_stride_v;
  }
  return 0;
}

LIBYUV_API
int ARGBToNV12(const uint8_t* src_argb,
               int src_stride_argb,
               uint8_t* dst_y,
               int dst_stride_y,
               uint8_t* dst_uv,
               int dst_stride_uv,
               int width,
               int height) {
  return ARGBToNV12Matrix(src_argb, src_stride_argb, dst_y, dst_stride_y, dst_uv,
                          dst_stride_uv, &kArgbI601Constants, width, height);
}

LIBYUV_API
int ARGBToNV12Matrix(const uint8_t* src_argb,
                     int src_stride_argb,
                     uint8_t* dst_y,
                     int dst_stride_y,
                     uint8_t* dst_uv,
                     int dst_stride_uv,
                     const struct ArgbConstants* argbconstants,
                     int width,
                     int height) {
  int y;
  int halfwidth = (width + 1) >> 1;
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

#if defined(HAS_ARGBTOUVMATRIXROW_NEON)
    if (TestCpuFlag(kCpuHasNEON)) {
      ARGBToUVMatrixRow = ARGBToUVMatrixRow_Any_NEON;
      if (IS_ALIGNED(width, 16)) {
        ARGBToUVMatrixRow = ARGBToUVMatrixRow_NEON;
      }
    }
#endif
#if defined(HAS_ARGBTOUVMATRIXROW_NEON_I8MM)
    if (TestCpuFlag(kCpuHasNEON) && TestCpuFlag(kCpuHasNeonI8MM)) {
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
    if (IS_ALIGNED(width, 16)) {
      ARGBToUVMatrixRow = ARGBToUVMatrixRow_AVX2;
    }
  }
#endif
  void (*MergeUVRow)(const uint8_t* src_u, const uint8_t* src_v,
                     uint8_t* dst_uv, int width) = MergeUVRow_C;
  if (!src_argb || !dst_y || !dst_uv || !argbconstants || width <= 0 ||
      height == 0) {
    return -1;
  }
  // Negative height means invert the image.
  if (height < 0) {
    height = -height;
    src_argb = src_argb + (height - 1) * src_stride_argb;
    src_stride_argb = -src_stride_argb;
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

  // Allocate a rows of uv.
  align_buffer_64(row_u, ((halfwidth + 31) & ~31) * 2);
  uint8_t* row_v = row_u + ((halfwidth + 31) & ~31);
  if (!row_u)
    return 1;

  for (y = 0; y < height - 1; y += 2) {
    ARGBToUVMatrixRow(src_argb, src_stride_argb, row_u, row_v, width,
                      argbconstants);
    MergeUVRow(row_u, row_v, dst_uv, halfwidth);
    ARGBToYMatrixRow(src_argb, dst_y, width, argbconstants);
    ARGBToYMatrixRow(src_argb + src_stride_argb, dst_y + dst_stride_y, width,
                      argbconstants);
    src_argb += src_stride_argb * 2;
    dst_y += dst_stride_y * 2;
    dst_uv += dst_stride_uv;
  }
  if (height & 1) {
    ARGBToUVMatrixRow(src_argb, 0, row_u, row_v, width, argbconstants);
    MergeUVRow(row_u, row_v, dst_uv, halfwidth);
    ARGBToYMatrixRow(src_argb, dst_y, width, argbconstants);
  }
  free_aligned_buffer_64(row_u);
  return 0;
}

int ARGBToNV21Matrix(const uint8_t* src_argb,
                     int src_stride_argb,
                     uint8_t* dst_y,
                     int dst_stride_y,
                     uint8_t* dst_vu,
                     int dst_stride_uv,
                     const struct ArgbConstants* argbconstants,
                     int width,
                     int height) {
  int y;
  int halfwidth = (width + 1) >> 1;
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

#if defined(HAS_ARGBTOUVMATRIXROW_NEON)
    if (TestCpuFlag(kCpuHasNEON)) {
      ARGBToUVMatrixRow = ARGBToUVMatrixRow_Any_NEON;
      if (IS_ALIGNED(width, 16)) {
        ARGBToUVMatrixRow = ARGBToUVMatrixRow_NEON;
      }
    }
#endif
#if defined(HAS_ARGBTOUVMATRIXROW_NEON_I8MM)
    if (TestCpuFlag(kCpuHasNEON) && TestCpuFlag(kCpuHasNeonI8MM)) {
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
    if (IS_ALIGNED(width, 16)) {
      ARGBToUVMatrixRow = ARGBToUVMatrixRow_AVX2;
    }
  }
#endif
  void (*MergeUVRow)(const uint8_t* src_u, const uint8_t* src_v,
                     uint8_t* dst_vu, int width) = MergeUVRow_C;
  if (!src_argb || !dst_y || !dst_vu || !argbconstants || width <= 0 ||
      height == 0) {
    return -1;
  }
  // Negative height means invert the image.
  if (height < 0) {
    height = -height;
    src_argb = src_argb + (height - 1) * src_stride_argb;
    src_stride_argb = -src_stride_argb;
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

  // Allocate a rows of uv.
  align_buffer_64(row_u, ((halfwidth + 31) & ~31) * 2);
  uint8_t* row_v = row_u + ((halfwidth + 31) & ~31);
  if (!row_u)
    return 1;

  for (y = 0; y < height - 1; y += 2) {
    ARGBToUVMatrixRow(src_argb, src_stride_argb, row_u, row_v, width,
                      argbconstants);
    MergeUVRow(row_u, row_v, dst_vu, halfwidth);
    ARGBToYMatrixRow(src_argb, dst_y, width, argbconstants);
    ARGBToYMatrixRow(src_argb + src_stride_argb, dst_y + dst_stride_y, width,
                      argbconstants);
    src_argb += src_stride_argb * 2;
    dst_y += dst_stride_y * 2;
    dst_vu += dst_stride_uv;
  }
  if (height & 1) {
    ARGBToUVMatrixRow(src_argb, 0, row_u, row_v, width, argbconstants);
    MergeUVRow(row_u, row_v, dst_vu, halfwidth);
    ARGBToYMatrixRow(src_argb, dst_y, width, argbconstants);
  }
  free_aligned_buffer_64(row_u);
  return 0;
}
LIBYUV_API
int ARGBToI400Matrix(const uint8_t* src_argb,
                     int src_stride_argb,
                     uint8_t* dst_y,
                     int dst_stride_y,
                     const struct ArgbConstants* constants,
                     int width,
                     int height) {
  int y;
  void (*ARGBToYMatrixRow)(const uint8_t* src_argb, uint8_t* dst_y, int width,
                           const struct ArgbConstants* c) = ARGBToYMatrixRow_C;
  if (!src_argb || !dst_y || !constants || width <= 0 || height == 0) {
    return -1;
  }
  if (height < 0) {
    height = -height;
    src_argb = src_argb + (height - 1) * src_stride_argb;
    src_stride_argb = -src_stride_argb;
  }
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

  for (y = 0; y < height; ++y) {
    ARGBToYMatrixRow(src_argb, dst_y, width, constants);
    src_argb += src_stride_argb;
    dst_y += dst_stride_y;
  }
  return 0;
}
LIBYUV_API
int ARGBToYUY2Matrix(const uint8_t* src_argb,
                     int src_stride_argb,
                     uint8_t* dst_yuy2,
                     int dst_stride_yuy2,
                     const struct ArgbConstants* constants,
                     int width,
                     int height) {
  int y;
  void (*ARGBToUVMatrixRow)(const uint8_t* src_argb, int src_stride_argb,
                            uint8_t* dst_u, uint8_t* dst_v, int width,
                            const struct ArgbConstants* c) = ARGBToUVMatrixRow_C;
  void (*ARGBToYMatrixRow)(const uint8_t* src_argb, uint8_t* dst_y, int width,
                           const struct ArgbConstants* c) = ARGBToYMatrixRow_C;
  void (*I422ToYUY2Row)(const uint8_t* src_y, const uint8_t* src_u,
                        const uint8_t* src_v, uint8_t* dst_yuy2, int width) =
      I422ToYUY2Row_C;

  if (!src_argb || !dst_yuy2 || !constants || width <= 0 || height == 0) {
    return -1;
  }
  if (height < 0) {
    height = -height;
    dst_yuy2 = dst_yuy2 + (height - 1) * dst_stride_yuy2;
    dst_stride_yuy2 = -dst_stride_yuy2;
  }
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
#if defined(HAS_ARGBTOUVMATRIXROW_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    ARGBToUVMatrixRow = ARGBToUVMatrixRow_Any_NEON;
    if (IS_ALIGNED(width, 16)) {
      ARGBToUVMatrixRow = ARGBToUVMatrixRow_NEON;
    }
  }
#endif
#if defined(HAS_I422TOYUY2ROW_AVX2)
  if (TestCpuFlag(kCpuHasAVX2)) {
    I422ToYUY2Row = I422ToYUY2Row_Any_AVX2;
    if (IS_ALIGNED(width, 32)) {
      I422ToYUY2Row = I422ToYUY2Row_AVX2;
    }
  }
#endif
#if defined(HAS_I422TOYUY2ROW_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    I422ToYUY2Row = I422ToYUY2Row_Any_NEON;
    if (IS_ALIGNED(width, 16)) {
      I422ToYUY2Row = I422ToYUY2Row_NEON;
    }
  }
#endif

  {
    align_buffer_64(row_y, ((width + 63) & ~63) * 2);
    uint8_t* row_u = row_y + ((width + 63) & ~63);
    uint8_t* row_v = row_u + ((width + 63) & ~63) / 2;
    if (!row_y)
      return 1;

    for (y = 0; y < height; ++y) {
      ARGBToUVMatrixRow(src_argb, 0, row_u, row_v, width, constants);
      ARGBToYMatrixRow(src_argb, row_y, width, constants);
      I422ToYUY2Row(row_y, row_u, row_v, dst_yuy2, width);
      src_argb += src_stride_argb;
      dst_yuy2 += dst_stride_yuy2;
    }

    free_aligned_buffer_64(row_y);
  }
  return 0;
}

LIBYUV_API
int ARGBToUYVYMatrix(const uint8_t* src_argb,
                     int src_stride_argb,
                     uint8_t* dst_uyvy,
                     int dst_stride_uyvy,
                     const struct ArgbConstants* constants,
                     int width,
                     int height) {
  int y;
  void (*ARGBToUVMatrixRow)(const uint8_t* src_argb, int src_stride_argb,
                            uint8_t* dst_u, uint8_t* dst_v, int width,
                            const struct ArgbConstants* c) = ARGBToUVMatrixRow_C;
  void (*ARGBToYMatrixRow)(const uint8_t* src_argb, uint8_t* dst_y, int width,
                           const struct ArgbConstants* c) = ARGBToYMatrixRow_C;
  void (*I422ToUYVYRow)(const uint8_t* src_y, const uint8_t* src_u,
                        const uint8_t* src_v, uint8_t* dst_uyvy, int width) =
      I422ToUYVYRow_C;

  if (!src_argb || !dst_uyvy || !constants || width <= 0 || height == 0) {
    return -1;
  }
  if (height < 0) {
    height = -height;
    dst_uyvy = dst_uyvy + (height - 1) * dst_stride_uyvy;
    dst_stride_uyvy = -dst_stride_uyvy;
  }
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
#if defined(HAS_ARGBTOUVMATRIXROW_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    ARGBToUVMatrixRow = ARGBToUVMatrixRow_Any_NEON;
    if (IS_ALIGNED(width, 16)) {
      ARGBToUVMatrixRow = ARGBToUVMatrixRow_NEON;
    }
  }
#endif
#if defined(HAS_I422TOUYVYROW_AVX2)
  if (TestCpuFlag(kCpuHasAVX2)) {
    I422ToUYVYRow = I422ToUYVYRow_Any_AVX2;
    if (IS_ALIGNED(width, 32)) {
      I422ToUYVYRow = I422ToUYVYRow_AVX2;
    }
  }
#endif
#if defined(HAS_I422TOUYVYROW_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    I422ToUYVYRow = I422ToUYVYRow_Any_NEON;
    if (IS_ALIGNED(width, 16)) {
      I422ToUYVYRow = I422ToUYVYRow_NEON;
    }
  }
#endif

  {
    align_buffer_64(row_y, ((width + 63) & ~63) * 2);
    uint8_t* row_u = row_y + ((width + 63) & ~63);
    uint8_t* row_v = row_u + ((width + 63) & ~63) / 2;
    if (!row_y)
      return 1;

    for (y = 0; y < height; ++y) {
      ARGBToUVMatrixRow(src_argb, 0, row_u, row_v, width, constants);
      ARGBToYMatrixRow(src_argb, row_y, width, constants);
      I422ToUYVYRow(row_y, row_u, row_v, dst_uyvy, width);
      src_argb += src_stride_argb;
      dst_uyvy += dst_stride_uyvy;
    }

    free_aligned_buffer_64(row_y);
  }
  return 0;
}



// Same as NV12 but U and V swapped.
LIBYUV_API
int ARGBToNV21(const uint8_t* src_argb,
               int src_stride_argb,
               uint8_t* dst_y,
               int dst_stride_y,
               uint8_t* dst_vu,
               int dst_stride_vu,
               int width,
               int height) {
  return ARGBToNV21Matrix(src_argb, src_stride_argb, dst_y, dst_stride_y, dst_vu,
                          dst_stride_vu, &kArgbI601Constants, width, height);
}

LIBYUV_API
int ABGRToNV12(const uint8_t* src_abgr,
               int src_stride_abgr,
               uint8_t* dst_y,
               int dst_stride_y,
               uint8_t* dst_uv,
               int dst_stride_uv,
               int width,
               int height) {
  return ARGBToNV12Matrix(src_abgr, src_stride_abgr, dst_y, dst_stride_y, dst_uv,
                          dst_stride_uv, &kAbgrI601Constants, width, height);
}

// Same as NV12 but U and V swapped.
LIBYUV_API
int ABGRToNV21(const uint8_t* src_abgr,
               int src_stride_abgr,
               uint8_t* dst_y,
               int dst_stride_y,
               uint8_t* dst_vu,
               int dst_stride_vu,
               int width,
               int height) {
  return ARGBToNV21Matrix(src_abgr, src_stride_abgr, dst_y, dst_stride_y, dst_vu,
                          dst_stride_vu, &kAbgrI601Constants, width, height);
}

// Convert ARGB to YUY2.
LIBYUV_API
int ARGBToYUY2(const uint8_t* src_argb,
               int src_stride_argb,
               uint8_t* dst_yuy2,
               int dst_stride_yuy2,
               int width,
               int height) {
  return ARGBToYUY2Matrix(src_argb, src_stride_argb, dst_yuy2, dst_stride_yuy2,
                          &kArgbI601Constants, width, height);
}

// Convert ARGB to UYVY.
LIBYUV_API
int ARGBToUYVY(const uint8_t* src_argb,
               int src_stride_argb,
               uint8_t* dst_uyvy,
               int dst_stride_uyvy,
               int width,
               int height) {
  return ARGBToUYVYMatrix(src_argb, src_stride_argb, dst_uyvy, dst_stride_uyvy,
                          &kArgbI601Constants, width, height);
}

// Convert ARGB to I400.
LIBYUV_API
int ARGBToI400(const uint8_t* src_argb,
               int src_stride_argb,
               uint8_t* dst_y,
               int dst_stride_y,
               int width,
               int height) {
  return ARGBToI400Matrix(src_argb, src_stride_argb, dst_y, dst_stride_y,
                          &kArgbI601Constants, width, height);
}

#ifndef __riscv
// Shuffle table for converting ARGB to RGBA.
static const uvec8 kShuffleMaskARGBToRGBA = {
    3u, 0u, 1u, 2u, 7u, 4u, 5u, 6u, 11u, 8u, 9u, 10u, 15u, 12u, 13u, 14u};

// Convert ARGB to RGBA.
LIBYUV_API
int ARGBToRGBA(const uint8_t* src_argb,
               int src_stride_argb,
               uint8_t* dst_rgba,
               int dst_stride_rgba,
               int width,
               int height) {
  return ARGBShuffle(src_argb, src_stride_argb, dst_rgba, dst_stride_rgba,
                     (const uint8_t*)(&kShuffleMaskARGBToRGBA), width, height);
}
#else
// Convert ARGB to RGBA.
LIBYUV_API
int ARGBToRGBA(const uint8_t* src_argb,
               int src_stride_argb,
               uint8_t* dst_rgba,
               int dst_stride_rgba,
               int width,
               int height) {
  int y;
  void (*ARGBToRGBARow)(const uint8_t* src_argb, uint8_t* dst_rgba, int width) =
      ARGBToRGBARow_C;
  if (!src_argb || !dst_rgba || width <= 0 || height == 0) {
    return -1;
  }
  if (height < 0) {
    height = -height;
    src_argb = src_argb + (height - 1) * src_stride_argb;
    src_stride_argb = -src_stride_argb;
  }
  // Coalesce rows.
  if (src_stride_argb == width * 4 && dst_stride_rgba == width * 4 &&
      (ptrdiff_t)width * height <= INT_MAX) {
    width *= height;
    height = 1;
    src_stride_argb = dst_stride_rgba = 0;
  }

#if defined(HAS_ARGBTORGBAROW_RVV)
  if (TestCpuFlag(kCpuHasRVV)) {
    ARGBToRGBARow = ARGBToRGBARow_RVV;
  }
#endif

  for (y = 0; y < height; ++y) {
    ARGBToRGBARow(src_argb, dst_rgba, width);
    src_argb += src_stride_argb;
    dst_rgba += dst_stride_rgba;
  }
  return 0;
}
#endif

// Convert ARGB To RGB24.
LIBYUV_API
int ARGBToRGB24(const uint8_t* src_argb,
                int src_stride_argb,
                uint8_t* dst_rgb24,
                int dst_stride_rgb24,
                int width,
                int height) {
  int y;
  void (*ARGBToRGB24Row)(const uint8_t* src_argb, uint8_t* dst_rgb, int width) =
      ARGBToRGB24Row_C;
  if (!src_argb || !dst_rgb24 || width <= 0 || height == 0) {
    return -1;
  }
  if (height < 0) {
    height = -height;
    src_argb = src_argb + (height - 1) * src_stride_argb;
    src_stride_argb = -src_stride_argb;
  }
  // Coalesce rows.
  if (src_stride_argb == width * 4 && dst_stride_rgb24 == width * 3 &&
      (ptrdiff_t)width * height <= INT_MAX) {
    width *= height;
    height = 1;
    src_stride_argb = dst_stride_rgb24 = 0;
  }
#if defined(HAS_ARGBTORGB24ROW_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3)) {
    ARGBToRGB24Row = ARGBToRGB24Row_Any_SSSE3;
    if (IS_ALIGNED(width, 16)) {
      ARGBToRGB24Row = ARGBToRGB24Row_SSSE3;
    }
  }
#endif
#if defined(HAS_ARGBTORGB24ROW_AVX2)
  if (TestCpuFlag(kCpuHasAVX2)) {
    ARGBToRGB24Row = ARGBToRGB24Row_Any_AVX2;
    if (IS_ALIGNED(width, 32)) {
      ARGBToRGB24Row = ARGBToRGB24Row_AVX2;
    }
  }
#endif
#if defined(HAS_ARGBTORGB24ROW_AVX512VBMI)
  if (TestCpuFlag(kCpuHasAVX512VBMI)) {
    ARGBToRGB24Row = ARGBToRGB24Row_Any_AVX512VBMI;
    if (IS_ALIGNED(width, 32)) {
      ARGBToRGB24Row = ARGBToRGB24Row_AVX512VBMI;
    }
  }
#endif
#if defined(HAS_ARGBTORGB24ROW_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    ARGBToRGB24Row = ARGBToRGB24Row_Any_NEON;
    if (IS_ALIGNED(width, 16)) {
      ARGBToRGB24Row = ARGBToRGB24Row_NEON;
    }
  }
#endif
#if defined(HAS_ARGBTORGB24ROW_SVE2)
  if (TestCpuFlag(kCpuHasSVE2)) {
    ARGBToRGB24Row = ARGBToRGB24Row_SVE2;
  }
#endif
#if defined(HAS_ARGBTORGB24ROW_LSX)
  if (TestCpuFlag(kCpuHasLSX)) {
    ARGBToRGB24Row = ARGBToRGB24Row_Any_LSX;
    if (IS_ALIGNED(width, 16)) {
      ARGBToRGB24Row = ARGBToRGB24Row_LSX;
    }
  }
#endif
#if defined(HAS_ARGBTORGB24ROW_LASX)
  if (TestCpuFlag(kCpuHasLASX)) {
    ARGBToRGB24Row = ARGBToRGB24Row_Any_LASX;
    if (IS_ALIGNED(width, 32)) {
      ARGBToRGB24Row = ARGBToRGB24Row_LASX;
    }
  }
#endif
#if defined(HAS_ARGBTORGB24ROW_RVV)
  if (TestCpuFlag(kCpuHasRVV)) {
    ARGBToRGB24Row = ARGBToRGB24Row_RVV;
  }
#endif

  for (y = 0; y < height; ++y) {
    ARGBToRGB24Row(src_argb, dst_rgb24, width);
    src_argb += src_stride_argb;
    dst_rgb24 += dst_stride_rgb24;
  }
  return 0;
}

// Convert ARGB To RAW.
LIBYUV_API
int ARGBToRAW(const uint8_t* src_argb,
              int src_stride_argb,
              uint8_t* dst_raw,
              int dst_stride_raw,
              int width,
              int height) {
  int y;
  void (*ARGBToRAWRow)(const uint8_t* src_argb, uint8_t* dst_rgb, int width) =
      ARGBToRAWRow_C;
  if (!src_argb || !dst_raw || width <= 0 || height == 0) {
    return -1;
  }
  if (height < 0) {
    height = -height;
    src_argb = src_argb + (height - 1) * src_stride_argb;
    src_stride_argb = -src_stride_argb;
  }
  // Coalesce rows.
  if (src_stride_argb == width * 4 && dst_stride_raw == width * 3 &&
      (ptrdiff_t)width * height <= INT_MAX) {
    width *= height;
    height = 1;
    src_stride_argb = dst_stride_raw = 0;
  }
#if defined(HAS_ARGBTORAWROW_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3)) {
    ARGBToRAWRow = ARGBToRAWRow_Any_SSSE3;
    if (IS_ALIGNED(width, 16)) {
      ARGBToRAWRow = ARGBToRAWRow_SSSE3;
    }
  }
#endif
#if defined(HAS_ARGBTORAWROW_AVX2)
  if (TestCpuFlag(kCpuHasAVX2)) {
    ARGBToRAWRow = ARGBToRAWRow_Any_AVX2;
    if (IS_ALIGNED(width, 32)) {
      ARGBToRAWRow = ARGBToRAWRow_AVX2;
    }
  }
#endif
#if defined(HAS_ARGBTORAWROW_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    ARGBToRAWRow = ARGBToRAWRow_Any_NEON;
    if (IS_ALIGNED(width, 8)) {
      ARGBToRAWRow = ARGBToRAWRow_NEON;
    }
  }
#endif
#if defined(HAS_ARGBTORAWROW_SVE2)
  if (TestCpuFlag(kCpuHasSVE2)) {
    ARGBToRAWRow = ARGBToRAWRow_SVE2;
  }
#endif
#if defined(HAS_ARGBTORAWROW_LSX)
  if (TestCpuFlag(kCpuHasLSX)) {
    ARGBToRAWRow = ARGBToRAWRow_Any_LSX;
    if (IS_ALIGNED(width, 16)) {
      ARGBToRAWRow = ARGBToRAWRow_LSX;
    }
  }
#endif
#if defined(HAS_ARGBTORAWROW_LASX)
  if (TestCpuFlag(kCpuHasLASX)) {
    ARGBToRAWRow = ARGBToRAWRow_Any_LASX;
    if (IS_ALIGNED(width, 32)) {
      ARGBToRAWRow = ARGBToRAWRow_LASX;
    }
  }
#endif
#if defined(HAS_ARGBTORAWROW_RVV)
  if (TestCpuFlag(kCpuHasRVV)) {
    ARGBToRAWRow = ARGBToRAWRow_RVV;
  }
#endif

  for (y = 0; y < height; ++y) {
    ARGBToRAWRow(src_argb, dst_raw, width);
    src_argb += src_stride_argb;
    dst_raw += dst_stride_raw;
  }
  return 0;
}

// Ordered 8x8 dither for 888 to 565.  Values from 0 to 7.
static const uint8_t kDither565_4x4[16] = {
    0, 4, 1, 5, 6, 2, 7, 3, 1, 5, 0, 4, 7, 3, 6, 2,
};

// Convert ARGB To RGB565 with 4x4 dither matrix (16 bytes).
LIBYUV_API
int ARGBToRGB565Dither(const uint8_t* src_argb,
                       int src_stride_argb,
                       uint8_t* dst_rgb565,
                       int dst_stride_rgb565,
                       const uint8_t* dither4x4,
                       int width,
                       int height) {
  int y;
  void (*ARGBToRGB565DitherRow)(const uint8_t* src_argb, uint8_t* dst_rgb,
                                uint32_t dither4, int width) =
      ARGBToRGB565DitherRow_C;
  if (!src_argb || !dst_rgb565 || width <= 0 || height == 0) {
    return -1;
  }
  if (height < 0) {
    height = -height;
    src_argb = src_argb + (height - 1) * src_stride_argb;
    src_stride_argb = -src_stride_argb;
  }
  if (!dither4x4) {
    dither4x4 = kDither565_4x4;
  }
#if defined(HAS_ARGBTORGB565DITHERROW_SSE2)
  if (TestCpuFlag(kCpuHasSSE2)) {
    ARGBToRGB565DitherRow = ARGBToRGB565DitherRow_Any_SSE2;
    if (IS_ALIGNED(width, 4)) {
      ARGBToRGB565DitherRow = ARGBToRGB565DitherRow_SSE2;
    }
  }
#endif
#if defined(HAS_ARGBTORGB565DITHERROW_AVX2)
  if (TestCpuFlag(kCpuHasAVX2)) {
    ARGBToRGB565DitherRow = ARGBToRGB565DitherRow_Any_AVX2;
    if (IS_ALIGNED(width, 8)) {
      ARGBToRGB565DitherRow = ARGBToRGB565DitherRow_AVX2;
    }
  }
#endif
#if defined(HAS_ARGBTORGB565DITHERROW_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    ARGBToRGB565DitherRow = ARGBToRGB565DitherRow_Any_NEON;
    if (IS_ALIGNED(width, 8)) {
      ARGBToRGB565DitherRow = ARGBToRGB565DitherRow_NEON;
    }
  }
#endif
#if defined(HAS_ARGBTORGB565DITHERROW_SVE2)
  if (TestCpuFlag(kCpuHasSVE2)) {
    ARGBToRGB565DitherRow = ARGBToRGB565DitherRow_SVE2;
  }
#endif
#if defined(HAS_ARGBTORGB565DITHERROW_LSX)
  if (TestCpuFlag(kCpuHasLSX)) {
    ARGBToRGB565DitherRow = ARGBToRGB565DitherRow_Any_LSX;
    if (IS_ALIGNED(width, 8)) {
      ARGBToRGB565DitherRow = ARGBToRGB565DitherRow_LSX;
    }
  }
#endif
#if defined(HAS_ARGBTORGB565DITHERROW_LASX)
  if (TestCpuFlag(kCpuHasLASX)) {
    ARGBToRGB565DitherRow = ARGBToRGB565DitherRow_Any_LASX;
    if (IS_ALIGNED(width, 16)) {
      ARGBToRGB565DitherRow = ARGBToRGB565DitherRow_LASX;
    }
  }
#endif

  for (y = 0; y < height; ++y) {
    ARGBToRGB565DitherRow(src_argb, dst_rgb565,
                          *(const uint32_t*)(dither4x4 + ((y & 3) << 2)),
                          width);
    src_argb += src_stride_argb;
    dst_rgb565 += dst_stride_rgb565;
  }
  return 0;
}

// Convert ARGB To RGB565.
// TODO(fbarchard): Consider using dither function low level with zeros.
LIBYUV_API
int ARGBToRGB565(const uint8_t* src_argb,
                 int src_stride_argb,
                 uint8_t* dst_rgb565,
                 int dst_stride_rgb565,
                 int width,
                 int height) {
  int y;
  void (*ARGBToRGB565Row)(const uint8_t* src_argb, uint8_t* dst_rgb,
                          int width) = ARGBToRGB565Row_C;
  if (!src_argb || !dst_rgb565 || width <= 0 || height == 0) {
    return -1;
  }
  if (height < 0) {
    height = -height;
    src_argb = src_argb + (height - 1) * src_stride_argb;
    src_stride_argb = -src_stride_argb;
  }
  // Coalesce rows.
  if (src_stride_argb == width * 4 && dst_stride_rgb565 == width * 2 &&
      (ptrdiff_t)width * height <= INT_MAX) {
    width *= height;
    height = 1;
    src_stride_argb = dst_stride_rgb565 = 0;
  }
#if defined(HAS_ARGBTORGB565ROW_SSE2)
  if (TestCpuFlag(kCpuHasSSE2)) {
    ARGBToRGB565Row = ARGBToRGB565Row_Any_SSE2;
    if (IS_ALIGNED(width, 4)) {
      ARGBToRGB565Row = ARGBToRGB565Row_SSE2;
    }
  }
#endif
#if defined(HAS_ARGBTORGB565ROW_AVX2)
  if (TestCpuFlag(kCpuHasAVX2)) {
    ARGBToRGB565Row = ARGBToRGB565Row_Any_AVX2;
    if (IS_ALIGNED(width, 8)) {
      ARGBToRGB565Row = ARGBToRGB565Row_AVX2;
    }
  }
#endif
#if defined(HAS_ARGBTORGB565ROW_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    ARGBToRGB565Row = ARGBToRGB565Row_Any_NEON;
    if (IS_ALIGNED(width, 8)) {
      ARGBToRGB565Row = ARGBToRGB565Row_NEON;
    }
  }
#endif
#if defined(HAS_ARGBTORGB565ROW_SVE2)
  if (TestCpuFlag(kCpuHasSVE2)) {
    ARGBToRGB565Row = ARGBToRGB565Row_SVE2;
  }
#endif
#if defined(HAS_ARGBTORGB565ROW_LSX)
  if (TestCpuFlag(kCpuHasLSX)) {
    ARGBToRGB565Row = ARGBToRGB565Row_Any_LSX;
    if (IS_ALIGNED(width, 8)) {
      ARGBToRGB565Row = ARGBToRGB565Row_LSX;
    }
  }
#endif

#if defined(HAS_ARGBTORGB565ROW_LASX)
  if (TestCpuFlag(kCpuHasLASX)) {
    ARGBToRGB565Row = ARGBToRGB565Row_Any_LASX;
    if (IS_ALIGNED(width, 16)) {
      ARGBToRGB565Row = ARGBToRGB565Row_LASX;
    }
  }
#endif

  for (y = 0; y < height; ++y) {
    ARGBToRGB565Row(src_argb, dst_rgb565, width);
    src_argb += src_stride_argb;
    dst_rgb565 += dst_stride_rgb565;
  }
  return 0;
}

// Convert ARGB To ARGB1555.
LIBYUV_API
int ARGBToARGB1555(const uint8_t* src_argb,
                   int src_stride_argb,
                   uint8_t* dst_argb1555,
                   int dst_stride_argb1555,
                   int width,
                   int height) {
  int y;
  void (*ARGBToARGB1555Row)(const uint8_t* src_argb, uint8_t* dst_rgb,
                            int width) = ARGBToARGB1555Row_C;
  if (!src_argb || !dst_argb1555 || width <= 0 || height == 0) {
    return -1;
  }
  if (height < 0) {
    height = -height;
    src_argb = src_argb + (height - 1) * src_stride_argb;
    src_stride_argb = -src_stride_argb;
  }
  // Coalesce rows.
  if (src_stride_argb == width * 4 && dst_stride_argb1555 == width * 2 &&
      (ptrdiff_t)width * height <= INT_MAX) {
    width *= height;
    height = 1;
    src_stride_argb = dst_stride_argb1555 = 0;
  }
#if defined(HAS_ARGBTOARGB1555ROW_SSE2)
  if (TestCpuFlag(kCpuHasSSE2)) {
    ARGBToARGB1555Row = ARGBToARGB1555Row_Any_SSE2;
    if (IS_ALIGNED(width, 4)) {
      ARGBToARGB1555Row = ARGBToARGB1555Row_SSE2;
    }
  }
#endif
#if defined(HAS_ARGBTOARGB1555ROW_AVX2)
  if (TestCpuFlag(kCpuHasAVX2)) {
    ARGBToARGB1555Row = ARGBToARGB1555Row_Any_AVX2;
    if (IS_ALIGNED(width, 8)) {
      ARGBToARGB1555Row = ARGBToARGB1555Row_AVX2;
    }
  }
#endif
#if defined(HAS_ARGBTOARGB1555ROW_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    ARGBToARGB1555Row = ARGBToARGB1555Row_Any_NEON;
    if (IS_ALIGNED(width, 8)) {
      ARGBToARGB1555Row = ARGBToARGB1555Row_NEON;
    }
  }
#endif
#if defined(HAS_ARGBTOARGB1555ROW_LSX)
  if (TestCpuFlag(kCpuHasLSX)) {
    ARGBToARGB1555Row = ARGBToARGB1555Row_Any_LSX;
    if (IS_ALIGNED(width, 8)) {
      ARGBToARGB1555Row = ARGBToARGB1555Row_LSX;
    }
  }
#endif
#if defined(HAS_ARGBTOARGB1555ROW_LASX)
  if (TestCpuFlag(kCpuHasLASX)) {
    ARGBToARGB1555Row = ARGBToARGB1555Row_Any_LASX;
    if (IS_ALIGNED(width, 16)) {
      ARGBToARGB1555Row = ARGBToARGB1555Row_LASX;
    }
  }
#endif

  for (y = 0; y < height; ++y) {
    ARGBToARGB1555Row(src_argb, dst_argb1555, width);
    src_argb += src_stride_argb;
    dst_argb1555 += dst_stride_argb1555;
  }
  return 0;
}

// Convert ARGB To ARGB4444.
LIBYUV_API
int ARGBToARGB4444(const uint8_t* src_argb,
                   int src_stride_argb,
                   uint8_t* dst_argb4444,
                   int dst_stride_argb4444,
                   int width,
                   int height) {
  int y;
  void (*ARGBToARGB4444Row)(const uint8_t* src_argb, uint8_t* dst_rgb,
                            int width) = ARGBToARGB4444Row_C;
  if (!src_argb || !dst_argb4444 || width <= 0 || height == 0) {
    return -1;
  }
  if (height < 0) {
    height = -height;
    src_argb = src_argb + (height - 1) * src_stride_argb;
    src_stride_argb = -src_stride_argb;
  }
  // Coalesce rows.
  if (src_stride_argb == width * 4 && dst_stride_argb4444 == width * 2 &&
      (ptrdiff_t)width * height <= INT_MAX) {
    width *= height;
    height = 1;
    src_stride_argb = dst_stride_argb4444 = 0;
  }
#if defined(HAS_ARGBTOARGB4444ROW_SSE2)
  if (TestCpuFlag(kCpuHasSSE2)) {
    ARGBToARGB4444Row = ARGBToARGB4444Row_Any_SSE2;
    if (IS_ALIGNED(width, 4)) {
      ARGBToARGB4444Row = ARGBToARGB4444Row_SSE2;
    }
  }
#endif
#if defined(HAS_ARGBTOARGB4444ROW_AVX2)
  if (TestCpuFlag(kCpuHasAVX2)) {
    ARGBToARGB4444Row = ARGBToARGB4444Row_Any_AVX2;
    if (IS_ALIGNED(width, 8)) {
      ARGBToARGB4444Row = ARGBToARGB4444Row_AVX2;
    }
  }
#endif
#if defined(HAS_ARGBTOARGB4444ROW_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    ARGBToARGB4444Row = ARGBToARGB4444Row_Any_NEON;
    if (IS_ALIGNED(width, 8)) {
      ARGBToARGB4444Row = ARGBToARGB4444Row_NEON;
    }
  }
#endif
#if defined(HAS_ARGBTOARGB4444ROW_LSX)
  if (TestCpuFlag(kCpuHasLSX)) {
    ARGBToARGB4444Row = ARGBToARGB4444Row_Any_LSX;
    if (IS_ALIGNED(width, 8)) {
      ARGBToARGB4444Row = ARGBToARGB4444Row_LSX;
    }
  }
#endif
#if defined(HAS_ARGBTOARGB4444ROW_LASX)
  if (TestCpuFlag(kCpuHasLASX)) {
    ARGBToARGB4444Row = ARGBToARGB4444Row_Any_LASX;
    if (IS_ALIGNED(width, 16)) {
      ARGBToARGB4444Row = ARGBToARGB4444Row_LASX;
    }
  }
#endif

  for (y = 0; y < height; ++y) {
    ARGBToARGB4444Row(src_argb, dst_argb4444, width);
    src_argb += src_stride_argb;
    dst_argb4444 += dst_stride_argb4444;
  }
  return 0;
}

// Convert ABGR To AR30.
LIBYUV_API
int ABGRToAR30(const uint8_t* src_abgr,
               int src_stride_abgr,
               uint8_t* dst_ar30,
               int dst_stride_ar30,
               int width,
               int height) {
  int y;
  void (*ABGRToAR30Row)(const uint8_t* src_abgr, uint8_t* dst_rgb, int width) =
      ABGRToAR30Row_C;
  if (!src_abgr || !dst_ar30 || width <= 0 || height == 0) {
    return -1;
  }
  if (height < 0) {
    height = -height;
    src_abgr = src_abgr + (height - 1) * src_stride_abgr;
    src_stride_abgr = -src_stride_abgr;
  }
  // Coalesce rows.
  if (src_stride_abgr == width * 4 && dst_stride_ar30 == width * 4 &&
      (ptrdiff_t)width * height <= INT_MAX) {
    width *= height;
    height = 1;
    src_stride_abgr = dst_stride_ar30 = 0;
  }
#if defined(HAS_ABGRTOAR30ROW_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    ABGRToAR30Row = ABGRToAR30Row_Any_NEON;
    if (IS_ALIGNED(width, 8)) {
      ABGRToAR30Row = ABGRToAR30Row_NEON;
    }
  }
#endif
#if defined(HAS_ABGRTOAR30ROW_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3)) {
    ABGRToAR30Row = ABGRToAR30Row_Any_SSSE3;
    if (IS_ALIGNED(width, 4)) {
      ABGRToAR30Row = ABGRToAR30Row_SSSE3;
    }
  }
#endif
#if defined(HAS_ABGRTOAR30ROW_AVX2)
  if (TestCpuFlag(kCpuHasAVX2)) {
    ABGRToAR30Row = ABGRToAR30Row_Any_AVX2;
    if (IS_ALIGNED(width, 8)) {
      ABGRToAR30Row = ABGRToAR30Row_AVX2;
    }
  }
#endif
  for (y = 0; y < height; ++y) {
    ABGRToAR30Row(src_abgr, dst_ar30, width);
    src_abgr += src_stride_abgr;
    dst_ar30 += dst_stride_ar30;
  }
  return 0;
}

// Convert ARGB To AR30.
LIBYUV_API
int ARGBToAR30(const uint8_t* src_argb,
               int src_stride_argb,
               uint8_t* dst_ar30,
               int dst_stride_ar30,
               int width,
               int height) {
  int y;
  void (*ARGBToAR30Row)(const uint8_t* src_argb, uint8_t* dst_rgb, int width) =
      ARGBToAR30Row_C;
  if (!src_argb || !dst_ar30 || width <= 0 || height == 0) {
    return -1;
  }
  if (height < 0) {
    height = -height;
    src_argb = src_argb + (height - 1) * src_stride_argb;
    src_stride_argb = -src_stride_argb;
  }
  // Coalesce rows.
  if (src_stride_argb == width * 4 && dst_stride_ar30 == width * 4 &&
      (ptrdiff_t)width * height <= INT_MAX) {
    width *= height;
    height = 1;
    src_stride_argb = dst_stride_ar30 = 0;
  }
#if defined(HAS_ARGBTOAR30ROW_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    ARGBToAR30Row = ARGBToAR30Row_Any_NEON;
    if (IS_ALIGNED(width, 8)) {
      ARGBToAR30Row = ARGBToAR30Row_NEON;
    }
  }
#endif
#if defined(HAS_ARGBTOAR30ROW_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3)) {
    ARGBToAR30Row = ARGBToAR30Row_Any_SSSE3;
    if (IS_ALIGNED(width, 4)) {
      ARGBToAR30Row = ARGBToAR30Row_SSSE3;
    }
  }
#endif
#if defined(HAS_ARGBTOAR30ROW_AVX2)
  if (TestCpuFlag(kCpuHasAVX2)) {
    ARGBToAR30Row = ARGBToAR30Row_Any_AVX2;
    if (IS_ALIGNED(width, 8)) {
      ARGBToAR30Row = ARGBToAR30Row_AVX2;
    }
  }
#endif
  for (y = 0; y < height; ++y) {
    ARGBToAR30Row(src_argb, dst_ar30, width);
    src_argb += src_stride_argb;
    dst_ar30 += dst_stride_ar30;
  }
  return 0;
}

// ARGB little endian (bgra in memory) to J444
LIBYUV_API
int ARGBToJ444(const uint8_t* src_argb,
               int src_stride_argb,
               uint8_t* dst_y,
               int dst_stride_y,
               uint8_t* dst_u,
               int dst_stride_u,
               uint8_t* dst_v,
               int dst_stride_v,
               int width,
               int height) {
  return ARGBToI444Matrix(src_argb, src_stride_argb, dst_y, dst_stride_y, dst_u,
                          dst_stride_u, dst_v, dst_stride_v, &kArgbJPEGConstants,
                          width, height);
}

// Convert ARGB to J420. (JPeg full range I420).
LIBYUV_API
int ARGBToJ420(const uint8_t* src_argb,
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
                          dst_stride_u, dst_v, dst_stride_v, &kArgbJPEGConstants,
                          width, height);
}

// Convert ARGB to J422. (JPeg full range I422).
LIBYUV_API
int ARGBToJ422(const uint8_t* src_argb,
               int src_stride_argb,
               uint8_t* dst_y,
               int dst_stride_y,
               uint8_t* dst_u,
               int dst_stride_u,
               uint8_t* dst_v,
               int dst_stride_v,
               int width,
               int height) {
  return ARGBToI422Matrix(src_argb, src_stride_argb, dst_y, dst_stride_y, dst_u,
                          dst_stride_u, dst_v, dst_stride_v, &kArgbJPEGConstants,
                          width, height);
}

// Convert ARGB to J400.
LIBYUV_API
int ARGBToJ400(const uint8_t* src_argb,
               int src_stride_argb,
               uint8_t* dst_y,
               int dst_stride_y,
               int width,
               int height) {
  return ARGBToI400Matrix(src_argb, src_stride_argb, dst_y, dst_stride_y,
                          &kArgbJPEGConstants, width, height);
}

// Convert RGBA to J400.
LIBYUV_API
int RGBAToJ400(const uint8_t* src_rgba,
               int src_stride_rgba,
               uint8_t* dst_yj,
               int dst_stride_yj,
               int width,
               int height) {
  int y;
  void (*RGBAToYJRow)(const uint8_t* src_rgba, uint8_t* dst_yj, int width) =
      RGBAToYJRow_C;
  if (!src_rgba || !dst_yj || width <= 0 || height == 0) {
    return -1;
  }
  if (height < 0) {
    height = -height;
    src_rgba = src_rgba + (height - 1) * src_stride_rgba;
    src_stride_rgba = -src_stride_rgba;
  }
  // Coalesce rows.
  if (src_stride_rgba == width * 4 && dst_stride_yj == width &&
      (ptrdiff_t)width * height <= INT_MAX) {
    width *= height;
    height = 1;
    src_stride_rgba = dst_stride_yj = 0;
  }
#if defined(HAS_RGBATOYJROW_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3)) {
    RGBAToYJRow = RGBAToYJRow_Any_SSSE3;
    if (IS_ALIGNED(width, 16)) {
      RGBAToYJRow = RGBAToYJRow_SSSE3;
    }
  }
#endif
#if defined(HAS_RGBATOYJROW_AVX2)
  if (TestCpuFlag(kCpuHasAVX2)) {
    RGBAToYJRow = RGBAToYJRow_Any_AVX2;
    if (IS_ALIGNED(width, 32)) {
      RGBAToYJRow = RGBAToYJRow_AVX2;
    }
  }
#endif
#if defined(HAS_ARGBTOYROW_AVX512BW)
  if (TestCpuFlag(kCpuHasAVX512BW)) {
    RGBAToYJRow = RGBAToYJRow_Any_AVX512BW;
    if (IS_ALIGNED(width, 64)) {
      RGBAToYJRow = RGBAToYJRow_AVX512BW;
    }
  }
#endif
#if defined(HAS_RGBATOYJROW_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    RGBAToYJRow = RGBAToYJRow_Any_NEON;
    if (IS_ALIGNED(width, 16)) {
      RGBAToYJRow = RGBAToYJRow_NEON;
    }
  }
#endif
#if defined(HAS_RGBATOYJROW_NEON_DOTPROD)
  if (TestCpuFlag(kCpuHasNeonDotProd)) {
    RGBAToYJRow = RGBAToYJRow_Any_NEON_DotProd;
    if (IS_ALIGNED(width, 16)) {
      RGBAToYJRow = RGBAToYJRow_NEON_DotProd;
    }
  }
#endif
#if defined(HAS_RGBATOYJROW_LSX)
  if (TestCpuFlag(kCpuHasLSX)) {
    RGBAToYJRow = RGBAToYJRow_Any_LSX;
    if (IS_ALIGNED(width, 16)) {
      RGBAToYJRow = RGBAToYJRow_LSX;
    }
  }
#endif
#if defined(HAS_RGBATOYJROW_LASX)
  if (TestCpuFlag(kCpuHasLASX)) {
    RGBAToYJRow = RGBAToYJRow_Any_LASX;
    if (IS_ALIGNED(width, 32)) {
      RGBAToYJRow = RGBAToYJRow_LASX;
    }
  }
#endif
#if defined(HAS_RGBATOYJROW_RVV)
  if (TestCpuFlag(kCpuHasRVV)) {
    RGBAToYJRow = RGBAToYJRow_RVV;
  }
#endif

  for (y = 0; y < height; ++y) {
    RGBAToYJRow(src_rgba, dst_yj, width);
    src_rgba += src_stride_rgba;
    dst_yj += dst_stride_yj;
  }
  return 0;
}

// Convert ABGR to J420. (JPeg full range I420).
LIBYUV_API
int ABGRToJ420(const uint8_t* src_abgr,
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
                          dst_stride_u, dst_v, dst_stride_v, &kAbgrJPEGConstants,
                          width, height);
}

// Convert ABGR to J422. (JPeg full range I422).
LIBYUV_API
int ABGRToJ422(const uint8_t* src_abgr,
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
                          dst_stride_u, dst_v, dst_stride_v, &kAbgrJPEGConstants,
                          width, height);
}

// Convert ABGR to J400.
LIBYUV_API
int ABGRToJ400(const uint8_t* src_abgr,
               int src_stride_abgr,
               uint8_t* dst_yj,
               int dst_stride_yj,
               int width,
               int height) {
  return ARGBToI400Matrix(src_abgr, src_stride_abgr, dst_yj, dst_stride_yj,
                          &kAbgrJPEGConstants, width, height);
}

// Convert ARGB to AR64.
LIBYUV_API
int ARGBToAR64(const uint8_t* src_argb,
               int src_stride_argb,
               uint16_t* dst_ar64,
               int dst_stride_ar64,
               int width,
               int height) {
  int y;
  void (*ARGBToAR64Row)(const uint8_t* src_argb, uint16_t* dst_ar64,
                        int width) = ARGBToAR64Row_C;
  if (!src_argb || !dst_ar64 || width <= 0 || height == 0) {
    return -1;
  }
  // Negative height means invert the image.
  if (height < 0) {
    height = -height;
    src_argb = src_argb + (height - 1) * src_stride_argb;
    src_stride_argb = -src_stride_argb;
  }
  // Coalesce rows.
  if (src_stride_argb == width * 4 && dst_stride_ar64 == width * 4 &&
      (ptrdiff_t)width * height <= INT_MAX) {
    width *= height;
    height = 1;
    src_stride_argb = dst_stride_ar64 = 0;
  }
#if defined(HAS_ARGBTOAR64ROW_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3)) {
    ARGBToAR64Row = ARGBToAR64Row_Any_SSSE3;
    if (IS_ALIGNED(width, 4)) {
      ARGBToAR64Row = ARGBToAR64Row_SSSE3;
    }
  }
#endif
#if defined(HAS_ARGBTOAR64ROW_AVX2)
  if (TestCpuFlag(kCpuHasAVX2)) {
    ARGBToAR64Row = ARGBToAR64Row_Any_AVX2;
    if (IS_ALIGNED(width, 8)) {
      ARGBToAR64Row = ARGBToAR64Row_AVX2;
    }
  }
#endif
#if defined(HAS_ARGBTOAR64ROW_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    ARGBToAR64Row = ARGBToAR64Row_Any_NEON;
    if (IS_ALIGNED(width, 8)) {
      ARGBToAR64Row = ARGBToAR64Row_NEON;
    }
  }
#endif
#if defined(HAS_ARGBTOAR64ROW_RVV)
  if (TestCpuFlag(kCpuHasRVV)) {
    ARGBToAR64Row = ARGBToAR64Row_RVV;
  }
#endif

  for (y = 0; y < height; ++y) {
    ARGBToAR64Row(src_argb, dst_ar64, width);
    src_argb += src_stride_argb;
    dst_ar64 += dst_stride_ar64;
  }
  return 0;
}

// Convert ARGB to AB64.
LIBYUV_API
int ARGBToAB64(const uint8_t* src_argb,
               int src_stride_argb,
               uint16_t* dst_ab64,
               int dst_stride_ab64,
               int width,
               int height) {
  int y;
  void (*ARGBToAB64Row)(const uint8_t* src_argb, uint16_t* dst_ar64,
                        int width) = ARGBToAB64Row_C;
  if (!src_argb || !dst_ab64 || width <= 0 || height == 0) {
    return -1;
  }
  // Negative height means invert the image.
  if (height < 0) {
    height = -height;
    src_argb = src_argb + (height - 1) * src_stride_argb;
    src_stride_argb = -src_stride_argb;
  }
  // Coalesce rows.
  if (src_stride_argb == width * 4 && dst_stride_ab64 == width * 4 &&
      (ptrdiff_t)width * height <= INT_MAX) {
    width *= height;
    height = 1;
    src_stride_argb = dst_stride_ab64 = 0;
  }
#if defined(HAS_ARGBTOAB64ROW_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3)) {
    ARGBToAB64Row = ARGBToAB64Row_Any_SSSE3;
    if (IS_ALIGNED(width, 4)) {
      ARGBToAB64Row = ARGBToAB64Row_SSSE3;
    }
  }
#endif
#if defined(HAS_ARGBTOAB64ROW_AVX2)
  if (TestCpuFlag(kCpuHasAVX2)) {
    ARGBToAB64Row = ARGBToAB64Row_Any_AVX2;
    if (IS_ALIGNED(width, 8)) {
      ARGBToAB64Row = ARGBToAB64Row_AVX2;
    }
  }
#endif
#if defined(HAS_ARGBTOAB64ROW_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    ARGBToAB64Row = ARGBToAB64Row_Any_NEON;
    if (IS_ALIGNED(width, 8)) {
      ARGBToAB64Row = ARGBToAB64Row_NEON;
    }
  }
#endif
#if defined(HAS_ARGBTOAB64ROW_RVV)
  if (TestCpuFlag(kCpuHasRVV)) {
    ARGBToAB64Row = ARGBToAB64Row_RVV;
  }
#endif

  for (y = 0; y < height; ++y) {
    ARGBToAB64Row(src_argb, dst_ab64, width);
    src_argb += src_stride_argb;
    dst_ab64 += dst_stride_ab64;
  }
  return 0;
}

// Convert RAW to NV21 with Matrix.
LIBYUV_API
int RAWToNV21Matrix(const uint8_t* src_raw,
                    int src_stride_raw,
                    uint8_t* dst_y,
                    int dst_stride_y,
                    uint8_t* dst_vu,
                    int dst_stride_vu,
                    const struct ArgbConstants* argbconstants,
                    int width,
                    int height) {
  int y;
  int halfwidth = (width + 1) >> 1;
  void (*RAWToARGBRow)(const uint8_t* src_rgb, uint8_t* dst_argb, int width) =
      RAWToARGBRow_C;
  void (*ARGBToUVMatrixRow)(const uint8_t* src_argb0, int src_stride_argb,
                            uint8_t* dst_u, uint8_t* dst_v, int width,
                            const struct ArgbConstants* c) =
      ARGBToUVMatrixRow_C;
  void (*ARGBToYMatrixRow)(const uint8_t* src_argb, uint8_t* dst_y, int width,
                           const struct ArgbConstants* c) = ARGBToYMatrixRow_C;
  void (*MergeUVRow)(const uint8_t* src_uj, const uint8_t* src_vj,
                      uint8_t* dst_vu, int width) = MergeUVRow_C;
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


  if (!src_raw || !dst_y || !dst_vu || !argbconstants || width <= 0 || height == 0) {
    return -1;
  }
  // Negative height means invert the image.
  if (height < 0) {
    height = -height;
    src_raw = src_raw + (height - 1) * src_stride_raw;
    src_stride_raw = -src_stride_raw;
  }

#if defined(HAS_RAWTOARGBROW_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3)) {
    RAWToARGBRow = RAWToARGBRow_Any_SSSE3;
    if (IS_ALIGNED(width, 16)) {
      RAWToARGBRow = RAWToARGBRow_SSSE3;
    }
  }
#endif
#if defined(HAS_RAWTOARGBROW_AVX2)
  if (TestCpuFlag(kCpuHasAVX2)) {
    RAWToARGBRow = RAWToARGBRow_Any_AVX2;
    if (IS_ALIGNED(width, 32)) {
      RAWToARGBRow = RAWToARGBRow_AVX2;
    }
  }
#endif
#if defined(HAS_RAWTOARGBROW_AVX512BW)
  if (TestCpuFlag(kCpuHasAVX512BW)) {
    RAWToARGBRow = RAWToARGBRow_Any_AVX512BW;
    if (IS_ALIGNED(width, 64)) {
      RAWToARGBRow = RAWToARGBRow_AVX512BW;
    }
  }
#endif
#if defined(HAS_RAWTOARGBROW_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    RAWToARGBRow = RAWToARGBRow_Any_NEON;
    if (IS_ALIGNED(width, 8)) {
      RAWToARGBRow = RAWToARGBRow_NEON;
    }
  }
#endif
#if defined(HAS_RAWTOARGBROW_SVE2)
  if (TestCpuFlag(kCpuHasSVE2)) {
    RAWToARGBRow = RAWToARGBRow_SVE2;
  }
#endif
#if defined(HAS_RAWTOARGBROW_LSX)
  if (TestCpuFlag(kCpuHasLSX)) {
    RAWToARGBRow = RAWToARGBRow_Any_LSX;
    if (IS_ALIGNED(width, 16)) {
      RAWToARGBRow = RAWToARGBRow_LSX;
    }
  }
#endif
#if defined(HAS_RAWTOARGBROW_LASX)
  if (TestCpuFlag(kCpuHasLASX)) {
    RAWToARGBRow = RAWToARGBRow_Any_LASX;
    if (IS_ALIGNED(width, 32)) {
      RAWToARGBRow = RAWToARGBRow_LASX;
    }
  }
#endif
#if defined(HAS_RAWTOARGBROW_RVV)
  if (TestCpuFlag(kCpuHasRVV)) {
    RAWToARGBRow = RAWToARGBRow_RVV;
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
    if (TestCpuFlag(kCpuHasNEON) && TestCpuFlag(kCpuHasNeonI8MM)) {
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
    if (IS_ALIGNED(width, 16)) {
      ARGBToUVMatrixRow = ARGBToUVMatrixRow_AVX2;
    }
  }
#endif
#if defined(HAS_ARGBTOUVMATRIXROW_AVX512BW)
  if (TestCpuFlag(kCpuHasAVX512BW)) {
    ARGBToUVMatrixRow = ARGBToUVMatrixRow_Any_AVX512BW;
    if (IS_ALIGNED(width, 32)) {
      ARGBToUVMatrixRow = ARGBToUVMatrixRow_AVX512BW;
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
    if (IS_ALIGNED(halfwidth, 64)) {
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

  {
    // Allocate 2 rows of ARGB.
    const int row_size = (width * 4 + 31) & ~31;
    align_buffer_64(row, row_size * 2);
    // Allocate 1 row of U and 1 row of V.
    align_buffer_64(row_u, halfwidth);
    align_buffer_64(row_v, halfwidth);

    if (!row || !row_u || !row_v) {
      free_aligned_buffer_64(row);
      free_aligned_buffer_64(row_u);
      free_aligned_buffer_64(row_v);
      return 1;
    }

    for (y = 0; y < height - 1; y += 2) {
      RAWToARGBRow(src_raw, row, width);
      RAWToARGBRow(src_raw + src_stride_raw, row + row_size, width);
      ARGBToUVMatrixRow(row, row_size, row_u, row_v, width, argbconstants);
      MergeUVRow(row_v, row_u, dst_vu, halfwidth);
      ARGBToYMatrixRow(row, dst_y, width, argbconstants);
      ARGBToYMatrixRow(row + row_size, dst_y + dst_stride_y, width, argbconstants);
      src_raw += src_stride_raw * 2;
      dst_y += dst_stride_y * 2;
      dst_vu += dst_stride_vu;
    }
    if (height & 1) {
      RAWToARGBRow(src_raw, row, width);
      ARGBToUVMatrixRow(row, 0, row_u, row_v, width, argbconstants);
      MergeUVRow(row_v, row_u, dst_vu, halfwidth);
      ARGBToYMatrixRow(row, dst_y, width, argbconstants);
    }
    free_aligned_buffer_64(row_v);
    free_aligned_buffer_64(row_u);
    free_aligned_buffer_64(row);
  }
  return 0;
}

LIBYUV_API
int RAWToJNV21(const uint8_t* src_raw,
               int src_stride_raw,
               uint8_t* dst_y,
               int dst_stride_y,
               uint8_t* dst_vu,
               int dst_stride_vu,
               int width,
               int height) {
  return RAWToNV21Matrix(src_raw, src_stride_raw, dst_y, dst_stride_y, dst_vu,
                         dst_stride_vu, &kArgbJPEGConstants, width, height);
}

LIBYUV_API
int RAWToNV21(const uint8_t* src_raw,
              int src_stride_raw,
              uint8_t* dst_y,
              int dst_stride_y,
              uint8_t* dst_vu,
              int dst_stride_vu,
              int width,
              int height) {
  return RAWToNV21Matrix(src_raw, src_stride_raw, dst_y, dst_stride_y, dst_vu,
                         dst_stride_vu, &kArgbI601Constants, width, height);
}

LIBYUV_API
int RGB24ToNV12(const uint8_t* src_rgb24,
                int src_stride_rgb24,
                uint8_t* dst_y,
                int dst_stride_y,
                uint8_t* dst_uv,
                int dst_stride_uv,
                int width,
                int height) {
  return RAWToNV21Matrix(src_rgb24, src_stride_rgb24, dst_y, dst_stride_y,
                         dst_uv, dst_stride_uv, &kAbgrI601Constants, width,
                         height);
}


#ifdef __cplusplus
}  // extern "C"
}  // namespace libyuv
#endif
