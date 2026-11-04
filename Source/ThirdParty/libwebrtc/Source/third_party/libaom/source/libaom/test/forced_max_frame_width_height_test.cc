/*
 * Copyright (c) 2022, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

// Tests for https://crbug.com/aomedia/3326.
//
// Set cfg.g_forced_max_frame_width and cfg.g_forced_max_frame_height and
// encode two frames of increasing sizes. The second aom_codec_encode() should
// not crash or have memory errors.

#include <algorithm>
#include <memory>
#include <vector>

#include "aom/aomcx.h"
#include "aom/aom_encoder.h"
#include "config/aom_config.h"
#if CONFIG_AV1_DECODER
#include "aom/aom_decoder.h"
#include "aom/aomdx.h"
#endif
#include "gtest/gtest.h"

namespace {

// cfg.g_lag_in_frames must be set to 0 or 1 to allow the frame size to change,
// as required by the following check in encoder_set_config() in
// av1/av1_cx_iface.c:
//
//   if (cfg->g_w != ctx->cfg.g_w || cfg->g_h != ctx->cfg.g_h) {
//     if (cfg->g_lag_in_frames > 1 || cfg->g_pass != AOM_RC_ONE_PASS)
//       ERROR("Cannot change width or height after initialization");
//     ...
//   }

void RunTest(unsigned int usage, unsigned int lag_in_frames,
             const char *tune_metric) {
  // A buffer of gray samples. Large enough for 128x128 and 256x256, YUV 4:2:0.
  constexpr size_t kImageDataSize = 256 * 256 + 2 * 128 * 128;
  std::unique_ptr<unsigned char[]> img_data(new unsigned char[kImageDataSize]);
  ASSERT_NE(img_data, nullptr);
  memset(img_data.get(), 128, kImageDataSize);

  aom_codec_iface_t *iface = aom_codec_av1_cx();
  aom_codec_enc_cfg_t cfg;
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_enc_config_default(iface, &cfg, usage));
  cfg.g_w = 128;
  cfg.g_h = 128;
  cfg.g_forced_max_frame_width = 256;
  cfg.g_forced_max_frame_height = 256;
  cfg.g_lag_in_frames = lag_in_frames;
  aom_codec_ctx_t enc;
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_enc_init(&enc, iface, &cfg, 0));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_set_option(&enc, "tune", tune_metric));

  aom_image_t img;
  EXPECT_EQ(&img,
            aom_img_wrap(&img, AOM_IMG_FMT_I420, 128, 128, 1, img_data.get()));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_encode(&enc, &img, 0, 1, 0));

  cfg.g_w = 256;
  cfg.g_h = 256;
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_enc_config_set(&enc, &cfg));

  EXPECT_EQ(&img,
            aom_img_wrap(&img, AOM_IMG_FMT_I420, 256, 256, 1, img_data.get()));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_encode(&enc, &img, 0, 1, 0));

  EXPECT_EQ(AOM_CODEC_OK, aom_codec_encode(&enc, nullptr, 0, 0, 0));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_destroy(&enc));
}

#if !CONFIG_REALTIME_ONLY

TEST(EncodeForcedMaxFrameWidthHeight, GoodQualityLag0TunePSNR) {
  RunTest(AOM_USAGE_GOOD_QUALITY, /*lag_in_frames=*/0, "psnr");
}

TEST(EncodeForcedMaxFrameWidthHeight, GoodQualityLag0TuneSSIM) {
  RunTest(AOM_USAGE_GOOD_QUALITY, /*lag_in_frames=*/0, "ssim");
}

TEST(EncodeForcedMaxFrameWidthHeight, GoodQualityLag1TunePSNR) {
  RunTest(AOM_USAGE_GOOD_QUALITY, /*lag_in_frames=*/1, "psnr");
}

TEST(EncodeForcedMaxFrameWidthHeight, GoodQualityLag1TuneSSIM) {
  RunTest(AOM_USAGE_GOOD_QUALITY, /*lag_in_frames=*/1, "ssim");
}

