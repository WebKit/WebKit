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

#include <climits>
#include <cstring>
#include <string>
#include <vector>

#include "gtest/gtest.h"

#include "aom/aomcx.h"
#include "aom/aom_encoder.h"

#include "test/codec_factory.h"
#include "test/encode_test_driver.h"
#include "test/i420_video_source.h"
#include "test/md5_helper.h"
#include "test/util.h"

namespace {

enum { kSkip = 0, kDeltaQ = 1, kDeltaLF = 2, kReference = 3 };

// Params: test mode, speed, aq_mode and screen_content mode.
class ROIMapTest
    : public ::libaom_test::CodecTestWith4Params<libaom_test::TestMode, int,
                                                 int, int>,
      public ::libaom_test::EncoderTest {
 protected:
  static const int kWidth = 640;
  static const int kHeight = 480;

  ROIMapTest() : EncoderTest(GET_PARAM(0)) {}
  ~ROIMapTest() override = default;

  void SetUp() override {
    InitializeConfig(GET_PARAM(1));
    cpu_used_ = GET_PARAM(2);
    aq_mode_ = GET_PARAM(3);
    screen_mode_ = GET_PARAM(4);
  }

  void PreEncodeFrameHook(::libaom_test::VideoSource *video,
                          ::libaom_test::Encoder *encoder) override {
    if (video->frame() == 0) {
      encoder->Control(AOME_SET_CPUUSED, cpu_used_);
      encoder->Control(AV1E_SET_ALLOW_WARPED_MOTION, 0);
      encoder->Control(AV1E_SET_ENABLE_GLOBAL_MOTION, 0);
      encoder->Control(AV1E_SET_ENABLE_OBMC, 0);
      encoder->Control(AV1E_SET_ENABLE_TPL_MODEL, 0);
      encoder->Control(AV1E_SET_DELTAQ_MODE, 0);
      encoder->Control(AV1E_SET_DELTALF_MODE, 0);
      encoder->Control(AV1E_SET_AQ_MODE, aq_mode_ ? 3 : 0);
      encoder->Control(AV1E_SET_TUNE_CONTENT, screen_mode_);
      if (screen_mode_) encoder->Control(AV1E_SET_ENABLE_PALETTE, 1);
      aom_roi_map_t roi = aom_roi_map_t();
      const int block_size = 4;
      roi.rows = (kHeight + block_size - 1) / block_size;
      roi.cols = (kWidth + block_size - 1) / block_size;
      memset(&roi.skip, 0, sizeof(roi.skip));
      memset(&roi.delta_q, 0, sizeof(roi.delta_q));
      memset(&roi.delta_lf, 0, sizeof(roi.delta_lf));
      memset(roi.ref_frame, -1, sizeof(roi.ref_frame));
      // Set ROI map to be 1 (segment #1) in middle squere of image,
      // 0 elsewhere.
      roi.enabled = 1;
      roi.roi_map =
          (uint8_t *)calloc(roi.rows * roi.cols, sizeof(*roi.roi_map));
      for (unsigned int i = 0; i < roi.rows; ++i) {
        for (unsigned int j = 0; j < roi.cols; ++j) {
          const int idx = i * roi.cols + j;
          if (i > roi.rows / 4 && i < (3 * roi.rows) / 4 && j > roi.cols / 4 &&
              j < (3 * roi.cols) / 4)
            roi.roi_map[idx] = 1;
          else
            roi.roi_map[idx] = 0;
        }
      }
      // Set the ROI feature, on segment #1.
      if (roi_feature_ == kSkip)
        roi.skip[1] = 1;
      else if (roi_feature_ == kDeltaQ)
        roi.delta_q[1] = -40;
      else if (roi_feature_ == kReference)
        roi.ref_frame[1] = 4;  // GOLDEN_FRAME;
      encoder->Control(AOME_SET_ROI_MAP, &roi);
      free(roi.roi_map);
    }
  }

  void ROISkipTest() {
    cfg_.g_lag_in_frames = 0;
    cfg_.rc_target_bitrate = 400;
    cfg_.rc_resize_mode = 0;
    cfg_.g_pass = AOM_RC_ONE_PASS;
    cfg_.rc_end_usage = AOM_CBR;
    cfg_.kf_max_dist = 90000;
    cfg_.rc_min_quantizer = 0;
    cfg_.rc_max_quantizer = 52;
    roi_feature_ = kSkip;
    ::libaom_test::I420VideoSource video("niklas_640_480_30.yuv", 640, 480, 30,
                                         1, 0, 400);

    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  }

  void ROIDeltaQTest() {
    cfg_.g_lag_in_frames = 0;
    cfg_.rc_target_bitrate = 400;
    cfg_.rc_resize_mode = 0;
    cfg_.g_pass = AOM_RC_ONE_PASS;
    cfg_.rc_end_usage = AOM_CBR;
    cfg_.kf_max_dist = 90000;
    cfg_.rc_min_quantizer = 0;
    cfg_.rc_max_quantizer = 40;
    roi_feature_ = kDeltaQ;
    ::libaom_test::I420VideoSource video("niklas_640_480_30.yuv", 640, 480, 30,
                                         1, 0, 400);

    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  }

  void ROIReferenceTest() {
    cfg_.g_lag_in_frames = 0;
    cfg_.rc_target_bitrate = 400;
    cfg_.rc_resize_mode = 0;
    cfg_.g_pass = AOM_RC_ONE_PASS;
    cfg_.rc_end_usage = AOM_CBR;
    cfg_.kf_max_dist = 90000;
    cfg_.rc_min_quantizer = 0;
    cfg_.rc_max_quantizer = 52;
    roi_feature_ = kReference;
    ::libaom_test::I420VideoSource video("niklas_640_480_30.yuv", 640, 480, 30,
                                         1, 0, 400);

    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  }

  int cpu_used_;
  int aq_mode_;
  int screen_mode_;

 private:
  int roi_feature_;
};

TEST_P(ROIMapTest, ROISkip) { ROISkipTest(); }

TEST_P(ROIMapTest, ROIDeltaQ) { ROIDeltaQTest(); }

TEST_P(ROIMapTest, ROIReference) { ROIReferenceTest(); }

AV1_INSTANTIATE_TEST_SUITE(ROIMapTest,
                           ::testing::Values(::libaom_test::kRealTime),
                           ::testing::Range(7, 12), ::testing::Values(0, 1),
                           ::testing::Values(0, 1));

// Codes the top left 32x32 of every 64x64 and skips the rest, so every
// superblock has a skipped segment and is coded without the variance
// partitioning. The output must not depend on the number of threads.
class ROIMapThreadTest : public ::libaom_test::CodecTestWithParam<int>,
                         public ::libaom_test::EncoderTest {
 protected:
  static const int kWidth = 640;
  static const int kHeight = 480;

  ROIMapThreadTest() : EncoderTest(GET_PARAM(0)), cpu_used_(GET_PARAM(1)) {}
  ~ROIMapThreadTest() override = default;

  void SetUp() override {
    InitializeConfig(::libaom_test::kRealTime);
    cfg_.g_lag_in_frames = 0;
    cfg_.rc_end_usage = AOM_CBR;
    cfg_.rc_target_bitrate = 4000;
    cfg_.kf_max_dist = 90000;
    cfg_.rc_min_quantizer = 0;
    cfg_.rc_max_quantizer = 52;
  }

  void PreEncodeFrameHook(::libaom_test::VideoSource *video,
                          ::libaom_test::Encoder *encoder) override {
    if (video->frame() != 0) return;
    encoder->Control(AOME_SET_CPUUSED, cpu_used_);
    encoder->Control(AV1E_SET_AQ_MODE, 0);
    encoder->Control(AV1E_SET_ROW_MT, 1);
    encoder->Control(AV1E_SET_TILE_COLUMNS, 1);
    // Costs updated per superblock differ between one thread and row mt.
    encoder->Control(AV1E_SET_COEFF_COST_UPD_FREQ, 3);
    encoder->Control(AV1E_SET_MODE_COST_UPD_FREQ, 3);
    encoder->Control(AV1E_SET_MV_COST_UPD_FREQ, 3);
    aom_roi_map_t roi = aom_roi_map_t();
    roi.rows = (kHeight + 3) / 4;
    roi.cols = (kWidth + 3) / 4;
    std::vector<uint8_t> map(roi.rows * roi.cols);
    for (unsigned int i = 0; i < roi.rows; ++i) {
      for (unsigned int j = 0; j < roi.cols; ++j) {
        map[i * roi.cols + j] = (i & 15) < 8 && (j & 15) < 8;
      }
    }
    roi.enabled = 1;
    roi.roi_map = map.data();
    memset(roi.ref_frame, -1, sizeof(roi.ref_frame));
    roi.skip[0] = 1;
    // Segment 1 needs a feature to stay a separate segment.
    roi.delta_q[1] = 8;
    encoder->Control(AOME_SET_ROI_MAP, &roi);
  }

  void FramePktHook(const aom_codec_cx_pkt_t *pkt) override {
    ::libaom_test::MD5 md5;
    md5.Add(static_cast<const uint8_t *>(pkt->data.frame.buf),
            pkt->data.frame.sz);
    md5_.push_back(md5.Get());
  }

  std::vector<std::string> Encode(int threads) {
    cfg_.g_threads = threads;
    md5_.clear();
    ::libaom_test::I420VideoSource video("niklas_640_480_30.yuv", kWidth,
                                         kHeight, 30, 1, 0, 30);
    EXPECT_NO_FATAL_FAILURE(RunLoop(&video));
    return md5_;
  }

  int cpu_used_;
  std::vector<std::string> md5_;
};

TEST_P(ROIMapThreadTest, SameOutput) {
  const std::vector<std::string> ref = Encode(1);
  ASSERT_EQ(ref.size(), 30u);
  EXPECT_EQ(ref, Encode(2));
  EXPECT_EQ(ref, Encode(4));
}

AV1_INSTANTIATE_TEST_SUITE(ROIMapThreadTest, ::testing::Values(9));

}  // namespace
