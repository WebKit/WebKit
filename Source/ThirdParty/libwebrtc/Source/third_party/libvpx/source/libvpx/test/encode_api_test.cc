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
#include <new>

#include "third_party/googletest/src/include/gtest/gtest.h"
#include "test/codec_factory.h"
#include "test/encode_test_driver.h"
#include "test/i420_video_source.h"
#include "test/video_source.h"

#include "./vpx_config.h"
#include "vpx/vp8cx.h"
#include "vpx/vpx_tpl.h"

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

// Verifies the fix for a float-cast-overflow in vp8_change_config().
//
// Causes cpi->framerate to become the largest possible value (10,000,000) in
// VP8 by setting cfg.g_timebase to 1/10000000 and passing a duration of 1 to
// vpx_codec_encode().
TEST(EncodeAPI, HugeFramerateVp8) {
  vpx_codec_iface_t *const iface = vpx_codec_vp8_cx();
  vpx_codec_enc_cfg_t cfg;
  ASSERT_EQ(vpx_codec_enc_config_default(iface, &cfg, 0), VPX_CODEC_OK);
  cfg.g_w = 271;
  cfg.g_h = 1080;
  cfg.g_timebase.num = 1;
  // Largest value (VP8's TICKS_PER_SEC) such that frame duration is nonzero (1
  // tick).
  cfg.g_timebase.den = 10000000;
  cfg.g_pass = VPX_RC_ONE_PASS;
  cfg.g_lag_in_frames = 0;
  cfg.rc_end_usage = VPX_CBR;

  vpx_codec_ctx_t enc;
  // Before we encode the first frame, cpi->framerate is set to a guess (the
  // reciprocal of cfg.g_timebase). If this guess doesn't seem reasonable
  // (> 180), cpi->framerate is set to 30.
  ASSERT_EQ(vpx_codec_enc_init(&enc, iface, &cfg, 0), VPX_CODEC_OK);

  ASSERT_EQ(vpx_codec_control(&enc, VP8E_SET_CPUUSED, -12), VPX_CODEC_OK);

  vpx_image_t *const image =
      vpx_img_alloc(nullptr, VPX_IMG_FMT_I420, cfg.g_w, cfg.g_h, 1);
  ASSERT_NE(image, nullptr);

  for (unsigned int i = 0; i < image->d_h; ++i) {
    memset(image->planes[0] + i * image->stride[0], 128, image->d_w);
  }
  const unsigned int uv_h = (image->d_h + 1) / 2;
  const unsigned int uv_w = (image->d_w + 1) / 2;
  for (unsigned int i = 0; i < uv_h; ++i) {
    memset(image->planes[1] + i * image->stride[1], 128, uv_w);
    memset(image->planes[2] + i * image->stride[2], 128, uv_w);
  }

  // Encode a frame.
  const unsigned long deadline = VPX_DL_REALTIME;
  // Up to this point cpi->framerate is 30. Now pass a duration of only 1. This
  // causes cpi->framerate to become 10,000,000.
  ASSERT_EQ(vpx_codec_encode(&enc, image, 0, 1, 0, deadline), VPX_CODEC_OK);

  // Change to the same config. Since cpi->framerate is now huge, when it is
  // used to calculate raw_target_rate (bit rate of uncompressed frames), the
  // result is likely to overflow an unsigned int.
  ASSERT_EQ(vpx_codec_enc_config_set(&enc, &cfg), VPX_CODEC_OK);

  vpx_img_free(image);
  ASSERT_EQ(vpx_codec_destroy(&enc), VPX_CODEC_OK);
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

      ASSERT_EQ(vpx_codec_enc_config_default(iface, &cfg, 0), VPX_CODEC_OK);
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

TEST(EncodeAPI, ConfigResizeChangeThreadCount) {
  constexpr int kInitWidth = 1024;
  constexpr int kInitHeight = 1024;

  for (const auto *iface : kCodecIfaces) {
    SCOPED_TRACE(vpx_codec_iface_name(iface));
    for (int i = 0; i < (IsVP9(iface) ? 2 : 1); ++i) {
      vpx_codec_enc_cfg_t cfg = {};
      struct Encoder {
        ~Encoder() { EXPECT_EQ(vpx_codec_destroy(&ctx), VPX_CODEC_OK); }
        vpx_codec_ctx_t ctx = {};
      } enc;

      ASSERT_EQ(vpx_codec_enc_config_default(iface, &cfg, 0), VPX_CODEC_OK);
      // Start in threaded mode to ensure resolution and thread related
      // allocations are updated correctly across changes in resolution and
      // thread counts. See https://crbug.com/1486441.
      cfg.g_threads = 4;
      EXPECT_NO_FATAL_FAILURE(
          InitCodec(*iface, kInitWidth, kInitHeight, &enc.ctx, &cfg));
      if (IsVP9(iface)) {
        EXPECT_EQ(vpx_codec_control_(&enc.ctx, VP9E_SET_TILE_COLUMNS, 6),
                  VPX_CODEC_OK);
        EXPECT_EQ(vpx_codec_control_(&enc.ctx, VP9E_SET_ROW_MT, i),
                  VPX_CODEC_OK);
      }

      cfg.g_w = 1000;
      cfg.g_h = 608;
      EXPECT_EQ(vpx_codec_enc_config_set(&enc.ctx, &cfg), VPX_CODEC_OK)
          << vpx_codec_error_detail(&enc.ctx);

      cfg.g_w = 1000;
      cfg.g_h = 720;

      for (const auto threads : { 1, 4, 8, 6, 2, 1 }) {
        cfg.g_threads = threads;
        EXPECT_NO_FATAL_FAILURE(EncodeWithConfig(cfg, &enc.ctx))
            << "iteration: " << i << " threads: " << threads;
      }
    }
  }
}

#if CONFIG_VP9_ENCODER
#if VPX_ARCH_X86_64 || VPX_ARCH_AARCH64
TEST(EncodeAPI, ConfigLargeTargetBitrateVp9) {
  constexpr int kWidth = 16383;
  constexpr int kHeight = 16383;
  constexpr auto *iface = &vpx_codec_vp9_cx_algo;
  SCOPED_TRACE(vpx_codec_iface_name(iface));
  vpx_codec_enc_cfg_t cfg = {};
  struct Encoder {
    ~Encoder() { EXPECT_EQ(vpx_codec_destroy(&ctx), VPX_CODEC_OK); }
    vpx_codec_ctx_t ctx = {};
  } enc;

  ASSERT_EQ(vpx_codec_enc_config_default(iface, &cfg, 0), VPX_CODEC_OK);
  // The following setting will cause avg_frame_bandwidth in rate control to be
  // larger than INT_MAX
  cfg.rc_target_bitrate = INT_MAX;
  cfg.g_timebase.den = 1;
  cfg.g_timebase.num = 10;
  EXPECT_NO_FATAL_FAILURE(InitCodec(*iface, kWidth, kHeight, &enc.ctx, &cfg));
  EXPECT_NO_FATAL_FAILURE(EncodeWithConfig(cfg, &enc.ctx))
      << "target bitrate: " << cfg.rc_target_bitrate << " framerate: "
      << static_cast<double>(cfg.g_timebase.den) / cfg.g_timebase.num;
}
#endif  // VPX_ARCH_X86_64 || VPX_ARCH_AARCH64

class EncodeApiGetTplStatsTest
    : public ::libvpx_test::EncoderTest,
      public ::testing::TestWithParam<const libvpx_test::CodecFactory *> {
 public:
  EncodeApiGetTplStatsTest() : EncoderTest(GetParam()), test_io_(false) {}
  ~EncodeApiGetTplStatsTest() override = default;

 protected:
  void SetUp() override {
    InitializeConfig();
    SetMode(::libvpx_test::kTwoPassGood);
  }

  void PreEncodeFrameHook(::libvpx_test::VideoSource *video,
                          ::libvpx_test::Encoder *encoder) override {
    if (video->frame() == 0) {
      encoder->Control(VP9E_SET_TPL, 1);
    }
  }

  vpx_codec_err_t AllocateTplList(VpxTplGopStats *data) {
    // Allocate MAX_ARF_GOP_SIZE (50) * sizeof(VpxTplFrameStats) that will be
    // filled by VP9E_GET_TPL_STATS.
    // MAX_ARF_GOP_SIZE is used here because the test doesn't know the size of
    // each GOP before getting TPL stats from the encoder.
    data->size = 50;
    data->frame_stats_list =
        static_cast<VpxTplFrameStats *>(calloc(50, sizeof(VpxTplFrameStats)));
    if (data->frame_stats_list == nullptr) return VPX_CODEC_MEM_ERROR;
    return VPX_CODEC_OK;
  }

  void CompareTplGopStats(const VpxTplGopStats &ref_gop_stats,
                          const VpxTplGopStats &test_gop_stats) {
    ASSERT_EQ(ref_gop_stats.size, test_gop_stats.size);
    for (int frame = 0; frame < ref_gop_stats.size; frame++) {
      const VpxTplFrameStats &ref_frame_stats =
          ref_gop_stats.frame_stats_list[frame];
      const VpxTplFrameStats &test_frame_stats =
          test_gop_stats.frame_stats_list[frame];
      ASSERT_EQ(ref_frame_stats.num_blocks, test_frame_stats.num_blocks);
      ASSERT_EQ(ref_frame_stats.frame_width, test_frame_stats.frame_width);
      ASSERT_EQ(ref_frame_stats.frame_height, test_frame_stats.frame_height);
      for (int block = 0; block < ref_frame_stats.num_blocks; block++) {
        const VpxTplBlockStats &ref_block_stats =
            ref_frame_stats.block_stats_list[block];
        const VpxTplBlockStats &test_block_stats =
            test_frame_stats.block_stats_list[block];
        ASSERT_EQ(ref_block_stats.inter_cost, test_block_stats.inter_cost);
        ASSERT_EQ(ref_block_stats.intra_cost, test_block_stats.intra_cost);
        ASSERT_EQ(ref_block_stats.mv_c, test_block_stats.mv_c);
        ASSERT_EQ(ref_block_stats.mv_r, test_block_stats.mv_r);
        ASSERT_EQ(ref_block_stats.recrf_dist, test_block_stats.recrf_dist);
        ASSERT_EQ(ref_block_stats.recrf_rate, test_block_stats.recrf_rate);
        ASSERT_EQ(ref_block_stats.ref_frame_index,
                  test_block_stats.ref_frame_index);
      }
    }
  }

  void PostEncodeFrameHook(::libvpx_test::Encoder *encoder) override {
    ::libvpx_test::CxDataIterator iter = encoder->GetCxData();
    while (const vpx_codec_cx_pkt_t *pkt = iter.Next()) {
      switch (pkt->kind) {
        case VPX_CODEC_CX_FRAME_PKT: {
          VpxTplGopStats tpl_stats;
          EXPECT_EQ(AllocateTplList(&tpl_stats), VPX_CODEC_OK);
          encoder->Control(VP9E_GET_TPL_STATS, &tpl_stats);
          bool stats_not_all_zero = false;
          for (int i = 0; i < tpl_stats.size; i++) {
            VpxTplFrameStats *frame_stats_list = tpl_stats.frame_stats_list;
            if (frame_stats_list[i].frame_width != 0) {
              ASSERT_EQ(frame_stats_list[i].frame_width, width_);
              ASSERT_EQ(frame_stats_list[i].frame_height, height_);
              ASSERT_GT(frame_stats_list[i].num_blocks, 0);
              ASSERT_NE(frame_stats_list[i].block_stats_list, nullptr);
              stats_not_all_zero = true;
            }
          }
          ASSERT_TRUE(stats_not_all_zero);
          if (test_io_ && tpl_stats.size > 0) {
            libvpx_test::TempOutFile *temp_out_file =
                new (std::nothrow) libvpx_test::TempOutFile("w+");
            ASSERT_NE(temp_out_file, nullptr);
            ASSERT_NE(temp_out_file->file(), nullptr);
            vpx_write_tpl_gop_stats(temp_out_file->file(), &tpl_stats);
            rewind(temp_out_file->file());
            VpxTplGopStats gop_stats_io;
            ASSERT_EQ(
                vpx_read_tpl_gop_stats(temp_out_file->file(), &gop_stats_io),
                VPX_CODEC_OK);
            CompareTplGopStats(gop_stats_io, tpl_stats);
            vpx_free_tpl_gop_stats(&gop_stats_io);
            delete temp_out_file;
          }
          free(tpl_stats.frame_stats_list);
          break;
        }
        default: break;
      }
    }
  }

  int width_;
  int height_;
  bool test_io_;
};

TEST_P(EncodeApiGetTplStatsTest, GetTplStats) {
  cfg_.g_lag_in_frames = 25;
  width_ = 352;
  height_ = 288;
  ::libvpx_test::I420VideoSource video("hantro_collage_w352h288.yuv", width_,
                                       height_, 30, 1, 0, 50);
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
}

TEST_P(EncodeApiGetTplStatsTest, GetTplStatsIO) {
  cfg_.g_lag_in_frames = 25;
  width_ = 352;
  height_ = 288;
  test_io_ = true;
  ::libvpx_test::I420VideoSource video("hantro_collage_w352h288.yuv", width_,
                                       height_, 30, 1, 0, 50);
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
}

INSTANTIATE_TEST_SUITE_P(
    VP9, EncodeApiGetTplStatsTest,
    ::testing::Values(
        static_cast<const libvpx_test::CodecFactory *>(&libvpx_test::kVP9)));
#endif  // CONFIG_VP9_ENCODER

}  // namespace