void FillImageGradient(aom_image_t *image, int bit_depth) {
  assert(image->range == AOM_CR_FULL_RANGE);
  for (int plane = 0; plane < 3; plane++) {
    const int plane_width = aom_img_plane_width(image, plane);
    const int plane_height = aom_img_plane_height(image, plane);
    unsigned char *row = image->planes[plane];
    const int stride = image->stride[plane];
    for (int y = 0; y < plane_height; ++y) {
      for (int x = 0; x < plane_width; ++x) {
        const int value = (x + y) * ((1 << bit_depth) - 1) /
                          std::max(1, plane_width + plane_height - 2);
        assert(value >= 0 && value <= (1 << bit_depth) - 1);
        if (bit_depth > 8) {
          reinterpret_cast<uint16_t *>(row)[x] = static_cast<uint16_t>(value);
        } else {
          row[x] = static_cast<unsigned char>(value);
        }
      }
      row += stride;
    }
  }
}

TEST(EncodeForcedMaxFrameWidthHeight, DimensionDecreasing) {
  constexpr int kWidth = 128;
  constexpr int kHeight = 128;
  constexpr size_t kBufferSize = 3 * kWidth * kHeight;
  std::vector<unsigned char> buffer(kBufferSize);

  aom_image_t img;
  EXPECT_EQ(&img, aom_img_wrap(&img, AOM_IMG_FMT_I420, kWidth, kHeight, 1,
                               buffer.data()));
  img.cp = AOM_CICP_CP_UNSPECIFIED;
  img.tc = AOM_CICP_TC_UNSPECIFIED;
  img.mc = AOM_CICP_MC_UNSPECIFIED;
  img.range = AOM_CR_FULL_RANGE;
  FillImageGradient(&img, 8);

  aom_codec_iface_t *iface = aom_codec_av1_cx();
  aom_codec_enc_cfg_t cfg;
  EXPECT_EQ(AOM_CODEC_OK,
            aom_codec_enc_config_default(iface, &cfg, AOM_USAGE_GOOD_QUALITY));
  cfg.rc_end_usage = AOM_Q;
  cfg.g_profile = 0;
  cfg.g_bit_depth = AOM_BITS_8;
  cfg.g_input_bit_depth = 8;
  cfg.g_w = kWidth;
  cfg.g_h = kHeight;
  cfg.g_forced_max_frame_width = kWidth;
  cfg.g_forced_max_frame_height = kHeight;
  cfg.g_lag_in_frames = 1;
  cfg.rc_min_quantizer = 20;
  cfg.rc_max_quantizer = 40;
  aom_codec_ctx_t enc;
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_enc_init(&enc, iface, &cfg, 0));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_control(&enc, AOME_SET_CQ_LEVEL, 30));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_control(&enc, AOME_SET_CPUUSED, 6));
  EXPECT_EQ(AOM_CODEC_OK,
            aom_codec_control(&enc, AV1E_SET_COLOR_RANGE, AOM_CR_FULL_RANGE));
  EXPECT_EQ(AOM_CODEC_OK,
            aom_codec_control(&enc, AOME_SET_TUNING, AOM_TUNE_SSIM));

  // First frame
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_encode(&enc, &img, 0, 1, 0));
  aom_codec_iter_t iter = nullptr;
  const aom_codec_cx_pkt_t *pkt = aom_codec_get_cx_data(&enc, &iter);
  ASSERT_NE(pkt, nullptr);
  EXPECT_EQ(pkt->kind, AOM_CODEC_CX_FRAME_PKT);
  // pkt->data.frame.flags is 0x1f0011.
  EXPECT_NE(pkt->data.frame.flags & AOM_FRAME_IS_KEY, 0u);
  pkt = aom_codec_get_cx_data(&enc, &iter);
  EXPECT_EQ(pkt, nullptr);

  // Second frame
  constexpr int kWidthSmall = 64;
  constexpr int kHeightSmall = 64;
  EXPECT_EQ(&img, aom_img_wrap(&img, AOM_IMG_FMT_I420, kWidthSmall,
                               kHeightSmall, 1, buffer.data()));
  img.cp = AOM_CICP_CP_UNSPECIFIED;
  img.tc = AOM_CICP_TC_UNSPECIFIED;
  img.mc = AOM_CICP_MC_UNSPECIFIED;
  img.range = AOM_CR_FULL_RANGE;
  FillImageGradient(&img, 8);
  cfg.g_w = kWidthSmall;
  cfg.g_h = kHeightSmall;
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_enc_config_set(&enc, &cfg));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_encode(&enc, &img, 0, 1, 0));
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
#endif  // !CONFIG_REALTIME_ONLY

TEST(EncodeForcedMaxFrameWidthHeight, RealtimeLag0TunePSNR) {
  RunTest(AOM_USAGE_REALTIME, /*lag_in_frames=*/0, "psnr");
}

