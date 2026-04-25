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
#include <emmintrin.h>  // SSE2

#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"

#include "aom_dsp/x86/synonyms.h"
#include "aom_ports/mem.h"

#include "av1/common/filter.h"
#include "av1/common/reconinter.h"

typedef uint32_t (*high_variance_fn_t)(const uint16_t *src, int src_stride,
                                       const uint16_t *ref, int ref_stride,
                                       uint32_t *sse, int *sum);

uint32_t aom_highbd_calc8x8var_sse2(const uint16_t *src, int src_stride,
                                    const uint16_t *ref, int ref_stride,
                                    uint32_t *sse, int *sum);

uint32_t aom_highbd_calc16x16var_sse2(const uint16_t *src, int src_stride,
                                      const uint16_t *ref, int ref_stride,
                                      uint32_t *sse, int *sum);

static void highbd_8_variance_sse2(const uint16_t *src, int src_stride,
                                   const uint16_t *ref, int ref_stride, int w,
                                   int h, uint32_t *sse, int *sum,
                                   high_variance_fn_t var_fn, int block_size) {
  int i, j;

  *sse = 0;
  *sum = 0;

  for (i = 0; i < h; i += block_size) {
    for (j = 0; j < w; j += block_size) {
      unsigned int sse0;
      int sum0;
      var_fn(src + src_stride * i + j, src_stride, ref + ref_stride * i + j,
             ref_stride, &sse0, &sum0);
      *sse += sse0;
      *sum += sum0;
    }
  }
}

static void highbd_10_variance_sse2(const uint16_t *src, int src_stride,
                                    const uint16_t *ref, int ref_stride, int w,
                                    int h, uint32_t *sse, int *sum,
                                    high_variance_fn_t var_fn, int block_size) {
  int i, j;
  uint64_t sse_long = 0;
  int32_t sum_long = 0;

  for (i = 0; i < h; i += block_size) {
    for (j = 0; j < w; j += block_size) {
      unsigned int sse0;
      int sum0;
      var_fn(src + src_stride * i + j, src_stride, ref + ref_stride * i + j,
             ref_stride, &sse0, &sum0);
      sse_long += sse0;
      sum_long += sum0;
    }
  }
  *sum = ROUND_POWER_OF_TWO(sum_long, 2);
  *sse = (uint32_t)ROUND_POWER_OF_TWO(sse_long, 4);
}

static void highbd_12_variance_sse2(const uint16_t *src, int src_stride,
                                    const uint16_t *ref, int ref_stride, int w,
                                    int h, uint32_t *sse, int *sum,
                                    high_variance_fn_t var_fn, int block_size) {
  int i, j;
  uint64_t sse_long = 0;
  int32_t sum_long = 0;

  for (i = 0; i < h; i += block_size) {
    for (j = 0; j < w; j += block_size) {
      unsigned int sse0;
      int sum0;
      var_fn(src + src_stride * i + j, src_stride, ref + ref_stride * i + j,
             ref_stride, &sse0, &sum0);
      sse_long += sse0;
      sum_long += sum0;
    }
  }
  *sum = ROUND_POWER_OF_TWO(sum_long, 4);
  *sse = (uint32_t)ROUND_POWER_OF_TWO(sse_long, 8);
}

