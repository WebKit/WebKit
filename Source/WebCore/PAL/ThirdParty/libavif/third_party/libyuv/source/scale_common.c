/*
 *  Copyright 2013 The LibYuv Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS. All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "libyuv/scale.h"

#include <assert.h>
#include <string.h>

#include "libyuv/planar_functions.h"  // For CopyARGB
#include "libyuv/row.h"
#include "libyuv/scale_row.h"

static __inline int Abs(int v) {
  return v >= 0 ? v : -v;
}

// Sample position: (O is src sample position, X is dst sample position)
//
//      v dst_ptr at here           v stop at here
//  X O X   X O X   X O X   X O X   X O X
//    ^ src_ptr at here
void ScaleRowUp2_Linear_C(const uint8_t* src_ptr,
                          uint8_t* dst_ptr,
                          int dst_width) {
  int src_width = dst_width >> 1;
  int x;
  assert((dst_width % 2 == 0) && (dst_width >= 0));
  for (x = 0; x < src_width; ++x) {
    dst_ptr[2 * x + 0] = (src_ptr[x + 0] * 3 + src_ptr[x + 1] * 1 + 2) >> 2;
    dst_ptr[2 * x + 1] = (src_ptr[x + 0] * 1 + src_ptr[x + 1] * 3 + 2) >> 2;
  }
}

// Sample position: (O is src sample position, X is dst sample position)
//
//    src_ptr at here
//  X v X   X   X   X   X   X   X   X   X
//    O       O       O       O       O
//  X   X   X   X   X   X   X   X   X   X
//      ^ dst_ptr at here           ^ stop at here
//  X   X   X   X   X   X   X   X   X   X
//    O       O       O       O       O
//  X   X   X   X   X   X   X   X   X   X
void ScaleRowUp2_Bilinear_C(const uint8_t* src_ptr,
                            ptrdiff_t src_stride,
                            uint8_t* dst_ptr,
                            ptrdiff_t dst_stride,
                            int dst_width) {
  const uint8_t* s = src_ptr;
  const uint8_t* t = src_ptr + src_stride;
  uint8_t* d = dst_ptr;
  uint8_t* e = dst_ptr + dst_stride;
  int src_width = dst_width >> 1;
  int x;
  assert((dst_width % 2 == 0) && (dst_width >= 0));
  for (x = 0; x < src_width; ++x) {
    d[2 * x + 0] =
        (s[x + 0] * 9 + s[x + 1] * 3 + t[x + 0] * 3 + t[x + 1] * 1 + 8) >> 4;
    d[2 * x + 1] =
        (s[x + 0] * 3 + s[x + 1] * 9 + t[x + 0] * 1 + t[x + 1] * 3 + 8) >> 4;
    e[2 * x + 0] =
        (s[x + 0] * 3 + s[x + 1] * 1 + t[x + 0] * 9 + t[x + 1] * 3 + 8) >> 4;
    e[2 * x + 1] =
        (s[x + 0] * 1 + s[x + 1] * 3 + t[x + 0] * 3 + t[x + 1] * 9 + 8) >> 4;
  }
}

// Only suitable for at most 14 bit range.
void ScaleRowUp2_Linear_16_C(const uint16_t* src_ptr,
                             uint16_t* dst_ptr,
                             int dst_width) {
  int src_width = dst_width >> 1;
  int x;
  assert((dst_width % 2 == 0) && (dst_width >= 0));
  for (x = 0; x < src_width; ++x) {
    dst_ptr[2 * x + 0] = (src_ptr[x + 0] * 3 + src_ptr[x + 1] * 1 + 2) >> 2;
    dst_ptr[2 * x + 1] = (src_ptr[x + 0] * 1 + src_ptr[x + 1] * 3 + 2) >> 2;
  }
}

// Only suitable for at most 12bit range.
void ScaleRowUp2_Bilinear_16_C(const uint16_t* src_ptr,
                               ptrdiff_t src_stride,
                               uint16_t* dst_ptr,
                               ptrdiff_t dst_stride,
                               int dst_width) {
  const uint16_t* s = src_ptr;
  const uint16_t* t = src_ptr + src_stride;
  uint16_t* d = dst_ptr;
  uint16_t* e = dst_ptr + dst_stride;
  int src_width = dst_width >> 1;
  int x;
  assert((dst_width % 2 == 0) && (dst_width >= 0));
  for (x = 0; x < src_width; ++x) {
    d[2 * x + 0] =
        (s[x + 0] * 9 + s[x + 1] * 3 + t[x + 0] * 3 + t[x + 1] * 1 + 8) >> 4;
    d[2 * x + 1] =
        (s[x + 0] * 3 + s[x + 1] * 9 + t[x + 0] * 1 + t[x + 1] * 3 + 8) >> 4;
    e[2 * x + 0] =
        (s[x + 0] * 3 + s[x + 1] * 1 + t[x + 0] * 9 + t[x + 1] * 3 + 8) >> 4;
    e[2 * x + 1] =
        (s[x + 0] * 1 + s[x + 1] * 3 + t[x + 0] * 3 + t[x + 1] * 9 + 8) >> 4;
  }
}

// Scales a single row of pixels using point sampling.
void ScaleCols_C(uint8_t* dst_ptr,
                 const uint8_t* src_ptr,
                 int dst_width,
                 int x,
                 int dx) {
  int j;
  for (j = 0; j < dst_width - 1; j += 2) {
    dst_ptr[0] = src_ptr[x >> 16];
    x += dx;
    dst_ptr[1] = src_ptr[x >> 16];
    x += dx;
    dst_ptr += 2;
  }
  if (dst_width & 1) {
    dst_ptr[0] = src_ptr[x >> 16];
  }
}

void ScaleCols_16_C(uint16_t* dst_ptr,
                    const uint16_t* src_ptr,
                    int dst_width,
                    int x,
                    int dx) {
  int j;
  for (j = 0; j < dst_width - 1; j += 2) {
    dst_ptr[0] = src_ptr[x >> 16];
    x += dx;
    dst_ptr[1] = src_ptr[x >> 16];
    x += dx;
    dst_ptr += 2;
  }
  if (dst_width & 1) {
    dst_ptr[0] = src_ptr[x >> 16];
  }
}

// Scales a single row of pixels up by 2x using point sampling.
void ScaleColsUp2_C(uint8_t* dst_ptr,
                    const uint8_t* src_ptr,
                    int dst_width,
                    int x,
                    int dx) {
  int j;
  (void)x;
  (void)dx;
  for (j = 0; j < dst_width - 1; j += 2) {
    dst_ptr[1] = dst_ptr[0] = src_ptr[0];
    src_ptr += 1;
    dst_ptr += 2;
  }
  if (dst_width & 1) {
    dst_ptr[0] = src_ptr[0];
  }
}

void ScaleColsUp2_16_C(uint16_t* dst_ptr,
                       const uint16_t* src_ptr,
                       int dst_width,
                       int x,
                       int dx) {
  int j;
  (void)x;
  (void)dx;
  for (j = 0; j < dst_width - 1; j += 2) {
    dst_ptr[1] = dst_ptr[0] = src_ptr[0];
    src_ptr += 1;
    dst_ptr += 2;
  }
  if (dst_width & 1) {
    dst_ptr[0] = src_ptr[0];
  }
}

// (1-f)a + fb can be replaced with a + f(b-a)
#if defined(__arm__) || defined(__aarch64__)
#define BLENDER(a, b, f) \
  (uint8_t)((int)(a) + ((((int)((f)) * ((int)(b) - (int)(a))) + 0x8000) >> 16))
#else
// Intel uses 7 bit math with rounding.
#define BLENDER(a, b, f) \
  (uint8_t)((int)(a) + (((int)((f) >> 9) * ((int)(b) - (int)(a)) + 0x40) >> 7))
#endif

void ScaleFilterCols_C(uint8_t* dst_ptr,
                       const uint8_t* src_ptr,
                       int dst_width,
                       int x,
                       int dx) {
  int j;
  for (j = 0; j < dst_width - 1; j += 2) {
    int xi = x >> 16;
    int a = src_ptr[xi];
    int b = src_ptr[xi + 1];
    dst_ptr[0] = BLENDER(a, b, x & 0xffff);
    x += dx;
    xi = x >> 16;
    a = src_ptr[xi];
    b = src_ptr[xi + 1];
    dst_ptr[1] = BLENDER(a, b, x & 0xffff);
    x += dx;
    dst_ptr += 2;
  }
  if (dst_width & 1) {
    int xi = x >> 16;
    int a = src_ptr[xi];
    int b = src_ptr[xi + 1];
    dst_ptr[0] = BLENDER(a, b, x & 0xffff);
  }
}

void ScaleFilterCols64_C(uint8_t* dst_ptr,
                         const uint8_t* src_ptr,
                         int dst_width,
                         int x32,
                         int dx) {
  int64_t x = (int64_t)(x32);
  int j;
  for (j = 0; j < dst_width - 1; j += 2) {
    int64_t xi = x >> 16;
    int a = src_ptr[xi];
    int b = src_ptr[xi + 1];
    dst_ptr[0] = BLENDER(a, b, x & 0xffff);
    x += dx;
    xi = x >> 16;
    a = src_ptr[xi];
    b = src_ptr[xi + 1];
    dst_ptr[1] = BLENDER(a, b, x & 0xffff);
    x += dx;
    dst_ptr += 2;
  }
  if (dst_width & 1) {
    int64_t xi = x >> 16;
    int a = src_ptr[xi];
    int b = src_ptr[xi + 1];
    dst_ptr[0] = BLENDER(a, b, x & 0xffff);
  }
}
#undef BLENDER

// Same as 8 bit arm blender but return is cast to uint16_t
#define BLENDER(a, b, f) \
  (uint16_t)(            \
      (int)(a) +         \
      (int)((((int64_t)((f)) * ((int64_t)(b) - (int)(a))) + 0x8000) >> 16))

void ScaleFilterCols_16_C(uint16_t* dst_ptr,
                          const uint16_t* src_ptr,
                          int dst_width,
                          int x,
                          int dx) {
  int j;
  for (j = 0; j < dst_width - 1; j += 2) {
    int xi = x >> 16;
    int a = src_ptr[xi];
    int b = src_ptr[xi + 1];
    dst_ptr[0] = BLENDER(a, b, x & 0xffff);
    x += dx;
    xi = x >> 16;
    a = src_ptr[xi];
    b = src_ptr[xi + 1];
    dst_ptr[1] = BLENDER(a, b, x & 0xffff);
    x += dx;
    dst_ptr += 2;
  }
  if (dst_width & 1) {
    int xi = x >> 16;
    int a = src_ptr[xi];
    int b = src_ptr[xi + 1];
    dst_ptr[0] = BLENDER(a, b, x & 0xffff);
  }
}

void ScaleFilterCols64_16_C(uint16_t* dst_ptr,
                            const uint16_t* src_ptr,
                            int dst_width,
                            int x32,
                            int dx) {
  int64_t x = (int64_t)(x32);
  int j;
  for (j = 0; j < dst_width - 1; j += 2) {
    int64_t xi = x >> 16;
    int a = src_ptr[xi];
    int b = src_ptr[xi + 1];
    dst_ptr[0] = BLENDER(a, b, x & 0xffff);
    x += dx;
    xi = x >> 16;
    a = src_ptr[xi];
    b = src_ptr[xi + 1];
    dst_ptr[1] = BLENDER(a, b, x & 0xffff);
    x += dx;
    dst_ptr += 2;
  }
  if (dst_width & 1) {
    int64_t xi = x >> 16;
    int a = src_ptr[xi];
    int b = src_ptr[xi + 1];
    dst_ptr[0] = BLENDER(a, b, x & 0xffff);
  }
}
#undef BLENDER


void ScaleAddRow_C(const uint8_t* src_ptr, uint16_t* dst_ptr, int src_width) {
  int x;
  assert(src_width > 0);
  for (x = 0; x < src_width - 1; x += 2) {
    dst_ptr[0] += src_ptr[0];
    dst_ptr[1] += src_ptr[1];
    src_ptr += 2;
    dst_ptr += 2;
  }
  if (src_width & 1) {
    dst_ptr[0] += src_ptr[0];
  }
}

void ScaleAddRow_16_C(const uint16_t* src_ptr,
                      uint32_t* dst_ptr,
                      int src_width) {
  int x;
  assert(src_width > 0);
  for (x = 0; x < src_width - 1; x += 2) {
    dst_ptr[0] += src_ptr[0];
    dst_ptr[1] += src_ptr[1];
    src_ptr += 2;
    dst_ptr += 2;
  }
  if (src_width & 1) {
    dst_ptr[0] += src_ptr[0];
  }
}


// Scale plane vertically with bilinear interpolation.
void ScalePlaneVertical(int src_height,
                        int dst_width,
                        int dst_height,
                        int src_stride,
                        int dst_stride,
                        const uint8_t* src_argb,
                        uint8_t* dst_argb,
                        int x,
                        int y,
                        int dy,
                        int bpp,  // bytes per pixel. 4 for ARGB.
                        enum FilterMode filtering) {
  // TODO(fbarchard): Allow higher bpp.
  int dst_width_bytes = dst_width * bpp;
  void (*InterpolateRow)(uint8_t* dst_argb, const uint8_t* src_argb,
                         ptrdiff_t src_stride, int dst_width,
                         int source_y_fraction) = InterpolateRow_C;
  const int max_y = (src_height > 1) ? ((src_height - 1) << 16) - 1 : 0;
  int j;
  assert(bpp >= 1 && bpp <= 4);
  assert(src_height != 0);
  assert(dst_width > 0);
  assert(dst_height > 0);
  src_argb += (x >> 16) * bpp;

  for (j = 0; j < dst_height; ++j) {
    int yi;
    int yf;
    if (y > max_y) {
      y = max_y;
    }
    yi = y >> 16;
    yf = filtering ? ((y >> 8) & 255) : 0;
    InterpolateRow(dst_argb, src_argb + yi * src_stride, src_stride,
                   dst_width_bytes, yf);
    dst_argb += dst_stride;
    y += dy;
  }
}

void ScalePlaneVertical_16(int src_height,
                           int dst_width,
                           int dst_height,
                           int src_stride,
                           int dst_stride,
                           const uint16_t* src_argb,
                           uint16_t* dst_argb,
                           int x,
                           int y,
                           int dy,
                           int wpp, /* words per pixel. normally 1 */
                           enum FilterMode filtering) {
  // TODO(fbarchard): Allow higher wpp.
  int dst_width_words = dst_width * wpp;
  void (*InterpolateRow)(uint16_t* dst_argb, const uint16_t* src_argb,
                         ptrdiff_t src_stride, int dst_width,
                         int source_y_fraction) = InterpolateRow_16_C;
  const int max_y = (src_height > 1) ? ((src_height - 1) << 16) - 1 : 0;
  int j;
  assert(wpp >= 1 && wpp <= 2);
  assert(src_height != 0);
  assert(dst_width > 0);
  assert(dst_height > 0);
  src_argb += (x >> 16) * wpp;
  for (j = 0; j < dst_height; ++j) {
    int yi;
    int yf;
    if (y > max_y) {
      y = max_y;
    }
    yi = y >> 16;
    yf = filtering ? ((y >> 8) & 255) : 0;
    InterpolateRow(dst_argb, src_argb + yi * src_stride, src_stride,
                   dst_width_words, yf);
    dst_argb += dst_stride;
    y += dy;
  }
}

