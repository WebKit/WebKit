/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include "av1/common/av1_common_int.h"
#include "av1/common/entropymv.h"

static const nmv_context default_nmv_context = {
  { AOM_CDF4(4096, 11264, 19328) },  // joints_cdf
  { {
        // Vertical component
        { AOM_CDF11(28672, 30976, 31858, 32320, 32551, 32656, 32740, 32757,
                    32762, 32767) },  // class_cdf // fp
        { { AOM_CDF4(16384, 24576, 26624) },
          { AOM_CDF4(12288, 21248, 24128) } },  // class0_fp_cdf
        { AOM_CDF4(8192, 17408, 21248) },       // fp_cdf
        { AOM_CDF2(128 * 128) },                // sign_cdf
        { AOM_CDF2(160 * 128) },                // class0_hp_cdf
        { AOM_CDF2(128 * 128) },                // hp_cdf
        { AOM_CDF2(216 * 128) },                // class0_cdf
        { { AOM_CDF2(128 * 136) },
          { AOM_CDF2(128 * 140) },
          { AOM_CDF2(128 * 148) },
          { AOM_CDF2(128 * 160) },
          { AOM_CDF2(128 * 176) },
          { AOM_CDF2(128 * 192) },
          { AOM_CDF2(128 * 224) },
          { AOM_CDF2(128 * 234) },
          { AOM_CDF2(128 * 234) },
          { AOM_CDF2(128 * 240) } },  // bits_cdf
    },
    {
        // Horizontal component
        { AOM_CDF11(28672, 30976, 31858, 32320, 32551, 32656, 32740, 32757,
                    32762, 32767) },  // class_cdf // fp
        { { AOM_CDF4(16384, 24576, 26624) },
          { AOM_CDF4(12288, 21248, 24128) } },  // class0_fp_cdf
        { AOM_CDF4(8192, 17408, 21248) },       // fp_cdf
        { AOM_CDF2(128 * 128) },                // sign_cdf
        { AOM_CDF2(160 * 128) },                // class0_hp_cdf
        { AOM_CDF2(128 * 128) },                // hp_cdf
        { AOM_CDF2(216 * 128) },                // class0_cdf
        { { AOM_CDF2(128 * 136) },
          { AOM_CDF2(128 * 140) },
          { AOM_CDF2(128 * 148) },
          { AOM_CDF2(128 * 160) },
          { AOM_CDF2(128 * 176) },
          { AOM_CDF2(128 * 192) },
          { AOM_CDF2(128 * 224) },
          { AOM_CDF2(128 * 234) },
          { AOM_CDF2(128 * 234) },
          { AOM_CDF2(128 * 240) } },  // bits_cdf
    } },
};

void av1_init_mv_probs(AV1_COMMON *cm) {
  // NB: this sets CDFs too
  cm->fc->nmvc = default_nmv_context;
  cm->fc->ndvc = default_nmv_context;
}