#define VAR_FN(w, h, block_size, shift)                                    \
  uint32_t aom_highbd_8_variance##w##x##h##_sse2(                          \
      const uint8_t *src8, int src_stride, const uint8_t *ref8,            \
      int ref_stride, uint32_t *sse) {                                     \
    int sum;                                                               \
    uint16_t *src = CONVERT_TO_SHORTPTR(src8);                             \
    uint16_t *ref = CONVERT_TO_SHORTPTR(ref8);                             \
    highbd_8_variance_sse2(                                                \
        src, src_stride, ref, ref_stride, w, h, sse, &sum,                 \
        aom_highbd_calc##block_size##x##block_size##var_sse2, block_size); \
    return *sse - (uint32_t)(((int64_t)sum * sum) >> shift);               \
  }                                                                        \
                                                                           \
  uint32_t aom_highbd_10_variance##w##x##h##_sse2(                         \
      const uint8_t *src8, int src_stride, const uint8_t *ref8,            \
      int ref_stride, uint32_t *sse) {                                     \
    int sum;                                                               \
    int64_t var;                                                           \
    uint16_t *src = CONVERT_TO_SHORTPTR(src8);                             \
    uint16_t *ref = CONVERT_TO_SHORTPTR(ref8);                             \
    highbd_10_variance_sse2(                                               \
        src, src_stride, ref, ref_stride, w, h, sse, &sum,                 \
        aom_highbd_calc##block_size##x##block_size##var_sse2, block_size); \
    var = (int64_t)(*sse) - (((int64_t)sum * sum) >> shift);               \
    return (var >= 0) ? (uint32_t)var : 0;                                 \
  }                                                                        \
                                                                           \
  uint32_t aom_highbd_12_variance##w##x##h##_sse2(                         \
      const uint8_t *src8, int src_stride, const uint8_t *ref8,            \
      int ref_stride, uint32_t *sse) {                                     \
    int sum;                                                               \
    int64_t var;                                                           \
    uint16_t *src = CONVERT_TO_SHORTPTR(src8);                             \
    uint16_t *ref = CONVERT_TO_SHORTPTR(ref8);                             \
    highbd_12_variance_sse2(                                               \
        src, src_stride, ref, ref_stride, w, h, sse, &sum,                 \
        aom_highbd_calc##block_size##x##block_size##var_sse2, block_size); \
    var = (int64_t)(*sse) - (((int64_t)sum * sum) >> shift);               \
    return (var >= 0) ? (uint32_t)var : 0;                                 \
  }

VAR_FN(128, 128, 16, 14)
VAR_FN(128, 64, 16, 13)
VAR_FN(64, 128, 16, 13)
VAR_FN(64, 64, 16, 12)
VAR_FN(64, 32, 16, 11)
VAR_FN(32, 64, 16, 11)
VAR_FN(32, 32, 16, 10)
VAR_FN(32, 16, 16, 9)
VAR_FN(16, 32, 16, 9)
VAR_FN(16, 16, 16, 8)
VAR_FN(16, 8, 8, 7)
VAR_FN(8, 16, 8, 7)
VAR_FN(8, 8, 8, 6)

#if !CONFIG_REALTIME_ONLY
VAR_FN(8, 32, 8, 8)
VAR_FN(32, 8, 8, 8)
VAR_FN(16, 64, 16, 10)
VAR_FN(64, 16, 16, 10)
#endif  // !CONFIG_REALTIME_ONLY

#undef VAR_FN

unsigned int aom_highbd_8_mse16x16_sse2(const uint8_t *src8, int src_stride,
                                        const uint8_t *ref8, int ref_stride,
                                        unsigned int *sse) {
  int sum;
  uint16_t *src = CONVERT_TO_SHORTPTR(src8);
  uint16_t *ref = CONVERT_TO_SHORTPTR(ref8);
  highbd_8_variance_sse2(src, src_stride, ref, ref_stride, 16, 16, sse, &sum,
                         aom_highbd_calc16x16var_sse2, 16);
  return *sse;
}

unsigned int aom_highbd_10_mse16x16_sse2(const uint8_t *src8, int src_stride,
                                         const uint8_t *ref8, int ref_stride,
                                         unsigned int *sse) {
  int sum;
  uint16_t *src = CONVERT_TO_SHORTPTR(src8);
  uint16_t *ref = CONVERT_TO_SHORTPTR(ref8);
  highbd_10_variance_sse2(src, src_stride, ref, ref_stride, 16, 16, sse, &sum,
                          aom_highbd_calc16x16var_sse2, 16);
  return *sse;
}

unsigned int aom_highbd_12_mse16x16_sse2(const uint8_t *src8, int src_stride,
                                         const uint8_t *ref8, int ref_stride,
                                         unsigned int *sse) {
  int sum;
  uint16_t *src = CONVERT_TO_SHORTPTR(src8);
  uint16_t *ref = CONVERT_TO_SHORTPTR(ref8);
  highbd_12_variance_sse2(src, src_stride, ref, ref_stride, 16, 16, sse, &sum,
                          aom_highbd_calc16x16var_sse2, 16);
  return *sse;
}

unsigned int aom_highbd_8_mse8x8_sse2(const uint8_t *src8, int src_stride,
                                      const uint8_t *ref8, int ref_stride,
                                      unsigned int *sse) {
  int sum;
  uint16_t *src = CONVERT_TO_SHORTPTR(src8);
  uint16_t *ref = CONVERT_TO_SHORTPTR(ref8);
  highbd_8_variance_sse2(src, src_stride, ref, ref_stride, 8, 8, sse, &sum,
                         aom_highbd_calc8x8var_sse2, 8);
  return *sse;
}

unsigned int aom_highbd_10_mse8x8_sse2(const uint8_t *src8, int src_stride,
                                       const uint8_t *ref8, int ref_stride,
                                       unsigned int *sse) {
  int sum;
  uint16_t *src = CONVERT_TO_SHORTPTR(src8);
  uint16_t *ref = CONVERT_TO_SHORTPTR(ref8);
  highbd_10_variance_sse2(src, src_stride, ref, ref_stride, 8, 8, sse, &sum,
                          aom_highbd_calc8x8var_sse2, 8);
  return *sse;
}

unsigned int aom_highbd_12_mse8x8_sse2(const uint8_t *src8, int src_stride,
                                       const uint8_t *ref8, int ref_stride,
                                       unsigned int *sse) {
  int sum;
  uint16_t *src = CONVERT_TO_SHORTPTR(src8);
  uint16_t *ref = CONVERT_TO_SHORTPTR(ref8);
  highbd_12_variance_sse2(src, src_stride, ref, ref_stride, 8, 8, sse, &sum,
                          aom_highbd_calc8x8var_sse2, 8);
  return *sse;
}

// The 2 unused parameters are place holders for PIC enabled build.
// These definitions are for functions defined in
// highbd_subpel_variance_impl_sse2.asm
#define DECL(w, opt)                                                         \
  int aom_highbd_sub_pixel_variance##w##xh_##opt(                            \
      const uint16_t *src, ptrdiff_t src_stride, int x_offset, int y_offset, \
      const uint16_t *dst, ptrdiff_t dst_stride, int height,                 \
      unsigned int *sse, void *unused0, void *unused);
