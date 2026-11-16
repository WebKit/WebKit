/*
 *  Copyright (c) 2014 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <array>
#include <vector>

#include "gtest/gtest.h"

#include "./vpx_config.h"
#if CONFIG_VP8_DECODER && CONFIG_MULTITHREAD && defined(__linux__)
#include <sys/resource.h>
#endif
#include "test/ivf_video_source.h"
#include "test/video_source.h"
#if CONFIG_VP8_ENCODER || CONFIG_VP9_ENCODER
#include "vpx/vp8cx.h"
#endif
#include "vpx/vp8dx.h"
#include "vpx/vpx_decoder.h"
#include "vpx/vpx_encoder.h"

namespace {

#define NELEMENTS(x) static_cast<int>(sizeof(x) / sizeof(x[0]))

TEST(DecodeAPI, InvalidParams) {
  static vpx_codec_iface_t *kCodecs[] = {
#if CONFIG_VP8_DECODER
    &vpx_codec_vp8_dx_algo,
#endif
#if CONFIG_VP9_DECODER
    &vpx_codec_vp9_dx_algo,
#endif
  };
  uint8_t buf[1] = { 0 };
  vpx_codec_ctx_t dec;

  EXPECT_EQ(vpx_codec_dec_init(nullptr, nullptr, nullptr, 0),
            VPX_CODEC_INVALID_PARAM);
  EXPECT_EQ(vpx_codec_dec_init(&dec, nullptr, nullptr, 0),
            VPX_CODEC_INVALID_PARAM);
  EXPECT_EQ(vpx_codec_decode(nullptr, nullptr, 0, nullptr, 0),
            VPX_CODEC_INVALID_PARAM);
  EXPECT_EQ(vpx_codec_decode(nullptr, buf, 0, nullptr, 0),
            VPX_CODEC_INVALID_PARAM);
  EXPECT_EQ(vpx_codec_decode(nullptr, buf, NELEMENTS(buf), nullptr, 0),
            VPX_CODEC_INVALID_PARAM);
  EXPECT_EQ(vpx_codec_decode(nullptr, nullptr, NELEMENTS(buf), nullptr, 0),
            VPX_CODEC_INVALID_PARAM);
  EXPECT_EQ(vpx_codec_destroy(nullptr), VPX_CODEC_INVALID_PARAM);
  EXPECT_NE(vpx_codec_error(nullptr), nullptr);
  EXPECT_EQ(vpx_codec_error_detail(nullptr), nullptr);

  for (int i = 0; i < NELEMENTS(kCodecs); ++i) {
    EXPECT_EQ(VPX_CODEC_INVALID_PARAM,
              vpx_codec_dec_init(nullptr, kCodecs[i], nullptr, 0));

    EXPECT_EQ(VPX_CODEC_OK, vpx_codec_dec_init(&dec, kCodecs[i], nullptr, 0));
    EXPECT_EQ(VPX_CODEC_UNSUP_BITSTREAM,
              vpx_codec_decode(&dec, buf, NELEMENTS(buf), nullptr, 0));
    EXPECT_EQ(VPX_CODEC_INVALID_PARAM,
              vpx_codec_decode(&dec, nullptr, NELEMENTS(buf), nullptr, 0));
    EXPECT_EQ(VPX_CODEC_INVALID_PARAM,
              vpx_codec_decode(&dec, buf, 0, nullptr, 0));

    EXPECT_EQ(VPX_CODEC_OK, vpx_codec_destroy(&dec));
  }
}

#if CONFIG_VP8_DECODER
TEST(DecodeAPI, OptionalParams) {
  vpx_codec_ctx_t dec;

#if CONFIG_ERROR_CONCEALMENT
  EXPECT_EQ(VPX_CODEC_OK,
            vpx_codec_dec_init(&dec, &vpx_codec_vp8_dx_algo, nullptr,
                               VPX_CODEC_USE_ERROR_CONCEALMENT));
#else
  EXPECT_EQ(VPX_CODEC_INCAPABLE,
            vpx_codec_dec_init(&dec, &vpx_codec_vp8_dx_algo, nullptr,
                               VPX_CODEC_USE_ERROR_CONCEALMENT));
#endif  // CONFIG_ERROR_CONCEALMENT
}

TEST(DecodeAPI, Vp8FlushAfterFragmentError) {
  static constexpr uint8_t kInvalidKeyframe[10] = { 0 };
  vpx_codec_ctx_t dec;

  ASSERT_EQ(VPX_CODEC_OK,
            vpx_codec_dec_init(&dec, &vpx_codec_vp8_dx_algo, nullptr,
                               VPX_CODEC_USE_INPUT_FRAGMENTS));
  EXPECT_EQ(VPX_CODEC_OK,
            vpx_codec_decode(&dec, kInvalidKeyframe, sizeof(kInvalidKeyframe),
                             nullptr, 0));
  EXPECT_EQ(VPX_CODEC_UNSUP_BITSTREAM,
            vpx_codec_decode(&dec, nullptr, 0, nullptr, 0));
  EXPECT_EQ(VPX_CODEC_OK, vpx_codec_decode(&dec, nullptr, 0, nullptr, 0));
  EXPECT_EQ(VPX_CODEC_OK, vpx_codec_destroy(&dec));
}

#if CONFIG_SIZE_LIMIT && \
    (DECODE_WIDTH_LIMIT < 16383 || DECODE_HEIGHT_LIMIT < 16383)
TEST(DecodeAPI, Vp8RejectsOversizedKeyframeBeforeInitialization) {
  vpx_codec_ctx_t dec;
  ASSERT_EQ(VPX_CODEC_OK,
            vpx_codec_dec_init(&dec, &vpx_codec_vp8_dx_algo, nullptr, 0));

#if DECODE_WIDTH_LIMIT < 16383
  constexpr uint16_t kWidth = DECODE_WIDTH_LIMIT + 1;
  constexpr uint16_t kHeight = 1;
#else
  constexpr uint16_t kWidth = 1;
  constexpr uint16_t kHeight = DECODE_HEIGHT_LIMIT + 1;
#endif
  static constexpr uint8_t kOversizedKeyframe[10] = {
    0x00,
    0x00,
    0x00,
    0x9d,
    0x01,
    0x2a,
    static_cast<uint8_t>(kWidth),
    static_cast<uint8_t>(kWidth >> 8),
    static_cast<uint8_t>(kHeight),
    static_cast<uint8_t>(kHeight >> 8),
  };
  EXPECT_EQ(
      VPX_CODEC_CORRUPT_FRAME,
      vpx_codec_decode(&dec, kOversizedKeyframe,
                       static_cast<unsigned int>(sizeof(kOversizedKeyframe)),
                       nullptr, 0));

  static constexpr uint8_t kInterframe[10] = { 0x01 };
  EXPECT_EQ(VPX_CODEC_UNSUP_BITSTREAM,
            vpx_codec_decode(&dec, kInterframe,
                             static_cast<unsigned int>(sizeof(kInterframe)),
                             nullptr, 0));
  EXPECT_EQ(VPX_CODEC_OK, vpx_codec_destroy(&dec));
}
#endif  // CONFIG_SIZE_LIMIT && dimension limit is representable in VP8

TEST(DecodeAPI, Vp8FlushWithNoFragments) {
  vpx_codec_ctx_t dec;
  vpx_codec_dec_cfg_t cfg = { 1, 0, 0 };
  vpx_codec_flags_t flags = VPX_CODEC_USE_INPUT_FRAGMENTS;

  EXPECT_EQ(VPX_CODEC_OK,
            vpx_codec_dec_init(&dec, &vpx_codec_vp8_dx_algo, &cfg, flags));
  EXPECT_EQ(VPX_CODEC_OK, vpx_codec_decode(&dec, nullptr, 0, nullptr, 0));
  EXPECT_EQ(VPX_CODEC_OK, vpx_codec_destroy(&dec));
}

#if CONFIG_VP8_ENCODER && CONFIG_MULTITHREAD
// Encodes a sequence of key frames using two token partitions, truncates the
// second token partition of one frame so that the worker thread's bool decoder
// runs past the end of its partition, and then verifies that the
// multi-threaded decoder still reports subsequent well-formed key frames as
// not corrupted.
TEST(DecodeAPI, Vp8MultiThreadedCorruptedStateResetAcrossFrames) {
  constexpr int kWidth = 16;
  constexpr int kHeight = 32;
  constexpr int kFrames = 4;

  std::vector<std::vector<uint8_t>> frames;
  {
    vpx_codec_ctx_t enc;
    vpx_codec_enc_cfg_t cfg;
    ASSERT_EQ(vpx_codec_enc_config_default(&vpx_codec_vp8_cx_algo, &cfg, 0),
              VPX_CODEC_OK);
    cfg.g_w = kWidth;
    cfg.g_h = kHeight;
    cfg.g_lag_in_frames = 0;
    ASSERT_EQ(vpx_codec_enc_init(&enc, &vpx_codec_vp8_cx_algo, &cfg, 0),
              VPX_CODEC_OK);
    ASSERT_EQ(vpx_codec_control(&enc, VP8E_SET_TOKEN_PARTITIONS,
                                VP8_TWO_TOKENPARTITION),
              VPX_CODEC_OK);

    libvpx_test::RandomVideoSource video;
    video.SetSize(kWidth, kHeight);
    video.set_limit(kFrames);
    for (video.Begin(); video.img() != nullptr; video.Next()) {
      ASSERT_EQ(
          vpx_codec_encode(&enc, video.img(), video.pts(), video.duration(),
                           VPX_EFLAG_FORCE_KF, VPX_DL_REALTIME),
          VPX_CODEC_OK);
      vpx_codec_iter_t iter = nullptr;
      const vpx_codec_cx_pkt_t *pkt;
      while ((pkt = vpx_codec_get_cx_data(&enc, &iter)) != nullptr) {
        if (pkt->kind != VPX_CODEC_CX_FRAME_PKT) continue;
        ASSERT_NE(pkt->data.frame.flags & VPX_FRAME_IS_KEY, 0u);
        const uint8_t *buf = static_cast<const uint8_t *>(pkt->data.frame.buf);
        frames.emplace_back(buf, buf + pkt->data.frame.sz);
      }
    }
    ASSERT_EQ(vpx_codec_destroy(&enc), VPX_CODEC_OK);
  }
  ASSERT_EQ(frames.size(), static_cast<size_t>(kFrames));

  // Truncate frame 1 so that only a single byte of the second token partition
  // remains. The first token partition (used by the main thread) is left
  // intact.
  {
    std::vector<uint8_t> &f = frames[1];
    ASSERT_GT(f.size(), 10u);
    const size_t first_part_sz =
        (static_cast<size_t>(f[0]) | (static_cast<size_t>(f[1]) << 8) |
         (static_cast<size_t>(f[2]) << 16)) >>
        5;
    const size_t part_sizes = 10 + first_part_sz;
    ASSERT_LT(part_sizes + 3, f.size());
    const size_t part0_sz = static_cast<size_t>(f[part_sizes]) |
                            (static_cast<size_t>(f[part_sizes + 1]) << 8) |
                            (static_cast<size_t>(f[part_sizes + 2]) << 16);
    const size_t part1_start = part_sizes + 3 + part0_sz;
    ASSERT_LT(part1_start, f.size());
    f.resize(part1_start + 1);
  }

  vpx_codec_ctx_t dec;
  vpx_codec_dec_cfg_t dec_cfg = { /*threads=*/4, /*w=*/0, /*h=*/0 };
  ASSERT_EQ(vpx_codec_dec_init(&dec, &vpx_codec_vp8_dx_algo, &dec_cfg, 0),
            VPX_CODEC_OK);
  for (int i = 0; i < kFrames; ++i) {
    const vpx_codec_err_t res = vpx_codec_decode(
        &dec, frames[i].data(), static_cast<unsigned int>(frames[i].size()),
        /*user_priv=*/nullptr, /*deadline=*/0);
    vpx_codec_iter_t iter = nullptr;
    while (vpx_codec_get_frame(&dec, &iter)) {
    }
    if (i == 1) continue;
    EXPECT_EQ(res, VPX_CODEC_OK)
        << "frame " << i << ": " << vpx_codec_error_detail(&dec);
    int corrupted = -1;
    EXPECT_EQ(vpx_codec_control(&dec, VP8D_GET_FRAME_CORRUPTED, &corrupted),
              VPX_CODEC_OK);
    EXPECT_EQ(corrupted, 0) << "frame " << i;
  }
  EXPECT_EQ(vpx_codec_destroy(&dec), VPX_CODEC_OK);
}

