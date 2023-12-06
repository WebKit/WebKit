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

#include <climits>
#include <vector>
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"
#include "test/codec_factory.h"
#include "test/encode_test_driver.h"
#include "test/i420_video_source.h"
#include "test/video_source.h"
#include "test/util.h"

namespace {

const unsigned int kCqLevel = 18;
const double kMaxPsnr = 100.0;

// kPsnrThreshold represents the psnr threshold used to validate the quality of
// the first frame. The indices correspond to one/two-pass, allintra and
// realtime encoding modes.
const double kPsnrThreshold[3] = { 29.0, 41.5, 41.5 };

// kPsnrFluctuation represents the maximum allowed psnr fluctuation w.r.t first
// frame. The indices correspond to one/two-pass, allintra and realtime
// encoding modes.
const double kPsnrFluctuation[3] = { 2.5, 0.3, 16.0 };

class MonochromeTest
    : public ::libaom_test::CodecTestWith3Params<libaom_test::TestMode, int,
                                                 int>,
      public ::libaom_test::EncoderTest {
 protected:
  MonochromeTest()
      : EncoderTest(GET_PARAM(0)), lossless_(GET_PARAM(2)),
        frame0_psnr_y_(0.0) {}

  ~MonochromeTest() override = default;

  void SetUp() override { InitializeConfig(GET_PARAM(1)); }

  void PreEncodeFrameHook(::libaom_test::VideoSource *video,
                          ::libaom_test::Encoder *encoder) override {
    if (video->frame() == 0) {
      encoder->Control(AOME_SET_CPUUSED, GET_PARAM(3));
      if (mode_ == ::libaom_test::kAllIntra) {
        encoder->Control(AOME_SET_CQ_LEVEL, kCqLevel);
      }
      if (lossless_) {
        encoder->Control(AV1E_SET_LOSSLESS, 1);
      }
    }
  }

  void DecompressedFrameHook(const aom_image_t &img,
                             aom_codec_pts_t pts) override {
    (void)pts;

    // Get value of top-left corner pixel of U plane
    int chroma_value = img.planes[AOM_PLANE_U][0];

    bool is_chroma_constant =
        ComparePlaneToValue(img, AOM_PLANE_U, chroma_value) &&
        ComparePlaneToValue(img, AOM_PLANE_V, chroma_value);

    // Chroma planes should be constant
    EXPECT_TRUE(is_chroma_constant);

    // Monochrome flag on image should be set
    EXPECT_EQ(img.monochrome, 1);

    chroma_value_list_.push_back(chroma_value);
  }

  // Returns true if all pixels on the plane are equal to value, and returns
  // false otherwise.
  bool ComparePlaneToValue(const aom_image_t &img, const int plane,
                           const int value) {
    const int w = aom_img_plane_width(&img, plane);
    const int h = aom_img_plane_height(&img, plane);
    const uint8_t *const buf = img.planes[plane];
    const int stride = img.stride[plane];

    for (int r = 0; r < h; ++r) {
      for (int c = 0; c < w; ++c) {
        if (buf[r * stride + c] != value) return false;
      }
    }
    return true;
  }

  void PSNRPktHook(const aom_codec_cx_pkt_t *pkt) override {
    // Check average PSNR value is >= 100 db in case of lossless encoding.
    if (lossless_) {
      EXPECT_GE(pkt->data.psnr.psnr[0], kMaxPsnr);
      return;
    }
    const int psnr_index = (mode_ == ::libaom_test::kRealTime)   ? 2
                           : (mode_ == ::libaom_test::kAllIntra) ? 1
                                                                 : 0;
    // Check that the initial Y PSNR value is 'high enough', and check that
    // subsequent Y PSNR values are 'close' to this initial value.
    if (frame0_psnr_y_ == 0.0) {
      frame0_psnr_y_ = pkt->data.psnr.psnr[1];
      EXPECT_GT(frame0_psnr_y_, kPsnrThreshold[psnr_index]);
    }
    EXPECT_NEAR(pkt->data.psnr.psnr[1], frame0_psnr_y_,
                kPsnrFluctuation[psnr_index]);
  }

  int lossless_;
  std::vector<int> chroma_value_list_;
  double frame0_psnr_y_;
};

TEST_P(MonochromeTest, TestMonochromeEncoding) {
  ::libaom_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352, 288,
                                       30, 1, 0, 5);