#define DECLS(opt) \
  DECL(8, opt)     \
  DECL(16, opt)

DECLS(sse2)

#undef DECLS
#undef DECL

#define FN(w, h, wf, wlog2, hlog2, opt, cast)                                  \
  uint32_t aom_highbd_8_sub_pixel_variance##w##x##h##_##opt(                   \
      const uint8_t *src8, int src_stride, int x_offset, int y_offset,         \
      const uint8_t *dst8, int dst_stride, uint32_t *sse_ptr) {                \
    uint16_t *src = CONVERT_TO_SHORTPTR(src8);                                 \
    uint16_t *dst = CONVERT_TO_SHORTPTR(dst8);                                 \
    int se = 0;                                                                \
    unsigned int sse = 0;                                                      \
    unsigned int sse2;                                                         \
    int row_rep = (w > 64) ? 2 : 1;                                            \
    for (int wd_64 = 0; wd_64 < row_rep; wd_64++) {                            \
      src += wd_64 * 64;                                                       \
      dst += wd_64 * 64;                                                       \
      int se2 = aom_highbd_sub_pixel_variance##wf##xh_##opt(                   \
          src, src_stride, x_offset, y_offset, dst, dst_stride, h, &sse2,      \
          NULL, NULL);                                                         \
      se += se2;                                                               \
      sse += sse2;                                                             \
      if (w > wf) {                                                            \
        se2 = aom_highbd_sub_pixel_variance##wf##xh_##opt(                     \
            src + wf, src_stride, x_offset, y_offset, dst + wf, dst_stride, h, \
            &sse2, NULL, NULL);                                                \
        se += se2;                                                             \
        sse += sse2;                                                           \
        if (w > wf * 2) {                                                      \
          se2 = aom_highbd_sub_pixel_variance##wf##xh_##opt(                   \
              src + 2 * wf, src_stride, x_offset, y_offset, dst + 2 * wf,      \
              dst_stride, h, &sse2, NULL, NULL);                               \
          se += se2;                                                           \
          sse += sse2;                                                         \
          se2 = aom_highbd_sub_pixel_variance##wf##xh_##opt(                   \
              src + 3 * wf, src_stride, x_offset, y_offset, dst + 3 * wf,      \
              dst_stride, h, &sse2, NULL, NULL);                               \
          se += se2;                                                           \
          sse += sse2;                                                         \
        }                                                                      \
      }                                                                        \
    }                                                                          \
    *sse_ptr = sse;                                                            \
    return sse - (uint32_t)((cast se * se) >> (wlog2 + hlog2));                \
  }                                                                            \
                                                                               \
  uint32_t aom_highbd_10_sub_pixel_variance##w##x##h##_##opt(                  \
      const uint8_t *src8, int src_stride, int x_offset, int y_offset,         \
      const uint8_t *dst8, int dst_stride, uint32_t *sse_ptr) {                \
    int64_t var;                                                               \
    uint32_t sse;                                                              \
    uint64_t long_sse = 0;                                                     \
    uint16_t *src = CONVERT_TO_SHORTPTR(src8);                                 \
    uint16_t *dst = CONVERT_TO_SHORTPTR(dst8);                                 \
    int se = 0;                                                                \
    int row_rep = (w > 64) ? 2 : 1;                                            \
    for (int wd_64 = 0; wd_64 < row_rep; wd_64++) {                            \
      src += wd_64 * 64;                                                       \
      dst += wd_64 * 64;                                                       \
      int se2 = aom_highbd_sub_pixel_variance##wf##xh_##opt(                   \
          src, src_stride, x_offset, y_offset, dst, dst_stride, h, &sse, NULL, \
          NULL);                                                               \
      se += se2;                                                               \
      long_sse += sse;                                                         \
      if (w > wf) {                                                            \
        uint32_t sse2;                                                         \
        se2 = aom_highbd_sub_pixel_variance##wf##xh_##opt(                     \
            src + wf, src_stride, x_offset, y_offset, dst + wf, dst_stride, h, \
            &sse2, NULL, NULL);                                                \
        se += se2;                                                             \
        long_sse += sse2;                                                      \
        if (w > wf * 2) {                                                      \
          se2 = aom_highbd_sub_pixel_variance##wf##xh_##opt(                   \
              src + 2 * wf, src_stride, x_offset, y_offset, dst + 2 * wf,      \
              dst_stride, h, &sse2, NULL, NULL);                               \
          se += se2;                                                           \
          long_sse += sse2;                                                    \
          se2 = aom_highbd_sub_pixel_variance##wf##xh_##opt(                   \
              src + 3 * wf, src_stride, x_offset, y_offset, dst + 3 * wf,      \
              dst_stride, h, &sse2, NULL, NULL);                               \
          se += se2;                                                           \
          long_sse += sse2;                                                    \
        }                                                                      \
      }                                                                        \
    }                                                                          \
    se = ROUND_POWER_OF_TWO(se, 2);                                            \
    sse = (uint32_t)ROUND_POWER_OF_TWO(long_sse, 4);                           \
    *sse_ptr = sse;                                                            \
    var = (int64_t)(sse) - ((cast se * se) >> (wlog2 + hlog2));                \
    return (var >= 0) ? (uint32_t)var : 0;                                     \
  }                                                                            \
                                                                               \
  uint32_t aom_highbd_12_sub_pixel_variance##w##x##h##_##opt(                  \
      const uint8_t *src8, int src_stride, int x_offset, int y_offset,         \
      const uint8_t *dst8, int dst_stride, uint32_t *sse_ptr) {                \
    int start_row;                                                             \
    uint32_t sse;                                                              \
    int se = 0;                                                                \
    int64_t var;                                                               \
    uint64_t long_sse = 0;                                                     \
    uint16_t *src = CONVERT_TO_SHORTPTR(src8);                                 \
    uint16_t *dst = CONVERT_TO_SHORTPTR(dst8);                                 \
    int row_rep = (w > 64) ? 2 : 1;                                            \
    for (start_row = 0; start_row < h; start_row += 16) {                      \
      uint32_t sse2;                                                           \
      int height = h - start_row < 16 ? h - start_row : 16;                    \
      uint16_t *src_tmp = src + (start_row * src_stride);                      \
      uint16_t *dst_tmp = dst + (start_row * dst_stride);                      \
      for (int wd_64 = 0; wd_64 < row_rep; wd_64++) {                          \
        src_tmp += wd_64 * 64;                                                 \
        dst_tmp += wd_64 * 64;                                                 \
        int se2 = aom_highbd_sub_pixel_variance##wf##xh_##opt(                 \
            src_tmp, src_stride, x_offset, y_offset, dst_tmp, dst_stride,      \
            height, &sse2, NULL, NULL);                                        \
        se += se2;                                                             \
        long_sse += sse2;                                                      \
        if (w > wf) {                                                          \
          se2 = aom_highbd_sub_pixel_variance##wf##xh_##opt(                   \
              src_tmp + wf, src_stride, x_offset, y_offset, dst_tmp + wf,      \
              dst_stride, height, &sse2, NULL, NULL);                          \
          se += se2;                                                           \
          long_sse += sse2;                                                    \
          if (w > wf * 2) {                                                    \
            se2 = aom_highbd_sub_pixel_variance##wf##xh_##opt(                 \
                src_tmp + 2 * wf, src_stride, x_offset, y_offset,              \
                dst_tmp + 2 * wf, dst_stride, height, &sse2, NULL, NULL);      \
            se += se2;                                                         \
            long_sse += sse2;                                                  \
            se2 = aom_highbd_sub_pixel_variance##wf##xh_##opt(                 \
                src_tmp + 3 * wf, src_stride, x_offset, y_offset,              \
                dst_tmp + 3 * wf, dst_stride, height, &sse2, NULL, NULL);      \
            se += se2;                                                         \
            long_sse += sse2;                                                  \
          }                                                                    \
        }                                                                      \
      }                                                                        \
    }                                                                          \
    se = ROUND_POWER_OF_TWO(se, 4);                                            \
    sse = (uint32_t)ROUND_POWER_OF_TWO(long_sse, 8);                           \
    *sse_ptr = sse;                                                            \
    var = (int64_t)(sse) - ((cast se * se) >> (wlog2 + hlog2));                \
    return (var >= 0) ? (uint32_t)var : 0;                                     \
  }

