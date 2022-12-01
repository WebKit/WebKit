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

#ifndef AOM_AOM_DSP_MIPS_LOOPFILTER_MASKS_DSPR2_H_
#define AOM_AOM_DSP_MIPS_LOOPFILTER_MASKS_DSPR2_H_

#include <stdlib.h>

#include "config/aom_dsp_rtcd.h"

#include "aom/aom_integer.h"
#include "aom_mem/aom_mem.h"

#ifdef __cplusplus
extern "C" {
#endif

#if HAVE_DSPR2
/* processing 4 pixels at the same time
 * compute hev and mask in the same function */
static INLINE void filter_hev_mask_dspr2(uint32_t limit, uint32_t flimit,
                                         uint32_t p1, uint32_t p0, uint32_t p3,
                                         uint32_t p2, uint32_t q0, uint32_t q1,
                                         uint32_t q2, uint32_t q3,
                                         uint32_t thresh, uint32_t *hev,
                                         uint32_t *mask) {
  uint32_t c, r, r3, r_k;
  uint32_t s1, s2, s3;
  uint32_t ones = 0xFFFFFFFF;
  uint32_t hev1;

  __asm__ __volatile__(
      /* mask |= (abs(p3 - p2) > limit) */
      "subu_s.qb      %[c],   %[p3],     %[p2]        \n\t"
      "subu_s.qb      %[r_k], %[p2],     %[p3]        \n\t"
      "or             %[r_k], %[r_k],    %[c]         \n\t"
      "cmpgu.lt.qb    %[c],   %[limit],  %[r_k]       \n\t"
      "or             %[r],   $0,        %[c]         \n\t"

      /* mask |= (abs(p2 - p1) > limit) */
      "subu_s.qb      %[c],   %[p2],     %[p1]        \n\t"
      "subu_s.qb      %[r_k], %[p1],     %[p2]        \n\t"
      "or             %[r_k], %[r_k],    %[c]         \n\t"
      "cmpgu.lt.qb    %[c],   %[limit],  %[r_k]       \n\t"
      "or             %[r],   %[r],      %[c]         \n\t"

      /* mask |= (abs(p1 - p0) > limit)
       * hev  |= (abs(p1 - p0) > thresh)
       */
      "subu_s.qb      %[c],   %[p1],     %[p0]        \n\t"
      "subu_s.qb      %[r_k], %[p0],     %[p1]        \n\t"
      "or             %[r_k], %[r_k],    %[c]         \n\t"
      "cmpgu.lt.qb    %[c],   %[thresh], %[r_k]       \n\t"
      "or             %[r3],  $0,        %[c]         \n\t"
      "cmpgu.lt.qb    %[c],   %[limit],  %[r_k]       \n\t"
      "or             %[r],   %[r],      %[c]         \n\t"

      /* mask |= (abs(q1 - q0) > limit)
       * hev  |= (abs(q1 - q0) > thresh)
       */
      "subu_s.qb      %[c],   %[q1],     %[q0]        \n\t"
      "subu_s.qb      %[r_k], %[q0],     %[q1]        \n\t"
      "or             %[r_k], %[r_k],    %[c]         \n\t"
      "cmpgu.lt.qb    %[c],   %[thresh], %[r_k]       \n\t"
      "or             %[r3],  %[r3],     %[c]         \n\t"
      "cmpgu.lt.qb    %[c],   %[limit],  %[r_k]       \n\t"
      "or             %[r],   %[r],      %[c]         \n\t"

      /* mask |= (abs(q2 - q1) > limit) */
      "subu_s.qb      %[c],   %[q2],     %[q1]        \n\t"
      "subu_s.qb      %[r_k], %[q1],     %[q2]        \n\t"
      "or             %[r_k], %[r_k],    %[c]         \n\t"
      "cmpgu.lt.qb    %[c],   %[limit],  %[r_k]       \n\t"
      "or             %[r],   %[r],      %[c]         \n\t"
      "sll            %[r3],    %[r3],    24          \n\t"

      /* mask |= (abs(q3 - q2) > limit) */
      "subu_s.qb      %[c],   %[q3],     %[q2]        \n\t"
      "subu_s.qb      %[r_k], %[q2],     %[q3]        \n\t"
      "or             %[r_k], %[r_k],    %[c]         \n\t"
      "cmpgu.lt.qb    %[c],   %[limit],  %[r_k]       \n\t"
      "or             %[r],   %[r],      %[c]         \n\t"

      : [c] "=&r"(c), [r_k] "=&r"(r_k), [r] "=&r"(r), [r3] "=&r"(r3)
      : [limit] "r"(limit), [p3] "r"(p3), [p2] "r"(p2), [p1] "r"(p1),
        [p0] "r"(p0), [q1] "r"(q1), [q0] "r"(q0), [q2] "r"(q2), [q3] "r"(q3),
        [thresh] "r"(thresh));

  __asm__ __volatile__(
      /* abs(p0 - q0) */
      "subu_s.qb      %[c],   %[p0],     %[q0]        \n\t"
      "subu_s.qb      %[r_k], %[q0],     %[p0]        \n\t"
      "wrdsp          %[r3]                           \n\t"
      "or             %[s1],  %[r_k],    %[c]         \n\t"

      /* abs(p1 - q1) */
      "subu_s.qb      %[c],    %[p1],    %[q1]        \n\t"
      "addu_s.qb      %[s3],   %[s1],    %[s1]        \n\t"
      "pick.qb        %[hev1], %[ones],  $0           \n\t"
      "subu_s.qb      %[r_k],  %[q1],    %[p1]        \n\t"
      "or             %[s2],   %[r_k],   %[c]         \n\t"

      /* abs(p0 - q0) * 2 + abs(p1 - q1) / 2  > flimit * 2 + limit */
      "shrl.qb        %[s2],   %[s2],     1           \n\t"
      "addu_s.qb      %[s1],   %[s2],     %[s3]       \n\t"
      "cmpgu.lt.qb    %[c],    %[flimit], %[s1]       \n\t"
      "or             %[r],    %[r],      %[c]        \n\t"
      "sll            %[r],    %[r],      24          \n\t"

      "wrdsp          %[r]                            \n\t"
      "pick.qb        %[s2],  $0,         %[ones]     \n\t"

      : [c] "=&r"(c), [r_k] "=&r"(r_k), [s1] "=&r"(s1), [hev1] "=&r"(hev1),
        [s2] "=&r"(s2), [r] "+r"(r), [s3] "=&r"(s3)
      : [p0] "r"(p0), [q0] "r"(q0), [p1] "r"(p1), [r3] "r"(r3), [q1] "r"(q1),
        [ones] "r"(ones), [flimit] "r"(flimit));

  *hev = hev1;
  *mask = s2;
}

static INLINE void filter_hev_mask_flatmask4_dspr2(
    uint32_t limit, uint32_t flimit, uint32_t thresh, uint32_t p1, uint32_t p0,
    uint32_t p3, uint32_t p2, uint32_t q0, uint32_t q1, uint32_t q2,
    uint32_t q3, uint32_t *hev, uint32_t *mask, uint32_t *flat) {
  uint32_t c, r, r3, r_k, r_flat;
  uint32_t s1, s2, s3;
  uint32_t ones = 0xFFFFFFFF;
  uint32_t flat_thresh = 0x01010101;
  uint32_t hev1;
  uint32_t flat1;

  __asm__ __volatile__(
      /* mask |= (abs(p3 - p2) > limit) */
      "subu_s.qb      %[c],       %[p3],          %[p2]        \n\t"
      "subu_s.qb      %[r_k],     %[p2],          %[p3]        \n\t"
      "or             %[r_k],     %[r_k],         %[c]         \n\t"
      "cmpgu.lt.qb    %[c],       %[limit],       %[r_k]       \n\t"
      "or             %[r],       $0,             %[c]         \n\t"

      /* mask |= (abs(p2 - p1) > limit) */
      "subu_s.qb      %[c],       %[p2],          %[p1]        \n\t"
      "subu_s.qb      %[r_k],     %[p1],          %[p2]        \n\t"
      "or             %[r_k],     %[r_k],         %[c]         \n\t"
      "cmpgu.lt.qb    %[c],       %[limit],       %[r_k]       \n\t"
      "or             %[r],       %[r],           %[c]         \n\t"

      /* mask |= (abs(p1 - p0) > limit)
       * hev  |= (abs(p1 - p0) > thresh)
       * flat |= (abs(p1 - p0) > thresh)
       */
      "subu_s.qb      %[c],       %[p1],          %[p0]        \n\t"
      "subu_s.qb      %[r_k],     %[p0],          %[p1]        \n\t"
      "or             %[r_k],     %[r_k],         %[c]         \n\t"
      "cmpgu.lt.qb    %[c],       %[thresh],      %[r_k]       \n\t"
      "or             %[r3],      $0,             %[c]         \n\t"
      "cmpgu.lt.qb    %[c],       %[limit],       %[r_k]       \n\t"
      "or             %[r],       %[r],           %[c]         \n\t"
      "cmpgu.lt.qb    %[c],       %[flat_thresh], %[r_k]       \n\t"
      "or             %[r_flat],  $0,             %[c]         \n\t"

      /* mask |= (abs(q1 - q0) > limit)
       * hev  |= (abs(q1 - q0) > thresh)
       * flat |= (abs(q1 - q0) > thresh)
       */
      "subu_s.qb      %[c],       %[q1],          %[q0]        \n\t"
      "subu_s.qb      %[r_k],     %[q0],          %[q1]        \n\t"
      "or             %[r_k],     %[r_k],         %[c]         \n\t"
      "cmpgu.lt.qb    %[c],       %[thresh],      %[r_k]       \n\t"
      "or             %[r3],      %[r3],          %[c]         \n\t"
      "cmpgu.lt.qb    %[c],       %[limit],       %[r_k]       \n\t"
      "or             %[r],       %[r],           %[c]         \n\t"
      "cmpgu.lt.qb    %[c],       %[flat_thresh], %[r_k]       \n\t"
      "or             %[r_flat],  %[r_flat],      %[c]         \n\t"

      /* flat |= (abs(p0 - p2) > thresh) */
      "subu_s.qb      %[c],       %[p0],          %[p2]        \n\t"
      "subu_s.qb      %[r_k],     %[p2],          %[p0]        \n\t"
      "or             %[r_k],     %[r_k],         %[c]         \n\t"
      "cmpgu.lt.qb    %[c],       %[flat_thresh], %[r_k]       \n\t"
      "or             %[r_flat],  %[r_flat],      %[c]         \n\t"

      /* flat |= (abs(q0 - q2) > thresh) */
      "subu_s.qb      %[c],       %[q0],          %[q2]        \n\t"
      "subu_s.qb      %[r_k],     %[q2],          %[q0]        \n\t"
      "or             %[r_k],     %[r_k],         %[c]         \n\t"
      "cmpgu.lt.qb    %[c],       %[flat_thresh], %[r_k]       \n\t"
      "or             %[r_flat],  %[r_flat],      %[c]         \n\t"

      /* flat |= (abs(p3 - p0) > thresh) */
      "subu_s.qb      %[c],       %[p3],          %[p0]        \n\t"
      "subu_s.qb      %[r_k],     %[p0],          %[p3]        \n\t"
      "or             %[r_k],     %[r_k],         %[c]         \n\t"
      "cmpgu.lt.qb    %[c],       %[flat_thresh], %[r_k]       \n\t"
      "or             %[r_flat],  %[r_flat],      %[c]         \n\t"

      /* flat |= (abs(q3 - q0) > thresh) */
      "subu_s.qb      %[c],       %[q3],          %[q0]        \n\t"
      "subu_s.qb      %[r_k],     %[q0],          %[q3]        \n\t"
      "or             %[r_k],     %[r_k],         %[c]         \n\t"
      "cmpgu.lt.qb    %[c],       %[flat_thresh], %[r_k]       \n\t"
      "or             %[r_flat],  %[r_flat],      %[c]         \n\t"
      "sll            %[r_flat],  %[r_flat],      24           \n\t"
      /* look at stall here */
      "wrdsp          %[r_flat]                                \n\t"
      "pick.qb        %[flat1],   $0,             %[ones]      \n\t"

      /* mask |= (abs(q2 - q1) > limit) */
      "subu_s.qb      %[c],       %[q2],          %[q1]        \n\t"
      "subu_s.qb      %[r_k],     %[q1],          %[q2]        \n\t"
      "or             %[r_k],     %[r_k],         %[c]         \n\t"
      "cmpgu.lt.qb    %[c],       %[limit],       %[r_k]       \n\t"
      "or             %[r],       %[r],           %[c]         \n\t"
      "sll            %[r3],      %[r3],          24           \n\t"

      /* mask |= (abs(q3 - q2) > limit) */
      "subu_s.qb      %[c],       %[q3],          %[q2]        \n\t"
      "subu_s.qb      %[r_k],     %[q2],          %[q3]        \n\t"
      "or             %[r_k],     %[r_k],         %[c]         \n\t"
      "cmpgu.lt.qb    %[c],       %[limit],       %[r_k]       \n\t"
      "or             %[r],       %[r],           %[c]         \n\t"

      : [c] "=&r"(c), [r_k] "=&r"(r_k), [r] "=&r"(r), [r3] "=&r"(r3),
        [r_flat] "=&r"(r_flat), [flat1] "=&r"(flat1)
      : [limit] "r"(limit), [p3] "r"(p3), [p2] "r"(p2), [p1] "r"(p1),
        [p0] "r"(p0), [q1] "r"(q1), [q0] "r"(q0), [q2] "r"(q2), [q3] "r"(q3),
        [thresh] "r"(thresh), [flat_thresh] "r"(flat_thresh), [ones] "r"(ones));

  __asm__ __volatile__(
      /* abs(p0 - q0) */
      "subu_s.qb      %[c],   %[p0],     %[q0]        \n\t"
      "subu_s.qb      %[r_k], %[q0],     %[p0]        \n\t"
      "wrdsp          %[r3]                           \n\t"
      "or             %[s1],  %[r_k],    %[c]         \n\t"

      /* abs(p1 - q1) */
      "subu_s.qb      %[c],    %[p1],    %[q1]        \n\t"
      "addu_s.qb      %[s3],   %[s1],    %[s1]        \n\t"
      "pick.qb        %[hev1], %[ones],  $0           \n\t"
      "subu_s.qb      %[r_k],  %[q1],    %[p1]        \n\t"
      "or             %[s2],   %[r_k],   %[c]         \n\t"

      /* abs(p0 - q0) * 2 + abs(p1 - q1) / 2  > flimit * 2 + limit */
      "shrl.qb        %[s2],   %[s2],     1           \n\t"
      "addu_s.qb      %[s1],   %[s2],     %[s3]       \n\t"
      "cmpgu.lt.qb    %[c],    %[flimit], %[s1]       \n\t"
      "or             %[r],    %[r],      %[c]        \n\t"
      "sll            %[r],    %[r],      24          \n\t"

      "wrdsp          %[r]                            \n\t"
      "pick.qb        %[s2],   $0,        %[ones]     \n\t"

      : [c] "=&r"(c), [r_k] "=&r"(r_k), [s1] "=&r"(s1), [hev1] "=&r"(hev1),
        [s2] "=&r"(s2), [r] "+r"(r), [s3] "=&r"(s3)
      : [p0] "r"(p0), [q0] "r"(q0), [p1] "r"(p1), [r3] "r"(r3), [q1] "r"(q1),
        [ones] "r"(ones), [flimit] "r"(flimit));

  *hev = hev1;
  *mask = s2;
  *flat = flat1;
}

static INLINE void flatmask5(uint32_t p4, uint32_t p3, uint32_t p2, uint32_t p1,
                             uint32_t p0, uint32_t q0, uint32_t q1, uint32_t q2,
                             uint32_t q3, uint32_t q4, uint32_t *flat2) {
  uint32_t c, r, r_k, r_flat;
  uint32_t ones = 0xFFFFFFFF;
  uint32_t flat_thresh = 0x01010101;
  uint32_t flat1, flat3;

  __asm__ __volatile__(
      /* flat |= (abs(p4 - p0) > thresh) */
      "subu_s.qb      %[c],   %[p4],           %[p0]        \n\t"
      "subu_s.qb      %[r_k], %[p0],           %[p4]        \n\t"
      "or             %[r_k], %[r_k],          %[c]         \n\t"
      "cmpgu.lt.qb    %[c],   %[flat_thresh],  %[r_k]       \n\t"
      "or             %[r],   $0,              %[c]         \n\t"

      /* flat |= (abs(q4 - q0) > thresh) */
      "subu_s.qb      %[c],     %[q4],           %[q0]     \n\t"
      "subu_s.qb      %[r_k],   %[q0],           %[q4]     \n\t"
      "or             %[r_k],   %[r_k],          %[c]      \n\t"
      "cmpgu.lt.qb    %[c],     %[flat_thresh],  %[r_k]    \n\t"
      "or             %[r],     %[r],            %[c]      \n\t"
      "sll            %[r],     %[r],            24        \n\t"
      "wrdsp          %[r]                                 \n\t"
      "pick.qb        %[flat3], $0,           %[ones]      \n\t"

      /* flat |= (abs(p1 - p0) > thresh) */
      "subu_s.qb      %[c],       %[p1],          %[p0]        \n\t"
      "subu_s.qb      %[r_k],     %[p0],          %[p1]        \n\t"
      "or             %[r_k],     %[r_k],         %[c]         \n\t"
      "cmpgu.lt.qb    %[c],       %[flat_thresh], %[r_k]       \n\t"
      "or             %[r_flat],  $0,             %[c]         \n\t"

      /* flat |= (abs(q1 - q0) > thresh) */
      "subu_s.qb      %[c],      %[q1],           %[q0]        \n\t"
      "subu_s.qb      %[r_k],    %[q0],           %[q1]        \n\t"
      "or             %[r_k],    %[r_k],          %[c]         \n\t"
      "cmpgu.lt.qb    %[c],      %[flat_thresh],  %[r_k]       \n\t"
      "or             %[r_flat], %[r_flat],       %[c]         \n\t"

      /* flat |= (abs(p0 - p2) > thresh) */
      "subu_s.qb      %[c],       %[p0],          %[p2]        \n\t"
      "subu_s.qb      %[r_k],     %[p2],          %[p0]        \n\t"
      "or             %[r_k],     %[r_k],         %[c]         \n\t"
      "cmpgu.lt.qb    %[c],       %[flat_thresh], %[r_k]       \n\t"
      "or             %[r_flat],  %[r_flat],      %[c]         \n\t"

      /* flat |= (abs(q0 - q2) > thresh) */
      "subu_s.qb      %[c],       %[q0],          %[q2]        \n\t"
      "subu_s.qb      %[r_k],     %[q2],          %[q0]        \n\t"
      "or             %[r_k],     %[r_k],         %[c]         \n\t"
      "cmpgu.lt.qb    %[c],       %[flat_thresh], %[r_k]       \n\t"
      "or             %[r_flat],  %[r_flat],      %[c]         \n\t"

      /* flat |= (abs(p3 - p0) > thresh) */
      "subu_s.qb      %[c],       %[p3],          %[p0]        \n\t"
      "subu_s.qb      %[r_k],     %[p0],          %[p3]        \n\t"
      "or             %[r_k],     %[r_k],         %[c]         \n\t"
      "cmpgu.lt.qb    %[c],       %[flat_thresh], %[r_k]       \n\t"
      "or             %[r_flat],  %[r_flat],      %[c]         \n\t"

      /* flat |= (abs(q3 - q0) > thresh) */
      "subu_s.qb      %[c],       %[q3],          %[q0]        \n\t"
      "subu_s.qb      %[r_k],     %[q0],          %[q3]        \n\t"
      "or             %[r_k],     %[r_k],         %[c]         \n\t"
      "cmpgu.lt.qb    %[c],       %[flat_thresh], %[r_k]       \n\t"
      "or             %[r_flat],  %[r_flat],      %[c]         \n\t"
      "sll            %[r_flat],  %[r_flat],      24           \n\t"
      "wrdsp          %[r_flat]                                \n\t"
      "pick.qb        %[flat1],   $0,             %[ones]      \n\t"
      /* flat & flatmask4(thresh, p3, p2, p1, p0, q0, q1, q2, q3) */
      "and            %[flat1],  %[flat3],        %[flat1]     \n\t"

      : [c] "=&r"(c), [r_k] "=&r"(r_k), [r] "=&r"(r), [r_flat] "=&r"(r_flat),
        [flat1] "=&r"(flat1), [flat3] "=&r"(flat3)
      : [p4] "r"(p4), [p3] "r"(p3), [p2] "r"(p2), [p1] "r"(p1), [p0] "r"(p0),
        [q0] "r"(q0), [q1] "r"(q1), [q2] "r"(q2), [q3] "r"(q3), [q4] "r"(q4),
        [flat_thresh] "r"(flat_thresh), [ones] "r"(ones));

  *flat2 = flat1;
}
#endif  // #if HAVE_DSPR2
#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AOM_AOM_DSP_MIPS_LOOPFILTER_MASKS_DSPR2_H_
