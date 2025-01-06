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
#include <vector>

#include "aom/aomcx.h"
#include "aom/aom_codec.h"
#include "aom/aom_encoder.h"
#include "aom/aom_image.h"
#include "gtest/gtest.h"

namespace {

// This test emulates how libavif calls libaom functions to encode a
// progressive AVIF image in libavif's ProgressiveTest.QualityChange test.
TEST(AVIFProgressiveTest, QualityChange) {
  constexpr int kWidth = 256;
  constexpr int kHeight = 256;
  // A buffer of neutral gray samples.
  constexpr size_t kBufferSize = 3 * kWidth * kHeight;
  std::vector<unsigned char> buffer(kBufferSize,
                                    static_cast<unsigned char>(128));

  aom_image_t img;
  EXPECT_EQ(&img, aom_img_wrap(&img, AOM_IMG_FMT_I444, kWidth, kHeight, 1,
                               buffer.data()));
  img.cp = AOM_CICP_CP_UNSPECIFIED;
  img.tc = AOM_CICP_TC_UNSPECIFIED;
  img.mc = AOM_CICP_MC_UNSPECIFIED;
  img.range = AOM_CR_FULL_RANGE;

  aom_codec_iface_t *iface = aom_codec_av1_cx();
  aom_codec_enc_cfg_t cfg;
  EXPECT_EQ(AOM_CODEC_OK,
            aom_codec_enc_config_default(iface, &cfg, AOM_USAGE_GOOD_QUALITY));
  cfg.g_profile = 1;
  cfg.g_w = kWidth;
  cfg.g_h = kHeight;
  cfg.g_bit_depth = AOM_BITS_8;
  cfg.g_input_bit_depth = 8;
  cfg.g_lag_in_frames = 0;
  cfg.rc_end_usage = AOM_Q;
  cfg.rc_min_quantizer = 50;
  cfg.rc_max_quantizer = 50;
  aom_codec_ctx_t enc;
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_enc_init(&enc, iface, &cfg, 0));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_control(&enc, AOME_SET_CQ_LEVEL, 50));
  EXPECT_EQ(AOM_CODEC_OK,
            aom_codec_control(&enc, AOME_SET_NUMBER_SPATIAL_LAYERS, 2));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_control(&enc, AOME_SET_CPUUSED, 6));
  EXPECT_EQ(AOM_CODEC_OK,
            aom_codec_control(&enc, AV1E_SET_COLOR_RANGE, AOM_CR_FULL_RANGE));
  EXPECT_EQ(AOM_CODEC_OK,
            aom_codec_control(&enc, AOME_SET_TUNING, AOM_TUNE_SSIM));

  // First frame (layer 0)
  EXPECT_EQ(AOM_CODEC_OK,
            aom_codec_control(&enc, AOME_SET_SPATIAL_LAYER_ID, 0));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_encode(&enc, &img, 0, 1, 0));
  aom_codec_iter_t iter = nullptr;
  const aom_codec_cx_pkt_t *pkt = aom_codec_get_cx_data(&enc, &iter);
  ASSERT_NE(pkt, nullptr);
  EXPECT_EQ(pkt->kind, AOM_CODEC_CX_FRAME_PKT);
  // pkt->data.frame.flags is 0x1f0011.
  EXPECT_EQ(pkt->data.frame.flags & AOM_FRAME_IS_KEY, AOM_FRAME_IS_KEY);
  pkt = aom_codec_get_cx_data(&enc, &iter);
  EXPECT_EQ(pkt, nullptr);

  // Second frame (layer 1)
  cfg.rc_min_quantizer = 0;
  cfg.rc_max_quantizer = 0;
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_enc_config_set(&enc, &cfg));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_control(&enc, AOME_SET_CQ_LEVEL, 0));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_control(&enc, AV1E_SET_LOSSLESS, 1));
  EXPECT_EQ(AOM_CODEC_OK,
            aom_codec_control(&enc, AOME_SET_SPATIAL_LAYER_ID, 1));
  aom_enc_frame_flags_t encode_flags =
      AOM_EFLAG_NO_REF_GF | AOM_EFLAG_NO_REF_ARF | AOM_EFLAG_NO_REF_BWD |
      AOM_EFLAG_NO_REF_ARF2 | AOM_EFLAG_NO_UPD_GF | AOM_EFLAG_NO_UPD_ARF;
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_encode(&enc, &img, 0, 1, encode_flags));
  iter = nullptr;
  pkt = aom_codec_get_cx_data(&enc, &iter);
  ASSERT_NE(pkt, nullptr);
  EXPECT_EQ(pkt->kind, AOM_CODEC_CX_FRAME_PKT);
  // pkt->data.frame.flags is 0.
  EXPECT_EQ(pkt->data.frame.flags & AOM_FRAME_IS_KEY, 0u);
  pkt = aom_codec_get_cx_data(&enc, &iter);
  EXPECT_EQ(pkt, nullptr);

  // Flush encoder
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_encode(&enc, nullptr, 0, 1, 0));
  iter = nullptr;
  pkt = aom_codec_get_cx_data(&enc, &iter);
  EXPECT_EQ(pkt, nullptr);

  EXPECT_EQ(AOM_CODEC_OK, aom_codec_destroy(&enc));
}

