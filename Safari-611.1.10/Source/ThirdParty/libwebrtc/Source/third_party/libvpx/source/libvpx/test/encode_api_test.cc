/*
 *  Copyright (c) 2016 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "third_party/googletest/src/include/gtest/gtest.h"

#include "./vpx_config.h"
#include "vpx/vp8cx.h"
#include "vpx/vpx_encoder.h"

namespace {

#define NELEMENTS(x) static_cast<int>(sizeof(x) / sizeof(x[0]))

TEST(EncodeAPI, InvalidParams) {
  static const vpx_codec_iface_t *kCodecs[] = {
#if CONFIG_VP8_ENCODER
    &vpx_codec_vp8_cx_algo,
#endif
#if CONFIG_VP9_ENCODER
    &vpx_codec_vp9_cx_algo,
#endif
  };
  uint8_t buf[1] = { 0 };
  vpx_image_t img;
  vpx_codec_ctx_t enc;
  vpx_codec_enc_cfg_t cfg;

  EXPECT_EQ(&img, vpx_img_wrap(&img, VPX_IMG_FMT_I420, 1, 1, 1, buf));

  EXPECT_EQ(VPX_CODEC_INVALID_PARAM, vpx_codec_enc_init(NULL, NULL, NULL, 0));
  EXPECT_EQ(VPX_CODEC_INVALID_PARAM, vpx_codec_enc_init(&enc, NULL, NULL, 0));
  EXPECT_EQ(VPX_CODEC_INVALID_PARAM, vpx_codec_encode(NULL, NULL, 0, 0, 0, 0));
  EXPECT_EQ(VPX_CODEC_INVALID_PARAM, vpx_codec_encode(NULL, &img, 0, 0, 0, 0));
  EXPECT_EQ(VPX_CODEC_INVALID_PARAM, vpx_codec_destroy(NULL));
  EXPECT_EQ(VPX_CODEC_INVALID_PARAM,
            vpx_codec_enc_config_default(NULL, NULL, 0));
  EXPECT_EQ(VPX_CODEC_INVALID_PARAM,
            vpx_codec_enc_config_default(NULL, &cfg, 0));
  EXPECT_TRUE(vpx_codec_error(NULL) != NULL);

  for (int i = 0; i < NELEMENTS(kCodecs); ++i) {
    SCOPED_TRACE(vpx_codec_iface_name(kCodecs[i]));
    EXPECT_EQ(VPX_CODEC_INVALID_PARAM,
              vpx_codec_enc_init(NULL, kCodecs[i], NULL, 0));
    EXPECT_EQ(VPX_CODEC_INVALID_PARAM,
              vpx_codec_enc_init(&enc, kCodecs[i], NULL, 0));
    EXPECT_EQ(VPX_CODEC_INVALID_PARAM,
              vpx_codec_enc_config_default(kCodecs[i], &cfg, 1));

    EXPECT_EQ(VPX_CODEC_OK, vpx_codec_enc_config_default(kCodecs[i], &cfg, 0));
    EXPECT_EQ(VPX_CODEC_OK, vpx_codec_enc_init(&enc, kCodecs[i], &cfg, 0));
    EXPECT_EQ(VPX_CODEC_OK, vpx_codec_encode(&enc, NULL, 0, 0, 0, 0));

    EXPECT_EQ(VPX_CODEC_OK, vpx_codec_destroy(&enc));
  }
}

TEST(EncodeAPI, HighBitDepthCapability) {
// VP8 should not claim VP9 HBD as a capability.
#if CONFIG_VP8_ENCODER
  const vpx_codec_caps_t vp8_caps = vpx_codec_get_caps(&vpx_codec_vp8_cx_algo);
  EXPECT_EQ(vp8_caps & VPX_CODEC_CAP_HIGHBITDEPTH, 0);
#endif

#if CONFIG_VP9_ENCODER
  const vpx_codec_caps_t vp9_caps = vpx_codec_get_caps(&vpx_codec_vp9_cx_algo);
#if CONFIG_VP9_HIGHBITDEPTH
  EXPECT_EQ(vp9_caps & VPX_CODEC_CAP_HIGHBITDEPTH, VPX_CODEC_CAP_HIGHBITDEPTH);
#else
  EXPECT_EQ(vp9_caps & VPX_CODEC_CAP_HIGHBITDEPTH, 0);
#endif
#endif
}

#if CONFIG_VP8_ENCODER
TEST(EncodeAPI, ImageSizeSetting) {
  const int width = 711;
  const int height = 360;
  const int bps = 12;
  vpx_image_t img;
  vpx_codec_ctx_t enc;
  vpx_codec_enc_cfg_t cfg;
  uint8_t *img_buf = reinterpret_cast<uint8_t *>(
      calloc(width * height * bps / 8, sizeof(*img_buf)));
  vpx_codec_enc_config_default(vpx_codec_vp8_cx(), &cfg, 0);

  cfg.g_w = width;
  cfg.g_h = height;

  vpx_img_wrap(&img, VPX_IMG_FMT_I420, width, height, 1, img_buf);

  vpx_codec_enc_init(&enc, vpx_codec_vp8_cx(), &cfg, 0);

  EXPECT_EQ(VPX_CODEC_OK, vpx_codec_encode(&enc, &img, 0, 1, 0, 0));

  free(img_buf);

  vpx_codec_destroy(&enc);
}
#endif

// Set up 2 spatial streams with 2 temporal layers per stream, and generate
// invalid configuration by setting the temporal layer rate allocation
// (ts_target_bitrate[]) to 0 for both layers. This should fail independent of
// CONFIG_MULTI_RES_ENCODING.
TEST(EncodeAPI, MultiResEncode) {
  static const vpx_codec_iface_t *kCodecs[] = {
#if CONFIG_VP8_ENCODER
    &vpx_codec_vp8_cx_algo,
#endif
#if CONFIG_VP9_ENCODER
    &vpx_codec_vp9_cx_algo,
#endif
  };
  const int width = 1280;
  const int height = 720;
  const int width_down = width / 2;
  const int height_down = height / 2;
  const int target_bitrate = 1000;
  const int framerate = 30;

  for (int c = 0; c < NELEMENTS(kCodecs); ++c) {
    const vpx_codec_iface_t *const iface = kCodecs[c];
    vpx_codec_ctx_t enc[2];
    vpx_codec_enc_cfg_t cfg[2];
    vpx_rational_t dsf[2] = { { 2, 1 }, { 2, 1 } };

    memset(enc, 0, sizeof(enc));

    for (int i = 0; i < 2; i++) {
      vpx_codec_enc_config_default(iface, &cfg[i], 0);
    }

    /* Highest-resolution encoder settings */
    cfg[0].g_w = width;
    cfg[0].g_h = height;
    cfg[0].rc_dropframe_thresh = 0;
    cfg[0].rc_end_usage = VPX_CBR;
    cfg[0].rc_resize_allowed = 0;
    cfg[0].rc_min_quantizer = 2;
    cfg[0].rc_max_quantizer = 56;
    cfg[0].rc_undershoot_pct = 100;
    cfg[0].rc_overshoot_pct = 15;
    cfg[0].rc_buf_initial_sz = 500;
    cfg[0].rc_buf_optimal_sz = 600;
    cfg[0].rc_buf_sz = 1000;
    cfg[0].g_error_resilient = 1; /* Enable error resilient mode */
    cfg[0].g_lag_in_frames = 0;

    cfg[0].kf_mode = VPX_KF_AUTO;
    cfg[0].kf_min_dist = 3000;
    cfg[0].kf_max_dist = 3000;

    cfg[0].rc_target_bitrate = target_bitrate; /* Set target bitrate */
    cfg[0].g_timebase.num = 1;                 /* Set fps */
    cfg[0].g_timebase.den = framerate;

    memcpy(&cfg[1], &cfg[0], sizeof(cfg[0]));
    cfg[1].rc_target_bitrate = 500;
    cfg[1].g_w = width_down;
    cfg[1].g_h = height_down;

    for (int i = 0; i < 2; i++) {
      cfg[i].ts_number_layers = 2;
      cfg[i].ts_periodicity = 2;
      cfg[i].ts_rate_decimator[0] = 2;
      cfg[i].ts_rate_decimator[1] = 1;
      cfg[i].ts_layer_id[0] = 0;
      cfg[i].ts_layer_id[1] = 1;
      // Invalid parameters.
      cfg[i].ts_target_bitrate[0] = 0;
      cfg[i].ts_target_bitrate[1] = 0;
    }

    // VP9 should report incapable, VP8 invalid for all configurations.
    const char kVP9Name[] = "WebM Project VP9";
    const bool is_vp9 = strncmp(kVP9Name, vpx_codec_iface_name(iface),
                                sizeof(kVP9Name) - 1) == 0;
    EXPECT_EQ(is_vp9 ? VPX_CODEC_INCAPABLE : VPX_CODEC_INVALID_PARAM,
              vpx_codec_enc_init_multi(&enc[0], iface, &cfg[0], 2, 0, &dsf[0]));

    for (int i = 0; i < 2; i++) {
      vpx_codec_destroy(&enc[i]);
    }
  }
}

}  // namespace