// Simplify the filtering based on scale factors.
enum FilterMode ScaleFilterReduce(int src_width,
                                  int src_height,
                                  int dst_width,
                                  int dst_height,
                                  enum FilterMode filtering) {
  if (src_width < 0) {
    src_width = -src_width;
  }
  if (src_height < 0) {
    src_height = -src_height;
  }
  if (filtering == kFilterBox) {
    // If scaling either axis to 0.5 or larger, switch from Box to Bilinear.
    if (dst_width * 2 >= src_width || dst_height * 2 >= src_height) {
      filtering = kFilterBilinear;
    }
  }
  if (filtering == kFilterBilinear) {
    if (src_height == 1) {
      filtering = kFilterLinear;
    }
    // TODO(fbarchard): Detect any odd scale factor and reduce to Linear.
    if (dst_height == src_height || dst_height * 3 == src_height) {
      filtering = kFilterLinear;
    }
    // TODO(fbarchard): Remove 1 pixel wide filter restriction, which is to
    // avoid reading 2 pixels horizontally that causes memory exception.
    if (src_width == 1) {
      filtering = kFilterNone;
    }
  }
  if (filtering == kFilterLinear) {
    if (src_width == 1) {
      filtering = kFilterNone;
    }
    // TODO(fbarchard): Detect any odd scale factor and reduce to None.
    if (dst_width == src_width || dst_width * 3 == src_width) {
      filtering = kFilterNone;
    }
  }
  return filtering;
}