// Issue: 559707641
// Encodes a sequence of frames with two token partitions, truncates the second
// token partition of one frame (used by a worker thread), and verifies that
// the multi-threaded decoder returns VPX_CODEC_CORRUPT_FRAME and does not emit
// a partially written frame. Also verifies that subsequent well-formed frames
// still decode correctly.
TEST(DecodeAPI, Vp8MultiThreadedWorkerDecodeErrorFailsFrame) {
  constexpr int kWidth = 32;
  constexpr int kHeight = 32;
  class PatternVideoSource : public libvpx_test::DummyVideoSource {
   protected:
    void FillFrame() override {
      if (!img_) return;
      // Keep row 0 flat so partition 0 size is small.
      memset(img_->planes[VPX_PLANE_Y], 128, img_->stride[VPX_PLANE_Y] * 16);
      // Fill row 1 with varying pattern so partition 1 has tokens to truncate.
      for (int y = 16; y < 32; ++y) {
        uint8_t *row =
            img_->planes[VPX_PLANE_Y] + y * img_->stride[VPX_PLANE_Y];
        for (int x = 0; x < 32; ++x) {
          row[x] = static_cast<uint8_t>((x * 37 + y * 73) ^ (frame_ * 19));
        }
      }
      memset(img_->planes[VPX_PLANE_U], 128, img_->stride[VPX_PLANE_U] * 16);
      memset(img_->planes[VPX_PLANE_V], 128, img_->stride[VPX_PLANE_V] * 16);
    }
  };

  constexpr int kFrames = 4;
  std::vector<std::vector<uint8_t>> frames;
  {
    vpx_codec_ctx_t enc;
    vpx_codec_enc_cfg_t cfg;
    ASSERT_EQ(vpx_codec_enc_config_default(&vpx_codec_vp8_cx_algo, &cfg, 0),
              VPX_CODEC_OK);
    cfg.g_w = kWidth;
    cfg.g_h = kHeight;
    cfg.g_lag_in_frames = 0;
    ASSERT_EQ(vpx_codec_enc_init(&enc, &vpx_codec_vp8_cx_algo, &cfg, 0),
              VPX_CODEC_OK);
    ASSERT_EQ(vpx_codec_control(&enc, VP8E_SET_TOKEN_PARTITIONS,
                                VP8_TWO_TOKENPARTITION),
              VPX_CODEC_OK);

    PatternVideoSource video;
    video.SetSize(kWidth, kHeight);
    video.set_limit(kFrames);
    for (video.Begin(); video.img() != nullptr; video.Next()) {
      ASSERT_EQ(
          vpx_codec_encode(&enc, video.img(), video.pts(), video.duration(),
                           VPX_EFLAG_FORCE_KF, VPX_DL_REALTIME),
          VPX_CODEC_OK);
      vpx_codec_iter_t iter = nullptr;
      const vpx_codec_cx_pkt_t *pkt;
      while ((pkt = vpx_codec_get_cx_data(&enc, &iter)) != nullptr) {
        if (pkt->kind != VPX_CODEC_CX_FRAME_PKT) continue;
        ASSERT_NE(pkt->data.frame.flags & VPX_FRAME_IS_KEY, 0u);
        const uint8_t *buf = static_cast<const uint8_t *>(pkt->data.frame.buf);
        frames.emplace_back(buf, buf + pkt->data.frame.sz);
      }
    }
    ASSERT_EQ(vpx_codec_destroy(&enc), VPX_CODEC_OK);
  }
  ASSERT_EQ(frames.size(), static_cast<size_t>(kFrames));

  // Truncate frame 1 so that only a single byte of the second token partition
  // remains. The first token partition (used by the main thread) is left
  // intact.
  {
    std::vector<uint8_t> &f = frames[1];
    ASSERT_GT(f.size(), 10u);
    const size_t first_part_sz =
        (static_cast<size_t>(f[0]) | (static_cast<size_t>(f[1]) << 8) |
         (static_cast<size_t>(f[2]) << 16)) >>
        5;
    const size_t part_sizes = 10 + first_part_sz;
    ASSERT_LT(part_sizes + 3, f.size());
    const size_t part0_sz = static_cast<size_t>(f[part_sizes]) |
                            (static_cast<size_t>(f[part_sizes + 1]) << 8) |
                            (static_cast<size_t>(f[part_sizes + 2]) << 16);
    const size_t part1_start = part_sizes + 3 + part0_sz;
    ASSERT_LT(part1_start, f.size());
    f.resize(part1_start + 1);
  }

  vpx_codec_ctx_t dec;
  vpx_codec_dec_cfg_t dec_cfg = { /*threads=*/4, /*w=*/0, /*h=*/0 };
  ASSERT_EQ(vpx_codec_dec_init(&dec, &vpx_codec_vp8_dx_algo, &dec_cfg, 0),
            VPX_CODEC_OK);
  for (int i = 0; i < kFrames; ++i) {
    const vpx_codec_err_t res = vpx_codec_decode(
        &dec, frames[i].data(), static_cast<unsigned int>(frames[i].size()),
        /*user_priv=*/nullptr, /*deadline=*/0);
    vpx_codec_iter_t iter = nullptr;
    const vpx_image_t *img = vpx_codec_get_frame(&dec, &iter);
    if (i == 1) {
      EXPECT_EQ(res, VPX_CODEC_CORRUPT_FRAME);
      EXPECT_EQ(img, nullptr);
      continue;
    }
    EXPECT_EQ(res, VPX_CODEC_OK)
        << "frame " << i << ": " << vpx_codec_error_detail(&dec);
    EXPECT_NE(img, nullptr) << "frame " << i;
    int corrupted = -1;
    EXPECT_EQ(vpx_codec_control(&dec, VP8D_GET_FRAME_CORRUPTED, &corrupted),
              VPX_CODEC_OK);
    EXPECT_EQ(corrupted, 0) << "frame " << i;
  }
  EXPECT_EQ(vpx_codec_destroy(&dec), VPX_CODEC_OK);
}
#endif  // CONFIG_VP8_ENCODER && CONFIG_MULTITHREAD