// This test emulates how libavif calls libaom functions to encode a
// progressive AVIF image in libavif's ProgressiveTest.DimensionChange test.
TEST(AVIFProgressiveTest, DimensionChange) {
  constexpr int kWidth = 256;
  constexpr int kHeight = 256;
  // A buffer of neutral gray samples.
  constexpr size_t kBufferSize = 3 * kWidth * kHeight;
  std::vector<unsigned char> buffer(kBufferSize,
                                    static_cast<unsigned char>(128));

  aom_image_t img;
  EXPECT_EQ(&img, aom_img_wrap(&img, AOM_IMG_FMT_I444, kWidth, kHeight, 1,
                               buffer.data()));
  img.cp = AOM_CICP_CP_UNSPECIFIED;
  img.tc = AOM_CICP_TC_UNSPECIFIED;
  img.mc = AOM_CICP_MC_UNSPECIFIED;
  img.range = AOM_CR_FULL_RANGE;

  aom_codec_iface_t *iface = aom_codec_av1_cx();
  aom_codec_enc_cfg_t cfg;
  EXPECT_EQ(AOM_CODEC_OK,
            aom_codec_enc_config_default(iface, &cfg, AOM_USAGE_GOOD_QUALITY));
  cfg.g_profile = 1;
  cfg.g_w = kWidth;
  cfg.g_h = kHeight;
  cfg.g_bit_depth = AOM_BITS_8;
  cfg.g_input_bit_depth = 8;
  cfg.g_lag_in_frames = 0;
  cfg.rc_end_usage = AOM_Q;
  cfg.rc_min_quantizer = 0;
  cfg.rc_max_quantizer = 0;
  aom_codec_ctx_t enc;
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_enc_init(&enc, iface, &cfg, 0));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_control(&enc, AOME_SET_CQ_LEVEL, 0));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_control(&enc, AV1E_SET_LOSSLESS, 1));
  EXPECT_EQ(AOM_CODEC_OK,
            aom_codec_control(&enc, AOME_SET_NUMBER_SPATIAL_LAYERS, 2));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_control(&enc, AOME_SET_CPUUSED, 6));
  EXPECT_EQ(AOM_CODEC_OK,
            aom_codec_control(&enc, AV1E_SET_COLOR_RANGE, AOM_CR_FULL_RANGE));
  EXPECT_EQ(AOM_CODEC_OK,
            aom_codec_control(&enc, AOME_SET_TUNING, AOM_TUNE_SSIM));

  // First frame (layer 0)
  EXPECT_EQ(AOM_CODEC_OK,
            aom_codec_control(&enc, AOME_SET_SPATIAL_LAYER_ID, 0));
  const aom_scaling_mode_t scaling_mode = { AOME_ONETWO, AOME_ONETWO };
  EXPECT_EQ(AOM_CODEC_OK,
            aom_codec_control(&enc, AOME_SET_SCALEMODE, &scaling_mode));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_encode(&enc, &img, 0, 1, 0));
  aom_codec_iter_t iter = nullptr;
  const aom_codec_cx_pkt_t *pkt = aom_codec_get_cx_data(&enc, &iter);
  ASSERT_NE(pkt, nullptr);
  EXPECT_EQ(pkt->kind, AOM_CODEC_CX_FRAME_PKT);
  // pkt->data.frame.flags is 0x1f0011.
  EXPECT_EQ(pkt->data.frame.flags & AOM_FRAME_IS_KEY, AOM_FRAME_IS_KEY);
  pkt = aom_codec_get_cx_data(&enc, &iter);
  EXPECT_EQ(pkt, nullptr);

  // Second frame (layer 1)
  EXPECT_EQ(AOM_CODEC_OK,
            aom_codec_control(&enc, AOME_SET_SPATIAL_LAYER_ID, 1));
  aom_enc_frame_flags_t encode_flags =
      AOM_EFLAG_NO_REF_GF | AOM_EFLAG_NO_REF_ARF | AOM_EFLAG_NO_REF_BWD |
      AOM_EFLAG_NO_REF_ARF2 | AOM_EFLAG_NO_UPD_GF | AOM_EFLAG_NO_UPD_ARF;
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_encode(&enc, &img, 0, 1, encode_flags));
  iter = nullptr;
  pkt = aom_codec_get_cx_data(&enc, &iter);
  ASSERT_NE(pkt, nullptr);
  EXPECT_EQ(pkt->kind, AOM_CODEC_CX_FRAME_PKT);
  // pkt->data.frame.flags is 0.
  EXPECT_EQ(pkt->data.frame.flags & AOM_FRAME_IS_KEY, 0u);
  pkt = aom_codec_get_cx_data(&enc, &iter);
  EXPECT_EQ(pkt, nullptr);

  // Flush encoder
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_encode(&enc, nullptr, 0, 1, 0));
  iter = nullptr;
  pkt = aom_codec_get_cx_data(&enc, &iter);
  EXPECT_EQ(pkt, nullptr);

  EXPECT_EQ(AOM_CODEC_OK, aom_codec_destroy(&enc));
}

