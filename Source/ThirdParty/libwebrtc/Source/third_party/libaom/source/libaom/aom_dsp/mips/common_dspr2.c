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
uint8_t aom_ff_cropTbl_a[256 + 2 * CROP_WIDTH];
uint8_t *aom_ff_cropTbl;

void aom_dsputil_static_init(void) {
  int i;

  for (i = 0; i < 256; i++) aom_ff_cropTbl_a[i + CROP_WIDTH] = i;

  for (i = 0; i < CROP_WIDTH; i++) {
    aom_ff_cropTbl_a[i] = 0;
    aom_ff_cropTbl_a[i + CROP_WIDTH + 256] = 255;
  }

  aom_ff_cropTbl = &aom_ff_cropTbl_a[CROP_WIDTH];
}

#endif
