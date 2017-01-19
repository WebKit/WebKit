/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/test/testsupport/metrics/video_metrics.h"

#include "webrtc/test/gtest.h"
#include "webrtc/test/testsupport/fileutils.h"

namespace webrtc {

static const int kWidth = 352;
static const int kHeight = 288;

static const int kMissingReferenceFileReturnCode = -1;
static const int kMissingTestFileReturnCode = -2;
static const int kEmptyFileReturnCode = -3;
static const double kPsnrPerfectResult =  48.0;
static const double kSsimPerfectResult = 1.0;

class VideoMetricsTest: public testing::Test {
 protected:
  VideoMetricsTest() {
    video_file_ = webrtc::test::ResourcePath("foreman_cif_short", "yuv");
  }
  virtual ~VideoMetricsTest() {}
  void SetUp() {
    non_existing_file_ = webrtc::test::OutputPath() +
        "video_metrics_unittest_non_existing";
    remove(non_existing_file_.c_str());  // To be sure it doesn't exist.

    // Create an empty file:
    empty_file_ = webrtc::test::TempFilename(
        webrtc::test::OutputPath(), "video_metrics_unittest_empty_file");
    FILE* dummy = fopen(empty_file_.c_str(), "wb");
    fclose(dummy);
  }
  void TearDown() {
    remove(empty_file_.c_str());
  }
  webrtc::test::QualityMetricsResult psnr_result_;
  webrtc::test::QualityMetricsResult ssim_result_;
  std::string non_existing_file_;
  std::string empty_file_;
  std::string video_file_;
};

// Tests that it is possible to run with the same reference as test file
TEST_F(VideoMetricsTest, ReturnsPerfectResultForIdenticalFilesPSNR) {
  EXPECT_EQ(0, I420PSNRFromFiles(video_file_.c_str(), video_file_.c_str(),
                                 kWidth, kHeight, &psnr_result_));
  EXPECT_EQ(kPsnrPerfectResult, psnr_result_.average);
}

TEST_F(VideoMetricsTest, ReturnsPerfectResultForIdenticalFilesSSIM) {
  EXPECT_EQ(0, I420SSIMFromFiles(video_file_.c_str(), video_file_.c_str(),
                                 kWidth, kHeight, &ssim_result_));
  EXPECT_EQ(kSsimPerfectResult, ssim_result_.average);
}

TEST_F(VideoMetricsTest, ReturnsPerfectResultForIdenticalFilesBothMetrics) {
  EXPECT_EQ(0, I420MetricsFromFiles(video_file_.c_str(), video_file_.c_str(),
                                    kWidth, kHeight, &psnr_result_,
                                    &ssim_result_));
  EXPECT_EQ(kPsnrPerfectResult, psnr_result_.average);
  EXPECT_EQ(kSsimPerfectResult, ssim_result_.average);
}

// Tests that the right return code is given when the reference file is missing.
TEST_F(VideoMetricsTest, MissingReferenceFilePSNR) {
  EXPECT_EQ(kMissingReferenceFileReturnCode,
            I420PSNRFromFiles(non_existing_file_.c_str(),
                              video_file_.c_str(), kWidth, kHeight,
                              &ssim_result_));
}

TEST_F(VideoMetricsTest, MissingReferenceFileSSIM) {
  EXPECT_EQ(kMissingReferenceFileReturnCode,
            I420SSIMFromFiles(non_existing_file_.c_str(),
                              video_file_.c_str(), kWidth, kHeight,
                              &ssim_result_));
}

TEST_F(VideoMetricsTest, MissingReferenceFileBothMetrics) {
  EXPECT_EQ(kMissingReferenceFileReturnCode,
            I420MetricsFromFiles(non_existing_file_.c_str(),
                                 video_file_.c_str(), kWidth, kHeight,
                                 &psnr_result_, &ssim_result_));
}

// Tests that the right return code is given when the test file is missing.
TEST_F(VideoMetricsTest, MissingTestFilePSNR) {
  EXPECT_EQ(kMissingTestFileReturnCode,
            I420PSNRFromFiles(video_file_.c_str(), non_existing_file_.c_str(),
                              kWidth, kHeight, &ssim_result_));
}

TEST_F(VideoMetricsTest, MissingTestFileSSIM) {
  EXPECT_EQ(kMissingTestFileReturnCode,
            I420SSIMFromFiles(video_file_.c_str(), non_existing_file_.c_str(),
                              kWidth, kHeight, &ssim_result_));
}

TEST_F(VideoMetricsTest, MissingTestFileBothMetrics) {
  EXPECT_EQ(kMissingTestFileReturnCode,
            I420MetricsFromFiles(video_file_.c_str(),
                                 non_existing_file_.c_str(), kWidth, kHeight,
                                 &psnr_result_, &ssim_result_));
}

// Tests that the method can be executed with empty files.
TEST_F(VideoMetricsTest, EmptyFilesPSNR) {
  EXPECT_EQ(kEmptyFileReturnCode,
            I420PSNRFromFiles(empty_file_.c_str(), video_file_.c_str(),
                              kWidth, kHeight, &ssim_result_));
  EXPECT_EQ(kEmptyFileReturnCode,
            I420PSNRFromFiles(video_file_.c_str(), empty_file_.c_str(),
                              kWidth, kHeight, &ssim_result_));
}

TEST_F(VideoMetricsTest, EmptyFilesSSIM) {
  EXPECT_EQ(kEmptyFileReturnCode,
            I420SSIMFromFiles(empty_file_.c_str(), video_file_.c_str(),
                              kWidth, kHeight, &ssim_result_));
  EXPECT_EQ(kEmptyFileReturnCode,
            I420SSIMFromFiles(video_file_.c_str(), empty_file_.c_str(),
                              kWidth, kHeight, &ssim_result_));
}

TEST_F(VideoMetricsTest, EmptyFilesBothMetrics) {
  EXPECT_EQ(kEmptyFileReturnCode,
            I420MetricsFromFiles(empty_file_.c_str(), video_file_.c_str(),
                                 kWidth, kHeight,
                                 &psnr_result_, &ssim_result_));
  EXPECT_EQ(kEmptyFileReturnCode,
              I420MetricsFromFiles(video_file_.c_str(), empty_file_.c_str(),
                                   kWidth, kHeight,
                                   &psnr_result_, &ssim_result_));
}

}  // namespace webrtc
