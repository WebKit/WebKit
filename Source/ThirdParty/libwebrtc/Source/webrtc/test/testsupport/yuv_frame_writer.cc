/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdio.h>

#include <string>

#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "test/testsupport/frame_writer.h"

namespace webrtc {
namespace test {

YuvFrameWriterImpl::YuvFrameWriterImpl(std::string output_filename,
                                       int width,
                                       int height)
    : output_filename_(output_filename),
      frame_length_in_bytes_(0),
      width_(width),
      height_(height),
      output_file_(nullptr) {}

YuvFrameWriterImpl::~YuvFrameWriterImpl() {
  Close();
}

bool YuvFrameWriterImpl::Init() {
  if (width_ <= 0 || height_ <= 0) {
    RTC_LOG(LS_ERROR) << "Frame width and height must be positive.";
    return false;
  }
  frame_length_in_bytes_ =
      width_ * height_ + 2 * ((width_ + 1) / 2) * ((height_ + 1) / 2);

  output_file_ = fopen(output_filename_.c_str(), "wb");
  if (output_file_ == nullptr) {
    RTC_LOG(LS_ERROR) << "Couldn't open output file: "
                      << output_filename_.c_str();
    return false;
  }
  return true;
}

bool YuvFrameWriterImpl::WriteFrame(const uint8_t* frame_buffer) {
  RTC_DCHECK(frame_buffer);
  if (output_file_ == nullptr) {
    RTC_LOG(LS_ERROR) << "YuvFrameWriterImpl is not initialized.";
    return false;
  }
  size_t bytes_written =
      fwrite(frame_buffer, 1, frame_length_in_bytes_, output_file_);
  if (bytes_written != frame_length_in_bytes_) {
    RTC_LOG(LS_ERROR) << "Cound't write frame to file: "
                      << output_filename_.c_str();
    return false;
  }
  return true;
}

void YuvFrameWriterImpl::Close() {
  if (output_file_ != nullptr) {
    fclose(output_file_);
    output_file_ = nullptr;
  }
}

size_t YuvFrameWriterImpl::FrameLength() {
  return frame_length_in_bytes_;
}

}  // namespace test
}  // namespace webrtc