#if CONFIG_MULTITHREAD && defined(__linux__)
// Issue: 571147583
// Verifies that when restarting decode threads fails on a keyframe with a new
// resolution, the cached dimensions (ctx->si.w, ctx->si.h) are cleared so a
// subsequent decode triggers a resolution change and reallocates frame buffers.
TEST(DecodeAPI, Vp8MultiThreadedRestartResolutionChange) {
  // Minimal 2-partition VP8 keyframe buffer (first partition = 256 bytes).
  std::vector<uint8_t> frame(1500, 0);
  frame[0] = 0x10;
  frame[1] = 0x20;
  frame[3] = 0x9d;
  frame[4] = 0x01;
  frame[5] = 0x2a;
  frame[6] = 0x00;  // width = 256
  frame[7] = 0x01;
  frame[8] = 0x00;  // height = 256
  frame[9] = 0x01;
  frame[10] = 0x0f;  // 2 token partitions
  frame[11] = 0xc1;
  frame[12] = 0x28;
  frame[266] = 0x58;  // token partition 0 size = 600
  frame[267] = 0x02;

  vpx_codec_ctx_t dec;
  vpx_codec_dec_cfg_t cfg = { /*threads=*/2, /*w=*/0, /*h=*/0 };
  ASSERT_EQ(vpx_codec_dec_init(&dec, &vpx_codec_vp8_dx_algo, &cfg, 0),
            VPX_CODEC_OK);

  // Frame 0: valid 256x256 keyframe initializes 256x256 frame buffers
  // (yv12_fb_new->y_width = 256, pc->Width = 256) and worker threads.
  ASSERT_EQ(
      vpx_codec_decode(&dec, frame.data(),
                       static_cast<unsigned int>(frame.size()), nullptr, 0),
      VPX_CODEC_OK);

  // Frame 1: truncate token partition 0 to 1 byte so the worker thread fails in
  // vp8mt_decode_mb_rows(), shutting down threads and setting
  // ctx->restart_threads = 1.
  frame[266] = 1;
  frame[267] = 0;
  EXPECT_EQ(
      vpx_codec_decode(&dec, frame.data(),
                       static_cast<unsigned int>(frame.size()), nullptr, 0),
      VPX_CODEC_CORRUPT_FRAME);

  // Change keyframe header to 16x64. In each iteration:
  // 1. Set RLIMIT_NPROC = 0 so pthread_create fails inside restart_threads and
  //    vpx_codec_decode returns VPX_CODEC_ERROR after vp8_peek_si_internal()
  //    has updated ctx->si.w = 16, ctx->si.h = 64.
  // 2. Restore RLIMIT_NPROC and decode again so restart_threads succeeds and
  //    allocates mt_yabove_row for pc->Width.
  //
  // Without the fix:
  // - ctx->si.w = 16, ctx->si.h = 64 remain cached after step 1, so step 2
  //   sees resolution_change == 0 and skips vp8_alloc_frame_buffers()
  //   (leaving yv12_fb_new->y_width = 256).
  // - In iteration 0 step 2, vp8_decode_frame() sets pc->Width = 16 from the
  //   keyframe header before failing on the truncated token partition
  //   (setting ctx->restart_threads = 1 again).
  // - In iteration 1 step 2, restart_threads allocates mt_yabove_row for
  //   pc->Width = 16 (103 bytes) while yv12_fb_new->y_width is still 256,
  //   causing a 261-byte write (heap-buffer-overflow) in
  //   vp8mt_decode_mb_rows().
  //
  // With the fix:
  // - Step 1 clears ctx->si.w = 0, ctx->si.h = 0 on thread restart failure.
  // - Step 2 detects resolution_change == 1, reallocates frame buffers to
  //   16x64, and cleanly returns VPX_CODEC_CORRUPT_FRAME on the truncated
  //   partition.
  frame[6] = 16;
  frame[7] = 0;
  frame[8] = 64;
  frame[9] = 0;
  struct rlimit old_lim;
  ASSERT_EQ(getrlimit(RLIMIT_NPROC, &old_lim), 0);
  const struct rlimit zero_lim = { 0, old_lim.rlim_max };
  for (int i = 0; i < 2; ++i) {
    ASSERT_EQ(setrlimit(RLIMIT_NPROC, &zero_lim), 0);
    const vpx_codec_err_t res =
        vpx_codec_decode(&dec, frame.data(),
                         static_cast<unsigned int>(frame.size()), nullptr, 0);
    ASSERT_EQ(setrlimit(RLIMIT_NPROC, &old_lim), 0);
    EXPECT_EQ(res, VPX_CODEC_ERROR);

    EXPECT_EQ(
        vpx_codec_decode(&dec, frame.data(),
                         static_cast<unsigned int>(frame.size()), nullptr, 0),
        VPX_CODEC_CORRUPT_FRAME);
  }

  EXPECT_EQ(vpx_codec_destroy(&dec), VPX_CODEC_OK);
}
#endif  // CONFIG_MULTITHREAD && defined(__linux__)
#endif  // CONFIG_VP8_DECODER

