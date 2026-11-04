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
#include <vector>

#include "gtest/gtest.h"

#include "config/aom_config.h"

#include "aom/aomcx.h"
#include "aom/aom_encoder.h"
#include "aom/aom_image.h"
#if CONFIG_AV1_DECODER
#include "aom/aom_decoder.h"
#include "aom/aomdx.h"
#endif
#include "aom_mem/aom_mem.h"

#include "test/acm_random.h"
#include "test/codec_factory.h"
#include "test/encode_test_driver.h"
#include "test/md5_helper.h"
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

// Bug: 538651949.
TEST(EncodeAPI, SvcFixBypass) {
  aom_codec_ctx_t enc;
  aom_codec_enc_cfg_t cfg;
  aom_codec_iface_t *iface = aom_codec_av1_cx();
  aom_svc_layer_id_t layer_id = { 3, 0 };
  aom_svc_params_t svc_params = {};

  EXPECT_EQ(aom_codec_enc_config_default(iface, &cfg, kUsage), AOM_CODEC_OK);
  cfg.g_w = 320;
  cfg.g_h = 240;
  cfg.g_threads = 1;
  cfg.g_lag_in_frames = 0;

  ASSERT_EQ(aom_codec_enc_init(&enc, iface, &cfg, 0), AOM_CODEC_OK);

  // Set number of spatial layers to 4 via control SET_NUMBER_SPATIAL_LAYERS.
  EXPECT_EQ(aom_codec_control(&enc, AOME_SET_NUMBER_SPATIAL_LAYERS, 4),
            AOM_CODEC_OK);

  // Set the active spatial layer id to 3. This is expected to fail since
  // cpi->svc.number_spatial_layers is still 1 (default).
  EXPECT_EQ(aom_codec_control(&enc, AV1E_SET_SVC_LAYER_ID, &layer_id),
            AOM_CODEC_INVALID_PARAM);

  // Set SVC params to 2x2 layer config, via AV1E_SET_SVC_PARAMS. This sets
  // all svc layer parameters, allocates for the layer context, and sets use_svc
  // = 1.
  svc_params.number_spatial_layers = 2;
  svc_params.number_temporal_layers = 2;
  for (int i = 0; i < AOM_MAX_LAYERS; i++) {
    svc_params.max_quantizers[i] = 52;
    svc_params.min_quantizers[i] = 10;
    svc_params.layer_target_bitrate[i] = 100;
  }
  svc_params.scaling_factor_num[0] = 1;
  svc_params.scaling_factor_den[0] = 2;
  svc_params.scaling_factor_num[1] = 1;
  svc_params.scaling_factor_den[1] = 1;
  svc_params.framerate_factor[0] = 2;
  svc_params.framerate_factor[1] = 1;

  EXPECT_EQ(aom_codec_control(&enc, AV1E_SET_SVC_PARAMS, &svc_params),
            AOM_CODEC_OK);

  // Now try setting number of spatial layers to 3 using the control
  // SET_NUMBER_SPATIAL_LAYERS. This is expected to fail because SVC was
  // configured via AV1E_SET_SVC_PARAMS, so use_svc = 1.
  EXPECT_EQ(aom_codec_control(&enc, AOME_SET_NUMBER_SPATIAL_LAYERS, 3),
            AOM_CODEC_INVALID_PARAM);

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

#if !CONFIG_REALTIME_ONLY
// Test for OSS-Fuzz Issue 559019046. The bug triggers when we increase the
// number of tiles after encoding a non-intra frame and then encode a key frame.
// Based on Yuan Tong's example in
// https://github.com/AOMediaCodec/libavif/pull/1069#discussion_r4090282167:
//   magick -size 128x1024 xc:gray -alpha set -channel A -evaluate set 60%
//       +channel flat.png
//   avifenc -s 6 -j 8 -k 2 flat.png flat.png --tilerowslog2:u 2 flat.png
//       -o flat.avif
TEST(EncodeAPI, Issue559019046) {
  aom_codec_iface_t *const iface = aom_codec_av1_cx();
  aom_codec_enc_cfg_t cfg;
  const unsigned int usage = AOM_USAGE_GOOD_QUALITY;
  ASSERT_EQ(aom_codec_enc_config_default(iface, &cfg, usage), AOM_CODEC_OK);
  cfg.g_threads = 8;
  cfg.g_w = 128;
  cfg.g_h = 1024;
  cfg.g_lag_in_frames = 0;
  cfg.kf_max_dist = 2;

  aom_codec_ctx_t enc;
  ASSERT_EQ(aom_codec_enc_init(&enc, iface, &cfg, 0), AOM_CODEC_OK);

  const int speed = 6;
  ASSERT_EQ(aom_codec_control(&enc, AOME_SET_CPUUSED, speed), AOM_CODEC_OK);

  const aom_enc_frame_flags_t flags = 0;
  int frame_index = 0;

  // Encode a key frame.
  aom_image_t *image = CreateGrayImage(AOM_IMG_FMT_I420, cfg.g_w, cfg.g_h);
  ASSERT_NE(image, nullptr);
  ASSERT_EQ(aom_codec_encode(&enc, image, frame_index, 1, flags), AOM_CODEC_OK);
  frame_index++;
  const aom_codec_cx_pkt_t *pkt;
  aom_codec_iter_t iter = nullptr;
  while ((pkt = aom_codec_get_cx_data(&enc, &iter)) != nullptr) {
    ASSERT_EQ(pkt->kind, AOM_CODEC_CX_FRAME_PKT);
    ASSERT_EQ(pkt->data.frame.flags & AOM_FRAME_IS_KEY, AOM_FRAME_IS_KEY);
  }

  // Encode a non-intra frame.
  ASSERT_EQ(aom_codec_encode(&enc, image, frame_index, 1, flags), AOM_CODEC_OK);
  frame_index++;
  iter = nullptr;
  while ((pkt = aom_codec_get_cx_data(&enc, &iter)) != nullptr) {
    ASSERT_EQ(pkt->kind, AOM_CODEC_CX_FRAME_PKT);
    ASSERT_EQ(
        pkt->data.frame.flags & (AOM_FRAME_IS_KEY | AOM_FRAME_IS_INTRAONLY), 0);
  }

  // Increase the number of tiles.
  ASSERT_EQ(aom_codec_control(&enc, AV1E_SET_TILE_ROWS, 2), AOM_CODEC_OK);

  // Encode a key frame.
  ASSERT_EQ(aom_codec_encode(&enc, image, frame_index, 1, flags), AOM_CODEC_OK);
  frame_index++;
  iter = nullptr;
  while ((pkt = aom_codec_get_cx_data(&enc, &iter)) != nullptr) {
    ASSERT_EQ(pkt->kind, AOM_CODEC_CX_FRAME_PKT);
    ASSERT_EQ(pkt->data.frame.flags & AOM_FRAME_IS_KEY, AOM_FRAME_IS_KEY);
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
#endif  // !CONFIG_REALTIME_ONLY

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

// Triggers the IntraBC motion search by using screen content tools to encode a
// key frame.
// Note: The speed must be low enough to run the IntraBC pixel search.
void IntraBCFrameSizeIncreaseTest(unsigned int usage, int speed) {
  aom_codec_iface_t *iface = aom_codec_av1_cx();
  aom_codec_enc_cfg_t cfg;
  ASSERT_EQ(aom_codec_enc_config_default(iface, &cfg, usage), AOM_CODEC_OK);
  cfg.g_w = 64;
  cfg.g_h = 16;
  cfg.g_forced_max_frame_width = 64;
  cfg.g_forced_max_frame_height = 64;
  cfg.g_lag_in_frames = 0;
  cfg.g_threads = 1;

  aom_codec_ctx_t enc;
  ASSERT_EQ(aom_codec_enc_init(&enc, iface, &cfg, 0), AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_control(&enc, AOME_SET_CPUUSED, speed), AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_control(&enc, AV1E_SET_TUNE_CONTENT, AOM_CONTENT_SCREEN),
            AOM_CODEC_OK);

  aom_image_t *img_small = aom_img_alloc(nullptr, AOM_IMG_FMT_I420, 64, 16, 1);
  ASSERT_NE(img_small, nullptr);
  FillImageRandom(img_small);
  aom_image_t *img_large = aom_img_alloc(nullptr, AOM_IMG_FMT_I420, 64, 64, 1);
  ASSERT_NE(img_large, nullptr);
  FillImageRandom(img_large);

  EncodeOne(&enc, img_small, 0);

  // Only the height grows, so the reconstructed frame keeps its stride
  // (SS_CFG_SRC) while the current lookahead source buffer is reallocated in
  // av1_lookahead_push() with AOM_BORDER_IN_PIXELS (160) instead of
  // oxcf->border_in_pixels (96), changing its stride (SS_CFG_LOOKAHEAD).
  // Furthermore, for GOOD_QUALITY and REALTIME modes (even with
  // lag_in_frames = 0), the lookahead ring buffer has 2 slots (max_pre_frames =
  // 1): frame 0 was placed in buf[0], so frame 1 is placed in buf[1] while
  // buf[0] retains the old stride.
  cfg.g_h = 64;
  ASSERT_EQ(aom_codec_enc_config_set(&enc, &cfg), AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_encode(&enc, img_large, 1, 1, AOM_EFLAG_FORCE_KF),
            AOM_CODEC_OK);
  bool got_key_frame = false;
  aom_codec_iter_t iter = nullptr;
  while (const aom_codec_cx_pkt_t *pkt = aom_codec_get_cx_data(&enc, &iter)) {
    if (pkt->kind == AOM_CODEC_CX_FRAME_PKT &&
        (pkt->data.frame.flags & AOM_FRAME_IS_KEY)) {
      got_key_frame = true;
    }
  }
  EXPECT_TRUE(got_key_frame);

  aom_img_free(img_small);
  aom_img_free(img_large);
  ASSERT_EQ(aom_codec_destroy(&enc), AOM_CODEC_OK);
}

#if !CONFIG_REALTIME_ONLY
TEST(EncodeAPI, IntraBCFrameSizeIncreaseGoodQuality) {
  IntraBCFrameSizeIncreaseTest(AOM_USAGE_GOOD_QUALITY, 2);
}

TEST(EncodeAPI, IntraBCFrameSizeIncreaseAllIntra) {
  IntraBCFrameSizeIncreaseTest(AOM_USAGE_ALL_INTRA, 5);
}
#endif  // !CONFIG_REALTIME_ONLY

TEST(EncodeAPI, IntraBCFrameSizeIncreaseRealtime) {
  IntraBCFrameSizeIncreaseTest(AOM_USAGE_REALTIME, 7);
}

// Encodes a narrow key frame after a wide one with screen content tools in
// realtime mode. The source buffer of the narrow frame has a smaller stride
// than the buffer of the wide frame, so the IntraBC motion search must not use
// search sites built for the wider stride. The assertion in
// av1_full_pixel_search() checks that the strides match.
TEST(EncodeAPI, IntraBCFrameSizeDecreaseRealtime) {
  aom_codec_iface_t *iface = aom_codec_av1_cx();
  aom_codec_enc_cfg_t cfg;
  ASSERT_EQ(aom_codec_enc_config_default(iface, &cfg, AOM_USAGE_REALTIME),
            AOM_CODEC_OK);
  cfg.g_w = 1024;
  cfg.g_h = 512;
  cfg.g_lag_in_frames = 0;
  cfg.g_threads = 1;

  aom_codec_ctx_t enc;
  ASSERT_EQ(aom_codec_enc_init(&enc, iface, &cfg, 0), AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_control(&enc, AOME_SET_CPUUSED, 7), AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_control(&enc, AV1E_SET_TUNE_CONTENT, AOM_CONTENT_SCREEN),
            AOM_CODEC_OK);

  aom_image_t *img_wide =
      aom_img_alloc(nullptr, AOM_IMG_FMT_I420, 1024, 512, 1);
  ASSERT_NE(img_wide, nullptr);
  FillImageRandom(img_wide);
  aom_image_t *img_narrow =
      aom_img_alloc(nullptr, AOM_IMG_FMT_I420, 32, 512, 1);
  ASSERT_NE(img_narrow, nullptr);
  FillImageRandom(img_narrow);

  EncodeOne(&enc, img_wide, 0);

  cfg.g_w = 32;
  ASSERT_EQ(aom_codec_enc_config_set(&enc, &cfg), AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_encode(&enc, img_narrow, 1, 1, AOM_EFLAG_FORCE_KF),
            AOM_CODEC_OK);
  aom_codec_iter_t iter = nullptr;
  while (aom_codec_get_cx_data(&enc, &iter) != nullptr) {
  }

  aom_img_free(img_wide);
  aom_img_free(img_narrow);
  ASSERT_EQ(aom_codec_destroy(&enc), AOM_CODEC_OK);
}

#if CONFIG_AV1_DECODER
// Draws a text like screen texture moved left by offset_x pixels.
void FillPannedScreen(aom_image_t *img, int offset_x) {
  for (int plane = 0; plane < 3; ++plane) {
    const int ss = plane ? 1 : 0;
    const int w = static_cast<int>(img->d_w + ss) >> ss;
    const int h = static_cast<int>(img->d_h + ss) >> ss;
    for (int y = 0; y < h; ++y) {
      uint8_t *row = img->planes[plane] + y * img->stride[plane];
      for (int x = 0; x < w; ++x) {
        const int px = (x << ss) + offset_x;
        const int py = y << ss;
        const uint32_t cell = static_cast<uint32_t>(px >> 4) * 73856093u ^
                              static_cast<uint32_t>(py >> 4) * 19349663u;
        const int bg = 40 + static_cast<int>(cell % 160);
        const bool glyph = (px * 7 + py * 3) % 11 < 3 && ((px ^ py) & 4);
        const int v = glyph ? 255 - bg : bg;
        row[x] = plane ? static_cast<uint8_t>(128 + ((v - 128) >> 2))
                       : static_cast<uint8_t>(v);
      }
    }
  }
}

// Encodes an IntraBC key frame with 2 threads, then pans the content so the
// inter frames predict from past the frame edge. The loop filters are off on
// IntraBC frames, so the encoder must still extend the key frame borders for
// its reconstruction to match the decoder.
TEST(EncodeAPI, IntraBCMultiThreadBorderExtension) {
  constexpr int kWidth = 320;
  constexpr int kHeight = 192;
  constexpr int kFrames = 5;
  aom_codec_iface_t *iface = aom_codec_av1_cx();
  aom_codec_enc_cfg_t cfg;
  ASSERT_EQ(aom_codec_enc_config_default(iface, &cfg, AOM_USAGE_REALTIME),
            AOM_CODEC_OK);
  cfg.g_w = kWidth;
  cfg.g_h = kHeight;
  cfg.g_lag_in_frames = 0;
  cfg.g_threads = 2;
  cfg.rc_min_quantizer = 8;
  cfg.rc_max_quantizer = 40;

  aom_codec_ctx_t enc;
  ASSERT_EQ(aom_codec_enc_init(&enc, iface, &cfg, 0), AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_control(&enc, AOME_SET_CPUUSED, 11), AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_control(&enc, AV1E_SET_TUNE_CONTENT, AOM_CONTENT_SCREEN),
            AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_control(&enc, AV1E_SET_ENABLE_INTRABC, 1), AOM_CODEC_OK);

  // Decode to 8-bit buffers like the encoder reconstruction, even when
  // FORCE_HIGHBITDEPTH_DECODING is on.
  aom_codec_dec_cfg_t dec_cfg = {};
  dec_cfg.allow_lowbitdepth = 1;
  aom_codec_ctx_t dec;
  ASSERT_EQ(aom_codec_dec_init(&dec, aom_codec_av1_dx(), &dec_cfg, 0),
            AOM_CODEC_OK);

  aom_image_t *img =
      aom_img_alloc(nullptr, AOM_IMG_FMT_I420, kWidth, kHeight, 1);
  ASSERT_NE(img, nullptr);
  for (int i = 0; i < kFrames; ++i) {
    FillPannedScreen(img, i * 3);
    ASSERT_EQ(aom_codec_encode(&enc, img, i, 1, 0), AOM_CODEC_OK);
    aom_codec_iter_t iter = nullptr;
    while (const aom_codec_cx_pkt_t *pkt = aom_codec_get_cx_data(&enc, &iter)) {
      if (pkt->kind != AOM_CODEC_CX_FRAME_PKT) continue;
      ASSERT_EQ(aom_codec_decode(
                    &dec, static_cast<const uint8_t *>(pkt->data.frame.buf),
                    pkt->data.frame.sz, nullptr),
                AOM_CODEC_OK);
      aom_codec_iter_t dec_iter = nullptr;
      const aom_image_t *decoded = aom_codec_get_frame(&dec, &dec_iter);
      ASSERT_NE(decoded, nullptr);
      aom_image_t recon;
      ASSERT_EQ(aom_codec_control(&enc, AV1_GET_NEW_FRAME_IMAGE, &recon),
                AOM_CODEC_OK);
      ::libaom_test::MD5 recon_md5;
      recon_md5.Add(&recon);
      ::libaom_test::MD5 decoded_md5;
      decoded_md5.Add(decoded);
      EXPECT_STREQ(recon_md5.Get(), decoded_md5.Get()) << "frame " << i;
    }
  }

  aom_img_free(img);
  ASSERT_EQ(aom_codec_destroy(&dec), AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_destroy(&enc), AOM_CODEC_OK);
}

// Encodes a key frame with screen content tools in realtime mode. A 64x64 patch
// of two color noise at (src_x, src_y) is copied to (dst_x, dst_y), so the
// IntraBC hash search finds a match that far away. A DV of 2048 pixels or more
// can't be coded, so the decoder must still accept the frame.
void IntraBCFarCopyTest(int width, int height, int src_x, int src_y, int dst_x,
                        int dst_y) {
  constexpr int kPatchSize = 64;
  aom_codec_iface_t *iface = aom_codec_av1_cx();
  aom_codec_enc_cfg_t cfg;
  ASSERT_EQ(aom_codec_enc_config_default(iface, &cfg, AOM_USAGE_REALTIME),
            AOM_CODEC_OK);
  cfg.g_w = width;
  cfg.g_h = height;
  cfg.g_lag_in_frames = 0;
  cfg.g_threads = 1;
  cfg.rc_min_quantizer = 10;
  cfg.rc_max_quantizer = 10;

  aom_codec_ctx_t enc;
  ASSERT_EQ(aom_codec_enc_init(&enc, iface, &cfg, 0), AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_control(&enc, AOME_SET_CPUUSED, 11), AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_control(&enc, AV1E_SET_TUNE_CONTENT, AOM_CONTENT_SCREEN),
            AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_control(&enc, AV1E_SET_ENABLE_INTRABC, 1), AOM_CODEC_OK);

  aom_image_t *img = aom_img_alloc(nullptr, AOM_IMG_FMT_I420, width, height, 1);
  ASSERT_NE(img, nullptr);
  memset(img->planes[0], 128, img->stride[0] * height);
  memset(img->planes[1], 128, img->stride[1] * ((height + 1) / 2));
  memset(img->planes[2], 128, img->stride[2] * ((height + 1) / 2));
  ::libaom_test::ACMRandom rnd;
  rnd.Reset(::libaom_test::ACMRandom::DeterministicSeed());
  for (int r = 0; r < kPatchSize; ++r) {
    uint8_t *src = img->planes[0] + (src_y + r) * img->stride[0] + src_x;
    uint8_t *dst = img->planes[0] + (dst_y + r) * img->stride[0] + dst_x;
    for (int c = 0; c < kPatchSize; ++c) {
      src[c] = (rnd.Rand8() & 1) ? 16 : 235;
    }
    memcpy(dst, src, kPatchSize);
  }

  aom_codec_ctx_t dec;
  ASSERT_EQ(aom_codec_dec_init(&dec, aom_codec_av1_dx(), nullptr, 0),
            AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_encode(&enc, img, 0, 1, 0), AOM_CODEC_OK);
  aom_codec_iter_t iter = nullptr;
  while (const aom_codec_cx_pkt_t *pkt = aom_codec_get_cx_data(&enc, &iter)) {
    if (pkt->kind != AOM_CODEC_CX_FRAME_PKT) continue;
    EXPECT_EQ(aom_codec_decode(
                  &dec, static_cast<const uint8_t *>(pkt->data.frame.buf),
                  pkt->data.frame.sz, nullptr),
              AOM_CODEC_OK)
        << aom_codec_error_detail(&dec);
  }

  aom_img_free(img);
  ASSERT_EQ(aom_codec_destroy(&dec), AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_destroy(&enc), AOM_CODEC_OK);
}

// The match is 2048 pixels to the left.
TEST(EncodeAPI, IntraBCFarCopyLeftRealtime) {
  IntraBCFarCopyTest(2112, 64, 0, 0, 2048, 0);
}

// The match is 2048 pixels above.
TEST(EncodeAPI, IntraBCFarCopyAboveRealtime) {
  IntraBCFarCopyTest(64, 2112, 0, 0, 0, 2048);
}

// The match is 2056 pixels to the right and 512 pixels above. The difference
// from the reference DV is past the end of the DV cost table, which ASan
// reports as a heap buffer overflow.
TEST(EncodeAPI, IntraBCFarCopyAboveRightRealtime) {
  IntraBCFarCopyTest(2120, 576, 2056, 0, 0, 512);
}
#endif  // CONFIG_AV1_DECODER

#if !CONFIG_REALTIME_ONLY
// Tests that search_site_cfg[SS_CFG_LOOKAHEAD] in init_motion_estimation() is
// updated using the current frame's unscaled_source->y_stride rather than
// lookahead->buf[0].img.y_stride. In TPL's motion_estimation() (tpl_model.c),
// when the reference buffer is an unscaled lookahead source buffer whose stride
// differs from the reconstructed frame stride (SS_CFG_SRC), it directly selects
// search_site_cfg[SS_CFG_LOOKAHEAD] and asserts that search_site_cfg->stride ==
// ref_stride (without calling av1_get_search_site_config()).
TEST(EncodeAPI, TplSubsamplingChangeGoodQuality) {
  aom_codec_iface_t *iface = aom_codec_av1_cx();
  aom_codec_enc_cfg_t cfg;
  ASSERT_EQ(aom_codec_enc_config_default(iface, &cfg, AOM_USAGE_GOOD_QUALITY),
            AOM_CODEC_OK);
  cfg.g_w = 64;
  cfg.g_h = 64;
  cfg.g_lag_in_frames = 4;
  cfg.g_threads = 1;
  cfg.monochrome = 1;

  aom_codec_ctx_t enc;
  ASSERT_EQ(aom_codec_enc_init(&enc, iface, &cfg, 0), AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_control(&enc, AOME_SET_CPUUSED, 5), AOM_CODEC_OK);
  // Disable temporal filtering so init_gop_frames_for_tpl() uses the lookahead
  // buffers (&buf->img, y_stride = 384) directly as tpl_frame->gf_picture
  // rather than replacing them with tf_buf (y_stride = 256 == SS_CFG_SRC).
  ASSERT_EQ(aom_codec_control(&enc, AOME_SET_ARNR_MAXFRAMES, 0), AOM_CODEC_OK);

  aom_image_t *img_420 = aom_img_alloc(nullptr, AOM_IMG_FMT_I420, 64, 64, 1);
  ASSERT_NE(img_420, nullptr);
  FillImageRandom(img_420);
  aom_image_t *img_444 = aom_img_alloc(nullptr, AOM_IMG_FMT_I444, 64, 64, 1);
  ASSERT_NE(img_444, nullptr);
  FillImageRandom(img_444);

  // Frame 0 (I420) goes into lookahead->buf[0] with border_in_pixels = 96
  // (y_stride = 256), and is then flushed.
  ASSERT_EQ(aom_codec_encode(&enc, img_420, 0, 1, 0), AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_encode(&enc, nullptr, 0, 0, 0), AOM_CODEC_OK);
  {
    aom_codec_iter_t iter = nullptr;
    while (aom_codec_get_cx_data(&enc, &iter) != nullptr) {
    }
  }

  // Frames 1..4 (I444) go into lookahead->buf[1..4]. Because monochrome = 1,
  // changing subsampling from I420 to I444 is valid across frames and triggers
  // new_dimensions in av1_lookahead_push(), reallocating buf[1..4] with
  // AOM_BORDER_IN_PIXELS = 160 (y_stride = 384) while buf[0] keeps
  // y_stride = 256.
  // Because av1_tpl_setup_stats() runs before av1_encode() updates
  // cpi->unscaled_source, frame 1 is encoded first (as a 1-frame GOP) to update
  // cpi->unscaled_source to buf[1] (y_stride = 384). Forcing a key frame on
  // frame 2 starts a new GOP at frame 2 so init_motion_estimation() and TPL run
  // on frames 2..4 (in slots buf[2..4]).
  ASSERT_EQ(aom_codec_encode(&enc, img_444, 1, 1, 0), AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_encode(&enc, img_444, 2, 1, AOM_EFLAG_FORCE_KF),
            AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_encode(&enc, img_444, 3, 1, 0), AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_encode(&enc, img_444, 4, 1, 0), AOM_CODEC_OK);
  // Each flush (nullptr) call drains 1 frame from the lookahead buffer, so 4
  // calls are needed for frames 1..4.
  for (int i = 0; i < 4; ++i) {
    ASSERT_EQ(aom_codec_encode(&enc, nullptr, 0, 0, 0), AOM_CODEC_OK);
    aom_codec_iter_t iter = nullptr;
    while (aom_codec_get_cx_data(&enc, &iter) != nullptr) {
    }
  }

  aom_img_free(img_420);
  aom_img_free(img_444);
  ASSERT_EQ(aom_codec_destroy(&enc), AOM_CODEC_OK);
}

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

// Fills |img| (8-bit 4:2:0) with frame |frame_index| of a video that zooms in
// on a fixed picture of random 3x3 blocks: frame i shows the picture magnified
// by a factor of (32 + i) / 32, with the top-left corner fixed. Global motion
// can model the zoom between frames.
void FillZoomingFrame(aom_image_t *img, int frame_index) {
  // Small blocks give global motion estimation plenty of corners to track.
  constexpr int kBlockSize = 3;
  const int w = static_cast<int>(img->d_w);
  const int h = static_cast<int>(img->d_h);
  const int blocks_w = (w + kBlockSize - 1) / kBlockSize;
  const int blocks_h = (h + kBlockSize - 1) / kBlockSize;
  // One random value per block. ACMRandom uses a fixed seed, so every frame
  // gets the same picture.
  ::libaom_test::ACMRandom rnd;
  std::vector<uint8_t> blocks(blocks_w * blocks_h);
  for (uint8_t &block : blocks) block = rnd.Rand8();

  const int magnification = 32 + frame_index;  // In units of 1/32.
  for (int r = 0; r < h; ++r) {
    uint8_t *const row =
        img->planes[AOM_PLANE_Y] + r * img->stride[AOM_PLANE_Y];
    for (int c = 0; c < w; ++c) {
      // Pixel (c, r) of the frame shows pixel (x, y) of the picture.
      const int x = c * 32 / magnification;
      const int y = r * 32 / magnification;
      row[c] = blocks[(y / kBlockSize) * blocks_w + x / kBlockSize];
    }
  }
  const int uv_w = (w + 1) / 2;
  const int uv_h = (h + 1) / 2;
  for (int r = 0; r < uv_h; ++r) {
    memset(img->planes[AOM_PLANE_U] + r * img->stride[AOM_PLANE_U], 128, uv_w);
    memset(img->planes[AOM_PLANE_V] + r * img->stride[AOM_PLANE_V], 128, uv_w);
  }
}

// Encodes 5 frames of zooming content in good-quality mode with no lag and
// stores the compressed data in |data|. Before frame i is encoded,
// AV1E_SET_ENABLE_GLOBAL_MOTION is set to each value in
// enable_global_motion[i], in order. If |large_scale_tile_on_first_frame| is
// true, large_scale_tile is set to 1 for the first frame and to 0 after it.
void EncodeZoomingFrames(
    const std::vector<std::vector<int>> &enable_global_motion,
    std::vector<uint8_t> *data, bool large_scale_tile_on_first_frame = false) {
  constexpr int kNumFrames = 5;
  aom_codec_iface_t *const iface = aom_codec_av1_cx();
  aom_codec_enc_cfg_t cfg;
  ASSERT_EQ(aom_codec_enc_config_default(iface, &cfg, AOM_USAGE_GOOD_QUALITY),
            AOM_CODEC_OK);
  cfg.g_w = 128;
  cfg.g_h = 128;
  cfg.g_lag_in_frames = 0;
  cfg.large_scale_tile = large_scale_tile_on_first_frame;
  aom_codec_ctx_t enc;
  ASSERT_EQ(aom_codec_enc_init(&enc, iface, &cfg, 0), AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_control(&enc, AOME_SET_CPUUSED, 5), AOM_CODEC_OK);

  aom_image_t *const image =
      aom_img_alloc(nullptr, AOM_IMG_FMT_I420, cfg.g_w, cfg.g_h, 1);
  ASSERT_NE(image, nullptr);
  data->clear();
  for (int i = 0; i < kNumFrames; ++i) {
    if (i == 1 && large_scale_tile_on_first_frame) {
      cfg.large_scale_tile = 0;
      ASSERT_EQ(aom_codec_enc_config_set(&enc, &cfg), AOM_CODEC_OK);
    }
    if (i < static_cast<int>(enable_global_motion.size())) {
      for (const int enable : enable_global_motion[i]) {
        ASSERT_EQ(
            aom_codec_control(&enc, AV1E_SET_ENABLE_GLOBAL_MOTION, enable),
            AOM_CODEC_OK);
      }
    }
    FillZoomingFrame(image, i);
    ASSERT_EQ(aom_codec_encode(&enc, image, i, 1, 0), AOM_CODEC_OK);
    aom_codec_iter_t iter = nullptr;
    const aom_codec_cx_pkt_t *pkt;
    while ((pkt = aom_codec_get_cx_data(&enc, &iter)) != nullptr) {
      ASSERT_EQ(pkt->kind, AOM_CODEC_CX_FRAME_PKT);
      const uint8_t *const buf =
          static_cast<const uint8_t *>(pkt->data.frame.buf);
      data->insert(data->end(), buf, buf + pkt->data.frame.sz);
    }
  }
  aom_img_free(image);
  ASSERT_EQ(aom_codec_destroy(&enc), AOM_CODEC_OK);
}

// Global motion needs an image pyramid in every source and reference frame
// buffer, but pyramids are only allocated while global motion is enabled. So
// global motion can only be enabled before the first frame is passed to
// aom_codec_encode(); later requests to enable it are ignored. Such requests
// used to cause an assertion failure in av1_encode_frame() in debug builds and
// a crash in global motion search in release builds.
//
// The tests below compare the output of encoders that make the same sequence
// of control calls, so that the only difference is the global motion setting.

TEST(EncodeAPI, EnableGlobalMotionBeforeFirstFrame) {
  std::vector<uint8_t> on, off_on, off;
  ASSERT_NO_FATAL_FAILURE(EncodeZoomingFrames({ { 1, 1 } }, &on));
  ASSERT_NO_FATAL_FAILURE(EncodeZoomingFrames({ { 0, 1 } }, &off_on));
  ASSERT_NO_FATAL_FAILURE(EncodeZoomingFrames({ { 0, 0 } }, &off));
  // Global motion makes a difference for this content.
  EXPECT_NE(on, off);
  // Global motion can be enabled (again) before the first frame.
  EXPECT_EQ(off_on, on);
}

TEST(EncodeAPI, EnableGlobalMotionAfterFirstFrame) {
  std::vector<uint8_t> off, off_on;
  ASSERT_NO_FATAL_FAILURE(EncodeZoomingFrames({ { 0 }, { 0 } }, &off));
  ASSERT_NO_FATAL_FAILURE(EncodeZoomingFrames({ { 0 }, { 1 } }, &off_on));
  // The request to enable global motion after the first frame is ignored.
  EXPECT_EQ(off_on, off);
}

TEST(EncodeAPI, DisableGlobalMotionAfterFirstFrame) {
  std::vector<uint8_t> off, on_off;
  ASSERT_NO_FATAL_FAILURE(EncodeZoomingFrames({ { 0 }, { 0 } }, &off));
  ASSERT_NO_FATAL_FAILURE(EncodeZoomingFrames({ { 1 }, { 0 } }, &on_off));
  // Global motion can be disabled after the first frame. The first frame is a
  // key frame, which does not use global motion, so this is equivalent to
  // disabling global motion before the first frame.
  EXPECT_EQ(on_off, off);

  std::vector<uint8_t> on_off_off, on_off_on;
  ASSERT_NO_FATAL_FAILURE(
      EncodeZoomingFrames({ { 1 }, { 0 }, { 0 } }, &on_off_off));
  ASSERT_NO_FATAL_FAILURE(
      EncodeZoomingFrames({ { 1 }, { 0 }, { 1 } }, &on_off_on));
  // Once disabled, global motion cannot be enabled again.
  EXPECT_EQ(on_off_on, on_off_off);
}

// large_scale_tile = 1 forces global motion off, so setting large_scale_tile to
// 0 after the first frame implicitly requests global motion to be enabled. That
// request is ignored, too.
TEST(EncodeAPI, DisableLargeScaleTileAfterFirstFrame) {
  std::vector<uint8_t> on, off;
  ASSERT_NO_FATAL_FAILURE(EncodeZoomingFrames(
      { { 1 } }, &on, /*large_scale_tile_on_first_frame=*/true));
  ASSERT_NO_FATAL_FAILURE(EncodeZoomingFrames(
      { { 0 } }, &off, /*large_scale_tile_on_first_frame=*/true));
  EXPECT_EQ(on, off);
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

#if !CONFIG_REALTIME_ONLY
TEST(EncodeAPI, Buganizer558434716) {
  aom_codec_iface_t *iface = aom_codec_av1_cx();
  aom_codec_enc_cfg_t cfg;
  ASSERT_EQ(aom_codec_enc_config_default(iface, &cfg, AOM_USAGE_GOOD_QUALITY),
            AOM_CODEC_OK);

  cfg.g_w = 128;
  cfg.g_h = 96;
  cfg.g_forced_max_frame_width = 1920;
  cfg.g_forced_max_frame_height = 1080;
  cfg.g_pass = AOM_RC_ONE_PASS;
  cfg.g_lag_in_frames = 0;
  cfg.g_threads = 1;

  aom_codec_ctx_t enc;
  ASSERT_EQ(aom_codec_enc_init(&enc, iface, &cfg, 0), AOM_CODEC_OK);

  aom_image_t *img_small = aom_img_alloc(nullptr, AOM_IMG_FMT_I420, 128, 96, 1);
  ASSERT_NE(img_small, nullptr);
  FillImage(img_small, 128);

  aom_image_t *img_large =
      aom_img_alloc(nullptr, AOM_IMG_FMT_I420, 320, 240, 1);
  ASSERT_NE(img_large, nullptr);
  FillImage(img_large, 128);

  // Frame 0 at 128x96
  ASSERT_EQ(aom_codec_encode(&enc, img_small, 0, 1, 0), AOM_CODEC_OK);

  // Switch to larger resolution
  cfg.g_w = 320;
  cfg.g_h = 240;
  ASSERT_EQ(aom_codec_enc_config_set(&enc, &cfg), AOM_CODEC_OK);

  // Frame 1 at 320x240
  ASSERT_EQ(aom_codec_encode(&enc, img_large, 1, 1, AOM_EFLAG_FORCE_KF),
            AOM_CODEC_OK);

  // Switch back to smaller resolution
  cfg.g_w = 128;
  cfg.g_h = 96;
  ASSERT_EQ(aom_codec_enc_config_set(&enc, &cfg), AOM_CODEC_OK);

  // Frame 2 at 128x96 (keyframe)
  ASSERT_EQ(aom_codec_encode(&enc, img_small, 2, 1, AOM_EFLAG_FORCE_KF),
            AOM_CODEC_OK);

  // Frames 3+ at 128x96 (interframes referencing frame 2)
  for (int i = 3; i < 8; ++i) {
    ASSERT_EQ(aom_codec_encode(&enc, img_small, i, 1, 0), AOM_CODEC_OK);
  }

  aom_img_free(img_small);
  aom_img_free(img_large);
  ASSERT_EQ(aom_codec_destroy(&enc), AOM_CODEC_OK);
}

TEST(EncodeAPI, PerceptualAIDynamicResolutionChange) {
  aom_codec_iface_t *iface = aom_codec_av1_cx();
  aom_codec_enc_cfg_t cfg;
  ASSERT_EQ(aom_codec_enc_config_default(iface, &cfg, AOM_USAGE_ALL_INTRA),
            AOM_CODEC_OK);

  cfg.g_w = 17;
  cfg.g_h = 1;
  cfg.g_forced_max_frame_width = 320;
  cfg.g_forced_max_frame_height = 240;
  cfg.g_pass = AOM_RC_ONE_PASS;
  cfg.g_lag_in_frames = 0;
  cfg.g_threads = 1;

  aom_codec_ctx_t enc;
  ASSERT_EQ(aom_codec_enc_init(&enc, iface, &cfg, 0), AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_control(&enc, AV1E_SET_DELTAQ_MODE, 3), AOM_CODEC_OK);

  aom_image_t *img_small = aom_img_alloc(nullptr, AOM_IMG_FMT_I420, 17, 1, 1);
  ASSERT_NE(img_small, nullptr);
  FillImage(img_small, 128);

  aom_image_t *img_large =
      aom_img_alloc(nullptr, AOM_IMG_FMT_I420, 320, 240, 1);
  ASSERT_NE(img_large, nullptr);
  FillImage(img_large, 128);

  // Frame 0 at 17x1
  ASSERT_EQ(aom_codec_encode(&enc, img_small, 0, 1, 0), AOM_CODEC_OK);

  // Switch to larger resolution
  cfg.g_w = 320;
  cfg.g_h = 240;
  ASSERT_EQ(aom_codec_enc_config_set(&enc, &cfg), AOM_CODEC_OK);

  // Frame 1 at 320x240
  ASSERT_EQ(aom_codec_encode(&enc, img_large, 1, 1, 0), AOM_CODEC_OK);

  // Switch back to 17x1
  cfg.g_w = 17;
  cfg.g_h = 1;
  ASSERT_EQ(aom_codec_enc_config_set(&enc, &cfg), AOM_CODEC_OK);

  // Frame 2 at 17x1
  ASSERT_EQ(aom_codec_encode(&enc, img_small, 2, 1, 0), AOM_CODEC_OK);

  aom_img_free(img_small);
  aom_img_free(img_large);
  ASSERT_EQ(aom_codec_destroy(&enc), AOM_CODEC_OK);
}

// Test for OSS-Fuzz Issue 559079132: Integer-overflow in
// av1_caq_select_segment. When COMPLEXITY_AQ (aq_mode 2) is enabled with a high
// target bitrate, sb64_target_rate is large, causing signed integer overflow
// when multiplying sb64_target_rate * xmis * ymis in 32-bit arithmetic and when
// casting target_rate to int.
TEST(EncodeAPI, Issue559079132) {
  aom_codec_iface_t *const iface = aom_codec_av1_cx();
  aom_codec_ctx_t enc;
  aom_codec_enc_cfg_t cfg;

  ASSERT_EQ(aom_codec_enc_config_default(iface, &cfg, AOM_USAGE_GOOD_QUALITY),
            AOM_CODEC_OK);

  cfg.g_w = 64;
  cfg.g_h = 64;
  cfg.g_timebase.num = 1;
  cfg.g_timebase.den = 1;
  cfg.rc_target_bitrate = 2000000;
  cfg.rc_end_usage = AOM_CBR;
  cfg.g_lag_in_frames = 0;

  ASSERT_EQ(aom_codec_enc_init(&enc, iface, &cfg, 0), AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_control(&enc, AV1E_SET_AQ_MODE, 2), AOM_CODEC_OK);

  aom_image_t *img = aom_img_alloc(nullptr, AOM_IMG_FMT_I420, 64, 64, 1);
  ASSERT_NE(img, nullptr);
  FillImageRandom(img);

  EncodeOne(&enc, img, 0);

  // Flush encoder.
  ASSERT_EQ(aom_codec_encode(&enc, nullptr, 0, 0, 0), AOM_CODEC_OK);
  aom_codec_iter_t iter = nullptr;
  while (aom_codec_get_cx_data(&enc, &iter) != nullptr) {
  }

  aom_img_free(img);
  ASSERT_EQ(aom_codec_destroy(&enc), AOM_CODEC_OK);
}
#endif  // !CONFIG_REALTIME_ONLY

// Tests for OSS-Fuzz Issues 558463888, 559075253, 559225640:
// When dynamically reducing threads with multiple tiles and row_mt=0, the
// primary worker pool size (mt_info->num_workers) remains at the higher
// capacity allocated previously while the module workers for the encode stage
// (mt_info->num_mod_workers[MOD_ENC]) is reduced. If encode_frame_internal()
// uses mt_info->num_workers instead of mt_info->num_mod_workers[MOD_ENC], it
// can erroneously call av1_encode_tiles_mt() or stride with the wrong worker
// count, causing tiles to be skipped and leading to a crash in bitstream
// packing.
void TestDynamicThreadReductionWithTiles(int tile_rows, int tile_columns,
                                         int init_threads, int max_threads) {
  aom_codec_iface_t *const iface = aom_codec_av1_cx();
  aom_codec_ctx_t enc;
  aom_codec_enc_cfg_t cfg;

  ASSERT_EQ(aom_codec_enc_config_default(iface, &cfg, AOM_USAGE_REALTIME),
            AOM_CODEC_OK);

  cfg.g_w = 256;
  cfg.g_h = 256;
  // Allow dynamic resolution changes up to 512x512.
  cfg.g_forced_max_frame_width = 512;
  cfg.g_forced_max_frame_height = 512;
  cfg.g_threads = init_threads;
  cfg.g_lag_in_frames = 0;
  cfg.g_error_resilient = 1;

  ASSERT_EQ(aom_codec_enc_init(&enc, iface, &cfg, 0), AOM_CODEC_OK);

  // Disable row-mt and configure tiling.
  ASSERT_EQ(aom_codec_control(&enc, AV1E_SET_ROW_MT, 0), AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_control(&enc, AOME_SET_CPUUSED, 7), AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_control(&enc, AV1E_SET_TILE_ROWS, tile_rows),
            AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_control(&enc, AV1E_SET_TILE_COLUMNS, tile_columns),
            AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_control(&enc, AOME_SET_ENABLEAUTOALTREF, 0),
            AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_control(&enc, AV1E_SET_AQ_MODE, 3), AOM_CODEC_OK);

  aom_image_t *img_256 = aom_img_alloc(nullptr, AOM_IMG_FMT_I420, 256, 256, 1);
  ASSERT_NE(img_256, nullptr);
  FillImageRandom(img_256);

  aom_image_t *img_512 = aom_img_alloc(nullptr, AOM_IMG_FMT_I420, 512, 512, 1);
  ASSERT_NE(img_512, nullptr);
  FillImageRandom(img_512);

  // Frame 0: Encode at 256x256 with init_threads.
  EncodeOne(&enc, img_256, 0);

  // Frame 1: Switch to 512x512 with max_threads. This allocates max_threads
  // workers in the primary worker pool (mt_info->num_workers = max_threads).
  cfg.g_w = 512;
  cfg.g_h = 512;
  cfg.g_threads = max_threads;
  ASSERT_EQ(aom_codec_enc_config_set(&enc, &cfg), AOM_CODEC_OK);
  EncodeOne(&enc, img_512, 1);

  // Frame 2: Switch back to 256x256 with init_threads. The worker pool retains
  // mt_info->num_workers = max_threads, but mt_info->num_mod_workers[MOD_ENC]
  // is init_threads. This verifies that tile encoding properly handles the
  // reduced worker count without skipping tiles.
  cfg.g_w = 256;
  cfg.g_h = 256;
  cfg.g_threads = init_threads;
  ASSERT_EQ(aom_codec_enc_config_set(&enc, &cfg), AOM_CODEC_OK);
  EncodeOne(&enc, img_256, 2);

  // Flush encoder.
  ASSERT_EQ(aom_codec_encode(&enc, nullptr, 0, 0, 0), AOM_CODEC_OK);
  aom_codec_iter_t iter = nullptr;
  while (aom_codec_get_cx_data(&enc, &iter) != nullptr) {
  }

  aom_img_free(img_256);
  aom_img_free(img_512);
  ASSERT_EQ(aom_codec_destroy(&enc), AOM_CODEC_OK);
}

TEST(EncodeAPI, Issue558463888) {
  TestDynamicThreadReductionWithTiles(/*tile_rows=*/1, /*tile_columns=*/0,
                                      /*init_threads=*/1, /*max_threads=*/2);
}

TEST(EncodeAPI, Issue559075253) {
  TestDynamicThreadReductionWithTiles(/*tile_rows=*/1, /*tile_columns=*/1,
                                      /*init_threads=*/2, /*max_threads=*/4);
}

TEST(EncodeAPI, Issue559225640) {
  TestDynamicThreadReductionWithTiles(/*tile_rows=*/0, /*tile_columns=*/1,
                                      /*init_threads=*/1, /*max_threads=*/4);
}

#if !CONFIG_REALTIME_ONLY
TEST(EncodeAPI, Buganizer558446054) {
  aom_codec_iface_t *iface = aom_codec_av1_cx();
  aom_codec_enc_cfg_t cfg;
  ASSERT_EQ(aom_codec_enc_config_default(iface, &cfg, AOM_USAGE_ALL_INTRA),
            AOM_CODEC_OK);

  cfg.g_w = 65;
  cfg.g_h = 33;
  cfg.g_threads = 0;
  cfg.g_lag_in_frames = 0;
  cfg.g_pass = AOM_RC_ONE_PASS;
  cfg.rc_end_usage = AOM_Q;

  aom_codec_ctx_t codec;
  ASSERT_EQ(aom_codec_enc_init(&codec, iface, &cfg, 0), AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_control(&codec, AOME_SET_CPUUSED, 1), AOM_CODEC_OK);

  aom_image_t raw;
  ASSERT_NE(aom_img_alloc(&raw, AOM_IMG_FMT_I420, 65, 33, 1), nullptr);
  FillImageRandom(&raw);

  ASSERT_EQ(aom_codec_encode(&codec, &raw, 0, 1, 0), AOM_CODEC_OK);

  aom_img_free(&raw);
  ASSERT_EQ(aom_codec_destroy(&codec), AOM_CODEC_OK);
}

TEST(EncodeAPI, Buganizer558463892_558589747) {
  aom_codec_iface_t *iface = aom_codec_av1_cx();
  aom_codec_enc_cfg_t cfg;
  ASSERT_EQ(aom_codec_enc_config_default(iface, &cfg, AOM_USAGE_ALL_INTRA),
            AOM_CODEC_OK);

  cfg.g_w = 64;
  cfg.g_h = 64;
  cfg.g_forced_max_frame_width = 1024;
  cfg.g_forced_max_frame_height = 1024;
  cfg.g_threads = 0;
  cfg.g_lag_in_frames = 0;
  cfg.g_pass = AOM_RC_ONE_PASS;
  cfg.rc_end_usage = AOM_Q;

  aom_codec_ctx_t codec;
  ASSERT_EQ(aom_codec_enc_init(&codec, iface, &cfg, 0), AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_control(&codec, AV1E_SET_DELTAQ_MODE, 3), AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_control(&codec, AOME_SET_CPUUSED, 0), AOM_CODEC_OK);

  // Encode frame 0 at 64x64
  aom_image_t raw;
  ASSERT_NE(aom_img_alloc(&raw, AOM_IMG_FMT_I420, 64, 64, 1), nullptr);
  FillImageRandom(&raw);
  ASSERT_EQ(aom_codec_encode(&codec, &raw, 0, 1, 0), AOM_CODEC_OK);
  aom_img_free(&raw);

  // Change resolution to 256x256 mid-stream
  cfg.g_w = 256;
  cfg.g_h = 256;
  ASSERT_EQ(aom_codec_enc_config_set(&codec, &cfg), AOM_CODEC_OK);

  // Encode frame 1 at 256x256
  ASSERT_NE(aom_img_alloc(&raw, AOM_IMG_FMT_I420, 256, 256, 1), nullptr);
  FillImageRandom(&raw);
  ASSERT_EQ(aom_codec_encode(&codec, &raw, 1, 1, 0), AOM_CODEC_OK);
  aom_img_free(&raw);

  ASSERT_EQ(aom_codec_destroy(&codec), AOM_CODEC_OK);
}

// Regression test for b/565488030: Heap-buffer-overflow in
// save_deblock_boundary_lines when Frame 0 is coded losslessly (q = 0, setting
// cm->features.all_lossless = 1) or with large_scale_tile = 1 at a small
// resolution and Frame 1 increases the resolution via
// aom_codec_enc_config_set() with large_scale_tile = 0 and lossy coding (q > 0,
// cm->features.all_lossless = 0) with loop restoration enabled.
TEST(EncodeAPI, Buganizer565488030) {
  for (unsigned int threads : { 1u, 4u }) {
    for (unsigned int large_scale_tile : { 0u, 1u }) {
      aom_codec_iface_t *const iface = aom_codec_av1_cx();
      aom_codec_enc_cfg_t cfg;
      ASSERT_EQ(
          aom_codec_enc_config_default(iface, &cfg, AOM_USAGE_GOOD_QUALITY),
          AOM_CODEC_OK);

      cfg.g_w = 4;
      cfg.g_h = 4;
      cfg.g_forced_max_frame_width = 256;
      cfg.g_forced_max_frame_height = 256;
      cfg.g_threads = threads;
      cfg.g_lag_in_frames = 0;
      cfg.large_scale_tile = large_scale_tile;
      cfg.rc_end_usage = AOM_CBR;
      cfg.rc_target_bitrate = 3999;
      cfg.rc_min_quantizer = 0;
      cfg.rc_max_quantizer = 63;

      aom_codec_ctx_t enc;
      ASSERT_EQ(aom_codec_enc_init(&enc, iface, &cfg, 0), AOM_CODEC_OK);
      ASSERT_EQ(aom_codec_control(&enc, AOME_SET_CPUUSED, 4), AOM_CODEC_OK);
      // large_scale_tile = 1 implicitly disables global motion, so switching
      // it to 0 below would turn global motion on mid-stream. The reference
      // frames coded before that have no image pyramid, which trips
      // assert(buf->buf.y_pyramid) in av1_encode_frame(). That is a separate
      // issue, so keep global motion off to focus on loop restoration.
      ASSERT_EQ(aom_codec_control(&enc, AV1E_SET_ENABLE_GLOBAL_MOTION, 0),
                AOM_CODEC_OK);

      aom_image_t *img_small =
          aom_img_alloc(nullptr, AOM_IMG_FMT_I420, 4, 4, 16);
      ASSERT_NE(img_small, nullptr);
      FillImage(img_small, 128);

      // Frame 0: 4x4 at high CBR bitrate picks q = 0 (all_lossless = 1).
      EncodeOne(&enc, img_small, 0);
      aom_img_free(img_small);

      // Frame 1: Reconfigure to 256x256 with large_scale_tile = 0 (picks q > 0,
      // all_lossless = 0).
      cfg.g_w = 256;
      cfg.g_h = 256;
      cfg.large_scale_tile = 0;
      ASSERT_EQ(aom_codec_enc_config_set(&enc, &cfg), AOM_CODEC_OK);

      aom_image_t *img_large =
          aom_img_alloc(nullptr, AOM_IMG_FMT_I420, 256, 256, 16);
      ASSERT_NE(img_large, nullptr);
      FillImage(img_large, 128);

      EncodeOne(&enc, img_large, 1);
      aom_img_free(img_large);

      // Flush encoder.
      ASSERT_EQ(aom_codec_encode(&enc, nullptr, 0, 0, 0), AOM_CODEC_OK);
      aom_codec_iter_t iter = nullptr;
      while (aom_codec_get_cx_data(&enc, &iter) != nullptr) {
      }

      ASSERT_EQ(aom_codec_destroy(&enc), AOM_CODEC_OK);
    }
  }
}
#endif  // !CONFIG_REALTIME_ONLY

// Regression test for b/568565869: Undefined left-shift overflow of
// this_rdc->rate in av1_block_yrd() when encoding a large partition in
// realtime lossless mode.
TEST(EncodeAPI, NonrdBlockYrdRateOverflow) {
  aom_codec_iface_t *const iface = aom_codec_av1_cx();
  aom_codec_enc_cfg_t cfg;
  ASSERT_EQ(aom_codec_enc_config_default(iface, &cfg, AOM_USAGE_REALTIME),
            AOM_CODEC_OK);

  cfg.g_w = 128;
  cfg.g_h = 128;
  cfg.g_lag_in_frames = 0;

  aom_codec_ctx_t enc;
  ASSERT_EQ(aom_codec_enc_init(&enc, iface, &cfg, 0), AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_control(&enc, AOME_SET_CPUUSED, 10), AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_control(&enc, AV1E_SET_AQ_MODE, 0), AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_control(&enc, AV1E_SET_LOSSLESS, 1), AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_control(&enc, AV1E_SET_SUPERBLOCK_SIZE,
                              AOM_SUPERBLOCK_SIZE_128X128),
            AOM_CODEC_OK);

  aom_image_t *img = aom_img_alloc(nullptr, AOM_IMG_FMT_I420, 128, 128, 16);
  ASSERT_NE(img, nullptr);
  FillImage(img, 0);
  EncodeOne(&enc, img, 0);

  // Use a 253/255 checkerboard in Frame 1 against all-zero Frame 0:
  // - The residual variance around the mean (254) is tiny (1), so
  //   variance-based partitioning does not split and evaluates a large
  //   partition.
  // - The large DC residual (~254) combined with non-zero AC (ncoeffs > 1)
  //   causes aom_satd_lp(low_qcoeff) in av1_block_yrd() to accumulate a rate
  //   above INT_MAX >> 11, which would overflow 32-bit int when shifted left
  //   by 2 + AV1_PROB_COST_SHIFT (11).
  for (int r = 0; r < 128; ++r) {
    for (int c = 0; c < 128; ++c) {
      img->planes[AOM_PLANE_Y][r * img->stride[AOM_PLANE_Y] + c] =
          ((r ^ c) & 1) ? 255 : 253;
    }
  }
  EncodeOne(&enc, img, 1);

  aom_img_free(img);
  ASSERT_EQ(aom_codec_destroy(&enc), AOM_CODEC_OK);
}

// Regression test for webrtc:496266459: Enabling compound prediction via
// AV1E_SET_SVC_REF_FRAME_COMP_PRED must not affect encoding of frames that do
// not have any active compound reference pair (e.g., single-reference delta
// frames referencing only LAST_FRAME).
TEST(EncodeAPI, SvcCompPredDoesNotAffectSingleRefFrames) {
  aom_codec_iface_t *const iface = aom_codec_av1_cx();
  aom_codec_enc_cfg_t cfg;
  ASSERT_EQ(aom_codec_enc_config_default(iface, &cfg, AOM_USAGE_REALTIME),
            AOM_CODEC_OK);
  cfg.g_w = 320;
  cfg.g_h = 240;
  cfg.g_lag_in_frames = 0;
  cfg.rc_end_usage = AOM_Q;

  aom_codec_ctx_t enc_no_comp;
  aom_codec_ctx_t enc_with_comp;
  ASSERT_EQ(aom_codec_enc_init(&enc_no_comp, iface, &cfg, 0), AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_enc_init(&enc_with_comp, iface, &cfg, 0), AOM_CODEC_OK);

  for (aom_codec_ctx_t *enc : { &enc_no_comp, &enc_with_comp }) {
    ASSERT_EQ(aom_codec_control(enc, AOME_SET_CPUUSED, 9), AOM_CODEC_OK);
    ASSERT_EQ(aom_codec_control(enc, AOME_SET_CQ_LEVEL, 40), AOM_CODEC_OK);
  }

  // Enable compound prediction for all three compound reference pairs
  // (GOLDEN_LAST, LAST2_LAST, ALTREF_LAST) on the second encoder.
  aom_svc_ref_frame_comp_pred_t comp_pred = {};
  comp_pred.use_comp_pred[0] = 1;
  comp_pred.use_comp_pred[1] = 1;
  comp_pred.use_comp_pred[2] = 1;
  ASSERT_EQ(aom_codec_control(&enc_with_comp, AV1E_SET_SVC_REF_FRAME_COMP_PRED,
                              &comp_pred),
            AOM_CODEC_OK);

  aom_image_t *img =
      aom_img_alloc(nullptr, AOM_IMG_FMT_I420, cfg.g_w, cfg.g_h, 1);
  ASSERT_NE(img, nullptr);

  constexpr int kNumFrames = 10;
  for (int frame = 0; frame < kNumFrames; ++frame) {
    SCOPED_TRACE(frame);
    // Fill moving textured pattern.
    for (unsigned int r = 0; r < img->d_h; ++r) {
      for (unsigned int c = 0; c < img->d_w; ++c) {
        const int texture = (((r + frame) / 8 + (c + 2 * frame) / 8) & 1) * 32;
        img->planes[AOM_PLANE_Y][r * img->stride[AOM_PLANE_Y] + c] =
            static_cast<uint8_t>((r + 2 * c + 3 * frame + texture) & 0xff);
      }
    }
    for (int plane = AOM_PLANE_U; plane <= AOM_PLANE_V; ++plane) {
      const unsigned int h =
          (img->d_h + img->y_chroma_shift) >> img->y_chroma_shift;
      for (unsigned int r = 0; r < h; ++r) {
        memset(img->planes[plane] + r * img->stride[plane], 128,
               (img->d_w + img->x_chroma_shift) >> img->x_chroma_shift);
      }
    }

    // Frame 0 refreshes slot 0; delta frames reference only LAST_FRAME (slot 0)
    // with all other reference[i] = 0, and refresh slot 0.
    aom_svc_ref_frame_config_t ref_cfg = {};
    ref_cfg.ref_idx[0] = 0;  // LAST_FRAME -> slot 0.
    ref_cfg.reference[0] = frame > 0;
    ref_cfg.refresh[0] = 1;

    for (aom_codec_ctx_t *enc : { &enc_no_comp, &enc_with_comp }) {
      ASSERT_EQ(aom_codec_control(enc, AV1E_SET_SVC_REF_FRAME_CONFIG, &ref_cfg),
                AOM_CODEC_OK);
      ASSERT_EQ(aom_codec_encode(enc, img, frame, 1, 0), AOM_CODEC_OK);
    }

    aom_codec_iter_t iter_no_comp = nullptr;
    aom_codec_iter_t iter_with_comp = nullptr;
    const aom_codec_cx_pkt_t *pkt_no_comp =
        aom_codec_get_cx_data(&enc_no_comp, &iter_no_comp);
    const aom_codec_cx_pkt_t *pkt_with_comp =
        aom_codec_get_cx_data(&enc_with_comp, &iter_with_comp);
    ASSERT_NE(pkt_no_comp, nullptr);
    ASSERT_NE(pkt_with_comp, nullptr);
    ASSERT_EQ(pkt_no_comp->kind, AOM_CODEC_CX_FRAME_PKT);
    ASSERT_EQ(pkt_with_comp->kind, AOM_CODEC_CX_FRAME_PKT);
    // If use_comp_ref_nonrd is left enabled when no compound reference pair is
    // active, single-reference frames switch reference mode
    // (REFERENCE_MODE_SELECT) and interpolation filter (SWITCHABLE) signaling
    // and mode search, altering both packet sizes and bitstream bytes. Verify
    // that the output frames remain bit-exact (same size and identical bytes)
    // as when compound prediction is disabled.
    EXPECT_EQ(pkt_with_comp->data.frame.sz, pkt_no_comp->data.frame.sz);
    if (pkt_with_comp->data.frame.sz == pkt_no_comp->data.frame.sz) {
      EXPECT_EQ(memcmp(pkt_with_comp->data.frame.buf,
                       pkt_no_comp->data.frame.buf, pkt_no_comp->data.frame.sz),
                0);
    }
  }

  aom_img_free(img);
  ASSERT_EQ(aom_codec_destroy(&enc_with_comp), AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_destroy(&enc_no_comp), AOM_CODEC_OK);
}

// Regression test for b/567944558: Out-of-bounds read in setup_planes when
// encoding with manual reference buffer control (AV1E_SET_SVC_REF_FRAME_CONFIG)
// after a resolution change, when LAST_FRAME is not referenced and GOLDEN_FRAME
// and ALTREF_FRAME have equal SADs.
TEST(EncodeAPI, Buganizer567944558) {
  for (int cpu_used : { 7, 9 }) {
    aom_codec_iface_t *const iface = aom_codec_av1_cx();
    aom_codec_enc_cfg_t cfg;
    ASSERT_EQ(aom_codec_enc_config_default(iface, &cfg, AOM_USAGE_REALTIME),
              AOM_CODEC_OK);

    cfg.g_w = 320;
    cfg.g_h = 240;
    cfg.g_lag_in_frames = 0;
    cfg.rc_end_usage = AOM_VBR;
    cfg.rc_target_bitrate = 500;

    aom_codec_ctx_t enc;
    ASSERT_EQ(aom_codec_enc_init(&enc, iface, &cfg, 0), AOM_CODEC_OK);
    ASSERT_EQ(aom_codec_control(&enc, AOME_SET_CPUUSED, cpu_used),
              AOM_CODEC_OK);
    ASSERT_EQ(aom_codec_control(&enc, AV1E_SET_ROW_MT, 1), AOM_CODEC_OK);

    aom_svc_params_t svc_params = {};
    svc_params.number_spatial_layers = 1;
    svc_params.number_temporal_layers = 1;
    svc_params.scaling_factor_num[0] = 1;
    svc_params.scaling_factor_den[0] = 1;
    svc_params.layer_target_bitrate[0] = cfg.rc_target_bitrate;
    svc_params.max_quantizers[0] = 56;
    svc_params.min_quantizers[0] = 10;
    svc_params.framerate_factor[0] = 1;
    ASSERT_EQ(aom_codec_control(&enc, AV1E_SET_SVC_PARAMS, &svc_params),
              AOM_CODEC_OK);

    auto set_ref_config = [&](bool ref_last, bool ref_golden, bool ref_altref,
                              int update_slot) {
      aom_svc_ref_frame_config_t ref_frame_config = {};
      ref_frame_config.ref_idx[0] = 0;  // LAST_FRAME -> slot 0
      ref_frame_config.ref_idx[3] = 1;  // GOLDEN_FRAME -> slot 1
      ref_frame_config.ref_idx[6] = 2;  // ALTREF_FRAME -> slot 2
      if (update_slot >= 0) {
        ref_frame_config.refresh[update_slot] = 1;
      }
      ref_frame_config.reference[0] = ref_last ? 1 : 0;
      ref_frame_config.reference[3] = ref_golden ? 1 : 0;
      ref_frame_config.reference[6] = ref_altref ? 1 : 0;
      ASSERT_EQ(aom_codec_control(&enc, AV1E_SET_SVC_REF_FRAME_CONFIG,
                                  &ref_frame_config),
                AOM_CODEC_OK);
    };

    aom_image_t *img_large =
        aom_img_alloc(nullptr, AOM_IMG_FMT_I420, 320, 240, 1);
    ASSERT_NE(img_large, nullptr);
    FillImageRandom(img_large);

    // Frame 0: Keyframe at 320x240.
    set_ref_config(/*ref_last=*/false, /*ref_golden=*/false,
                   /*ref_altref=*/false, /*update_slot=*/0);
    ASSERT_EQ(aom_codec_encode(&enc, img_large, 0, 1, AOM_EFLAG_FORCE_KF),
              AOM_CODEC_OK);

    // Frame 1: Delta frame at 320x240 referencing LAST and updating slot 0.
    FillImageRandom(img_large);
    set_ref_config(/*ref_last=*/true, /*ref_golden=*/false,
                   /*ref_altref=*/false, /*update_slot=*/0);
    ASSERT_EQ(aom_codec_encode(&enc, img_large, 1, 1, 0), AOM_CODEC_OK);

    // Reconfigure down to 42x19.
    cfg.g_w = 42;
    cfg.g_h = 19;
    ASSERT_EQ(aom_codec_enc_config_set(&enc, &cfg), AOM_CODEC_OK);
    ASSERT_EQ(aom_codec_control(&enc, AV1E_SET_SVC_PARAMS, &svc_params),
              AOM_CODEC_OK);

    aom_image_t *img_small =
        aom_img_alloc(nullptr, AOM_IMG_FMT_I420, 42, 19, 1);
    ASSERT_NE(img_small, nullptr);
    FillImageRandom(img_small);

    // Frame 2: Keyframe at 42x19 (refreshes all slots with the same frame,
    // leaving INVALID_MV in mi_alloc).
    set_ref_config(/*ref_last=*/false, /*ref_golden=*/false,
                   /*ref_altref=*/false, /*update_slot=*/0);
    ASSERT_EQ(aom_codec_encode(&enc, img_small, 2, 1, AOM_EFLAG_FORCE_KF),
              AOM_CODEC_OK);

    // Frame 3: Delta frame at 42x19 referencing GOLDEN and ALTREF (without
    // LAST). GOLDEN and ALTREF hold the same frame so their SADs tie.
    FillImageRandom(img_small);
    set_ref_config(/*ref_last=*/false, /*ref_golden=*/true, /*ref_altref=*/true,
                   /*update_slot=*/0);
    ASSERT_EQ(aom_codec_encode(&enc, img_small, 3, 1, 0), AOM_CODEC_OK);

    aom_img_free(img_small);
    aom_img_free(img_large);
    ASSERT_EQ(aom_codec_destroy(&enc), AOM_CODEC_OK);
  }
}

// Fills `img` with deterministic content that changes from frame to frame, so
// that inter prediction and rate control have something to work with.
static void FillMovingPattern(aom_image_t *img, int frame) {
  for (unsigned int r = 0; r < img->d_h; ++r) {
    for (unsigned int c = 0; c < img->d_w; ++c) {
      const int texture = (((r + frame) / 8 + (c + 2 * frame) / 8) & 1) * 32;
      img->planes[AOM_PLANE_Y][r * img->stride[AOM_PLANE_Y] + c] =
          static_cast<uint8_t>((r + 2 * c + 3 * frame + texture) & 0xff);
    }
  }
  for (int plane = AOM_PLANE_U; plane <= AOM_PLANE_V; ++plane) {
    const unsigned int h =
        (img->d_h + img->y_chroma_shift) >> img->y_chroma_shift;
    for (unsigned int r = 0; r < h; ++r) {
      memset(img->planes[plane] + r * img->stride[plane], 128,
             (img->d_w + img->x_chroma_shift) >> img->x_chroma_shift);
    }
  }
}

#if CONFIG_AV1_DECODER
// AV1E_SET_SVC_REF_FRAME_CONFIG: the refresh[] flags are indexed by buffer
// slot, and a frame may refresh a slot that none of the seven ref_idx[] entries
// point to. A typical example is a "ping-pong" buffer structure, where each
// frame predicts from one slot (the only reference in use) and stores its
// reconstruction in another slot, which it does not reference.
//
// Such refreshes used to be dropped: the slot was left out of
// refresh_frame_flags, so it silently kept its old content. This test checks,
// on the decoder side, which slots each frame refreshes.
TEST(EncodeAPI, SvcRefFrameConfigRefreshesSlotNotInRefIdx) {
  aom_codec_iface_t *const iface = aom_codec_av1_cx();
  aom_codec_enc_cfg_t cfg;
  ASSERT_EQ(aom_codec_enc_config_default(iface, &cfg, AOM_USAGE_REALTIME),
            AOM_CODEC_OK);
  cfg.g_w = 64;
  cfg.g_h = 64;
  cfg.g_lag_in_frames = 0;
  aom_codec_ctx_t enc;
  ASSERT_EQ(aom_codec_enc_init(&enc, iface, &cfg, 0), AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_control(&enc, AOME_SET_CPUUSED, 9), AOM_CODEC_OK);
  aom_codec_ctx_t dec;
  ASSERT_EQ(aom_codec_dec_init(&dec, aom_codec_av1_dx(), nullptr, 0),
            AOM_CODEC_OK);
  aom_image_t *img =
      aom_img_alloc(nullptr, AOM_IMG_FMT_I420, cfg.g_w, cfg.g_h, 1);
  ASSERT_NE(img, nullptr);

  // Frame 0 is a key frame, which refreshes all slots. Frame n > 0 predicts
  // from slot n - 1 and refreshes slot n. All seven ref_idx[] entries point
  // to the predicted slot, so the refreshed slot is not among them.
  for (int frame = 0; frame < 4; ++frame) {
    SCOPED_TRACE(frame);
    aom_svc_ref_frame_config_t ref_cfg = {};
    if (frame > 0) {
      for (int i = 0; i < 7; ++i) ref_cfg.ref_idx[i] = frame - 1;
      ref_cfg.reference[0] = 1;  // LAST.
    }
    ref_cfg.refresh[frame] = 1;
    ASSERT_EQ(aom_codec_control(&enc, AV1E_SET_SVC_REF_FRAME_CONFIG, &ref_cfg),
              AOM_CODEC_OK);
    FillMovingPattern(img, frame);
    ASSERT_EQ(aom_codec_encode(&enc, img, frame, 1, 0), AOM_CODEC_OK);
    aom_codec_iter_t iter = nullptr;
    const aom_codec_cx_pkt_t *pkt = aom_codec_get_cx_data(&enc, &iter);
    ASSERT_NE(pkt, nullptr);
    ASSERT_EQ(pkt->kind, AOM_CODEC_CX_FRAME_PKT);
    ASSERT_EQ(aom_codec_decode(
                  &dec, static_cast<const uint8_t *>(pkt->data.frame.buf),
                  pkt->data.frame.sz, nullptr),
              AOM_CODEC_OK);
    int ref_updates = 0;
    ASSERT_EQ(aom_codec_control(&dec, AOMD_GET_LAST_REF_UPDATES, &ref_updates),
              AOM_CODEC_OK);
    EXPECT_EQ(ref_updates, frame == 0 ? 0xff : 1 << frame);
  }

  aom_img_free(img);
  ASSERT_EQ(aom_codec_destroy(&dec), AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_destroy(&enc), AOM_CODEC_OK);
}
#endif  // CONFIG_AV1_DECODER

// AV1E_SET_SVC_REF_FRAME_CONFIG: the encoder keeps track of when each buffer
// slot was last refreshed. Among other things, this is used to detect recovery
// frames with RPS (reference picture selection): if a frame's closest
// reference is several frames old, but the previous frame did predict from its
// predecessor, the frame is encoded at a lower QP (see
// set_flag_rps_bias_recovery_frame()).
//
// This bookkeeping used to only be done for refreshed slots that one of the
// ref_idx[] entries points to. This test runs two encoders with the same RPS
// structure: frame n is stored in slot n % 6 and predicts from frame n - 1,
// except for every 8th frame, which predicts from frame n - 5. LAST is the
// only reference in use. The encoders only differ in where the unused
// references point:
//  - "Aliased": the refreshed slot is aliased as LAST2, and the other unused
//    references point to the predicted slot.
//  - "Not aliased": all seven ref_idx[] entries point to the predicted slot,
//    so the refreshed slot is not among them.
// Both encoders must produce frames with identical QPs and sizes.
TEST(EncodeAPI, SvcRefFrameConfigTracksRefreshOfSlotNotInRefIdx) {
  constexpr int kNumFrames = 20;
  constexpr int kNumSlots = 6;
  aom_codec_iface_t *const iface = aom_codec_av1_cx();
  aom_codec_enc_cfg_t cfg;
  ASSERT_EQ(aom_codec_enc_config_default(iface, &cfg, AOM_USAGE_REALTIME),
            AOM_CODEC_OK);
  cfg.g_w = 320;
  cfg.g_h = 240;
  cfg.g_lag_in_frames = 0;
  cfg.rc_end_usage = AOM_CBR;
  cfg.rc_target_bitrate = 300;

  aom_codec_ctx_t enc_aliased;
  aom_codec_ctx_t enc_not_aliased;
  ASSERT_EQ(aom_codec_enc_init(&enc_aliased, iface, &cfg, 0), AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_enc_init(&enc_not_aliased, iface, &cfg, 0), AOM_CODEC_OK);
  for (aom_codec_ctx_t *enc : { &enc_aliased, &enc_not_aliased }) {
    ASSERT_EQ(aom_codec_control(enc, AOME_SET_CPUUSED, 9), AOM_CODEC_OK);
    // Order hints are off, as is common for RTC. This also ensures that the
    // two encoders' frame headers have the same size.
    ASSERT_EQ(aom_codec_control(enc, AV1E_SET_ENABLE_ORDER_HINT, 0),
              AOM_CODEC_OK);
  }
  aom_image_t *img =
      aom_img_alloc(nullptr, AOM_IMG_FMT_I420, cfg.g_w, cfg.g_h, 1);
  ASSERT_NE(img, nullptr);

  for (int frame = 0; frame < kNumFrames; ++frame) {
    SCOPED_TRACE(frame);
    const int refreshed_slot = frame % kNumSlots;
    int predicted_slot = 0;
    if (frame > 0) {
      const int predicted_frame = frame % 8 == 0 ? frame - 5 : frame - 1;
      predicted_slot = predicted_frame % kNumSlots;
    }
    aom_svc_ref_frame_config_t not_aliased_cfg = {};
    for (int i = 0; i < 7; ++i) not_aliased_cfg.ref_idx[i] = predicted_slot;
    not_aliased_cfg.reference[0] = frame > 0;  // LAST.
    not_aliased_cfg.refresh[refreshed_slot] = 1;
    aom_svc_ref_frame_config_t aliased_cfg = not_aliased_cfg;
    aliased_cfg.ref_idx[1] = refreshed_slot;  // LAST2.

    FillMovingPattern(img, frame);
    int qp[2];
    size_t size[2];
    for (int i = 0; i < 2; ++i) {
      aom_codec_ctx_t *enc = i == 0 ? &enc_aliased : &enc_not_aliased;
      ASSERT_EQ(aom_codec_control(enc, AV1E_SET_SVC_REF_FRAME_CONFIG,
                                  i == 0 ? &aliased_cfg : &not_aliased_cfg),
                AOM_CODEC_OK);
      ASSERT_EQ(aom_codec_encode(enc, img, frame, 1, 0), AOM_CODEC_OK);
      aom_codec_iter_t iter = nullptr;
      const aom_codec_cx_pkt_t *pkt = aom_codec_get_cx_data(enc, &iter);
      ASSERT_NE(pkt, nullptr);
      ASSERT_EQ(pkt->kind, AOM_CODEC_CX_FRAME_PKT);
      size[i] = pkt->data.frame.sz;
      ASSERT_EQ(aom_codec_control(enc, AOME_GET_LAST_QUANTIZER, &qp[i]),
                AOM_CODEC_OK);
    }
    EXPECT_EQ(qp[0], qp[1]);
    EXPECT_EQ(size[0], size[1]);
  }

  aom_img_free(img);
  ASSERT_EQ(aom_codec_destroy(&enc_aliased), AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_destroy(&enc_not_aliased), AOM_CODEC_OK);
}

// AV1E_SET_SVC_REF_FRAME_CONFIG: the ref_idx[] entries of references whose
// reference[] flag is 0 are don't-cares, and must not affect the encoding.
//
// This test runs two encoders with the same prediction structure (every frame
// predicts from the previous frame's reconstruction, using LAST only) that
// only differ in which buffer slots they use:
//  - "In place": every frame predicts from slot 0 and refreshes slot 0. The six
//    unused references also point to slot 0, which is what a zero-initialized
//    config gives (libaom's fixed SVC mode does the same for base layer frames
//    on the lowest spatial layer).
//  - "Ping-pong": frames alternate between predicting from slot 0 and
//    refreshing slot 1, and vice versa. The refreshed slot is aliased as LAST2
//    (like libaom's internal single layer RTC structure does) and the other
//    unused references point to the predicted slot.
// Both encoders must produce frames with identical QPs and sizes.
//
// Previously, the in-place encoder treated every frame as a golden and altref
// update, because the unused GOLDEN/BWDREF/ALTREF aliases pointed to the
// refreshed slot. How that affected the encoding depends on the rate control
// mode, see the tests below.
static void SvcRefFrameConfigUnusedAliasesTest(aom_rc_mode rc_mode) {
  constexpr int kNumFrames = 20;
  aom_codec_iface_t *const iface = aom_codec_av1_cx();
  aom_codec_enc_cfg_t cfg;
  ASSERT_EQ(aom_codec_enc_config_default(iface, &cfg, AOM_USAGE_REALTIME),
            AOM_CODEC_OK);
  cfg.g_w = 320;
  cfg.g_h = 240;
  cfg.g_lag_in_frames = 0;
  cfg.rc_end_usage = rc_mode;
  cfg.rc_target_bitrate = 300;

  aom_codec_ctx_t enc_in_place;
  aom_codec_ctx_t enc_ping_pong;
  ASSERT_EQ(aom_codec_enc_init(&enc_in_place, iface, &cfg, 0), AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_enc_init(&enc_ping_pong, iface, &cfg, 0), AOM_CODEC_OK);
  for (aom_codec_ctx_t *enc : { &enc_in_place, &enc_ping_pong }) {
    ASSERT_EQ(aom_codec_control(enc, AOME_SET_CPUUSED, 9), AOM_CODEC_OK);
    ASSERT_EQ(aom_codec_control(enc, AOME_SET_CQ_LEVEL, 40), AOM_CODEC_OK);
    // Order hints are off, as is common for RTC. This also ensures that the
    // two encoders' frame headers have the same size.
    ASSERT_EQ(aom_codec_control(enc, AV1E_SET_ENABLE_ORDER_HINT, 0),
              AOM_CODEC_OK);
  }
  aom_image_t *img =
      aom_img_alloc(nullptr, AOM_IMG_FMT_I420, cfg.g_w, cfg.g_h, 1);
  ASSERT_NE(img, nullptr);

  for (int frame = 0; frame < kNumFrames; ++frame) {
    SCOPED_TRACE(frame);
    aom_svc_ref_frame_config_t in_place_cfg = {};
    in_place_cfg.reference[0] = frame > 0;  // LAST.
    in_place_cfg.refresh[0] = 1;

    aom_svc_ref_frame_config_t ping_pong_cfg = {};
    const int predicted_slot = (frame + 1) % 2;
    const int refreshed_slot = frame % 2;
    for (int i = 0; i < 7; ++i) ping_pong_cfg.ref_idx[i] = predicted_slot;
    ping_pong_cfg.ref_idx[1] = refreshed_slot;  // LAST2.
    ping_pong_cfg.reference[0] = frame > 0;     // LAST.
    ping_pong_cfg.refresh[refreshed_slot] = 1;

    FillMovingPattern(img, frame);
    int qp[2];
    size_t size[2];
    for (int i = 0; i < 2; ++i) {
      aom_codec_ctx_t *enc = i == 0 ? &enc_in_place : &enc_ping_pong;
      ASSERT_EQ(aom_codec_control(enc, AV1E_SET_SVC_REF_FRAME_CONFIG,
                                  i == 0 ? &in_place_cfg : &ping_pong_cfg),
                AOM_CODEC_OK);
      ASSERT_EQ(aom_codec_encode(enc, img, frame, 1, 0), AOM_CODEC_OK);
      aom_codec_iter_t iter = nullptr;
      const aom_codec_cx_pkt_t *pkt = aom_codec_get_cx_data(enc, &iter);
      ASSERT_NE(pkt, nullptr);
      ASSERT_EQ(pkt->kind, AOM_CODEC_CX_FRAME_PKT);
      size[i] = pkt->data.frame.sz;
      ASSERT_EQ(aom_codec_control(enc, AOME_GET_LAST_QUANTIZER, &qp[i]),
                AOM_CODEC_OK);
    }
    EXPECT_EQ(qp[0], qp[1]);
    EXPECT_EQ(size[0], size[1]);
  }

  aom_img_free(img);
  ASSERT_EQ(aom_codec_destroy(&enc_in_place), AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_destroy(&enc_ping_pong), AOM_CODEC_OK);
}

// In single layer CBR mode, among other things, the average inter frame QP is
// not updated on golden and altref updates.
TEST(EncodeAPI, SvcRefFrameConfigUnusedAliasesDoNotAffectEncodingInCbrMode) {
  SvcRefFrameConfigUnusedAliasesTest(AOM_CBR);
}

// In AOM_Q mode, golden and altref updates are encoded at a lower QP than the
// configured cq-level.
TEST(EncodeAPI, SvcRefFrameConfigUnusedAliasesDoNotAffectEncodingInQMode) {
  SvcRefFrameConfigUnusedAliasesTest(AOM_Q);
}

#if CONFIG_AV1_DECODER
// Encodes the next frame from `enc`, and decodes it with `dec`. Returns the
// decoder's status, and the size of the decoded frame in `decoded_w` and
// `decoded_h`.
static aom_codec_err_t EncodeAndDecodeFrame(
    aom_codec_ctx_t *enc, aom_codec_ctx_t *dec, const aom_image_t *img,
    aom_codec_pts_t pts, unsigned int *decoded_w, unsigned int *decoded_h) {
  *decoded_w = 0;
  *decoded_h = 0;
  EXPECT_EQ(aom_codec_encode(enc, img, pts, 1, 0), AOM_CODEC_OK);
  aom_codec_iter_t iter = nullptr;
  const aom_codec_cx_pkt_t *pkt = aom_codec_get_cx_data(enc, &iter);
  EXPECT_NE(pkt, nullptr);
  if (pkt == nullptr) return AOM_CODEC_ERROR;
  EXPECT_EQ(pkt->kind, AOM_CODEC_CX_FRAME_PKT);
  const aom_codec_err_t status =
      aom_codec_decode(dec, static_cast<const uint8_t *>(pkt->data.frame.buf),
                       pkt->data.frame.sz, nullptr);
  if (status != AOM_CODEC_OK) return status;
  aom_codec_iter_t dec_iter = nullptr;
  const aom_image_t *decoded = aom_codec_get_frame(dec, &dec_iter);
  if (decoded != nullptr) {
    *decoded_w = decoded->d_w;
    *decoded_h = decoded->d_h;
  }
  return status;
}

// AV1E_SET_SVC_REF_FRAME_CONFIG: the ref_idx[] entries of references whose
// reference[] flag is 0 are don't-cares, even if they point to a slot that is
// not a valid reference for the current frame. The frame header signals all
// seven references though, and decoders (libaom, dav1d) reject a frame if any
// of them is more than 2x larger, or more than 16x smaller, than the frame.
//
// This test encodes two spatial layers, with the lower layer at 1/4 of the
// resolution of the upper layer. Each layer predicts from its own previous
// frame (LAST): slot 0 for the lower layer and slot 1 for the upper layer. The
// upper layer also predicts from the lower layer's frame (GOLDEN, slot 0). All
// unused references point to slot 1, which for the lower layer is a 4x larger
// frame from the upper layer, and so an invalid reference.
//
// Order hints are off, as is common for RTC. With order hints, unused
// references also affect the decoding process, so the encoder leaves them as
// configured, and the application must make sure that they are valid.
TEST(EncodeAPI, SvcRefFrameConfigUnusedRefIdxMayPointToInvalidReference) {
  aom_codec_iface_t *const iface = aom_codec_av1_cx();
  aom_codec_enc_cfg_t cfg;
  ASSERT_EQ(aom_codec_enc_config_default(iface, &cfg, AOM_USAGE_REALTIME),
            AOM_CODEC_OK);
  cfg.g_w = 320;
  cfg.g_h = 240;
  cfg.g_lag_in_frames = 0;
  cfg.rc_end_usage = AOM_CBR;
  cfg.rc_target_bitrate = 400;
  aom_codec_ctx_t enc;
  ASSERT_EQ(aom_codec_enc_init(&enc, iface, &cfg, 0), AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_control(&enc, AOME_SET_CPUUSED, 9), AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_control(&enc, AV1E_SET_ENABLE_ORDER_HINT, 0),
            AOM_CODEC_OK);
  aom_svc_params_t svc_params = {};
  svc_params.number_spatial_layers = 2;
  svc_params.number_temporal_layers = 1;
  svc_params.scaling_factor_num[0] = 1;
  svc_params.scaling_factor_den[0] = 4;
  svc_params.scaling_factor_num[1] = 1;
  svc_params.scaling_factor_den[1] = 1;
  svc_params.layer_target_bitrate[0] = 100;
  svc_params.layer_target_bitrate[1] = 300;
  svc_params.framerate_factor[0] = 1;
  for (int sl = 0; sl < 2; ++sl) {
    svc_params.min_quantizers[sl] = 0;
    svc_params.max_quantizers[sl] = 56;
  }
  ASSERT_EQ(aom_codec_control(&enc, AV1E_SET_SVC_PARAMS, &svc_params),
            AOM_CODEC_OK);
  aom_codec_ctx_t dec;
  ASSERT_EQ(aom_codec_dec_init(&dec, aom_codec_av1_dx(), nullptr, 0),
            AOM_CODEC_OK);
  aom_image_t *img =
      aom_img_alloc(nullptr, AOM_IMG_FMT_I420, cfg.g_w, cfg.g_h, 1);
  ASSERT_NE(img, nullptr);

  for (int frame = 0; frame < 3; ++frame) {
    FillMovingPattern(img, frame);
    for (int sl = 0; sl < 2; ++sl) {
      SCOPED_TRACE(testing::Message() << "frame " << frame << ", layer " << sl);
      aom_svc_layer_id_t layer_id = {};
      layer_id.spatial_layer_id = sl;
      ASSERT_EQ(aom_codec_control(&enc, AV1E_SET_SVC_LAYER_ID, &layer_id),
                AOM_CODEC_OK);
      aom_svc_ref_frame_config_t ref_cfg = {};
      for (int i = 0; i < 7; ++i) ref_cfg.ref_idx[i] = 1;
      ref_cfg.ref_idx[0] = sl;  // LAST.
      ref_cfg.reference[0] = 1;
      if (sl == 1) {
        ref_cfg.ref_idx[3] = 0;  // GOLDEN.
        ref_cfg.reference[3] = 1;
      }
      ref_cfg.refresh[sl] = 1;
      ASSERT_EQ(
          aom_codec_control(&enc, AV1E_SET_SVC_REF_FRAME_CONFIG, &ref_cfg),
          AOM_CODEC_OK);
      unsigned int decoded_w;
      unsigned int decoded_h;
      EXPECT_EQ(
          EncodeAndDecodeFrame(&enc, &dec, img, frame, &decoded_w, &decoded_h),
          AOM_CODEC_OK)
          << aom_codec_error_detail(&dec);
    }
  }

  aom_img_free(img);
  ASSERT_EQ(aom_codec_destroy(&dec), AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_destroy(&enc), AOM_CODEC_OK);
}

// Like the test above, but with a single layer that changes resolution. When
// a frame's size differs from the maximum size, the frame header may signal
// that it has the same size as one of its seven references. This must take
// the references that are actually signaled into account.
//
// The frames are 320x240, 160x120, 320x240 and 160x120. The last frame
// predicts from the 320x240 frame (LAST), and its unused references point to
// the slot holding the first 160x120 frame.
TEST(EncodeAPI, SvcRefFrameConfigUnusedRefIdxWithResolutionChange) {
  aom_codec_iface_t *const iface = aom_codec_av1_cx();
  aom_codec_enc_cfg_t cfg;
  ASSERT_EQ(aom_codec_enc_config_default(iface, &cfg, AOM_USAGE_REALTIME),
            AOM_CODEC_OK);
  cfg.g_w = 320;
  cfg.g_h = 240;
  cfg.g_lag_in_frames = 0;
  cfg.rc_end_usage = AOM_CBR;
  cfg.rc_target_bitrate = 300;
  // Allows changing the resolution without key frames.
  cfg.g_forced_max_frame_width = 320;
  cfg.g_forced_max_frame_height = 240;
  aom_codec_ctx_t enc;
  ASSERT_EQ(aom_codec_enc_init(&enc, iface, &cfg, 0), AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_control(&enc, AOME_SET_CPUUSED, 9), AOM_CODEC_OK);
  // Order hints are off, as is common for RTC.
  ASSERT_EQ(aom_codec_control(&enc, AV1E_SET_ENABLE_ORDER_HINT, 0),
            AOM_CODEC_OK);
  aom_codec_ctx_t dec;
  ASSERT_EQ(aom_codec_dec_init(&dec, aom_codec_av1_dx(), nullptr, 0),
            AOM_CODEC_OK);

  struct FrameConfig {
    unsigned int width;
    unsigned int height;
    int predicted_slot;
    int refreshed_slot;
    int unused_slot;
  };
  // Frame 0 is a key frame, which refreshes all slots.
  constexpr FrameConfig kFrames[] = { { 320, 240, 0, 0, 0 },
                                      { 160, 120, 0, 1, 0 },
                                      { 320, 240, 1, 0, 1 },
                                      { 160, 120, 0, 2, 1 } };
  for (int frame = 0; frame < 4; ++frame) {
    SCOPED_TRACE(frame);
    const FrameConfig &config = kFrames[frame];
    cfg.g_w = config.width;
    cfg.g_h = config.height;
    ASSERT_EQ(aom_codec_enc_config_set(&enc, &cfg), AOM_CODEC_OK);
    aom_svc_ref_frame_config_t ref_cfg = {};
    for (int i = 0; i < 7; ++i) ref_cfg.ref_idx[i] = config.unused_slot;
    ref_cfg.ref_idx[0] = config.predicted_slot;  // LAST.
    ref_cfg.reference[0] = frame > 0;
    ref_cfg.refresh[config.refreshed_slot] = 1;
    ASSERT_EQ(aom_codec_control(&enc, AV1E_SET_SVC_REF_FRAME_CONFIG, &ref_cfg),
              AOM_CODEC_OK);
    aom_image_t *img =
        aom_img_alloc(nullptr, AOM_IMG_FMT_I420, cfg.g_w, cfg.g_h, 1);
    ASSERT_NE(img, nullptr);
    FillMovingPattern(img, frame);
    unsigned int decoded_w;
    unsigned int decoded_h;
    EXPECT_EQ(
        EncodeAndDecodeFrame(&enc, &dec, img, frame, &decoded_w, &decoded_h),
        AOM_CODEC_OK)
        << aom_codec_error_detail(&dec);
    EXPECT_EQ(decoded_w, config.width);
    EXPECT_EQ(decoded_h, config.height);
    // Only frame 0 is a key frame.
    int ref_updates = 0;
    ASSERT_EQ(aom_codec_control(&dec, AOMD_GET_LAST_REF_UPDATES, &ref_updates),
              AOM_CODEC_OK);
    EXPECT_EQ(ref_updates, frame == 0 ? 0xff : 1 << config.refreshed_slot);
    aom_img_free(img);
  }

  ASSERT_EQ(aom_codec_destroy(&dec), AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_destroy(&enc), AOM_CODEC_OK);
}
#endif  // CONFIG_AV1_DECODER

// Verify that when handle_inter_mode_nonrd() reuses the superblock motion
// vector (x->sb_me_mv, searched relative to zero) for NEWMV on screen content,
// the vector is clamped to the valid subpel range relative to the block's
// reference MV predictor (ref_mv) so that |mv - ref_mv| does not exceed MV_MAX.
TEST(EncodeAPI, NonrdSbMeForcedNewmvRange) {
  // At >= 4K resolution (width * height >= 3840 * 2160),
  // do_int_pro_motion_estimation() doubles the screen-content search window
  // from 512 to 1024 full-pel pixels ([-1024, +1023] relative to (0, 0)).
  // Use a portrait 2160x3840 frame so +/-1023 pixel vertical displacements
  // stay well within the frame height.
  constexpr int kWidth = 2160;
  constexpr int kHeight = 3840;
  constexpr int kActiveW = 128;
  constexpr int kActiveH = 2312;

  aom_codec_iface_t *const iface = aom_codec_av1_cx();
  aom_codec_enc_cfg_t cfg;
  ASSERT_EQ(aom_codec_enc_config_default(iface, &cfg, AOM_USAGE_REALTIME),
            AOM_CODEC_OK);
  cfg.g_w = kWidth;
  cfg.g_h = kHeight;
  cfg.g_lag_in_frames = 0;
  cfg.g_threads = 1;
  cfg.rc_min_quantizer = 1;
  cfg.rc_max_quantizer = 1;

  aom_codec_ctx_t enc;
  ASSERT_EQ(aom_codec_enc_init(&enc, iface, &cfg, 0), AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_control(&enc, AOME_SET_CPUUSED, 7), AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_control(&enc, AV1E_SET_TUNE_CONTENT, AOM_CONTENT_SCREEN),
            AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_control(&enc, AV1E_SET_QUANTIZER_ONE_PASS, 1),
            AOM_CODEC_OK);

  aom_image_t *img0 =
      aom_img_alloc(nullptr, AOM_IMG_FMT_I420, kWidth, kHeight, 1);
  aom_image_t *img1 =
      aom_img_alloc(nullptr, AOM_IMG_FMT_I420, kWidth, kHeight, 1);
  ASSERT_NE(img0, nullptr);
  ASSERT_NE(img1, nullptr);
  FillImage(img0, 128);
  FillImage(img1, 128);

  // Fill a 128-px wide strip (one superblock column) of the keyframe (img0)
  // with random luma values so that displaced copies in img1 have a unique
  // zero-SAD match and a large zero-MV SAD.
  ::libaom_test::ACMRandom rnd(::libaom_test::ACMRandom::DeterministicSeed());
  for (int y = 0; y < kActiveH; ++y) {
    uint8_t *const row0 = img0->planes[AOM_PLANE_Y] + y * img0->stride[0];
    for (int x = 0; x < kActiveW; ++x) {
      row0[x] = rnd.Rand8();
    }
  }

  // Construct three vertically adjacent superblock regions in img1 (delta
  // frame) to produce an unrepresentable MV difference if x->sb_me_mv is not
  // clamped relative to ref_mv:
  //
  // 1. y in [0, 1152): copied from img0 at y + 1023 (dy = +1023 px).
  //    do_int_pro_motion_estimation() searches [-1024, +1023] relative to
  //    (0, 0), finds the exact match, and sets x->sb_me_mv = (+8184, 0)
  //    (+1023 px in 1/8-pel units).
  //
  // 2. y in [1152, 1280): copied from img0 at y + 1031 (dy = +1031 px).
  //    Because +1031 px is outside int_pro's [-1024, +1023] window around
  //    (0, 0), x->sb_me_block is not set. Instead, normal NEWMV motion search
  //    (search_new_mv()) starts from the top neighbor's predictor
  //    ref_mv = (+8184, 0) (+1023 px) and steps +8 px to find the exact match
  //    at mv = (+8248, 0) (+1031 px).
  //
  // 3. y in [1280, 1408): copied from img0 at y - 1023 (dy = -1023 px).
  //    At y >= 1280, -1023 px upward is inside img0, so int_pro ME finds the
  //    exact match relative to (0, 0) and sets x->sb_me_block = 1 with
  //    x->sb_me_mv = (-8184, 0) (-1023 px). However, this superblock's coding
  //    predictor from its top neighbor (region 2) is ref_mv = (+8248, 0)
  //    (+1031 px). Without clamping x->sb_me_mv relative to ref_mv in
  //    handle_inter_mode_nonrd(), NEWMV encodes the difference:
  //      mv_diff.row = -8184 - 8248 = -16432 (-2054 px)
  //    which exceeds MV_MAX (16383, +/-2047.875 px) and triggers
  //    Assertion `c <= MV_CLASS_10' failed in av1_get_mv_class().
  for (int y = 0; y < 1408; ++y) {
    const int dy = (y < 1152) ? 1023 : (y < 1280) ? 1031 : -1023;
    memcpy(img1->planes[AOM_PLANE_Y] + y * img1->stride[0],
           img0->planes[AOM_PLANE_Y] + (y + dy) * img0->stride[0], kActiveW);
  }

  ASSERT_EQ(aom_codec_encode(&enc, img0, 0, 1, AOM_EFLAG_FORCE_KF),
            AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_encode(&enc, img1, 1, 1, 0), AOM_CODEC_OK);

  aom_img_free(img1);
  aom_img_free(img0);
  ASSERT_EQ(aom_codec_destroy(&enc), AOM_CODEC_OK);
}

#if CONFIG_AV1_HIGHBITDEPTH
// A test that reproduces issues.oss-fuzz.com/issues/572842743: signed integer
// overflow in fill_variance_tree_leaves().
TEST(EncodeAPI, Avg16x16IntegerOverflow) {
  // clang-format off
  // YUV 4:2:0
  // Layer 0: 16x1
  // Y height is padded to the even number 2. There is an unused last row,
  // set to 0.
  constexpr unsigned short kBuffer0[16 * 2 + 8 * 1 + 8 * 1] = {
    // Y plane
    667, 1, 2591, 1134, 725, 1, 0, 4095, 4095, 0, 0, 2417, 2721, 1, 1, 4095,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    // U plane
    3238, 0, 1548, 4095, 4095, 0, 3545, 4095,
    // V plane
    3859, 413, 4095, 1, 3692, 3398, 0, 1,
  };
  // Layer 1: 2x1
  // Y height is padded to the even number 2. There is an unused last row,
  // set to 0.
  constexpr unsigned short kBuffer1[2 * 2 + 1 * 1 + 1 * 1] = {
    // Y plane
    2293, 1918,
    0, 0,
    // U plane
    0,
    // V plane
    71,
  };
  // Layer 2: 2x2
  constexpr unsigned short kBuffer2[2 * 2 + 1 * 1 + 1 * 1] = {
    // Y plane
    1, 4095,
    1, 4095,
    // U plane
    1,
    // V plane
    0,
  };
  // Layer 3: 18x28
  constexpr unsigned short kBuffer3[18 * 28 + 9 * 14 + 9 * 14] = {
    // Y plane
    3318, 1, 4095, 1, 0, 1, 4095, 1, 2987, 4095, 1993, 0, 0, 1, 0, 0, 0, 4095,
    4095, 4095, 1, 3467, 2171, 3082, 1, 4095, 0, 0, 0, 2776, 1, 4095, 4095, 947, 0, 4095,
    382, 0, 0, 4095, 1, 4095, 4095, 4095, 1, 4095, 0, 4095, 1603, 0, 1547, 1123, 1, 2206,
    1, 4095, 3963, 1440, 0, 1, 4095, 1, 2179, 3181, 4095, 1708, 4095, 0, 3401, 0, 4095, 0,
    805, 0, 0, 2330, 2496, 1, 1, 0, 4095, 1, 1, 4095, 1, 4095, 2606, 1675, 0, 1858,
    0, 2061, 0, 3599, 0, 0, 4095, 3121, 1, 4095, 1, 0, 1, 3253, 638, 4095, 2792, 1,
    0, 4095, 4095, 1, 1, 1709, 1, 4095, 4095, 1, 4095, 4095, 1, 4095, 1926, 0, 0, 4063,
    2893, 4095, 0, 4095, 4095, 0, 3581, 4095, 0, 4095, 0, 0, 340, 0, 3213, 4095, 0, 788,
    1, 0, 0, 1, 2601, 4095, 1, 0, 2577, 4095, 0, 0, 1891, 3700, 0, 1427, 1, 4095,
    47, 4095, 4095, 3451, 4095, 1, 0, 4095, 0, 4095, 4095, 4095, 1, 1, 1, 4095, 0, 0,
    1, 0, 1, 0, 3737, 1, 1280, 0, 1, 4095, 0, 2845, 1, 2079, 3417, 4095, 1972, 4095,
    1, 1, 4095, 4095, 0, 4075, 1, 3985, 0, 4095, 3462, 2080, 1, 0, 0, 0, 1, 1,
    482, 4095, 0, 2107, 4095, 2548, 1, 4095, 1226, 3702, 2396, 4095, 4095, 1, 0, 1, 3669, 1,
    2368, 2328, 0, 4095, 1, 0, 4095, 4095, 0, 0, 0, 4095, 4095, 2099, 0, 1649, 85, 4095,
    1, 3100, 4095, 0, 4095, 0, 1, 4095, 0, 4095, 0, 0, 1, 1, 4095, 1064, 4095, 0,
    1, 4095, 1, 3493, 650, 1, 0, 0, 1255, 2397, 0, 0, 1, 0, 0, 1, 3374, 65,
    1, 1, 0, 4095, 845, 0, 0, 0, 1, 4095, 1, 4095, 0, 808, 1, 4095, 4095, 2222,
    2031, 143, 1, 578, 1, 4095, 0, 340, 4095, 0, 4095, 1, 1, 0, 1470, 0, 4095, 0,
    1, 1746, 0, 1, 1, 1, 1, 1306, 4095, 0, 0, 3963, 4095, 4095, 1, 447, 0, 0,
    1, 1, 4095, 0, 4095, 4095, 3247, 4095, 1, 3663, 1, 4095, 4095, 0, 1592, 1, 142, 0,
    1, 4095, 4095, 1, 0, 4095, 450, 1, 1, 0, 4095, 1517, 1, 1, 3380, 1, 0, 2773,
    4095, 1434, 1, 0, 1, 0, 4095, 1, 1286, 4095, 3051, 3618, 361, 4095, 0, 4095, 1470, 0,
    0, 1, 0, 0, 3354, 1, 0, 4095, 2693, 1, 4095, 0, 0, 2809, 1, 845, 0, 2463,
    4095, 3093, 4095, 1, 1, 4095, 1485, 1, 0, 1, 1839, 0, 0, 1, 1, 2195, 1, 4095,
    4095, 528, 4095, 0, 228, 4095, 1, 4, 0, 4095, 1, 1, 2550, 1, 0, 1, 1, 0,
    1, 4095, 0, 4095, 3309, 2151, 0, 0, 1, 1, 1538, 1, 1, 1, 0, 4095, 1, 1,
    0, 4095, 1919, 1, 1, 4095, 1, 0, 3499, 4095, 4095, 4095, 1, 0, 768, 0, 1836, 1,
    1001, 2602, 3353, 1, 1007, 4095, 1298, 2329, 1, 1, 4095, 0, 3714, 0, 0, 1, 0, 0,
    // U plane
    4095, 1, 1, 1, 1753, 1, 1, 4095, 1,
    4024, 1, 0, 1, 1, 4095, 4095, 4095, 1,
    1721, 3999, 1, 2306, 1, 4095, 852, 4095, 2860,
    0, 4095, 1, 0, 2168, 1, 0, 1, 4095,
    0, 0, 1050, 0, 1, 1, 1, 4095, 589,
    4095, 1, 0, 0, 1427, 2419, 309, 4095, 1,
    1, 1, 4095, 1, 0, 0, 4095, 2111, 1,
    4095, 1, 710, 2455, 1363, 1, 1, 0, 1,
    4095, 4052, 2456, 4095, 1, 4095, 4095, 3442, 2167,
    0, 0, 1, 4095, 0, 2405, 0, 2904, 3209,
    1043, 1, 2524, 4095, 1495, 0, 4095, 4095, 1,
    0, 85, 4095, 3958, 1, 0, 4095, 3844, 0,
    4095, 2945, 2169, 3612, 4095, 4095, 301, 0, 3483,
    2516, 0, 2902, 4095, 1, 4095, 0, 1, 2402,
    // V plane
    1, 1, 0, 1652, 0, 0, 3167, 4095, 1,
    1, 4095, 0, 4095, 1, 434, 1103, 4095, 0,
    907, 1, 0, 4095, 0, 0, 0, 0, 1,
    4095, 4095, 1, 2003, 1, 1082, 1, 1, 4095,
    1113, 0, 1659, 0, 0, 1002, 0, 0, 3990,
    0, 1, 0, 1, 2948, 0, 1489, 4095, 4095,
    1, 4095, 1, 1, 1, 2093, 2901, 4095, 3901,
    1, 4009, 4095, 576, 0, 4095, 0, 2648, 1,
    4095, 3527, 4095, 4095, 2285, 1, 0, 0, 1,
    1259, 2927, 0, 0, 3691, 4095, 0, 4095, 0,
    2735, 4095, 937, 4095, 1013, 1, 1, 4095, 1,
    4095, 1575, 0, 1, 0, 4095, 0, 4095, 2377,
    2204, 4095, 2193, 0, 0, 4066, 4095, 0, 2220,
    1, 3545, 1, 1, 0, 1170, 4095, 3778, 4095,
  };
  // clang-format on

  aom_image_t img;
  EXPECT_EQ(&img, aom_img_wrap(&img, AOM_IMG_FMT_I42016, 16, 1, 1,
                               reinterpret_cast<unsigned char *>(
                                   const_cast<unsigned short *>(kBuffer0))));

  aom_codec_iface_t *iface = aom_codec_av1_cx();
  aom_codec_enc_cfg_t cfg;
  EXPECT_EQ(AOM_CODEC_OK,
            aom_codec_enc_config_default(iface, &cfg, AOM_USAGE_REALTIME));
  cfg.rc_end_usage = AOM_CBR;
  cfg.g_profile = 2;
  cfg.g_bit_depth = AOM_BITS_12;
  cfg.g_input_bit_depth = 12;
  cfg.g_w = 16;
  cfg.g_h = 1;
  cfg.g_forced_max_frame_width = 18;
  cfg.g_forced_max_frame_height = 28;
  cfg.g_limit = 4;
  cfg.g_lag_in_frames = 0;
  cfg.g_threads = 40;
  cfg.rc_min_quantizer = 57;
  cfg.rc_max_quantizer = 63;
  aom_codec_ctx_t enc;
  ASSERT_EQ(AOM_CODEC_OK,
            aom_codec_enc_init(&enc, iface, &cfg, AOM_CODEC_USE_HIGHBITDEPTH));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_control(&enc, AV1E_SET_TILE_ROWS, 5));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_control(&enc, AV1E_SET_TILE_COLUMNS, 2));
  EXPECT_EQ(AOM_CODEC_OK,
            aom_codec_control(&enc, AOME_SET_NUMBER_SPATIAL_LAYERS, 4));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_control(&enc, AOME_SET_CPUUSED, 9));
  EXPECT_EQ(AOM_CODEC_OK,
            aom_codec_control(&enc, AV1E_SET_COLOR_RANGE, AOM_CR_FULL_RANGE));
  EXPECT_EQ(AOM_CODEC_OK,
            aom_codec_control(&enc, AOME_SET_TUNING, AOM_TUNE_IQ));
  EXPECT_EQ(AOM_CODEC_OK,
            aom_codec_control(&enc, AV1E_SET_ENABLE_RESTORATION, 0));

  // Encode frame
  EXPECT_EQ(AOM_CODEC_OK,
            aom_codec_control(&enc, AOME_SET_SPATIAL_LAYER_ID, 0));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_encode(&enc, &img, 0, 1, 0));
  aom_codec_iter_t iter = nullptr;
  const aom_codec_cx_pkt_t *pkt = aom_codec_get_cx_data(&enc, &iter);
  ASSERT_NE(pkt, nullptr);
  EXPECT_EQ(pkt->kind, AOM_CODEC_CX_FRAME_PKT);
  pkt = aom_codec_get_cx_data(&enc, &iter);
  ASSERT_EQ(pkt, nullptr);

  // Encode frame
  EXPECT_EQ(&img, aom_img_wrap(&img, AOM_IMG_FMT_I42016, 2, 1, 1,
                               reinterpret_cast<unsigned char *>(
                                   const_cast<unsigned short *>(kBuffer1))));
  cfg.g_w = 2;
  cfg.g_h = 1;
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_enc_config_set(&enc, &cfg));
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
  pkt = aom_codec_get_cx_data(&enc, &iter);
  EXPECT_EQ(pkt, nullptr);

  // Encode frame
  EXPECT_EQ(&img, aom_img_wrap(&img, AOM_IMG_FMT_I42016, 2, 2, 1,
                               reinterpret_cast<unsigned char *>(
                                   const_cast<unsigned short *>(kBuffer2))));
  cfg.g_w = 2;
  cfg.g_h = 2;
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_enc_config_set(&enc, &cfg));
  EXPECT_EQ(AOM_CODEC_OK,
            aom_codec_control(&enc, AOME_SET_SPATIAL_LAYER_ID, 2));
  encode_flags = AOM_EFLAG_NO_REF_GF | AOM_EFLAG_NO_REF_ARF |
                 AOM_EFLAG_NO_REF_BWD | AOM_EFLAG_NO_REF_ARF2 |
                 AOM_EFLAG_NO_UPD_GF | AOM_EFLAG_NO_UPD_ARF;
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_encode(&enc, &img, 0, 1, encode_flags));
  iter = nullptr;
  pkt = aom_codec_get_cx_data(&enc, &iter);
  ASSERT_NE(pkt, nullptr);
  EXPECT_EQ(pkt->kind, AOM_CODEC_CX_FRAME_PKT);
  pkt = aom_codec_get_cx_data(&enc, &iter);
  EXPECT_EQ(pkt, nullptr);

  // Encode frame
  EXPECT_EQ(&img, aom_img_wrap(&img, AOM_IMG_FMT_I42016, 18, 28, 1,
                               reinterpret_cast<unsigned char *>(
                                   const_cast<unsigned short *>(kBuffer3))));
  cfg.g_w = 18;
  cfg.g_h = 28;
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_enc_config_set(&enc, &cfg));
  EXPECT_EQ(AOM_CODEC_OK,
            aom_codec_control(&enc, AOME_SET_SPATIAL_LAYER_ID, 3));
  encode_flags = AOM_EFLAG_NO_REF_GF | AOM_EFLAG_NO_REF_ARF |
                 AOM_EFLAG_NO_REF_BWD | AOM_EFLAG_NO_REF_ARF2 |
                 AOM_EFLAG_NO_UPD_GF | AOM_EFLAG_NO_UPD_ARF;
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_encode(&enc, &img, 0, 1, encode_flags));
  iter = nullptr;
  pkt = aom_codec_get_cx_data(&enc, &iter);
  ASSERT_NE(pkt, nullptr);
  EXPECT_EQ(pkt->kind, AOM_CODEC_CX_FRAME_PKT);
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
