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

#include <stddef.h>
#include <stdint.h>

#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"

#include "aom_dsp/aom_dsp_common.h"

// The 2 unused parameters are place holders for PIC enabled build.
// These definitions are for functions defined in subpel_variance.asm
#define DECL(w, opt)                                                           \
  int aom_sub_pixel_variance##w##xh_##opt(                                     \
      const uint8_t *src, ptrdiff_t src_stride, int x_offset, int y_offset,    \
      const uint8_t *dst, ptrdiff_t dst_stride, int height, unsigned int *sse, \
      void *unused0, void *unused)
#define DECLS(opt) \
  DECL(4, opt);    \
  DECL(8, opt);    \
  DECL(16, opt)

DECLS(ssse3);
#undef DECLS
#undef DECL

#define FN(w, h, wf, wlog2, hlog2, opt, cast_prod, cast)                      \
  unsigned int aom_sub_pixel_variance##w##x##h##_##opt(                       \
      const uint8_t *src, int src_stride, int x_offset, int y_offset,         \
      const uint8_t *dst, int dst_stride, unsigned int *sse_ptr) {            \
    /*Avoid overflow in helper by capping height.*/                           \
    const int hf = AOMMIN(h, 64);                                             \
    unsigned int sse = 0;                                                     \
    int se = 0;                                                               \
    for (int i = 0; i < (w / wf); ++i) {                                      \
      const uint8_t *src_ptr = src;                                           \
      const uint8_t *dst_ptr = dst;                                           \
      for (int j = 0; j < (h / hf); ++j) {                                    \
        unsigned int sse2;                                                    \
        const int se2 = aom_sub_pixel_variance##wf##xh_##opt(                 \
            src_ptr, src_stride, x_offset, y_offset, dst_ptr, dst_stride, hf, \
            &sse2, NULL, NULL);                                               \
        dst_ptr += hf * dst_stride;                                           \
        src_ptr += hf * src_stride;                                           \
        se += se2;                                                            \
        sse += sse2;                                                          \
      }                                                                       \
      src += wf;                                                              \
      dst += wf;                                                              \
    }                                                                         \
    *sse_ptr = sse;                                                           \
    return sse - (unsigned int)(cast_prod(cast se * se) >> (wlog2 + hlog2));  \
  }

#if !CONFIG_REALTIME_ONLY
#define FNS(opt)                                    \
  FN(128, 128, 16, 7, 7, opt, (int64_t), (int64_t)) \
  FN(128, 64, 16, 7, 6, opt, (int64_t), (int64_t))  \
  FN(64, 128, 16, 6, 7, opt, (int64_t), (int64_t))  \
  FN(64, 64, 16, 6, 6, opt, (int64_t), (int64_t))   \
  FN(64, 32, 16, 6, 5, opt, (int64_t), (int64_t))   \
  FN(32, 64, 16, 5, 6, opt, (int64_t), (int64_t))   \
  FN(32, 32, 16, 5, 5, opt, (int64_t), (int64_t))   \
  FN(32, 16, 16, 5, 4, opt, (int64_t), (int64_t))   \
  FN(16, 32, 16, 4, 5, opt, (int64_t), (int64_t))   \
  FN(16, 16, 16, 4, 4, opt, (uint32_t), (int64_t))  \
  FN(16, 8, 16, 4, 3, opt, (int32_t), (int32_t))    \
  FN(8, 16, 8, 3, 4, opt, (int32_t), (int32_t))     \
  FN(8, 8, 8, 3, 3, opt, (int32_t), (int32_t))      \
  FN(8, 4, 8, 3, 2, opt, (int32_t), (int32_t))      \
  FN(4, 8, 4, 2, 3, opt, (int32_t), (int32_t))      \
  FN(4, 4, 4, 2, 2, opt, (int32_t), (int32_t))      \
  FN(4, 16, 4, 2, 4, opt, (int32_t), (int32_t))     \
  FN(16, 4, 16, 4, 2, opt, (int32_t), (int32_t))    \
  FN(8, 32, 8, 3, 5, opt, (uint32_t), (int64_t))    \
  FN(32, 8, 16, 5, 3, opt, (uint32_t), (int64_t))   \
  FN(16, 64, 16, 4, 6, opt, (int64_t), (int64_t))   \
  FN(64, 16, 16, 6, 4, opt, (int64_t), (int64_t))
