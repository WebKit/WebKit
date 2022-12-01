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

#include "av1/encoder/firstpass.h"
#include "av1/encoder/ratectrl.h"
#include "av1/encoder/tpl_model.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

namespace {

TEST(RatectrlTest, QModeGetQIndexTest) {
  int base_q_index = 36;
  int gf_update_type = INTNL_ARF_UPDATE;
  int gf_pyramid_level = 1;
  int arf_q = 100;
  int q_index = av1_q_mode_get_q_index(base_q_index, gf_update_type,
                                       gf_pyramid_level, arf_q);
  EXPECT_EQ(q_index, arf_q);

  gf_update_type = INTNL_ARF_UPDATE;
  gf_pyramid_level = 3;
  q_index = av1_q_mode_get_q_index(base_q_index, gf_update_type,
                                   gf_pyramid_level, arf_q);
  EXPECT_LT(q_index, arf_q);

  gf_update_type = LF_UPDATE;
  q_index = av1_q_mode_get_q_index(base_q_index, gf_update_type,
                                   gf_pyramid_level, arf_q);
  EXPECT_EQ(q_index, base_q_index);
}
}  // namespace
