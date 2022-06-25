/*
 *  Copyright (c) 2013 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <assert.h>

#include "./vpx_dsp_rtcd.h"
#include "vpx_dsp/vpx_dsp_common.h"
#include "vpx_ports/mem.h"

void vpx_convolve8_neon(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst,
                        ptrdiff_t dst_stride, const InterpKernel *filter,
                        int x0_q4, int x_step_q4, int y0_q4, int y_step_q4,
                        int w, int h) {
  /* Given our constraints: w <= 64, h <= 64, taps == 8 we can reduce the
   * maximum buffer size to 64 * 64 + 7 (+ 1 to make it divisible by 4).
   */
  uint8_t temp[64 * 72];

  // Account for the vertical phase needing 3 lines prior and 4 lines post
  // (+ 1 to make it divisible by 4).
  const int intermediate_height = h + 8;

  assert(y_step_q4 == 16);
  assert(x_step_q4 == 16);

  /* Filter starting 3 lines back. The neon implementation will ignore the given
   * height and filter a multiple of 4 lines. Since this goes in to the temp
   * buffer which has lots of extra room and is subsequently discarded this is
   * safe if somewhat less than ideal.   */
  vpx_convolve8_horiz_neon(src - src_stride * 3, src_stride, temp, w, filter,
                           x0_q4, x_step_q4, y0_q4, y_step_q4, w,
                           intermediate_height);

  /* Step into the temp buffer 3 lines to get the actual frame data */
  vpx_convolve8_vert_neon(temp + w * 3, w, dst, dst_stride, filter, x0_q4,
                          x_step_q4, y0_q4, y_step_q4, w, h);
}

void vpx_convolve8_avg_neon(const uint8_t *src, ptrdiff_t src_stride,
                            uint8_t *dst, ptrdiff_t dst_stride,
                            const InterpKernel *filter, int x0_q4,
                            int x_step_q4, int y0_q4, int y_step_q4, int w,
                            int h) {
  uint8_t temp[64 * 72];
  const int intermediate_height = h + 8;

  assert(y_step_q4 == 16);
  assert(x_step_q4 == 16);

  /* This implementation has the same issues as above. In addition, we only want
   * to average the values after both passes.
   */
  vpx_convolve8_horiz_neon(src - src_stride * 3, src_stride, temp, w, filter,
                           x0_q4, x_step_q4, y0_q4, y_step_q4, w,
                           intermediate_height);
  vpx_convolve8_avg_vert_neon(temp + w * 3, w, dst, dst_stride, filter, x0_q4,
                              x_step_q4, y0_q4, y_step_q4, w, h);
}