#else
#define FNS(opt)                                    \
  FN(128, 128, 16, 7, 7, opt, (int64_t), (int64_t)) \
  FN(128, 64, 16, 7, 6, opt, (int64_t), (int64_t))  \
  FN(64, 128, 16, 6, 7, opt, (int64_t), (int64_t))  \
  FN(64, 64, 16, 6, 6, opt, (int64_t), (int64_t))   \
  FN(64, 32, 16, 6, 5, opt, (int64_t), (int64_t))   \
  FN(32, 64, 16, 5, 6, opt, (int64_t), (int64_t))   \
  FN(32, 32, 16, 5, 5, opt, (int64_t), (int64_t))   \
  FN(32, 16, 16, 5, 4, opt, (int64_t), (int64_t))   \
  FN(16, 32, 16, 4, 5, opt, (int64_t), (int64_t))   \
  FN(16, 16, 16, 4, 4, opt, (uint32_t), (int64_t))  \
  FN(16, 8, 16, 4, 3, opt, (int32_t), (int32_t))    \
  FN(8, 16, 8, 3, 4, opt, (int32_t), (int32_t))     \
  FN(8, 8, 8, 3, 3, opt, (int32_t), (int32_t))      \
  FN(8, 4, 8, 3, 2, opt, (int32_t), (int32_t))      \
  FN(4, 8, 4, 2, 3, opt, (int32_t), (int32_t))      \
  FN(4, 4, 4, 2, 2, opt, (int32_t), (int32_t))
#endif

FNS(ssse3)

#undef FNS
#undef FN

// The 2 unused parameters are place holders for PIC enabled build.
#define DECL(w, opt)                                                        \
  int aom_sub_pixel_avg_variance##w##xh_##opt(                              \
      const uint8_t *src, ptrdiff_t src_stride, int x_offset, int y_offset, \
      const uint8_t *dst, ptrdiff_t dst_stride, const uint8_t *sec,         \
      ptrdiff_t sec_stride, int height, unsigned int *sse, void *unused0,   \
      void *unused)
#define DECLS(opt) \
  DECL(4, opt);    \
  DECL(8, opt);    \
  DECL(16, opt)

DECLS(ssse3);
#undef DECL
#undef DECLS

#define FN(w, h, wf, wlog2, hlog2, opt, cast_prod, cast)                     \
  unsigned int aom_sub_pixel_avg_variance##w##x##h##_##opt(                  \
      const uint8_t *src, int src_stride, int x_offset, int y_offset,        \
      const uint8_t *dst, int dst_stride, unsigned int *sse_ptr,             \
      const uint8_t *sec) {                                                  \
    /*Avoid overflow in helper by capping height.*/                          \
    const int hf = AOMMIN(h, 64);                                            \
    unsigned int sse = 0;                                                    \
    int se = 0;                                                              \
    for (int i = 0; i < (w / wf); ++i) {                                     \
      const uint8_t *src_ptr = src;                                          \
      const uint8_t *dst_ptr = dst;                                          \
      const uint8_t *sec_ptr = sec;                                          \
      for (int j = 0; j < (h / hf); ++j) {                                   \
        unsigned int sse2;                                                   \
        const int se2 = aom_sub_pixel_avg_variance##wf##xh_##opt(            \
            src_ptr, src_stride, x_offset, y_offset, dst_ptr, dst_stride,    \
            sec_ptr, w, hf, &sse2, NULL, NULL);                              \
        dst_ptr += hf * dst_stride;                                          \
        src_ptr += hf * src_stride;                                          \
        sec_ptr += hf * w;                                                   \
        se += se2;                                                           \
        sse += sse2;                                                         \
      }                                                                      \
      src += wf;                                                             \
      dst += wf;                                                             \
      sec += wf;                                                             \
    }                                                                        \
    *sse_ptr = sse;                                                          \
    return sse - (unsigned int)(cast_prod(cast se * se) >> (wlog2 + hlog2)); \
  }

