/*
 *  Copyright 2011 The LibYuv Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS. All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "libyuv/scale.h"

#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "libyuv/cpu_id.h"
#include "libyuv/planar_functions.h"  // For CopyARGB
#include "libyuv/row.h"
#include "libyuv/scale_argb.h"
#include "libyuv/scale_row.h"

#ifdef __cplusplus
namespace libyuv {
extern "C" {
#endif

static __inline int Abs(int v) {
  return v >= 0 ? v : -v;
}

// ScaleARGB ARGB, 1/2
// This is an optimized version for scaling down a ARGB to 1/2 of
// its original size.
static void ScaleARGBDown2(int src_width,
                           int src_height,
                           int dst_width,
                           int dst_height,
                           ptrdiff_t src_stride,
                           ptrdiff_t dst_stride,
                           const uint8_t* src_argb,
                           uint8_t* dst_argb,
                           int x,
                           int dx,
                           int y,
                           int dy,
                           enum FilterMode filtering) {
  int j;
  ptrdiff_t row_stride = src_stride * (dy >> 16);
  void (*ScaleARGBRowDown2)(const uint8_t* src_argb, ptrdiff_t src_stride,
                            uint8_t* dst_argb, int dst_width) =
      filtering == kFilterNone
          ? ScaleARGBRowDown2_C
          : (filtering == kFilterLinear ? ScaleARGBRowDown2Linear_C
                                        : ScaleARGBRowDown2Box_C);
  (void)src_width;
  (void)src_height;
  (void)dx;
  assert(dx == 65536 * 2);      // Test scale factor of 2.
  assert((dy & 0x1ffff) == 0);  // Test vertical scale is multiple of 2.
  // Advance to odd row, even column.
  if (filtering == kFilterBilinear) {
    src_argb += (y >> 16) * src_stride + (x >> 16) * 4;
  } else {
    src_argb += (y >> 16) * src_stride + ((x >> 16) - 1) * 4;
  }

#if defined(HAS_SCALEARGBROWDOWN2_SSE2)
  if (TestCpuFlag(kCpuHasSSE2)) {
    ScaleARGBRowDown2 =
        filtering == kFilterNone
            ? ScaleARGBRowDown2_Any_SSE2
            : (filtering == kFilterLinear ? ScaleARGBRowDown2Linear_Any_SSE2
                                          : ScaleARGBRowDown2Box_Any_SSE2);
    if (IS_ALIGNED(dst_width, 4)) {
      ScaleARGBRowDown2 =
          filtering == kFilterNone
              ? ScaleARGBRowDown2_SSE2
              : (filtering == kFilterLinear ? ScaleARGBRowDown2Linear_SSE2
                                            : ScaleARGBRowDown2Box_SSE2);
    }
  }
#endif
#if defined(HAS_SCALEARGBROWDOWN2_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    ScaleARGBRowDown2 =
        filtering == kFilterNone
            ? ScaleARGBRowDown2_Any_NEON
            : (filtering == kFilterLinear ? ScaleARGBRowDown2Linear_Any_NEON
                                          : ScaleARGBRowDown2Box_Any_NEON);
    if (IS_ALIGNED(dst_width, 8)) {
      ScaleARGBRowDown2 =
          filtering == kFilterNone
              ? ScaleARGBRowDown2_NEON
              : (filtering == kFilterLinear ? ScaleARGBRowDown2Linear_NEON
                                            : ScaleARGBRowDown2Box_NEON);
    }
  }
#endif
#if defined(HAS_SCALEARGBROWDOWN2_SME)
  if (TestCpuFlag(kCpuHasSME)) {
    ScaleARGBRowDown2 = filtering == kFilterNone ? ScaleARGBRowDown2_SME
                        : filtering == kFilterLinear
                            ? ScaleARGBRowDown2Linear_SME
                            : ScaleARGBRowDown2Box_SME;
  }
#endif
#if defined(HAS_SCALEARGBROWDOWN2_LSX)
  if (TestCpuFlag(kCpuHasLSX)) {
    ScaleARGBRowDown2 =
        filtering == kFilterNone
            ? ScaleARGBRowDown2_Any_LSX
            : (filtering == kFilterLinear ? ScaleARGBRowDown2Linear_Any_LSX
                                          : ScaleARGBRowDown2Box_Any_LSX);
    if (IS_ALIGNED(dst_width, 4)) {
      ScaleARGBRowDown2 =
          filtering == kFilterNone
              ? ScaleARGBRowDown2_LSX
              : (filtering == kFilterLinear ? ScaleARGBRowDown2Linear_LSX
                                            : ScaleARGBRowDown2Box_LSX);
    }
  }
#endif
#if defined(HAS_SCALEARGBROWDOWN2_RVV) &&       \
    defined(HAS_SCALEARGBROWDOWN2LINEAR_RVV) && \
    defined(HAS_SCALEARGBROWDOWN2BOX_RVV)
  if (TestCpuFlag(kCpuHasRVV)) {
    ScaleARGBRowDown2 =
        filtering == kFilterNone
            ? ScaleARGBRowDown2_RVV
            : (filtering == kFilterLinear ? ScaleARGBRowDown2Linear_RVV
                                          : ScaleARGBRowDown2Box_RVV);
  }
#endif

  if (filtering == kFilterLinear) {
    src_stride = 0;
  }
  for (j = 0; j < dst_height; ++j) {
    ScaleARGBRowDown2(src_argb, src_stride, dst_argb, dst_width);
    src_argb += row_stride;
    dst_argb += dst_stride;
  }
}

// ScaleARGB ARGB, 1/4
// This is an optimized version for scaling down a ARGB to 1/4 of
// its original size.
static int ScaleARGBDown4Box(int src_width,
                             int src_height,
                             int dst_width,
                             int dst_height,
                             ptrdiff_t src_stride,
                             ptrdiff_t dst_stride,
                             const uint8_t* src_argb,
                             uint8_t* dst_argb,
                             int x,
                             int dx,
                             int y,
                             int dy) {
  int j;
  // Allocate 2 rows of ARGB.
  const int row_size = (dst_width * 2 * 4 + 31) & ~31;
  // TODO(fbarchard): Remove this row buffer and implement a ScaleARGBRowDown4
  // but implemented via a 2 pass wrapper that uses a very small array on the
  // stack with a horizontal loop.
  align_buffer_64(row, row_size * 2);
  if (!row)
    return 1;
  ptrdiff_t row_stride = src_stride * (dy >> 16);
  void (*ScaleARGBRowDown2)(const uint8_t* src_argb, ptrdiff_t src_stride,
                            uint8_t* dst_argb, int dst_width) =
      ScaleARGBRowDown2Box_C;
  // Advance to odd row, even column.
  src_argb += (y >> 16) * src_stride + (x >> 16) * 4;
  (void)src_width;
  (void)src_height;
  (void)dx;
  assert(dx == 65536 * 4);      // Test scale factor of 4.
  assert((dy & 0x3ffff) == 0);  // Test vertical scale is multiple of 4.
#if defined(HAS_SCALEARGBROWDOWN2_SSE2)
  if (TestCpuFlag(kCpuHasSSE2)) {
    ScaleARGBRowDown2 = ScaleARGBRowDown2Box_Any_SSE2;
    if (IS_ALIGNED(dst_width, 4)) {
      ScaleARGBRowDown2 = ScaleARGBRowDown2Box_SSE2;
    }
  }
#endif
#if defined(HAS_SCALEARGBROWDOWN2_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    ScaleARGBRowDown2 = ScaleARGBRowDown2Box_Any_NEON;
    if (IS_ALIGNED(dst_width, 8)) {
      ScaleARGBRowDown2 = ScaleARGBRowDown2Box_NEON;
    }
  }
#endif
#if defined(HAS_SCALEARGBROWDOWN2_SME)
  if (TestCpuFlag(kCpuHasSME)) {
    ScaleARGBRowDown2 = ScaleARGBRowDown2Box_SME;
  }
#endif
#if defined(HAS_SCALEARGBROWDOWN2BOX_RVV)
  if (TestCpuFlag(kCpuHasRVV)) {
    ScaleARGBRowDown2 = ScaleARGBRowDown2Box_RVV;
  }
#endif

  for (j = 0; j < dst_height; ++j) {
    ScaleARGBRowDown2(src_argb, src_stride, row, dst_width * 2);
    ScaleARGBRowDown2(src_argb + src_stride * 2, src_stride, row + row_size,
                      dst_width * 2);
    ScaleARGBRowDown2(row, row_size, dst_argb, dst_width);
    src_argb += row_stride;
    dst_argb += dst_stride;
  }
  free_aligned_buffer_64(row);
  return 0;
}

// ScaleARGB ARGB Even
// This is an optimized version for scaling down a ARGB to even
// multiple of its original size.
static void ScaleARGBDownEven(int src_width,
                              int src_height,
                              int dst_width,
                              int dst_height,
                              ptrdiff_t src_stride,
                              ptrdiff_t dst_stride,
                              const uint8_t* src_argb,
                              uint8_t* dst_argb,
                              int x,
                              int dx,
                              int y,
                              int dy,
                              enum FilterMode filtering) {
  int j;
  int col_step = dx >> 16;
  ptrdiff_t row_stride = (dy >> 16) * src_stride;
  void (*ScaleARGBRowDownEven)(const uint8_t* src_argb, ptrdiff_t src_stride,
                               int src_step, uint8_t* dst_argb, int dst_width) =
      filtering ? ScaleARGBRowDownEvenBox_C : ScaleARGBRowDownEven_C;
  (void)src_width;
  (void)src_height;
  assert(IS_ALIGNED(src_width, 2));
  assert(IS_ALIGNED(src_height, 2));
  src_argb += (y >> 16) * src_stride + (x >> 16) * 4;
#if defined(HAS_SCALEARGBROWDOWNEVEN_SSE2)
  if (TestCpuFlag(kCpuHasSSE2)) {
    ScaleARGBRowDownEven = filtering ? ScaleARGBRowDownEvenBox_Any_SSE2
                                     : ScaleARGBRowDownEven_Any_SSE2;
    if (IS_ALIGNED(dst_width, 4)) {
      ScaleARGBRowDownEven =
          filtering ? ScaleARGBRowDownEvenBox_SSE2 : ScaleARGBRowDownEven_SSE2;
    }
  }
#endif
#if defined(HAS_SCALEARGBROWDOWNEVEN_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    ScaleARGBRowDownEven = filtering ? ScaleARGBRowDownEvenBox_Any_NEON
                                     : ScaleARGBRowDownEven_Any_NEON;
    if (IS_ALIGNED(dst_width, 4)) {
      ScaleARGBRowDownEven =
          filtering ? ScaleARGBRowDownEvenBox_NEON : ScaleARGBRowDownEven_NEON;
    }
  }
#endif
#if defined(HAS_SCALEARGBROWDOWNEVEN_LSX)
  if (TestCpuFlag(kCpuHasLSX)) {
    ScaleARGBRowDownEven = filtering ? ScaleARGBRowDownEvenBox_Any_LSX
                                     : ScaleARGBRowDownEven_Any_LSX;
    if (IS_ALIGNED(dst_width, 4)) {
      ScaleARGBRowDownEven =
          filtering ? ScaleARGBRowDownEvenBox_LSX : ScaleARGBRowDownEven_LSX;
    }
  }
#endif
#if defined(HAS_SCALEARGBROWDOWNEVENBOX_RVV)
  if (filtering && TestCpuFlag(kCpuHasRVV)) {
    ScaleARGBRowDownEven = ScaleARGBRowDownEvenBox_RVV;
  }
#endif
#if defined(HAS_SCALEARGBROWDOWNEVEN_RVV)
  if (!filtering && TestCpuFlag(kCpuHasRVV)) {
    ScaleARGBRowDownEven = ScaleARGBRowDownEven_RVV;
  }
#endif

  if (filtering == kFilterLinear) {
    src_stride = 0;
  }
  for (j = 0; j < dst_height; ++j) {
    ScaleARGBRowDownEven(src_argb, src_stride, col_step, dst_argb, dst_width);
    src_argb += row_stride;
    dst_argb += dst_stride;
  }
}

// Scale ARGB down with bilinear interpolation.
static int ScaleARGBBilinearDown(int src_width,
                                 int src_height,
                                 int dst_width,
                                 int dst_height,
                                 ptrdiff_t src_stride,
                                 ptrdiff_t dst_stride,
                                 const uint8_t* src_argb,
                                 uint8_t* dst_argb,
                                 int x,
                                 int dx,
                                 int y,
                                 int dy,
                                 enum FilterMode filtering) {
  int j;
  void (*InterpolateRow)(uint8_t* dst_argb, const uint8_t* src_argb,
                         ptrdiff_t src_stride, int dst_width,
                         int source_y_fraction) = InterpolateRow_C;
  void (*ScaleARGBFilterCols)(uint8_t* dst_argb, const uint8_t* src_argb,
                              int dst_width, int x, int dx) =
      (src_width >= 32768) ? ScaleARGBFilterCols64_C : ScaleARGBFilterCols_C;
  int64_t xlast = x + (int64_t)(dst_width - 1) * dx;
  int64_t xl = (dx >= 0) ? x : xlast;
  int64_t xr = (dx >= 0) ? xlast : x;
  int clip_src_width;
  xl = (xl >> 16) & ~3;    // Left edge aligned.
  xr = (xr >> 16) + 1;     // Right most pixel used.  Bilinear uses 2 pixels.
  xr = (xr + 1 + 3) & ~3;  // 1 beyond 4 pixel aligned right most pixel.
  if (xr > src_width) {
    xr = src_width;
  }
  clip_src_width = (int)(xr - xl) * 4;  // Width aligned to 4.
  src_argb += xl * 4;
  x -= (int)(xl << 16);
#if defined(HAS_INTERPOLATEROW_AVX2)
  if (TestCpuFlag(kCpuHasAVX2)) {
    InterpolateRow = InterpolateRow_Any_AVX2;
    if (IS_ALIGNED(clip_src_width, 32)) {
      InterpolateRow = InterpolateRow_AVX2;
    }
  }
#endif
#if defined(HAS_INTERPOLATEROW_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    InterpolateRow = InterpolateRow_Any_NEON;
    if (IS_ALIGNED(clip_src_width, 16)) {
      InterpolateRow = InterpolateRow_NEON;
    }
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
    if (IS_ALIGNED(clip_src_width, 32)) {
      InterpolateRow = InterpolateRow_LSX;
    }
  }
#endif
#if defined(HAS_INTERPOLATEROW_RVV)
  if (TestCpuFlag(kCpuHasRVV)) {
    InterpolateRow = InterpolateRow_RVV;
  }
#endif
#if defined(HAS_SCALEARGBFILTERCOLS_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3) && src_width < 32768) {
    ScaleARGBFilterCols = ScaleARGBFilterCols_SSSE3;
  }
#endif
#if defined(HAS_SCALEARGBFILTERCOLS_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    ScaleARGBFilterCols = ScaleARGBFilterCols_Any_NEON;
    if (IS_ALIGNED(dst_width, 4)) {
      ScaleARGBFilterCols = ScaleARGBFilterCols_NEON;
    }
  }
#endif
#if defined(HAS_SCALEARGBFILTERCOLS_LSX)
  if (TestCpuFlag(kCpuHasLSX)) {
    ScaleARGBFilterCols = ScaleARGBFilterCols_Any_LSX;
    if (IS_ALIGNED(dst_width, 8)) {
      ScaleARGBFilterCols = ScaleARGBFilterCols_LSX;
    }
  }
#endif
#if defined(HAS_SCALEARGBFILTERCOLS_RVV)
  if (TestCpuFlag(kCpuHasRVV)) {
    ScaleARGBFilterCols = ScaleARGBFilterCols_RVV;
  }
#endif

  // TODO(fbarchard): Consider not allocating row buffer for kFilterLinear.
  // Allocate a row of ARGB.
  {
    align_buffer_64(row, clip_src_width * 4);
    if (!row)
      return 1;

    const int max_y = (src_height - 1) << 16;
    if (y > max_y) {
      y = max_y;
    }
    for (j = 0; j < dst_height; ++j) {
      int yi = y >> 16;
      const uint8_t* src = src_argb + yi * src_stride;
      if (filtering == kFilterLinear) {
        ScaleARGBFilterCols(dst_argb, src, dst_width, x, dx);
      } else {
        int yf = (y >> 8) & 255;
        InterpolateRow(row, src, src_stride, clip_src_width, yf);
        ScaleARGBFilterCols(dst_argb, row, dst_width, x, dx);
      }
      dst_argb += dst_stride;
      y += dy;
      if (y > max_y) {
        y = max_y;
      }
    }
    free_aligned_buffer_64(row);
  }
  return 0;
}

// Scale ARGB up with bilinear interpolation.
static int ScaleARGBBilinearUp(int src_width,
                               int src_height,
                               int dst_width,
                               int dst_height,
                               ptrdiff_t src_stride,
                               ptrdiff_t dst_stride,
                               const uint8_t* src_argb,
                               uint8_t* dst_argb,
                               int x,
                               int dx,
                               int y,
                               int dy,
                               enum FilterMode filtering) {
  int j;
  void (*InterpolateRow)(uint8_t* dst_argb, const uint8_t* src_argb,
                         ptrdiff_t src_stride, int dst_width,
                         int source_y_fraction) = InterpolateRow_C;
  void (*ScaleARGBFilterCols)(uint8_t* dst_argb, const uint8_t* src_argb,
                              int dst_width, int x, int dx) =
      filtering ? ScaleARGBFilterCols_C : ScaleARGBCols_C;
  const int max_y = (src_height - 1) << 16;
#if defined(HAS_INTERPOLATEROW_AVX2)
  if (TestCpuFlag(kCpuHasAVX2)) {
    InterpolateRow = InterpolateRow_Any_AVX2;
    if (IS_ALIGNED(dst_width, 8)) {
      InterpolateRow = InterpolateRow_AVX2;
    }
  }
#endif
#if defined(HAS_INTERPOLATEROW_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    InterpolateRow = InterpolateRow_Any_NEON;
    if (IS_ALIGNED(dst_width, 4)) {
      InterpolateRow = InterpolateRow_NEON;
    }
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
    if (IS_ALIGNED(dst_width, 8)) {
      InterpolateRow = InterpolateRow_LSX;
    }
  }
#endif
#if defined(HAS_INTERPOLATEROW_RVV)
  if (TestCpuFlag(kCpuHasRVV)) {
    InterpolateRow = InterpolateRow_RVV;
  }
#endif
  if (src_width >= 32768) {
    ScaleARGBFilterCols =
        filtering ? ScaleARGBFilterCols64_C : ScaleARGBCols64_C;
  }
#if defined(HAS_SCALEARGBFILTERCOLS_SSSE3)
  if (filtering && TestCpuFlag(kCpuHasSSSE3) && src_width < 32768) {
    ScaleARGBFilterCols = ScaleARGBFilterCols_SSSE3;
  }
#endif
#if defined(HAS_SCALEARGBFILTERCOLS_NEON)
  if (filtering && TestCpuFlag(kCpuHasNEON)) {
    ScaleARGBFilterCols = ScaleARGBFilterCols_Any_NEON;
    if (IS_ALIGNED(dst_width, 4)) {
      ScaleARGBFilterCols = ScaleARGBFilterCols_NEON;
    }
  }
#endif
#if defined(HAS_SCALEARGBFILTERCOLS_LSX)
  if (filtering && TestCpuFlag(kCpuHasLSX)) {
    ScaleARGBFilterCols = ScaleARGBFilterCols_Any_LSX;
    if (IS_ALIGNED(dst_width, 8)) {
      ScaleARGBFilterCols = ScaleARGBFilterCols_LSX;
    }
  }
#endif
#if defined(HAS_SCALEARGBFILTERCOLS_RVV)
  if (filtering && TestCpuFlag(kCpuHasRVV)) {
    ScaleARGBFilterCols = ScaleARGBFilterCols_RVV;
  }
#endif
#if defined(HAS_SCALEARGBCOLS_SSE2)
  if (!filtering && TestCpuFlag(kCpuHasSSE2) && src_width < 32768) {
    ScaleARGBFilterCols = ScaleARGBCols_SSE2;
  }
#endif
#if defined(HAS_SCALEARGBCOLS_NEON)
  if (!filtering && TestCpuFlag(kCpuHasNEON)) {
    ScaleARGBFilterCols = ScaleARGBCols_Any_NEON;
    if (IS_ALIGNED(dst_width, 8)) {
      ScaleARGBFilterCols = ScaleARGBCols_NEON;
    }
  }
#endif
#if defined(HAS_SCALEARGBCOLS_LSX)
  if (!filtering && TestCpuFlag(kCpuHasLSX)) {
    ScaleARGBFilterCols = ScaleARGBCols_Any_LSX;
    if (IS_ALIGNED(dst_width, 4)) {
      ScaleARGBFilterCols = ScaleARGBCols_LSX;
    }
  }
#endif
  if (!filtering && src_width * 2 == dst_width && x < 0x8000) {
    ScaleARGBFilterCols = ScaleARGBColsUp2_C;
#if defined(HAS_SCALEARGBCOLSUP2_SSE2)
    if (TestCpuFlag(kCpuHasSSE2) && IS_ALIGNED(dst_width, 8)) {
      ScaleARGBFilterCols = ScaleARGBColsUp2_SSE2;
    }
#endif
  }

  if (y > max_y) {
    y = max_y;
  }

  {
    int yi = y >> 16;
    const uint8_t* src = src_argb + yi * src_stride;

    // Allocate 2 rows of ARGB.
    const int row_size = (dst_width * 4 + 31) & ~31;
    align_buffer_64(row, row_size * 2);
    if (!row)
      return 1;

    uint8_t* rowptr = row;
    ptrdiff_t rowstride = row_size;
    int lasty = yi;

    ScaleARGBFilterCols(rowptr, src, dst_width, x, dx);
    if (src_height > 1) {
      src += src_stride;
    }
    ScaleARGBFilterCols(rowptr + rowstride, src, dst_width, x, dx);
    if (src_height > 2) {
      src += src_stride;
    }

    for (j = 0; j < dst_height; ++j) {
      yi = y >> 16;
      if (yi != lasty) {
        if (y > max_y) {
          y = max_y;
          yi = y >> 16;
          src = src_argb + yi * src_stride;
        }
        if (yi != lasty) {
          ScaleARGBFilterCols(rowptr, src, dst_width, x, dx);
          rowptr += rowstride;
          rowstride = -rowstride;
          lasty = yi;
          if ((y + 65536) < max_y) {
            src += src_stride;
          }
        }
      }
      if (filtering == kFilterLinear) {
        InterpolateRow(dst_argb, rowptr, 0, dst_width * 4, 0);
      } else {
        int yf = (y >> 8) & 255;
        InterpolateRow(dst_argb, rowptr, rowstride, dst_width * 4, yf);
      }
      dst_argb += dst_stride;
      y += dy;
    }
    free_aligned_buffer_64(row);
  }
  return 0;
}

// Scale ARGB to/from any dimensions, without interpolation.
// Fixed point math is used for performance: The upper 16 bits
// of x and dx is the integer part of the source position and
// the lower 16 bits are the fixed decimal part.

static void ScaleARGBSimple(int src_width,
                            int src_height,
                            int dst_width,
                            int dst_height,
                            ptrdiff_t src_stride,
                            ptrdiff_t dst_stride,
                            const uint8_t* src_argb,
                            uint8_t* dst_argb,
                            int x,
                            int dx,
                            int y,
                            int dy) {
  int j;
  void (*ScaleARGBCols)(uint8_t* dst_argb, const uint8_t* src_argb,
                        int dst_width, int x, int dx) =
      (src_width >= 32768) ? ScaleARGBCols64_C : ScaleARGBCols_C;
  (void)src_height;
#if defined(HAS_SCALEARGBCOLS_SSE2)
  if (TestCpuFlag(kCpuHasSSE2) && src_width < 32768) {
    ScaleARGBCols = ScaleARGBCols_SSE2;
  }
#endif
#if defined(HAS_SCALEARGBCOLS_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    ScaleARGBCols = ScaleARGBCols_Any_NEON;
    if (IS_ALIGNED(dst_width, 8)) {
      ScaleARGBCols = ScaleARGBCols_NEON;
    }
  }
#endif
#if defined(HAS_SCALEARGBCOLS_LSX)
  if (TestCpuFlag(kCpuHasLSX)) {
    ScaleARGBCols = ScaleARGBCols_Any_LSX;
    if (IS_ALIGNED(dst_width, 4)) {
      ScaleARGBCols = ScaleARGBCols_LSX;
    }
  }
#endif
  if (src_width * 2 == dst_width && x < 0x8000) {
    ScaleARGBCols = ScaleARGBColsUp2_C;
#if defined(HAS_SCALEARGBCOLSUP2_SSE2)
    if (TestCpuFlag(kCpuHasSSE2) && IS_ALIGNED(dst_width, 8)) {
      ScaleARGBCols = ScaleARGBColsUp2_SSE2;
    }
#endif
  }

  for (j = 0; j < dst_height; ++j) {
    ScaleARGBCols(dst_argb, src_argb + (y >> 16) * src_stride, dst_width, x,
                  dx);
    dst_argb += dst_stride;
    y += dy;
  }
}

// ScaleARGB a ARGB.
// This function in turn calls a scaling function
// suitable for handling the desired resolutions.
static int ScaleARGB(const uint8_t* src,
                     int src_stride,
                     int src_width,
                     int src_height,
                     uint8_t* dst,
                     int dst_stride,
                     int dst_width,
                     int dst_height,
                     int clip_x,
                     int clip_y,
                     int clip_width,
                     int clip_height,
                     enum FilterMode filtering) {
  // Initial source x/y coordinate and step values as 16.16 fixed point.
  int x = 0;
  int y = 0;
  int dx = 0;
  int dy = 0;
  // ARGB does not support box filter yet, but allow the user to pass it.
  // Simplify filtering when possible.
  filtering = ScaleFilterReduce(src_width, src_height, dst_width, dst_height,
                                filtering);

  // Negative src_height means invert the image.
  if (src_height < 0) {
    src_height = -src_height;
    src = src + (src_height - 1) * (ptrdiff_t)src_stride;
    src_stride = -src_stride;
  }
  ScaleSlope(src_width, src_height, dst_width, dst_height, filtering, &x, &y,
             &dx, &dy);
  src_width = Abs(src_width);
  if (clip_x) {
    int64_t clipf = (int64_t)(clip_x)*dx;
    x += (clipf & 0xffff);
    src += (clipf >> 16) * 4;
    dst += clip_x * 4;
  }
  if (clip_y) {
    int64_t clipf = (int64_t)(clip_y)*dy;
    y += (clipf & 0xffff);
    src += (clipf >> 16) * (ptrdiff_t)src_stride;
    dst += clip_y * (ptrdiff_t)dst_stride;
  }

  // Special case for integer step values.
  if (((dx | dy) & 0xffff) == 0) {
    if (!dx || !dy) {  // 1 pixel wide and/or tall.
      filtering = kFilterNone;
    } else {
      // Optimized even scale down. ie 2, 4, 6, 8, 10x.
      if (!(dx & 0x10000) && !(dy & 0x10000)) {
        if (dx == 0x20000 && dy == 0x20000) {
          // Optimized 1/2 downsample.
          ScaleARGBDown2(src_width, src_height, clip_width, clip_height,
                         src_stride, dst_stride, src, dst, x, dx, y, dy,
                         filtering);
          return 0;
        }
        if (dx == 0x40000 && dy == 0x40000 && filtering == kFilterBox) {
          // Optimized 1/4 box downsample.
          return ScaleARGBDown4Box(src_width, src_height, clip_width,
                                   clip_height, src_stride, dst_stride, src,
                                   dst, x, dx, y, dy);
        }
        ScaleARGBDownEven(src_width, src_height, clip_width, clip_height,
                          src_stride, dst_stride, src, dst, x, dx, y, dy,
                          filtering);
        return 0;
      }
      // Optimized odd scale down. ie 3, 5, 7, 9x.
      if ((dx & 0x10000) && (dy & 0x10000)) {
        filtering = kFilterNone;
        if (dx == 0x10000 && dy == 0x10000) {
          // Straight copy.
          ARGBCopy(src + (y >> 16) * (ptrdiff_t)src_stride + (x >> 16) * 4,
                   src_stride, dst, dst_stride, clip_width, clip_height);
          return 0;
        }
      }
    }
  }
  if (dx == 0x10000 && (x & 0xffff) == 0) {
    // Arbitrary scale vertically, but unscaled horizontally.
    ScalePlaneVertical(src_height, clip_width, clip_height, src_stride,
                       dst_stride, src, dst, x, y, dy, /*bpp=*/4, filtering);
    return 0;
  }
  if (filtering && dy < 65536) {
    return ScaleARGBBilinearUp(src_width, src_height, clip_width, clip_height,
                               src_stride, dst_stride, src, dst, x, dx, y, dy,
                               filtering);
  }
  if (filtering) {
    return ScaleARGBBilinearDown(src_width, src_height, clip_width, clip_height,
                                 src_stride, dst_stride, src, dst, x, dx, y, dy,
                                 filtering);
  }
  ScaleARGBSimple(src_width, src_height, clip_width, clip_height, src_stride,
                  dst_stride, src, dst, x, dx, y, dy);
  return 0;
}

