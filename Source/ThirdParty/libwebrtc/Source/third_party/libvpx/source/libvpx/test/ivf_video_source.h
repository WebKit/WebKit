/*
 *  Copyright (c) 2012 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef VPX_TEST_IVF_VIDEO_SOURCE_H_
#define VPX_TEST_IVF_VIDEO_SOURCE_H_
#include <cstdio>
#include <cstdlib>
#include <new>
#include <string>
#include "test/video_source.h"

namespace libvpx_test {
const unsigned int kCodeBufferSize = 256 * 1024 * 1024;
const unsigned int kIvfFileHdrSize = 32;
const unsigned int kIvfFrameHdrSize = 12;

static unsigned int MemGetLe32(const uint8_t *mem) {
  return (mem[3] << 24) | (mem[2] << 16) | (mem[1] << 8) | (mem[0]);
}

// This class extends VideoSource to allow parsing of ivf files,
// so that we can do actual file decodes.
class IVFVideoSource : public CompressedVideoSource {
 public:
  explicit IVFVideoSource(const std::string &file_name)
      : file_name_(file_name), input_file_(NULL), compressed_frame_buf_(NULL),
        frame_sz_(0), frame_(0), end_of_file_(false) {}

  virtual ~IVFVideoSource() {
    delete[] compressed_frame_buf_;

    if (input_file_) fclose(input_file_);
  }

  virtual void Init() {
    // Allocate a buffer for read in the compressed video frame.
    compressed_frame_buf_ = new uint8_t[libvpx_test::kCodeBufferSize];
    ASSERT_TRUE(compressed_frame_buf_ != NULL)
        << "Allocate frame buffer failed";
  }

  virtual void Begin() {
    input_file_ = OpenTestDataFile(file_name_);
    ASSERT_TRUE(input_file_ != NULL)
        << "Input file open failed. Filename: " << file_name_;

    // Read file header
    uint8_t file_hdr[kIvfFileHdrSize];
    ASSERT_EQ(kIvfFileHdrSize, fread(file_hdr, 1, kIvfFileHdrSize, input_file_))
        << "File header read failed.";
    // Check file header
    ASSERT_TRUE(file_hdr[0] == 'D' && file_hdr[1] == 'K' &&
                file_hdr[2] == 'I' && file_hdr[3] == 'F')
        << "Input is not an IVF file.";

    FillFrame();
  }

  virtual void Next() {
    ++frame_;
    FillFrame();
  }

  void FillFrame() {
    ASSERT_TRUE(input_file_ != NULL);
    uint8_t frame_hdr[kIvfFrameHdrSize];
    // Check frame header and read a frame from input_file.
    if (fread(frame_hdr, 1, kIvfFrameHdrSize, input_file_) !=
        kIvfFrameHdrSize) {
      end_of_file_ = true;
    } else {
      end_of_file_ = false;

      frame_sz_ = MemGetLe32(frame_hdr);
      ASSERT_LE(frame_sz_, kCodeBufferSize)
          << "Frame is too big for allocated code buffer";
      ASSERT_EQ(frame_sz_,
                fread(compressed_frame_buf_, 1, frame_sz_, input_file_))
          << "Failed to read complete frame";
    }
  }

  virtual const uint8_t *cxdata() const {
    return end_of_file_ ? NULL : compressed_frame_buf_;
  }
  virtual size_t frame_size() const { return frame_sz_; }
  virtual unsigned int frame_number() const { return frame_; }

 protected:
  std::string file_name_;
  FILE *input_file_;
  uint8_t *compressed_frame_buf_;
  size_t frame_sz_;
  unsigned int frame_;
  bool end_of_file_;
};

}  // namespace libvpx_test

#endif  // VPX_TEST_IVF_VIDEO_SOURCE_H_
