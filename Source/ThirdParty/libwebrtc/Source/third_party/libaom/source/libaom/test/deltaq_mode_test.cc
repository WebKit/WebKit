/*
 * Copyright (c) 2023, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <cstddef>
#include <cstdint>
#include <vector>

#include "aom/aomcx.h"
#include "aom/aom_codec.h"
#include "aom/aom_encoder.h"
#include "aom/aom_image.h"
#include "config/aom_config.h"
#include "gtest/gtest.h"

namespace {

/*
   Reproduces https://crbug.com/aomedia/3376. Emulates the command line:

   ./aomenc --cpu-used=6 --threads=10 --cq-level=14 --passes=1 --limit=1 \
     --lag-in-frames=0 --end-usage=q --deltaq-mode=3 --min-q=0 --max-q=63 \
     -o output.av1 niklas_1280_720_30.y4m
*/
TEST(DeltaqModeTest, DeltaqMode3MultiThread) {
  constexpr int kWidth = 1280;
  constexpr int kHeight = 720;
  // Dummy buffer of neutral gray samples.
  constexpr size_t kBufferSize = kWidth * kHeight + kWidth * kHeight / 2;
  std::vector<unsigned char> buffer(kBufferSize,
                                    static_cast<unsigned char>(128));

  aom_image_t img;
  EXPECT_EQ(&img, aom_img_wrap(&img, AOM_IMG_FMT_I420, kWidth, kHeight, 1,
                               buffer.data()));

  aom_codec_iface_t *iface = aom_codec_av1_cx();
  aom_codec_enc_cfg_t cfg;
  EXPECT_EQ(aom_codec_enc_config_default(iface, &cfg, AOM_USAGE_GOOD_QUALITY),
            AOM_CODEC_OK);
  cfg.g_w = kWidth;
  cfg.g_h = kHeight;
  cfg.g_threads = 10;
  cfg.rc_end_usage = AOM_Q;
  cfg.g_profile = 0;
  cfg.g_bit_depth = AOM_BITS_8;
  cfg.g_input_bit_depth = 8;
  cfg.g_lag_in_frames = 0;
  cfg.rc_min_quantizer = 0;
  cfg.rc_max_quantizer = 63;
  cfg.g_pass = AOM_RC_ONE_PASS;
  cfg.g_limit = 1;
  aom_codec_ctx_t enc;
  EXPECT_EQ(aom_codec_enc_init(&enc, iface, &cfg, 0), AOM_CODEC_OK);
  EXPECT_EQ(aom_codec_control(&enc, AOME_SET_CPUUSED, 6), AOM_CODEC_OK);
  EXPECT_EQ(aom_codec_control(&enc, AOME_SET_CQ_LEVEL, 14), AOM_CODEC_OK);
  EXPECT_EQ(aom_codec_control(&enc, AV1E_SET_DELTAQ_MODE, 3), AOM_CODEC_OK);
  EXPECT_EQ(aom_codec_set_option(&enc, "passes", "1"), AOM_CODEC_OK);
  EXPECT_EQ(aom_codec_control(&enc, AV1E_SET_COLOR_RANGE, AOM_CR_STUDIO_RANGE),
            AOM_CODEC_OK);

  EXPECT_EQ(aom_codec_encode(&enc, &img, 0, 1, 0), AOM_CODEC_OK);
  aom_codec_iter_t iter = nullptr;
  const aom_codec_cx_pkt_t *pkt = aom_codec_get_cx_data(&enc, &iter);
  ASSERT_NE(pkt, nullptr);
  EXPECT_EQ(pkt->kind, AOM_CODEC_CX_FRAME_PKT);
  // pkt->data.frame.flags is 0x1f0011.
  EXPECT_EQ(pkt->data.frame.flags & AOM_FRAME_IS_KEY, AOM_FRAME_IS_KEY);
  pkt = aom_codec_get_cx_data(&enc, &iter);
  EXPECT_EQ(pkt, nullptr);

  // Flush encoder
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_encode(&enc, nullptr, 0, 1, 0));
  iter = nullptr;
  pkt = aom_codec_get_cx_data(&enc, &iter);
  EXPECT_EQ(pkt, nullptr);

  EXPECT_EQ(AOM_CODEC_OK, aom_codec_destroy(&enc));
}