#if CONFIG_REALTIME_ONLY
#define FNS(opt)                         \
  FN(128, 128, 16, 7, 7, opt, (int64_t)) \
  FN(128, 64, 16, 7, 6, opt, (int64_t))  \
  FN(64, 128, 16, 6, 7, opt, (int64_t))  \
  FN(64, 64, 16, 6, 6, opt, (int64_t))   \
  FN(64, 32, 16, 6, 5, opt, (int64_t))   \
  FN(32, 64, 16, 5, 6, opt, (int64_t))   \
  FN(32, 32, 16, 5, 5, opt, (int64_t))   \
  FN(32, 16, 16, 5, 4, opt, (int64_t))   \
  FN(16, 32, 16, 4, 5, opt, (int64_t))   \
  FN(16, 16, 16, 4, 4, opt, (int64_t))   \
  FN(16, 8, 16, 4, 3, opt, (int64_t))    \
  FN(8, 16, 8, 3, 4, opt, (int64_t))     \
  FN(8, 8, 8, 3, 3, opt, (int64_t))      \
  FN(8, 4, 8, 3, 2, opt, (int64_t))
#else  // !CONFIG_REALTIME_ONLY
#define FNS(opt)                         \
  FN(128, 128, 16, 7, 7, opt, (int64_t)) \
  FN(128, 64, 16, 7, 6, opt, (int64_t))  \
  FN(64, 128, 16, 6, 7, opt, (int64_t))  \
  FN(64, 64, 16, 6, 6, opt, (int64_t))   \
  FN(64, 32, 16, 6, 5, opt, (int64_t))   \
  FN(32, 64, 16, 5, 6, opt, (int64_t))   \
  FN(32, 32, 16, 5, 5, opt, (int64_t))   \
  FN(32, 16, 16, 5, 4, opt, (int64_t))   \
  FN(16, 32, 16, 4, 5, opt, (int64_t))   \
  FN(16, 16, 16, 4, 4, opt, (int64_t))   \
  FN(16, 8, 16, 4, 3, opt, (int64_t))    \
  FN(8, 16, 8, 3, 4, opt, (int64_t))     \
  FN(8, 8, 8, 3, 3, opt, (int64_t))      \
  FN(8, 4, 8, 3, 2, opt, (int64_t))      \
  FN(16, 4, 16, 4, 2, opt, (int64_t))    \
  FN(8, 32, 8, 3, 5, opt, (int64_t))     \
  FN(32, 8, 16, 5, 3, opt, (int64_t))    \
  FN(16, 64, 16, 4, 6, opt, (int64_t))   \
  FN(64, 16, 16, 6, 4, opt, (int64_t))
