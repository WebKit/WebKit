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

#include <assert.h>

#include "webrtc/test/frame_utils.h"
#include "webrtc/test/testsupport/fileutils.h"
#include "webrtc/common_video/include/video_frame_buffer.h"

namespace webrtc {
namespace test {

FrameReaderImpl::FrameReaderImpl(std::string input_filename,
                                 int width, int height)
    : input_filename_(input_filename),
      width_(width), height_(height),
      input_file_(NULL) {
}

FrameReaderImpl::~FrameReaderImpl() {
  Close();
}

bool FrameReaderImpl::Init() {
  if (width_ <= 0 || height_ <= 0) {
    fprintf(stderr, "Frame width and height must be >0, was %d x %d\n",
            width_, height_);
    return false;
  }
  frame_length_in_bytes_ =
      width_ * height_ + 2 * ((width_ + 1) / 2) * ((height_ + 1) / 2);

  input_file_ = fopen(input_filename_.c_str(), "rb");
  if (input_file_ == NULL) {
    fprintf(stderr, "Couldn't open input file for reading: %s\n",
            input_filename_.c_str());
    return false;
  }
  // Calculate total number of frames.
  size_t source_file_size = GetFileSize(input_filename_);
  if (source_file_size <= 0u) {
    fprintf(stderr, "Found empty file: %s\n", input_filename_.c_str());
    return false;
  }
  number_of_frames_ = static_cast<int>(source_file_size /
                                       frame_length_in_bytes_);
  return true;
}

void FrameReaderImpl::Close() {
  if (input_file_ != NULL) {
    fclose(input_file_);
    input_file_ = NULL;
  }
}

rtc::scoped_refptr<I420Buffer> FrameReaderImpl::ReadFrame() {
  if (input_file_ == NULL) {
    fprintf(stderr, "FrameReader is not initialized (input file is NULL)\n");
    return nullptr;
  }
  rtc::scoped_refptr<I420Buffer> buffer(
      ReadI420Buffer(width_, height_, input_file_));
  if (!buffer && ferror(input_file_)) {
    fprintf(stderr, "Error reading from input file: %s\n",
            input_filename_.c_str());
  }
  return buffer;
}

size_t FrameReaderImpl::FrameLength() { return frame_length_in_bytes_; }
int FrameReaderImpl::NumberOfFrames() { return number_of_frames_; }

}  // namespace test
}  // namespace webrtc