// The implementation of multi-threading for deltaq-mode=3 in allintra
// mode is based on row multi-threading.
// The test ensures that When row mt is turned off,
// deltaq-mode = 3 can still properly encode and decode.
TEST(DeltaqModeTest, DeltaqMode3MultiThreadNoRowMT) {
  constexpr int kWidth = 1280;
  constexpr int kHeight = 720;
  // Dummy buffer of neutral gray samples.
  constexpr size_t kBufferSize = kWidth * kHeight + kWidth * kHeight / 2;
  std::vector<unsigned char> buffer(kBufferSize,
                                    static_cast<unsigned char>(128));

  aom_image_t img;
  EXPECT_EQ(&img, aom_img_wrap(&img, AOM_IMG_FMT_I420, kWidth, kHeight, 1,
                               buffer.data()));

  aom_codec_iface_t *iface = aom_codec_av1_cx();
  aom_codec_enc_cfg_t cfg;
  EXPECT_EQ(aom_codec_enc_config_default(iface, &cfg, AOM_USAGE_GOOD_QUALITY),
            AOM_CODEC_OK);
  cfg.g_w = kWidth;
  cfg.g_h = kHeight;
  cfg.g_threads = 10;
  cfg.rc_end_usage = AOM_Q;
  cfg.g_profile = 0;
  cfg.g_bit_depth = AOM_BITS_8;
  cfg.g_input_bit_depth = 8;
  cfg.g_lag_in_frames = 0;
  cfg.rc_min_quantizer = 0;
  cfg.rc_max_quantizer = 63;
  cfg.g_pass = AOM_RC_ONE_PASS;
  cfg.g_limit = 1;
  aom_codec_ctx_t enc;
  EXPECT_EQ(aom_codec_enc_init(&enc, iface, &cfg, 0), AOM_CODEC_OK);
  EXPECT_EQ(aom_codec_control(&enc, AV1E_SET_ROW_MT, 0), AOM_CODEC_OK);
  EXPECT_EQ(aom_codec_control(&enc, AOME_SET_CPUUSED, 6), AOM_CODEC_OK);
  EXPECT_EQ(aom_codec_control(&enc, AOME_SET_CQ_LEVEL, 14), AOM_CODEC_OK);
  EXPECT_EQ(aom_codec_control(&enc, AV1E_SET_DELTAQ_MODE, 3), AOM_CODEC_OK);
  EXPECT_EQ(aom_codec_set_option(&enc, "passes", "1"), AOM_CODEC_OK);
  EXPECT_EQ(aom_codec_control(&enc, AV1E_SET_COLOR_RANGE, AOM_CR_STUDIO_RANGE),
            AOM_CODEC_OK);

  EXPECT_EQ(aom_codec_encode(&enc, &img, 0, 1, 0), AOM_CODEC_OK);
  aom_codec_iter_t iter = nullptr;
  const aom_codec_cx_pkt_t *pkt = aom_codec_get_cx_data(&enc, &iter);
  ASSERT_NE(pkt, nullptr);
  EXPECT_EQ(pkt->kind, AOM_CODEC_CX_FRAME_PKT);
  // pkt->data.frame.flags is 0x1f0011.
  EXPECT_EQ(pkt->data.frame.flags & AOM_FRAME_IS_KEY, AOM_FRAME_IS_KEY);
  pkt = aom_codec_get_cx_data(&enc, &iter);
  EXPECT_EQ(pkt, nullptr);

  // Flush encoder
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_encode(&enc, nullptr, 0, 1, 0));
  iter = nullptr;
  pkt = aom_codec_get_cx_data(&enc, &iter);
  EXPECT_EQ(pkt, nullptr);

  EXPECT_EQ(AOM_CODEC_OK, aom_codec_destroy(&enc));
}

