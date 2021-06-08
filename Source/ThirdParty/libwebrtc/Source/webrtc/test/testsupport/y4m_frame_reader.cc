/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdio.h>

#include <string>

#include "api/scoped_refptr.h"
#include "api/video/i420_buffer.h"
#include "test/testsupport/file_utils.h"
#include "test/testsupport/frame_reader.h"

namespace webrtc {
namespace test {
namespace {

// Size of header: "YUV4MPEG2 W50 H20 F30:1 C420\n"
const size_t kFileHeaderSize = 29;
// Size of header: "FRAME\n"
const size_t kFrameHeaderSize = 6;

}  // namespace

Y4mFrameReaderImpl::Y4mFrameReaderImpl(std::string input_filename,
                                       int width,
                                       int height)
    : YuvFrameReaderImpl(input_filename, width, height) {
  frame_length_in_bytes_ += kFrameHeaderSize;
  buffer_ = new uint8_t[kFileHeaderSize];
}
Y4mFrameReaderImpl::~Y4mFrameReaderImpl() {
  delete[] buffer_;
}

bool Y4mFrameReaderImpl::Init() {
  if (input_width_ <= 0 || input_height_ <= 0) {
    fprintf(stderr, "Frame width and height must be >0, was %d x %d\n",
            input_width_, input_height_);
    return false;
  }
  input_file_ = fopen(input_filename_.c_str(), "rb");
  if (input_file_ == nullptr) {
    fprintf(stderr, "Couldn't open input file for reading: %s\n",
            input_filename_.c_str());
    return false;
  }
  size_t source_file_size = GetFileSize(input_filename_);
  if (source_file_size <= 0u) {
    fprintf(stderr, "Found empty file: %s\n", input_filename_.c_str());
    return false;
  }
  if (fread(buffer_, 1, kFileHeaderSize, input_file_) < kFileHeaderSize) {
    fprintf(stderr, "Failed to read file header from input file: %s\n",
            input_filename_.c_str());
    return false;
  }
  // Calculate total number of frames.
  number_of_frames_ = static_cast<int>((source_file_size - kFileHeaderSize) /
                                       frame_length_in_bytes_);
  return true;
}

rtc::scoped_refptr<I420Buffer> Y4mFrameReaderImpl::ReadFrame() {
  if (input_file_ == nullptr) {
    fprintf(stderr,
            "Y4mFrameReaderImpl is not initialized (input file is NULL)\n");
    return nullptr;
  }
  if (fread(buffer_, 1, kFrameHeaderSize, input_file_) < kFrameHeaderSize &&
      ferror(input_file_)) {
    fprintf(stderr, "Failed to read frame header from input file: %s\n",
            input_filename_.c_str());
    return nullptr;
  }
  return YuvFrameReaderImpl::ReadFrame();
}

}  // namespace test
}  // namespace webrtc
