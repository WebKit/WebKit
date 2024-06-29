/*
 *  Copyright (c) 2024, Alliance for Open Media. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef AOM_AOM_DSP_ARM_AOM_NEON_SVE2_BRIDGE_H_
#define AOM_AOM_DSP_ARM_AOM_NEON_SVE2_BRIDGE_H_

#include <arm_neon_sve_bridge.h>

#include "config/aom_dsp_rtcd.h"
#include "config/aom_config.h"

// We can access instructions exclusive to the SVE2 instruction set from a
// predominantly Neon context by making use of the Neon-SVE bridge intrinsics
// to reinterpret Neon vectors as SVE vectors - with the high part of the SVE
// vector (if it's longer than 128 bits) being "don't care".

// While sub-optimal on machines that have SVE vector length > 128-bit - as the
// remainder of the vector is unused - this approach is still beneficial when
// compared to a Neon-only solution.

static INLINE int16x8_t aom_tbl2_s16(int16x8_t s0, int16x8_t s1,
                                     uint16x8_t tbl) {
  svint16x2_t samples = svcreate2_s16(svset_neonq_s16(svundef_s16(), s0),
                                      svset_neonq_s16(svundef_s16(), s1));
  return svget_neonq_s16(
      svtbl2_s16(samples, svset_neonq_u16(svundef_u16(), tbl)));
}

#endif  // AOM_AOM_DSP_ARM_AOM_NEON_SVE2_BRIDGE_H_