LIBYUV_API
int ARGBScaleClip(const uint8_t* src_argb,
                  int src_stride_argb,
                  int src_width,
                  int src_height,
                  uint8_t* dst_argb,
                  int dst_stride_argb,
                  int dst_width,
                  int dst_height,
                  int clip_x,
                  int clip_y,
                  int clip_width,
                  int clip_height,
                  enum FilterMode filtering) {
  if (!src_argb || src_width == 0 || src_height == 0 || !dst_argb ||
      dst_width <= 0 || dst_height <= 0 || clip_x < 0 || clip_y < 0 ||
      clip_width > 32768 || clip_height > 32768 ||
      (clip_x + clip_width) > dst_width ||
      (clip_y + clip_height) > dst_height) {
    return -1;
  }
  return ScaleARGB(src_argb, src_stride_argb, src_width, src_height, dst_argb,
                   dst_stride_argb, dst_width, dst_height, clip_x, clip_y,
                   clip_width, clip_height, filtering);
}

// Scale an ARGB image.
LIBYUV_API
int ARGBScale(const uint8_t* src_argb,
              int src_stride_argb,
              int src_width,
              int src_height,
              uint8_t* dst_argb,
              int dst_stride_argb,
              int dst_width,
              int dst_height,
              enum FilterMode filtering) {
  if (!src_argb || src_width == 0 || src_height == 0 || src_width > 32768 ||
      src_height > 32768 || !dst_argb || dst_width <= 0 || dst_height <= 0) {
    return -1;
  }
  return ScaleARGB(src_argb, src_stride_argb, src_width, src_height, dst_argb,
                   dst_stride_argb, dst_width, dst_height, 0, 0, dst_width,
                   dst_height, filtering);
}

