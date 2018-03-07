/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include <string.h>
#include <stdio.h>
#include <cstring>
#include <iostream>
#include <vector>

#include "rtc_tools/frame_analyzer/reference_less_video_analysis_lib.h"
#include "test/gtest.h"
#include "test/testsupport/fileutils.h"

class ReferenceLessVideoAnalysisTest : public ::testing::Test {
 public:
  void SetUp() override {
    video_file =
           webrtc::test::ResourcePath("reference_less_video_test_file", "y4m");
  }
  std::string video_file;
  std::vector<double> psnr_per_frame;
  std::vector<double> ssim_per_frame;
};

TEST_F(ReferenceLessVideoAnalysisTest, MatchComputedMetrics) {
  compute_metrics(video_file, &psnr_per_frame, &ssim_per_frame);
  EXPECT_EQ(74, (int)psnr_per_frame.size());

  ASSERT_NEAR(27.2f, psnr_per_frame[1], 0.1f);
  ASSERT_NEAR(24.9f, psnr_per_frame[5], 0.1f);

  ASSERT_NEAR(0.9f, ssim_per_frame[1], 0.1f);
  ASSERT_NEAR(0.9f, ssim_per_frame[5], 0.1f);
}

TEST_F(ReferenceLessVideoAnalysisTest, MatchHeightWidthFps) {
  int height = 0, width = 0, fps = 0;
  get_height_width_fps(&height, &width, &fps, video_file.c_str());
  EXPECT_EQ(height, 720);
  EXPECT_EQ(width, 1280);
  EXPECT_EQ(fps, 25);
}

TEST_F(ReferenceLessVideoAnalysisTest, MatchIdenticalFrameClusters) {
  compute_metrics(video_file, &psnr_per_frame, &ssim_per_frame);
  std::vector<int> identical_frame_clusters =
      find_frame_clusters(psnr_per_frame, ssim_per_frame);
  EXPECT_EQ(5, (int)identical_frame_clusters.size());
  EXPECT_EQ(1, identical_frame_clusters[0]);
  EXPECT_EQ(1, identical_frame_clusters[4]);
}

TEST_F(ReferenceLessVideoAnalysisTest, CheckFileExtension) {
  EXPECT_TRUE(check_file_extension(video_file));
  std::string txt_file =
      webrtc::test::ResourcePath("video_quality_analysis_frame", "txt");
  EXPECT_FALSE(check_file_extension(txt_file));
}




