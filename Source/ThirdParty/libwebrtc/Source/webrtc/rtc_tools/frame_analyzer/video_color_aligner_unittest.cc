/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_tools/frame_analyzer/video_color_aligner.h"

#include "rtc_tools/frame_analyzer/video_quality_analysis.h"
#include "rtc_tools/video_file_reader.h"
#include "test/gtest.h"
#include "test/testsupport/fileutils.h"
#include "third_party/libyuv/include/libyuv/scale.h"

namespace webrtc {
namespace test {

namespace {

const ColorTransformationMatrix kIdentityColorMatrix = {
    {{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}}};

void ExpectNear(const ColorTransformationMatrix& expected,
                const ColorTransformationMatrix& actual) {
  // The scaling factor on y/u/v should be pretty precise.
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 3; ++j)
      EXPECT_NEAR(expected[i][j], actual[i][j], /* abs_error= */ 1.0e-3)
          << "at element i: " << i << ", j: " << j;
  }
  // The offset can be less precise since the range is [0, 255].
  for (int i = 0; i < 3; ++i)
    EXPECT_NEAR(expected[i][3], actual[i][3], /* abs_error= */ 0.1)
        << "at element i: " << i;
}

}  // namespace

class VideoColorAlignerTest : public ::testing::Test {
 protected:
  void SetUp() {
    reference_video_ =
        OpenYuvFile(ResourcePath("foreman_128x96", "yuv"), 128, 96);
    ASSERT_TRUE(reference_video_);
  }

  rtc::scoped_refptr<Video> reference_video_;
};

TEST_F(VideoColorAlignerTest, AdjustColorsFrameIdentity) {
  const rtc::scoped_refptr<I420BufferInterface> test_frame =
      reference_video_->GetFrame(0);

  // Assume perfect match, i.e. ssim == 1.
  EXPECT_EQ(1.0,
            Ssim(test_frame, AdjustColors(kIdentityColorMatrix, test_frame)));
}

TEST_F(VideoColorAlignerTest, AdjustColorsFrame1x1) {
  const ColorTransformationMatrix color_matrix = {
      {{1, 2, 3, 4}, {5, 6, 7, 8}, {9, 10, 11, 12}}};

  const uint8_t data_y[] = {2};
  const uint8_t data_u[] = {6};
  const uint8_t data_v[] = {7};
  const rtc::scoped_refptr<I420BufferInterface> i420_buffer = I420Buffer::Copy(
      /* width= */ 1, /* height= */ 1, data_y, /* stride_y= */ 1, data_u,
      /* stride_u= */ 1, data_v, /* stride_v= */ 1);

  const rtc::scoped_refptr<I420BufferInterface> adjusted_buffer =
      AdjustColors(color_matrix, i420_buffer);

  EXPECT_EQ(2 * 1 + 6 * 2 + 7 * 3 + 4, adjusted_buffer->DataY()[0]);
  EXPECT_EQ(2 * 5 + 6 * 6 + 7 * 7 + 8, adjusted_buffer->DataU()[0]);
  EXPECT_EQ(2 * 9 + 6 * 10 + 7 * 11 + 12, adjusted_buffer->DataV()[0]);
}

TEST_F(VideoColorAlignerTest, AdjustColorsFrame1x1Negative) {
  const ColorTransformationMatrix color_matrix = {
      {{-1, 0, 0, 255}, {0, -1, 0, 255}, {0, 0, -1, 255}}};

  const uint8_t data_y[] = {2};
  const uint8_t data_u[] = {6};
  const uint8_t data_v[] = {7};
  const rtc::scoped_refptr<I420BufferInterface> i420_buffer = I420Buffer::Copy(
      /* width= */ 1, /* height= */ 1, data_y, /* stride_y= */ 1, data_u,
      /* stride_u= */ 1, data_v, /* stride_v= */ 1);

  const rtc::scoped_refptr<I420BufferInterface> adjusted_buffer =
      AdjustColors(color_matrix, i420_buffer);

  EXPECT_EQ(255 - 2, adjusted_buffer->DataY()[0]);
  EXPECT_EQ(255 - 6, adjusted_buffer->DataU()[0]);
  EXPECT_EQ(255 - 7, adjusted_buffer->DataV()[0]);
}