// This test reproduces bug aomedia:3382. Certain parameters such as width,
// height, g_threads, usage, etc. were carefully chosen based on the
// complicated logic of av1_select_sb_size() to cause an inconsistent sb_size.
TEST(AVIFProgressiveTest, DimensionChangeBigImageMultiThread) {
  constexpr int kWidth = 1920;
  constexpr int kHeight = 1080;
  // A buffer of neutral gray samples.
  constexpr size_t kBufferSize = 2 * kWidth * kHeight;
  std::vector<unsigned char> buffer(kBufferSize,
                                    static_cast<unsigned char>(128));

  aom_image_t img;
  EXPECT_EQ(&img, aom_img_wrap(&img, AOM_IMG_FMT_I420, kWidth, kHeight, 1,
                               buffer.data()));
  img.cp = AOM_CICP_CP_UNSPECIFIED;
  img.tc = AOM_CICP_TC_UNSPECIFIED;
  img.mc = AOM_CICP_MC_UNSPECIFIED;
  img.range = AOM_CR_FULL_RANGE;

  aom_codec_iface_t *iface = aom_codec_av1_cx();
  aom_codec_enc_cfg_t cfg;
  EXPECT_EQ(AOM_CODEC_OK,
            aom_codec_enc_config_default(iface, &cfg, AOM_USAGE_GOOD_QUALITY));
  cfg.g_profile = 0;
  cfg.g_w = img.w;
  cfg.g_h = img.h;
  cfg.g_bit_depth = AOM_BITS_8;
  cfg.g_input_bit_depth = 8;
  cfg.g_lag_in_frames = 0;
  cfg.g_threads = 2;  // MultiThread
  cfg.rc_end_usage = AOM_Q;
  cfg.rc_min_quantizer = 0;
  cfg.rc_max_quantizer = 63;
  aom_codec_ctx_t enc;
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_enc_init(&enc, iface, &cfg, 0));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_control(&enc, AOME_SET_CQ_LEVEL, 31));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_control(&enc, AOME_SET_CPUUSED, 6));
  EXPECT_EQ(AOM_CODEC_OK,
            aom_codec_control(&enc, AV1E_SET_ROW_MT, 1));  // MultiThread
  EXPECT_EQ(AOM_CODEC_OK,
            aom_codec_control(&enc, AV1E_SET_COLOR_RANGE, AOM_CR_FULL_RANGE));
  EXPECT_EQ(AOM_CODEC_OK,
            aom_codec_control(&enc, AOME_SET_TUNING, AOM_TUNE_SSIM));
  EXPECT_EQ(AOM_CODEC_OK,
            aom_codec_control(&enc, AOME_SET_NUMBER_SPATIAL_LAYERS, 2));

  // First frame (layer 0)
  EXPECT_EQ(AOM_CODEC_OK,
            aom_codec_control(&enc, AOME_SET_SPATIAL_LAYER_ID, 0));
  const aom_scaling_mode_t scaling_mode = { AOME_ONETWO, AOME_ONETWO };
  EXPECT_EQ(AOM_CODEC_OK,
            aom_codec_control(&enc, AOME_SET_SCALEMODE, &scaling_mode));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_encode(&enc, &img, 0, 1, 0));
  aom_codec_iter_t iter = nullptr;
  const aom_codec_cx_pkt_t *pkt = aom_codec_get_cx_data(&enc, &iter);
  ASSERT_NE(pkt, nullptr);
  EXPECT_EQ(pkt->kind, AOM_CODEC_CX_FRAME_PKT);
  // pkt->data.frame.flags is 0x1f0011.
  EXPECT_EQ(pkt->data.frame.flags & AOM_FRAME_IS_KEY, AOM_FRAME_IS_KEY);
  pkt = aom_codec_get_cx_data(&enc, &iter);
  EXPECT_EQ(pkt, nullptr);

  // Second frame (layer 1)
  EXPECT_EQ(AOM_CODEC_OK,
            aom_codec_control(&enc, AOME_SET_SPATIAL_LAYER_ID, 1));
  aom_enc_frame_flags_t encode_flags =
      AOM_EFLAG_NO_REF_GF | AOM_EFLAG_NO_REF_ARF | AOM_EFLAG_NO_REF_BWD |
      AOM_EFLAG_NO_REF_ARF2 | AOM_EFLAG_NO_UPD_GF | AOM_EFLAG_NO_UPD_ARF;
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_encode(&enc, &img, 0, 1, encode_flags));
  iter = nullptr;
  pkt = aom_codec_get_cx_data(&enc, &iter);
  ASSERT_NE(pkt, nullptr);
  EXPECT_EQ(pkt->kind, AOM_CODEC_CX_FRAME_PKT);
  // pkt->data.frame.flags is 0.
  EXPECT_EQ(pkt->data.frame.flags & AOM_FRAME_IS_KEY, 0u);
  pkt = aom_codec_get_cx_data(&enc, &iter);
  EXPECT_EQ(pkt, nullptr);

  // Flush encoder
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_encode(&enc, nullptr, 0, 1, 0));
  iter = nullptr;
  pkt = aom_codec_get_cx_data(&enc, &iter);
  EXPECT_EQ(pkt, nullptr);

  EXPECT_EQ(AOM_CODEC_OK, aom_codec_destroy(&enc));
}

