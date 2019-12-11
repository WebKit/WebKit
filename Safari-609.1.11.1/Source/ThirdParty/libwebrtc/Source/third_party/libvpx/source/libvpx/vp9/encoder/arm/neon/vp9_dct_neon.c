/*
 *  Copyright (c) 2014 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <arm_neon.h>

#include "./vp9_rtcd.h"
#include "./vpx_config.h"
#include "./vpx_dsp_rtcd.h"

#include "vp9/common/vp9_blockd.h"
#include "vpx_dsp/txfm_common.h"
#include "vpx_dsp/vpx_dsp_common.h"

void vp9_fdct8x8_quant_neon(const int16_t *input, int stride,
                            tran_low_t *coeff_ptr, intptr_t n_coeffs,
                            int skip_block, const int16_t *round_ptr,
                            const int16_t *quant_ptr, tran_low_t *qcoeff_ptr,
                            tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr,
                            uint16_t *eob_ptr, const int16_t *scan_ptr,
                            const int16_t *iscan_ptr) {
  tran_low_t temp_buffer[64];
  (void)coeff_ptr;

  vpx_fdct8x8_neon(input, temp_buffer, stride);
  vp9_quantize_fp_neon(temp_buffer, n_coeffs, skip_block, round_ptr, quant_ptr,
                       qcoeff_ptr, dqcoeff_ptr, dequant_ptr, eob_ptr, scan_ptr,
                       iscan_ptr);
}