TEST(EncodeForcedMaxFrameWidthHeight, RealtimeLag0TuneSSIM) {
  RunTest(AOM_USAGE_REALTIME, /*lag_in_frames=*/0, "ssim");
}

TEST(EncodeForcedMaxFrameWidthHeight, RealtimeLag1TunePSNR) {
  RunTest(AOM_USAGE_REALTIME, /*lag_in_frames=*/1, "psnr");
}

TEST(EncodeForcedMaxFrameWidthHeight, RealtimeLag1TuneSSIM) {
  RunTest(AOM_USAGE_REALTIME, /*lag_in_frames=*/1, "ssim");
}

TEST(EncodeForcedMaxFrameWidthHeight, MaxFrameSizeTooBig) {
  aom_codec_iface_t *iface = aom_codec_av1_cx();
  aom_codec_enc_cfg_t cfg;
  EXPECT_EQ(AOM_CODEC_OK,
            aom_codec_enc_config_default(iface, &cfg, AOM_USAGE_REALTIME));
  cfg.g_w = 256;
  cfg.g_h = 256;
  cfg.g_forced_max_frame_width = 131072;
  cfg.g_forced_max_frame_height = 131072;
  aom_codec_ctx_t enc;
  EXPECT_EQ(AOM_CODEC_INVALID_PARAM, aom_codec_enc_init(&enc, iface, &cfg, 0));
}

TEST(EncodeForcedMaxFrameWidthHeight, FirstFrameTooBig) {
  aom_codec_iface_t *iface = aom_codec_av1_cx();
  aom_codec_enc_cfg_t cfg;
  EXPECT_EQ(AOM_CODEC_OK,
            aom_codec_enc_config_default(iface, &cfg, AOM_USAGE_REALTIME));
  cfg.g_w = 258;
  cfg.g_h = 256;
  cfg.g_forced_max_frame_width = 256;
  cfg.g_forced_max_frame_height = 256;
  aom_codec_ctx_t enc;
  EXPECT_EQ(AOM_CODEC_INVALID_PARAM, aom_codec_enc_init(&enc, iface, &cfg, 0));
  cfg.g_w = 256;
  cfg.g_h = 258;
  EXPECT_EQ(AOM_CODEC_INVALID_PARAM, aom_codec_enc_init(&enc, iface, &cfg, 0));
  cfg.g_w = 256;
  cfg.g_h = 256;
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_enc_init(&enc, iface, &cfg, 0));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_destroy(&enc));
}

TEST(EncodeForcedMaxFrameWidthHeight, SecondFrameTooBig) {
  // A buffer of gray samples. Large enough for 128x128 and 256x256, YUV 4:2:0.
  constexpr size_t kImageDataSize = 256 * 256 + 2 * 128 * 128;
  std::unique_ptr<unsigned char[]> img_data(new unsigned char[kImageDataSize]);
  ASSERT_NE(img_data, nullptr);
  memset(img_data.get(), 128, kImageDataSize);

  aom_codec_iface_t *iface = aom_codec_av1_cx();
  aom_codec_enc_cfg_t cfg;
  EXPECT_EQ(AOM_CODEC_OK,
            aom_codec_enc_config_default(iface, &cfg, AOM_USAGE_REALTIME));
  cfg.g_w = 128;
  cfg.g_h = 128;
  cfg.g_forced_max_frame_width = 255;
  cfg.g_forced_max_frame_height = 256;
  aom_codec_ctx_t enc;
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_enc_init(&enc, iface, &cfg, 0));

  aom_image_t img;
  EXPECT_EQ(&img,
            aom_img_wrap(&img, AOM_IMG_FMT_I420, 128, 128, 1, img_data.get()));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_encode(&enc, &img, 0, 1, 0));

  cfg.g_w = 256;
  cfg.g_h = 256;
  EXPECT_EQ(AOM_CODEC_INVALID_PARAM, aom_codec_enc_config_set(&enc, &cfg));

  EXPECT_EQ(AOM_CODEC_OK, aom_codec_destroy(&enc));
}