// A variant of the previous test, without the spatial layers.
TEST(AVIFProgressiveTest, DimensionChangeBigImageMultiThread2) {
  constexpr int kWidth = 1920;
  constexpr int kHeight = 1080;
  // A buffer of neutral gray samples.
  constexpr size_t kBufferSize = 2 * kWidth * kHeight;
  std::vector<unsigned char> buffer(kBufferSize,
                                    static_cast<unsigned char>(128));

  aom_image_t img;
  EXPECT_EQ(&img, aom_img_wrap(&img, AOM_IMG_FMT_I420, kWidth, kHeight, 1,
                               buffer.data()));
  img.cp = AOM_CICP_CP_UNSPECIFIED;
  img.tc = AOM_CICP_TC_UNSPECIFIED;
  img.mc = AOM_CICP_MC_UNSPECIFIED;
  img.range = AOM_CR_FULL_RANGE;

  aom_codec_iface_t *iface = aom_codec_av1_cx();
  aom_codec_enc_cfg_t cfg;
  EXPECT_EQ(AOM_CODEC_OK,
            aom_codec_enc_config_default(iface, &cfg, AOM_USAGE_GOOD_QUALITY));
  cfg.g_profile = 0;
  cfg.g_w = img.w;
  cfg.g_h = img.h;
  cfg.g_bit_depth = AOM_BITS_8;
  cfg.g_input_bit_depth = 8;
  cfg.g_lag_in_frames = 0;
  cfg.g_threads = 2;  // MultiThread
  cfg.rc_end_usage = AOM_Q;
  cfg.rc_min_quantizer = 0;
  cfg.rc_max_quantizer = 63;
  aom_codec_ctx_t enc;
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_enc_init(&enc, iface, &cfg, 0));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_control(&enc, AOME_SET_CQ_LEVEL, 31));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_control(&enc, AOME_SET_CPUUSED, 6));
  EXPECT_EQ(AOM_CODEC_OK,
            aom_codec_control(&enc, AV1E_SET_ROW_MT, 1));  // MultiThread
  EXPECT_EQ(AOM_CODEC_OK,
            aom_codec_control(&enc, AV1E_SET_COLOR_RANGE, AOM_CR_FULL_RANGE));
  EXPECT_EQ(AOM_CODEC_OK,
            aom_codec_control(&enc, AOME_SET_TUNING, AOM_TUNE_SSIM));

  // First frame
  const aom_scaling_mode_t scaling_mode = { AOME_ONETWO, AOME_ONETWO };
  EXPECT_EQ(AOM_CODEC_OK,
            aom_codec_control(&enc, AOME_SET_SCALEMODE, &scaling_mode));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_encode(&enc, &img, 0, 1, 0));
  aom_codec_iter_t iter = nullptr;
  const aom_codec_cx_pkt_t *pkt = aom_codec_get_cx_data(&enc, &iter);
  ASSERT_NE(pkt, nullptr);
  EXPECT_EQ(pkt->kind, AOM_CODEC_CX_FRAME_PKT);
  // pkt->data.frame.flags is 0x1f0011.
  EXPECT_EQ(pkt->data.frame.flags & AOM_FRAME_IS_KEY, AOM_FRAME_IS_KEY);
  pkt = aom_codec_get_cx_data(&enc, &iter);
  EXPECT_EQ(pkt, nullptr);

  // Second frame
  aom_enc_frame_flags_t encode_flags =
      AOM_EFLAG_NO_REF_GF | AOM_EFLAG_NO_REF_ARF | AOM_EFLAG_NO_REF_BWD |
      AOM_EFLAG_NO_REF_ARF2 | AOM_EFLAG_NO_UPD_GF | AOM_EFLAG_NO_UPD_ARF;
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_encode(&enc, &img, 0, 1, encode_flags));
  iter = nullptr;
  pkt = aom_codec_get_cx_data(&enc, &iter);
  ASSERT_NE(pkt, nullptr);
  EXPECT_EQ(pkt->kind, AOM_CODEC_CX_FRAME_PKT);
  // pkt->data.frame.flags is 0.
  EXPECT_EQ(pkt->data.frame.flags & AOM_FRAME_IS_KEY, 0u);
  pkt = aom_codec_get_cx_data(&enc, &iter);
  EXPECT_EQ(pkt, nullptr);

  // Flush encoder
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_encode(&enc, nullptr, 0, 1, 0));
  iter = nullptr;
  pkt = aom_codec_get_cx_data(&enc, &iter);
  EXPECT_EQ(pkt, nullptr);

  EXPECT_EQ(AOM_CODEC_OK, aom_codec_destroy(&enc));
}

}  // namespace