#if CONFIG_AV1_HIGHBITDEPTH
// 10-bit version of the DeltaqMode3MultiThread test.
TEST(DeltaqModeTest, DeltaqMode3MultiThreadHighbd) {
  constexpr int kWidth = 1280;
  constexpr int kHeight = 720;
  // Dummy buffer of 10-bit neutral gray samples.
  constexpr size_t kBufferSize = kWidth * kHeight + kWidth * kHeight / 2;
  std::vector<uint16_t> buffer(kBufferSize, 512);

  aom_image_t img;
  EXPECT_EQ(&img,
            aom_img_wrap(&img, AOM_IMG_FMT_I42016, kWidth, kHeight, 1,
                         reinterpret_cast<unsigned char *>(buffer.data())));

  aom_codec_iface_t *iface = aom_codec_av1_cx();
  aom_codec_enc_cfg_t cfg;
  EXPECT_EQ(aom_codec_enc_config_default(iface, &cfg, AOM_USAGE_GOOD_QUALITY),
            AOM_CODEC_OK);
  cfg.g_w = kWidth;
  cfg.g_h = kHeight;
  cfg.g_threads = 10;
  cfg.rc_end_usage = AOM_Q;
  cfg.g_profile = 0;
  cfg.g_bit_depth = AOM_BITS_10;
  cfg.g_input_bit_depth = 10;
  cfg.g_lag_in_frames = 0;
  cfg.rc_min_quantizer = 0;
  cfg.rc_max_quantizer = 63;
  cfg.g_pass = AOM_RC_ONE_PASS;
  cfg.g_limit = 1;
  aom_codec_ctx_t enc;
  EXPECT_EQ(aom_codec_enc_init(&enc, iface, &cfg, AOM_CODEC_USE_HIGHBITDEPTH),
            AOM_CODEC_OK);
  EXPECT_EQ(aom_codec_control(&enc, AOME_SET_CPUUSED, 6), AOM_CODEC_OK);
  EXPECT_EQ(aom_codec_control(&enc, AOME_SET_CQ_LEVEL, 14), AOM_CODEC_OK);
  EXPECT_EQ(aom_codec_control(&enc, AV1E_SET_DELTAQ_MODE, 3), AOM_CODEC_OK);
  EXPECT_EQ(aom_codec_set_option(&enc, "passes", "1"), AOM_CODEC_OK);
  EXPECT_EQ(aom_codec_control(&enc, AV1E_SET_COLOR_RANGE, AOM_CR_STUDIO_RANGE),
            AOM_CODEC_OK);

  EXPECT_EQ(aom_codec_encode(&enc, &img, 0, 1, 0), AOM_CODEC_OK);
  aom_codec_iter_t iter = nullptr;
  const aom_codec_cx_pkt_t *pkt = aom_codec_get_cx_data(&enc, &iter);
  ASSERT_NE(pkt, nullptr);
  EXPECT_EQ(pkt->kind, AOM_CODEC_CX_FRAME_PKT);
  // pkt->data.frame.flags is 0x1f0011.
  EXPECT_EQ(pkt->data.frame.flags & AOM_FRAME_IS_KEY, AOM_FRAME_IS_KEY);
  pkt = aom_codec_get_cx_data(&enc, &iter);
  EXPECT_EQ(pkt, nullptr);

  // Flush encoder
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_encode(&enc, nullptr, 0, 1, 0));
  iter = nullptr;
  pkt = aom_codec_get_cx_data(&enc, &iter);
  EXPECT_EQ(pkt, nullptr);

  EXPECT_EQ(AOM_CODEC_OK, aom_codec_destroy(&enc));
}
#endif  // CONFIG_AV1_HIGHBITDEPTH

}  // namespace