TEST_F(VideoColorAlignerTest, AdjustColorsFrame2x2) {
  const ColorTransformationMatrix color_matrix = {
      {{1, 2, 3, 4}, {5, 6, 7, 8}, {9, 10, 11, 12}}};

  const uint8_t data_y[] = {0, 1, 3, 4};
  const uint8_t data_u[] = {6};
  const uint8_t data_v[] = {7};
  const rtc::scoped_refptr<I420BufferInterface> i420_buffer = I420Buffer::Copy(
      /* width= */ 2, /* height= */ 2, data_y, /* stride_y= */ 2, data_u,
      /* stride_u= */ 1, data_v, /* stride_v= */ 1);

  const rtc::scoped_refptr<I420BufferInterface> adjusted_buffer =
      AdjustColors(color_matrix, i420_buffer);

  EXPECT_EQ(0 * 1 + 6 * 2 + 7 * 3 + 4, adjusted_buffer->DataY()[0]);
  EXPECT_EQ(1 * 1 + 6 * 2 + 7 * 3 + 4, adjusted_buffer->DataY()[1]);
  EXPECT_EQ(3 * 1 + 6 * 2 + 7 * 3 + 4, adjusted_buffer->DataY()[2]);
  EXPECT_EQ(4 * 1 + 6 * 2 + 7 * 3 + 4, adjusted_buffer->DataY()[3]);

  EXPECT_EQ(2 * 5 + 6 * 6 + 7 * 7 + 8, adjusted_buffer->DataU()[0]);
  EXPECT_EQ(2 * 9 + 6 * 10 + 7 * 11 + 12, adjusted_buffer->DataV()[0]);
}

TEST_F(VideoColorAlignerTest, CalculateColorTransformationMatrixIdentity) {
  EXPECT_EQ(kIdentityColorMatrix, CalculateColorTransformationMatrix(
                                      reference_video_, reference_video_));
}

TEST_F(VideoColorAlignerTest, CalculateColorTransformationMatrixOffset) {
  const uint8_t small_data_y[] = {0, 1, 2,  3,  4,  5,  6,  7,
                                  8, 9, 10, 11, 12, 13, 14, 15};
  const uint8_t small_data_u[] = {15, 13, 17, 29};
  const uint8_t small_data_v[] = {3, 200, 170, 29};
  const rtc::scoped_refptr<I420BufferInterface> small_i420_buffer =
      I420Buffer::Copy(
          /* width= */ 4, /* height= */ 4, small_data_y, /* stride_y= */ 4,
          small_data_u, /* stride_u= */ 2, small_data_v, /* stride_v= */ 2);

  uint8_t big_data_y[16];
  uint8_t big_data_u[4];
  uint8_t big_data_v[4];
  // Create another I420 frame where all values are 10 bigger.
  for (int i = 0; i < 16; ++i)
    big_data_y[i] = small_data_y[i] + 10;
  for (int i = 0; i < 4; ++i)
    big_data_u[i] = small_data_u[i] + 10;
  for (int i = 0; i < 4; ++i)
    big_data_v[i] = small_data_v[i] + 10;

  const rtc::scoped_refptr<I420BufferInterface> big_i420_buffer =
      I420Buffer::Copy(
          /* width= */ 4, /* height= */ 4, big_data_y, /* stride_y= */ 4,
          big_data_u, /* stride_u= */ 2, big_data_v, /* stride_v= */ 2);

  const ColorTransformationMatrix color_matrix =
      CalculateColorTransformationMatrix(big_i420_buffer, small_i420_buffer);

  ExpectNear({{{1, 0, 0, 10}, {0, 1, 0, 10}, {0, 0, 1, 10}}}, color_matrix);
}

TEST_F(VideoColorAlignerTest, CalculateColorTransformationMatrix) {
  // Arbitrary color transformation matrix.
  const ColorTransformationMatrix org_color_matrix = {
      {{0.8, 0.05, 0.04, -4}, {-0.2, 0.7, 0.1, 10}, {0.1, 0.2, 0.4, 20}}};

  const ColorTransformationMatrix result_color_matrix =
      CalculateColorTransformationMatrix(
          AdjustColors(org_color_matrix, reference_video_), reference_video_);

  ExpectNear(org_color_matrix, result_color_matrix);
}

}  // namespace test
}  // namespace webrtc
