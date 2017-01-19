/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/test/testsupport/frame_reader.h"

#include "webrtc/test/gtest.h"
#include "webrtc/test/testsupport/fileutils.h"
#include "webrtc/common_video/include/video_frame_buffer.h"

namespace webrtc {
namespace test {

const std::string kInputFileContents = "baz";
const size_t kFrameLength = 3;

class FrameReaderTest: public testing::Test {
 protected:
  FrameReaderTest() {}
  virtual ~FrameReaderTest() {}
  void SetUp() {
    // Create a dummy input file.
    temp_filename_ = webrtc::test::TempFilename(webrtc::test::OutputPath(),
                                                "frame_reader_unittest");
    FILE* dummy = fopen(temp_filename_.c_str(), "wb");
    fprintf(dummy, "%s", kInputFileContents.c_str());
    fclose(dummy);

    frame_reader_ = new FrameReaderImpl(temp_filename_, 1, 1);
    ASSERT_TRUE(frame_reader_->Init());
  }
  void TearDown() {
    delete frame_reader_;
    // Cleanup the dummy input file.
    remove(temp_filename_.c_str());
  }
  FrameReader* frame_reader_;
  std::string temp_filename_;
};

TEST_F(FrameReaderTest, InitSuccess) {
  FrameReaderImpl frame_reader(temp_filename_, 1, 1);
  ASSERT_TRUE(frame_reader.Init());
  ASSERT_EQ(kFrameLength, frame_reader.FrameLength());
  ASSERT_EQ(1, frame_reader.NumberOfFrames());
}

TEST_F(FrameReaderTest, ReadFrame) {
  rtc::scoped_refptr<VideoFrameBuffer> buffer;
  buffer = frame_reader_->ReadFrame();
  ASSERT_TRUE(buffer);
  ASSERT_EQ(kInputFileContents[0], buffer->DataY()[0]);
  ASSERT_EQ(kInputFileContents[1], buffer->DataU()[0]);
  ASSERT_EQ(kInputFileContents[2], buffer->DataV()[0]);
  ASSERT_FALSE(frame_reader_->ReadFrame());  // End of file
}

TEST_F(FrameReaderTest, ReadFrameUninitialized) {
  FrameReaderImpl file_reader(temp_filename_, 1, 1);
  ASSERT_FALSE(file_reader.ReadFrame());
}

}  // namespace test
}  // namespace webrtc