// A regression test for bug 555350218.
TEST(EncodeForcedMaxFrameWidthHeight, ReducedStillPictureHeader) {
  constexpr size_t kImageDataSize = 128 * 128 + 2 * 64 * 64;
  std::unique_ptr<unsigned char[]> img_data(new unsigned char[kImageDataSize]);
  ASSERT_NE(img_data, nullptr);
  memset(img_data.get(), 128, kImageDataSize);
  aom_image_t img;
  EXPECT_EQ(&img,
            aom_img_wrap(&img, AOM_IMG_FMT_I420, 128, 128, 1, img_data.get()));

  aom_codec_iface_t *iface = aom_codec_av1_cx();
  aom_codec_enc_cfg_t cfg;
  EXPECT_EQ(AOM_CODEC_OK,
            aom_codec_enc_config_default(iface, &cfg, AOM_USAGE_REALTIME));
  cfg.g_w = 128;
  cfg.g_h = 128;
  cfg.g_limit = 1;

  aom_codec_ctx_t enc;

  cfg.g_forced_max_frame_width = 128;
  cfg.g_forced_max_frame_height = 128;
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_enc_init(&enc, iface, &cfg, 0));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_encode(&enc, &img, 0, 1, 0));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_destroy(&enc));

  cfg.g_forced_max_frame_width = 256;
  cfg.g_forced_max_frame_height = 256;
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_enc_init(&enc, iface, &cfg, 0));
  EXPECT_NE(AOM_CODEC_OK, aom_codec_encode(&enc, &img, 0, 1, 0));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_destroy(&enc));
}

TEST(EncodeForcedMaxFrameWidthHeight, DynamicUpscaleDoesNotForceKeyframe) {
  constexpr size_t kImageDataSize = 256 * 256 + 2 * 128 * 128;
  std::unique_ptr<unsigned char[]> img_data(new unsigned char[kImageDataSize]);
  ASSERT_NE(img_data, nullptr);
  memset(img_data.get(), 128, kImageDataSize);

  aom_codec_iface_t *iface = aom_codec_av1_cx();
  aom_codec_enc_cfg_t cfg;
  EXPECT_EQ(AOM_CODEC_OK,
            aom_codec_enc_config_default(iface, &cfg, AOM_USAGE_REALTIME));
  cfg.g_w = 128;
  cfg.g_h = 128;
  cfg.g_forced_max_frame_width = 256;
  cfg.g_forced_max_frame_height = 256;
  cfg.g_lag_in_frames = 0;
  cfg.kf_mode = AOM_KF_DISABLED;
  aom_codec_ctx_t enc;
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_enc_init(&enc, iface, &cfg, 0));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_control(&enc, AOME_SET_CPUUSED, 7));

#if CONFIG_AV1_DECODER
  aom_codec_ctx_t dec;
  EXPECT_EQ(AOM_CODEC_OK,
            aom_codec_dec_init(&dec, aom_codec_av1_dx(), nullptr, 0));
#endif

  // Frame 0: 128x128 keyframe.
  aom_image_t img;
  EXPECT_EQ(&img,
            aom_img_wrap(&img, AOM_IMG_FMT_I420, 128, 128, 1, img_data.get()));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_encode(&enc, &img, 0, 1, 0));

  aom_codec_iter_t iter = nullptr;
  const aom_codec_cx_pkt_t *pkt = aom_codec_get_cx_data(&enc, &iter);
  ASSERT_NE(pkt, nullptr);
  EXPECT_EQ(pkt->kind, AOM_CODEC_CX_FRAME_PKT);
  EXPECT_TRUE(pkt->data.frame.flags & AOM_FRAME_IS_KEY);

#if CONFIG_AV1_DECODER
  EXPECT_EQ(
      AOM_CODEC_OK,
      aom_codec_decode(&dec, static_cast<const uint8_t *>(pkt->data.frame.buf),
                       pkt->data.frame.sz, nullptr));
  aom_codec_iter_t dec_iter = nullptr;
  aom_image_t *dec_img = aom_codec_get_frame(&dec, &dec_iter);
  ASSERT_NE(dec_img, nullptr);
  EXPECT_EQ(dec_img->d_w, 128u);
  EXPECT_EQ(dec_img->d_h, 128u);
#endif

  // Frame 1: Upscale to 256x256.
  cfg.g_w = 256;
  cfg.g_h = 256;
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_enc_config_set(&enc, &cfg));

  EXPECT_EQ(&img,
            aom_img_wrap(&img, AOM_IMG_FMT_I420, 256, 256, 1, img_data.get()));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_encode(&enc, &img, 1, 1, 0));

  iter = nullptr;
  pkt = aom_codec_get_cx_data(&enc, &iter);
  ASSERT_NE(pkt, nullptr);
  EXPECT_EQ(pkt->kind, AOM_CODEC_CX_FRAME_PKT);
  EXPECT_FALSE(pkt->data.frame.flags & AOM_FRAME_IS_KEY);

