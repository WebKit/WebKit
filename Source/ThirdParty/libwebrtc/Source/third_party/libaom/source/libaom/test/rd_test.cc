/*
 * Copyright (c) 2021, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <math.h>
#include <vector>

#include "av1/common/quant_common.h"
#include "av1/encoder/rd.h"
#include "aom/aom_codec.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

namespace {

TEST(RdTest, GetDeltaqOffsetValueTest1) {
  aom_bit_depth_t bit_depth = AOM_BITS_8;
  double beta = 4;
  int q_index = 29;
  int dc_q_step =
      av1_dc_quant_QTX(q_index, 0, static_cast<aom_bit_depth_t>(bit_depth));
  EXPECT_EQ(dc_q_step, 32);

  int ref_new_dc_q_step = static_cast<int>(round(dc_q_step / sqrt(beta)));
  EXPECT_EQ(ref_new_dc_q_step, 16);

  int delta_q = av1_get_deltaq_offset(bit_depth, q_index, beta);
  int new_dc_q_step = av1_dc_quant_QTX(q_index, delta_q,
                                       static_cast<aom_bit_depth_t>(bit_depth));

  EXPECT_EQ(new_dc_q_step, ref_new_dc_q_step);
}

TEST(RdTest, GetDeltaqOffsetValueTest2) {
  aom_bit_depth_t bit_depth = AOM_BITS_8;
  double beta = 1.0 / 4.0;
  int q_index = 29;
  int dc_q_step =
      av1_dc_quant_QTX(q_index, 0, static_cast<aom_bit_depth_t>(bit_depth));
  EXPECT_EQ(dc_q_step, 32);

  int ref_new_dc_q_step = static_cast<int>(round(dc_q_step / sqrt(beta)));
  EXPECT_EQ(ref_new_dc_q_step, 64);

  int delta_q = av1_get_deltaq_offset(bit_depth, q_index, beta);
  int new_dc_q_step = av1_dc_quant_QTX(q_index, delta_q,
                                       static_cast<aom_bit_depth_t>(bit_depth));

  EXPECT_EQ(new_dc_q_step, ref_new_dc_q_step);
}

TEST(RdTest, GetDeltaqOffsetBoundaryTest1) {
  aom_bit_depth_t bit_depth = AOM_BITS_8;
  double beta = 0.000000001;
  std::vector<int> q_index_ls = { 254, 255 };
  for (auto q_index : q_index_ls) {
    int delta_q = av1_get_deltaq_offset(bit_depth, q_index, beta);
    EXPECT_EQ(q_index + delta_q, 255);
  }
}

TEST(RdTest, GetDeltaqOffsetBoundaryTest2) {
  aom_bit_depth_t bit_depth = AOM_BITS_8;
  double beta = 100;
  std::vector<int> q_index_ls = { 1, 0 };
  for (auto q_index : q_index_ls) {
    int delta_q = av1_get_deltaq_offset(bit_depth, q_index, beta);
    EXPECT_EQ(q_index + delta_q, 0);
  }
}

TEST(RdTest, GetDeltaqOffsetUnitaryTest1) {
  aom_bit_depth_t bit_depth = AOM_BITS_8;
  double beta = 1;
  for (int q_index = 0; q_index < 255; ++q_index) {
    int delta_q = av1_get_deltaq_offset(bit_depth, q_index, beta);
    EXPECT_EQ(delta_q, 0);
  }
}

}  // namespace
