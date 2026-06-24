/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <array>
#include <cassert>
#include <climits>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <memory>
#include <tuple>

#include "gtest/gtest.h"

#include "config/aom_config.h"

#include "aom/aomcx.h"
#include "aom/aom_encoder.h"
#include "aom/aom_image.h"
#include "aom_mem/aom_mem.h"

#include "test/codec_factory.h"
#include "test/encode_test_driver.h"
#include "test/util.h"
#include "test/video_source.h"

namespace {

#if CONFIG_REALTIME_ONLY
const unsigned int kUsage = AOM_USAGE_REALTIME;
#else
const unsigned int kUsage = AOM_USAGE_GOOD_QUALITY;
#endif

static void *Memset16(void *dest, int val, size_t length) {
  uint16_t *dest16 = (uint16_t *)dest;
  for (size_t i = 0; i < length; ++i) *dest16++ = val;
  return dest;
}

static void FillImage(aom_image_t *img, uint8_t val) {
  for (int p = 0; p < 3; ++p) {
    uint8_t *buf = img->planes[p];
    const int h = p == 0 ? (int)img->h : (int)((img->h + 1) / 2);
    const int w = p == 0 ? (int)img->w : (int)((img->w + 1) / 2);
    for (int r = 0; r < h; ++r) {
      memset(buf, val, w);
      buf += img->stride[p];
    }
  }
}

static void FillImageRandom(aom_image_t *img) {
  ::libaom_test::ACMRandom rnd;
  rnd.Reset(::libaom_test::ACMRandom::DeterministicSeed());
  for (int p = 0; p < 3; ++p) {
    uint8_t *buf = img->planes[p];
    const int h = p == 0 ? (int)img->h : (int)((img->h + 1) / 2);
    const int w = p == 0 ? (int)img->w : (int)((img->w + 1) / 2);
    for (int r = 0; r < h; ++r) {
      for (int c = 0; c < w; ++c) {
        buf[c] = rnd.Rand8();
      }
      buf += img->stride[p];
    }
  }
}

static void SetSvc(aom_codec_ctx_t *codec, int num_sl, const int *num,
                   const int *den, const int *bitrates) {
  aom_svc_params_t sp = {};
  sp.number_spatial_layers = num_sl;
  sp.number_temporal_layers = 1;
  sp.framerate_factor[0] = 1;
  for (int i = 0; i < num_sl; ++i) {
    sp.scaling_factor_num[i] = num[i];
    sp.scaling_factor_den[i] = den[i];
    sp.max_quantizers[i] = 56;
    sp.min_quantizers[i] = 2;
    sp.layer_target_bitrate[i] = bitrates[i];
  }
  ASSERT_EQ(aom_codec_control(codec, AV1E_SET_SVC_PARAMS, &sp), AOM_CODEC_OK);
}

static void EncodeOne(aom_codec_ctx_t *codec, aom_image_t *img,
                      aom_codec_pts_t pts) {
  ASSERT_EQ(aom_codec_encode(codec, img, pts, 1, 0), AOM_CODEC_OK);
  aom_codec_iter_t iter = NULL;
  while (aom_codec_get_cx_data(codec, &iter) != NULL) {
  }
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
  cfg.g_w = 1 << 16;
  cfg.g_h = (1 << 14) + 1;
  EXPECT_EQ(AOM_CODEC_INVALID_PARAM, aom_codec_enc_init(&enc, iface, &cfg, 0));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_enc_config_default(iface, &cfg, kUsage));
  cfg.g_w = (1 << 14) + 1;
  cfg.g_h = 1 << 16;
  EXPECT_EQ(AOM_CODEC_INVALID_PARAM, aom_codec_enc_init(&enc, iface, &cfg, 0));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_enc_config_default(iface, &cfg, kUsage));
  cfg.g_forced_max_frame_width = 1 << 16;
  cfg.g_forced_max_frame_height = (1 << 14) + 1;
  EXPECT_EQ(AOM_CODEC_INVALID_PARAM, aom_codec_enc_init(&enc, iface, &cfg, 0));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_enc_config_default(iface, &cfg, kUsage));
  cfg.g_forced_max_frame_width = (1 << 14) + 1;
  cfg.g_forced_max_frame_height = 1 << 16;
  EXPECT_EQ(AOM_CODEC_INVALID_PARAM, aom_codec_enc_init(&enc, iface, &cfg, 0));
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

