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

#include "av1/encoder/cost.h"
#include "av1/common/entropy.h"

// round(-log2(i/256.) * (1 << AV1_PROB_COST_SHIFT)); i = 128~255.
const uint16_t av1_prob_cost[128] = {
  512, 506, 501, 495, 489, 484, 478, 473, 467, 462, 456, 451, 446, 441, 435,
  430, 425, 420, 415, 410, 405, 400, 395, 390, 385, 380, 375, 371, 366, 361,
  356, 352, 347, 343, 338, 333, 329, 324, 320, 316, 311, 307, 302, 298, 294,
  289, 285, 281, 277, 273, 268, 264, 260, 256, 252, 248, 244, 240, 236, 232,
  228, 224, 220, 216, 212, 209, 205, 201, 197, 194, 190, 186, 182, 179, 175,
  171, 168, 164, 161, 157, 153, 150, 146, 143, 139, 136, 132, 129, 125, 122,
  119, 115, 112, 109, 105, 102, 99,  95,  92,  89,  86,  82,  79,  76,  73,
  70,  66,  63,  60,  57,  54,  51,  48,  45,  42,  38,  35,  32,  29,  26,
  23,  20,  18,  15,  12,  9,   6,   3,
};

void av1_cost_tokens_from_cdf(int *costs, const aom_cdf_prob *cdf,
                              const int *inv_map) {
  int i;
  aom_cdf_prob prev_cdf = 0;
  for (i = 0;; ++i) {
    aom_cdf_prob p15 = AOM_ICDF(cdf[i]) - prev_cdf;
    p15 = (p15 < EC_MIN_PROB) ? EC_MIN_PROB : p15;
    prev_cdf = AOM_ICDF(cdf[i]);

    if (inv_map)
      costs[inv_map[i]] = av1_cost_symbol(p15);
    else
      costs[i] = av1_cost_symbol(p15);

    // Stop once we reach the end of the CDF
    if (cdf[i] == AOM_ICDF(CDF_PROB_TOP)) break;
  }
}