#if CONFIG_VP9_DECODER
// Test VP9 codec controls after a decode error to ensure the code doesn't
// misbehave.
void TestVp9Controls(vpx_codec_ctx_t *dec) {
  static const int kControls[] = { VP8D_GET_LAST_REF_UPDATES,
                                   VP8D_GET_FRAME_CORRUPTED,
                                   VP9D_GET_DISPLAY_SIZE, VP9D_GET_FRAME_SIZE };
  int val[2];

  for (int i = 0; i < NELEMENTS(kControls); ++i) {
    const vpx_codec_err_t res = vpx_codec_control_(dec, kControls[i], val);
    switch (kControls[i]) {
      case VP8D_GET_FRAME_CORRUPTED:
        EXPECT_EQ(VPX_CODEC_ERROR, res) << kControls[i];
        break;
      default: EXPECT_EQ(VPX_CODEC_OK, res) << kControls[i]; break;
    }
    EXPECT_EQ(VPX_CODEC_INVALID_PARAM,
              vpx_codec_control_(dec, kControls[i], nullptr));
  }

  vp9_ref_frame_t ref;
  ref.idx = 0;
  EXPECT_EQ(VPX_CODEC_ERROR, vpx_codec_control(dec, VP9_GET_REFERENCE, &ref));
  EXPECT_EQ(VPX_CODEC_INVALID_PARAM,
            vpx_codec_control(dec, VP9_GET_REFERENCE, nullptr));

  vpx_ref_frame_t ref_copy;
  const int width = 352;
  const int height = 288;
  EXPECT_NE(vpx_img_alloc(&ref_copy.img, VPX_IMG_FMT_I420, width, height, 1),
            nullptr);
  ref_copy.frame_type = VP8_LAST_FRAME;
  EXPECT_EQ(VPX_CODEC_ERROR,
            vpx_codec_control(dec, VP8_COPY_REFERENCE, &ref_copy));
  EXPECT_EQ(VPX_CODEC_INVALID_PARAM,
            vpx_codec_control(dec, VP8_COPY_REFERENCE, nullptr));
  vpx_img_free(&ref_copy.img);
}

