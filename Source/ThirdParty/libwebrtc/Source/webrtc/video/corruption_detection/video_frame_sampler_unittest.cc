/*
 * Copyright 2025 The WebRTC project authors. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file in the root of the source
 * tree. An additional intellectual property rights grant can be found
 * in the file PATENTS.  All contributing project authors may
 * be found in the AUTHORS file in the root of the source tree.
 */

#include "video/corruption_detection/video_frame_sampler.h"

#include <cstdint>
#include <cstring>
#include <memory>

#include "api/scoped_refptr.h"
#include "api/video/i420_buffer.h"
#include "api/video/i444_buffer.h"
#include "api/video/nv12_buffer.h"
#include "api/video/video_frame.h"
#include "test/gmock.h"
#include "test/gtest.h"

using ::testing::_;

namespace webrtc {

scoped_refptr<I420Buffer> MakeSimpleI420FrameBuffer() {
  // Create a simple I420 frame of size 4x4 with each sample having a unique
  // value. An additional stride is used with a "poisoned" value of 99.
  scoped_refptr<I420Buffer> buffer = I420Buffer::Create(/*width=*/4,
                                                        /*height=*/4,
                                                        /*stride_y=*/5,
                                                        /*stride_u=*/3,
                                                        /*stride_v=*/3);
  const uint8_t kYContent[] = {
      // clang-format off
      1,  2,  3,  4,  99,
      5,  6,  7,  8,  99,
      9,  10, 11, 12, 99,
      13, 14, 15, 16, 99
      // clang-format on
  };
  memcpy(buffer->MutableDataY(), kYContent, sizeof(kYContent));

  const uint8_t kUContent[] = {
      // clang-format off
      17, 18, 99,
      19, 20, 99
      // clang-format on
  };
  memcpy(buffer->MutableDataU(), kUContent, sizeof(kUContent));

  const uint8_t kVContent[] = {
      // clang-format off
      21, 22, 99,
      23, 24, 99
      // clang-format on
  };
  memcpy(buffer->MutableDataV(), kVContent, sizeof(kVContent));

  return buffer;
}

scoped_refptr<I444Buffer> MakeSimpleI444FrameBuffer() {
  // Create an I444 frame, with the same contents as `MakeSimpleI420FrameBuffer`
  // just upscaled with nearest-neighbour.
  scoped_refptr<I444Buffer> buffer = I444Buffer::Create(/*width=*/4,
                                                        /*height=*/4,
                                                        /*stride_y=*/5,
                                                        /*stride_u=*/5,
                                                        /*stride_v=*/5);
  const uint8_t kYContent[] = {
      // clang-format off
      1,  2,  3,  4,  99,
      5,  6,  7,  8,  99,
      9,  10, 11, 12, 99,
      13, 14, 15, 16, 99
      // clang-format on
  };
  memcpy(buffer->MutableDataY(), kYContent, sizeof(kYContent));

  const uint8_t kUContent[] = {
      // clang-format off
      17, 17, 18, 18, 99,
      17, 17, 18, 18, 99,
      19, 19, 20, 20, 99,
      19, 19, 20, 20, 99
      // clang-format on
  };
  memcpy(buffer->MutableDataU(), kUContent, sizeof(kUContent));

  const uint8_t kVContent[] = {
      // clang-format off
      21, 21, 22, 22, 99,
      21, 21, 22, 22, 99,
      23, 23, 24, 24, 99,
      23, 23, 24, 24, 99
      // clang-format on
  };
  memcpy(buffer->MutableDataV(), kVContent, sizeof(kVContent));

  return buffer;
}

std::unique_ptr<VideoFrameSampler> GetDefaultSampler() {
  return VideoFrameSampler::Create(
      VideoFrame::Builder()
          .set_video_frame_buffer(MakeSimpleI420FrameBuffer())
          .build());
}

TEST(VideoFrameSampler, ParsesI420YChannel) {
  std::unique_ptr<VideoFrameSampler> sampler = GetDefaultSampler();
  int val = 1;
  for (int row = 0; row < 4; ++row) {
    for (int col = 0; col < 4; ++col) {
      EXPECT_EQ(
          sampler->GetSampleValue(VideoFrameSampler::ChannelType::Y, col, row),
          val++);
    }
  }
}

TEST(VideoFrameSampler, ParsesI420UChannel) {
  std::unique_ptr<VideoFrameSampler> sampler = GetDefaultSampler();
  EXPECT_EQ(sampler->GetSampleValue(VideoFrameSampler::ChannelType::U, 0, 0),
            17);
  EXPECT_EQ(sampler->GetSampleValue(VideoFrameSampler::ChannelType::U, 1, 0),
            18);
  EXPECT_EQ(sampler->GetSampleValue(VideoFrameSampler::ChannelType::U, 0, 1),
            19);
  EXPECT_EQ(sampler->GetSampleValue(VideoFrameSampler::ChannelType::U, 1, 1),
            20);
}

TEST(VideoFrameSampler, ParsesI420VChannel) {
  std::unique_ptr<VideoFrameSampler> sampler = GetDefaultSampler();
  EXPECT_EQ(sampler->GetSampleValue(VideoFrameSampler::ChannelType::V, 0, 0),
            21);
  EXPECT_EQ(sampler->GetSampleValue(VideoFrameSampler::ChannelType::V, 1, 0),
            22);
  EXPECT_EQ(sampler->GetSampleValue(VideoFrameSampler::ChannelType::V, 0, 1),
            23);
  EXPECT_EQ(sampler->GetSampleValue(VideoFrameSampler::ChannelType::V, 1, 1),
            24);
}

TEST(VideoFrameSampler, ReportsI420Resolution) {
  std::unique_ptr<VideoFrameSampler> sampler = GetDefaultSampler();
  EXPECT_EQ(sampler->width(VideoFrameSampler::ChannelType::Y), 4);
  EXPECT_EQ(sampler->height(VideoFrameSampler::ChannelType::Y), 4);
  EXPECT_EQ(sampler->width(VideoFrameSampler::ChannelType::U), 2);
  EXPECT_EQ(sampler->height(VideoFrameSampler::ChannelType::U), 2);
  EXPECT_EQ(sampler->width(VideoFrameSampler::ChannelType::V), 2);
  EXPECT_EQ(sampler->height(VideoFrameSampler::ChannelType::V), 2);
}

TEST(VideoFrameSampler, ParsesNV12YChannel) {
  std::unique_ptr<VideoFrameSampler> sampler =
      VideoFrameSampler::Create(VideoFrame::Builder()
                                    .set_video_frame_buffer(NV12Buffer::Copy(
                                        *MakeSimpleI420FrameBuffer()))
                                    .build());
  int val = 1;
  for (int row = 0; row < 4; ++row) {
    for (int col = 0; col < 4; ++col) {
      EXPECT_EQ(
          sampler->GetSampleValue(VideoFrameSampler::ChannelType::Y, col, row),
          val++);
    }
  }
}

TEST(VideoFrameSampler, ParsesNV12UChannel) {
  std::unique_ptr<VideoFrameSampler> sampler =
      VideoFrameSampler::Create(VideoFrame::Builder()
                                    .set_video_frame_buffer(NV12Buffer::Copy(
                                        *MakeSimpleI420FrameBuffer()))
                                    .build());
  EXPECT_EQ(sampler->GetSampleValue(VideoFrameSampler::ChannelType::U, 0, 0),
            17);
  EXPECT_EQ(sampler->GetSampleValue(VideoFrameSampler::ChannelType::U, 1, 0),
            18);
  EXPECT_EQ(sampler->GetSampleValue(VideoFrameSampler::ChannelType::U, 0, 1),
            19);
  EXPECT_EQ(sampler->GetSampleValue(VideoFrameSampler::ChannelType::U, 1, 1),
            20);
}

TEST(VideoFrameSampler, ParsesNV12VChannel) {
  std::unique_ptr<VideoFrameSampler> sampler =
      VideoFrameSampler::Create(VideoFrame::Builder()
                                    .set_video_frame_buffer(NV12Buffer::Copy(
                                        *MakeSimpleI420FrameBuffer()))
                                    .build());
  EXPECT_EQ(sampler->GetSampleValue(VideoFrameSampler::ChannelType::V, 0, 0),
            21);
  EXPECT_EQ(sampler->GetSampleValue(VideoFrameSampler::ChannelType::V, 1, 0),
            22);
  EXPECT_EQ(sampler->GetSampleValue(VideoFrameSampler::ChannelType::V, 0, 1),
            23);
  EXPECT_EQ(sampler->GetSampleValue(VideoFrameSampler::ChannelType::V, 1, 1),
            24);
}

TEST(VideoFrameSampler, ReportsNV12Resolution) {
  std::unique_ptr<VideoFrameSampler> sampler =
      VideoFrameSampler::Create(VideoFrame::Builder()
                                    .set_video_frame_buffer(NV12Buffer::Copy(
                                        *MakeSimpleI420FrameBuffer()))
                                    .build());
  EXPECT_EQ(sampler->width(VideoFrameSampler::ChannelType::Y), 4);
  EXPECT_EQ(sampler->height(VideoFrameSampler::ChannelType::Y), 4);
  EXPECT_EQ(sampler->width(VideoFrameSampler::ChannelType::U), 2);
  EXPECT_EQ(sampler->height(VideoFrameSampler::ChannelType::U), 2);
  EXPECT_EQ(sampler->width(VideoFrameSampler::ChannelType::V), 2);
  EXPECT_EQ(sampler->height(VideoFrameSampler::ChannelType::V), 2);
}

TEST(VideoFrameSampler, ParsesI444YChannel) {
  // I444 will be converted to I420, but the Y channel should remain unchanged.
  std::unique_ptr<VideoFrameSampler> sampler = VideoFrameSampler::Create(
      VideoFrame::Builder()
          .set_video_frame_buffer(MakeSimpleI444FrameBuffer())
          .build());
  int val = 1;
  for (int row = 0; row < 4; ++row) {
    for (int col = 0; col < 4; ++col) {
      EXPECT_EQ(
          sampler->GetSampleValue(VideoFrameSampler::ChannelType::Y, col, row),
          val++);
    }
  }
}

TEST(VideoFrameSampler, ParsesI444UChannel) {
  // I444 will be converted to I420, with U/V dowscaled by 2x.
  std::unique_ptr<VideoFrameSampler> sampler = VideoFrameSampler::Create(
      VideoFrame::Builder()
          .set_video_frame_buffer(MakeSimpleI444FrameBuffer())
          .build());
  EXPECT_EQ(sampler->GetSampleValue(VideoFrameSampler::ChannelType::U, 0, 0),
            17);
  EXPECT_EQ(sampler->GetSampleValue(VideoFrameSampler::ChannelType::U, 1, 0),
            18);
  EXPECT_EQ(sampler->GetSampleValue(VideoFrameSampler::ChannelType::U, 0, 1),
            19);
  EXPECT_EQ(sampler->GetSampleValue(VideoFrameSampler::ChannelType::U, 1, 1),
            20);
}

TEST(VideoFrameSampler, ParsesI444VChannel) {
  // I444 will be converted to I420, with U/V dowscaled by 2x.
  std::unique_ptr<VideoFrameSampler> sampler = VideoFrameSampler::Create(
      VideoFrame::Builder()
          .set_video_frame_buffer(MakeSimpleI444FrameBuffer())
          .build());
  EXPECT_EQ(sampler->GetSampleValue(VideoFrameSampler::ChannelType::V, 0, 0),
            21);
  EXPECT_EQ(sampler->GetSampleValue(VideoFrameSampler::ChannelType::V, 1, 0),
            22);
  EXPECT_EQ(sampler->GetSampleValue(VideoFrameSampler::ChannelType::V, 0, 1),
            23);
  EXPECT_EQ(sampler->GetSampleValue(VideoFrameSampler::ChannelType::V, 1, 1),
            24);
}

TEST(VideoFrameSampler, ReportsI444Resolution) {
  // I444 will be converted to I420, with U/V dowscaled by 2x.
  std::unique_ptr<VideoFrameSampler> sampler = VideoFrameSampler::Create(
      VideoFrame::Builder()
          .set_video_frame_buffer(MakeSimpleI444FrameBuffer())
          .build());
  EXPECT_EQ(sampler->width(VideoFrameSampler::ChannelType::Y), 4);
  EXPECT_EQ(sampler->height(VideoFrameSampler::ChannelType::Y), 4);
  EXPECT_EQ(sampler->width(VideoFrameSampler::ChannelType::U), 2);
  EXPECT_EQ(sampler->height(VideoFrameSampler::ChannelType::U), 2);
  EXPECT_EQ(sampler->width(VideoFrameSampler::ChannelType::V), 2);
  EXPECT_EQ(sampler->height(VideoFrameSampler::ChannelType::V), 2);
}

#if GTEST_HAS_DEATH_TEST
TEST(VideoFrameSampler, RejectsNegativeColumn) {
  EXPECT_DEATH(GetDefaultSampler()->GetSampleValue(
                   VideoFrameSampler::ChannelType::Y, -1, 0),
               _);
}

TEST(VideoFrameSampler, RejectsNegativeRow) {
  EXPECT_DEATH(GetDefaultSampler()->GetSampleValue(
                   VideoFrameSampler::ChannelType::Y, 0, -1),
               _);
}

TEST(VideoFrameSampler, RejectsTooLargeYColumn) {
  EXPECT_DEATH(GetDefaultSampler()->GetSampleValue(
                   VideoFrameSampler::ChannelType::Y, 4, 0),
               _);
}

TEST(VideoFrameSampler, RejectsTooLargeYRow) {
  EXPECT_DEATH(GetDefaultSampler()->GetSampleValue(
                   VideoFrameSampler::ChannelType::Y, 5, 0),
               _);
}

TEST(VideoFrameSampler, RejectsTooLargeUColumn) {
  EXPECT_DEATH(GetDefaultSampler()->GetSampleValue(
                   VideoFrameSampler::ChannelType::U, 3, 0),
               _);
}

TEST(VideoFrameSampler, RejectsTooLargeURow) {
  EXPECT_DEATH(GetDefaultSampler()->GetSampleValue(
                   VideoFrameSampler::ChannelType::U, 2, 0),
               _);
}

TEST(VideoFrameSampler, RejectsTooLargeVColumn) {
  EXPECT_DEATH(GetDefaultSampler()->GetSampleValue(
                   VideoFrameSampler::ChannelType::V, 2, 0),
               _);
}

TEST(VideoFrameSampler, RejectsTooLargeVRow) {
  EXPECT_DEATH(GetDefaultSampler()->GetSampleValue(
                   VideoFrameSampler::ChannelType::V, 2, 0),
               _);
}
#endif  // GTEST_HAS_DEATH_TEST

}  // namespace webrtc