#endif  // CONFIG_REALTIME_ONLY

FNS(sse2)

#undef FNS
#undef FN

// The 2 unused parameters are place holders for PIC enabled build.
#define DECL(w, opt)                                                         \
  int aom_highbd_sub_pixel_avg_variance##w##xh_##opt(                        \
      const uint16_t *src, ptrdiff_t src_stride, int x_offset, int y_offset, \
      const uint16_t *dst, ptrdiff_t dst_stride, const uint16_t *sec,        \
      ptrdiff_t sec_stride, int height, unsigned int *sse, void *unused0,    \
      void *unused);
#define DECLS(opt) \
  DECL(16, opt)    \
  DECL(8, opt)

DECLS(sse2)
#undef DECL
#undef DECLS

#define FN(w, h, wf, wlog2, hlog2, opt, cast)                                  \
  uint32_t aom_highbd_8_sub_pixel_avg_variance##w##x##h##_##opt(               \
      const uint8_t *src8, int src_stride, int x_offset, int y_offset,         \
      const uint8_t *dst8, int dst_stride, uint32_t *sse_ptr,                  \
      const uint8_t *sec8) {                                                   \
    uint32_t sse;                                                              \
    uint16_t *src = CONVERT_TO_SHORTPTR(src8);                                 \
    uint16_t *dst = CONVERT_TO_SHORTPTR(dst8);                                 \
    uint16_t *sec = CONVERT_TO_SHORTPTR(sec8);                                 \
    int se = aom_highbd_sub_pixel_avg_variance##wf##xh_##opt(                  \
        src, src_stride, x_offset, y_offset, dst, dst_stride, sec, w, h, &sse, \
        NULL, NULL);                                                           \
    if (w > wf) {                                                              \
      uint32_t sse2;                                                           \
      int se2 = aom_highbd_sub_pixel_avg_variance##wf##xh_##opt(               \
          src + wf, src_stride, x_offset, y_offset, dst + wf, dst_stride,      \
          sec + wf, w, h, &sse2, NULL, NULL);                                  \
      se += se2;                                                               \
      sse += sse2;                                                             \
      if (w > wf * 2) {                                                        \
        se2 = aom_highbd_sub_pixel_avg_variance##wf##xh_##opt(                 \
            src + 2 * wf, src_stride, x_offset, y_offset, dst + 2 * wf,        \
            dst_stride, sec + 2 * wf, w, h, &sse2, NULL, NULL);                \
        se += se2;                                                             \
        sse += sse2;                                                           \
        se2 = aom_highbd_sub_pixel_avg_variance##wf##xh_##opt(                 \
            src + 3 * wf, src_stride, x_offset, y_offset, dst + 3 * wf,        \
            dst_stride, sec + 3 * wf, w, h, &sse2, NULL, NULL);                \
        se += se2;                                                             \
        sse += sse2;                                                           \
      }                                                                        \
    }                                                                          \
    *sse_ptr = sse;                                                            \
    return sse - (uint32_t)((cast se * se) >> (wlog2 + hlog2));                \
  }                                                                            \
                                                                               \
  uint32_t aom_highbd_10_sub_pixel_avg_variance##w##x##h##_##opt(              \
      const uint8_t *src8, int src_stride, int x_offset, int y_offset,         \
      const uint8_t *dst8, int dst_stride, uint32_t *sse_ptr,                  \
      const uint8_t *sec8) {                                                   \
    int64_t var;                                                               \
    uint32_t sse;                                                              \
    uint16_t *src = CONVERT_TO_SHORTPTR(src8);                                 \
    uint16_t *dst = CONVERT_TO_SHORTPTR(dst8);                                 \
    uint16_t *sec = CONVERT_TO_SHORTPTR(sec8);                                 \
    int se = aom_highbd_sub_pixel_avg_variance##wf##xh_##opt(                  \
        src, src_stride, x_offset, y_offset, dst, dst_stride, sec, w, h, &sse, \
        NULL, NULL);                                                           \
    if (w > wf) {                                                              \
      uint32_t sse2;                                                           \
      int se2 = aom_highbd_sub_pixel_avg_variance##wf##xh_##opt(               \
          src + wf, src_stride, x_offset, y_offset, dst + wf, dst_stride,      \
          sec + wf, w, h, &sse2, NULL, NULL);                                  \
      se += se2;                                                               \
      sse += sse2;                                                             \
      if (w > wf * 2) {                                                        \
        se2 = aom_highbd_sub_pixel_avg_variance##wf##xh_##opt(                 \
            src + 2 * wf, src_stride, x_offset, y_offset, dst + 2 * wf,        \
            dst_stride, sec + 2 * wf, w, h, &sse2, NULL, NULL);                \
        se += se2;                                                             \
        sse += sse2;                                                           \
        se2 = aom_highbd_sub_pixel_avg_variance##wf##xh_##opt(                 \
            src + 3 * wf, src_stride, x_offset, y_offset, dst + 3 * wf,        \
            dst_stride, sec + 3 * wf, w, h, &sse2, NULL, NULL);                \
        se += se2;                                                             \
        sse += sse2;                                                           \
      }                                                                        \
    }                                                                          \
    se = ROUND_POWER_OF_TWO(se, 2);                                            \
    sse = ROUND_POWER_OF_TWO(sse, 4);                                          \
    *sse_ptr = sse;                                                            \
    var = (int64_t)(sse) - ((cast se * se) >> (wlog2 + hlog2));                \
    return (var >= 0) ? (uint32_t)var : 0;                                     \
  }                                                                            \
                                                                               \
  uint32_t aom_highbd_12_sub_pixel_avg_variance##w##x##h##_##opt(              \
      const uint8_t *src8, int src_stride, int x_offset, int y_offset,         \
      const uint8_t *dst8, int dst_stride, uint32_t *sse_ptr,                  \
      const uint8_t *sec8) {                                                   \
    int start_row;                                                             \
    int64_t var;                                                               \
    uint32_t sse;                                                              \
    int se = 0;                                                                \
    uint64_t long_sse = 0;                                                     \
    uint16_t *src = CONVERT_TO_SHORTPTR(src8);                                 \
    uint16_t *dst = CONVERT_TO_SHORTPTR(dst8);                                 \
    uint16_t *sec = CONVERT_TO_SHORTPTR(sec8);                                 \
    for (start_row = 0; start_row < h; start_row += 16) {                      \
      uint32_t sse2;                                                           \
      int height = h - start_row < 16 ? h - start_row : 16;                    \
      int se2 = aom_highbd_sub_pixel_avg_variance##wf##xh_##opt(               \
          src + (start_row * src_stride), src_stride, x_offset, y_offset,      \
          dst + (start_row * dst_stride), dst_stride, sec + (start_row * w),   \
          w, height, &sse2, NULL, NULL);                                       \
      se += se2;                                                               \
      long_sse += sse2;                                                        \
      if (w > wf) {                                                            \
        se2 = aom_highbd_sub_pixel_avg_variance##wf##xh_##opt(                 \
            src + wf + (start_row * src_stride), src_stride, x_offset,         \
            y_offset, dst + wf + (start_row * dst_stride), dst_stride,         \
            sec + wf + (start_row * w), w, height, &sse2, NULL, NULL);         \
        se += se2;                                                             \
        long_sse += sse2;                                                      \
        if (w > wf * 2) {                                                      \
          se2 = aom_highbd_sub_pixel_avg_variance##wf##xh_##opt(               \
              src + 2 * wf + (start_row * src_stride), src_stride, x_offset,   \
              y_offset, dst + 2 * wf + (start_row * dst_stride), dst_stride,   \
              sec + 2 * wf + (start_row * w), w, height, &sse2, NULL, NULL);   \
          se += se2;                                                           \
          long_sse += sse2;                                                    \
          se2 = aom_highbd_sub_pixel_avg_variance##wf##xh_##opt(               \
              src + 3 * wf + (start_row * src_stride), src_stride, x_offset,   \
              y_offset, dst + 3 * wf + (start_row * dst_stride), dst_stride,   \
              sec + 3 * wf + (start_row * w), w, height, &sse2, NULL, NULL);   \
          se += se2;                                                           \
          long_sse += sse2;                                                    \
        }                                                                      \
      }                                                                        \
    }                                                                          \
    se = ROUND_POWER_OF_TWO(se, 4);                                            \
    sse = (uint32_t)ROUND_POWER_OF_TWO(long_sse, 8);                           \
    *sse_ptr = sse;                                                            \
    var = (int64_t)(sse) - ((cast se * se) >> (wlog2 + hlog2));                \
    return (var >= 0) ? (uint32_t)var : 0;                                     \
  }

