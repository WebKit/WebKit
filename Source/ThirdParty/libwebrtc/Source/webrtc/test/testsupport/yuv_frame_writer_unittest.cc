/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>

#include "webrtc/test/gtest.h"
#include "webrtc/test/testsupport/fileutils.h"
#include "webrtc/test/testsupport/frame_writer.h"

namespace webrtc {
namespace test {

namespace {
const size_t kFrameWidth = 50;
const size_t kFrameHeight = 20;
const size_t kFrameLength = 3 * kFrameWidth * kFrameHeight / 2;  // I420.
}  // namespace

class YuvFrameWriterTest : public testing::Test {
 protected:
  YuvFrameWriterTest() = default;
  ~YuvFrameWriterTest() override = default;

  void SetUp() override {
    temp_filename_ = webrtc::test::TempFilename(webrtc::test::OutputPath(),
                                                "yuv_frame_writer_unittest");
    frame_writer_.reset(
        new YuvFrameWriterImpl(temp_filename_, kFrameWidth, kFrameHeight));
    ASSERT_TRUE(frame_writer_->Init());
  }

  void TearDown() override { remove(temp_filename_.c_str()); }

  std::unique_ptr<FrameWriter> frame_writer_;
  std::string temp_filename_;
};

TEST_F(YuvFrameWriterTest, InitSuccess) {}

TEST_F(YuvFrameWriterTest, FrameLength) {
  EXPECT_EQ(kFrameLength, frame_writer_->FrameLength());
}

TEST_F(YuvFrameWriterTest, WriteFrame) {
  uint8_t buffer[kFrameLength];
  memset(buffer, 9, kFrameLength);  // Write lots of 9s to the buffer.
  bool result = frame_writer_->WriteFrame(buffer);
  ASSERT_TRUE(result);

  frame_writer_->Close();
  EXPECT_EQ(kFrameLength, GetFileSize(temp_filename_));
}

TEST_F(YuvFrameWriterTest, WriteFrameUninitialized) {
  uint8_t buffer[kFrameLength];
  YuvFrameWriterImpl frame_writer(temp_filename_, kFrameWidth, kFrameHeight);
  EXPECT_FALSE(frame_writer.WriteFrame(buffer));
}

}  // namespace test
}  // namespace webrtc