// Divide num by div and return as 16.16 fixed point result.
int FixedDiv_C(int num, int div) {
  return (int)(((int64_t)(num) << 16) / div);
}

// Divide num - 1 by div - 1 and return as 16.16 fixed point result.
int FixedDiv1_C(int num, int div) {
  return (int)((((int64_t)(num) << 16) - 0x00010001) / (div - 1));
}

#define CENTERSTART(dx, s) (dx < 0) ? -((-dx >> 1) + s) : ((dx >> 1) + s)

// Compute slope values for stepping.
void ScaleSlope(int src_width,
                int src_height,
                int dst_width,
                int dst_height,
                enum FilterMode filtering,
                int* x,
                int* y,
                int* dx,
                int* dy) {
  assert(x != NULL);
  assert(y != NULL);
  assert(dx != NULL);
  assert(dy != NULL);
  assert(src_width != 0);
  assert(src_height != 0);
  assert(dst_width > 0);
  assert(dst_height > 0);
  // Check for 1 pixel and avoid FixedDiv overflow.
  if (dst_width == 1 && src_width >= 32768) {
    dst_width = src_width;
  }
  if (dst_height == 1 && src_height >= 32768) {
    dst_height = src_height;
  }
  if (filtering == kFilterBox) {
    // Scale step for point sampling duplicates all pixels equally.
    *dx = FixedDiv(Abs(src_width), dst_width);
    *dy = FixedDiv(src_height, dst_height);
    *x = 0;
    *y = 0;
  } else if (filtering == kFilterBilinear) {
    // Scale step for bilinear sampling renders last pixel once for upsample.
    if (dst_width <= Abs(src_width)) {
      *dx = FixedDiv(Abs(src_width), dst_width);
      *x = CENTERSTART(*dx, -32768);  // Subtract 0.5 (32768) to center filter.
    } else if (src_width > 1 && dst_width > 1) {
      *dx = FixedDiv1(Abs(src_width), dst_width);
      *x = 0;
    }
    if (dst_height <= src_height) {
      *dy = FixedDiv(src_height, dst_height);
      *y = CENTERSTART(*dy, -32768);  // Subtract 0.5 (32768) to center filter.
    } else if (src_height > 1 && dst_height > 1) {
      *dy = FixedDiv1(src_height, dst_height);
      *y = 0;
    }
  } else if (filtering == kFilterLinear) {
    // Scale step for bilinear sampling renders last pixel once for upsample.
    if (dst_width <= Abs(src_width)) {
      *dx = FixedDiv(Abs(src_width), dst_width);
      *x = CENTERSTART(*dx, -32768);  // Subtract 0.5 (32768) to center filter.
    } else if (src_width > 1 && dst_width > 1) {
      *dx = FixedDiv1(Abs(src_width), dst_width);
      *x = 0;
    }
    *dy = FixedDiv(src_height, dst_height);
    *y = *dy >> 1;
  } else {
    // Scale step for point sampling duplicates all pixels equally.
    *dx = FixedDiv(Abs(src_width), dst_width);
    *dy = FixedDiv(src_height, dst_height);
    *x = CENTERSTART(*dx, 0);
    *y = CENTERSTART(*dy, 0);
  }
  // Negative src_width means horizontally mirror.
  if (src_width < 0) {
    *x += (dst_width - 1) * *dx;
    *dx = -*dx;
    // src_width = -src_width;   // Caller must do this.
  }
}
#undef CENTERSTART
