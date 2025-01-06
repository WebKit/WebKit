/*
 * Copyright 2024 The WebRTC project authors. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file in the root of the source
 * tree. An additional intellectual property rights grant can be found
 * in the file PATENTS.  All contributing project authors may
 * be found in the AUTHORS file in the root of the source tree.
 */

#include "video/corruption_detection/frame_instrumentation_evaluation.h"

#include <cstdint>
#include <optional>
#include <vector>

#include "api/scoped_refptr.h"
#include "api/video/i420_buffer.h"
#include "api/video/video_frame.h"
#include "common_video/frame_instrumentation_data.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

scoped_refptr<I420Buffer> MakeI420FrameBufferWithDifferentPixelValues() {
  // Create an I420 frame of size 4x4.
  const int kDefaultLumaWidth = 4;
  const int kDefaultLumaHeight = 4;
  const int kDefaultChromaWidth = 2;
  std::vector<uint8_t> kDefaultYContent = {1, 2,  3,  4,  5,  6,  7,  8,
                                           9, 10, 11, 12, 13, 14, 15, 16};
  std::vector<uint8_t> kDefaultUContent = {17, 18, 19, 20};
  std::vector<uint8_t> kDefaultVContent = {21, 22, 23, 24};

  return I420Buffer::Copy(kDefaultLumaWidth, kDefaultLumaHeight,
                          kDefaultYContent.data(), kDefaultLumaWidth,
                          kDefaultUContent.data(), kDefaultChromaWidth,
                          kDefaultVContent.data(), kDefaultChromaWidth);
}

TEST(FrameInstrumentationEvaluationTest,
     HaveNoCorruptionScoreWhenNoSampleValuesAreProvided) {
  FrameInstrumentationData data = {.sequence_index = 0,
                                   .communicate_upper_bits = false,
                                   .std_dev = 0.0,
                                   .luma_error_threshold = 0,
                                   .chroma_error_threshold = 0,
                                   .sample_values = {}};
  VideoFrame frame =
      VideoFrame::Builder()
          .set_video_frame_buffer(MakeI420FrameBufferWithDifferentPixelValues())
          .build();

  std::optional<double> corruption_score = GetCorruptionScore(data, frame);

  EXPECT_FALSE(corruption_score.has_value());
}

TEST(FrameInstrumentationEvaluationTest,
     HaveACorruptionScoreWhenSampleValuesAreProvided) {
  FrameInstrumentationData data = {
      .sequence_index = 0,
      .communicate_upper_bits = false,
      .std_dev = 0.0,
      .luma_error_threshold = 0,
      .chroma_error_threshold = 0,
      .sample_values = {12, 12, 12, 12, 12, 12, 12, 12}};
  VideoFrame frame =
      VideoFrame::Builder()
          .set_video_frame_buffer(MakeI420FrameBufferWithDifferentPixelValues())
          .build();

  std::optional<double> corruption_score = GetCorruptionScore(data, frame);

  ASSERT_TRUE(corruption_score.has_value());
  EXPECT_DOUBLE_EQ(*corruption_score, 1.0);
}

TEST(FrameInstrumentationEvaluationTest,
     ApplyThresholdsWhenNonNegativeThresholdsAreProvided) {
  FrameInstrumentationData data = {
      .sequence_index = 0,
      .communicate_upper_bits = false,
      .std_dev = 0.0,
      .luma_error_threshold = 8,
      .chroma_error_threshold = 8,
      .sample_values = {12, 12, 12, 12, 12, 12, 12, 12}};
  VideoFrame frame =
      VideoFrame::Builder()
          .set_video_frame_buffer(MakeI420FrameBufferWithDifferentPixelValues())
          .build();

  std::optional<double> corruption_score = GetCorruptionScore(data, frame);

  ASSERT_TRUE(corruption_score.has_value());
  EXPECT_LE(*corruption_score, 1);
  EXPECT_GE(*corruption_score, 0);
}

TEST(FrameInstrumentationEvaluationTest,
     ApplyStdDevWhenNonNegativeStdDevIsProvided) {
  FrameInstrumentationData data = {
      .sequence_index = 0,
      .communicate_upper_bits = false,
      .std_dev = 0.6,
      .luma_error_threshold = 8,
      .chroma_error_threshold = 8,
      .sample_values = {12, 12, 12, 12, 12, 12, 12, 12}};

  std::vector<double> sample_values = {12, 12, 12, 12, 12, 12, 12, 12};
  VideoFrame frame =
      VideoFrame::Builder()
          .set_video_frame_buffer(MakeI420FrameBufferWithDifferentPixelValues())
          .build();

  std::optional<double> corruption_score = GetCorruptionScore(data, frame);

  ASSERT_TRUE(corruption_score.has_value());
  EXPECT_LE(*corruption_score, 1);
  EXPECT_GE(*corruption_score, 0);
}

TEST(FrameInstrumentationEvaluationTest, ApplySequenceIndexWhenProvided) {
  FrameInstrumentationData data = {
      .sequence_index = 1,
      .communicate_upper_bits = false,
      .std_dev = 0.6,
      .luma_error_threshold = 8,
      .chroma_error_threshold = 8,
      .sample_values = {12, 12, 12, 12, 12, 12, 12, 12}};

  std::vector<double> sample_values = {12, 12, 12, 12, 12, 12, 12, 12};
  VideoFrame frame =
      VideoFrame::Builder()
          .set_video_frame_buffer(MakeI420FrameBufferWithDifferentPixelValues())
          .build();

  std::optional<double> corruption_score = GetCorruptionScore(data, frame);

  ASSERT_TRUE(corruption_score.has_value());
  EXPECT_LE(*corruption_score, 1);
  EXPECT_GE(*corruption_score, 0);
}

}  // namespace
}  // namespace webrtc
