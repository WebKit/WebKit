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

#include <cstdlib>
#include <cstring>
#include <tuple>

#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

#include "config/aom_config.h"

#include "aom/aomcx.h"
#include "aom/aom_encoder.h"
#include "aom/aom_image.h"

namespace {

#if CONFIG_REALTIME_ONLY
const int kUsage = AOM_USAGE_REALTIME;
#else
const int kUsage = AOM_USAGE_GOOD_QUALITY;
#endif

static void *Memset16(void *dest, int val, size_t length) {
  uint16_t *dest16 = (uint16_t *)dest;
  for (size_t i = 0; i < length; ++i) *dest16++ = val;
  return dest;
}

TEST(EncodeAPI, InvalidParams) {
  uint8_t buf[1] = { 0 };
  aom_image_t img;
  aom_codec_ctx_t enc;
  aom_codec_enc_cfg_t cfg;

  EXPECT_EQ(&img, aom_img_wrap(&img, AOM_IMG_FMT_I420, 1, 1, 1, buf));

  EXPECT_EQ(AOM_CODEC_INVALID_PARAM,
            aom_codec_enc_init(nullptr, nullptr, nullptr, 0));
  EXPECT_EQ(AOM_CODEC_INVALID_PARAM,
            aom_codec_enc_init(&enc, nullptr, nullptr, 0));
  EXPECT_EQ(AOM_CODEC_INVALID_PARAM,
            aom_codec_encode(nullptr, nullptr, 0, 0, 0));
  EXPECT_EQ(AOM_CODEC_INVALID_PARAM, aom_codec_encode(nullptr, &img, 0, 0, 0));
  EXPECT_EQ(AOM_CODEC_INVALID_PARAM, aom_codec_destroy(nullptr));
  EXPECT_EQ(AOM_CODEC_INVALID_PARAM,
            aom_codec_enc_config_default(nullptr, nullptr, 0));
  EXPECT_EQ(AOM_CODEC_INVALID_PARAM,
            aom_codec_enc_config_default(nullptr, &cfg, 0));
  EXPECT_NE(aom_codec_error(nullptr), nullptr);

  aom_codec_iface_t *iface = aom_codec_av1_cx();
  SCOPED_TRACE(aom_codec_iface_name(iface));
  EXPECT_EQ(AOM_CODEC_INVALID_PARAM,
            aom_codec_enc_init(nullptr, iface, nullptr, 0));
  EXPECT_EQ(AOM_CODEC_INVALID_PARAM,
            aom_codec_enc_init(&enc, iface, nullptr, 0));
  EXPECT_EQ(AOM_CODEC_INVALID_PARAM,
            aom_codec_enc_config_default(iface, &cfg, 3));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_enc_config_default(iface, &cfg, kUsage));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_enc_init(&enc, iface, &cfg, 0));
  EXPECT_EQ(nullptr, aom_codec_get_global_headers(nullptr));

  aom_fixed_buf_t *glob_headers = aom_codec_get_global_headers(&enc);
  EXPECT_NE(glob_headers->buf, nullptr);
  if (glob_headers) {
    free(glob_headers->buf);
    free(glob_headers);
  }
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_encode(&enc, nullptr, 0, 0, 0));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_destroy(&enc));
}

TEST(EncodeAPI, InvalidControlId) {
  aom_codec_iface_t *iface = aom_codec_av1_cx();
  aom_codec_ctx_t enc;
  aom_codec_enc_cfg_t cfg;
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_enc_config_default(iface, &cfg, kUsage));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_enc_init(&enc, iface, &cfg, 0));
  EXPECT_EQ(AOM_CODEC_ERROR, aom_codec_control(&enc, -1, 0));
  EXPECT_EQ(AOM_CODEC_INVALID_PARAM, aom_codec_control(&enc, 0, 0));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_destroy(&enc));
}

void EncodeSetSFrameOnFirstFrame(aom_img_fmt fmt, aom_codec_flags_t flag) {
  constexpr int kWidth = 2;
  constexpr int kHeight = 128;
  unsigned char kBuffer[kWidth * kHeight * 3] = { 0 };
  aom_image_t img;
  ASSERT_EQ(aom_img_wrap(&img, fmt, kWidth, kHeight, 1, kBuffer), &img);

  aom_codec_iface_t *iface = aom_codec_av1_cx();
  aom_codec_enc_cfg_t cfg;
  ASSERT_EQ(aom_codec_enc_config_default(iface, &cfg, kUsage), AOM_CODEC_OK);
  cfg.g_w = kWidth;
  cfg.g_h = kHeight;

  aom_codec_ctx_t enc;
  ASSERT_EQ(aom_codec_enc_init(&enc, iface, &cfg, flag), AOM_CODEC_OK);
  // One of these aom_codec_encode() calls should fail.
  if (aom_codec_encode(&enc, &img, 0, 1, AOM_EFLAG_SET_S_FRAME) ==
      AOM_CODEC_OK) {
    EXPECT_NE(aom_codec_encode(&enc, nullptr, 0, 0, 0), AOM_CODEC_OK);
  }
  EXPECT_EQ(aom_codec_destroy(&enc), AOM_CODEC_OK);
}

