/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <string>

#include "rtc_base/refcountedobject.h"
#include "rtc_tools/video_file_reader.h"
#include "rtc_tools/video_file_writer.h"
#include "test/gtest.h"
#include "test/testsupport/fileutils.h"

namespace webrtc {
namespace test {

class VideoFileWriterTest : public ::testing::Test {
 public:
  void SetUp() override {
    const std::string filename =
        webrtc::test::OutputPath() + "test_video_file.y4m";

    // Create simple test video of size 6x4.
    FILE* file = fopen(filename.c_str(), "wb");
    ASSERT_TRUE(file != nullptr);
    fprintf(file, "YUV4MPEG2 W6 H4 F60:1 C420 dummyParam\n");
    fprintf(file, "FRAME\n");

    const int i420_size = width * height * 3 / 2;
    // First frame.
    for (int i = 0; i < i420_size; ++i)
      fputc(static_cast<char>(i), file);
    fprintf(file, "FRAME\n");
    // Second frame.
    for (int i = 0; i < i420_size; ++i)
      fputc(static_cast<char>(i + i420_size), file);
    fclose(file);

    // Open the newly created file.
    video = webrtc::test::OpenY4mFile(filename);
    ASSERT_TRUE(video);
  }

  // Write and read Y4M file.
  void WriteVideoY4m() {
    const std::string filename =
        webrtc::test::OutputPath() + "test_video_file2.y4m";
    webrtc::test::WriteVideoToFile(video, filename, fps);
    written_video = webrtc::test::OpenY4mFile(filename);
    ASSERT_TRUE(written_video);
  }

  // Write and read YUV file.
  void WriteVideoYuv() {
    const std::string filename =
        webrtc::test::OutputPath() + "test_video_file2.yuv";
    webrtc::test::WriteVideoToFile(video, filename, fps);
    written_video = webrtc::test::OpenYuvFile(filename, width, height);
    ASSERT_TRUE(written_video);
  }

  const int width = 6;
  const int height = 4;
  const int fps = 60;
  rtc::scoped_refptr<webrtc::test::Video> video;
  rtc::scoped_refptr<webrtc::test::Video> written_video;
};

TEST_F(VideoFileWriterTest, TestParsingFileHeaderY4m) {
  WriteVideoY4m();
  EXPECT_EQ(video->width(), written_video->width());
  EXPECT_EQ(video->height(), written_video->height());
}

TEST_F(VideoFileWriterTest, TestParsingFileHeaderYuv) {
  WriteVideoYuv();
  EXPECT_EQ(video->width(), written_video->width());
  EXPECT_EQ(video->height(), written_video->height());
}

TEST_F(VideoFileWriterTest, TestParsingNumberOfFramesY4m) {
  WriteVideoY4m();
  EXPECT_EQ(video->number_of_frames(), written_video->number_of_frames());
}

TEST_F(VideoFileWriterTest, TestParsingNumberOfFramesYuv) {
  WriteVideoYuv();
  EXPECT_EQ(video->number_of_frames(), written_video->number_of_frames());
}

TEST_F(VideoFileWriterTest, TestPixelContentY4m) {
  WriteVideoY4m();
  int cnt = 0;
  for (const rtc::scoped_refptr<I420BufferInterface> frame : *written_video) {
    for (int i = 0; i < width * height; ++i, ++cnt)
      EXPECT_EQ(cnt, frame->DataY()[i]);
    for (int i = 0; i < width / 2 * height / 2; ++i, ++cnt)
      EXPECT_EQ(cnt, frame->DataU()[i]);
    for (int i = 0; i < width / 2 * height / 2; ++i, ++cnt)
      EXPECT_EQ(cnt, frame->DataV()[i]);
  }
}

TEST_F(VideoFileWriterTest, TestPixelContentYuv) {
  WriteVideoYuv();
  int cnt = 0;
  for (const rtc::scoped_refptr<I420BufferInterface> frame : *written_video) {
    for (int i = 0; i < width * height; ++i, ++cnt)
      EXPECT_EQ(cnt, frame->DataY()[i]);
    for (int i = 0; i < width / 2 * height / 2; ++i, ++cnt)
      EXPECT_EQ(cnt, frame->DataU()[i]);
    for (int i = 0; i < width / 2 * height / 2; ++i, ++cnt)
      EXPECT_EQ(cnt, frame->DataV()[i]);
  }
}

}  // namespace test
}  // namespace webrtc
