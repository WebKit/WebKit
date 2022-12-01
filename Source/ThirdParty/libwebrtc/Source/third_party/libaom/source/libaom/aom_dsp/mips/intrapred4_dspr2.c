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

#include "aom_dsp/mips/common_dspr2.h"

#if HAVE_DSPR2
void aom_h_predictor_4x4_dspr2(uint8_t *dst, ptrdiff_t stride,
                               const uint8_t *above, const uint8_t *left) {
  int32_t tmp1, tmp2, tmp3, tmp4;
  (void)above;

  __asm__ __volatile__(
      "lb         %[tmp1],      (%[left])                    \n\t"
      "lb         %[tmp2],      1(%[left])                   \n\t"
      "lb         %[tmp3],      2(%[left])                   \n\t"
      "lb         %[tmp4],      3(%[left])                   \n\t"
      "replv.qb   %[tmp1],      %[tmp1]                      \n\t"
      "replv.qb   %[tmp2],      %[tmp2]                      \n\t"
      "replv.qb   %[tmp3],      %[tmp3]                      \n\t"
      "replv.qb   %[tmp4],      %[tmp4]                      \n\t"
      "sw         %[tmp1],      (%[dst])                     \n\t"
      "add        %[dst],       %[dst],         %[stride]    \n\t"
      "sw         %[tmp2],      (%[dst])                     \n\t"
      "add        %[dst],       %[dst],         %[stride]    \n\t"
      "sw         %[tmp3],      (%[dst])                     \n\t"
      "add        %[dst],       %[dst],         %[stride]    \n\t"
      "sw         %[tmp4],      (%[dst])                     \n\t"

      : [tmp1] "=&r"(tmp1), [tmp2] "=&r"(tmp2), [tmp3] "=&r"(tmp3),
        [tmp4] "=&r"(tmp4)
      : [left] "r"(left), [dst] "r"(dst), [stride] "r"(stride));
}

void aom_dc_predictor_4x4_dspr2(uint8_t *dst, ptrdiff_t stride,
                                const uint8_t *above, const uint8_t *left) {
  int32_t expected_dc;
  int32_t average;
  int32_t tmp, above_c, above_l, above_r, left_c, left_r, left_l;

  __asm__ __volatile__(
      "lw              %[above_c],         (%[above])                    \n\t"
      "lw              %[left_c],          (%[left])                     \n\t"

      "preceu.ph.qbl   %[above_l],         %[above_c]                    \n\t"
      "preceu.ph.qbr   %[above_r],         %[above_c]                    \n\t"
      "preceu.ph.qbl   %[left_l],          %[left_c]                     \n\t"
      "preceu.ph.qbr   %[left_r],          %[left_c]                     \n\t"

      "addu.ph         %[average],         %[above_r],       %[above_l]  \n\t"
      "addu.ph         %[average],         %[average],       %[left_l]   \n\t"
      "addu.ph         %[average],         %[average],       %[left_r]   \n\t"
      "addiu           %[average],         %[average],       4           \n\t"
      "srl             %[tmp],             %[average],       16          \n\t"
      "addu.ph         %[average],         %[tmp],           %[average]  \n\t"
      "srl             %[expected_dc],     %[average],       3           \n\t"
      "replv.qb        %[expected_dc],     %[expected_dc]                \n\t"

      "sw              %[expected_dc],     (%[dst])                      \n\t"
      "add             %[dst],              %[dst],          %[stride]   \n\t"
      "sw              %[expected_dc],     (%[dst])                      \n\t"
      "add             %[dst],              %[dst],          %[stride]   \n\t"
      "sw              %[expected_dc],     (%[dst])                      \n\t"
      "add             %[dst],              %[dst],          %[stride]   \n\t"
      "sw              %[expected_dc],     (%[dst])                      \n\t"

      : [above_c] "=&r"(above_c), [above_l] "=&r"(above_l),
        [above_r] "=&r"(above_r), [left_c] "=&r"(left_c),
        [left_l] "=&r"(left_l), [left_r] "=&r"(left_r),
        [average] "=&r"(average), [tmp] "=&r"(tmp),
        [expected_dc] "=&r"(expected_dc)
      : [above] "r"(above), [left] "r"(left), [dst] "r"(dst),
        [stride] "r"(stride));
}
#endif  // #if HAVE_DSPR2