TEST(EncodeAPI, SetSFrameOnFirstFrame) {
  EncodeSetSFrameOnFirstFrame(AOM_IMG_FMT_I420, 0);
}

#if CONFIG_AV1_HIGHBITDEPTH
TEST(EncodeAPI, SetSFrameOnFirstFrameHighbd) {
  EncodeSetSFrameOnFirstFrame(AOM_IMG_FMT_I42016, AOM_CODEC_USE_HIGHBITDEPTH);
}
#endif  // CONFIG_AV1_HIGHBITDEPTH

TEST(EncodeAPI, MonochromeInProfiles) {
  aom_codec_iface_t *iface = aom_codec_av1_cx();
  aom_codec_enc_cfg_t cfg;
  ASSERT_EQ(AOM_CODEC_OK, aom_codec_enc_config_default(iface, &cfg, kUsage));
  cfg.g_w = 128;
  cfg.g_h = 128;
  cfg.monochrome = 1;
  aom_codec_ctx_t enc;

  // Test Profile 0
  cfg.g_profile = 0;
  ASSERT_EQ(AOM_CODEC_OK, aom_codec_enc_init(&enc, iface, &cfg, 0));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_destroy(&enc));

  // Test Profile 1
  cfg.g_profile = 1;
  ASSERT_EQ(AOM_CODEC_INVALID_PARAM, aom_codec_enc_init(&enc, iface, &cfg, 0));

  // Test Profile 3
  cfg.g_profile = 2;
  ASSERT_EQ(AOM_CODEC_OK, aom_codec_enc_init(&enc, iface, &cfg, 0));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_destroy(&enc));
}

TEST(EncodeAPI, LowBDEncoderLowBDImage) {
  aom_codec_iface_t *iface = aom_codec_av1_cx();
  aom_codec_enc_cfg_t cfg;
  ASSERT_EQ(aom_codec_enc_config_default(iface, &cfg, kUsage), AOM_CODEC_OK);

  aom_codec_ctx_t enc;
  ASSERT_EQ(aom_codec_enc_init(&enc, iface, &cfg, 0), AOM_CODEC_OK);

  aom_image_t *image =
      aom_img_alloc(NULL, AOM_IMG_FMT_I420, cfg.g_w, cfg.g_h, 0);
  ASSERT_NE(image, nullptr);

  // Set the image to two colors so that av1_set_screen_content_options() will
  // call av1_get_perpixel_variance().
  int luma_value = 0;
  for (unsigned int i = 0; i < image->d_h; ++i) {
    memset(image->planes[0] + i * image->stride[0], luma_value, image->d_w);
    luma_value = 255 - luma_value;
  }
  unsigned int uv_h = (image->d_h + 1) / 2;
  unsigned int uv_w = (image->d_w + 1) / 2;
  for (unsigned int i = 0; i < uv_h; ++i) {
    memset(image->planes[1] + i * image->stride[1], 128, uv_w);
    memset(image->planes[2] + i * image->stride[2], 128, uv_w);
  }

  ASSERT_EQ(aom_codec_encode(&enc, image, 0, 1, 0), AOM_CODEC_OK);

  aom_img_free(image);
  ASSERT_EQ(aom_codec_destroy(&enc), AOM_CODEC_OK);
}

TEST(EncodeAPI, HighBDEncoderHighBDImage) {
  aom_codec_iface_t *iface = aom_codec_av1_cx();
  aom_codec_enc_cfg_t cfg;
  ASSERT_EQ(aom_codec_enc_config_default(iface, &cfg, kUsage), AOM_CODEC_OK);

  aom_codec_ctx_t enc;
  aom_codec_err_t init_status =
      aom_codec_enc_init(&enc, iface, &cfg, AOM_CODEC_USE_HIGHBITDEPTH);
#if !CONFIG_AV1_HIGHBITDEPTH
  ASSERT_EQ(init_status, AOM_CODEC_INCAPABLE);
#else
  ASSERT_EQ(init_status, AOM_CODEC_OK);

  aom_image_t *image =
      aom_img_alloc(NULL, AOM_IMG_FMT_I42016, cfg.g_w, cfg.g_h, 0);
  ASSERT_NE(image, nullptr);

  // Set the image to two colors so that av1_set_screen_content_options() will
  // call av1_get_perpixel_variance().
  int luma_value = 0;
  for (unsigned int i = 0; i < image->d_h; ++i) {
    Memset16(image->planes[0] + i * image->stride[0], luma_value, image->d_w);
    luma_value = 255 - luma_value;
  }
  unsigned int uv_h = (image->d_h + 1) / 2;
  unsigned int uv_w = (image->d_w + 1) / 2;
  for (unsigned int i = 0; i < uv_h; ++i) {
    Memset16(image->planes[1] + i * image->stride[1], 128, uv_w);
    Memset16(image->planes[2] + i * image->stride[2], 128, uv_w);
  }

  ASSERT_EQ(aom_codec_encode(&enc, image, 0, 1, 0), AOM_CODEC_OK);

  aom_img_free(image);
  ASSERT_EQ(aom_codec_destroy(&enc), AOM_CODEC_OK);
#endif
}

