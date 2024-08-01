/*
 * Copyright (c) 2017, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <immintrin.h>

#include "config/aom_dsp_rtcd.h"

#include "aom_dsp/x86/common_avx2.h"
#include "aom_dsp/x86/lpf_common_sse2.h"
#include "aom/aom_integer.h"

void aom_highbd_lpf_horizontal_14_dual_avx2(
    uint16_t *s, int p, const uint8_t *blimit0, const uint8_t *limit0,
    const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1,
    const uint8_t *thresh1, int bd) {
  aom_highbd_lpf_horizontal_14_dual_sse2(s, p, blimit0, limit0, thresh0,
                                         blimit1, limit1, thresh1, bd);
}

void aom_highbd_lpf_vertical_14_dual_avx2(
    uint16_t *s, int p, const uint8_t *blimit0, const uint8_t *limit0,
    const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1,
    const uint8_t *thresh1, int bd) {
  aom_highbd_lpf_vertical_14_dual_sse2(s, p, blimit0, limit0, thresh0, blimit1,
                                       limit1, thresh1, bd);
}

void aom_highbd_lpf_horizontal_4_dual_avx2(
    uint16_t *s, int p, const uint8_t *blimit0, const uint8_t *limit0,
    const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1,
    const uint8_t *thresh1, int bd) {
  aom_highbd_lpf_horizontal_4_dual_sse2(s, p, blimit0, limit0, thresh0, blimit1,
                                        limit1, thresh1, bd);
}

void aom_highbd_lpf_horizontal_8_dual_avx2(
    uint16_t *s, int p, const uint8_t *blimit0, const uint8_t *limit0,
    const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1,
    const uint8_t *thresh1, int bd) {
  aom_highbd_lpf_horizontal_8_dual_sse2(s, p, blimit0, limit0, thresh0, blimit1,
                                        limit1, thresh1, bd);
}

void aom_highbd_lpf_vertical_4_dual_avx2(
    uint16_t *s, int p, const uint8_t *blimit0, const uint8_t *limit0,
    const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1,
    const uint8_t *thresh1, int bd) {
  aom_highbd_lpf_vertical_4_dual_sse2(s, p, blimit0, limit0, thresh0, blimit1,
                                      limit1, thresh1, bd);
}

void aom_highbd_lpf_vertical_8_dual_avx2(
    uint16_t *s, int p, const uint8_t *blimit0, const uint8_t *limit0,
    const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1,
    const uint8_t *thresh1, int bd) {
  aom_highbd_lpf_vertical_8_dual_sse2(s, p, blimit0, limit0, thresh0, blimit1,
                                      limit1, thresh1, bd);
}
