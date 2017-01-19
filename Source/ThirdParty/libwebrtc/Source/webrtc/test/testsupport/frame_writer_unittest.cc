/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/test/testsupport/frame_writer.h"

#include "webrtc/test/gtest.h"
#include "webrtc/test/testsupport/fileutils.h"

namespace webrtc {
namespace test {

const size_t kFrameLength = 1000;

class FrameWriterTest: public testing::Test {
 protected:
  FrameWriterTest() {}
  virtual ~FrameWriterTest() {}
  void SetUp() {
    temp_filename_ = webrtc::test::TempFilename(webrtc::test::OutputPath(),
                                                "frame_writer_unittest");
    frame_writer_ = new FrameWriterImpl(temp_filename_, kFrameLength);
    ASSERT_TRUE(frame_writer_->Init());
  }
  void TearDown() {
    delete frame_writer_;
    // Cleanup the temporary file.
    remove(temp_filename_.c_str());
  }
  FrameWriter* frame_writer_;
  std::string temp_filename_;
};

TEST_F(FrameWriterTest, InitSuccess) {
  FrameWriterImpl frame_writer(temp_filename_, kFrameLength);
  ASSERT_TRUE(frame_writer.Init());
  ASSERT_EQ(kFrameLength, frame_writer.FrameLength());
}

TEST_F(FrameWriterTest, WriteFrame) {
  uint8_t buffer[kFrameLength];
  memset(buffer, 9, kFrameLength);  // Write lots of 9s to the buffer
  bool result = frame_writer_->WriteFrame(buffer);
  ASSERT_TRUE(result);  // success
  // Close the file and verify the size.
  frame_writer_->Close();
  ASSERT_EQ(kFrameLength, GetFileSize(temp_filename_));
}

TEST_F(FrameWriterTest, WriteFrameUninitialized) {
  uint8_t buffer[3];
  FrameWriterImpl frame_writer(temp_filename_, kFrameLength);
  ASSERT_FALSE(frame_writer.WriteFrame(buffer));
}

}  // namespace test
}  // namespace webrtc