TEST(DecodeAPI, Vp9InvalidDecode) {
  vpx_codec_iface_t *const codec = &vpx_codec_vp9_dx_algo;
  const char filename[] =
      "invalid-vp90-2-00-quantizer-00.webm.ivf.s5861_r01-05_b6-.v2.ivf";
  libvpx_test::IVFVideoSource video(filename);
  video.Init();
  video.Begin();
  ASSERT_TRUE(!HasFailure());

  vpx_codec_ctx_t dec;
  EXPECT_EQ(VPX_CODEC_OK, vpx_codec_dec_init(&dec, codec, nullptr, 0));
  const uint32_t frame_size = static_cast<uint32_t>(video.frame_size());
#if CONFIG_VP9_HIGHBITDEPTH
  EXPECT_EQ(VPX_CODEC_MEM_ERROR,
            vpx_codec_decode(&dec, video.cxdata(), frame_size, nullptr, 0));
#else
  EXPECT_EQ(VPX_CODEC_UNSUP_BITSTREAM,
            vpx_codec_decode(&dec, video.cxdata(), frame_size, nullptr, 0));
#endif
  vpx_codec_iter_t iter = nullptr;
  EXPECT_EQ(nullptr, vpx_codec_get_frame(&dec, &iter));

  TestVp9Controls(&dec);
  EXPECT_EQ(VPX_CODEC_OK, vpx_codec_destroy(&dec));
}

