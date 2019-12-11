/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include <vector>

#include "api/scoped_refptr.h"
#include "rtc_tools/frame_analyzer/reference_less_video_analysis_lib.h"
#include "rtc_tools/video_file_reader.h"
#include "test/gtest.h"
#include "test/testsupport/file_utils.h"

class ReferenceLessVideoAnalysisTest : public ::testing::Test {
 public:
  void SetUp() override {
    video = webrtc::test::OpenY4mFile(
        webrtc::test::ResourcePath("reference_less_video_test_file", "y4m"));
    ASSERT_TRUE(video);
  }

  rtc::scoped_refptr<webrtc::test::Video> video;
  std::vector<double> psnr_per_frame;
  std::vector<double> ssim_per_frame;
};

TEST_F(ReferenceLessVideoAnalysisTest, MatchComputedMetrics) {
  compute_metrics(video, &psnr_per_frame, &ssim_per_frame);
  EXPECT_EQ(74, (int)psnr_per_frame.size());

  ASSERT_NEAR(27.2f, psnr_per_frame[1], 0.1f);
  ASSERT_NEAR(24.9f, psnr_per_frame[5], 0.1f);

  ASSERT_NEAR(0.9f, ssim_per_frame[1], 0.1f);
  ASSERT_NEAR(0.9f, ssim_per_frame[5], 0.1f);
}

TEST_F(ReferenceLessVideoAnalysisTest, MatchIdenticalFrameClusters) {
  compute_metrics(video, &psnr_per_frame, &ssim_per_frame);
  std::vector<int> identical_frame_clusters =
      find_frame_clusters(psnr_per_frame, ssim_per_frame);
  EXPECT_EQ(5, (int)identical_frame_clusters.size());
  EXPECT_EQ(1, identical_frame_clusters[0]);
  EXPECT_EQ(1, identical_frame_clusters[4]);
}
