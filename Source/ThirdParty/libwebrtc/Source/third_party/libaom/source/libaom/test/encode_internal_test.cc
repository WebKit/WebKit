/*
 * Copyright (c) 2026, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <csetjmp>
#include <cstdint>
#include <cstring>
#include <memory>

#include "gtest/gtest.h"

#include "config/aom_config.h"

#include "aom_mem/aom_mem.h"
#include "av1/common/blockd.h"
#include "av1/common/reconintra.h"
#include "av1/encoder/allintra_vis.h"

namespace {

TEST(EncodeInternal, Buganizer558446054) {
  av1_init_intra_predictors();

  MACROBLOCKD xd = {};
  YV12_BUFFER_CONFIG cur_buf = {};
  xd.cur_buf = &cur_buf;
  MB_MODE_INFO mbmi = {};
  mbmi.bsize = BLOCK_8X8;
  mbmi.partition = PARTITION_NONE;
  mbmi.mode = D45_PRED;
  MB_MODE_INFO *mbmi_ptr = &mbmi;
  xd.mi = &mbmi_ptr;
  xd.bd = 8;
  xd.left_available = 1;
  xd.up_available = 1;
  xd.tile.mi_row_end = 100;
  xd.tile.mi_col_end = 100;
  // Set mb_to_bottom_edge and mb_to_right_edge such that
  // yd + txhpx < 0 and xr < 0:
  // yd + txhpx = (mb_to_bottom_edge >> 3) + hpx - y = -16 + 8 - 0 = -8 < 0
  // xr = (mb_to_right_edge >> 3) + wpx - x - txwpx = -5 + 8 - 0 - 4 = -1 < 0
  // xr + txwpx = 3 > 0 (so n_top_px = 3).
  xd.mb_to_bottom_edge = -16 * 8;
  xd.mb_to_right_edge = -5 * 8;

  uint8_t ref_buf[64 * 64];
  memset(ref_buf, 200, sizeof(ref_buf));
  uint8_t dst_buf[64 * 64] = { 0 };
  av1_predict_intra_block(&xd, BLOCK_64X64, /*enable_intra_edge_filter=*/0,
                          /*wpx=*/8, /*hpx=*/8, TX_4X4, D45_PRED,
                          /*angle_delta=*/0, /*use_palette=*/0,
                          FILTER_INTRA_MODES, ref_buf + 64 * 8 + 8, 64, dst_buf,
                          64, /*col_off=*/0, /*row_off=*/0, /*plane=*/0);
  // In Debug builds (-UNDEBUG), reverting clamp() triggers:
  //   Assertion `n_left_px >= 0' failed.
  // In Release builds (-DNDEBUG), reverting clamp() causes n_topright_px < 0
  // (-4 < 0), skipping top-right extension of above_row[3]=200 into
  // above_row[4..7] and leaving dst_buf[3] == 127 instead of 200.
  ASSERT_EQ(dst_buf[3], 200);
}

TEST(EncodeInternal, Buganizer558463892_558589747) {
  std::unique_ptr<AV1_COMP> cpi_test(new AV1_COMP());
  struct aom_internal_error_info error = {};
  if (setjmp(error.jmp)) FAIL();
  error.setjmp = 1;
  cpi_test->common.error = &error;
  cpi_test->frame_info.mi_rows = 16;
  cpi_test->frame_info.mi_cols = 16;
  av1_init_mb_wiener_var_buffer(cpi_test.get());
  ASSERT_NE(cpi_test->mb_weber_stats, nullptr);
  cpi_test->mb_weber_stats[0].satd = 12345;

  // Increase dimensions to 64x64 MI units: mb_weber_stats must be reallocated.
  cpi_test->frame_info.mi_rows = 64;
  cpi_test->frame_info.mi_cols = 64;
  av1_init_mb_wiener_var_buffer(cpi_test.get());
  EXPECT_EQ(cpi_test->mb_weber_stats[0].satd, 0);
  aom_free(cpi_test->mb_weber_stats);
}

TEST(EncodeInternal, Buganizer558417547) {
  std::unique_ptr<AV1_COMP> cpi_test(new AV1_COMP());
  SequenceHeader seq_params = {};
  seq_params.bit_depth = AOM_BITS_8;
  seq_params.sb_size = BLOCK_64X64;
  cpi_test->common.seq_params = &seq_params;
  cpi_test->common.mi_params.mi_rows = 16;
  cpi_test->common.mi_params.mi_cols = 16;
  cpi_test->common.quant_params.base_qindex = 128;
  cpi_test->common.delta_q_info.delta_q_res = 4;
  cpi_test->frame_info.mi_rows = 16;
  cpi_test->frame_info.mi_cols = 16;
  cpi_test->norm_wiener_variance = 100;
  struct aom_internal_error_info error = {};
  if (setjmp(error.jmp)) FAIL();
  error.setjmp = 1;
  cpi_test->common.error = &error;
  av1_init_mb_wiener_var_buffer(cpi_test.get());
  ASSERT_NE(cpi_test->mb_weber_stats, nullptr);

  // Set large SATD and distortion values exceeding INT32_MAX.
  for (int i = 0; i < 16 * 16; ++i) {
    cpi_test->mb_weber_stats[i].satd = 3000000000LL;
    cpi_test->mb_weber_stats[i].distortion = 3000000000LL;
    cpi_test->mb_weber_stats[i].rec_pix_max = 255;
  }

  // Test out-of-bounds / negative mi_row and mi_col without crashing or SIGFPE.
  int q_neg = av1_get_sbq_perceptual_ai(cpi_test.get(), BLOCK_64X64, -16, -16);
  EXPECT_GE(q_neg, 0);
  int q_oob = av1_get_sbq_perceptual_ai(cpi_test.get(), BLOCK_64X64, 100, 100);
  EXPECT_GE(q_oob, 0);

  aom_free(cpi_test->mb_weber_stats);
}

}  // namespace