  init_flags_ = AOM_CODEC_USE_PSNR;

  cfg_.rc_buf_initial_sz = 500;
  cfg_.rc_buf_optimal_sz = 600;
  cfg_.rc_buf_sz = 1000;
  cfg_.rc_min_quantizer = 2;
  cfg_.rc_max_quantizer = 56;
  cfg_.rc_undershoot_pct = 50;
  cfg_.rc_overshoot_pct = 50;
  cfg_.rc_end_usage = AOM_CBR;
  cfg_.g_lag_in_frames = 1;
  cfg_.kf_min_dist = cfg_.kf_max_dist = 3000;
  // Enable dropped frames.
  cfg_.rc_dropframe_thresh = 1;
  // Run at low bitrate.
  cfg_.rc_target_bitrate = 40;
  // Set monochrome encoding flag
  cfg_.monochrome = 1;

  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));

  // Check that the chroma planes are equal across all frames
  std::vector<int>::const_iterator iter = chroma_value_list_.begin();
  int initial_chroma_value = *iter;
  for (; iter != chroma_value_list_.end(); ++iter) {
    // Check that all decoded frames have the same constant chroma planes.
    EXPECT_EQ(*iter, initial_chroma_value);
  }
}

class MonochromeAllIntraTest : public MonochromeTest {};

TEST_P(MonochromeAllIntraTest, TestMonochromeEncoding) {
  ::libaom_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352, 288,
                                       30, 1, 0, 5);
  init_flags_ = AOM_CODEC_USE_PSNR;
  // Set monochrome encoding flag
  cfg_.monochrome = 1;
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));

  // Check that the chroma planes are equal across all frames
  std::vector<int>::const_iterator iter = chroma_value_list_.begin();
  int initial_chroma_value = *iter;
  for (; iter != chroma_value_list_.end(); ++iter) {
    // Check that all decoded frames have the same constant chroma planes.
    EXPECT_EQ(*iter, initial_chroma_value);
  }
}

class MonochromeRealtimeTest : public MonochromeTest {};

TEST_P(MonochromeRealtimeTest, TestMonochromeEncoding) {
  ::libaom_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352, 288,
                                       30, 1, 0, 30);
  init_flags_ = AOM_CODEC_USE_PSNR;
  // Set monochrome encoding flag
  cfg_.monochrome = 1;
  // Run at low bitrate.
  cfg_.rc_target_bitrate = 40;
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));

  // Check that the chroma planes are equal across all frames
  std::vector<int>::const_iterator iter = chroma_value_list_.begin();
  int initial_chroma_value = *iter;
  for (; iter != chroma_value_list_.end(); ++iter) {
    // Check that all decoded frames have the same constant chroma planes.
    EXPECT_EQ(*iter, initial_chroma_value);
  }
}

AV1_INSTANTIATE_TEST_SUITE(MonochromeTest,
                           ::testing::Values(::libaom_test::kOnePassGood,
                                             ::libaom_test::kTwoPassGood),
                           ::testing::Values(0),   // lossless
                           ::testing::Values(0));  // cpu_used

AV1_INSTANTIATE_TEST_SUITE(MonochromeAllIntraTest,
                           ::testing::Values(::libaom_test::kAllIntra),
                           ::testing::Values(0, 1),   // lossless
                           ::testing::Values(6, 9));  // cpu_used

AV1_INSTANTIATE_TEST_SUITE(MonochromeRealtimeTest,
                           ::testing::Values(::libaom_test::kRealTime),
                           ::testing::Values(0),          // lossless
                           ::testing::Values(6, 8, 10));  // cpu_used

}  // namespace
