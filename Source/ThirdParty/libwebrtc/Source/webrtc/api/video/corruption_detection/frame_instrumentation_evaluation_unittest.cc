/*
 * Copyright 2024 The WebRTC project authors. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file in the root of the source
 * tree. An additional intellectual property rights grant can be found
 * in the file PATENTS.  All contributing project authors may
 * be found in the AUTHORS file in the root of the source tree.
 */

#include "api/video/corruption_detection/frame_instrumentation_evaluation.h"

#include <cstdint>
#include <memory>
#include <vector>

#include "api/scoped_refptr.h"
#include "api/video/corruption_detection/frame_instrumentation_data.h"
#include "api/video/i420_buffer.h"
#include "api/video/video_content_type.h"
#include "api/video/video_frame.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::_;
using ::testing::AllOf;
using ::testing::Ge;
using ::testing::Le;

class MockCorruptionScoreObserver : public CorruptionScoreObserver {
 public:
  MOCK_METHOD(void, OnCorruptionScore, (double, VideoContentType), (override));
};

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
  FrameInstrumentationData data;
  VideoFrame frame =
      VideoFrame::Builder()
          .set_video_frame_buffer(MakeI420FrameBufferWithDifferentPixelValues())
          .build();

  MockCorruptionScoreObserver observer;
  std::unique_ptr<FrameInstrumentationEvaluation> evaluator =
      FrameInstrumentationEvaluation::Create(&observer);
  EXPECT_CALL(observer, OnCorruptionScore).Times(0);
  evaluator->OnInstrumentedFrame(data, frame, VideoContentType::UNSPECIFIED);
}

TEST(FrameInstrumentationEvaluationTest,
     HaveACorruptionScoreWhenSampleValuesAreProvided) {
  FrameInstrumentationData data;
  data.SetStdDev(1.0);
  data.SetSampleValues({12, 12, 12, 12, 12, 12, 12, 12});
  VideoFrame frame =
      VideoFrame::Builder()
          .set_video_frame_buffer(MakeI420FrameBufferWithDifferentPixelValues())
          .build();

  MockCorruptionScoreObserver observer;
  std::unique_ptr<FrameInstrumentationEvaluation> evaluator =
      FrameInstrumentationEvaluation::Create(&observer);
  EXPECT_CALL(observer, OnCorruptionScore(1.0, VideoContentType::SCREENSHARE));
  evaluator->OnInstrumentedFrame(data, frame, VideoContentType::SCREENSHARE);
}

TEST(FrameInstrumentationEvaluationTest,
     ApplyThresholdsWhenNonNegativeThresholdsAreProvided) {
  FrameInstrumentationData data;
  data.SetStdDev(1.0);
  data.SetLumaErrorThreshold(8);
  data.SetChromaErrorThreshold(8);
  data.SetSampleValues({12, 12, 12, 12, 12, 12, 12, 12});
  VideoFrame frame =
      VideoFrame::Builder()
          .set_video_frame_buffer(MakeI420FrameBufferWithDifferentPixelValues())
          .build();

  MockCorruptionScoreObserver observer;
  std::unique_ptr<FrameInstrumentationEvaluation> evaluator =
      FrameInstrumentationEvaluation::Create(&observer);
  EXPECT_CALL(observer, OnCorruptionScore(AllOf(Ge(0.0), Le(1.0)), _));
  evaluator->OnInstrumentedFrame(data, frame, VideoContentType::UNSPECIFIED);
}

TEST(FrameInstrumentationEvaluationTest,
     ApplyStdDevWhenNonNegativeStdDevIsProvided) {
  FrameInstrumentationData data;
  data.SetStdDev(0.6);
  data.SetLumaErrorThreshold(8);
  data.SetChromaErrorThreshold(8);
  data.SetSampleValues({12, 12, 12, 12, 12, 12, 12, 12});

  VideoFrame frame =
      VideoFrame::Builder()
          .set_video_frame_buffer(MakeI420FrameBufferWithDifferentPixelValues())
          .build();

  MockCorruptionScoreObserver observer;
  std::unique_ptr<FrameInstrumentationEvaluation> evaluator =
      FrameInstrumentationEvaluation::Create(&observer);
  EXPECT_CALL(observer, OnCorruptionScore(AllOf(Ge(0.0), Le(1.0)), _));
  evaluator->OnInstrumentedFrame(data, frame, VideoContentType::UNSPECIFIED);
}

TEST(FrameInstrumentationEvaluationTest, ApplySequenceIndexWhenProvided) {
  FrameInstrumentationData data;
  data.SetSequenceIndex(1);
  data.SetStdDev(0.6);
  data.SetLumaErrorThreshold(8);
  data.SetChromaErrorThreshold(8);
  data.SetSampleValues({12, 12, 12, 12, 12, 12, 12, 12});

  VideoFrame frame =
      VideoFrame::Builder()
          .set_video_frame_buffer(MakeI420FrameBufferWithDifferentPixelValues())
          .build();

  MockCorruptionScoreObserver observer;
  std::unique_ptr<FrameInstrumentationEvaluation> evaluator =
      FrameInstrumentationEvaluation::Create(&observer);
  EXPECT_CALL(observer, OnCorruptionScore(AllOf(Ge(0.0), Le(1.0)), _));
  evaluator->OnInstrumentedFrame(data, frame, VideoContentType::UNSPECIFIED);
}

}  // namespace
}  // namespace webrtc
