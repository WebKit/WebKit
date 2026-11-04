/*
 *  Copyright 2011 The LibYuv Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS. All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "libyuv/convert_argb.h"

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "libyuv/cpu_id.h"
#ifdef HAVE_JPEG
#include "libyuv/mjpeg_decoder.h"
#endif
#include "libyuv/rotate_argb.h"
#include "libyuv/row.h"
#include "libyuv/video_common.h"

#ifdef __cplusplus
namespace libyuv {
extern "C" {
#endif

static int NV16ToARGB(const uint8_t* src_y,
                      int src_stride_y,
                      const uint8_t* src_uv,
                      int src_stride_uv,
                      uint8_t* dst_argb,
                      int dst_stride_argb,
                      int width,
                      int height) {
  int y;
  void (*NV12ToARGBRow)(
      const uint8_t* y_buf, const uint8_t* uv_buf, uint8_t* rgb_buf,
      const struct YuvConstants* yuvconstants, int width) = NV12ToARGBRow_C;
  if (!src_y || !src_uv || !dst_argb || width <= 0 || height == 0 ||
      height == INT_MIN) {
    return -1;
  }
  if (height < 0) {
    height = -height;
    dst_argb = dst_argb + (ptrdiff_t)(height - 1) * dst_stride_argb;
    dst_stride_argb = -dst_stride_argb;
  }
#if defined(HAS_NV12TOARGBROW_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3)) {
    NV12ToARGBRow = NV12ToARGBRow_Any_SSSE3;
    if (IS_ALIGNED(width, 8)) {
      NV12ToARGBRow = NV12ToARGBRow_SSSE3;
    }
  }
#endif
#if defined(HAS_NV12TOARGBROW_AVX2)
  if (TestCpuFlag(kCpuHasAVX2)) {
    NV12ToARGBRow = NV12ToARGBRow_Any_AVX2;
    if (IS_ALIGNED(width, 16)) {
      NV12ToARGBRow = NV12ToARGBRow_AVX2;
    }
  }
#endif
#if defined(HAS_NV12TOARGBROW_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    NV12ToARGBRow = NV12ToARGBRow_Any_NEON;
    if (IS_ALIGNED(width, 8)) {
      NV12ToARGBRow = NV12ToARGBRow_NEON;
    }
  }
#endif
#if defined(HAS_NV12TOARGBROW_SVE2)
  if (TestCpuFlag(kCpuHasSVE2)) {
    NV12ToARGBRow = NV12ToARGBRow_SVE2;
  }
#endif
#if defined(HAS_NV12TOARGBROW_SME)
  if (TestCpuFlag(kCpuHasSME)) {
    NV12ToARGBRow = NV12ToARGBRow_SME;
  }
#endif
#if defined(HAS_NV12TOARGBROW_LSX)
  if (TestCpuFlag(kCpuHasLSX)) {
    NV12ToARGBRow = NV12ToARGBRow_Any_LSX;
    if (IS_ALIGNED(width, 8)) {
      NV12ToARGBRow = NV12ToARGBRow_LSX;
    }
  }
#endif
#if defined(HAS_NV12TOARGBROW_LASX)
  if (TestCpuFlag(kCpuHasLASX)) {
    NV12ToARGBRow = NV12ToARGBRow_Any_LASX;
    if (IS_ALIGNED(width, 16)) {
      NV12ToARGBRow = NV12ToARGBRow_LASX;
    }
  }
#endif
#if defined(HAS_NV12TOARGBROW_RVV)
  if (TestCpuFlag(kCpuHasRVV)) {
    NV12ToARGBRow = NV12ToARGBRow_RVV;
  }
#endif
#if defined(HAS_NV12TOARGBROW_WASMSIMD)
  if (TestCpuFlag(kCpuHasWASMSIMD)) {
    NV12ToARGBRow = NV12ToARGBRow_Any_WASMSIMD;
    if (IS_ALIGNED(width, 8)) {
      NV12ToARGBRow = NV12ToARGBRow_WASMSIMD;
    }
  }
#endif

  for (y = 0; y < height; ++y) {
    NV12ToARGBRow(src_y, src_uv, dst_argb, &kYuvI601Constants, width);
    dst_argb += dst_stride_argb;
    src_y += src_stride_y;
    src_uv += src_stride_uv;
  }
  return 0;
}

static int NV24ToARGB(const uint8_t* src_y,
                      int src_stride_y,
                      const uint8_t* src_uv,
                      int src_stride_uv,
                      uint8_t* dst_argb,
                      int dst_stride_argb,
                      int width,
                      int height) {
  int y;
  void (*I444ToARGBRow)(const uint8_t* y_buf, const uint8_t* u_buf,
                        const uint8_t* v_buf, uint8_t* rgb_buf,
                        const struct YuvConstants* yuvconstants, int width) =
      I444ToARGBRow_C;
  void (*SplitUVRow)(const uint8_t* src_uv, uint8_t* dst_u, uint8_t* dst_v,
                     int width) = SplitUVRow_C;
  if (!src_y || !src_uv || !dst_argb || width <= 0 || height == 0 ||
      height == INT_MIN) {
    return -1;
  }
  if (height < 0) {
    height = -height;
    dst_argb = dst_argb + (ptrdiff_t)(height - 1) * dst_stride_argb;
    dst_stride_argb = -dst_stride_argb;
  }
#if defined(HAS_I444TOARGBROW_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3)) {
    I444ToARGBRow = I444ToARGBRow_Any_SSSE3;
    if (IS_ALIGNED(width, 8)) {
      I444ToARGBRow = I444ToARGBRow_SSSE3;
    }
  }
#endif
#if defined(HAS_I444TOARGBROW_AVX2)
  if (TestCpuFlag(kCpuHasAVX2)) {
    I444ToARGBRow = I444ToARGBRow_Any_AVX2;
    if (IS_ALIGNED(width, 16)) {
      I444ToARGBRow = I444ToARGBRow_AVX2;
    }
  }
#endif
#if defined(HAS_I444TOARGBROW_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    I444ToARGBRow = I444ToARGBRow_Any_NEON;
    if (IS_ALIGNED(width, 8)) {
      I444ToARGBRow = I444ToARGBRow_NEON;
    }
  }
#endif
#if defined(HAS_I444TOARGBROW_SVE2)
  if (TestCpuFlag(kCpuHasSVE2)) {
    I444ToARGBRow = I444ToARGBRow_SVE2;
  }
#endif
#if defined(HAS_I444TOARGBROW_SME)
  if (TestCpuFlag(kCpuHasSME)) {
    I444ToARGBRow = I444ToARGBRow_SME;
  }
#endif
#if defined(HAS_I444TOARGBROW_LSX)
  if (TestCpuFlag(kCpuHasLSX)) {
    I444ToARGBRow = I444ToARGBRow_Any_LSX;
    if (IS_ALIGNED(width, 8)) {
      I444ToARGBRow = I444ToARGBRow_LSX;
    }
  }
#endif
#if defined(HAS_I444TOARGBROW_LASX)
  if (TestCpuFlag(kCpuHasLASX)) {
    I444ToARGBRow = I444ToARGBRow_Any_LASX;
    if (IS_ALIGNED(width, 16)) {
      I444ToARGBRow = I444ToARGBRow_LASX;
    }
  }
#endif
#if defined(HAS_I444TOARGBROW_RVV)
  if (TestCpuFlag(kCpuHasRVV)) {
    I444ToARGBRow = I444ToARGBRow_RVV;
  }
#endif

#if defined(HAS_SPLITUVROW_SSE2)
  if (TestCpuFlag(kCpuHasSSE2)) {
    SplitUVRow = SplitUVRow_Any_SSE2;
    if (IS_ALIGNED(width, 16)) {
      SplitUVRow = SplitUVRow_SSE2;
    }
  }
#endif
#if defined(HAS_SPLITUVROW_AVX2)
  if (TestCpuFlag(kCpuHasAVX2)) {
    SplitUVRow = SplitUVRow_Any_AVX2;
    if (IS_ALIGNED(width, 32)) {
      SplitUVRow = SplitUVRow_AVX2;
    }
  }
#endif
#if defined(HAS_SPLITUVROW_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    SplitUVRow = SplitUVRow_Any_NEON;
    if (IS_ALIGNED(width, 16)) {
      SplitUVRow = SplitUVRow_NEON;
    }
  }
#endif
#if defined(HAS_SPLITUVROW_RVV)
  if (TestCpuFlag(kCpuHasRVV)) {
    SplitUVRow = SplitUVRow_RVV;
  }
#endif

  align_buffer_64(row_u, width);
  align_buffer_64(row_v, width);
  if (!row_u || !row_v) {
    free_aligned_buffer_64(row_u);
    free_aligned_buffer_64(row_v);
    return 1;
  }
  for (y = 0; y < height; ++y) {
    SplitUVRow(src_uv, row_u, row_v, width);
    I444ToARGBRow(src_y, row_u, row_v, dst_argb, &kYuvI601Constants, width);
    dst_argb += dst_stride_argb;
    src_y += src_stride_y;
    src_uv += src_stride_uv;
  }
  free_aligned_buffer_64(row_u);
  free_aligned_buffer_64(row_v);
  return 0;
}

// Convert camera sample to ARGB with cropping, rotation and vertical flip.
// src_width is used for source stride computation
// src_height is used to compute location of planes, and indicate inversion
// sample_size is measured in bytes and is the size of the frame.
//   With MJPEG it is the compressed size of the frame.

// TODO(fbarchard): Add the following:
// H010ToARGB
// I010ToARGB

LIBYUV_API
int ConvertToARGB(const uint8_t* sample,
                  size_t sample_size,
                  uint8_t* dst_argb,
                  int dst_stride_argb,
                  int crop_x,
                  int crop_y,
                  int src_width,
                  int src_height,
                  int crop_width,
                  int crop_height,
                  enum RotationMode rotation,
                  uint32_t fourcc) {
  if (src_height == INT_MIN || crop_height == INT_MIN) {
    return -1;
  }

  int abs_src_height = (src_height < 0) ? -src_height : src_height;
  int abs_crop_height = (crop_height < 0) ? -crop_height : crop_height;

  if (dst_argb == NULL || sample == NULL || src_width <= 0 ||
      src_width > INT_MAX / 4 || crop_width <= 0 || crop_width > INT_MAX / 4 ||
      src_height == 0 || crop_height == 0 || crop_x < 0 || crop_y < 0 ||
      crop_width > src_width || crop_x > src_width - crop_width ||
      abs_crop_height > abs_src_height ||
      crop_y > abs_src_height - abs_crop_height) {
    return -1;
  }

  uint32_t format = CanonicalFourCC(fourcc);
  int aligned_src_width = (src_width + 1) & ~1;
  const uint8_t* src;
  const uint8_t* src_uv;
  int r = 0;

  // One pass rotation is available for some formats. For the rest, convert
  // to ARGB (with optional vertical flipping) into a temporary ARGB buffer,
  // and then rotate the ARGB to the final destination buffer.
  // For in-place conversion, if destination dst_argb is same as source sample,
  // also enable temporary buffer.
  int need_buf = (rotation && format != FOURCC_ARGB) || dst_argb == sample;
  uint8_t* dest_argb = dst_argb;
  int dest_dst_stride_argb = dst_stride_argb;
  uint8_t* rotate_buffer = NULL;
  int inv_crop_height = (crop_height < 0) ? -crop_height : crop_height;

  if (src_height < 0) {
    inv_crop_height = -inv_crop_height;
  }

  if (need_buf) {
    const uint64_t rotate_buffer_size =
        (uint64_t)crop_width * 4 * abs_crop_height;
#if UINT64_MAX > SIZE_MAX
    if (rotate_buffer_size > SIZE_MAX) {
      return -1;  // Invalid size.
    }
#endif
    rotate_buffer = (uint8_t*)malloc((size_t)rotate_buffer_size);
    if (!rotate_buffer) {
      return 1;  // Out of memory runtime error.
    }
    dst_argb = rotate_buffer;
    dst_stride_argb = crop_width * 4;
  }

  switch (format) {
    // Single plane formats
    case FOURCC_YUY2:
      src = sample + ((ptrdiff_t)aligned_src_width * crop_y + crop_x) * 2;
      r = YUY2ToARGB(src, aligned_src_width * 2, dst_argb, dst_stride_argb,
                     crop_width, inv_crop_height);
      break;
    case FOURCC_UYVY:
      src = sample + ((ptrdiff_t)aligned_src_width * crop_y + crop_x) * 2;
      r = UYVYToARGB(src, aligned_src_width * 2, dst_argb, dst_stride_argb,
                     crop_width, inv_crop_height);
      break;
    case FOURCC_24BG:
      src = sample + ((ptrdiff_t)src_width * crop_y + crop_x) * 3;
      r = RGB24ToARGB(src, src_width * 3, dst_argb, dst_stride_argb, crop_width,
                      inv_crop_height);
      break;
    case FOURCC_RAW:
      src = sample + ((ptrdiff_t)src_width * crop_y + crop_x) * 3;
      r = RAWToARGB(src, src_width * 3, dst_argb, dst_stride_argb, crop_width,
                    inv_crop_height);
      break;
    case FOURCC_ARGB:
      if (!need_buf && !rotation) {
        src = sample + ((ptrdiff_t)src_width * crop_y + crop_x) * 4;
        r = ARGBToARGB(src, src_width * 4, dst_argb, dst_stride_argb,
                       crop_width, inv_crop_height);
      }
      break;
    case FOURCC_BGRA:
      src = sample + ((ptrdiff_t)src_width * crop_y + crop_x) * 4;
      r = BGRAToARGB(src, src_width * 4, dst_argb, dst_stride_argb, crop_width,
                     inv_crop_height);
      break;
    case FOURCC_ABGR:
      src = sample + ((ptrdiff_t)src_width * crop_y + crop_x) * 4;
      r = ABGRToARGB(src, src_width * 4, dst_argb, dst_stride_argb, crop_width,
                     inv_crop_height);
      break;
    case FOURCC_RGBA:
      src = sample + ((ptrdiff_t)src_width * crop_y + crop_x) * 4;
      r = RGBAToARGB(src, src_width * 4, dst_argb, dst_stride_argb, crop_width,
                     inv_crop_height);
      break;
    case FOURCC_AR30:
      src = sample + ((ptrdiff_t)src_width * crop_y + crop_x) * 4;
      r = AR30ToARGB(src, src_width * 4, dst_argb, dst_stride_argb, crop_width,
                     inv_crop_height);
      break;
    case FOURCC_AB30:
      src = sample + ((ptrdiff_t)src_width * crop_y + crop_x) * 4;
      r = AB30ToARGB(src, src_width * 4, dst_argb, dst_stride_argb, crop_width,
                     inv_crop_height);
      break;
    case FOURCC_RGBP:
      src = sample + ((ptrdiff_t)src_width * crop_y + crop_x) * 2;
      r = RGB565ToARGB(src, src_width * 2, dst_argb, dst_stride_argb,
                       crop_width, inv_crop_height);
      break;
    case FOURCC_RGBO:
      src = sample + ((ptrdiff_t)src_width * crop_y + crop_x) * 2;
      r = ARGB1555ToARGB(src, src_width * 2, dst_argb, dst_stride_argb,
                         crop_width, inv_crop_height);
      break;
    case FOURCC_R444:
      src = sample + ((ptrdiff_t)src_width * crop_y + crop_x) * 2;
      r = ARGB4444ToARGB(src, src_width * 2, dst_argb, dst_stride_argb,
                         crop_width, inv_crop_height);
      break;
    case FOURCC_I400:
      src = sample + (ptrdiff_t)src_width * crop_y + crop_x;
      r = I400ToARGB(src, src_width, dst_argb, dst_stride_argb, crop_width,
                     inv_crop_height);
      break;
    case FOURCC_J400:
      src = sample + (ptrdiff_t)src_width * crop_y + crop_x;
      r = J400ToARGB(src, src_width, dst_argb, dst_stride_argb, crop_width,
                     inv_crop_height);
      break;

    // Biplanar formats
    case FOURCC_NV12:
      src = sample + ((ptrdiff_t)src_width * crop_y + crop_x);
      src_uv = sample +
               aligned_src_width * ((ptrdiff_t)abs_src_height + crop_y / 2) +
               crop_x;
      r = NV12ToARGB(src, src_width, src_uv, aligned_src_width, dst_argb,
                     dst_stride_argb, crop_width, inv_crop_height);
      break;
    case FOURCC_NV21:
      src = sample + ((ptrdiff_t)src_width * crop_y + crop_x);
      src_uv = sample +
               aligned_src_width * ((ptrdiff_t)abs_src_height + crop_y / 2) +
               crop_x;
      // Call NV12 but with u and v parameters swapped.
      r = NV21ToARGB(src, src_width, src_uv, aligned_src_width, dst_argb,
                     dst_stride_argb, crop_width, inv_crop_height);
      break;
    case FOURCC_NV16:
      src = sample + ((ptrdiff_t)src_width * crop_y + crop_x);
      src_uv = sample +
               aligned_src_width * ((ptrdiff_t)abs_src_height + crop_y) +
               (crop_x & ~1);
      r = NV16ToARGB(src, src_width, src_uv, aligned_src_width, dst_argb,
                     dst_stride_argb, crop_width, inv_crop_height);
      break;
    case FOURCC_NV24:
      src = sample + ((ptrdiff_t)src_width * crop_y + crop_x);
      src_uv = sample + (ptrdiff_t)aligned_src_width * abs_src_height +
               (ptrdiff_t)aligned_src_width * 2 * crop_y +
               (ptrdiff_t)crop_x * 2;
      r = NV24ToARGB(src, src_width, src_uv, aligned_src_width * 2, dst_argb,
                     dst_stride_argb, crop_width, inv_crop_height);
      break;
    case FOURCC_P010: {
      const uint16_t* src16 = (const uint16_t*)sample;
      const uint16_t* src_y16 =
          src16 + ((ptrdiff_t)src_width * crop_y + crop_x);
      const uint16_t* src_uv16 =
          src16 + ((ptrdiff_t)src_width * abs_src_height) +
          ((ptrdiff_t)(crop_y / 2) * aligned_src_width) + ((crop_x / 2) * 2);
      r = P010ToARGBMatrix(src_y16, src_width, src_uv16, aligned_src_width,
                           dst_argb, dst_stride_argb, &kYuvI601Constants,
                           crop_width, inv_crop_height);
      break;
    }
    case FOURCC_P210: {
      const uint16_t* src16 = (const uint16_t*)sample;
      const uint16_t* src_y16 =
          src16 + ((ptrdiff_t)src_width * crop_y + crop_x);
      const uint16_t* src_uv16 =
          src16 + ((ptrdiff_t)src_width * abs_src_height) +
          ((ptrdiff_t)crop_y * aligned_src_width) + ((crop_x / 2) * 2);
      r = P210ToARGBMatrix(src_y16, src_width, src_uv16, aligned_src_width,
                           dst_argb, dst_stride_argb, &kYuvI601Constants,
                           crop_width, inv_crop_height);
      break;
    }
    case FOURCC_P410: {
      const uint16_t* src16 = (const uint16_t*)sample;
      const uint16_t* src_y16 =
          src16 + ((ptrdiff_t)src_width * crop_y + crop_x);
      const uint16_t* src_uv16 =
          src16 + ((ptrdiff_t)src_width * abs_src_height) +
          ((ptrdiff_t)crop_y * src_width * 2) + crop_x * 2;
      r = P410ToARGBMatrix(src_y16, src_width, src_uv16, src_width * 2,
                           dst_argb, dst_stride_argb, &kYuvI601Constants,
                           crop_width, inv_crop_height);
      break;
    }
    // Triplanar formats
    case FOURCC_I420:
    case FOURCC_YV12: {
      const uint8_t* src_y = sample + ((ptrdiff_t)src_width * crop_y + crop_x);
      const uint8_t* src_u;
      const uint8_t* src_v;
      int halfwidth = (src_width + 1) / 2;
      int halfheight = (abs_src_height + 1) / 2;
      if (format == FOURCC_YV12) {
        src_v = sample + (ptrdiff_t)src_width * abs_src_height +
                ((ptrdiff_t)halfwidth * crop_y + crop_x) / 2;
        src_u = sample + (ptrdiff_t)src_width * abs_src_height +
                halfwidth * ((ptrdiff_t)halfheight + crop_y / 2) + crop_x / 2;
      } else {
        src_u = sample + (ptrdiff_t)src_width * abs_src_height +
                ((ptrdiff_t)halfwidth * crop_y + crop_x) / 2;
        src_v = sample + (ptrdiff_t)src_width * abs_src_height +
                halfwidth * ((ptrdiff_t)halfheight + crop_y / 2) + crop_x / 2;
      }
      r = I420ToARGB(src_y, src_width, src_u, halfwidth, src_v, halfwidth,
                     dst_argb, dst_stride_argb, crop_width, inv_crop_height);
      break;
    }

    case FOURCC_J420: {
      int halfwidth = (src_width + 1) / 2;
      int halfheight = (abs_src_height + 1) / 2;
      const uint8_t* src_y = sample + ((ptrdiff_t)src_width * crop_y + crop_x);
      const uint8_t* src_u = sample + (ptrdiff_t)src_width * abs_src_height +
                             ((ptrdiff_t)halfwidth * crop_y + crop_x) / 2;
      const uint8_t* src_v = sample + (ptrdiff_t)src_width * abs_src_height +
                             halfwidth * ((ptrdiff_t)halfheight + crop_y / 2) +
                             crop_x / 2;
      r = J420ToARGB(src_y, src_width, src_u, halfwidth, src_v, halfwidth,
                     dst_argb, dst_stride_argb, crop_width, inv_crop_height);
      break;
    }

    case FOURCC_H420: {
      int halfwidth = (src_width + 1) / 2;
      int halfheight = (abs_src_height + 1) / 2;
      const uint8_t* src_y = sample + ((ptrdiff_t)src_width * crop_y + crop_x);
      const uint8_t* src_u = sample + (ptrdiff_t)src_width * abs_src_height +
                             ((ptrdiff_t)halfwidth * crop_y + crop_x) / 2;
      const uint8_t* src_v = sample + (ptrdiff_t)src_width * abs_src_height +
                             halfwidth * ((ptrdiff_t)halfheight + crop_y / 2) +
                             crop_x / 2;
      r = H420ToARGB(src_y, src_width, src_u, halfwidth, src_v, halfwidth,
                     dst_argb, dst_stride_argb, crop_width, inv_crop_height);
      break;
    }

    case FOURCC_U420: {
      int halfwidth = (src_width + 1) / 2;
      int halfheight = (abs_src_height + 1) / 2;
      const uint8_t* src_y = sample + ((ptrdiff_t)src_width * crop_y + crop_x);
      const uint8_t* src_u = sample + (ptrdiff_t)src_width * abs_src_height +
                             ((ptrdiff_t)halfwidth * crop_y + crop_x) / 2;
      const uint8_t* src_v = sample + (ptrdiff_t)src_width * abs_src_height +
                             halfwidth * ((ptrdiff_t)halfheight + crop_y / 2) +
                             crop_x / 2;
      r = U420ToARGB(src_y, src_width, src_u, halfwidth, src_v, halfwidth,
                     dst_argb, dst_stride_argb, crop_width, inv_crop_height);
      break;
    }

    case FOURCC_I422:
    case FOURCC_YV16: {
      int halfwidth = (src_width + 1) / 2;
      const uint8_t* src_y = sample + (ptrdiff_t)src_width * crop_y + crop_x;
      const uint8_t* src_u;
      const uint8_t* src_v;
      if (format == FOURCC_YV16) {
        src_v = sample + (ptrdiff_t)src_width * abs_src_height +
                (ptrdiff_t)halfwidth * crop_y + crop_x / 2;
        src_u = sample + (ptrdiff_t)src_width * abs_src_height +
                halfwidth * ((ptrdiff_t)abs_src_height + crop_y) + crop_x / 2;
      } else {
        src_u = sample + (ptrdiff_t)src_width * abs_src_height +
                (ptrdiff_t)halfwidth * crop_y + crop_x / 2;
        src_v = sample + (ptrdiff_t)src_width * abs_src_height +
                halfwidth * ((ptrdiff_t)abs_src_height + crop_y) + crop_x / 2;
      }
      r = I422ToARGB(src_y, src_width, src_u, halfwidth, src_v, halfwidth,
                     dst_argb, dst_stride_argb, crop_width, inv_crop_height);
      break;
    }

    case FOURCC_J422: {
      int halfwidth = (src_width + 1) / 2;
      const uint8_t* src_y = sample + (ptrdiff_t)src_width * crop_y + crop_x;
      const uint8_t* src_u = sample + (ptrdiff_t)src_width * abs_src_height +
                             (ptrdiff_t)halfwidth * crop_y + crop_x / 2;
      const uint8_t* src_v = sample + (ptrdiff_t)src_width * abs_src_height +
                             halfwidth * ((ptrdiff_t)abs_src_height + crop_y) +
                             crop_x / 2;
      r = J422ToARGB(src_y, src_width, src_u, halfwidth, src_v, halfwidth,
                     dst_argb, dst_stride_argb, crop_width, inv_crop_height);
      break;
    }

    case FOURCC_H422: {
      int halfwidth = (src_width + 1) / 2;
      const uint8_t* src_y = sample + (ptrdiff_t)src_width * crop_y + crop_x;
      const uint8_t* src_u = sample + (ptrdiff_t)src_width * abs_src_height +
                             (ptrdiff_t)halfwidth * crop_y + crop_x / 2;
      const uint8_t* src_v = sample + (ptrdiff_t)src_width * abs_src_height +
                             halfwidth * ((ptrdiff_t)abs_src_height + crop_y) +
                             crop_x / 2;
      r = H422ToARGB(src_y, src_width, src_u, halfwidth, src_v, halfwidth,
                     dst_argb, dst_stride_argb, crop_width, inv_crop_height);
      break;
    }

    case FOURCC_U422: {
      int halfwidth = (src_width + 1) / 2;
      const uint8_t* src_y = sample + (ptrdiff_t)src_width * crop_y + crop_x;
      const uint8_t* src_u = sample + (ptrdiff_t)src_width * abs_src_height +
                             (ptrdiff_t)halfwidth * crop_y + crop_x / 2;
      const uint8_t* src_v = sample + (ptrdiff_t)src_width * abs_src_height +
                             halfwidth * ((ptrdiff_t)abs_src_height + crop_y) +
                             crop_x / 2;
      r = H422ToARGB(src_y, src_width, src_u, halfwidth, src_v, halfwidth,
                     dst_argb, dst_stride_argb, crop_width, inv_crop_height);
      break;
    }

    case FOURCC_I444:
    case FOURCC_YV24: {
      const uint8_t* src_y = sample + (ptrdiff_t)src_width * crop_y + crop_x;
      const uint8_t* src_u;
      const uint8_t* src_v;
      if (format == FOURCC_YV24) {
        src_v =
            sample + src_width * ((ptrdiff_t)abs_src_height + crop_y) + crop_x;
        src_u = sample + src_width * ((ptrdiff_t)abs_src_height * 2 + crop_y) +
                crop_x;
      } else {
        src_u =
            sample + src_width * ((ptrdiff_t)abs_src_height + crop_y) + crop_x;
        src_v = sample + src_width * ((ptrdiff_t)abs_src_height * 2 + crop_y) +
                crop_x;
      }
      r = I444ToARGB(src_y, src_width, src_u, src_width, src_v, src_width,
                     dst_argb, dst_stride_argb, crop_width, inv_crop_height);
      break;
    }

    case FOURCC_J444: {
      const uint8_t* src_y = sample + (ptrdiff_t)src_width * crop_y + crop_x;
      const uint8_t* src_u =
          sample + src_width * ((ptrdiff_t)abs_src_height + crop_y) + crop_x;
      const uint8_t* src_v =
          sample + src_width * ((ptrdiff_t)abs_src_height * 2 + crop_y) +
          crop_x;
      r = J444ToARGB(src_y, src_width, src_u, src_width, src_v, src_width,
                     dst_argb, dst_stride_argb, crop_width, inv_crop_height);
      break;
    }

    case FOURCC_H444: {
      const uint8_t* src_y = sample + (ptrdiff_t)src_width * crop_y + crop_x;
      const uint8_t* src_u =
          sample + src_width * ((ptrdiff_t)abs_src_height + crop_y) + crop_x;
      const uint8_t* src_v =
          sample + src_width * ((ptrdiff_t)abs_src_height * 2 + crop_y) +
          crop_x;
      r = H444ToARGB(src_y, src_width, src_u, src_width, src_v, src_width,
                     dst_argb, dst_stride_argb, crop_width, inv_crop_height);
      break;
    }

    case FOURCC_U444: {
      const uint8_t* src_y = sample + (ptrdiff_t)src_width * crop_y + crop_x;
      const uint8_t* src_u =
          sample + src_width * ((ptrdiff_t)abs_src_height + crop_y) + crop_x;
      const uint8_t* src_v =
          sample + src_width * ((ptrdiff_t)abs_src_height * 2 + crop_y) +
          crop_x;
      r = U444ToARGB(src_y, src_width, src_u, src_width, src_v, src_width,
                     dst_argb, dst_stride_argb, crop_width, inv_crop_height);
      break;
    }

#ifdef HAVE_JPEG
    case FOURCC_MJPG:
      r = MJPGToARGB(sample, sample_size, dst_argb, dst_stride_argb, src_width,
                     abs_src_height, crop_width, inv_crop_height);
      break;
#endif
    default:
      r = -1;  // unknown fourcc - return failure code.
  }

  if (need_buf) {
    if (!r) {
      r = ARGBRotate(dst_argb, dst_stride_argb, dest_argb, dest_dst_stride_argb,
                     crop_width, abs_crop_height, rotation);
    }
    free(rotate_buffer);
  } else if (rotation) {
    src = sample + ((ptrdiff_t)src_width * crop_y + crop_x) * 4;
    r = ARGBRotate(src, src_width * 4, dst_argb, dst_stride_argb, crop_width,
                   inv_crop_height, rotation);
  }

  return r;
}

#ifdef __cplusplus
}  // extern "C"
}  // namespace libyuv
#endif
