/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_TEST_TESTSUPPORT_FRAME_WRITER_H_
#define WEBRTC_TEST_TESTSUPPORT_FRAME_WRITER_H_

#include <stdio.h>

#include <string>

#include "webrtc/typedefs.h"

namespace webrtc {
namespace test {

// Handles writing of video files.
class FrameWriter {
 public:
  virtual ~FrameWriter() {}

  // Initializes the file handler, i.e. opens the input and output files etc.
  // This must be called before reading or writing frames has started.
  // Returns false if an error has occurred, in addition to printing to stderr.
  virtual bool Init() = 0;

  // Writes a frame of the configured frame length to the output file.
  // Returns true if the write was successful, false otherwise.
  virtual bool WriteFrame(uint8_t* frame_buffer) = 0;

  // Closes the output file if open. Essentially makes this class impossible
  // to use anymore. Will also be invoked by the destructor.
  virtual void Close() = 0;

  // Frame length in bytes of a single frame image.
  virtual size_t FrameLength() = 0;
};

class FrameWriterImpl : public FrameWriter {
 public:
  // Creates a file handler. The input file is assumed to exist and be readable
  // and the output file must be writable.
  // Parameters:
  //   output_filename         The file to write. Will be overwritten if already
  //                           existing.
  //   frame_length_in_bytes   The size of each frame.
  //                           For YUV: 3*width*height/2
  FrameWriterImpl(std::string output_filename, size_t frame_length_in_bytes);
  ~FrameWriterImpl() override;
  bool Init() override;
  bool WriteFrame(uint8_t* frame_buffer) override;
  void Close() override;
  size_t FrameLength() override;

 private:
  std::string output_filename_;
  size_t frame_length_in_bytes_;
  FILE* output_file_;
};

}  // namespace test
}  // namespace webrtc

#endif  // WEBRTC_TEST_TESTSUPPORT_FRAME_WRITER_H_
