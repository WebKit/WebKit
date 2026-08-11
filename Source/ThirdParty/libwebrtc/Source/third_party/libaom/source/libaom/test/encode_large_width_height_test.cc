/*
 * Copyright (c) 2026, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

// Test for encoding frames with dimensions > 32768 pixels.
//
// pickcdef.c used FULLPEL_MV (int16_t row/col) to store pixel positions in
// get_filt_error(). For frames taller than 32768 pixels, the pixel row
// overflows int16_t, wrapping to a negative value and causing an out-of-bounds
// read. aom_codec_encode() should not crash or read out of bounds. See bug
// 489473886.

#include <memory>

#include "gtest/gtest.h"

#include "aom/aom_encoder.h"
#include "aom/aom_image.h"
#include "aom/aomcx.h"
#include "config/aom_config.h"
#include "test/video_source.h"

namespace {

// Encode a single frame with the given dimensions and flush.
void EncodeSingleFrame(unsigned int width, unsigned int height,
                       unsigned int usage, int cpu_used) {
  aom_codec_iface_t *iface = aom_codec_av1_cx();
  aom_codec_enc_cfg_t cfg;
  ASSERT_EQ(aom_codec_enc_config_default(iface, &cfg, usage), AOM_CODEC_OK);
  cfg.g_w = width;
  cfg.g_h = height;
  cfg.g_lag_in_frames = 0;
  cfg.g_pass = AOM_RC_ONE_PASS;
  // Use VBR with moderate QP to ensure non-skip blocks, which is necessary
  // to exercise the CDEF search path. AOM_Q with low cq_level triggers an
  // early return in adaptive CDEF for ALL_INTRA mode.
  cfg.rc_end_usage = AOM_VBR;
  cfg.rc_target_bitrate = 1000;

  aom_codec_ctx_t ctx;
  ASSERT_EQ(aom_codec_enc_init(&ctx, iface, &cfg, 0), AOM_CODEC_OK);
  std::unique_ptr<aom_codec_ctx_t, decltype(&aom_codec_destroy)> enc(
      &ctx, &aom_codec_destroy);
  ASSERT_EQ(aom_codec_control(enc.get(), AOME_SET_CPUUSED, cpu_used),
            AOM_CODEC_OK);

  libaom_test::RandomVideoSource video;
  video.SetSize(width, height);
  video.SetImageFormat(AOM_IMG_FMT_I420);
  video.Begin();

  ASSERT_EQ(aom_codec_encode(enc.get(), video.img(), video.pts(),
                             /*duration=*/1, /*flags=*/0),
            AOM_CODEC_OK)
      << aom_codec_error_detail(enc.get());
  ASSERT_EQ(aom_codec_encode(enc.get(), nullptr, 0, 0, 0), AOM_CODEC_OK)
      << aom_codec_error_detail(enc.get());
}

class EncodeBigDimension
    : public testing::TestWithParam<unsigned int /*usage*/> {};

// Height > 32768: triggers the CDEF int16_t overflow at superblock row 512.
// 32832 = 513 * 64, giving 513 superblock rows (fbr 0..512).
// Use ALL_INTRA at speed 6 so the full CDEF search runs (not CDEF_PICK_FROM_Q)
// while keeping encode time reasonable.
TEST_P(EncodeBigDimension, TallFrame) {
  EncodeSingleFrame(/*width=*/64, /*height=*/32832, /*usage=*/GetParam(),
                    /*cpu_used=*/5);
}

// Width > 32768: same int16_t overflow but in the column direction.
TEST_P(EncodeBigDimension, WideFrame) {
  EncodeSingleFrame(/*width=*/32832, /*height=*/64, /*usage=*/GetParam(),
                    /*cpu_used=*/5);
}

constexpr unsigned int kUsages[] = {
  AOM_USAGE_REALTIME,
#if !CONFIG_REALTIME_ONLY
  AOM_USAGE_GOOD_QUALITY,
  AOM_USAGE_ALL_INTRA,
#endif
};

INSTANTIATE_TEST_SUITE_P(All, EncodeBigDimension, testing::ValuesIn(kUsages));

}  // namespace