// Scale with YUV conversion to ARGB and clipping.
LIBYUV_API
int YUVToARGBScaleClip(const uint8_t* src_y,
                       int src_stride_y,
                       const uint8_t* src_u,
                       int src_stride_u,
                       const uint8_t* src_v,
                       int src_stride_v,
                       uint32_t src_fourcc,
                       int src_width,
                       int src_height,
                       uint8_t* dst_argb,
                       int dst_stride_argb,
                       uint32_t dst_fourcc,
                       int dst_width,
                       int dst_height,
                       int clip_x,
                       int clip_y,
                       int clip_width,
                       int clip_height,
                       enum FilterMode filtering) {
  int r;
  (void)src_fourcc;  // TODO(fbarchard): implement and/or assert.
  (void)dst_fourcc;
  const int abs_src_height = (src_height < 0) ? -src_height : src_height;
  if (!src_y || !src_u || !src_v || !dst_argb || src_width <= 0 ||
      src_width > INT_MAX / 4 || src_height == 0 || dst_width <= 0 ||
      dst_height <= 0 || clip_width <= 0 || clip_height <= 0) {
    return -1;
  }
  const uint64_t argb_buffer_size = (uint64_t)src_width * abs_src_height * 4;
  if (argb_buffer_size > SIZE_MAX) {
    return -1;  // Invalid size.
  }
  uint8_t* argb_buffer = (uint8_t*)malloc((size_t)argb_buffer_size);
  if (!argb_buffer) {
    return 1;  // Out of memory runtime error.
  }
  I420ToARGB(src_y, src_stride_y, src_u, src_stride_u, src_v, src_stride_v,
             argb_buffer, src_width * 4, src_width, src_height);

  r = ARGBScaleClip(argb_buffer, src_width * 4, src_width, abs_src_height,
                    dst_argb, dst_stride_argb, dst_width, dst_height, clip_x,
                    clip_y, clip_width, clip_height, filtering);
  free(argb_buffer);
  return r;
}

#ifdef __cplusplus
}  // extern "C"
}  // namespace libyuv
#endif