#if CONFIG_AV1_DECODER
  EXPECT_EQ(
      AOM_CODEC_OK,
      aom_codec_decode(&dec, static_cast<const uint8_t *>(pkt->data.frame.buf),
                       pkt->data.frame.sz, nullptr));
  dec_iter = nullptr;
  dec_img = aom_codec_get_frame(&dec, &dec_iter);
  ASSERT_NE(dec_img, nullptr);
  EXPECT_EQ(dec_img->d_w, 256u);
  EXPECT_EQ(dec_img->d_h, 256u);
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_destroy(&dec));
#endif

  EXPECT_EQ(AOM_CODEC_OK, aom_codec_encode(&enc, nullptr, 0, 0, 0));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_destroy(&enc));
}

TEST(EncodeForcedMaxFrameWidthHeight, DynamicUpscaleUpTo16x) {
  constexpr size_t kImageDataSize = 2048 * 2048 + 2 * 1024 * 1024;
  std::unique_ptr<unsigned char[]> img_data(new unsigned char[kImageDataSize]);
  ASSERT_NE(img_data, nullptr);
  memset(img_data.get(), 128, kImageDataSize);

  aom_codec_iface_t *iface = aom_codec_av1_cx();
  aom_codec_enc_cfg_t cfg;
  EXPECT_EQ(AOM_CODEC_OK,
            aom_codec_enc_config_default(iface, &cfg, AOM_USAGE_REALTIME));
  cfg.g_w = 128;
  cfg.g_h = 128;
  cfg.g_forced_max_frame_width = 2048;
  cfg.g_forced_max_frame_height = 2048;
  cfg.g_lag_in_frames = 0;
  cfg.kf_mode = AOM_KF_DISABLED;
  aom_codec_ctx_t enc;
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_enc_init(&enc, iface, &cfg, 0));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_control(&enc, AOME_SET_CPUUSED, 7));

#if CONFIG_AV1_DECODER
  aom_codec_ctx_t dec;
  EXPECT_EQ(AOM_CODEC_OK,
            aom_codec_dec_init(&dec, aom_codec_av1_dx(), nullptr, 0));
#endif

  // Frame 0: 128x128 keyframe.
  aom_image_t img;
  EXPECT_EQ(&img,
            aom_img_wrap(&img, AOM_IMG_FMT_I420, 128, 128, 1, img_data.get()));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_encode(&enc, &img, 0, 1, 0));

  aom_codec_iter_t iter = nullptr;
  const aom_codec_cx_pkt_t *pkt = aom_codec_get_cx_data(&enc, &iter);
  ASSERT_NE(pkt, nullptr);
  EXPECT_EQ(pkt->kind, AOM_CODEC_CX_FRAME_PKT);
  EXPECT_TRUE(pkt->data.frame.flags & AOM_FRAME_IS_KEY);

#if CONFIG_AV1_DECODER
  EXPECT_EQ(
      AOM_CODEC_OK,
      aom_codec_decode(&dec, static_cast<const uint8_t *>(pkt->data.frame.buf),
                       pkt->data.frame.sz, nullptr));
#endif

  // Frame 1: Upscale 16x to 2048x2048.
  cfg.g_w = 2048;
  cfg.g_h = 2048;
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_enc_config_set(&enc, &cfg));

  EXPECT_EQ(&img, aom_img_wrap(&img, AOM_IMG_FMT_I420, 2048, 2048, 1,
                               img_data.get()));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_encode(&enc, &img, 1, 1, 0));

  iter = nullptr;
  pkt = aom_codec_get_cx_data(&enc, &iter);
  ASSERT_NE(pkt, nullptr);
  EXPECT_EQ(pkt->kind, AOM_CODEC_CX_FRAME_PKT);
  EXPECT_FALSE(pkt->data.frame.flags & AOM_FRAME_IS_KEY);

