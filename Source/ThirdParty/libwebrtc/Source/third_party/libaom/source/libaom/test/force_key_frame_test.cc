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

// Tests for https://crbug.com/aomedia/3327.
//
// In good-quality mode, set cfg.g_lag_in_frames to 1 or 0 and encode two
// frames in one-pass mode. Pass AOM_EFLAG_FORCE_KF to the second
// aom_codec_encode() call. Both frames should be encoded as key frames.

#include <memory>

#include "aom/aomcx.h"
#include "aom/aom_encoder.h"
#include "gtest/gtest.h"

namespace {

void TestOnePassMode(unsigned int lag_in_frames) {
  // A buffer of gray samples of size 128x128, YUV 4:2:0.
  constexpr size_t kImageDataSize = 128 * 128 + 2 * 64 * 64;
  std::unique_ptr<unsigned char[]> img_data(new unsigned char[kImageDataSize]);
  ASSERT_NE(img_data, nullptr);
  memset(img_data.get(), 128, kImageDataSize);

  aom_codec_iface_t *iface = aom_codec_av1_cx();
  aom_codec_enc_cfg_t cfg;
  ASSERT_EQ(AOM_CODEC_OK,
            aom_codec_enc_config_default(iface, &cfg, AOM_USAGE_GOOD_QUALITY));
  cfg.g_w = 128;
  cfg.g_h = 128;
  cfg.g_pass = AOM_RC_ONE_PASS;
  cfg.g_lag_in_frames = lag_in_frames;
  aom_codec_ctx_t enc;
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_enc_init(&enc, iface, &cfg, 0));

  aom_image_t img;
  EXPECT_EQ(&img,
            aom_img_wrap(&img, AOM_IMG_FMT_I420, 128, 128, 1, img_data.get()));

  aom_codec_iter_t iter;
  const aom_codec_cx_pkt_t *pkt;
  int frame_count = 0;

  EXPECT_EQ(AOM_CODEC_OK, aom_codec_encode(&enc, &img, 0, 1, 0));

  iter = nullptr;
  while ((pkt = aom_codec_get_cx_data(&enc, &iter)) != nullptr) {
    ASSERT_EQ(pkt->kind, AOM_CODEC_CX_FRAME_PKT);
    EXPECT_NE(pkt->data.frame.flags & AOM_FRAME_IS_KEY, 0u)
        << "frame " << frame_count;
    frame_count++;
  }

  EXPECT_EQ(AOM_CODEC_OK,
            aom_codec_encode(&enc, &img, 1, 1, AOM_EFLAG_FORCE_KF));

  iter = nullptr;
  while ((pkt = aom_codec_get_cx_data(&enc, &iter)) != nullptr) {
    ASSERT_EQ(pkt->kind, AOM_CODEC_CX_FRAME_PKT);
    EXPECT_NE(pkt->data.frame.flags & AOM_FRAME_IS_KEY, 0u)
        << "frame " << frame_count;
    frame_count++;
  }

  EXPECT_EQ(AOM_CODEC_OK, aom_codec_encode(&enc, nullptr, 0, 0, 0));

  iter = nullptr;
  while ((pkt = aom_codec_get_cx_data(&enc, &iter)) != nullptr) {
    ASSERT_EQ(pkt->kind, AOM_CODEC_CX_FRAME_PKT);
    EXPECT_NE(pkt->data.frame.flags & AOM_FRAME_IS_KEY, 0u)
        << "frame " << frame_count;
    frame_count++;
  }

  EXPECT_EQ(frame_count, 2);
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_destroy(&enc));
}

TEST(ForceKeyFrameTest, OnePassModeLag0) { TestOnePassMode(0); }

TEST(ForceKeyFrameTest, OnePassModeLag1) { TestOnePassMode(1); }

TEST(ForceKeyFrameTest, OnePassModeLag2) { TestOnePassMode(2); }

}  // namespace