TEST(EncodeAPI, InvalidSvcParams) {
  uint8_t buf[6] = { 0 };
  aom_image_t img;
  aom_codec_ctx_t enc;
  aom_codec_enc_cfg_t cfg;

  EXPECT_EQ(&img, aom_img_wrap(&img, AOM_IMG_FMT_I420, 1, 1, 1, buf));

  aom_codec_iface_t *iface = aom_codec_av1_cx();
  EXPECT_EQ(AOM_CODEC_OK,
            aom_codec_enc_config_default(iface, &cfg, AOM_USAGE_REALTIME));
  cfg.g_w = 1;
  cfg.g_h = 1;
  EXPECT_EQ(aom_codec_enc_init(&enc, iface, &cfg, 0), AOM_CODEC_OK);
  EXPECT_EQ(aom_codec_encode(&enc, &img, 0, 1, 0), AOM_CODEC_OK);

  EXPECT_EQ(aom_codec_control(&enc, AOME_SET_NUMBER_SPATIAL_LAYERS, -1),
            AOM_CODEC_INVALID_PARAM);
  EXPECT_EQ(aom_codec_control(&enc, AOME_SET_NUMBER_SPATIAL_LAYERS,
                              AOM_MAX_SS_LAYERS + 1),
            AOM_CODEC_INVALID_PARAM);

  aom_svc_params_t svc_params = {};
  svc_params.framerate_factor[0] = 2;
  svc_params.framerate_factor[1] = 1;
  svc_params.layer_target_bitrate[0] = 60 * cfg.rc_target_bitrate / 100;
  svc_params.layer_target_bitrate[1] = cfg.rc_target_bitrate;
  // Check scale factors.
  for (int i = 0; i < AOM_MAX_SS_LAYERS; i++) {
    svc_params.scaling_factor_num[i] = 1;
    svc_params.scaling_factor_den[i] = 1;
  }
  svc_params.number_spatial_layers = 2;
  svc_params.number_temporal_layers = 1;
  // Set invalid factors.
  svc_params.scaling_factor_num[0] = 2;
  svc_params.scaling_factor_den[0] = 1;
  EXPECT_EQ(aom_codec_control(&enc, AV1E_SET_SVC_PARAMS, &svc_params),
            AOM_CODEC_INVALID_PARAM);
  // Set valid factors.
  svc_params.scaling_factor_num[0] = 1;
  svc_params.scaling_factor_den[0] = 2;
  EXPECT_EQ(aom_codec_control(&enc, AV1E_SET_SVC_PARAMS, &svc_params),
            AOM_CODEC_OK);
  svc_params.scaling_factor_num[0] = 1;
  svc_params.scaling_factor_den[0] = 1;
  for (const bool use_flexible_mode : { false, true }) {
    if (use_flexible_mode) {
      aom_svc_ref_frame_config_t ref_frame_config = {};
      ref_frame_config.refresh[0] = 1;
      ASSERT_EQ(aom_codec_control(&enc, AV1E_SET_SVC_REF_FRAME_CONFIG,
                                  &ref_frame_config),
                AOM_CODEC_OK);
    }

    const int max_valid_num_spatial = use_flexible_mode ? AOM_MAX_SS_LAYERS : 3;
    const int max_valid_num_temporal =
        use_flexible_mode ? AOM_MAX_TS_LAYERS : 3;
    for (int num_spatial = AOM_MAX_SS_LAYERS;
         num_spatial <= AOM_MAX_SS_LAYERS + 1; ++num_spatial) {
      for (int num_temporal = AOM_MAX_TS_LAYERS;
           num_temporal <= AOM_MAX_TS_LAYERS + 1; ++num_temporal) {
        svc_params.number_spatial_layers = num_spatial;
        svc_params.number_temporal_layers = num_temporal;
        if (num_spatial > 0 && num_spatial <= AOM_MAX_SS_LAYERS &&
            num_temporal > 0 && num_temporal <= AOM_MAX_TS_LAYERS) {
          EXPECT_EQ(aom_codec_control(&enc, AV1E_SET_SVC_PARAMS, &svc_params),
                    AOM_CODEC_OK)
              << "num_spatial: " << num_spatial
              << " num_temporal: " << num_temporal
              << " use_flexible_mode: " << use_flexible_mode;
        } else {
          EXPECT_EQ(aom_codec_control(&enc, AV1E_SET_SVC_PARAMS, &svc_params),
                    AOM_CODEC_INVALID_PARAM)
              << "num_spatial: " << num_spatial
              << " num_temporal: " << num_temporal
              << " use_flexible_mode: " << use_flexible_mode;
        }
        if (use_flexible_mode || (num_spatial <= max_valid_num_spatial &&
                                  num_temporal <= max_valid_num_temporal)) {
          EXPECT_EQ(aom_codec_encode(&enc, &img, 0, 1, 0), AOM_CODEC_OK);
        } else {
          EXPECT_EQ(aom_codec_encode(&enc, &img, 0, 1, 0),
                    AOM_CODEC_INVALID_PARAM);
        }
      }
    }
  }

  EXPECT_EQ(aom_codec_encode(&enc, &img, 0, 1, 0), AOM_CODEC_OK);
  EXPECT_EQ(aom_codec_encode(&enc, nullptr, 0, 0, 0), AOM_CODEC_OK);
  EXPECT_EQ(aom_codec_destroy(&enc), AOM_CODEC_OK);
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

TEST(EncodeAPI, InvalidUVStrides) {
  static constexpr std::array<aom_img_fmt_t, 9> kAv1ImageFormats = {
    AOM_IMG_FMT_YV12,   AOM_IMG_FMT_I420,   AOM_IMG_FMT_I422,
    AOM_IMG_FMT_I444,   AOM_IMG_FMT_NV12,   AOM_IMG_FMT_I42016,
    AOM_IMG_FMT_YV1216, AOM_IMG_FMT_I42216, AOM_IMG_FMT_I44416
  };
  struct UVStride {
    int u_stride;
    int v_stride;
  };
  constexpr int kWidth = 64;
  constexpr int kHeight = 64;
  static constexpr std::array<UVStride, 4> kUVStrides = {
    UVStride{ kWidth, kWidth - 1 }, UVStride{ kWidth, kWidth + 1 },
    UVStride{ kWidth - 1, kWidth }, UVStride{ kWidth + 1, kWidth }
  };
  aom_image_t img;
  aom_codec_ctx_t enc;
  aom_codec_enc_cfg_t cfg;
  // Allocate a buffer large enough for a non-subsampled, high bitdepth image.
  auto buf = std::make_unique<uint8_t[]>(kWidth * kHeight * 3 * 2);

  ASSERT_EQ(aom_codec_enc_config_default(aom_codec_av1_cx(), &cfg,
                                         /*usage=*/AOM_USAGE_REALTIME),
            AOM_CODEC_OK);

  for (const auto img_fmt : kAv1ImageFormats) {
    const bool high_bit_depth =
        (img_fmt & AOM_IMG_FMT_HIGHBITDEPTH) == AOM_IMG_FMT_HIGHBITDEPTH;
    if (high_bit_depth && !(aom_codec_get_caps(aom_codec_av1_cx()) &
                            AOM_CODEC_CAP_HIGHBITDEPTH)) {
      break;
    }
    ASSERT_EQ(aom_img_wrap(&img, img_fmt, kWidth, kHeight, /*stride_align=*/1,
                           buf.get()),
              &img)
        << "Unable to wrap aom_image for format: " << img_fmt;
    const bool is_444 = (img.x_chroma_shift == 0 && img.y_chroma_shift == 0);
    const bool is_422 = (img.x_chroma_shift == 1 && img.y_chroma_shift == 0);
    // 4:4:4 is allowed in profile 1, 4:2:2 in profile 2.
    cfg.g_profile = is_444 + 2 * is_422;
    cfg.g_w = kWidth;
    cfg.g_h = kHeight;
    cfg.g_bit_depth = high_bit_depth ? AOM_BITS_10 : AOM_BITS_8;
    // Monochrome is not allowed in profile 1.
    const unsigned int max_monochrome = (cfg.g_profile == 1) ? 0u : 1u;
    for (cfg.monochrome = 0; cfg.monochrome <= max_monochrome;
         ++cfg.monochrome) {
      ASSERT_EQ(
          aom_codec_enc_init(&enc, aom_codec_av1_cx(), &cfg,
                             high_bit_depth ? AOM_CODEC_USE_HIGHBITDEPTH : 0),
          AOM_CODEC_OK)
          << " high_bit_depth: " << high_bit_depth;

      for (const auto uv_stride : kUVStrides) {
        const UVStride orig = { img.stride[AOM_PLANE_U],
                                img.stride[AOM_PLANE_V] };
        img.stride[AOM_PLANE_U] = uv_stride.u_stride;
        img.stride[AOM_PLANE_V] =
            (img_fmt == AOM_IMG_FMT_NV12) ? 0 : uv_stride.v_stride;
        img.monochrome = cfg.monochrome;
        // Monochrome should ignore the U and V planes and NV12 only sets one
        // stride value, they should always succeed.
        const aom_codec_err_t expected_err =
            (cfg.monochrome || img_fmt == AOM_IMG_FMT_NV12)
                ? AOM_CODEC_OK
                : AOM_CODEC_INVALID_PARAM;
        EXPECT_EQ(aom_codec_encode(&enc, &img, /*pts=*/0, /*duration=*/1,
                                   /*flags=*/0),
                  expected_err)
            << "Error: " << aom_codec_error_detail(&enc)
            << ", format: " << img_fmt << ", U stride: " << uv_stride.u_stride
            << ", V stride: " << uv_stride.v_stride;

        // Ensure the encoder can recover when given valid strides.
        img.stride[AOM_PLANE_U] = orig.u_stride;
        img.stride[AOM_PLANE_V] = orig.v_stride;
        EXPECT_EQ(aom_codec_encode(&enc, &img, /*pts=*/0, /*duration=*/1,
                                   /*flags=*/0),
                  AOM_CODEC_OK)
            << "Error: " << aom_codec_error_detail(&enc)
            << ", format: " << img_fmt << ", U stride: " << orig.u_stride
            << ", V stride: " << orig.v_stride;
      }

      EXPECT_EQ(
          aom_codec_encode(&enc, /*img=*/nullptr, /*pts=*/0, /*duration=*/0,
                           /*flags=*/0),
          AOM_CODEC_OK);
      EXPECT_EQ(aom_codec_destroy(&enc), AOM_CODEC_OK);
    }
  }
}

#if CONFIG_AV1_HIGHBITDEPTH
TEST(EncodeAPI, InvalidInputRange) {
  constexpr int kWidth = 64;
  constexpr int kHeight = 64;
  aom_codec_enc_cfg_t cfg;

  ASSERT_EQ(aom_codec_enc_config_default(aom_codec_av1_cx(), &cfg,
                                         /*usage=*/AOM_USAGE_REALTIME),
            AOM_CODEC_OK);
  cfg.g_w = kWidth;
  cfg.g_h = kHeight;

  std::unique_ptr<aom_image_t, decltype(&aom_img_free)> image(
      aom_img_alloc(nullptr, AOM_IMG_FMT_I42016, cfg.g_w, cfg.g_h, 0),
      &aom_img_free);
  aom_image_t *img = image.get();
  ASSERT_NE(img, nullptr);

  for (const auto bitdepth : { AOM_BITS_8, AOM_BITS_10, AOM_BITS_12 }) {
    cfg.g_profile = (bitdepth == AOM_BITS_12) ? 2 : 0;
    cfg.g_bit_depth = bitdepth;

    // A value that's slightly over the max is used to ensure when the range is
    // not checked an encode won't produce any integer overflows. Values
    // outside of the valid range are not supported, so overflows will be
    // ignored in that case.
    const int val = 1 << bitdepth;
    for (const int plane : { AOM_PLANE_Y, AOM_PLANE_U, AOM_PLANE_V }) {
      const int width = aom_img_plane_width(img, plane);
      const int height = aom_img_plane_height(img, plane);
      uint8_t *p = img->planes[plane];
      for (int y = 0; y < height; ++y) {
        aom_memset16(p, val, width);
        p += img->stride[plane];
      }
    }

    for (cfg.monochrome = 0; cfg.monochrome <= 1; ++cfg.monochrome) {
      aom_codec_ctx_t enc;
      ASSERT_EQ(aom_codec_enc_init(&enc, aom_codec_av1_cx(), &cfg,
                                   AOM_CODEC_USE_HIGHBITDEPTH),
                AOM_CODEC_OK);

      for (int check_input_range = 0; check_input_range <= 1;
           ++check_input_range) {
        uint8_t *u = img->planes[AOM_PLANE_U];
        uint8_t *v = img->planes[AOM_PLANE_V];
        // Applications may not set img->monochrome. Leave it set to 0, but
        // clear the planes to ensure the encoder properly ignores their
        // values.
        if (cfg.monochrome) {
          img->planes[AOM_PLANE_U] = img->planes[AOM_PLANE_V] = nullptr;
        }

        EXPECT_EQ(aom_codec_control(&enc, AOME_SET_VALIDATE_HBD_INPUT,
                                    check_input_range),
                  AOM_CODEC_OK);

        EXPECT_EQ(aom_codec_encode(&enc, img, /*pts=*/0, /*duration=*/1,
                                   /*flags=*/0),
                  check_input_range ? AOM_CODEC_INVALID_PARAM : AOM_CODEC_OK)
            << "Error: " << aom_codec_error_detail(&enc)
            << ", bitdepth: " << bitdepth << ", monochrome: " << img->monochrome
            << ", check_input_range: " << check_input_range;

        img->planes[AOM_PLANE_U] = u;
        img->planes[AOM_PLANE_V] = v;
      }

      EXPECT_EQ(
          aom_codec_encode(&enc, /*img=*/nullptr, /*pts=*/0, /*duration=*/0,
                           /*flags=*/0),
          AOM_CODEC_OK);
      EXPECT_EQ(aom_codec_destroy(&enc), AOM_CODEC_OK);
    }
  }
}
#endif  // CONFIG_AV1_HIGHBITDEPTH

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
      aom_img_alloc(nullptr, AOM_IMG_FMT_I420, cfg.g_w, cfg.g_h, 0);
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
      aom_img_alloc(nullptr, AOM_IMG_FMT_I42016, cfg.g_w, cfg.g_h, 0);
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
      aom_img_alloc(nullptr, AOM_IMG_FMT_I420, cfg.g_w, cfg.g_h, 0);
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
      aom_img_alloc(nullptr, AOM_IMG_FMT_I42016, cfg.g_w, cfg.g_h, 0);
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

aom_image_t *CreateGrayImage(aom_img_fmt_t fmt, unsigned int w,
                             unsigned int h) {
  aom_image_t *const image = aom_img_alloc(nullptr, fmt, w, h, 1);
  if (!image) return image;

  for (unsigned int i = 0; i < image->d_h; ++i) {
    memset(image->planes[0] + i * image->stride[0], 128, image->d_w);
  }
  const unsigned int uv_h = (image->d_h + 1) / 2;
  const unsigned int uv_w = (image->d_w + 1) / 2;
  for (unsigned int i = 0; i < uv_h; ++i) {
    memset(image->planes[1] + i * image->stride[1], 128, uv_w);
    memset(image->planes[2] + i * image->stride[2], 128, uv_w);
  }
  return image;
}

TEST(EncodeAPI, Buganizer310548198) {
  aom_codec_iface_t *const iface = aom_codec_av1_cx();
  aom_codec_enc_cfg_t cfg;
  const unsigned int usage = AOM_USAGE_REALTIME;
  ASSERT_EQ(aom_codec_enc_config_default(iface, &cfg, usage), AOM_CODEC_OK);
  cfg.g_w = 1;
  cfg.g_h = 444;
  cfg.g_pass = AOM_RC_ONE_PASS;
  cfg.g_lag_in_frames = 0;

  aom_codec_ctx_t enc;
  ASSERT_EQ(aom_codec_enc_init(&enc, iface, &cfg, 0), AOM_CODEC_OK);

  const int speed = 6;
  ASSERT_EQ(aom_codec_control(&enc, AOME_SET_CPUUSED, speed), AOM_CODEC_OK);

  const aom_enc_frame_flags_t flags = 0;
  int frame_index = 0;

  // Encode a frame.
  aom_image_t *image = CreateGrayImage(AOM_IMG_FMT_I420, cfg.g_w, cfg.g_h);
  ASSERT_NE(image, nullptr);
  ASSERT_EQ(aom_codec_encode(&enc, image, frame_index, 1, flags), AOM_CODEC_OK);
  frame_index++;
  const aom_codec_cx_pkt_t *pkt;
  aom_codec_iter_t iter = nullptr;
  while ((pkt = aom_codec_get_cx_data(&enc, &iter)) != nullptr) {
    ASSERT_EQ(pkt->kind, AOM_CODEC_CX_FRAME_PKT);
  }
  aom_img_free(image);

  cfg.g_w = 1;
  cfg.g_h = 254;
  ASSERT_EQ(aom_codec_enc_config_set(&enc, &cfg), AOM_CODEC_OK)
      << aom_codec_error_detail(&enc);

  cfg.g_w = 1;
  cfg.g_h = 154;
  ASSERT_EQ(aom_codec_enc_config_set(&enc, &cfg), AOM_CODEC_OK)
      << aom_codec_error_detail(&enc);

  // Encode a frame.
  image = CreateGrayImage(AOM_IMG_FMT_I420, cfg.g_w, cfg.g_h);
  ASSERT_EQ(aom_codec_encode(&enc, image, frame_index, 1, flags), AOM_CODEC_OK);
  frame_index++;
  iter = nullptr;
  while ((pkt = aom_codec_get_cx_data(&enc, &iter)) != nullptr) {
    ASSERT_EQ(pkt->kind, AOM_CODEC_CX_FRAME_PKT);
  }
  aom_img_free(image);

  // Flush the encoder.
  bool got_data;
  do {
    ASSERT_EQ(aom_codec_encode(&enc, nullptr, 0, 0, 0), AOM_CODEC_OK);
    got_data = false;
    iter = nullptr;
    while ((pkt = aom_codec_get_cx_data(&enc, &iter)) != nullptr) {
      ASSERT_EQ(pkt->kind, AOM_CODEC_CX_FRAME_PKT);
      got_data = true;
    }
  } while (got_data);

  ASSERT_EQ(aom_codec_destroy(&enc), AOM_CODEC_OK);
}

// Emulates the WebCodecs VideoEncoder interface.
class AV1Encoder {
 public:
  explicit AV1Encoder(int speed) : speed_(speed) {}
  ~AV1Encoder();

  void Configure(unsigned int threads, unsigned int width, unsigned int height,
                 aom_rc_mode end_usage, unsigned int usage);
  void Encode(bool key_frame);

  aom_codec_ctx_t *GetCodecCtx() { return &enc_; }

 private:
  // Flushes the encoder. Should be called after all the Encode() calls.
  void Flush();

  const int speed_;
  bool initialized_ = false;
  aom_codec_enc_cfg_t cfg_;
  aom_codec_ctx_t enc_;
  int frame_index_ = 0;
};

AV1Encoder::~AV1Encoder() {
  if (initialized_) {
    Flush();
    EXPECT_EQ(aom_codec_destroy(&enc_), AOM_CODEC_OK);
  }
}

void AV1Encoder::Configure(unsigned int threads, unsigned int width,
                           unsigned int height, aom_rc_mode end_usage,
                           unsigned int usage) {
  if (!initialized_) {
    aom_codec_iface_t *const iface = aom_codec_av1_cx();
    ASSERT_EQ(aom_codec_enc_config_default(iface, &cfg_, usage), AOM_CODEC_OK);
    cfg_.g_threads = threads;
    cfg_.g_w = width;
    cfg_.g_h = height;
    cfg_.g_forced_max_frame_width = cfg_.g_w;
    cfg_.g_forced_max_frame_height = cfg_.g_h;
    cfg_.g_timebase.num = 1;
    cfg_.g_timebase.den = 1000 * 1000;  // microseconds
    cfg_.g_pass = AOM_RC_ONE_PASS;
    cfg_.g_lag_in_frames = 0;
    cfg_.rc_end_usage = end_usage;
    cfg_.rc_min_quantizer = 2;
    cfg_.rc_max_quantizer = 58;
    ASSERT_EQ(aom_codec_enc_init(&enc_, iface, &cfg_, 0), AOM_CODEC_OK);
    ASSERT_EQ(aom_codec_control(&enc_, AOME_SET_CPUUSED, speed_), AOM_CODEC_OK);

    const int log2_threads =
        (cfg_.g_threads == 0) ? 0 : static_cast<int>(std::log2(cfg_.g_threads));
    int tile_columns_log2 = 0;
    int tile_rows_log2 = 0;
    switch (log2_threads) {
      case 4:
        tile_columns_log2 = 2;
        tile_rows_log2 = 2;
        break;
      case 3:
        tile_columns_log2 = 2;
        tile_rows_log2 = 1;
        break;
      case 2:
        tile_columns_log2 = 1;
        tile_rows_log2 = 1;
        break;
      default: tile_columns_log2 = log2_threads;
    }
    ASSERT_EQ(
        aom_codec_control(&enc_, AV1E_SET_TILE_COLUMNS, tile_columns_log2),
        AOM_CODEC_OK);
    ASSERT_EQ(aom_codec_control(&enc_, AV1E_SET_TILE_ROWS, tile_rows_log2),
              AOM_CODEC_OK);

    initialized_ = true;
    return;
  }

  ASSERT_EQ(usage, cfg_.g_usage);
  cfg_.g_threads = threads;
  cfg_.g_w = width;
  cfg_.g_h = height;
  cfg_.rc_end_usage = end_usage;
  ASSERT_EQ(aom_codec_enc_config_set(&enc_, &cfg_), AOM_CODEC_OK)
      << aom_codec_error_detail(&enc_);
}

void AV1Encoder::Encode(bool key_frame) {
  assert(initialized_);
  // TODO(wtc): Support high bit depths and other YUV formats.
  aom_image_t *const image =
      CreateGrayImage(AOM_IMG_FMT_I420, cfg_.g_w, cfg_.g_h);
  ASSERT_NE(image, nullptr);
  const aom_enc_frame_flags_t flags = key_frame ? AOM_EFLAG_FORCE_KF : 0;
  ASSERT_EQ(aom_codec_encode(&enc_, image, frame_index_, 1, flags),
            AOM_CODEC_OK);
  frame_index_++;
  const aom_codec_cx_pkt_t *pkt;
  aom_codec_iter_t iter = nullptr;
  while ((pkt = aom_codec_get_cx_data(&enc_, &iter)) != nullptr) {
    ASSERT_EQ(pkt->kind, AOM_CODEC_CX_FRAME_PKT);
    if (key_frame) {
      ASSERT_EQ(pkt->data.frame.flags & AOM_FRAME_IS_KEY, AOM_FRAME_IS_KEY);
    }
  }
  aom_img_free(image);
}

void AV1Encoder::Flush() {
  bool got_data;
  do {
    ASSERT_EQ(aom_codec_encode(&enc_, nullptr, 0, 0, 0), AOM_CODEC_OK);
    got_data = false;
    const aom_codec_cx_pkt_t *pkt;
    aom_codec_iter_t iter = nullptr;
    while ((pkt = aom_codec_get_cx_data(&enc_, &iter)) != nullptr) {
      ASSERT_EQ(pkt->kind, AOM_CODEC_CX_FRAME_PKT);
      got_data = true;
    }
  } while (got_data);
}

TEST(EncodeAPI, Buganizer314858909) {
  AV1Encoder encoder(7);

  encoder.Configure(6, 1582, 750, AOM_CBR, AOM_USAGE_REALTIME);

  // Encode a frame.
  encoder.Encode(false);

  encoder.Configure(0, 1582, 23, AOM_CBR, AOM_USAGE_REALTIME);

  // Encode a frame..
  encoder.Encode(false);

  encoder.Configure(16, 1542, 363, AOM_CBR, AOM_USAGE_REALTIME);

  // Encode a frame..
  encoder.Encode(false);
}

// Run this test to reproduce the bug in fuzz test: ASSERT: cpi->rec_sse !=
// UINT64_MAX in av1_rc_bits_per_mb.
TEST(EncodeAPI, Buganizer310766628) {
  AV1Encoder encoder(7);

  encoder.Configure(16, 759, 383, AOM_CBR, AOM_USAGE_REALTIME);

  // Encode a frame.
  encoder.Encode(false);

  encoder.Configure(2, 759, 383, AOM_VBR, AOM_USAGE_REALTIME);

  // Encode a frame. This will trigger the assertion failure.
  encoder.Encode(false);
}

// This test covers a possible use case where the change of frame sizes and
// thread numbers happens before and after the first frame coding.
TEST(EncodeAPI, Buganizer310455204) {
  AV1Encoder encoder(7);

  encoder.Configure(0, 1915, 503, AOM_VBR, AOM_USAGE_REALTIME);

  encoder.Configure(4, 1, 1, AOM_VBR, AOM_USAGE_REALTIME);

  encoder.Configure(6, 559, 503, AOM_CBR, AOM_USAGE_REALTIME);

  // Encode a frame.
  encoder.Encode(false);

  // Increase the number of threads.
  encoder.Configure(16, 1915, 503, AOM_CBR, AOM_USAGE_REALTIME);

  // Encode a frame.
  encoder.Encode(false);
}

// Run this test to reproduce the bug in fuzz test: Float-cast-overflow in
// av1_rc_bits_per_mb.
TEST(EncodeAPI, Buganizer310457427) {
  AV1Encoder encoder(7);

  encoder.Configure(12, 896, 1076, AOM_CBR, AOM_USAGE_REALTIME);

  encoder.Configure(6, 609, 1076, AOM_VBR, AOM_USAGE_REALTIME);

  // Encode a frame.
  encoder.Encode(false);

  // Encode a frame. This will trigger the float-cast-overflow bug which was
  // caused by division by zero.
  encoder.Encode(false);
}

TEST(EncodeAPI, PtsSmallerThanInitialPts) {
  // Initialize libaom encoder.
  aom_codec_iface_t *const iface = aom_codec_av1_cx();
  aom_codec_ctx_t enc;
  aom_codec_enc_cfg_t cfg;

  ASSERT_EQ(aom_codec_enc_config_default(iface, &cfg, AOM_USAGE_REALTIME),
            AOM_CODEC_OK);

  cfg.g_w = 1280;
  cfg.g_h = 720;
  cfg.rc_target_bitrate = 1000;

  ASSERT_EQ(aom_codec_enc_init(&enc, iface, &cfg, 0), AOM_CODEC_OK);

  // Create input image.
  aom_image_t *const image =
      CreateGrayImage(AOM_IMG_FMT_I420, cfg.g_w, cfg.g_h);
  ASSERT_NE(image, nullptr);

  // Encode frame.
  ASSERT_EQ(aom_codec_encode(&enc, image, 12, 1, 0), AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_encode(&enc, image, 13, 1, 0), AOM_CODEC_OK);
  // pts (10) is smaller than the initial pts (12).
  ASSERT_EQ(aom_codec_encode(&enc, image, 10, 1, 0), AOM_CODEC_INVALID_PARAM);

  // Free resources.
  aom_img_free(image);
  aom_codec_destroy(&enc);
}

TEST(EncodeAPI, PtsOrDurationTooBig) {
  // Initialize libaom encoder.
  aom_codec_iface_t *const iface = aom_codec_av1_cx();
  aom_codec_ctx_t enc;
  aom_codec_enc_cfg_t cfg;

  ASSERT_EQ(aom_codec_enc_config_default(iface, &cfg, AOM_USAGE_REALTIME),
            AOM_CODEC_OK);

  cfg.g_w = 1280;
  cfg.g_h = 720;
  cfg.rc_target_bitrate = 1000;

  ASSERT_EQ(aom_codec_enc_init(&enc, iface, &cfg, 0), AOM_CODEC_OK);

  // Create input image.
  aom_image_t *const image =
      CreateGrayImage(AOM_IMG_FMT_I420, cfg.g_w, cfg.g_h);
  ASSERT_NE(image, nullptr);

  // Encode frame.
  ASSERT_EQ(aom_codec_encode(&enc, image, 0, 1, 0), AOM_CODEC_OK);
  // pts, when converted to ticks, is too big.
  ASSERT_EQ(aom_codec_encode(&enc, image, INT64_MAX / 1000000 + 1, 1, 0),
            AOM_CODEC_INVALID_PARAM);
#if ULONG_MAX > INT64_MAX
  // duration is too big.
  ASSERT_EQ(aom_codec_encode(&enc, image, 0, (1ul << 63), 0),
            AOM_CODEC_INVALID_PARAM);
  // pts + duration is too big.
  ASSERT_EQ(aom_codec_encode(&enc, image, 1, INT64_MAX, 0),
            AOM_CODEC_INVALID_PARAM);
#endif
  // pts + duration, when converted to ticks, is too big.
#if ULONG_MAX > INT64_MAX
  ASSERT_EQ(aom_codec_encode(&enc, image, 0, 0x1c0a0a1a3232, 0),
            AOM_CODEC_INVALID_PARAM);
#endif
  ASSERT_EQ(aom_codec_encode(&enc, image, INT64_MAX / 1000000, 1, 0),
            AOM_CODEC_INVALID_PARAM);

  // Free resources.
  aom_img_free(image);
  aom_codec_destroy(&enc);
}

// Reproduces https://crbug.com/339877165.
TEST(EncodeAPI, Buganizer339877165) {
  // Initialize libaom encoder.
  aom_codec_iface_t *const iface = aom_codec_av1_cx();
  aom_codec_ctx_t enc;
  aom_codec_enc_cfg_t cfg;

  ASSERT_EQ(aom_codec_enc_config_default(iface, &cfg, AOM_USAGE_REALTIME),
            AOM_CODEC_OK);

  cfg.g_w = 2560;
  cfg.g_h = 1600;
  cfg.rc_target_bitrate = 231;
  cfg.rc_end_usage = AOM_CBR;
  cfg.g_threads = 8;

  ASSERT_EQ(aom_codec_enc_init(&enc, iface, &cfg, 0), AOM_CODEC_OK);

  // From libaom_av1_encoder.cc in WebRTC.
  ASSERT_EQ(aom_codec_control(&enc, AOME_SET_CPUUSED, 11), AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_control(&enc, AV1E_SET_ENABLE_CDEF, 1), AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_control(&enc, AV1E_SET_ENABLE_TPL_MODEL, 0),
            AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_control(&enc, AV1E_SET_DELTAQ_MODE, 0), AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_control(&enc, AV1E_SET_ENABLE_ORDER_HINT, 0),
            AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_control(&enc, AV1E_SET_AQ_MODE, 3), AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_control(&enc, AOME_SET_MAX_INTRA_BITRATE_PCT, 300),
            AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_control(&enc, AV1E_SET_COEFF_COST_UPD_FREQ, 3),
            AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_control(&enc, AV1E_SET_MODE_COST_UPD_FREQ, 3),
            AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_control(&enc, AV1E_SET_MV_COST_UPD_FREQ, 3),
            AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_control(&enc, AV1E_SET_TUNE_CONTENT, AOM_CONTENT_SCREEN),
            AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_control(&enc, AV1E_SET_ENABLE_PALETTE, 1), AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_control(&enc, AV1E_SET_TILE_ROWS, 1), AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_control(&enc, AV1E_SET_TILE_COLUMNS, 2), AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_control(&enc, AV1E_SET_ENABLE_OBMC, 0), AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_control(&enc, AV1E_SET_NOISE_SENSITIVITY, 0),
            AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_control(&enc, AV1E_SET_ENABLE_WARPED_MOTION, 0),
            AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_control(&enc, AV1E_SET_ENABLE_GLOBAL_MOTION, 0),
            AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_control(&enc, AV1E_SET_ENABLE_REF_FRAME_MVS, 0),
            AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_control(&enc, AV1E_SET_SUPERBLOCK_SIZE,
                              AOM_SUPERBLOCK_SIZE_DYNAMIC),
            AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_control(&enc, AV1E_SET_ENABLE_CFL_INTRA, 0),
            AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_control(&enc, AV1E_SET_ENABLE_SMOOTH_INTRA, 0),
            AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_control(&enc, AV1E_SET_ENABLE_ANGLE_DELTA, 0),
            AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_control(&enc, AV1E_SET_ENABLE_FILTER_INTRA, 0),
            AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_control(&enc, AV1E_SET_INTRA_DEFAULT_TX_ONLY, 1),
            AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_control(&enc, AV1E_SET_DISABLE_TRELLIS_QUANT, 1),
            AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_control(&enc, AV1E_SET_ENABLE_DIST_WTD_COMP, 0),
            AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_control(&enc, AV1E_SET_ENABLE_DIFF_WTD_COMP, 0),
            AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_control(&enc, AV1E_SET_ENABLE_DUAL_FILTER, 0),
            AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_control(&enc, AV1E_SET_ENABLE_INTERINTRA_COMP, 0),
            AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_control(&enc, AV1E_SET_ENABLE_INTERINTRA_WEDGE, 0),
            AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_control(&enc, AV1E_SET_ENABLE_INTRA_EDGE_FILTER, 0),
            AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_control(&enc, AV1E_SET_ENABLE_INTRABC, 0), AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_control(&enc, AV1E_SET_ENABLE_MASKED_COMP, 0),
            AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_control(&enc, AV1E_SET_ENABLE_PAETH_INTRA, 0),
            AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_control(&enc, AV1E_SET_ENABLE_QM, 0), AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_control(&enc, AV1E_SET_ENABLE_RECT_PARTITIONS, 0),
            AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_control(&enc, AV1E_SET_ENABLE_RESTORATION, 0),
            AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_control(&enc, AV1E_SET_ENABLE_SMOOTH_INTERINTRA, 0),
            AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_control(&enc, AV1E_SET_ENABLE_TX64, 0), AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_control(&enc, AV1E_SET_MAX_REFERENCE_FRAMES, 3),
            AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_enc_config_set(&enc, &cfg), AOM_CODEC_OK);

  aom_svc_params_t svc_params = {};
  svc_params.number_spatial_layers = 2;
  svc_params.number_temporal_layers = 1;
  svc_params.max_quantizers[0] = svc_params.max_quantizers[1] = 56;
  svc_params.min_quantizers[0] = svc_params.min_quantizers[1] = 10;
  svc_params.scaling_factor_num[0] = svc_params.scaling_factor_num[1] = 1;
  svc_params.scaling_factor_den[0] = 2;
  svc_params.scaling_factor_den[1] = 1;
  svc_params.layer_target_bitrate[0] = cfg.rc_target_bitrate;
  svc_params.framerate_factor[0] = 1;
  ASSERT_EQ(aom_codec_control(&enc, AV1E_SET_SVC_PARAMS, &svc_params),
            AOM_CODEC_OK);

  aom_svc_layer_id_t layer_id = {};
  ASSERT_EQ(aom_codec_control(&enc, AV1E_SET_SVC_LAYER_ID, &layer_id),
            AOM_CODEC_OK);

  aom_svc_ref_frame_config_t ref_frame_config = {};
  ref_frame_config.refresh[0] = 1;
  ASSERT_EQ(
      aom_codec_control(&enc, AV1E_SET_SVC_REF_FRAME_CONFIG, &ref_frame_config),
      AOM_CODEC_OK);

  // Create input image.
  aom_image_t *const image =
      CreateGrayImage(AOM_IMG_FMT_I420, cfg.g_w, cfg.g_h);
  ASSERT_NE(image, nullptr);

  // Encode layer 0.
  ASSERT_EQ(aom_codec_encode(&enc, image, 0, 1, 0), AOM_CODEC_OK);

  layer_id.spatial_layer_id = 1;
  ASSERT_EQ(aom_codec_control(&enc, AV1E_SET_SVC_LAYER_ID, &layer_id),
            AOM_CODEC_OK);

  ref_frame_config.refresh[0] = 0;
  ASSERT_EQ(
      aom_codec_control(&enc, AV1E_SET_SVC_REF_FRAME_CONFIG, &ref_frame_config),
      AOM_CODEC_OK);

  // Encode layer 1.
  ASSERT_EQ(aom_codec_encode(&enc, image, 0, 1, 0), AOM_CODEC_OK);

  // Free resources.
  aom_img_free(image);
  aom_codec_destroy(&enc);
}

TEST(EncodeAPI, AomediaIssue3509VbrMinSection2Percent) {
  // Initialize libaom encoder.
  aom_codec_iface_t *const iface = aom_codec_av1_cx();
  aom_codec_ctx_t enc;
  aom_codec_enc_cfg_t cfg;

  ASSERT_EQ(aom_codec_enc_config_default(iface, &cfg, AOM_USAGE_REALTIME),
            AOM_CODEC_OK);

  cfg.g_w = 1920;
  cfg.g_h = 1080;
  cfg.rc_target_bitrate = 1000000;
  // Set this to more than 1 percent to cause a signed integer overflow in the
  // multiplication rc->avg_frame_bandwidth * oxcf->rc_cfg.vbrmin_section in
  // av1_rc_update_framerate() if the multiplication is done in the `int` type.
  cfg.rc_2pass_vbr_minsection_pct = 2;

  ASSERT_EQ(aom_codec_enc_init(&enc, iface, &cfg, 0), AOM_CODEC_OK);

  // Create input image.
  aom_image_t *const image =
      CreateGrayImage(AOM_IMG_FMT_I420, cfg.g_w, cfg.g_h);
  ASSERT_NE(image, nullptr);

  // Encode frame.
  // `duration` can go as high as 300, but the UBSan error is gone if
  // `duration` is 301 or higher.
  ASSERT_EQ(aom_codec_encode(&enc, image, 0, /*duration=*/300, 0),
            AOM_CODEC_OK);

  // Free resources.
  aom_img_free(image);
  ASSERT_EQ(aom_codec_destroy(&enc), AOM_CODEC_OK);
}

TEST(EncodeAPI, AomediaIssue3509VbrMinSection101Percent) {
  // Initialize libaom encoder.
  aom_codec_iface_t *const iface = aom_codec_av1_cx();
  aom_codec_ctx_t enc;
  aom_codec_enc_cfg_t cfg;

  ASSERT_EQ(aom_codec_enc_config_default(iface, &cfg, AOM_USAGE_REALTIME),
            AOM_CODEC_OK);

  cfg.g_w = 1920;
  cfg.g_h = 1080;
  cfg.rc_target_bitrate = 1000000;
  // Set this to more than 100 percent to cause an error when vbr_min_bits is
  // cast to `int` in av1_rc_update_framerate() if vbr_min_bits is not clamped
  // to INT_MAX.
  cfg.rc_2pass_vbr_minsection_pct = 101;

  ASSERT_EQ(aom_codec_enc_init(&enc, iface, &cfg, 0), AOM_CODEC_OK);

  // Create input image.
  aom_image_t *const image =
      CreateGrayImage(AOM_IMG_FMT_I420, cfg.g_w, cfg.g_h);
  ASSERT_NE(image, nullptr);

  // Encode frame.
  // `duration` can go as high as 300, but the UBSan error is gone if
  // `duration` is 301 or higher.
  ASSERT_EQ(aom_codec_encode(&enc, image, 0, /*duration=*/300, 0),
            AOM_CODEC_OK);

  // Free resources.
  aom_img_free(image);
  ASSERT_EQ(aom_codec_destroy(&enc), AOM_CODEC_OK);
}

TEST(EncodeAPI, Buganizer392929025) {
  // Initialize libaom encoder.
  aom_codec_iface_t *const iface = aom_codec_av1_cx();
  aom_codec_ctx_t enc;
  aom_codec_enc_cfg_t cfg;

  ASSERT_EQ(aom_codec_enc_config_default(iface, &cfg, AOM_USAGE_REALTIME),
            AOM_CODEC_OK);

  cfg.g_w = 16;
  cfg.g_h = 16;

  ASSERT_EQ(aom_codec_enc_init(&enc, iface, &cfg, 0), AOM_CODEC_OK);

  ASSERT_EQ(aom_codec_control(&enc, AV1E_SET_MATRIX_COEFFICIENTS,
                              AOM_CICP_MC_IDENTITY),
            AOM_CODEC_OK);

  // Create input image.
  aom_image_t *const image =
      CreateGrayImage(AOM_IMG_FMT_I420, cfg.g_w, cfg.g_h);
  ASSERT_NE(image, nullptr);

  // Encode frame.
  // AOM_CICP_MC_IDENTITY requires subsampling to be 0.
  EXPECT_EQ(
      aom_codec_encode(&enc, image, /*pts=*/0, /*duration=*/1, /*flags=*/0),
      AOM_CODEC_INVALID_PARAM);

  // Attempt to reconfigure with non-zero subsampling.
  EXPECT_EQ(aom_codec_control(&enc, AV1E_SET_CHROMA_SUBSAMPLING_X, 1),
            AOM_CODEC_INVALID_PARAM);
  EXPECT_EQ(aom_codec_control(&enc, AV1E_SET_CHROMA_SUBSAMPLING_Y, 1),
            AOM_CODEC_INVALID_PARAM);

  // Free resources.
  aom_img_free(image);
  ASSERT_EQ(aom_codec_destroy(&enc), AOM_CODEC_OK);
}

void ReproBuganizer487259772(const bool row_mt, int initial_threads = 2) {
  AV1Encoder encoder(7);

  encoder.Configure(initial_threads, 800, 600, AOM_VBR, AOM_USAGE_REALTIME);
  // This is not exposed by the WebCodecs interface. It's set to 1 in Chrome's
  // implementation.
  ASSERT_EQ(aom_codec_control(encoder.GetCodecCtx(), AV1E_SET_ROW_MT, row_mt),
            AOM_CODEC_OK);
  encoder.Encode(false);
  encoder.Encode(false);
  encoder.Encode(false);

  encoder.Configure(1, 352, 288, AOM_VBR, AOM_USAGE_REALTIME);
  encoder.Encode(false);
  encoder.Encode(false);
  encoder.Encode(false);
  encoder.Encode(false);

  encoder.Configure(1, 48, 480, AOM_VBR, AOM_USAGE_REALTIME);
  encoder.Encode(false);
  encoder.Encode(true);
  encoder.Encode(false);

  encoder.Configure(1, 8, 8, AOM_VBR, AOM_USAGE_REALTIME);
  encoder.Encode(false);
  encoder.Encode(false);

  encoder.Configure(1, 24, 24, AOM_VBR, AOM_USAGE_REALTIME);
  encoder.Encode(false);
  encoder.Encode(false);

  encoder.Configure(1, 97, 53, AOM_VBR, AOM_USAGE_REALTIME);
  encoder.Encode(false);
  encoder.Encode(false);

  encoder.Configure(1, 32, 320, AOM_VBR, AOM_USAGE_REALTIME);
  encoder.Encode(false);
  encoder.Encode(false);
}

TEST(EncodeAPI, Buganizer487259772NoThreads) {
  ReproBuganizer487259772(/*row_mt=*/false, /*initial_threads=*/1);
}

// TODO: bug 487259772 - Enable this test after assertion/crash (NULL mbmi) in
// av1_loopfilter is fixed.
TEST(EncodeAPI, DISABLED_Buganizer487259772NoRowMT) {
  ReproBuganizer487259772(/*row_mt=*/false);
}

TEST(EncodeAPI, Buganizer487259772RowMT) {
  ReproBuganizer487259772(/*row_mt=*/true);
}

class EncodeAPIParameterized
    : public testing::TestWithParam<std::tuple<
          /*usage=*/unsigned int, /*speed=*/int, /*aq_mode=*/unsigned int>> {};

// Encodes two frames at a given usage, speed, and aq_mode setting.
// Reproduces b/303023614
TEST_P(EncodeAPIParameterized, HighBDEncoderHighBDFrames) {
  const unsigned int usage = std::get<0>(GetParam());
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
  ASSERT_EQ(init_status, AOM_CODEC_OK);

  const unsigned int aq_mode = std::get<2>(GetParam());

  ASSERT_EQ(aom_codec_control(&enc, AOME_SET_CPUUSED, speed), AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_control(&enc, AV1E_SET_AQ_MODE, aq_mode), AOM_CODEC_OK);

  aom_image_t *image =
      aom_img_alloc(nullptr, AOM_IMG_FMT_I42016, cfg.g_w, cfg.g_h, 0);
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

const unsigned int kUsages[] = {
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

TEST(EncodeAPI, AllIntraAndUsePsnr) {
  aom_codec_iface_t *iface = aom_codec_av1_cx();
  aom_codec_enc_cfg_t cfg;
  ASSERT_EQ(aom_codec_enc_config_default(iface, &cfg, AOM_USAGE_ALL_INTRA),
            AOM_CODEC_OK);

  aom_codec_ctx_t enc;
  ASSERT_EQ(aom_codec_enc_init(&enc, iface, &cfg, AOM_CODEC_USE_PSNR),
            AOM_CODEC_OK);

  aom_image_t *image = CreateGrayImage(AOM_IMG_FMT_I420, cfg.g_w, cfg.g_h);
  ASSERT_NE(image, nullptr);

  ASSERT_EQ(aom_codec_encode(&enc, image, 0, 1, 0), AOM_CODEC_OK);
  const aom_codec_cx_pkt_t *pkt;
  aom_codec_iter_t iter = nullptr;
  while ((pkt = aom_codec_get_cx_data(&enc, &iter)) != nullptr) {
    if (pkt->kind != AOM_CODEC_CX_FRAME_PKT) {
      ASSERT_EQ(pkt->kind, AOM_CODEC_PSNR_PKT);
    }
  }

  aom_img_free(image);
  ASSERT_EQ(aom_codec_destroy(&enc), AOM_CODEC_OK);
}

TEST(EncodeAPI, AllIntraAndTuneIq) {
  aom_codec_iface_t *iface = aom_codec_av1_cx();
  aom_codec_enc_cfg_t cfg;
  ASSERT_EQ(aom_codec_enc_config_default(iface, &cfg, AOM_USAGE_ALL_INTRA),
            AOM_CODEC_OK);

  aom_codec_ctx_t enc;
  ASSERT_EQ(aom_codec_enc_init(&enc, iface, &cfg, 0), AOM_CODEC_OK);

  ASSERT_EQ(aom_codec_control(&enc, AOME_SET_TUNING, AOM_TUNE_SSIMULACRA2),
            AOM_CODEC_OK);

  aom_image_t *image = CreateGrayImage(AOM_IMG_FMT_I420, cfg.g_w, cfg.g_h);
  ASSERT_NE(image, nullptr);

  ASSERT_EQ(aom_codec_encode(&enc, image, 0, 1, 0), AOM_CODEC_OK);
  const aom_codec_cx_pkt_t *pkt;
  aom_codec_iter_t iter = nullptr;
  pkt = aom_codec_get_cx_data(&enc, &iter);
  ASSERT_NE(pkt, nullptr);
  ASSERT_EQ(pkt->kind, AOM_CODEC_CX_FRAME_PKT);
  pkt = aom_codec_get_cx_data(&enc, &iter);
  ASSERT_EQ(pkt, nullptr);

  // Flush the encoder.
  ASSERT_EQ(aom_codec_encode(&enc, nullptr, 0, 0, 0), AOM_CODEC_OK);
  iter = nullptr;
  pkt = aom_codec_get_cx_data(&enc, &iter);
  ASSERT_EQ(pkt, nullptr);

  aom_img_free(image);
  ASSERT_EQ(aom_codec_destroy(&enc), AOM_CODEC_OK);
}

// A test that reproduces bug aomedia:3534.
TEST(EncodeAPI, AllIntraAndNoRefLast) {
  aom_codec_iface_t *iface = aom_codec_av1_cx();
  aom_codec_enc_cfg_t cfg;
  ASSERT_EQ(aom_codec_enc_config_default(iface, &cfg, AOM_USAGE_ALL_INTRA),
            AOM_CODEC_OK);

  aom_codec_ctx_t enc;
  ASSERT_EQ(aom_codec_enc_init(&enc, iface, &cfg, 0), AOM_CODEC_OK);

  aom_image_t *image = CreateGrayImage(AOM_IMG_FMT_I420, cfg.g_w, cfg.g_h);
  ASSERT_NE(image, nullptr);

  ASSERT_EQ(aom_codec_encode(&enc, image, 0, 1, AOM_EFLAG_NO_REF_LAST),
            AOM_CODEC_OK);

  aom_img_free(image);
  ASSERT_EQ(aom_codec_destroy(&enc), AOM_CODEC_OK);
}
#endif  // !CONFIG_REALTIME_ONLY

TEST(EncodeAPI, PerFramePsnr) {
  aom_codec_iface_t *iface = aom_codec_av1_cx();
  aom_codec_enc_cfg_t cfg;
  ASSERT_EQ(aom_codec_enc_config_default(iface, &cfg, AOM_USAGE_REALTIME),
            AOM_CODEC_OK);
  ASSERT_EQ(cfg.g_lag_in_frames, 0);

  aom_codec_ctx_t enc;
  ASSERT_EQ(aom_codec_enc_init(&enc, iface, &cfg, 0), AOM_CODEC_OK);

  aom_image_t *image = CreateGrayImage(AOM_IMG_FMT_I420, cfg.g_w, cfg.g_h);
  ASSERT_NE(image, nullptr);

  aom_enc_frame_flags_t psnr_flags = AOM_EFLAG_CALCULATE_PSNR;
  ASSERT_EQ(
      aom_codec_encode(&enc, image, /*pts=*/0, /*duration=*/1, psnr_flags),
      AOM_CODEC_OK);

  const aom_codec_cx_pkt_t *pkt;
  aom_codec_iter_t iter = nullptr;
  bool had_psnr = false;
  while ((pkt = aom_codec_get_cx_data(&enc, &iter)) != nullptr) {
    if (pkt->kind != AOM_CODEC_CX_FRAME_PKT) {
      ASSERT_EQ(pkt->kind, AOM_CODEC_PSNR_PKT);
      had_psnr = true;
    }
  }
  EXPECT_TRUE(had_psnr);

  aom_enc_frame_flags_t no_psnr_flags = 0;
  ASSERT_EQ(
      aom_codec_encode(&enc, image, /*pts=*/1, /*duration=*/1, no_psnr_flags),
      AOM_CODEC_OK);

  iter = nullptr;
  had_psnr = false;
  while ((pkt = aom_codec_get_cx_data(&enc, &iter)) != nullptr) {
    if (pkt->kind != AOM_CODEC_CX_FRAME_PKT) {
      ASSERT_EQ(pkt->kind, AOM_CODEC_PSNR_PKT);
      had_psnr = true;
    }
  }
#if CONFIG_INTERNAL_STATS
  // CONFIG_INTERNAL_STATS unconditionally generates PSNR.
  EXPECT_TRUE(had_psnr);
#else
  EXPECT_FALSE(had_psnr);
#endif  // CONFIG_INTERNAL_STATS

  aom_img_free(image);
  ASSERT_EQ(aom_codec_destroy(&enc), AOM_CODEC_OK);
}

#if !CONFIG_REALTIME_ONLY
TEST(EncodeAPI, PerFramePsnrGoodQualityZeroLagInFrames) {
  aom_codec_iface_t *iface = aom_codec_av1_cx();
  aom_codec_enc_cfg_t cfg;
  ASSERT_EQ(aom_codec_enc_config_default(iface, &cfg, AOM_USAGE_GOOD_QUALITY),
            AOM_CODEC_OK);
  ASSERT_NE(cfg.g_lag_in_frames, 0);
  cfg.g_lag_in_frames = 0;

  aom_codec_ctx_t enc;
  ASSERT_EQ(aom_codec_enc_init(&enc, iface, &cfg, 0), AOM_CODEC_OK);

  aom_image_t *image = CreateGrayImage(AOM_IMG_FMT_I420, cfg.g_w, cfg.g_h);
  ASSERT_NE(image, nullptr);

  aom_enc_frame_flags_t psnr_flags = AOM_EFLAG_CALCULATE_PSNR;
  ASSERT_EQ(
      aom_codec_encode(&enc, image, /*pts=*/0, /*duration=*/1, psnr_flags),
      AOM_CODEC_OK);
  const aom_codec_cx_pkt_t *pkt;
  aom_codec_iter_t iter = nullptr;
  bool had_psnr = false;
  while ((pkt = aom_codec_get_cx_data(&enc, &iter)) != nullptr) {
    if (pkt->kind != AOM_CODEC_CX_FRAME_PKT) {
      ASSERT_EQ(pkt->kind, AOM_CODEC_PSNR_PKT);
      had_psnr = true;
    }
  }
  EXPECT_TRUE(had_psnr);

  aom_enc_frame_flags_t no_psnr_flags = 0;
  ASSERT_EQ(
      aom_codec_encode(&enc, image, /*pts=*/1, /*duration=*/1, no_psnr_flags),
      AOM_CODEC_OK);
  iter = nullptr;
  had_psnr = false;
  while ((pkt = aom_codec_get_cx_data(&enc, &iter)) != nullptr) {
    if (pkt->kind != AOM_CODEC_CX_FRAME_PKT) {
      ASSERT_EQ(pkt->kind, AOM_CODEC_PSNR_PKT);
      had_psnr = true;
    }
  }
#if CONFIG_INTERNAL_STATS
  // CONFIG_INTERNAL_STATS unconditionally generates PSNR.
  EXPECT_TRUE(had_psnr);
#else
  EXPECT_FALSE(had_psnr);
#endif  // CONFIG_INTERNAL_STATS
  aom_img_free(image);
  ASSERT_EQ(aom_codec_destroy(&enc), AOM_CODEC_OK);
}

TEST(EncodeAPI, PerFramePsnrNotSupportedWithLagInFrames) {
  aom_codec_iface_t *iface = aom_codec_av1_cx();
  aom_codec_enc_cfg_t cfg;
  ASSERT_EQ(aom_codec_enc_config_default(iface, &cfg, AOM_USAGE_GOOD_QUALITY),
            AOM_CODEC_OK);
  ASSERT_NE(cfg.g_lag_in_frames, 0);

  aom_codec_ctx_t enc;
  ASSERT_EQ(aom_codec_enc_init(&enc, iface, &cfg, 0), AOM_CODEC_OK);

  aom_image_t *image = CreateGrayImage(AOM_IMG_FMT_I420, cfg.g_w, cfg.g_h);
  ASSERT_NE(image, nullptr);

  aom_enc_frame_flags_t psnr_flags = AOM_EFLAG_CALCULATE_PSNR;
  ASSERT_EQ(
      aom_codec_encode(&enc, image, /*pts=*/0, /*duration=*/1, psnr_flags),
      AOM_CODEC_INCAPABLE);

  aom_img_free(image);
  ASSERT_EQ(aom_codec_destroy(&enc), AOM_CODEC_OK);
}
#endif  // !CONFIG_REALTIME_ONLY

TEST(EncodeAPI, FreezeInternalState) {
  aom_codec_iface_t *iface = aom_codec_av1_cx();
  aom_codec_enc_cfg_t cfg;
  ASSERT_EQ(aom_codec_enc_config_default(iface, &cfg, kUsage), AOM_CODEC_OK);
  cfg.g_w = 176;
  cfg.g_h = 144;
  cfg.rc_target_bitrate = 200;
  cfg.g_lag_in_frames = 0;  // Needed for single frame updates

  aom_codec_ctx_t enc;
  ASSERT_EQ(aom_codec_enc_init(&enc, iface, &cfg, AOM_CODEC_USE_PSNR),
            AOM_CODEC_OK);

  aom_image_t *image = CreateGrayImage(AOM_IMG_FMT_I420, cfg.g_w, cfg.g_h);
  ASSERT_NE(image, nullptr);

  // Encode Frame A (Keyframe)
  ASSERT_EQ(aom_codec_encode(&enc, image, /*pts=*/0, /*duration=*/1,
                             /*flags=*/AOM_EFLAG_FORCE_KF),
            AOM_CODEC_OK);

  aom_codec_iter_t iter = nullptr;
  while (aom_codec_get_cx_data(&enc, &iter) != nullptr) {
    // Drain packets
  }

  std::vector<uint8_t> bitstream1;
  bool psnr1 = false;

  // Encode Frame B with freeze flag
  ASSERT_EQ(aom_codec_encode(&enc, image, /*pts=*/1, /*duration=*/1,
                             /*flags=*/AOM_EFLAG_FREEZE_INTERNAL_STATE |
                                 AOM_EFLAG_CALCULATE_PSNR),
            AOM_CODEC_OK);
  iter = nullptr;
  const aom_codec_cx_pkt_t *pkt;
  while ((pkt = aom_codec_get_cx_data(&enc, &iter)) != nullptr) {
    if (pkt->kind == AOM_CODEC_CX_FRAME_PKT) {
      bitstream1.assign((uint8_t *)pkt->data.frame.buf,
                        (uint8_t *)pkt->data.frame.buf + pkt->data.frame.sz);
    } else if (pkt->kind == AOM_CODEC_PSNR_PKT) {
      psnr1 = true;
    }
  }
  EXPECT_TRUE(psnr1);
  EXPECT_FALSE(bitstream1.empty());

  std::vector<uint8_t> bitstream2;
  bool psnr2 = false;

  // Encode Frame B again without freeze flag
  ASSERT_EQ(aom_codec_encode(&enc, image, /*pts=*/1, /*duration=*/1,
                             /*flags=*/AOM_EFLAG_CALCULATE_PSNR),
            AOM_CODEC_OK);
  iter = nullptr;
  while ((pkt = aom_codec_get_cx_data(&enc, &iter)) != nullptr) {
    if (pkt->kind == AOM_CODEC_CX_FRAME_PKT) {
      bitstream2.assign((uint8_t *)pkt->data.frame.buf,
                        (uint8_t *)pkt->data.frame.buf + pkt->data.frame.sz);
    } else if (pkt->kind == AOM_CODEC_PSNR_PKT) {
      psnr2 = true;
    }
  }
  EXPECT_TRUE(psnr2);
  EXPECT_FALSE(bitstream2.empty());

  // Bitstreams should be identical
  EXPECT_EQ(bitstream1, bitstream2);

  // Encode one more frame for good measure.
  ASSERT_EQ(aom_codec_encode(&enc, image, /*pts=*/2, /*duration=*/1,
                             /*flags=*/0),
            AOM_CODEC_OK);
  while (aom_codec_get_cx_data(&enc, &iter) != nullptr) {
    // Drain packets
  }

  aom_img_free(image);
  ASSERT_EQ(aom_codec_destroy(&enc), AOM_CODEC_OK);
}

TEST(EncodeAPI, FreezeInternalStateSVC) {
  aom_codec_iface_t *iface = aom_codec_av1_cx();
  aom_codec_enc_cfg_t cfg;
  ASSERT_EQ(aom_codec_enc_config_default(iface, &cfg, AOM_USAGE_REALTIME),
            AOM_CODEC_OK);
  cfg.g_w = 176;
  cfg.g_h = 144;
  cfg.rc_target_bitrate = 300;
  cfg.g_lag_in_frames = 0;
  cfg.rc_end_usage = AOM_CBR;

  aom_codec_ctx_t enc;
  ASSERT_EQ(aom_codec_enc_init(&enc, iface, &cfg, AOM_CODEC_USE_PSNR),
            AOM_CODEC_OK);

  ASSERT_EQ(aom_codec_control(&enc, AOME_SET_CPUUSED, 7), AOM_CODEC_OK);

  aom_svc_params_t svc_params = {};
  svc_params.number_spatial_layers = 2;
  svc_params.number_temporal_layers = 1;
  svc_params.max_quantizers[0] = 56;
  svc_params.min_quantizers[0] = 10;
  svc_params.max_quantizers[1] = 56;
  svc_params.min_quantizers[1] = 10;
  svc_params.scaling_factor_num[0] = 1;
  svc_params.scaling_factor_den[0] = 2;
  svc_params.scaling_factor_num[1] = 1;
  svc_params.scaling_factor_den[1] = 1;
  svc_params.layer_target_bitrate[0] = cfg.rc_target_bitrate * 2 / 3;
  svc_params.layer_target_bitrate[1] = cfg.rc_target_bitrate;
  svc_params.framerate_factor[0] = 1;
  ASSERT_EQ(aom_codec_control(&enc, AV1E_SET_SVC_PARAMS, &svc_params),
            AOM_CODEC_OK);

  aom_image_t *image = CreateGrayImage(AOM_IMG_FMT_I420, cfg.g_w, cfg.g_h);
  ASSERT_NE(image, nullptr);

  aom_svc_layer_id_t layer_id = {};
  aom_svc_ref_frame_config_t ref_frame_config = {};

  // Encode SL0 - Keyframe
  layer_id.spatial_layer_id = 0;
  ASSERT_EQ(aom_codec_control(&enc, AV1E_SET_SVC_LAYER_ID, &layer_id),
            AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_encode(&enc, image, /*pts=*/0, /*duration=*/1,
                             /*flags=*/AOM_EFLAG_FORCE_KF),
            AOM_CODEC_OK);

  aom_codec_iter_t iter = nullptr;
  while (aom_codec_get_cx_data(&enc, &iter) != nullptr) {
  }  // Drain

  // Encode SL1 - Delta Frame with Freeze
  layer_id.spatial_layer_id = 1;
  ASSERT_EQ(aom_codec_control(&enc, AV1E_SET_SVC_LAYER_ID, &layer_id),
            AOM_CODEC_OK);
  ref_frame_config.refresh[0] = 1;
  ref_frame_config.reference[0] = 1;
  ref_frame_config.ref_idx[0] = 0;
  ASSERT_EQ(
      aom_codec_control(&enc, AV1E_SET_SVC_REF_FRAME_CONFIG, &ref_frame_config),
      AOM_CODEC_OK);

  std::vector<uint8_t> bitstream1;
  bool psnr1 = false;
  ASSERT_EQ(aom_codec_encode(&enc, image, /*pts=*/1, /*duration=*/1,
                             /*flags=*/AOM_EFLAG_FREEZE_INTERNAL_STATE |
                                 AOM_EFLAG_CALCULATE_PSNR),
            AOM_CODEC_OK);
  iter = nullptr;
  const aom_codec_cx_pkt_t *pkt;
  while ((pkt = aom_codec_get_cx_data(&enc, &iter)) != nullptr) {
    if (pkt->kind == AOM_CODEC_CX_FRAME_PKT) {
      bitstream1.assign((uint8_t *)pkt->data.frame.buf,
                        (uint8_t *)pkt->data.frame.buf + pkt->data.frame.sz);
    } else if (pkt->kind == AOM_CODEC_PSNR_PKT) {
      psnr1 = true;
    }
  }
  EXPECT_TRUE(psnr1);
  EXPECT_FALSE(bitstream1.empty());

  // Encode SL1 - Delta Frame again without Freeze
  std::vector<uint8_t> bitstream2;
  bool psnr2 = false;
  ASSERT_EQ(aom_codec_encode(&enc, image, /*pts=*/1, /*duration=*/1,
                             /*flags=*/AOM_EFLAG_CALCULATE_PSNR),
            AOM_CODEC_OK);
  iter = nullptr;
  while ((pkt = aom_codec_get_cx_data(&enc, &iter)) != nullptr) {
    if (pkt->kind == AOM_CODEC_CX_FRAME_PKT) {
      bitstream2.assign((uint8_t *)pkt->data.frame.buf,
                        (uint8_t *)pkt->data.frame.buf + pkt->data.frame.sz);
    } else if (pkt->kind == AOM_CODEC_PSNR_PKT) {
      psnr2 = true;
    }
  }
  EXPECT_TRUE(psnr2);
  EXPECT_FALSE(bitstream2.empty());

  EXPECT_EQ(bitstream1, bitstream2);

  // Encode one more frame for good measure.
  layer_id.spatial_layer_id = 0;
  ASSERT_EQ(aom_codec_control(&enc, AV1E_SET_SVC_LAYER_ID, &layer_id),
            AOM_CODEC_OK);
  ref_frame_config.refresh[0] = 1;
  ref_frame_config.reference[0] = 1;
  ref_frame_config.ref_idx[0] = 0;
  ASSERT_EQ(aom_codec_encode(&enc, image, /*pts=*/2, /*duration=*/1,
                             /*flags=*/0),
            AOM_CODEC_OK);
  while (aom_codec_get_cx_data(&enc, &iter) != nullptr) {
    // Drain packets
  }

  aom_img_free(image);
  ASSERT_EQ(aom_codec_destroy(&enc), AOM_CODEC_OK);
}

TEST(EncodeAPI, FreezeInternalStateTemporalLayers) {
  aom_codec_iface_t *iface = aom_codec_av1_cx();
  aom_codec_enc_cfg_t cfg;
  ASSERT_EQ(aom_codec_enc_config_default(iface, &cfg, AOM_USAGE_REALTIME),
            AOM_CODEC_OK);
  cfg.g_w = 176;
  cfg.g_h = 144;
  cfg.rc_target_bitrate = 300;
  cfg.g_lag_in_frames = 0;
  cfg.rc_end_usage = AOM_CBR;

  aom_codec_ctx_t enc;
  ASSERT_EQ(aom_codec_enc_init(&enc, iface, &cfg, AOM_CODEC_USE_PSNR),
            AOM_CODEC_OK);

  ASSERT_EQ(aom_codec_control(&enc, AOME_SET_CPUUSED, 7), AOM_CODEC_OK);

  aom_svc_params_t svc_params = {};
  svc_params.number_spatial_layers = 1;
  svc_params.number_temporal_layers = 2;
  svc_params.max_quantizers[0] = 56;
  svc_params.min_quantizers[0] = 10;
  svc_params.max_quantizers[1] = 56;
  svc_params.min_quantizers[1] = 10;
  svc_params.scaling_factor_num[0] = 1;
  svc_params.scaling_factor_den[0] = 1;
  svc_params.layer_target_bitrate[0] = cfg.rc_target_bitrate * 2 / 3;
  svc_params.layer_target_bitrate[1] = cfg.rc_target_bitrate;
  svc_params.framerate_factor[0] = 1;
  svc_params.framerate_factor[1] = 2;
  ASSERT_EQ(aom_codec_control(&enc, AV1E_SET_SVC_PARAMS, &svc_params),
            AOM_CODEC_OK);

  aom_image_t *image = CreateGrayImage(AOM_IMG_FMT_I420, cfg.g_w, cfg.g_h);
  ASSERT_NE(image, nullptr);

  aom_svc_layer_id_t layer_id = {};
  aom_svc_ref_frame_config_t ref_frame_config = {};

  // Encode a Keyframe.
  layer_id.spatial_layer_id = 0;
  layer_id.temporal_layer_id = 0;
  ASSERT_EQ(aom_codec_control(&enc, AV1E_SET_SVC_LAYER_ID, &layer_id),
            AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_encode(&enc, image, /*pts=*/0, /*duration=*/1,
                             /*flags=*/AOM_EFLAG_FORCE_KF),
            AOM_CODEC_OK);

  aom_codec_iter_t iter = nullptr;
  while (aom_codec_get_cx_data(&enc, &iter) != nullptr) {
  }  // Drain

  // Encode a TL1 - Delta Frame
  layer_id.temporal_layer_id = 1;
  ASSERT_EQ(aom_codec_control(&enc, AV1E_SET_SVC_LAYER_ID, &layer_id),
            AOM_CODEC_OK);
  ref_frame_config.refresh[0] = 0;
  ref_frame_config.reference[0] = 1;
  ref_frame_config.ref_idx[0] = 0;
  ASSERT_EQ(
      aom_codec_control(&enc, AV1E_SET_SVC_REF_FRAME_CONFIG, &ref_frame_config),
      AOM_CODEC_OK);

  ASSERT_EQ(aom_codec_encode(&enc, image, /*pts=*/1, /*duration=*/1,
                             /*flags=*/0),
            AOM_CODEC_OK);

  while (aom_codec_get_cx_data(&enc, &iter) != nullptr) {
  }  // Drain

  // Encode a TL0 - Delta Frame, with frozen state.
  layer_id.temporal_layer_id = 0;
  ASSERT_EQ(aom_codec_control(&enc, AV1E_SET_SVC_LAYER_ID, &layer_id),
            AOM_CODEC_OK);
  ref_frame_config.refresh[0] = 1;
  ref_frame_config.reference[0] = 1;
  ref_frame_config.ref_idx[0] = 0;

  std::vector<uint8_t> bitstream1;
  bool psnr1 = false;
  ASSERT_EQ(aom_codec_encode(&enc, image, /*pts=*/2, /*duration=*/1,
                             /*flags=*/AOM_EFLAG_FREEZE_INTERNAL_STATE |
                                 AOM_EFLAG_CALCULATE_PSNR),
            AOM_CODEC_OK);
  iter = nullptr;
  const aom_codec_cx_pkt_t *pkt;
  while ((pkt = aom_codec_get_cx_data(&enc, &iter)) != nullptr) {
    if (pkt->kind == AOM_CODEC_CX_FRAME_PKT) {
      bitstream1.assign((uint8_t *)pkt->data.frame.buf,
                        (uint8_t *)pkt->data.frame.buf + pkt->data.frame.sz);
    } else if (pkt->kind == AOM_CODEC_PSNR_PKT) {
      psnr1 = true;
    }
  }
  EXPECT_TRUE(psnr1);
  EXPECT_FALSE(bitstream1.empty());

  // Encode TL1 - Delta Frame again without frozen state.
  std::vector<uint8_t> bitstream2;
  bool psnr2 = false;
  ASSERT_EQ(aom_codec_encode(&enc, image, /*pts=*/2, /*duration=*/1,
                             /*flags=*/AOM_EFLAG_CALCULATE_PSNR),
            AOM_CODEC_OK);
  iter = nullptr;
  while ((pkt = aom_codec_get_cx_data(&enc, &iter)) != nullptr) {
    if (pkt->kind == AOM_CODEC_CX_FRAME_PKT) {
      bitstream2.assign((uint8_t *)pkt->data.frame.buf,
                        (uint8_t *)pkt->data.frame.buf + pkt->data.frame.sz);
    } else if (pkt->kind == AOM_CODEC_PSNR_PKT) {
      psnr2 = true;
    }
  }
  EXPECT_TRUE(psnr2);
  EXPECT_FALSE(bitstream2.empty());

  EXPECT_EQ(bitstream1, bitstream2);

  // Encode one more frame for good measure.
  layer_id.temporal_layer_id = 1;
  ASSERT_EQ(aom_codec_control(&enc, AV1E_SET_SVC_LAYER_ID, &layer_id),
            AOM_CODEC_OK);
  ref_frame_config.refresh[0] = 0;
  ref_frame_config.reference[0] = 1;
  ref_frame_config.ref_idx[0] = 0;
  ASSERT_EQ(aom_codec_encode(&enc, image, /*pts=*/3, /*duration=*/1,
                             /*flags=*/0),
            AOM_CODEC_OK);
  while (aom_codec_get_cx_data(&enc, &iter) != nullptr) {
    // Drain packets
  }

  aom_img_free(image);
  ASSERT_EQ(aom_codec_destroy(&enc), AOM_CODEC_OK);
}

TEST(EncodeAPI, FreezeInternalStateNotAllowedWithNonZeroLag) {
  aom_codec_iface_t *iface = aom_codec_av1_cx();
  aom_codec_enc_cfg_t cfg;
  ASSERT_EQ(aom_codec_enc_config_default(iface, &cfg, kUsage), AOM_CODEC_OK);
  cfg.g_w = 176;
  cfg.g_h = 144;
  cfg.rc_target_bitrate = 200;
  cfg.g_lag_in_frames =
      1;  // Not supported with AOM_EFLAG_FREEZE_INTERNAL_STATE.

  aom_codec_ctx_t enc;
  ASSERT_EQ(aom_codec_enc_init(&enc, iface, &cfg, 0), AOM_CODEC_OK);

  aom_image_t *image = CreateGrayImage(AOM_IMG_FMT_I420, cfg.g_w, cfg.g_h);
  ASSERT_NE(image, nullptr);

  ASSERT_EQ(aom_codec_encode(
                &enc, image, /*pts=*/0, /*duration=*/1,
                /*flags=*/AOM_EFLAG_FREEZE_INTERNAL_STATE | AOM_EFLAG_FORCE_KF),
            AOM_CODEC_INCAPABLE);

  aom_img_free(image);
  ASSERT_EQ(aom_codec_destroy(&enc), AOM_CODEC_OK);
}

#if !CONFIG_REALTIME_ONLY
// This test is based on the issue:449376308. The segfault in
// av1_update_layer_context_change_config() is triggered
// by doing svc with nonzero lag_in_frames and good_quality usage.
// Note good_quality mode is needed because for realtime mode lag_in_frames
// is forced to 0 in set_encoder_config() .
TEST(EncodeAPI, Issue449376308) {
  aom_codec_iface_t *iface = aom_codec_av1_cx();
  aom_codec_enc_cfg_t cfg;
  ASSERT_EQ(aom_codec_enc_config_default(iface, &cfg, AOM_USAGE_GOOD_QUALITY),
            AOM_CODEC_OK);
  cfg.g_w = 320;
  cfg.g_h = 240;
  cfg.g_timebase.num = 1;
  cfg.g_timebase.den = 30;
  cfg.rc_target_bitrate = 1000;
  cfg.g_lag_in_frames = 25;
  aom_codec_ctx_t enc;
  ASSERT_EQ(aom_codec_enc_init(&enc, iface, &cfg, 0), AOM_CODEC_OK);
  // AV1E_SET_SVC_PARAMS
  aom_svc_params_t svc_params = {};
  svc_params.number_spatial_layers = 3;
  svc_params.number_temporal_layers = 2;
  for (int i = 0; i < AOM_MAX_LAYERS; i++) {
    svc_params.max_quantizers[i] = 30;
    svc_params.min_quantizers[i] = 0;
    svc_params.layer_target_bitrate[i] = 1000;
  }
  for (int i = 0; i < AOM_MAX_SS_LAYERS; i++) {
    svc_params.scaling_factor_num[i] = 1;
    svc_params.scaling_factor_den[i] = 1;
  }
  for (int i = 0; i < AOM_MAX_TS_LAYERS; i++) {
    svc_params.framerate_factor[i] = 1;
  }
  // set_svc_params should fail since lag_in_frames > 0.
  EXPECT_NE(aom_codec_control(&enc, AV1E_SET_SVC_PARAMS, &svc_params),
            AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_destroy(&enc), AOM_CODEC_OK);
}
#endif

// This test is based on the issue:449341177. The issue occurs in the
// codec_destroy after invalid params (quantizer out of range) are passed
// to the set_svc_params control. The issue can occur for realtime mode
// with lag_in_frames = 0.
TEST(EncodeAPI, Issue449341177) {
  aom_codec_iface_t *iface = aom_codec_av1_cx();
  aom_codec_enc_cfg_t cfg;
  ASSERT_EQ(aom_codec_enc_config_default(iface, &cfg, AOM_USAGE_REALTIME),
            AOM_CODEC_OK);
  cfg.g_w = 320;
  cfg.g_h = 240;
  cfg.rc_target_bitrate = 500;
  cfg.g_timebase.num = 1;
  cfg.g_timebase.den = 30;
  cfg.rc_end_usage = AOM_CBR;
  cfg.g_lag_in_frames = 0;
  aom_codec_ctx_t enc;
  ASSERT_EQ(aom_codec_enc_init(&enc, iface, &cfg, 0), AOM_CODEC_OK);
  // AV1E_SET_SVC_PARAMS
  aom_svc_params_t svc_params = {};
  svc_params.number_spatial_layers = 3;
  svc_params.number_temporal_layers = 2;
  for (int i = 0; i < AOM_MAX_LAYERS; i++) {
    // Set quantizer out of range.
    svc_params.max_quantizers[i] = 80;
    svc_params.min_quantizers[i] = 0;
    svc_params.layer_target_bitrate[i] = 1000;
  }
  for (int i = 0; i < AOM_MAX_SS_LAYERS; i++) {
    svc_params.scaling_factor_num[i] = 1;
    svc_params.scaling_factor_den[i] = 1;
  }
  for (int i = 0; i < AOM_MAX_TS_LAYERS; i++) {
    svc_params.framerate_factor[i] = 1;
  }
  // set_svc_params should fail because svc_params.max_quantizer[i] is set out
  // of range.
  EXPECT_NE(aom_codec_control(&enc, AV1E_SET_SVC_PARAMS, &svc_params),
            AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_destroy(&enc), AOM_CODEC_OK);
}

// This test is based on the issue:471095598. For profile 2 444 12 bit,
// assert is triggered on first frame in intra_mode_search.c, the function
// av1_count_colors_highbd, for good quality mode.
// Disabled until the assert issue is fixed.
TEST(EncodeAPI, DISABLED_Issue471095598) {
  aom_codec_iface_t *encoder_iface = aom_codec_av1_cx();
  aom_codec_enc_cfg_t cfg;
  ASSERT_EQ(
      aom_codec_enc_config_default(encoder_iface, &cfg, AOM_USAGE_GOOD_QUALITY),
      AOM_CODEC_OK);
  cfg.g_profile = 2;
  cfg.g_bit_depth = AOM_BITS_12;
  cfg.g_lag_in_frames = 0;
  aom_codec_ctx_t encoder;
  ASSERT_EQ(aom_codec_enc_init(&encoder, encoder_iface, &cfg,
                               AOM_CODEC_USE_HIGHBITDEPTH),
            AOM_CODEC_OK);
  aom_image_t *img = CreateGrayImage(AOM_IMG_FMT_I44416, cfg.g_w, cfg.g_h);
  ASSERT_EQ(aom_codec_encode(&encoder, img, 0, 1, 0), AOM_CODEC_OK);
  aom_img_free(img);
  ASSERT_EQ(aom_codec_destroy(&encoder), AOM_CODEC_OK);
}

#if !CONFIG_REALTIME_ONLY
class GetGopInfoTest : public ::libaom_test::EncoderTest,
                       public ::testing::Test {
 protected:
  GetGopInfoTest() : EncoderTest(&::libaom_test::kAV1) {}
  ~GetGopInfoTest() override = default;

  void SetUp() override {
    InitializeConfig(::libaom_test::kTwoPassGood);
    cfg_.g_w = 176;
    cfg_.g_h = 144;
    cfg_.rc_target_bitrate = 200;
    cfg_.g_lag_in_frames = 25;
    cfg_.g_limit = kFrameLimit;
  }

  void PreEncodeFrameHook(::libaom_test::VideoSource *video,
                          ::libaom_test::Encoder *encoder) override {
    if (video->frame() == 0) {
      encoder->Control(AOME_SET_CPUUSED, 3);
    }
  }

  void PostEncodeFrameHook(::libaom_test::Encoder *encoder) override {
    if (cfg_.g_pass == AOM_RC_FIRST_PASS) return;
    encoder->Control(AV1E_GET_GOP_INFO, &gop_info_);
  }

  void FramePktHook(const aom_codec_cx_pkt_t * /*pkt*/) override {
    // This is verified here (not in PostEncodeFrameHook) because
    // PostEncodeFrameHook is also called when encoder is reading frames into
    // lookahead buffer, when GOP structure hasn't been determined.
    ASSERT_GT(gop_info_.gop_size, 0);
  }

  aom_gop_info_t gop_info_;
  static constexpr int kFrameLimit = 10;
};

TEST_F(GetGopInfoTest, GetGopInfo) {
  ::libaom_test::RandomVideoSource video;
  video.SetSize(cfg_.g_w, cfg_.g_h);
  video.set_limit(kFrameLimit);
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
}
#endif  // !CONFIG_REALTIME_ONLY

TEST(EncodeAPI, SizeAlignOverflow) {
  aom_codec_iface_t *iface = aom_codec_av1_cx();
  aom_codec_enc_cfg_t cfg;
  ASSERT_EQ(aom_codec_enc_config_default(iface, &cfg, AOM_USAGE_REALTIME),
            AOM_CODEC_OK);

  cfg.g_w = 16;
  cfg.g_h = 16;
  cfg.g_threads = 1;
  cfg.g_lag_in_frames = 0;

  aom_codec_ctx_t enc;
  ASSERT_EQ(aom_codec_enc_init(&enc, iface, &cfg, 0), AOM_CODEC_OK);

  // Bug aomdiea:480978101: size_align=32 causes w,h=32 while d_w,d_h=16
  // This mismatch causes buffer overflow in av1_copy_and_extend_frame()
  // if the fix is not present.
  aom_image_t *img =
      aom_img_alloc_with_border(NULL, AOM_IMG_FMT_NV12, /*d_w=*/16, /*d_h=*/16,
                                /*align=*/32,
                                /*size_align=*/32,
                                /*border=*/15);
  ASSERT_NE(img, nullptr);
  memset(img->img_data, 128, img->sz);

  // Should not crash with heap-buffer-overflow
  EXPECT_EQ(aom_codec_encode(&enc, img, /*pts=*/0, /*duration=*/1, /*flags=*/0),
            AOM_CODEC_OK);

  aom_img_free(img);
  ASSERT_EQ(aom_codec_destroy(&enc), AOM_CODEC_OK);
}

TEST(EncodeAPI, DynamicSvcAq0Issue499606109) {
  aom_codec_iface_t *iface = aom_codec_av1_cx();
  aom_codec_enc_cfg_t cfg;
  ASSERT_EQ(aom_codec_enc_config_default(iface, &cfg, AOM_USAGE_REALTIME),
            AOM_CODEC_OK);

  // Init at 1920x1080.
  cfg.g_w = 1920;
  cfg.g_h = 1080;
  cfg.g_timebase.num = 1;
  cfg.g_timebase.den = 30;
  cfg.rc_end_usage = AOM_CBR;
  cfg.rc_target_bitrate = 2000;
  cfg.g_lag_in_frames = 0;
  cfg.g_error_resilient = 1;
  cfg.g_threads = 1;
  cfg.kf_max_dist = 3000;
  cfg.kf_min_dist = 3000;
  cfg.rc_dropframe_thresh = 30;

  aom_codec_ctx_t codec;
  ASSERT_EQ(aom_codec_enc_init(&codec, iface, &cfg, 0), AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_control(&codec, AOME_SET_CPUUSED, 10), AOM_CODEC_OK);
  ASSERT_EQ(
      aom_codec_control(&codec, AV1E_SET_MAX_CONSEC_FRAME_DROP_MS_CBR, 34),
      AOM_CODEC_OK);

  // Phase 0: shrink to 640x360, 2 SLs. Frame 0 allocates
  // source_last_TL0 at 640x360 (small buffer).
  cfg.g_w = 640;
  cfg.g_h = 360;
  cfg.rc_target_bitrate = 500;
  ASSERT_EQ(aom_codec_enc_config_set(&codec, &cfg), AOM_CODEC_OK);
  {
    const int num[] = { 1, 1 }, den[] = { 1, 1 }, br[] = { 200, 300 };
    SetSvc(&codec, 2, num, den, br);
  }
  aom_codec_pts_t pts = 0;
  aom_image_t *raw = aom_img_alloc(nullptr, AOM_IMG_FMT_I420, 640, 360, 1);
  ASSERT_NE(raw, nullptr);
  for (int f = 0; f < 3; ++f) {
    for (int sl = 0; sl < 2; ++sl) {
      aom_svc_layer_id_t lid = { sl, 0 };
      ASSERT_EQ(aom_codec_control(&codec, AV1E_SET_SVC_LAYER_ID, &lid),
                AOM_CODEC_OK);
      FillImage(raw, static_cast<uint8_t>(128 + f));
      EncodeOne(&codec, raw, pts++);
    }
  }
  aom_img_free(raw);

  // Phase 1: grow to 1920x1080, 3 SLs. SL2 encoding sets
  // mi_cols_full_resoln = 480, mi_rows_full_resoln = 270.
  cfg.g_w = 1920;
  cfg.g_h = 1080;
  cfg.rc_target_bitrate = 2000;
  ASSERT_EQ(aom_codec_enc_config_set(&codec, &cfg), AOM_CODEC_OK);
  {
    const int num[] = { 1, 2, 1 }, den[] = { 3, 3, 1 },
              br[] = { 200, 500, 1300 };
    SetSvc(&codec, 3, num, den, br);
  }
  raw = aom_img_alloc(nullptr, AOM_IMG_FMT_I420, 1920, 1080, 1);
  ASSERT_NE(raw, nullptr);
  for (int f = 0; f < 3; ++f) {
    for (int sl = 0; sl < 3; ++sl) {
      aom_svc_layer_id_t lid = { sl, 0 };
      ASSERT_EQ(aom_codec_control(&codec, AV1E_SET_SVC_LAYER_ID, &lid),
                AOM_CODEC_OK);
      FillImage(raw, static_cast<uint8_t>(64 + f));
      EncodeOne(&codec, raw, pts++);
    }
  }
  aom_img_free(raw);

  // Phase 2: back to 640x360, 2 SLs. Encode SL0 only with mi_cols
  // stays stale. Very low SL0 bitrate forces a frame drop.
  // After the drop, next SL0 gets last_source = source_last_TL0
  // (640x360 small buffer). SAD loop overruns it with OOB. */
  cfg.g_w = 640;
  cfg.g_h = 360;
  cfg.rc_target_bitrate = 500;
  ASSERT_EQ(aom_codec_enc_config_set(&codec, &cfg), AOM_CODEC_OK);
  {
    const int num[] = { 1, 1 }, den[] = { 1, 1 }, br[] = { 1, 499 };
    SetSvc(&codec, 2, num, den, br);
  }
  raw = aom_img_alloc(nullptr, AOM_IMG_FMT_I420, 640, 360, 1);
  ASSERT_NE(raw, nullptr);
  for (int f = 0; f < 20; ++f) {
    aom_svc_layer_id_t lid = { 0, 0 };
    ASSERT_EQ(aom_codec_control(&codec, AV1E_SET_SVC_LAYER_ID, &lid),
              AOM_CODEC_OK);
    FillImage(raw, static_cast<uint8_t>((f % 2 == 0) ? 16 : 235));
    EncodeOne(&codec, raw, pts++);
  }

  aom_img_free(raw);
  ASSERT_EQ(aom_codec_destroy(&codec), AOM_CODEC_OK);
}

TEST(EncodeAPI, DynamicSvcAq3Issue499606109) {
  aom_codec_iface_t *iface = aom_codec_av1_cx();
  aom_codec_enc_cfg_t cfg;
  ASSERT_EQ(aom_codec_enc_config_default(iface, &cfg, AOM_USAGE_REALTIME),
            AOM_CODEC_OK);

  // Init at 1920x1080.
  cfg.g_w = 1920;
  cfg.g_h = 1080;
  cfg.g_timebase.num = 1;
  cfg.g_timebase.den = 30;
  cfg.rc_end_usage = AOM_CBR;
  cfg.rc_target_bitrate = 2000;
  cfg.g_lag_in_frames = 0;
  cfg.g_error_resilient = 1;
  cfg.g_threads = 1;
  cfg.kf_max_dist = 3000;
  cfg.kf_min_dist = 3000;
  cfg.rc_dropframe_thresh = 30;

  aom_codec_ctx_t codec;
  ASSERT_EQ(aom_codec_enc_init(&codec, iface, &cfg, 0), AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_control(&codec, AOME_SET_CPUUSED, 10), AOM_CODEC_OK);
  ASSERT_EQ(
      aom_codec_control(&codec, AV1E_SET_MAX_CONSEC_FRAME_DROP_MS_CBR, 34),
      AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_control(&codec, AV1E_SET_AQ_MODE, 3), AOM_CODEC_OK);

  // Phase 0: shrink to 640x360, 2 SLs. Frame 0 allocates
  // source_last_TL0 at 640x360 (small buffer).
  cfg.g_w = 640;
  cfg.g_h = 360;
  cfg.rc_target_bitrate = 500;
  ASSERT_EQ(aom_codec_enc_config_set(&codec, &cfg), AOM_CODEC_OK);
  {
    const int num[] = { 1, 1 }, den[] = { 1, 1 }, br[] = { 200, 300 };
    SetSvc(&codec, 2, num, den, br);
  }
  aom_codec_pts_t pts = 0;
  aom_image_t *raw = aom_img_alloc(nullptr, AOM_IMG_FMT_I420, 640, 360, 1);
  ASSERT_NE(raw, nullptr);
  for (int f = 0; f < 3; ++f) {
    for (int sl = 0; sl < 2; ++sl) {
      aom_svc_layer_id_t lid = { sl, 0 };
      ASSERT_EQ(aom_codec_control(&codec, AV1E_SET_SVC_LAYER_ID, &lid),
                AOM_CODEC_OK);
      FillImage(raw, static_cast<uint8_t>(128 + f));
      EncodeOne(&codec, raw, pts++);
    }
  }
  aom_img_free(raw);

  // Phase 1: grow to 1920x1080, 3 SLs. SL2 encoding sets
  // mi_cols_full_resoln = 480, mi_rows_full_resoln = 270.
  cfg.g_w = 1920;
  cfg.g_h = 1080;
  cfg.rc_target_bitrate = 2000;
  ASSERT_EQ(aom_codec_enc_config_set(&codec, &cfg), AOM_CODEC_OK);
  {
    const int num[] = { 1, 2, 1 }, den[] = { 3, 3, 1 },
              br[] = { 200, 500, 1300 };
    SetSvc(&codec, 3, num, den, br);
  }
  raw = aom_img_alloc(nullptr, AOM_IMG_FMT_I420, 1920, 1080, 1);
  ASSERT_NE(raw, nullptr);
  for (int f = 0; f < 3; ++f) {
    for (int sl = 0; sl < 3; ++sl) {
      aom_svc_layer_id_t lid = { sl, 0 };
      ASSERT_EQ(aom_codec_control(&codec, AV1E_SET_SVC_LAYER_ID, &lid),
                AOM_CODEC_OK);
      FillImage(raw, static_cast<uint8_t>(64 + f));
      EncodeOne(&codec, raw, pts++);
    }
  }
  aom_img_free(raw);

  // Phase 2: back to 640x360, 2 SLs. Encode SL0 only with mi_cols
  // stays stale. Very low SL0 bitrate forces a frame drop.
  // After the drop, next SL0 gets last_source = source_last_TL0
  // (640x360 small buffer). SAD loop overruns it with OOB. */
  cfg.g_w = 640;
  cfg.g_h = 360;
  cfg.rc_target_bitrate = 500;
  ASSERT_EQ(aom_codec_enc_config_set(&codec, &cfg), AOM_CODEC_OK);
  {
    const int num[] = { 1, 1 }, den[] = { 1, 1 }, br[] = { 1, 499 };
    SetSvc(&codec, 2, num, den, br);
  }
  raw = aom_img_alloc(nullptr, AOM_IMG_FMT_I420, 640, 360, 1);
  ASSERT_NE(raw, nullptr);
  for (int f = 0; f < 20; ++f) {
    aom_svc_layer_id_t lid = { 0, 0 };
    ASSERT_EQ(aom_codec_control(&codec, AV1E_SET_SVC_LAYER_ID, &lid),
              AOM_CODEC_OK);
    FillImage(raw, static_cast<uint8_t>((f % 2 == 0) ? 16 : 235));
    EncodeOne(&codec, raw, pts++);
  }

  aom_img_free(raw);
  ASSERT_EQ(aom_codec_destroy(&codec), AOM_CODEC_OK);
}

TEST(EncodeAPI, DynamicSvcTemporalIssue502735235) {
  aom_codec_iface_t *iface = aom_codec_av1_cx();
  aom_codec_enc_cfg_t cfg;
  ASSERT_EQ(aom_codec_enc_config_default(iface, &cfg, AOM_USAGE_REALTIME),
            AOM_CODEC_OK);

  // Init at 256x512.
  cfg.g_w = 256;
  cfg.g_h = 512;
  cfg.g_timebase.num = 1;
  cfg.g_timebase.den = 30;
  cfg.rc_end_usage = AOM_CBR;
  cfg.rc_target_bitrate = 1000;
  cfg.g_lag_in_frames = 0;

  aom_codec_ctx_t codec;
  ASSERT_EQ(aom_codec_enc_init(&codec, iface, &cfg, 0), AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_control(&codec, AOME_SET_CPUUSED, 10), AOM_CODEC_OK);

  // AV1E_SET_SVC_PARAMS
  aom_svc_params_t svc_params = {};
  svc_params.number_spatial_layers = 1;
  svc_params.number_temporal_layers = 2;
  svc_params.scaling_factor_num[0] = 1;
  svc_params.scaling_factor_den[0] = 1;
  svc_params.framerate_factor[0] = 2;
  svc_params.framerate_factor[1] = 1;
  svc_params.max_quantizers[0] = 56;
  svc_params.min_quantizers[0] = 10;
  svc_params.max_quantizers[1] = 56;
  svc_params.min_quantizers[1] = 10;
  svc_params.layer_target_bitrate[0] = cfg.rc_target_bitrate * 60 / 100;
  svc_params.layer_target_bitrate[1] = cfg.rc_target_bitrate;
  EXPECT_EQ(aom_codec_control(&codec, AV1E_SET_SVC_PARAMS, &svc_params),
            AOM_CODEC_OK);

  // Encode at 256x512. TL0.
  aom_svc_layer_id_t layer_id = {};
  layer_id.spatial_layer_id = 0;
  layer_id.temporal_layer_id = 0;
  ASSERT_EQ(aom_codec_control(&codec, AV1E_SET_SVC_LAYER_ID, &layer_id),
            AOM_CODEC_OK);
  aom_image_t *raw = aom_img_alloc(nullptr, AOM_IMG_FMT_I420, 256, 512, 1);
  ASSERT_NE(raw, nullptr);
  FillImage(raw, 128);
  ASSERT_EQ(aom_codec_encode(&codec, raw, /*pts=*/0, /*duration=*/1,
                             /*flags=*/0),
            AOM_CODEC_OK);
  aom_img_free(raw);

  // Encode at 256x64 TL1, twice, set keyframe on first one, delta
  // on second.
  cfg.g_w = 256;
  cfg.g_h = 64;
  ASSERT_EQ(aom_codec_enc_config_set(&codec, &cfg), AOM_CODEC_OK);
  layer_id.spatial_layer_id = 0;
  layer_id.temporal_layer_id = 1;
  ASSERT_EQ(aom_codec_control(&codec, AV1E_SET_SVC_LAYER_ID, &layer_id),
            AOM_CODEC_OK);
  raw = aom_img_alloc(nullptr, AOM_IMG_FMT_I420, 256, 64, 1);
  ASSERT_NE(raw, nullptr);
  FillImage(raw, 128);
  ASSERT_EQ(aom_codec_encode(&codec, raw, /*pts=*/1, /*duration=*/1,
                             /*flags=*/AOM_EFLAG_FORCE_KF),
            AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_encode(&codec, raw, /*pts=*/2, /*duration=*/1,
                             /*flags=*/0),
            AOM_CODEC_OK);
  aom_img_free(raw);

  // Encode TL0 back at original resolution 256x512.
  cfg.g_w = 256;
  cfg.g_h = 512;
  ASSERT_EQ(aom_codec_enc_config_set(&codec, &cfg), AOM_CODEC_OK);
  layer_id.spatial_layer_id = 0;
  layer_id.temporal_layer_id = 0;
  ASSERT_EQ(aom_codec_control(&codec, AV1E_SET_SVC_LAYER_ID, &layer_id),
            AOM_CODEC_OK);
  raw = aom_img_alloc(nullptr, AOM_IMG_FMT_I420, 256, 512, 1);
  ASSERT_NE(raw, nullptr);
  FillImage(raw, 128);
  ASSERT_EQ(aom_codec_encode(&codec, raw, /*pts=*/3, /*duration=*/1,
                             /*flags=*/0),
            AOM_CODEC_OK);
  aom_img_free(raw);

  ASSERT_EQ(aom_codec_destroy(&codec), AOM_CODEC_OK);
}

TEST(EncodeAPI, Buganizer503197490) {
  aom_codec_iface_t *iface = aom_codec_av1_cx();
  aom_codec_enc_cfg_t cfg;
  ASSERT_EQ(aom_codec_enc_config_default(iface, &cfg, AOM_USAGE_REALTIME),
            AOM_CODEC_OK);

  cfg.g_w = 640;
  cfg.g_h = 360;
  cfg.g_timebase.num = 570;
  cfg.g_timebase.den = 30;
  cfg.rc_end_usage = AOM_CBR;
  cfg.rc_target_bitrate = 1728882;  // 1728882000 bits/s

  aom_codec_ctx_t codec;
  ASSERT_EQ(aom_codec_enc_init(&codec, iface, &cfg, 0), AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_destroy(&codec), AOM_CODEC_OK);
}

#if !CONFIG_REALTIME_ONLY
TEST(EncodeAPI, Buganizer504317456) {
  aom_codec_iface_t *iface = aom_codec_av1_cx();
  aom_codec_enc_cfg_t cfg;
  ASSERT_EQ(aom_codec_enc_config_default(iface, &cfg, AOM_USAGE_GOOD_QUALITY),
            AOM_CODEC_OK);

  cfg.g_w = 640;
  cfg.g_h = 360;
  cfg.g_lag_in_frames = 1;
  cfg.g_error_resilient = 0;

  aom_codec_ctx_t enc;
  ASSERT_EQ(aom_codec_enc_init(&enc, iface, &cfg, 0), AOM_CODEC_OK);

  aom_image_t *img = aom_img_alloc(nullptr, AOM_IMG_FMT_I420, 640, 360, 1);
  FillImage(img, 128);
  ASSERT_NE(img, nullptr);

  struct Frame {
    int64_t pts;
    unsigned long duration;
    aom_enc_frame_flags_t flags;
  } frames[] = { { 0, 2, 0 },
                 { 1075, 9, AOM_EFLAG_NO_UPD_LAST | AOM_EFLAG_NO_UPD_GF },
                 { 2099, 6, AOM_EFLAG_FORCE_KF },
                 { 3035, 5, AOM_EFLAG_NO_UPD_LAST | AOM_EFLAG_FORCE_KF } };

  for (const auto &f : frames) {
    ASSERT_EQ(aom_codec_encode(&enc, img, f.pts, f.duration, f.flags),
              AOM_CODEC_OK);
  }

  // Add 10+ additional frames to extend overflow and cause crash on destruction
  for (int i = 0; i < 15; ++i) {
    int64_t pts = 4000 + i * 100;
    aom_enc_frame_flags_t flags = (i % 3 == 0) ? AOM_EFLAG_FORCE_KF : 0;
    ASSERT_EQ(aom_codec_encode(&enc, img, pts, 5, flags), AOM_CODEC_OK);
  }

  aom_img_free(img);
  ASSERT_EQ(aom_codec_destroy(&enc), AOM_CODEC_OK);
}
#endif  // !CONFIG_REALTIME_ONLY

// Test for issue: 505976409.
TEST(EncodeAPI, SvcSourceLastTL0Uninitialized) {
  aom_codec_iface_t *iface = aom_codec_av1_cx();
  aom_codec_enc_cfg_t cfg;
  ASSERT_EQ(aom_codec_enc_config_default(iface, &cfg, AOM_USAGE_REALTIME),
            AOM_CODEC_OK);

  cfg.g_w = 64;
  cfg.g_h = 64;
  cfg.rc_end_usage = AOM_CBR;
  cfg.g_lag_in_frames = 0;
  cfg.rc_target_bitrate = 400;

  aom_codec_ctx_t codec;
  ASSERT_EQ(aom_codec_enc_init(&codec, iface, &cfg, 0), AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_control(&codec, AOME_SET_CPUUSED, 9), AOM_CODEC_OK);

  aom_svc_params_t svc = {};
  svc.number_spatial_layers = 1;
  svc.number_temporal_layers = 2;
  svc.scaling_factor_num[0] = 1;
  svc.scaling_factor_den[0] = 4;
  for (int i = 0; i < 32; ++i) svc.layer_target_bitrate[i] = 200;
  for (int i = 0; i < 8; ++i) svc.framerate_factor[i] = 1;
  ASSERT_EQ(aom_codec_control(&codec, AV1E_SET_SVC_PARAMS, &svc), AOM_CODEC_OK);

  aom_image_t *img = aom_img_alloc(nullptr, AOM_IMG_FMT_I420, 64, 64, 1);
  ASSERT_NE(img, nullptr);
  FillImage(img, 128);

  // Frame 0: TL1 (Starts with non-zero temporal layer).
  aom_svc_layer_id_t layer_id = { 0, 1 };
  ASSERT_EQ(aom_codec_control(&codec, AV1E_SET_SVC_LAYER_ID, &layer_id),
            AOM_CODEC_OK);

  aom_svc_ref_frame_config_t ref_cfg = {};
  for (int r = 0; r < 7; ++r) {
    ref_cfg.reference[r] = 1;
    ref_cfg.ref_idx[r] = 7;
  }
  ref_cfg.refresh[2] = 1;
  ref_cfg.refresh[6] = 1;
  ASSERT_EQ(aom_codec_control(&codec, AV1E_SET_SVC_REF_FRAME_CONFIG, &ref_cfg),
            AOM_CODEC_OK);

  ASSERT_EQ(aom_codec_encode(&codec, img, 0, 1, AOM_EFLAG_FORCE_KF),
            AOM_CODEC_OK);

  // Frame 1: TL0.
  layer_id.temporal_layer_id = 0;
  ASSERT_EQ(aom_codec_control(&codec, AV1E_SET_SVC_LAYER_ID, &layer_id),
            AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_encode(&codec, img, 1, 1, 0), AOM_CODEC_OK);

  // Frame 2: TL1 (Triggers use of source_last_TL0).
  layer_id.temporal_layer_id = 1;
  ASSERT_EQ(aom_codec_control(&codec, AV1E_SET_SVC_LAYER_ID, &layer_id),
            AOM_CODEC_OK);
  // This frame used to crash or hang before the fix.
  ASSERT_EQ(aom_codec_encode(&codec, img, 2, 1, 0), AOM_CODEC_OK);

  aom_img_free(img);
  ASSERT_EQ(aom_codec_destroy(&codec), AOM_CODEC_OK);
}

TEST(EncodeAPI, Buganizer503810640) {
  aom_codec_iface_t *iface = aom_codec_av1_cx();
  aom_codec_enc_cfg_t cfg;
  ASSERT_EQ(aom_codec_enc_config_default(iface, &cfg, AOM_USAGE_REALTIME),
            AOM_CODEC_OK);

  cfg.g_w = 176;
  cfg.g_h = 144;

  aom_codec_ctx_t codec;
  ASSERT_EQ(aom_codec_enc_init(&codec, iface, &cfg, 0), AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_control(&codec, AV1E_SET_DENOISE_NOISE_LEVEL, 29),
            AOM_CODEC_OK);

  aom_image_t *raw = aom_img_alloc(nullptr, AOM_IMG_FMT_I420, 176, 144, 1);
  ASSERT_NE(raw, nullptr);

  FillImage(raw, 128);
  ::libaom_test::ACMRandom rnd;
  rnd.Reset(::libaom_test::ACMRandom::DeterministicSeed());
  for (int y = 0; y < 144; ++y) {
    for (int x = 0; x < 176; ++x) {
      raw->planes[AOM_PLANE_Y][y * raw->stride[AOM_PLANE_Y] + x] = rnd.Rand8();
    }
  }

  ASSERT_EQ(aom_codec_encode(&codec, raw, 0, 1, 0), AOM_CODEC_OK);

  aom_img_free(raw);
  ASSERT_EQ(aom_codec_destroy(&codec), AOM_CODEC_OK);
}

TEST(EncodeAPI, Buganizer503810640V2) {
  aom_codec_iface_t *iface = aom_codec_av1_cx();
  aom_codec_enc_cfg_t cfg;
  ASSERT_EQ(aom_codec_enc_config_default(iface, &cfg, AOM_USAGE_REALTIME),
            AOM_CODEC_OK);

  cfg.g_w = 437;
  cfg.g_h = 1121;

  aom_codec_ctx_t codec;
  ASSERT_EQ(aom_codec_enc_init(&codec, iface, &cfg, 0), AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_control(&codec, AV1E_SET_DENOISE_NOISE_LEVEL, 29),
            AOM_CODEC_OK);

  aom_image_t *raw = aom_img_alloc(nullptr, AOM_IMG_FMT_I420, 437, 1121, 1);
  ASSERT_NE(raw, nullptr);

  FillImageRandom(raw);

  ASSERT_EQ(aom_codec_encode(&codec, raw, 0, 1, 0), AOM_CODEC_OK);

  aom_img_free(raw);
  ASSERT_EQ(aom_codec_destroy(&codec), AOM_CODEC_OK);
}

}  // namespace
