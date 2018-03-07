/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/testsupport/frame_reader.h"

#include "api/video/i420_buffer.h"
#include "test/frame_utils.h"
#include "test/testsupport/fileutils.h"

namespace webrtc {
namespace test {

YuvFrameReaderImpl::YuvFrameReaderImpl(std::string input_filename,
                                       int width,
                                       int height)
    : input_filename_(input_filename),
      frame_length_in_bytes_(0),
      width_(width),
      height_(height),
      number_of_frames_(-1),
      input_file_(nullptr) {}

YuvFrameReaderImpl::~YuvFrameReaderImpl() {
  Close();
}

bool YuvFrameReaderImpl::Init() {
  if (width_ <= 0 || height_ <= 0) {
    fprintf(stderr, "Frame width and height must be >0, was %d x %d\n", width_,
            height_);
    return false;
  }
  frame_length_in_bytes_ =
      width_ * height_ + 2 * ((width_ + 1) / 2) * ((height_ + 1) / 2);

  input_file_ = fopen(input_filename_.c_str(), "rb");
  if (input_file_ == nullptr) {
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
  number_of_frames_ =
      static_cast<int>(source_file_size / frame_length_in_bytes_);
  return true;
}

rtc::scoped_refptr<I420Buffer> YuvFrameReaderImpl::ReadFrame() {
  if (input_file_ == nullptr) {
    fprintf(stderr,
            "YuvFrameReaderImpl is not initialized (input file is NULL)\n");
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

void YuvFrameReaderImpl::Close() {
  if (input_file_ != nullptr) {
    fclose(input_file_);
    input_file_ = nullptr;
  }
}

size_t YuvFrameReaderImpl::FrameLength() {
  return frame_length_in_bytes_;
}

int YuvFrameReaderImpl::NumberOfFrames() {
  return number_of_frames_;
}

}  // namespace test
}  // namespace webrtc
