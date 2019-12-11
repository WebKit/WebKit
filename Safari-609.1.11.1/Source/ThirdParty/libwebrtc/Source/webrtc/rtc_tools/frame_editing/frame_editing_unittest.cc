/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdio.h>
#include <stdlib.h>

#include <fstream>
#include <memory>

#include "common_video/libyuv/include/webrtc_libyuv.h"
#include "rtc_tools/frame_editing/frame_editing_lib.h"
#include "test/gtest.h"
#include "test/testsupport/fileutils.h"

namespace webrtc {
namespace test {

const int kWidth = 352;
const int kHeight = 288;
const size_t kFrameSize = CalcBufferSize(VideoType::kI420, kWidth, kHeight);

class FrameEditingTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    reference_video_ = ResourcePath("foreman_cif", "yuv");
    test_video_ = webrtc::test::TempFilename(webrtc::test::OutputPath(),
                                             "frame_editing_unittest.yuv");

    original_fid_ = fopen(reference_video_.c_str(), "rb");
    ASSERT_TRUE(original_fid_ != NULL);

    // Ensure the output file exists on disk.
    std::ofstream(test_video_.c_str(), std::ios::out);
    // Open the output file for reading.
    // TODO(holmer): Figure out why this file has to be opened here (test fails
    // if it's opened after the write operation performed in EditFrames).
    edited_fid_ = fopen(test_video_.c_str(), "rb");
    ASSERT_TRUE(edited_fid_ != NULL);

    original_buffer_.reset(new int[kFrameSize]);
    edited_buffer_.reset(new int[kFrameSize]);
    num_frames_read_ = 0;
  }
  virtual void TearDown() {
    fclose(original_fid_);
    fclose(edited_fid_);
    remove(test_video_.c_str());
  }
  // Compares the frames in both streams to the end of one of the streams.
  void CompareToTheEnd(FILE* test_video_fid,
                       FILE* ref_video_fid,
                       std::unique_ptr<int[]>* ref_buffer,
                       std::unique_ptr<int[]>* test_buffer) {
    while (!feof(test_video_fid) && !feof(ref_video_fid)) {
      num_bytes_read_ = fread(ref_buffer->get(), 1, kFrameSize, ref_video_fid);
      if (!feof(ref_video_fid)) {
        EXPECT_EQ(kFrameSize, num_bytes_read_);
      }
      num_bytes_read_ =
          fread(test_buffer->get(), 1, kFrameSize, test_video_fid);
      if (!feof(test_video_fid)) {
        EXPECT_EQ(kFrameSize, num_bytes_read_);
      }
      if (!feof(test_video_fid) && !feof(test_video_fid)) {
        EXPECT_EQ(0, memcmp(ref_buffer->get(), test_buffer->get(), kFrameSize));
      }
    }
    // There should not be anything left in either stream.
    EXPECT_EQ(!feof(test_video_fid), !feof(ref_video_fid));
  }
  std::string reference_video_;
  std::string test_video_;
  FILE* original_fid_;
  FILE* edited_fid_;
  size_t num_bytes_read_;
  std::unique_ptr<int[]> original_buffer_;
  std::unique_ptr<int[]> edited_buffer_;
  int num_frames_read_;
};

TEST_F(FrameEditingTest, ValidInPath) {
  const int kFirstFrameToProcess = 160;
  const int kInterval = -1;
  const int kLastFrameToProcess = 240;

  int result =
      EditFrames(reference_video_, kWidth, kHeight, kFirstFrameToProcess,
                 kInterval, kLastFrameToProcess, test_video_);
  EXPECT_EQ(0, result);

  for (int i = 1; i < kFirstFrameToProcess; ++i) {
    num_bytes_read_ =
        fread(original_buffer_.get(), 1, kFrameSize, original_fid_);
    EXPECT_EQ(kFrameSize, num_bytes_read_);

    num_bytes_read_ = fread(edited_buffer_.get(), 1, kFrameSize, edited_fid_);
    EXPECT_EQ(kFrameSize, num_bytes_read_);

    EXPECT_EQ(0,
              memcmp(original_buffer_.get(), edited_buffer_.get(), kFrameSize));
  }
  // Do not compare the frames that have been cut.
  for (int i = kFirstFrameToProcess; i <= kLastFrameToProcess; ++i) {
    num_bytes_read_ =
        fread(original_buffer_.get(), 1, kFrameSize, original_fid_);
    EXPECT_EQ(kFrameSize, num_bytes_read_);
  }
  CompareToTheEnd(edited_fid_, original_fid_, &original_buffer_,
                  &edited_buffer_);
}

