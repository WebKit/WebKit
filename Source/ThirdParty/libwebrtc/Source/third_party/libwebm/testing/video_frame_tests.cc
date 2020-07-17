// Copyright (c) 2016 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include "common/video_frame.h"

#include "gtest/gtest.h"

namespace {
const libwebm::VideoFrame::Codec kCodec = libwebm::VideoFrame::kVP8;
const std::int64_t kPts = 12345;
const std::size_t kSize = 1;
const std::size_t kEmptySize = 0;

TEST(VideoFrameTests, DefaultsTest) {
  libwebm::VideoFrame frame;
  EXPECT_EQ(kEmptySize, frame.buffer().capacity);
  EXPECT_EQ(kEmptySize, frame.buffer().length);
  EXPECT_EQ(nullptr, frame.buffer().data.get());
  EXPECT_FALSE(frame.keyframe());
  EXPECT_EQ(0, frame.nanosecond_pts());
  EXPECT_EQ(libwebm::VideoFrame::kVP9, frame.codec());
}

TEST(VideoFrameTests, SizeTest) {
  libwebm::VideoFrame frame;
  EXPECT_TRUE(frame.Init(kSize));

  // Buffer inits empty, length should be 0, aka |kEmpty|.
  EXPECT_GT(kSize, frame.buffer().length);
  EXPECT_EQ(kEmptySize, frame.buffer().length);

  // Capacity should be equal to |kSize|.
  EXPECT_EQ(kSize, frame.buffer().capacity);
  EXPECT_FALSE(frame.SetBufferLength(kSize + 1));

  // Write a byte into the buffer via the raw data pointer, update length, and
  // verify expected behavior.
  uint8_t* write_ptr = reinterpret_cast<uint8_t*>(frame.buffer().data.get());
  *write_ptr = 0xFF;
  EXPECT_TRUE(frame.SetBufferLength(1));
  EXPECT_EQ(frame.buffer().length, frame.buffer().capacity);
}

TEST(VideoFrameTests, OverloadsTest) {
  const bool kKeyframe = true;

  // Test VideoFrame::VideoFrame(bool keyframe, int64_t nano_pts, Codec c).
  libwebm::VideoFrame keyframe(kKeyframe, kPts, kCodec);
  EXPECT_EQ(kKeyframe, keyframe.keyframe());
  EXPECT_EQ(kPts, keyframe.nanosecond_pts());
  EXPECT_EQ(kCodec, keyframe.codec());
  EXPECT_EQ(kEmptySize, keyframe.buffer().capacity);
  EXPECT_EQ(kEmptySize, keyframe.buffer().length);
  EXPECT_EQ(nullptr, keyframe.buffer().data.get());

  // Test VideoFrame::Init(std::size_t length).
  EXPECT_TRUE(keyframe.Init(kSize));
  EXPECT_EQ(kKeyframe, keyframe.keyframe());
  EXPECT_EQ(kPts, keyframe.nanosecond_pts());
  EXPECT_EQ(kCodec, keyframe.codec());
  EXPECT_NE(nullptr, keyframe.buffer().data.get());

  // Test VideoFrame::Init(size_t length, int64_t nano_pts, Codec c).
  EXPECT_TRUE(keyframe.Init(kSize, kPts + 1, libwebm::VideoFrame::kVP9));
  EXPECT_EQ(kSize, keyframe.buffer().capacity);
  EXPECT_GT(kSize, keyframe.buffer().length);
  EXPECT_NE(kPts, keyframe.nanosecond_pts());
  EXPECT_NE(kCodec, keyframe.codec());
}

}  // namespace