#if !CONFIG_REALTIME_ONLY
#define FNS(opt)                                    \
  FN(128, 128, 16, 7, 7, opt, (int64_t), (int64_t)) \
  FN(128, 64, 16, 7, 6, opt, (int64_t), (int64_t))  \
  FN(64, 128, 16, 6, 7, opt, (int64_t), (int64_t))  \
  FN(64, 64, 16, 6, 6, opt, (int64_t), (int64_t))   \
  FN(64, 32, 16, 6, 5, opt, (int64_t), (int64_t))   \
  FN(32, 64, 16, 5, 6, opt, (int64_t), (int64_t))   \
  FN(32, 32, 16, 5, 5, opt, (int64_t), (int64_t))   \
  FN(32, 16, 16, 5, 4, opt, (int64_t), (int64_t))   \
  FN(16, 32, 16, 4, 5, opt, (int64_t), (int64_t))   \
  FN(16, 16, 16, 4, 4, opt, (uint32_t), (int64_t))  \
  FN(16, 8, 16, 4, 3, opt, (uint32_t), (int32_t))   \
  FN(8, 16, 8, 3, 4, opt, (uint32_t), (int32_t))    \
  FN(8, 8, 8, 3, 3, opt, (uint32_t), (int32_t))     \
  FN(8, 4, 8, 3, 2, opt, (uint32_t), (int32_t))     \
  FN(4, 8, 4, 2, 3, opt, (uint32_t), (int32_t))     \
  FN(4, 4, 4, 2, 2, opt, (uint32_t), (int32_t))     \
  FN(4, 16, 4, 2, 4, opt, (int32_t), (int32_t))     \
  FN(16, 4, 16, 4, 2, opt, (int32_t), (int32_t))    \
  FN(8, 32, 8, 3, 5, opt, (uint32_t), (int64_t))    \
  FN(32, 8, 16, 5, 3, opt, (uint32_t), (int64_t))   \
  FN(16, 64, 16, 4, 6, opt, (int64_t), (int64_t))   \
  FN(64, 16, 16, 6, 4, opt, (int64_t), (int64_t))
#else
#define FNS(opt)                                    \
  FN(128, 128, 16, 7, 7, opt, (int64_t), (int64_t)) \
  FN(128, 64, 16, 7, 6, opt, (int64_t), (int64_t))  \
  FN(64, 128, 16, 6, 7, opt, (int64_t), (int64_t))  \
  FN(64, 64, 16, 6, 6, opt, (int64_t), (int64_t))   \
  FN(64, 32, 16, 6, 5, opt, (int64_t), (int64_t))   \
  FN(32, 64, 16, 5, 6, opt, (int64_t), (int64_t))   \
  FN(32, 32, 16, 5, 5, opt, (int64_t), (int64_t))   \
  FN(32, 16, 16, 5, 4, opt, (int64_t), (int64_t))   \
  FN(16, 32, 16, 4, 5, opt, (int64_t), (int64_t))   \
  FN(16, 16, 16, 4, 4, opt, (uint32_t), (int64_t))  \
  FN(16, 8, 16, 4, 3, opt, (uint32_t), (int32_t))   \
  FN(8, 16, 8, 3, 4, opt, (uint32_t), (int32_t))    \
  FN(8, 8, 8, 3, 3, opt, (uint32_t), (int32_t))     \
  FN(8, 4, 8, 3, 2, opt, (uint32_t), (int32_t))     \
  FN(4, 8, 4, 2, 3, opt, (uint32_t), (int32_t))     \
  FN(4, 4, 4, 2, 2, opt, (uint32_t), (int32_t))
#endif

FNS(ssse3)

#undef FNS
#undef FN