#if CONFIG_REALTIME_ONLY
#define FNS(opt)                       \
  FN(64, 64, 16, 6, 6, opt, (int64_t)) \
  FN(64, 32, 16, 6, 5, opt, (int64_t)) \
  FN(32, 64, 16, 5, 6, opt, (int64_t)) \
  FN(32, 32, 16, 5, 5, opt, (int64_t)) \
  FN(32, 16, 16, 5, 4, opt, (int64_t)) \
  FN(16, 32, 16, 4, 5, opt, (int64_t)) \
  FN(16, 16, 16, 4, 4, opt, (int64_t)) \
  FN(16, 8, 16, 4, 3, opt, (int64_t))  \
  FN(8, 16, 8, 3, 4, opt, (int64_t))   \
  FN(8, 8, 8, 3, 3, opt, (int64_t))    \
  FN(8, 4, 8, 3, 2, opt, (int64_t))
#else  // !CONFIG_REALTIME_ONLY
#define FNS(opt)                       \
  FN(64, 64, 16, 6, 6, opt, (int64_t)) \
  FN(64, 32, 16, 6, 5, opt, (int64_t)) \
  FN(32, 64, 16, 5, 6, opt, (int64_t)) \
  FN(32, 32, 16, 5, 5, opt, (int64_t)) \
  FN(32, 16, 16, 5, 4, opt, (int64_t)) \
  FN(16, 32, 16, 4, 5, opt, (int64_t)) \
  FN(16, 16, 16, 4, 4, opt, (int64_t)) \
  FN(16, 8, 16, 4, 3, opt, (int64_t))  \
  FN(8, 16, 8, 3, 4, opt, (int64_t))   \
  FN(8, 8, 8, 3, 3, opt, (int64_t))    \
  FN(8, 4, 8, 3, 2, opt, (int64_t))    \
  FN(16, 4, 16, 4, 2, opt, (int64_t))  \
  FN(8, 32, 8, 3, 5, opt, (int64_t))   \
  FN(32, 8, 16, 5, 3, opt, (int64_t))  \
  FN(16, 64, 16, 4, 6, opt, (int64_t)) \
  FN(64, 16, 16, 6, 4, opt, (int64_t))
