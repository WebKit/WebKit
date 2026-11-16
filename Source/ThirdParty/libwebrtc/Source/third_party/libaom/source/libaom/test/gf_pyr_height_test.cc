/*
 * Copyright (c) 2019, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <ostream>

#include "gtest/gtest.h"
#include "test/codec_factory.h"
#include "test/encode_test_driver.h"
#include "test/i420_video_source.h"
#include "test/util.h"

namespace {

static const struct GFPyrHeightTestParam {
  int gf_min_pyr_height;
  int gf_max_pyr_height;
  double psnr_thresh;
} kTestParams[] = {
  // gf_min_pyr_height = 0
  { 0, 0, 32.30 },
  { 0, 1, 33.90 },
  { 0, 2, 34.00 },
  { 0, 3, 34.20 },
  { 0, 4, 34.30 },
  { 0, 5, 34.35 },
  // gf_min_pyr_height = 1
  { 1, 1, 33.90 },
  { 1, 2, 34.00 },
  { 1, 3, 34.20 },
  { 1, 4, 34.30 },
  { 1, 5, 34.35 },
  // gf_min_pyr_height = 2
  { 2, 2, 34.00 },
  { 2, 3, 34.20 },
  { 2, 4, 34.30 },
  { 2, 5, 34.35 },
  // gf_min_pyr_height = 3
  { 3, 3, 34.20 },
  { 3, 4, 34.30 },
  { 3, 5, 34.35 },
  // gf_min_pyr_height = 4
  { 4, 4, 34.30 },
  { 4, 5, 34.35 },
  // gf_min_pyr_height = 5
  { 5, 5, 34.35 },
};

// Compiler may decide to add some padding to the struct above for alignment,
// which the gtest may try to print (on error for example). This would cause
// valgrind to complain that the padding is uninitialized. To avoid that, we
// provide our own function to print the struct.
// This also makes '--gtest_list_tests' output more understandable.
std::ostream &operator<<(std::ostream &os, const GFPyrHeightTestParam &p) {
  os << "GFPyrHeightTestParam { "
     << "gf_min_pyr_height = " << p.gf_min_pyr_height << ", "
     << "gf_max_pyr_height = " << p.gf_max_pyr_height << ", "
     << "psnr_thresh = " << p.psnr_thresh << " }";
  return os;
}

// Params: encoding mode, rate control mode and GFPyrHeightTestParam object.
class GFPyrHeightTest
    : public ::libaom_test::CodecTestWith3Params<
          libaom_test::TestMode, aom_rc_mode, GFPyrHeightTestParam>,
      public ::libaom_test::EncoderTest {
 protected:
  GFPyrHeightTest()
      : EncoderTest(GET_PARAM(0)), encoding_mode_(GET_PARAM(1)),
        rc_mode_(GET_PARAM(2)) {
    gf_min_pyr_height_ = GET_PARAM(3).gf_min_pyr_height;
    gf_max_pyr_height_ = GET_PARAM(3).gf_max_pyr_height;
    psnr_threshold_ = GET_PARAM(3).psnr_thresh;
  }
  ~GFPyrHeightTest() override = default;

  void SetUp() override {
    InitializeConfig(encoding_mode_);
    const aom_rational timebase = { 1, 30 };
    cfg_.g_timebase = timebase;
    cpu_used_ = 4;
    cfg_.rc_end_usage = rc_mode_;
    if (rc_mode_ == AOM_VBR) {
      cfg_.rc_target_bitrate = 200;
    }
    cfg_.g_lag_in_frames = 19;
    cfg_.g_threads = 0;
    init_flags_ = AOM_CODEC_USE_PSNR;
  }

  void BeginPassHook(unsigned int) override {
    psnr_ = 0.0;
    nframes_ = 0;
  }

  void PSNRPktHook(const aom_codec_cx_pkt_t *pkt) override {
    psnr_ += pkt->data.psnr.psnr[0];
    nframes_++;
  }

  void PreEncodeFrameHook(::libaom_test::VideoSource *video,
                          ::libaom_test::Encoder *encoder) override {
    if (video->frame() == 0) {
      encoder->Control(AOME_SET_CPUUSED, cpu_used_);
      if (rc_mode_ == AOM_Q) {
        encoder->Control(AOME_SET_CQ_LEVEL, 32);
      }
      if (encoding_mode_ != ::libaom_test::kRealTime) {
        encoder->Control(AOME_SET_ENABLEAUTOALTREF, 1);
        encoder->Control(AOME_SET_ARNR_MAXFRAMES, 7);
        encoder->Control(AOME_SET_ARNR_STRENGTH, 5);
      }
      encoder->Control(AV1E_SET_GF_MIN_PYRAMID_HEIGHT, gf_min_pyr_height_);
      encoder->Control(AV1E_SET_GF_MAX_PYRAMID_HEIGHT, gf_max_pyr_height_);
    }
  }

  double GetAveragePsnr() const {
    if (nframes_) return psnr_ / nframes_;
    return 0.0;
  }

  double GetPsnrThreshold() { return psnr_threshold_; }

  ::libaom_test::TestMode encoding_mode_;
  aom_rc_mode rc_mode_;
  double psnr_threshold_;
  int gf_min_pyr_height_;
  int gf_max_pyr_height_;
  int cpu_used_;
  int nframes_;
  double psnr_;
};

TEST_P(GFPyrHeightTest, EncodeAndVerifyPSNR) {
  libaom_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352, 288,
                                     cfg_.g_timebase.den, cfg_.g_timebase.num,
                                     0, 32);
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  EXPECT_GT(GetAveragePsnr(), GetPsnrThreshold())
      << "GF Min Pyramid Height = " << gf_min_pyr_height_ << ", "
      << "GF Max Pyramid Height = " << gf_max_pyr_height_;
}

AV1_INSTANTIATE_TEST_SUITE(GFPyrHeightTest, NONREALTIME_TEST_MODES,
                           ::testing::Values(AOM_Q, AOM_VBR),
                           ::testing::ValuesIn(kTestParams));
}  // namespace
