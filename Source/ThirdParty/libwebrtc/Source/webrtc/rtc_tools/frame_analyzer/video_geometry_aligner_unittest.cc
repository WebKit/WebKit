/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_tools/frame_analyzer/video_geometry_aligner.h"

#include <vector>

#include "rtc_tools/frame_analyzer/video_quality_analysis.h"
#include "rtc_tools/video_file_reader.h"
#include "test/gtest.h"
#include "test/testsupport/fileutils.h"

namespace webrtc {
namespace test {

class VideoGeometryAlignerTest : public ::testing::Test {
 protected:
  void SetUp() {
    reference_video_ =
        OpenYuvFile(ResourcePath("foreman_128x96", "yuv"), 128, 96);
    ASSERT_TRUE(reference_video_);

    // Very simple 4x4 frame used for verying CropAndZoom.
    const uint8_t data_y[] = {0, 1, 2,  3,  4,  5,  6,  7,
                              8, 9, 10, 11, 12, 13, 14, 15};
    const uint8_t data_u[] = {0, 1, 2, 3};
    const uint8_t data_v[] = {0, 1, 2, 3};
    test_frame_ = I420Buffer::Copy(
        /* width= */ 4, /* height= */ 4, data_y, /* stride_y= */ 4, data_u,
        /* stride_u= */ 2, data_v, /* stride_v= */ 2);
  }

  rtc::scoped_refptr<Video> reference_video_;
  rtc::scoped_refptr<I420BufferInterface> test_frame_;
};

// Teach gtest how to compare CropRegions.
bool operator==(const CropRegion& a, const CropRegion& b) {
  return a.left == b.left && a.top == b.top && a.right == b.right &&
         a.bottom == b.bottom;
}

TEST_F(VideoGeometryAlignerTest, CropAndZoomIdentity) {
  const rtc::scoped_refptr<I420BufferInterface> frame =
      reference_video_->GetFrame(0);

  // Assume perfect match, i.e. SSIM == 1.
  CropRegion identity_region;
  EXPECT_EQ(1.0, Ssim(frame, CropAndZoom(identity_region, frame)));
}

TEST_F(VideoGeometryAlignerTest, CropAndZoomLeft) {
  CropRegion region;
  region.left = 2;
  const rtc::scoped_refptr<I420BufferInterface> cropped_frame =
      CropAndZoom(region, test_frame_);
  EXPECT_EQ(std::vector<uint8_t>(
                {2, 2, 3, 3, 6, 6, 7, 7, 10, 10, 11, 11, 14, 14, 15, 15}),
            std::vector<uint8_t>(cropped_frame->DataY(),
                                 cropped_frame->DataY() + 16));
  EXPECT_EQ(
      std::vector<uint8_t>({1, 1, 3, 3}),
      std::vector<uint8_t>(cropped_frame->DataU(), cropped_frame->DataU() + 4));
  EXPECT_EQ(
      std::vector<uint8_t>({1, 1, 3, 3}),
      std::vector<uint8_t>(cropped_frame->DataV(), cropped_frame->DataV() + 4));
}

TEST_F(VideoGeometryAlignerTest, CropAndZoomTop) {
  CropRegion region;
  region.top = 2;
  const rtc::scoped_refptr<I420BufferInterface> cropped_frame =
      CropAndZoom(region, test_frame_);
  EXPECT_EQ(std::vector<uint8_t>(
                {8, 9, 10, 11, 10, 11, 12, 13, 12, 13, 14, 15, 12, 13, 14, 15}),
            std::vector<uint8_t>(cropped_frame->DataY(),
                                 cropped_frame->DataY() + 16));
  EXPECT_EQ(
      std::vector<uint8_t>({2, 3, 2, 3}),
      std::vector<uint8_t>(cropped_frame->DataU(), cropped_frame->DataU() + 4));
  EXPECT_EQ(
      std::vector<uint8_t>({2, 3, 2, 3}),
      std::vector<uint8_t>(cropped_frame->DataV(), cropped_frame->DataV() + 4));
}

TEST_F(VideoGeometryAlignerTest, CropAndZoomRight) {
  CropRegion region;
  region.right = 2;
  const rtc::scoped_refptr<I420BufferInterface> cropped_frame =
      CropAndZoom(region, test_frame_);
  EXPECT_EQ(std::vector<uint8_t>(
                {0, 0, 1, 1, 4, 4, 5, 5, 8, 8, 9, 9, 12, 12, 13, 13}),
            std::vector<uint8_t>(cropped_frame->DataY(),
                                 cropped_frame->DataY() + 16));
  EXPECT_EQ(
      std::vector<uint8_t>({0, 0, 2, 2}),
      std::vector<uint8_t>(cropped_frame->DataU(), cropped_frame->DataU() + 4));
  EXPECT_EQ(
      std::vector<uint8_t>({0, 0, 2, 2}),
      std::vector<uint8_t>(cropped_frame->DataV(), cropped_frame->DataV() + 4));
}

TEST_F(VideoGeometryAlignerTest, CropAndZoomBottom) {
  CropRegion region;
  region.bottom = 2;
  const rtc::scoped_refptr<I420BufferInterface> cropped_frame =
      CropAndZoom(region, test_frame_);
  EXPECT_EQ(
      std::vector<uint8_t>({0, 1, 2, 3, 2, 3, 4, 5, 4, 5, 6, 7, 4, 5, 6, 7}),
      std::vector<uint8_t>(cropped_frame->DataY(),
                           cropped_frame->DataY() + 16));
  EXPECT_EQ(
      std::vector<uint8_t>({0, 1, 0, 1}),
      std::vector<uint8_t>(cropped_frame->DataU(), cropped_frame->DataU() + 4));
  EXPECT_EQ(
      std::vector<uint8_t>({0, 1, 0, 1}),
      std::vector<uint8_t>(cropped_frame->DataV(), cropped_frame->DataV() + 4));
}

TEST_F(VideoGeometryAlignerTest, CalculateCropRegionIdentity) {
  const rtc::scoped_refptr<I420BufferInterface> frame =
      reference_video_->GetFrame(0);
  CropRegion identity_region;
  EXPECT_EQ(identity_region, CalculateCropRegion(frame, frame));
}

TEST_F(VideoGeometryAlignerTest, CalculateCropRegionArbitrary) {
  // Arbitrary crop region.
  CropRegion crop_region;
  crop_region.left = 2;
  crop_region.top = 4;
  crop_region.right = 5;
  crop_region.bottom = 3;

  const rtc::scoped_refptr<I420BufferInterface> frame =
      reference_video_->GetFrame(0);

  EXPECT_EQ(crop_region,
            CalculateCropRegion(frame, CropAndZoom(crop_region, frame)));
}

}  // namespace test
}  // namespace webrtc
