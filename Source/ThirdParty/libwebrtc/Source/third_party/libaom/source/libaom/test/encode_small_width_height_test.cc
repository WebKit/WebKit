/*
 * Copyright (c) 2020, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

// Tests for https://crbug.com/aomedia/2777.
//
// Encode images with a small width (<= two AV1 superblocks) or a small height
// (<= one AV1 superblock) with multiple threads. aom_codec_encode() should
// not crash.

#include <memory>

#include "aom/aomcx.h"
#include "aom/aom_encoder.h"
#include "config/aom_config.h"
#include "gtest/gtest.h"

namespace {

// Dummy buffer of zero samples.
constexpr unsigned char kBuffer[2 * (256 * 512 + 2 * 128 * 256)] = { 0 };
#if CONFIG_REALTIME_ONLY
const int kUsage = 1;
#else
const int kUsage = 0;
#endif

void EncodeSmallWidthMultiThreaded(aom_img_fmt fmt, aom_codec_flags_t flag) {
  // The image has only one tile and the tile is two AV1 superblocks wide.
  // For speed >= 1, superblock size is 64x64 (see av1_select_sb_size()).
  constexpr int kWidth = 128;
  constexpr int kHeight = 512;

  aom_image_t img;
  EXPECT_EQ(&img, aom_img_wrap(&img, fmt, kWidth, kHeight, 1,
                               const_cast<unsigned char *>(kBuffer)));

  aom_codec_iface_t *iface = aom_codec_av1_cx();
  aom_codec_enc_cfg_t cfg;
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_enc_config_default(iface, &cfg, kUsage));
  cfg.g_threads = 2;
  cfg.g_w = kWidth;
  cfg.g_h = kHeight;
  aom_codec_ctx_t enc;
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_enc_init(&enc, iface, &cfg, flag));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_control(&enc, AOME_SET_CPUUSED, 5));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_encode(&enc, &img, 0, 1, 0));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_encode(&enc, nullptr, 0, 0, 0));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_destroy(&enc));
}

TEST(EncodeSmallWidthHeight, SmallWidthMultiThreaded) {
  EncodeSmallWidthMultiThreaded(AOM_IMG_FMT_I420, 0);
}

#if CONFIG_AV1_HIGHBITDEPTH
TEST(HighbdEncodeSmallWidthHeight, SmallWidthMultiThreaded) {
  EncodeSmallWidthMultiThreaded(AOM_IMG_FMT_I42016, AOM_CODEC_USE_HIGHBITDEPTH);
}
#endif  // CONFIG_AV1_HIGHBITDEPTH

#if !CONFIG_REALTIME_ONLY
void EncodeSmallWidthMultiThreadedSpeed0(aom_img_fmt fmt,
                                         aom_codec_flags_t flag) {
  // The image has only one tile and the tile is two AV1 superblocks wide.
  // For speed 0, superblock size is 128x128 (see av1_select_sb_size()).
  constexpr int kWidth = 256;
  constexpr int kHeight = 512;

  aom_image_t img;
  EXPECT_EQ(&img, aom_img_wrap(&img, fmt, kWidth, kHeight, 1,
                               const_cast<unsigned char *>(kBuffer)));

  aom_codec_iface_t *iface = aom_codec_av1_cx();
  aom_codec_enc_cfg_t cfg;
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_enc_config_default(iface, &cfg, 0));
  cfg.g_threads = 2;
  cfg.g_w = kWidth;
  cfg.g_h = kHeight;
  aom_codec_ctx_t enc;
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_enc_init(&enc, iface, &cfg, flag));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_control(&enc, AOME_SET_CPUUSED, 0));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_encode(&enc, &img, 0, 1, 0));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_encode(&enc, nullptr, 0, 0, 0));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_destroy(&enc));
}

TEST(EncodeSmallWidthHeight, SmallWidthMultiThreadedSpeed0) {
  EncodeSmallWidthMultiThreadedSpeed0(AOM_IMG_FMT_I420, 0);
}

#if CONFIG_AV1_HIGHBITDEPTH
TEST(HighbdEncodeSmallWidthHeight, SmallWidthMultiThreadedSpeed0) {
  EncodeSmallWidthMultiThreadedSpeed0(AOM_IMG_FMT_I42016,
                                      AOM_CODEC_USE_HIGHBITDEPTH);
}
#endif  // CONFIG_AV1_HIGHBITDEPTH

#endif

void EncodeSmallHeightMultiThreaded(aom_img_fmt fmt, aom_codec_flags_t flag) {
  // The image has only one tile and the tile is one AV1 superblock tall.
  // For speed >= 1, superblock size is 64x64 (see av1_select_sb_size()).
  constexpr int kWidth = 512;
  constexpr int kHeight = 64;

  aom_image_t img;
  EXPECT_EQ(&img, aom_img_wrap(&img, fmt, kWidth, kHeight, 1,
                               const_cast<unsigned char *>(kBuffer)));

  aom_codec_iface_t *iface = aom_codec_av1_cx();
  aom_codec_enc_cfg_t cfg;
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_enc_config_default(iface, &cfg, kUsage));
  cfg.g_threads = 2;
  cfg.g_w = kWidth;
  cfg.g_h = kHeight;
  aom_codec_ctx_t enc;
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_enc_init(&enc, iface, &cfg, flag));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_control(&enc, AOME_SET_CPUUSED, 5));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_encode(&enc, &img, 0, 1, 0));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_encode(&enc, nullptr, 0, 0, 0));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_destroy(&enc));
}

TEST(EncodeSmallWidthHeight, SmallHeightMultiThreaded) {
  EncodeSmallHeightMultiThreaded(AOM_IMG_FMT_I420, 0);
}

#if CONFIG_AV1_HIGHBITDEPTH
TEST(HighbdEncodeSmallWidthHeight, SmallHeightMultiThreaded) {
  EncodeSmallHeightMultiThreaded(AOM_IMG_FMT_I42016,
                                 AOM_CODEC_USE_HIGHBITDEPTH);
}
#endif  // CONFIG_AV1_HIGHBITDEPTH

#if !CONFIG_REALTIME_ONLY
void EncodeSmallHeightMultiThreadedSpeed0(aom_img_fmt fmt,
                                          aom_codec_flags_t flag) {
  // The image has only one tile and the tile is one AV1 superblock tall.
  // For speed 0, superblock size is 128x128 (see av1_select_sb_size()).
  constexpr int kWidth = 512;
  constexpr int kHeight = 128;

  aom_image_t img;
  EXPECT_EQ(&img, aom_img_wrap(&img, fmt, kWidth, kHeight, 1,
                               const_cast<unsigned char *>(kBuffer)));

  aom_codec_iface_t *iface = aom_codec_av1_cx();
  aom_codec_enc_cfg_t cfg;
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_enc_config_default(iface, &cfg, 0));
  cfg.g_threads = 2;
  cfg.g_w = kWidth;
  cfg.g_h = kHeight;
  aom_codec_ctx_t enc;
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_enc_init(&enc, iface, &cfg, flag));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_control(&enc, AOME_SET_CPUUSED, 0));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_encode(&enc, &img, 0, 1, 0));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_encode(&enc, nullptr, 0, 0, 0));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_destroy(&enc));
}

TEST(EncodeSmallWidthHeight, SmallHeightMultiThreadedSpeed0) {
  EncodeSmallHeightMultiThreadedSpeed0(AOM_IMG_FMT_I420, 0);
}

#if CONFIG_AV1_HIGHBITDEPTH
TEST(HighbdEncodeSmallWidthHeight, SmallHeightMultiThreadedSpeed0) {
  EncodeSmallHeightMultiThreadedSpeed0(AOM_IMG_FMT_I42016,
                                       AOM_CODEC_USE_HIGHBITDEPTH);
}
#endif  // CONFIG_AV1_HIGHBITDEPTH
#endif

// A reproducer test for aomedia:3113. The test should complete without any
// memory errors.
void Encode1x1(aom_img_fmt fmt, int bitdepth, aom_codec_flags_t flags) {
  constexpr int kWidth = 1;
  constexpr int kHeight = 1;

  // This test cannot use aom_img_alloc() or aom_img_wrap() because they call
  // align_image_dimension() to align img.w and img.h to the next even number
  // (2). In this test it is important to set img.w and img.h to 1. Therefore we
  // set up img manually.
  aom_image_t img;
  memset(&img, 0, sizeof(img));
  img.fmt = fmt;
  img.bit_depth = bitdepth;
  img.w = kWidth;
  img.h = kHeight;
  img.d_w = kWidth;
  img.d_h = kHeight;
  img.x_chroma_shift = 1;
  img.y_chroma_shift = 1;
  img.bps = 12;
  const int y_stride = kWidth;
  const int uv_stride = (kWidth + 1) >> 1;
  int y_height = kHeight;
  int uv_height = (kHeight + 1) >> 1;
  if (bitdepth > 8) {
    y_height <<= 1;
    uv_height <<= 1;
  }
  img.stride[AOM_PLANE_Y] = y_stride;
  img.stride[AOM_PLANE_U] = img.stride[AOM_PLANE_V] = uv_stride;
  std::unique_ptr<unsigned char[]> y_plane(
      new unsigned char[y_height * y_stride]());
  ASSERT_NE(y_plane, nullptr);
  std::unique_ptr<unsigned char[]> u_plane(
      new unsigned char[uv_height * uv_stride]());
  ASSERT_NE(u_plane, nullptr);
  std::unique_ptr<unsigned char[]> v_plane(
      new unsigned char[uv_height * uv_stride]());
  ASSERT_NE(v_plane, nullptr);
  img.planes[AOM_PLANE_Y] = y_plane.get();
  img.planes[AOM_PLANE_U] = u_plane.get();
  img.planes[AOM_PLANE_V] = v_plane.get();

  aom_codec_iface_t *iface = aom_codec_av1_cx();
  aom_codec_enc_cfg_t cfg;
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_enc_config_default(iface, &cfg, kUsage));
  cfg.g_w = kWidth;
  cfg.g_h = kHeight;
  aom_codec_ctx_t enc;
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_enc_init(&enc, iface, &cfg, flags));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_control(&enc, AOME_SET_CPUUSED, 5));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_encode(&enc, &img, 0, 1, 0));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_encode(&enc, nullptr, 0, 0, 0));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_destroy(&enc));
}

TEST(EncodeSmallWidthHeight, 1x1) { Encode1x1(AOM_IMG_FMT_I420, 8, 0); }

#if CONFIG_AV1_HIGHBITDEPTH
TEST(HighbdEncodeSmallWidthHeight, 1x1) {
  Encode1x1(AOM_IMG_FMT_I42016, 12, AOM_CODEC_USE_HIGHBITDEPTH);
}
#endif  // CONFIG_AV1_HIGHBITDEPTH

}  // namespace