TEST_F(FrameEditingTest, EmptySetToCut) {
  const int kFirstFrameToProcess = 2;
  const int kInterval = -1;
  const int kLastFrameToProcess = 1;

  int result =
      EditFrames(reference_video_, kWidth, kHeight, kFirstFrameToProcess,
                 kInterval, kLastFrameToProcess, test_video_);
  EXPECT_EQ(-10, result);
}

TEST_F(FrameEditingTest, InValidInPath) {
  const std::string kRefVideo_ = "PATH/THAT/DOES/NOT/EXIST";

  const int kFirstFrameToProcess = 30;
  const int kInterval = 1;
  const int kLastFrameToProcess = 120;

  int result = EditFrames(kRefVideo_, kWidth, kHeight, kFirstFrameToProcess,
                          kInterval, kLastFrameToProcess, test_video_);
  EXPECT_EQ(-11, result);
}

TEST_F(FrameEditingTest, DeletingEverySecondFrame) {
  const int kFirstFrameToProcess = 1;
  const int kInterval = -2;
  const int kLastFrameToProcess = 10000;
  // Set kLastFrameToProcess to a large value so that all frame are processed.
  int result =
      EditFrames(reference_video_, kWidth, kHeight, kFirstFrameToProcess,
                 kInterval, kLastFrameToProcess, test_video_);
  EXPECT_EQ(0, result);

  while (!feof(original_fid_) && !feof(edited_fid_)) {
    num_bytes_read_ =
        fread(original_buffer_.get(), 1, kFrameSize, original_fid_);
    if (!feof(original_fid_)) {
      EXPECT_EQ(kFrameSize, num_bytes_read_);
      num_frames_read_++;
    }
    // We want to compare every second frame of the original to the edited.
    // kInterval=-2 and (num_frames_read_ - 1) % kInterval  will be -1 for
    // every second frame.
    // num_frames_read_ - 1 because we have deleted frame number 2, 4 , 6 etc.
    if ((num_frames_read_ - 1) % kInterval == -1) {
      num_bytes_read_ = fread(edited_buffer_.get(), 1, kFrameSize, edited_fid_);
      if (!feof(edited_fid_)) {
        EXPECT_EQ(kFrameSize, num_bytes_read_);
      }
      if (!feof(original_fid_) && !feof(edited_fid_)) {
        EXPECT_EQ(0, memcmp(original_buffer_.get(), edited_buffer_.get(),
                            kFrameSize));
      }
    }
  }
}

TEST_F(FrameEditingTest, RepeatFrames) {
  const int kFirstFrameToProcess = 160;
  const int kInterval = 2;
  const int kLastFrameToProcess = 240;

  int result =
      EditFrames(reference_video_, kWidth, kHeight, kFirstFrameToProcess,
                 kInterval, kLastFrameToProcess, test_video_);
  EXPECT_EQ(0, result);

  for (int i = 1; i < kFirstFrameToProcess; ++i) {
    num_bytes_read_ =
        fread(original_buffer_.get(), 1, kFrameSize, original_fid_);
    EXPECT_EQ(kFrameSize, num_bytes_read_);

    num_bytes_read_ = fread(edited_buffer_.get(), 1, kFrameSize, edited_fid_);
    EXPECT_EQ(kFrameSize, num_bytes_read_);

    EXPECT_EQ(0,
              memcmp(original_buffer_.get(), edited_buffer_.get(), kFrameSize));
  }
  // Do not compare the frames that have been repeated.
  for (int i = kFirstFrameToProcess; i <= kLastFrameToProcess; ++i) {
    num_bytes_read_ =
        fread(original_buffer_.get(), 1, kFrameSize, original_fid_);
    EXPECT_EQ(kFrameSize, num_bytes_read_);
    for (int i = 1; i <= kInterval; ++i) {
      num_bytes_read_ = fread(edited_buffer_.get(), 1, kFrameSize, edited_fid_);
      EXPECT_EQ(kFrameSize, num_bytes_read_);
      EXPECT_EQ(
          0, memcmp(original_buffer_.get(), edited_buffer_.get(), kFrameSize));
    }
  }
  CompareToTheEnd(edited_fid_, original_fid_, &original_buffer_,
                  &edited_buffer_);
}
}  // namespace test
}  // namespace webrtc
