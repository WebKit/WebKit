/*
 *  Copyright (c) 2016 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <climits>
#include <cstring>
#include <initializer_list>

#include "third_party/googletest/src/include/gtest/gtest.h"

#include "./vpx_config.h"
#include "test/video_source.h"
#include "vpx/vp8cx.h"
#include "vpx/vpx_encoder.h"

namespace {

const vpx_codec_iface_t *kCodecIfaces[] = {
#if CONFIG_VP8_ENCODER
  &vpx_codec_vp8_cx_algo,
#endif
#if CONFIG_VP9_ENCODER
  &vpx_codec_vp9_cx_algo,
#endif
};

bool IsVP9(const vpx_codec_iface_t *iface) {
  static const char kVP9Name[] = "WebM Project VP9";
  return strncmp(kVP9Name, vpx_codec_iface_name(iface), sizeof(kVP9Name) - 1) ==
         0;
}

TEST(EncodeAPI, InvalidParams) {
  uint8_t buf[1] = { 0 };
  vpx_image_t img;
  vpx_codec_ctx_t enc;
  vpx_codec_enc_cfg_t cfg;

  EXPECT_EQ(&img, vpx_img_wrap(&img, VPX_IMG_FMT_I420, 1, 1, 1, buf));

  EXPECT_EQ(VPX_CODEC_INVALID_PARAM,
            vpx_codec_enc_init(nullptr, nullptr, nullptr, 0));
  EXPECT_EQ(VPX_CODEC_INVALID_PARAM,
            vpx_codec_enc_init(&enc, nullptr, nullptr, 0));
  EXPECT_EQ(VPX_CODEC_INVALID_PARAM,
            vpx_codec_encode(nullptr, nullptr, 0, 0, 0, 0));
  EXPECT_EQ(VPX_CODEC_INVALID_PARAM,
            vpx_codec_encode(nullptr, &img, 0, 0, 0, 0));
  EXPECT_EQ(VPX_CODEC_INVALID_PARAM, vpx_codec_destroy(nullptr));
  EXPECT_EQ(VPX_CODEC_INVALID_PARAM,
            vpx_codec_enc_config_default(nullptr, nullptr, 0));
  EXPECT_EQ(VPX_CODEC_INVALID_PARAM,
            vpx_codec_enc_config_default(nullptr, &cfg, 0));
  EXPECT_NE(vpx_codec_error(nullptr), nullptr);

  for (const auto *iface : kCodecIfaces) {
    SCOPED_TRACE(vpx_codec_iface_name(iface));
    EXPECT_EQ(VPX_CODEC_INVALID_PARAM,
              vpx_codec_enc_init(nullptr, iface, nullptr, 0));
    EXPECT_EQ(VPX_CODEC_INVALID_PARAM,
              vpx_codec_enc_init(&enc, iface, nullptr, 0));
    EXPECT_EQ(VPX_CODEC_INVALID_PARAM,
              vpx_codec_enc_config_default(iface, &cfg, 1));

    EXPECT_EQ(VPX_CODEC_OK, vpx_codec_enc_config_default(iface, &cfg, 0));
    EXPECT_EQ(VPX_CODEC_OK, vpx_codec_enc_init(&enc, iface, &cfg, 0));
    EXPECT_EQ(VPX_CODEC_OK, vpx_codec_encode(&enc, nullptr, 0, 0, 0, 0));

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
  const int width = 1280;
  const int height = 720;
  const int width_down = width / 2;
  const int height_down = height / 2;
  const int target_bitrate = 1000;
  const int framerate = 30;

  for (const auto *iface : kCodecIfaces) {
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
    EXPECT_EQ(IsVP9(iface) ? VPX_CODEC_INCAPABLE : VPX_CODEC_INVALID_PARAM,
              vpx_codec_enc_init_multi(&enc[0], iface, &cfg[0], 2, 0, &dsf[0]));

    for (int i = 0; i < 2; i++) {
      vpx_codec_destroy(&enc[i]);
    }
  }
}

TEST(EncodeAPI, SetRoi) {
  static struct {
    const vpx_codec_iface_t *iface;
    int ctrl_id;
  } kCodecs[] = {
#if CONFIG_VP8_ENCODER
    { &vpx_codec_vp8_cx_algo, VP8E_SET_ROI_MAP },
#endif
#if CONFIG_VP9_ENCODER
    { &vpx_codec_vp9_cx_algo, VP9E_SET_ROI_MAP },
#endif
  };
  constexpr int kWidth = 64;
  constexpr int kHeight = 64;

  for (const auto &codec : kCodecs) {
    SCOPED_TRACE(vpx_codec_iface_name(codec.iface));
    vpx_codec_ctx_t enc;
    vpx_codec_enc_cfg_t cfg;

    EXPECT_EQ(vpx_codec_enc_config_default(codec.iface, &cfg, 0), VPX_CODEC_OK);
    cfg.g_w = kWidth;
    cfg.g_h = kHeight;
    EXPECT_EQ(vpx_codec_enc_init(&enc, codec.iface, &cfg, 0), VPX_CODEC_OK);

    vpx_roi_map_t roi = {};
    uint8_t roi_map[kWidth * kHeight] = {};
    if (IsVP9(codec.iface)) {
      roi.rows = (cfg.g_w + 7) >> 3;
      roi.cols = (cfg.g_h + 7) >> 3;
    } else {
      roi.rows = (cfg.g_w + 15) >> 4;
      roi.cols = (cfg.g_h + 15) >> 4;
    }
    EXPECT_EQ(vpx_codec_control_(&enc, codec.ctrl_id, &roi), VPX_CODEC_OK);

    roi.roi_map = roi_map;
    // VP8 only. This value isn't range checked.
    roi.static_threshold[1] = 1000;
    roi.static_threshold[2] = UINT_MAX / 2 + 1;
    roi.static_threshold[3] = UINT_MAX;

    for (const auto delta : { -63, -1, 0, 1, 63 }) {
      for (int i = 0; i < 8; ++i) {
        roi.delta_q[i] = delta;
        roi.delta_lf[i] = delta;
        // VP9 only.
        roi.skip[i] ^= 1;
        roi.ref_frame[i] = (roi.ref_frame[i] + 1) % 4;
        EXPECT_EQ(vpx_codec_control_(&enc, codec.ctrl_id, &roi), VPX_CODEC_OK);
      }
    }

    vpx_codec_err_t expected_error;
    for (const auto delta : { -64, 64, INT_MIN, INT_MAX }) {
      expected_error = VPX_CODEC_INVALID_PARAM;
      for (int i = 0; i < 8; ++i) {
        roi.delta_q[i] = delta;
        // The max segment count for VP8 is 4, the remainder of the entries are
        // ignored.
        if (i >= 4 && !IsVP9(codec.iface)) expected_error = VPX_CODEC_OK;

        EXPECT_EQ(vpx_codec_control_(&enc, codec.ctrl_id, &roi), expected_error)
            << "delta_q[" << i << "]: " << delta;
        roi.delta_q[i] = 0;

        roi.delta_lf[i] = delta;
        EXPECT_EQ(vpx_codec_control_(&enc, codec.ctrl_id, &roi), expected_error)
            << "delta_lf[" << i << "]: " << delta;
        roi.delta_lf[i] = 0;
      }
    }

    // VP8 should ignore skip[] and ref_frame[] values.
    expected_error =
        IsVP9(codec.iface) ? VPX_CODEC_INVALID_PARAM : VPX_CODEC_OK;
    for (const auto skip : { -2, 2, INT_MIN, INT_MAX }) {
      for (int i = 0; i < 8; ++i) {
        roi.skip[i] = skip;
        EXPECT_EQ(vpx_codec_control_(&enc, codec.ctrl_id, &roi), expected_error)
            << "skip[" << i << "]: " << skip;
        roi.skip[i] = 0;
      }
    }

    // VP9 allows negative values to be used to disable segmentation.
    for (int ref_frame = -3; ref_frame < 0; ++ref_frame) {
      for (int i = 0; i < 8; ++i) {
        roi.ref_frame[i] = ref_frame;
        EXPECT_EQ(vpx_codec_control_(&enc, codec.ctrl_id, &roi), VPX_CODEC_OK)
            << "ref_frame[" << i << "]: " << ref_frame;
        roi.ref_frame[i] = 0;
      }
    }

    for (const auto ref_frame : { 4, INT_MIN, INT_MAX }) {
      for (int i = 0; i < 8; ++i) {
        roi.ref_frame[i] = ref_frame;
        EXPECT_EQ(vpx_codec_control_(&enc, codec.ctrl_id, &roi), expected_error)
            << "ref_frame[" << i << "]: " << ref_frame;
        roi.ref_frame[i] = 0;
      }
    }

    EXPECT_EQ(vpx_codec_destroy(&enc), VPX_CODEC_OK);
  }
}

void InitCodec(const vpx_codec_iface_t &iface, int width, int height,
               vpx_codec_ctx_t *enc, vpx_codec_enc_cfg_t *cfg) {
  ASSERT_EQ(vpx_codec_enc_config_default(&iface, cfg, 0), VPX_CODEC_OK);
  cfg->g_w = width;
  cfg->g_h = height;
  cfg->g_lag_in_frames = 0;
  cfg->g_pass = VPX_RC_ONE_PASS;
  ASSERT_EQ(vpx_codec_enc_init(enc, &iface, cfg, 0), VPX_CODEC_OK);

  ASSERT_EQ(vpx_codec_control_(enc, VP8E_SET_CPUUSED, 2), VPX_CODEC_OK);
}

// Encodes 1 frame of size |cfg.g_w| x |cfg.g_h| setting |enc|'s configuration
// to |cfg|.
void EncodeWithConfig(const vpx_codec_enc_cfg_t &cfg, vpx_codec_ctx_t *enc) {
  libvpx_test::DummyVideoSource video;
  video.SetSize(cfg.g_w, cfg.g_h);
  video.Begin();
  EXPECT_EQ(vpx_codec_enc_config_set(enc, &cfg), VPX_CODEC_OK)
      << vpx_codec_error_detail(enc);

  EXPECT_EQ(vpx_codec_encode(enc, video.img(), video.pts(), video.duration(),
                             /*flags=*/0, VPX_DL_GOOD_QUALITY),
            VPX_CODEC_OK)
      << vpx_codec_error_detail(enc);
}

