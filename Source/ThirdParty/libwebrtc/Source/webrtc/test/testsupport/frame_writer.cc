/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/test/testsupport/frame_writer.h"

#include <assert.h>

namespace webrtc {
namespace test {

FrameWriterImpl::FrameWriterImpl(std::string output_filename,
                                 size_t frame_length_in_bytes)
    : output_filename_(output_filename),
      frame_length_in_bytes_(frame_length_in_bytes),
      output_file_(NULL) {
}

FrameWriterImpl::~FrameWriterImpl() {
  Close();
}

bool FrameWriterImpl::Init() {
  if (frame_length_in_bytes_ <= 0) {
    fprintf(stderr, "Frame length must be >0, was %zu\n",
            frame_length_in_bytes_);
    return false;
  }
  output_file_ = fopen(output_filename_.c_str(), "wb");
  if (output_file_ == NULL) {
    fprintf(stderr, "Couldn't open output file for writing: %s\n",
            output_filename_.c_str());
    return false;
  }
  return true;
}

void FrameWriterImpl::Close() {
  if (output_file_ != NULL) {
    fclose(output_file_);
    output_file_ = NULL;
  }
}

size_t FrameWriterImpl::FrameLength() { return frame_length_in_bytes_; }

bool FrameWriterImpl::WriteFrame(uint8_t* frame_buffer) {
  assert(frame_buffer);
  if (output_file_ == NULL) {
    fprintf(stderr, "FrameWriter is not initialized (output file is NULL)\n");
    return false;
  }
  size_t bytes_written = fwrite(frame_buffer, 1, frame_length_in_bytes_,
                                output_file_);
  if (bytes_written != frame_length_in_bytes_) {
    fprintf(stderr, "Failed to write %zu bytes to file %s\n",
            frame_length_in_bytes_, output_filename_.c_str());
    return false;
  }
  return true;
}

}  // namespace test
}  // namespace webrtc
