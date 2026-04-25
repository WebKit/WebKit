/*
 * Copyright 2026 The WebRTC project authors. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file in the root of the source
 * tree. An additional intellectual property rights grant can be found
 * in the file PATENTS.  All contributing project authors may
 * be found in the AUTHORS file in the root of the source tree.
 */

#include "test/video_frame_matcher.h"

#include <array>
#include <cstdint>

#include "api/video/i420_buffer.h"
#include "api/video/video_frame.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

constexpr int kWidth = 4;
constexpr int kHeight = 4;
constexpr int kChromaWidth = 2;
constexpr int kChromaHeight = 2;
constexpr int kStrideY = kWidth;
constexpr int kStrideU = kChromaWidth;
constexpr int kStrideV = kChromaWidth;

// An arbitrary 4x4 raw YUV420 frame.
constexpr std::array<uint8_t, kWidth * kHeight> kFrameYContent = {
    12,  5,   7,   11,  //
    159, 15,  11,  0,   //
    4,   240, 131, 59,  //
    61,  87,  11,  0    //
};
constexpr std::array<uint8_t, kChromaWidth * kChromaHeight> kFrameUContent = {
    248, 184,  //
    139, 229   //
};
constexpr std::array<uint8_t, kChromaWidth * kChromaHeight> kFrameVContent = {
    32, 69,  //
    7, 193   //
};

VideoFrame CreateTestFrameWithData() {
  return VideoFrame::Builder()
      .set_video_frame_buffer(I420Buffer::Copy(
          kWidth, kHeight, kFrameYContent.data(), kStrideY,
          kFrameUContent.data(), kStrideU, kFrameVContent.data(), kStrideV))
      .build();
}

TEST(PixelValuesEqualMatcherTest, ExpectedEqualActualTest) {
  EXPECT_THAT(CreateTestFrameWithData(),
              PixelValuesEqual(CreateTestFrameWithData()));
}

TEST(PixelValuesEqualMatcherTest, ExpectedYNotEqualActualYTest) {
  std::array<uint8_t, kWidth * kHeight> frame_wrong_y_content = kFrameYContent;
  frame_wrong_y_content[5] = 12;
  EXPECT_THAT(VideoFrame::Builder()
                  .set_video_frame_buffer(I420Buffer::Copy(
                      kWidth, kHeight, frame_wrong_y_content.data(), kWidth,
                      kFrameUContent.data(), kChromaWidth,
                      kFrameVContent.data(), kChromaWidth))
                  .build(),
              Not(PixelValuesEqual(CreateTestFrameWithData())));
}

TEST(PixelValuesEqualMatcherTest, ExpectedUNotEqualActualUTest) {
  std::array<uint8_t, kChromaWidth * kChromaHeight> frame_wrong_u_content =
      kFrameUContent;
  frame_wrong_u_content[1] = 14;
  EXPECT_THAT(VideoFrame::Builder()
                  .set_video_frame_buffer(I420Buffer::Copy(
                      kWidth, kHeight, kFrameYContent.data(), kWidth,
                      frame_wrong_u_content.data(), kChromaWidth,
                      kFrameVContent.data(), kChromaWidth))
                  .build(),
              Not(PixelValuesEqual(CreateTestFrameWithData())));
}

TEST(PixelValuesEqualMatcherTest, ExpectedVNotEqualActualVTest) {
  std::array<uint8_t, kChromaWidth * kChromaHeight> frame_wrong_v_content =
      kFrameVContent;
  frame_wrong_v_content[1] = 14;
  EXPECT_THAT(VideoFrame::Builder()
                  .set_video_frame_buffer(I420Buffer::Copy(
                      kWidth, kHeight, kFrameYContent.data(), kWidth,
                      kFrameUContent.data(), kChromaWidth,
                      frame_wrong_v_content.data(), kChromaWidth))
                  .build(),
              Not(PixelValuesEqual(CreateTestFrameWithData())));
}

}  // namespace
}  // namespace webrtc
