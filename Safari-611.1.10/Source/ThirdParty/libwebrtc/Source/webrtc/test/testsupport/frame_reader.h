/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_TESTSUPPORT_FRAME_READER_H_
#define TEST_TESTSUPPORT_FRAME_READER_H_

#include <stdio.h>

#include <string>

#include "api/scoped_refptr.h"

namespace webrtc {
class I420Buffer;
namespace test {

// Handles reading of I420 frames from video files.
class FrameReader {
 public:
  virtual ~FrameReader() {}

  // Initializes the frame reader, i.e. opens the input file.
  // This must be called before reading of frames has started.
  // Returns false if an error has occurred, in addition to printing to stderr.
  virtual bool Init() = 0;

  // Reads a frame from the input file. On success, returns the frame.
  // Returns nullptr if encountering end of file or a read error.
  virtual rtc::scoped_refptr<I420Buffer> ReadFrame() = 0;

  // Closes the input file if open. Essentially makes this class impossible
  // to use anymore. Will also be invoked by the destructor.
  virtual void Close() = 0;

  // Frame length in bytes of a single frame image.
  virtual size_t FrameLength() = 0;
  // Total number of frames in the input video source.
  virtual int NumberOfFrames() = 0;
};

class YuvFrameReaderImpl : public FrameReader {
 public:
  // Creates a file handler. The input file is assumed to exist and be readable.
  // Parameters:
  //   input_filename          The file to read from.
  //   width, height           Size of each frame to read.
  YuvFrameReaderImpl(std::string input_filename, int width, int height);
  ~YuvFrameReaderImpl() override;
  bool Init() override;
  rtc::scoped_refptr<I420Buffer> ReadFrame() override;
  void Close() override;
  size_t FrameLength() override;
  int NumberOfFrames() override;

 protected:
  const std::string input_filename_;
  // It is not const, so subclasses will be able to add frame header size.
  size_t frame_length_in_bytes_;
  const int width_;
  const int height_;
  int number_of_frames_;
  FILE* input_file_;
};

class Y4mFrameReaderImpl : public YuvFrameReaderImpl {
 public:
  // Creates a file handler. The input file is assumed to exist and be readable.
  // Parameters:
  //   input_filename          The file to read from.
  //   width, height           Size of each frame to read.
  Y4mFrameReaderImpl(std::string input_filename, int width, int height);
  ~Y4mFrameReaderImpl() override;
  bool Init() override;
  rtc::scoped_refptr<I420Buffer> ReadFrame() override;

 private:
  // Buffer that is used to read file and frame headers.
  uint8_t* buffer_;
};

}  // namespace test
}  // namespace webrtc

#endif  // TEST_TESTSUPPORT_FRAME_READER_H_
