/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <memory>
#include <string>

#include "test/gtest.h"
#include "test/testsupport/file_utils.h"
#include "test/testsupport/frame_writer.h"

namespace webrtc {
namespace test {

namespace {
const size_t kFrameWidth = 50;
const size_t kFrameHeight = 20;
const size_t kFrameLength = 3 * kFrameWidth * kFrameHeight / 2;  // I420.
const size_t kFrameRate = 30;

const std::string kFileHeader = "YUV4MPEG2 W50 H20 F30:1 C420\n";
const std::string kFrameHeader = "FRAME\n";
}  // namespace

class Y4mFrameWriterTest : public ::testing::Test {
 protected:
  Y4mFrameWriterTest() = default;
  ~Y4mFrameWriterTest() override = default;

  void SetUp() override {
    temp_filename_ = webrtc::test::TempFilename(webrtc::test::OutputPath(),
                                                "y4m_frame_writer_unittest");
    frame_writer_.reset(new Y4mFrameWriterImpl(temp_filename_, kFrameWidth,
                                               kFrameHeight, kFrameRate));
    ASSERT_TRUE(frame_writer_->Init());
  }

  void TearDown() override { remove(temp_filename_.c_str()); }

  std::unique_ptr<FrameWriter> frame_writer_;
  std::string temp_filename_;
};

TEST_F(Y4mFrameWriterTest, InitSuccess) {}

TEST_F(Y4mFrameWriterTest, FrameLength) {
  EXPECT_EQ(kFrameLength, frame_writer_->FrameLength());
}

TEST_F(Y4mFrameWriterTest, WriteFrame) {
  uint8_t buffer[kFrameLength];
  memset(buffer, 9, kFrameLength);  // Write lots of 9s to the buffer.
  bool result = frame_writer_->WriteFrame(buffer);
  ASSERT_TRUE(result);
  result = frame_writer_->WriteFrame(buffer);
  ASSERT_TRUE(result);

  frame_writer_->Close();
  EXPECT_EQ(kFileHeader.size() + 2 * kFrameHeader.size() + 2 * kFrameLength,
            GetFileSize(temp_filename_));
}

TEST_F(Y4mFrameWriterTest, WriteFrameUninitialized) {
  uint8_t buffer[kFrameLength];
  Y4mFrameWriterImpl frame_writer(temp_filename_, kFrameWidth, kFrameHeight,
                                  kFrameRate);
  EXPECT_FALSE(frame_writer.WriteFrame(buffer));
}

}  // namespace test
}  // namespace webrtc