#if CONFIG_AV1_DECODER
  EXPECT_EQ(
      AOM_CODEC_OK,
      aom_codec_decode(&dec, static_cast<const uint8_t *>(pkt->data.frame.buf),
                       pkt->data.frame.sz, nullptr));
  aom_codec_iter_t dec_iter = nullptr;
  aom_image_t *dec_img = aom_codec_get_frame(&dec, &dec_iter);
  ASSERT_NE(dec_img, nullptr);
  EXPECT_EQ(dec_img->d_w, 2048u);
  EXPECT_EQ(dec_img->d_h, 2048u);
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_destroy(&dec));
#endif

  EXPECT_EQ(AOM_CODEC_OK, aom_codec_encode(&enc, nullptr, 0, 0, 0));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_destroy(&enc));
}

// Values of the AV1_LEVEL enum in av1/common/enums.h, which is not part of the
// public API.
//
// Passing kLevelKeepStats as the target level only switches on level statistics
// collection. Unlike a real target level it does not constrain the encoder, so
// it does not change what the rest of the test measures.
constexpr int kSeqLevelIdx5_0 = 12;
constexpr int kLevelKeepStats = 32;
constexpr int kMaxOperatingPoints = 32;

TEST(EncodeForcedMaxFrameWidthHeight,
     SequenceLevelIndexUsesForcedMaxDimensions) {
  constexpr size_t kImageDataSize = 2560 * 1536 + 2 * 1280 * 768;
  std::unique_ptr<unsigned char[]> img_data(new unsigned char[kImageDataSize]);
  ASSERT_NE(img_data, nullptr);
  memset(img_data.get(), 128, kImageDataSize);

  aom_codec_iface_t *iface = aom_codec_av1_cx();
  aom_codec_enc_cfg_t cfg;
  EXPECT_EQ(AOM_CODEC_OK,
            aom_codec_enc_config_default(iface, &cfg, AOM_USAGE_REALTIME));
  cfg.g_w = 640;
  cfg.g_h = 384;
  cfg.g_forced_max_frame_width = 2560;
  cfg.g_forced_max_frame_height = 1536;
  cfg.g_lag_in_frames = 0;
  cfg.kf_mode = AOM_KF_DISABLED;
  aom_codec_ctx_t enc;
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_enc_init(&enc, iface, &cfg, 0));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_control(&enc, AOME_SET_CPUUSED, 7));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_control(&enc, AV1E_SET_TARGET_SEQ_LEVEL_IDX,
                                            kLevelKeepStats));

#if CONFIG_AV1_DECODER
  aom_codec_ctx_t dec;
  EXPECT_EQ(AOM_CODEC_OK,
            aom_codec_dec_init(&dec, aom_codec_av1_dx(), nullptr, 0));
#endif

  // Frame 0: 640x384 keyframe.
  aom_image_t img;
  EXPECT_EQ(&img,
            aom_img_wrap(&img, AOM_IMG_FMT_I420, 640, 384, 1, img_data.get()));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_encode(&enc, &img, 0, 1, 0));

  aom_codec_iter_t iter = nullptr;
  const aom_codec_cx_pkt_t *pkt = aom_codec_get_cx_data(&enc, &iter);
  ASSERT_NE(pkt, nullptr);
  EXPECT_EQ(pkt->kind, AOM_CODEC_CX_FRAME_PKT);

#if CONFIG_AV1_DECODER
  EXPECT_EQ(
      AOM_CODEC_OK,
      aom_codec_decode(&dec, static_cast<const uint8_t *>(pkt->data.frame.buf),
                       pkt->data.frame.sz, nullptr));
#endif

  // Frame 1: Upscale 4x to 2560x1536.
  cfg.g_w = 2560;
  cfg.g_h = 1536;
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_enc_config_set(&enc, &cfg));

  EXPECT_EQ(&img, aom_img_wrap(&img, AOM_IMG_FMT_I420, 2560, 1536, 1,
                               img_data.get()));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_encode(&enc, &img, 1, 1, 0));

  iter = nullptr;
  pkt = aom_codec_get_cx_data(&enc, &iter);
  ASSERT_NE(pkt, nullptr);
  EXPECT_EQ(pkt->kind, AOM_CODEC_CX_FRAME_PKT);
  EXPECT_FALSE(pkt->data.frame.flags & AOM_FRAME_IS_KEY);