#endif  // CONFIG_REALTIME_ONLY

FNS(sse2)

#undef FNS
#undef FN

static uint64_t mse_4xh_16bit_highbd_sse2(uint16_t *dst, int dstride,
                                          uint16_t *src, int sstride, int h) {
  uint64_t sum = 0;
  __m128i reg0_4x16, reg1_4x16;
  __m128i src_8x16;
  __m128i dst_8x16;
  __m128i res0_4x32, res1_4x32, res0_4x64, res1_4x64, res2_4x64, res3_4x64;
  __m128i sub_result_8x16;
  const __m128i zeros = _mm_setzero_si128();
  __m128i square_result = _mm_setzero_si128();
  for (int i = 0; i < h; i += 2) {
    reg0_4x16 = _mm_loadl_epi64((__m128i const *)(&dst[(i + 0) * dstride]));
    reg1_4x16 = _mm_loadl_epi64((__m128i const *)(&dst[(i + 1) * dstride]));
    dst_8x16 = _mm_unpacklo_epi64(reg0_4x16, reg1_4x16);

    reg0_4x16 = _mm_loadl_epi64((__m128i const *)(&src[(i + 0) * sstride]));
    reg1_4x16 = _mm_loadl_epi64((__m128i const *)(&src[(i + 1) * sstride]));
    src_8x16 = _mm_unpacklo_epi64(reg0_4x16, reg1_4x16);

    sub_result_8x16 = _mm_sub_epi16(src_8x16, dst_8x16);

    res0_4x32 = _mm_unpacklo_epi16(sub_result_8x16, zeros);
    res1_4x32 = _mm_unpackhi_epi16(sub_result_8x16, zeros);

    res0_4x32 = _mm_madd_epi16(res0_4x32, res0_4x32);
    res1_4x32 = _mm_madd_epi16(res1_4x32, res1_4x32);

    res0_4x64 = _mm_unpacklo_epi32(res0_4x32, zeros);
    res1_4x64 = _mm_unpackhi_epi32(res0_4x32, zeros);
    res2_4x64 = _mm_unpacklo_epi32(res1_4x32, zeros);
    res3_4x64 = _mm_unpackhi_epi32(res1_4x32, zeros);

    square_result = _mm_add_epi64(
        square_result,
        _mm_add_epi64(
            _mm_add_epi64(_mm_add_epi64(res0_4x64, res1_4x64), res2_4x64),
            res3_4x64));
  }

  const __m128i sum_1x64 =
      _mm_add_epi64(square_result, _mm_srli_si128(square_result, 8));
  xx_storel_64(&sum, sum_1x64);
  return sum;
}