void TestPeekInfo(const uint8_t *const data, uint32_t data_sz,
                  uint32_t peek_size) {
  vpx_codec_iface_t *const codec = &vpx_codec_vp9_dx_algo;
  // Verify behavior of vpx_codec_decode. vpx_codec_decode doesn't even get
  // to decoder_peek_si_internal on frames of size < 8.
  if (data_sz >= 8) {
    vpx_codec_ctx_t dec;
    EXPECT_EQ(VPX_CODEC_OK, vpx_codec_dec_init(&dec, codec, nullptr, 0));
    EXPECT_EQ((data_sz < peek_size) ? VPX_CODEC_UNSUP_BITSTREAM
                                    : VPX_CODEC_CORRUPT_FRAME,
              vpx_codec_decode(&dec, data, data_sz, nullptr, 0));
    vpx_codec_iter_t iter = nullptr;
    EXPECT_EQ(nullptr, vpx_codec_get_frame(&dec, &iter));
    EXPECT_EQ(VPX_CODEC_OK, vpx_codec_destroy(&dec));
  }

  // Verify behavior of vpx_codec_peek_stream_info.
  vpx_codec_stream_info_t si;
  si.sz = sizeof(si);
  EXPECT_EQ((data_sz < peek_size) ? VPX_CODEC_UNSUP_BITSTREAM : VPX_CODEC_OK,
            vpx_codec_peek_stream_info(codec, data, data_sz, &si));
}

