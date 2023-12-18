/*
 *  Copyright (c) 2023, Alliance for Open Media. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef AOM_AOM_DSP_ARM_DIST_WTD_AVG_NEON_H_
#define AOM_AOM_DSP_ARM_DIST_WTD_AVG_NEON_H_

#include <arm_neon.h>

#include "aom_dsp/aom_dsp_common.h"
#include "av1/common/enums.h"

static INLINE uint8x8_t dist_wtd_avg_u8x8(uint8x8_t a, uint8x8_t b,
                                          uint8x8_t wta, uint8x8_t wtb) {
  uint16x8_t wtd_sum = vmull_u8(a, wta);

  wtd_sum = vmlal_u8(wtd_sum, b, wtb);

  return vrshrn_n_u16(wtd_sum, DIST_PRECISION_BITS);
}

static INLINE uint16x4_t dist_wtd_avg_u16x4(uint16x4_t a, uint16x4_t b,
                                            uint16x4_t wta, uint16x4_t wtb) {
  uint32x4_t wtd_sum = vmull_u16(a, wta);

  wtd_sum = vmlal_u16(wtd_sum, b, wtb);

  return vrshrn_n_u32(wtd_sum, DIST_PRECISION_BITS);
}

static INLINE uint8x16_t dist_wtd_avg_u8x16(uint8x16_t a, uint8x16_t b,
                                            uint8x16_t wta, uint8x16_t wtb) {
  uint16x8_t wtd_sum_lo = vmull_u8(vget_low_u8(a), vget_low_u8(wta));
  uint16x8_t wtd_sum_hi = vmull_u8(vget_high_u8(a), vget_high_u8(wta));

  wtd_sum_lo = vmlal_u8(wtd_sum_lo, vget_low_u8(b), vget_low_u8(wtb));
  wtd_sum_hi = vmlal_u8(wtd_sum_hi, vget_high_u8(b), vget_high_u8(wtb));

  uint8x8_t wtd_avg_lo = vrshrn_n_u16(wtd_sum_lo, DIST_PRECISION_BITS);
  uint8x8_t wtd_avg_hi = vrshrn_n_u16(wtd_sum_hi, DIST_PRECISION_BITS);

  return vcombine_u8(wtd_avg_lo, wtd_avg_hi);
}

static INLINE uint16x8_t dist_wtd_avg_u16x8(uint16x8_t a, uint16x8_t b,
                                            uint16x8_t wta, uint16x8_t wtb) {
  uint32x4_t wtd_sum_lo = vmull_u16(vget_low_u16(a), vget_low_u16(wta));
  uint32x4_t wtd_sum_hi = vmull_u16(vget_high_u16(a), vget_high_u16(wta));

  wtd_sum_lo = vmlal_u16(wtd_sum_lo, vget_low_u16(b), vget_low_u16(wtb));
  wtd_sum_hi = vmlal_u16(wtd_sum_hi, vget_high_u16(b), vget_high_u16(wtb));

  uint16x4_t wtd_avg_lo = vrshrn_n_u32(wtd_sum_lo, DIST_PRECISION_BITS);
  uint16x4_t wtd_avg_hi = vrshrn_n_u32(wtd_sum_hi, DIST_PRECISION_BITS);

  return vcombine_u16(wtd_avg_lo, wtd_avg_hi);
}

#endif  // AOM_AOM_DSP_ARM_DIST_WTD_AVG_NEON_H_