static uint64_t mse_8xh_16bit_highbd_sse2(uint16_t *dst, int dstride,
                                          uint16_t *src, int sstride, int h) {
  uint64_t sum = 0;
  __m128i src_8x16;
  __m128i dst_8x16;
  __m128i res0_4x32, res1_4x32, res0_4x64, res1_4x64, res2_4x64, res3_4x64;
  __m128i sub_result_8x16;
  const __m128i zeros = _mm_setzero_si128();
  __m128i square_result = _mm_setzero_si128();

  for (int i = 0; i < h; i++) {
    dst_8x16 = _mm_loadu_si128((__m128i *)&dst[i * dstride]);
    src_8x16 = _mm_loadu_si128((__m128i *)&src[i * sstride]);

    sub_result_8x16 = _mm_sub_epi16(src_8x16, dst_8x16);

    res0_4x32 = _mm_unpacklo_epi16(sub_result_8x16, zeros);
    res1_4x32 = _mm_unpackhi_epi16(sub_result_8x16, zeros);

    res0_4x32 = _mm_madd_epi16(res0_4x32, res0_4x32);
    res1_4x32 = _mm_madd_epi16(res1_4x32, res1_4x32);

    res0_4x64 = _mm_unpacklo_epi32(res0_4x32, zeros);
    res1_4x64 = _mm_unpackhi_epi32(res0_4x32, zeros);
    res2_4x64 = _mm_unpacklo_epi32(res1_4x32, zeros);
    res3_4x64 = _mm_unpackhi_epi32(res1_4x32, zeros);

    square_result = _mm_add_epi64(
        square_result,
        _mm_add_epi64(
            _mm_add_epi64(_mm_add_epi64(res0_4x64, res1_4x64), res2_4x64),
            res3_4x64));
  }

  const __m128i sum_1x64 =
      _mm_add_epi64(square_result, _mm_srli_si128(square_result, 8));
  xx_storel_64(&sum, sum_1x64);
  return sum;
}

uint64_t aom_mse_wxh_16bit_highbd_sse2(uint16_t *dst, int dstride,
                                       uint16_t *src, int sstride, int w,
                                       int h) {
  assert((w == 8 || w == 4) && (h == 8 || h == 4) &&
         "w=8/4 and h=8/4 must satisfy");
  switch (w) {
    case 4: return mse_4xh_16bit_highbd_sse2(dst, dstride, src, sstride, h);
    case 8: return mse_8xh_16bit_highbd_sse2(dst, dstride, src, sstride, h);
    default: assert(0 && "unsupported width"); return -1;
  }
}
