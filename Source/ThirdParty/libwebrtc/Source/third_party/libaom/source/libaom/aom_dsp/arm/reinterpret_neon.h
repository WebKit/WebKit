/*
 *  Copyright (c) 2023, Alliance for Open Media. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef AOM_AOM_DSP_ARM_REINTERPRET_NEON_H_
#define AOM_AOM_DSP_ARM_REINTERPRET_NEON_H_

#include <arm_neon.h>

#include "aom/aom_integer.h"  // For AOM_FORCE_INLINE.
#include "config/aom_config.h"

#define REINTERPRET_NEON(u, to_sz, to_count, from_sz, from_count, n, q)     \
  static AOM_FORCE_INLINE u##int##to_sz##x##to_count##x##n##_t              \
      aom_reinterpret##q##_##u##to_sz##_##u##from_sz##_x##n(                \
          const u##int##from_sz##x##from_count##x##n##_t src) {             \
    u##int##to_sz##x##to_count##x##n##_t ret;                               \
    for (int i = 0; i < (n); ++i) {                                         \
      ret.val[i] = vreinterpret##q##_##u##to_sz##_##u##from_sz(src.val[i]); \
    }                                                                       \
    return ret;                                                             \
  }

REINTERPRET_NEON(u, 8, 8, 16, 4, 2, )    // uint8x8x2_t from uint16x4x2_t
REINTERPRET_NEON(u, 8, 16, 16, 8, 2, q)  // uint8x16x2_t from uint16x8x2_t

#endif  // AOM_AOM_DSP_ARM_REINTERPRET_NEON_H_