TEST(DecodeAPI, Vp9PeekStreamInfo) {
  // The first 9 bytes are valid and the rest of the bytes are made up. Until
  // size 10, this should return VPX_CODEC_UNSUP_BITSTREAM and after that it
  // should return VPX_CODEC_CORRUPT_FRAME.
  const uint8_t data[32] = {
    0x85, 0xa4, 0xc1, 0xa1, 0x38, 0x81, 0xa3, 0x49, 0x83, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  };

  for (uint32_t data_sz = 1; data_sz <= 32; ++data_sz) {
    TestPeekInfo(data, data_sz, 10);
  }
}

TEST(DecodeAPI, Vp9PeekStreamInfoTruncated) {
  // This profile 1 header requires 10.25 bytes, ensure
  // vpx_codec_peek_stream_info doesn't over read.
  const uint8_t profile1_data[10] = { 0xa4, 0xe9, 0x30, 0x68, 0x53,
                                      0xe9, 0x30, 0x68, 0x53, 0x04 };

  for (uint32_t data_sz = 1; data_sz <= 10; ++data_sz) {
    TestPeekInfo(profile1_data, data_sz, 11);
  }
}

TEST(DecodeAPI, Buganizer499206650) {
  vpx_codec_ctx_t dec;
  ASSERT_EQ(vpx_codec_dec_init(&dec, vpx_codec_vp9_dx(), /*cfg=*/nullptr,
                               /*flags=*/0),
            VPX_CODEC_OK);

  // Frame 1: VP9 intra_only, profile 0, 8x8, error_resilient=1,
  // refresh_frame_flags=0x00. Clears need_resync without updating ref slots.
  static constexpr std::array<uint8_t, 83> frame1 = {
    0x85, 0xa4, 0xc1, 0xa1, 0x00, 0x00, 0x03, 0x80, 0x03, 0x80, 0x00, 0x40,
    0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  };
  EXPECT_EQ(vpx_codec_decode(&dec, frame1.data(),
                             static_cast<unsigned int>(frame1.size()),
                             /*user_priv=*/nullptr, /*deadline=*/0),
            VPX_CODEC_OK)
      << vpx_codec_error_detail(&dec);

  vpx_codec_iter_t it = nullptr;
  while (vpx_codec_get_frame(&dec, &it)) {
  }

  // Frame 2: VP9 inter, profile 0, show_frame=1, error_resilient=1,
  // refresh=0xFF, references slot 0 (ref_frame_map[0] == -1).
  static constexpr std::array<uint8_t, 16> frame2 = {
    0x87, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  };
  // Ensure there is no out of bounds read using the invalid index and that the
  // decoder reports an error.
  EXPECT_EQ(vpx_codec_decode(&dec, frame2.data(),
                             static_cast<unsigned int>(frame2.size()),
                             /*user_priv=*/nullptr, /*deadline=*/0),
            VPX_CODEC_CORRUPT_FRAME);

  EXPECT_EQ(vpx_codec_destroy(&dec), VPX_CODEC_OK);
}
#endif  // CONFIG_VP9_DECODER

