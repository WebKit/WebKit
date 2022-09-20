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

#include "rtc_base/logging.h"
#include "test/testsupport/frame_writer.h"

namespace webrtc {
namespace test {

Y4mFrameWriterImpl::Y4mFrameWriterImpl(std::string output_filename,
                                       int width,
                                       int height,
                                       int frame_rate)
    : YuvFrameWriterImpl(output_filename, width, height),
      frame_rate_(frame_rate) {}

Y4mFrameWriterImpl::~Y4mFrameWriterImpl() = default;

bool Y4mFrameWriterImpl::Init() {
  if (!YuvFrameWriterImpl::Init()) {
    return false;
  }
  int bytes_written = fprintf(output_file_, "YUV4MPEG2 W%d H%d F%d:1 C420\n",
                              width_, height_, frame_rate_);
  if (bytes_written < 0) {
    RTC_LOG(LS_ERROR) << "Failed to write Y4M file header to file: "
                      << output_filename_.c_str();
    return false;
  }
  return true;
}

bool Y4mFrameWriterImpl::WriteFrame(const uint8_t* frame_buffer) {
  if (output_file_ == nullptr) {
    RTC_LOG(LS_ERROR) << "Y4mFrameWriterImpl is not initialized.";
    return false;
  }
  int bytes_written = fprintf(output_file_, "FRAME\n");
  if (bytes_written < 0) {
    RTC_LOG(LS_ERROR) << "Couldn't write Y4M frame header to file: "
                      << output_filename_.c_str();
    return false;
  }
  return YuvFrameWriterImpl::WriteFrame(frame_buffer);
}

}  // namespace test
}  // namespace webrtc
