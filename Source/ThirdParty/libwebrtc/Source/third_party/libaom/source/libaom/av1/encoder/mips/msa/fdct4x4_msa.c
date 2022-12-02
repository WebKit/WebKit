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

#include <assert.h>

#include "av1/common/enums.h"

void av1_fwht4x4_msa(const int16_t *input, int16_t *output,
                     int32_t src_stride) {
  v8i16 in0, in1, in2, in3, in4;

  LD_SH4(input, src_stride, in0, in1, in2, in3);

  in0 += in1;
  in3 -= in2;
  in4 = (in0 - in3) >> 1;
  SUB2(in4, in1, in4, in2, in1, in2);
  in0 -= in2;
  in3 += in1;

  TRANSPOSE4x4_SH_SH(in0, in2, in3, in1, in0, in2, in3, in1);

  in0 += in2;
  in1 -= in3;
  in4 = (in0 - in1) >> 1;
  SUB2(in4, in2, in4, in3, in2, in3);
  in0 -= in3;
  in1 += in2;

  SLLI_4V(in0, in1, in2, in3, 2);

  TRANSPOSE4x4_SH_SH(in0, in3, in1, in2, in0, in3, in1, in2);

  ST4x2_UB(in0, output, 4);
  ST4x2_UB(in3, output + 4, 4);
  ST4x2_UB(in1, output + 8, 4);
  ST4x2_UB(in2, output + 12, 4);
}