TEST(DecodeAPI, HighBitDepthCapability) {
// VP8 should not claim VP9 HBD as a capability.
#if CONFIG_VP8_DECODER
  const vpx_codec_caps_t vp8_caps = vpx_codec_get_caps(&vpx_codec_vp8_dx_algo);
  EXPECT_EQ(vp8_caps & VPX_CODEC_CAP_HIGHBITDEPTH, 0);
#endif

#if CONFIG_VP9_DECODER
  const vpx_codec_caps_t vp9_caps = vpx_codec_get_caps(&vpx_codec_vp9_dx_algo);
#if CONFIG_VP9_HIGHBITDEPTH
  EXPECT_EQ(vp9_caps & VPX_CODEC_CAP_HIGHBITDEPTH, VPX_CODEC_CAP_HIGHBITDEPTH);
#else
  EXPECT_EQ(vp9_caps & VPX_CODEC_CAP_HIGHBITDEPTH, 0);
#endif
#endif
}

#if CONFIG_VP8_DECODER
TEST(DecodeAPI, SetReferenceValidationVp8) {
  vpx_codec_ctx_t dec;
  ASSERT_EQ(vpx_codec_dec_init(&dec, vpx_codec_vp8_dx(), nullptr, 0),
            VPX_CODEC_OK);

  vpx_image_t ref_img;
  ASSERT_NE(vpx_img_alloc(&ref_img, VPX_IMG_FMT_I420, 64, 64, 1), nullptr);

  for (const int plane : { VPX_PLANE_Y, VPX_PLANE_U, VPX_PLANE_V }) {
    vpx_ref_frame_t ref_frame;
    ref_frame.frame_type = VP8_LAST_FRAME;
    ref_frame.img = ref_img;
    ref_frame.img.planes[plane] = nullptr;
    EXPECT_EQ(vpx_codec_control(&dec, VP8_SET_REFERENCE, &ref_frame),
              VPX_CODEC_INVALID_PARAM);
    EXPECT_EQ(vpx_codec_control(&dec, VP8_COPY_REFERENCE, &ref_frame),
              VPX_CODEC_INVALID_PARAM);
  }

  vpx_img_free(&ref_img);
  EXPECT_EQ(vpx_codec_destroy(&dec), VPX_CODEC_OK);
}
#endif  // CONFIG_VP8_DECODER

#if CONFIG_VP9_DECODER
TEST(DecodeAPI, SetReferenceValidationVp9) {
  vpx_codec_ctx_t dec;
  ASSERT_EQ(vpx_codec_dec_init(&dec, vpx_codec_vp9_dx(), nullptr, 0),
            VPX_CODEC_OK);

  vpx_image_t ref_img;
  ASSERT_NE(vpx_img_alloc(&ref_img, VPX_IMG_FMT_I420, 64, 64, 1), nullptr);

  for (const int plane : { VPX_PLANE_Y, VPX_PLANE_U, VPX_PLANE_V }) {
    vpx_ref_frame_t ref_frame;
    ref_frame.frame_type = VP8_LAST_FRAME;
    ref_frame.img = ref_img;
    ref_frame.img.planes[plane] = nullptr;
    EXPECT_EQ(vpx_codec_control(&dec, VP8_SET_REFERENCE, &ref_frame),
              VPX_CODEC_INVALID_PARAM);
    EXPECT_EQ(vpx_codec_control(&dec, VP8_COPY_REFERENCE, &ref_frame),
              VPX_CODEC_INVALID_PARAM);
  }

  vpx_img_free(&ref_img);
  EXPECT_EQ(vpx_codec_destroy(&dec), VPX_CODEC_OK);
}
#endif  // CONFIG_VP9_DECODER

}  // namespace
