/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <assert.h>
#include <stdio.h>

#include "config/aom_dsp_rtcd.h"

#include "aom_dsp/mips/convolve_common_dspr2.h"
#include "aom_dsp/aom_dsp_common.h"
#include "aom_dsp/aom_filter.h"
#include "aom_ports/mem.h"

#if HAVE_DSPR2
void aom_convolve_copy_dspr2(const uint8_t *src, ptrdiff_t src_stride,
                             uint8_t *dst, ptrdiff_t dst_stride, int w, int h) {
  int x, y;

  /* prefetch data to cache memory */
  prefetch_load(src);
  prefetch_load(src + 32);
  prefetch_store(dst);

  switch (w) {
    case 4: {
      uint32_t tp1;

      /* 1 word storage */
      for (y = h; y--;) {
        prefetch_load(src + src_stride);
        prefetch_load(src + src_stride + 32);
        prefetch_store(dst + dst_stride);

        __asm__ __volatile__(
            "ulw              %[tp1],         (%[src])      \n\t"
            "sw               %[tp1],         (%[dst])      \n\t" /* store */

            : [tp1] "=&r"(tp1)
            : [src] "r"(src), [dst] "r"(dst));

        src += src_stride;
        dst += dst_stride;
      }
    } break;
    case 8: {
      uint32_t tp1, tp2;

      /* 2 word storage */
      for (y = h; y--;) {
        prefetch_load(src + src_stride);
        prefetch_load(src + src_stride + 32);
        prefetch_store(dst + dst_stride);

        __asm__ __volatile__(
            "ulw              %[tp1],         0(%[src])      \n\t"
            "ulw              %[tp2],         4(%[src])      \n\t"
            "sw               %[tp1],         0(%[dst])      \n\t" /* store */
            "sw               %[tp2],         4(%[dst])      \n\t" /* store */

            : [tp1] "=&r"(tp1), [tp2] "=&r"(tp2)
            : [src] "r"(src), [dst] "r"(dst));

        src += src_stride;
        dst += dst_stride;
      }
    } break;
    case 16: {
      uint32_t tp1, tp2, tp3, tp4;

      /* 4 word storage */
      for (y = h; y--;) {
        prefetch_load(src + src_stride);
        prefetch_load(src + src_stride + 32);
        prefetch_store(dst + dst_stride);

        __asm__ __volatile__(
            "ulw              %[tp1],         0(%[src])      \n\t"
            "ulw              %[tp2],         4(%[src])      \n\t"
            "ulw              %[tp3],         8(%[src])      \n\t"
            "ulw              %[tp4],         12(%[src])     \n\t"

            "sw               %[tp1],         0(%[dst])      \n\t" /* store */
            "sw               %[tp2],         4(%[dst])      \n\t" /* store */
            "sw               %[tp3],         8(%[dst])      \n\t" /* store */
            "sw               %[tp4],         12(%[dst])     \n\t" /* store */

            : [tp1] "=&r"(tp1), [tp2] "=&r"(tp2), [tp3] "=&r"(tp3),
              [tp4] "=&r"(tp4)
            : [src] "r"(src), [dst] "r"(dst));

        src += src_stride;
        dst += dst_stride;
      }
    } break;
    case 32: {
      uint32_t tp1, tp2, tp3, tp4;
      uint32_t tp5, tp6, tp7, tp8;

      /* 8 word storage */
      for (y = h; y--;) {
        prefetch_load(src + src_stride);
        prefetch_load(src + src_stride + 32);
        prefetch_store(dst + dst_stride);

        __asm__ __volatile__(
            "ulw              %[tp1],         0(%[src])      \n\t"
            "ulw              %[tp2],         4(%[src])      \n\t"
            "ulw              %[tp3],         8(%[src])      \n\t"
            "ulw              %[tp4],         12(%[src])     \n\t"
            "ulw              %[tp5],         16(%[src])     \n\t"
            "ulw              %[tp6],         20(%[src])     \n\t"
            "ulw              %[tp7],         24(%[src])     \n\t"
            "ulw              %[tp8],         28(%[src])     \n\t"

            "sw               %[tp1],         0(%[dst])      \n\t" /* store */
            "sw               %[tp2],         4(%[dst])      \n\t" /* store */
            "sw               %[tp3],         8(%[dst])      \n\t" /* store */
            "sw               %[tp4],         12(%[dst])     \n\t" /* store */
            "sw               %[tp5],         16(%[dst])     \n\t" /* store */
            "sw               %[tp6],         20(%[dst])     \n\t" /* store */
            "sw               %[tp7],         24(%[dst])     \n\t" /* store */
            "sw               %[tp8],         28(%[dst])     \n\t" /* store */

            : [tp1] "=&r"(tp1), [tp2] "=&r"(tp2), [tp3] "=&r"(tp3),
              [tp4] "=&r"(tp4), [tp5] "=&r"(tp5), [tp6] "=&r"(tp6),
              [tp7] "=&r"(tp7), [tp8] "=&r"(tp8)
            : [src] "r"(src), [dst] "r"(dst));

        src += src_stride;
        dst += dst_stride;
      }
    } break;
    case 64: {
      uint32_t tp1, tp2, tp3, tp4;
      uint32_t tp5, tp6, tp7, tp8;

      prefetch_load(src + 64);
      prefetch_store(dst + 32);

      /* 16 word storage */
      for (y = h; y--;) {
        prefetch_load(src + src_stride);
        prefetch_load(src + src_stride + 32);
        prefetch_load(src + src_stride + 64);
        prefetch_store(dst + dst_stride);
        prefetch_store(dst + dst_stride + 32);

        __asm__ __volatile__(
            "ulw              %[tp1],         0(%[src])      \n\t"
            "ulw              %[tp2],         4(%[src])      \n\t"
            "ulw              %[tp3],         8(%[src])      \n\t"
            "ulw              %[tp4],         12(%[src])     \n\t"
            "ulw              %[tp5],         16(%[src])     \n\t"
            "ulw              %[tp6],         20(%[src])     \n\t"
            "ulw              %[tp7],         24(%[src])     \n\t"
            "ulw              %[tp8],         28(%[src])     \n\t"

            "sw               %[tp1],         0(%[dst])      \n\t" /* store */
            "sw               %[tp2],         4(%[dst])      \n\t" /* store */
            "sw               %[tp3],         8(%[dst])      \n\t" /* store */
            "sw               %[tp4],         12(%[dst])     \n\t" /* store */
            "sw               %[tp5],         16(%[dst])     \n\t" /* store */
            "sw               %[tp6],         20(%[dst])     \n\t" /* store */
            "sw               %[tp7],         24(%[dst])     \n\t" /* store */
            "sw               %[tp8],         28(%[dst])     \n\t" /* store */

            "ulw              %[tp1],         32(%[src])     \n\t"
            "ulw              %[tp2],         36(%[src])     \n\t"
            "ulw              %[tp3],         40(%[src])     \n\t"
            "ulw              %[tp4],         44(%[src])     \n\t"
            "ulw              %[tp5],         48(%[src])     \n\t"
            "ulw              %[tp6],         52(%[src])     \n\t"
            "ulw              %[tp7],         56(%[src])     \n\t"
            "ulw              %[tp8],         60(%[src])     \n\t"

            "sw               %[tp1],         32(%[dst])     \n\t" /* store */
            "sw               %[tp2],         36(%[dst])     \n\t" /* store */
            "sw               %[tp3],         40(%[dst])     \n\t" /* store */
            "sw               %[tp4],         44(%[dst])     \n\t" /* store */
            "sw               %[tp5],         48(%[dst])     \n\t" /* store */
            "sw               %[tp6],         52(%[dst])     \n\t" /* store */
            "sw               %[tp7],         56(%[dst])     \n\t" /* store */
            "sw               %[tp8],         60(%[dst])     \n\t" /* store */

            : [tp1] "=&r"(tp1), [tp2] "=&r"(tp2), [tp3] "=&r"(tp3),
              [tp4] "=&r"(tp4), [tp5] "=&r"(tp5), [tp6] "=&r"(tp6),
              [tp7] "=&r"(tp7), [tp8] "=&r"(tp8)
            : [src] "r"(src), [dst] "r"(dst));

        src += src_stride;
        dst += dst_stride;
      }
    } break;
    default:
      for (y = h; y--;) {
        for (x = 0; x < w; ++x) {
          dst[x] = src[x];
        }

        src += src_stride;
        dst += dst_stride;
      }
      break;
  }
}
#endif
