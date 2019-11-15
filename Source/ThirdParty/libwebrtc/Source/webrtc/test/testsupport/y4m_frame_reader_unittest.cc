/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdint.h>
#include <stdio.h>

#include <memory>
#include <string>

#include "absl/strings/string_view.h"
#include "api/scoped_refptr.h"
#include "api/video/i420_buffer.h"
#include "api/video/video_frame_buffer.h"
#include "test/gtest.h"
#include "test/testsupport/file_utils.h"
#include "test/testsupport/frame_reader.h"

namespace webrtc {
namespace test {
namespace {

const absl::string_view kFileHeader = "YUV4MPEG2 W50 H20 F30:1 C420\n";
const absl::string_view kFrameHeader = "FRAME\n";
const absl::string_view kInputVideoContents = "abcdef";

const size_t kFrameWidth = 2;
const size_t kFrameHeight = 2;
const size_t kFrameLength = 3 * kFrameWidth * kFrameHeight / 2;  // I420.

}  // namespace

class Y4mFrameReaderTest : public ::testing::Test {
 protected:
  Y4mFrameReaderTest() = default;
  ~Y4mFrameReaderTest() override = default;

  void SetUp() override {
    temp_filename_ = webrtc::test::TempFilename(webrtc::test::OutputPath(),
                                                "y4m_frame_reader_unittest");
    FILE* dummy = fopen(temp_filename_.c_str(), "wb");
    fprintf(dummy, "%s",
            (std::string(kFileHeader) + std::string(kFrameHeader) +
             std::string(kInputVideoContents))
                .c_str());
    fclose(dummy);

    frame_reader_.reset(
        new Y4mFrameReaderImpl(temp_filename_, kFrameWidth, kFrameHeight));
    ASSERT_TRUE(frame_reader_->Init());
  }

  void TearDown() override { remove(temp_filename_.c_str()); }

  std::unique_ptr<FrameReader> frame_reader_;
  std::string temp_filename_;
};

TEST_F(Y4mFrameReaderTest, InitSuccess) {}

TEST_F(Y4mFrameReaderTest, FrameLength) {
  EXPECT_EQ(kFrameHeader.size() + kFrameLength, frame_reader_->FrameLength());
}

TEST_F(Y4mFrameReaderTest, NumberOfFrames) {
  EXPECT_EQ(1, frame_reader_->NumberOfFrames());
}

TEST_F(Y4mFrameReaderTest, ReadFrame) {
  rtc::scoped_refptr<I420BufferInterface> buffer = frame_reader_->ReadFrame();
  ASSERT_TRUE(buffer);
  // Expect I420 packed as YUV.
  EXPECT_EQ(kInputVideoContents[0], buffer->DataY()[0]);
  EXPECT_EQ(kInputVideoContents[1], buffer->DataY()[1]);
  EXPECT_EQ(kInputVideoContents[2], buffer->DataY()[2]);
  EXPECT_EQ(kInputVideoContents[3], buffer->DataY()[3]);
  EXPECT_EQ(kInputVideoContents[4], buffer->DataU()[0]);
  EXPECT_EQ(kInputVideoContents[5], buffer->DataV()[0]);
  EXPECT_FALSE(frame_reader_->ReadFrame());  // End of file.
}

TEST_F(Y4mFrameReaderTest, ReadFrameUninitialized) {
  Y4mFrameReaderImpl file_reader(temp_filename_, kFrameWidth, kFrameHeight);
  EXPECT_FALSE(file_reader.ReadFrame());
}

}  // namespace test
}  // namespace webrtc