#if CONFIG_AV1_DECODER
  EXPECT_EQ(
      AOM_CODEC_OK,
      aom_codec_decode(&dec, static_cast<const uint8_t *>(pkt->data.frame.buf),
                       pkt->data.frame.sz, nullptr));
  aom_codec_iter_t dec_iter = nullptr;
  aom_image_t *dec_img = aom_codec_get_frame(&dec, &dec_iter);
  ASSERT_NE(dec_img, nullptr);
  EXPECT_EQ(dec_img->d_w, 2560u);
  EXPECT_EQ(dec_img->d_h, 1536u);
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_destroy(&dec));
#endif

  // The level the encoder derives from the statistics it collected has to
  // account for the upscaled frame, not just the initial 640x384 one.
  int seq_level_idx[kMaxOperatingPoints];
  EXPECT_EQ(AOM_CODEC_OK,
            aom_codec_control(&enc, AV1E_GET_SEQ_LEVEL_IDX, seq_level_idx));
  EXPECT_EQ(seq_level_idx[0], kSeqLevelIdx5_0);

  EXPECT_EQ(AOM_CODEC_OK, aom_codec_encode(&enc, nullptr, 0, 0, 0));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_destroy(&enc));
}

// Fills an 8-bit image with a non-flat pattern, different for each value of
// |seed|, so that the encoder codes coefficients in every block.
void FillImagePattern(aom_image_t *image, int seed) {
  for (int plane = 0; plane < 3; plane++) {
    const int plane_width = aom_img_plane_width(image, plane);
    const int plane_height = aom_img_plane_height(image, plane);
    for (int y = 0; y < plane_height; ++y) {
      unsigned char *row = image->planes[plane] + y * image->stride[plane];
      for (int x = 0; x < plane_width; ++x) {
        row[x] =
            static_cast<unsigned char>((x * 7 + y * 13 + seed * 37) ^ (x * y));
      }
    }
  }
}

// Raising g_forced_max_frame_width/height with aom_codec_enc_config_set()
// after a frame has been encoded, without changing the frame size, does not
// reallocate the coefficient buffer. It must still be indexed with the layout
// it was allocated with, not one computed from the new forced maximum.
void RunRaiseForcedMaxTest(unsigned int usage) {
  constexpr unsigned int kWidth = 256;
  constexpr unsigned int kHeight = 256;
  constexpr unsigned int kMaxWidth = 1024;
  constexpr unsigned int kMaxHeight = 1024;
  aom_codec_iface_t *iface = aom_codec_av1_cx();
  aom_codec_enc_cfg_t cfg;
  ASSERT_EQ(AOM_CODEC_OK, aom_codec_enc_config_default(iface, &cfg, usage));
  cfg.g_w = kWidth;
  cfg.g_h = kHeight;
  cfg.g_lag_in_frames = 0;
  aom_codec_ctx_t enc;
  ASSERT_EQ(AOM_CODEC_OK, aom_codec_enc_init(&enc, iface, &cfg, 0));
  ASSERT_EQ(AOM_CODEC_OK,
            aom_codec_control(&enc, AOME_SET_CPUUSED,
                              usage == AOM_USAGE_REALTIME ? 9 : 6));
  ASSERT_EQ(AOM_CODEC_OK, aom_codec_set_option(&enc, "tune", "ssim"));

  aom_image_t *image =
      aom_img_alloc(nullptr, AOM_IMG_FMT_I420, kWidth, kHeight, 1);
  ASSERT_NE(image, nullptr);
  FillImagePattern(image, 0);
  ASSERT_EQ(AOM_CODEC_OK, aom_codec_encode(&enc, image, 0, 1, 0));

  cfg.g_forced_max_frame_width = kMaxWidth;
  cfg.g_forced_max_frame_height = kMaxHeight;
  ASSERT_EQ(AOM_CODEC_OK, aom_codec_enc_config_set(&enc, &cfg));
  FillImagePattern(image, 1);
  ASSERT_EQ(AOM_CODEC_OK, aom_codec_encode(&enc, image, 1, 1, 0));

  ASSERT_EQ(AOM_CODEC_OK, aom_codec_encode(&enc, nullptr, 0, 0, 0));
  aom_img_free(image);
  ASSERT_EQ(AOM_CODEC_OK, aom_codec_destroy(&enc));
}

TEST(EncodeForcedMaxFrameWidthHeight, RaiseForcedMaxRealtime) {
  RunRaiseForcedMaxTest(AOM_USAGE_REALTIME);
}

#if !CONFIG_REALTIME_ONLY
TEST(EncodeForcedMaxFrameWidthHeight, RaiseForcedMaxGoodQuality) {
  RunRaiseForcedMaxTest(AOM_USAGE_GOOD_QUALITY);
}
#endif  // !CONFIG_REALTIME_ONLY

}  // namespace