TEST(EncodeAPI, HighBDEncoderLowBDImage) {
  aom_codec_iface_t *iface = aom_codec_av1_cx();
  aom_codec_enc_cfg_t cfg;
  ASSERT_EQ(aom_codec_enc_config_default(iface, &cfg, kUsage), AOM_CODEC_OK);

  aom_codec_ctx_t enc;
  aom_codec_err_t init_status =
      aom_codec_enc_init(&enc, iface, &cfg, AOM_CODEC_USE_HIGHBITDEPTH);
#if !CONFIG_AV1_HIGHBITDEPTH
  ASSERT_EQ(init_status, AOM_CODEC_INCAPABLE);
#else
  ASSERT_EQ(init_status, AOM_CODEC_OK);

  aom_image_t *image =
      aom_img_alloc(NULL, AOM_IMG_FMT_I420, cfg.g_w, cfg.g_h, 0);
  ASSERT_NE(image, nullptr);

  // Set the image to two colors so that av1_set_screen_content_options() will
  // call av1_get_perpixel_variance().
  int luma_value = 0;
  for (unsigned int i = 0; i < image->d_h; ++i) {
    memset(image->planes[0] + i * image->stride[0], luma_value, image->d_w);
    luma_value = 255 - luma_value;
  }
  unsigned int uv_h = (image->d_h + 1) / 2;
  unsigned int uv_w = (image->d_w + 1) / 2;
  for (unsigned int i = 0; i < uv_h; ++i) {
    memset(image->planes[1] + i * image->stride[1], 128, uv_w);
    memset(image->planes[2] + i * image->stride[2], 128, uv_w);
  }

  ASSERT_EQ(aom_codec_encode(&enc, image, 0, 1, 0), AOM_CODEC_INVALID_PARAM);

  aom_img_free(image);
  ASSERT_EQ(aom_codec_destroy(&enc), AOM_CODEC_OK);
#endif
}

TEST(EncodeAPI, LowBDEncoderHighBDImage) {
  aom_codec_iface_t *iface = aom_codec_av1_cx();
  aom_codec_enc_cfg_t cfg;
  ASSERT_EQ(aom_codec_enc_config_default(iface, &cfg, kUsage), AOM_CODEC_OK);

  aom_codec_ctx_t enc;
  ASSERT_EQ(aom_codec_enc_init(&enc, iface, &cfg, 0), AOM_CODEC_OK);

  aom_image_t *image =
      aom_img_alloc(NULL, AOM_IMG_FMT_I42016, cfg.g_w, cfg.g_h, 0);
  ASSERT_NE(image, nullptr);

  // Set the image to two colors so that av1_set_screen_content_options() will
  // call av1_get_perpixel_variance().
  int luma_value = 0;
  for (unsigned int i = 0; i < image->d_h; ++i) {
    Memset16(image->planes[0] + i * image->stride[0], luma_value, image->d_w);
    luma_value = 255 - luma_value;
  }
  unsigned int uv_h = (image->d_h + 1) / 2;
  unsigned int uv_w = (image->d_w + 1) / 2;
  for (unsigned int i = 0; i < uv_h; ++i) {
    Memset16(image->planes[1] + i * image->stride[1], 128, uv_w);
    Memset16(image->planes[2] + i * image->stride[2], 128, uv_w);
  }

  ASSERT_EQ(aom_codec_encode(&enc, image, 0, 1, 0), AOM_CODEC_INVALID_PARAM);

  aom_img_free(image);
  ASSERT_EQ(aom_codec_destroy(&enc), AOM_CODEC_OK);
}

class EncodeAPIParameterized
    : public testing::TestWithParam<
          std::tuple</*usage=*/int, /*speed=*/int, /*aq_mode=*/int>> {};

