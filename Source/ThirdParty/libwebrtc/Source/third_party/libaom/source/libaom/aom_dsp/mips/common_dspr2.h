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

#ifndef AOM_AOM_DSP_MIPS_COMMON_DSPR2_H_
#define AOM_AOM_DSP_MIPS_COMMON_DSPR2_H_

#include <assert.h>

#include "config/aom_config.h"

#include "aom/aom_integer.h"

#ifdef __cplusplus
extern "C" {
#endif
#if HAVE_DSPR2
#define CROP_WIDTH 512

extern uint8_t *aom_ff_cropTbl;  // From "aom_dsp/mips/intrapred4_dspr2.c"

static INLINE void prefetch_load(const unsigned char *src) {
  __asm__ __volatile__("pref   0,  0(%[src])   \n\t" : : [src] "r"(src));
}

/* prefetch data for store */
static INLINE void prefetch_store(unsigned char *dst) {
  __asm__ __volatile__("pref   1,  0(%[dst])   \n\t" : : [dst] "r"(dst));
}

static INLINE void prefetch_load_streamed(const unsigned char *src) {
  __asm__ __volatile__("pref   4,  0(%[src])   \n\t" : : [src] "r"(src));
}

/* prefetch data for store */
static INLINE void prefetch_store_streamed(unsigned char *dst) {
  __asm__ __volatile__("pref   5,  0(%[dst])   \n\t" : : [dst] "r"(dst));
}
#endif  // #if HAVE_DSPR2
#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AOM_AOM_DSP_MIPS_COMMON_DSPR2_H_