TEST(EncodeAPI, ConfigChangeThreadCount) {
  constexpr int kWidth = 1920;
  constexpr int kHeight = 1080;

  for (const auto *iface : kCodecIfaces) {
    SCOPED_TRACE(vpx_codec_iface_name(iface));
    for (int i = 0; i < (IsVP9(iface) ? 2 : 1); ++i) {
      vpx_codec_enc_cfg_t cfg = {};
      struct Encoder {
        ~Encoder() { EXPECT_EQ(vpx_codec_destroy(&ctx), VPX_CODEC_OK); }
        vpx_codec_ctx_t ctx = {};
      } enc;

      EXPECT_NO_FATAL_FAILURE(
          InitCodec(*iface, kWidth, kHeight, &enc.ctx, &cfg));
      if (IsVP9(iface)) {
        EXPECT_EQ(vpx_codec_control_(&enc.ctx, VP9E_SET_TILE_COLUMNS, 6),
                  VPX_CODEC_OK);
        EXPECT_EQ(vpx_codec_control_(&enc.ctx, VP9E_SET_ROW_MT, i),
                  VPX_CODEC_OK);
      }

      for (const auto threads : { 1, 4, 8, 6, 2, 1 }) {
        cfg.g_threads = threads;
        EXPECT_NO_FATAL_FAILURE(EncodeWithConfig(cfg, &enc.ctx))
            << "iteration: " << i << " threads: " << threads;
      }
    }
  }
}

}  // namespace