// Encodes two frames at a given usage, speed, and aq_mode setting.
// Reproduces b/303023614
TEST_P(EncodeAPIParameterized, HighBDEncoderHighBDFrames) {
  const int usage = std::get<0>(GetParam());
  int speed = std::get<1>(GetParam());

  if (speed == 10 && usage != AOM_USAGE_REALTIME) {
    speed = 9;  // 10 is only allowed in AOM_USAGE_REALTIME
  }

  aom_codec_iface_t *iface = aom_codec_av1_cx();
  aom_codec_enc_cfg_t cfg;
  ASSERT_EQ(aom_codec_enc_config_default(iface, &cfg, usage), AOM_CODEC_OK);
  cfg.g_w = 500;
  cfg.g_h = 400;

  aom_codec_ctx_t enc;
  aom_codec_err_t init_status =
      aom_codec_enc_init(&enc, iface, &cfg, AOM_CODEC_USE_HIGHBITDEPTH);
#if !CONFIG_AV1_HIGHBITDEPTH
  ASSERT_EQ(init_status, AOM_CODEC_INCAPABLE);
#else
  const int aq_mode = std::get<2>(GetParam());

  ASSERT_EQ(init_status, AOM_CODEC_OK);

  ASSERT_EQ(aom_codec_control(&enc, AOME_SET_CPUUSED, speed), AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_control(&enc, AV1E_SET_AQ_MODE, aq_mode), AOM_CODEC_OK);

  aom_image_t *image =
      aom_img_alloc(NULL, AOM_IMG_FMT_I42016, cfg.g_w, cfg.g_h, 0);
  ASSERT_NE(image, nullptr);

  for (unsigned int i = 0; i < image->d_h; ++i) {
    Memset16(image->planes[0] + i * image->stride[0], 128, image->d_w);
  }
  unsigned int uv_h = (image->d_h + 1) / 2;
  unsigned int uv_w = (image->d_w + 1) / 2;
  for (unsigned int i = 0; i < uv_h; ++i) {
    Memset16(image->planes[1] + i * image->stride[1], 128, uv_w);
    Memset16(image->planes[2] + i * image->stride[2], 128, uv_w);
  }

  // Encode two frames.
  ASSERT_EQ(
      aom_codec_encode(&enc, image, /*pts=*/0, /*duration=*/1, /*flags=*/0),
      AOM_CODEC_OK);
  ASSERT_EQ(
      aom_codec_encode(&enc, image, /*pts=*/1, /*duration=*/1, /*flags=*/0),
      AOM_CODEC_OK);

  aom_img_free(image);
  ASSERT_EQ(aom_codec_destroy(&enc), AOM_CODEC_OK);
#endif
}

const int kUsages[] = {
  AOM_USAGE_REALTIME,
#if !CONFIG_REALTIME_ONLY
  AOM_USAGE_GOOD_QUALITY,
  AOM_USAGE_ALL_INTRA,
#endif
};

INSTANTIATE_TEST_SUITE_P(All, EncodeAPIParameterized,
                         testing::Combine(
                             /*usage=*/testing::ValuesIn(kUsages),
                             /*speed=*/testing::Values(6, 7, 10),
                             /*aq_mode=*/testing::Values(0, 1, 2, 3)));

#if !CONFIG_REALTIME_ONLY
TEST(EncodeAPI, AllIntraMode) {
  aom_codec_iface_t *iface = aom_codec_av1_cx();
  aom_codec_ctx_t enc;
  aom_codec_enc_cfg_t cfg;
  EXPECT_EQ(AOM_CODEC_OK,
            aom_codec_enc_config_default(iface, &cfg, AOM_USAGE_ALL_INTRA));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_enc_init(&enc, iface, &cfg, 0));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_destroy(&enc));

  // Set g_lag_in_frames to a nonzero value. This should cause
  // aom_codec_enc_init() to fail.
  EXPECT_EQ(AOM_CODEC_OK,
            aom_codec_enc_config_default(iface, &cfg, AOM_USAGE_ALL_INTRA));
  cfg.g_lag_in_frames = 1;
  EXPECT_EQ(AOM_CODEC_INVALID_PARAM, aom_codec_enc_init(&enc, iface, &cfg, 0));

  // Set kf_max_dist to a nonzero value. This should cause aom_codec_enc_init()
  // to fail.
  EXPECT_EQ(AOM_CODEC_OK,
            aom_codec_enc_config_default(iface, &cfg, AOM_USAGE_ALL_INTRA));
  cfg.kf_max_dist = 1;
  EXPECT_EQ(AOM_CODEC_INVALID_PARAM, aom_codec_enc_init(&enc, iface, &cfg, 0));
}
#endif

}  // namespace
