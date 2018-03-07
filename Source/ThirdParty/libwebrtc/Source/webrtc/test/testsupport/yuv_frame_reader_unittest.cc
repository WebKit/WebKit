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
#include <string>

#include "api/video/i420_buffer.h"
#include "test/gtest.h"
#include "test/testsupport/fileutils.h"
#include "test/testsupport/frame_reader.h"

namespace webrtc {
namespace test {

namespace {
const std::string kInputFileContents = "bazouk";

const size_t kFrameWidth = 2;
const size_t kFrameHeight = 2;
const size_t kFrameLength = 3 * kFrameWidth * kFrameHeight / 2;  // I420.
}  // namespace

class YuvFrameReaderTest : public testing::Test {
 protected:
  YuvFrameReaderTest() = default;
  ~YuvFrameReaderTest() override = default;

  void SetUp() override {
    temp_filename_ = webrtc::test::TempFilename(webrtc::test::OutputPath(),
                                                "yuv_frame_reader_unittest");
    FILE* dummy = fopen(temp_filename_.c_str(), "wb");
    fprintf(dummy, "%s", kInputFileContents.c_str());
    fclose(dummy);

    frame_reader_.reset(
        new YuvFrameReaderImpl(temp_filename_, kFrameWidth, kFrameHeight));
    ASSERT_TRUE(frame_reader_->Init());
  }

  void TearDown() override { remove(temp_filename_.c_str()); }

  std::unique_ptr<FrameReader> frame_reader_;
  std::string temp_filename_;
};

TEST_F(YuvFrameReaderTest, InitSuccess) {}

TEST_F(YuvFrameReaderTest, FrameLength) {
  EXPECT_EQ(kFrameLength, frame_reader_->FrameLength());
}

TEST_F(YuvFrameReaderTest, NumberOfFrames) {
  EXPECT_EQ(1, frame_reader_->NumberOfFrames());
}

TEST_F(YuvFrameReaderTest, ReadFrame) {
  rtc::scoped_refptr<I420BufferInterface> buffer = frame_reader_->ReadFrame();
  ASSERT_TRUE(buffer);
  // Expect I420 packed as YUV.
  EXPECT_EQ(kInputFileContents[0], buffer->DataY()[0]);
  EXPECT_EQ(kInputFileContents[1], buffer->DataY()[1]);
  EXPECT_EQ(kInputFileContents[2], buffer->DataY()[2]);
  EXPECT_EQ(kInputFileContents[3], buffer->DataY()[3]);
  EXPECT_EQ(kInputFileContents[4], buffer->DataU()[0]);
  EXPECT_EQ(kInputFileContents[5], buffer->DataV()[0]);
  EXPECT_FALSE(frame_reader_->ReadFrame());  // End of file.
}

TEST_F(YuvFrameReaderTest, ReadFrameUninitialized) {
  YuvFrameReaderImpl file_reader(temp_filename_, kFrameWidth, kFrameHeight);
  EXPECT_FALSE(file_reader.ReadFrame());
}

}  // namespace test
}  // namespace webrtc
