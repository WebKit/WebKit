/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_TOOLS_CONVERTER_CONVERTER_H_
#define RTC_TOOLS_CONVERTER_CONVERTER_H_

#include <string>

#include "third_party/libyuv/include/libyuv/compare.h"
#include "third_party/libyuv/include/libyuv/convert.h"

namespace webrtc {
namespace test {

// Handles a conversion between a set of RGBA frames to a YUV (I420) video.
class Converter {
 public:
  Converter(int width, int height);

  // Converts RGBA to YUV video. If the delete_frames argument is true, the
  // method will delete the input frames after conversion.
  bool ConvertRGBAToI420Video(std::string frames_dir,
                              std::string output_file_name,
                              bool delete_frames);

 private:
  int width_;   // Width of the video (respectively of the RGBA frames).
  int height_;  // Height of the video (respectively of the RGBA frames).

  // Returns the size of the Y plane in bytes.
  int YPlaneSize() const { return width_ * height_; }

  // Returns the size of the U plane in bytes.
  int UPlaneSize() const { return ((width_ + 1) / 2) * ((height_) / 2); }

  // Returns the size of the V plane in bytes.
  int VPlaneSize() const { return ((width_ + 1) / 2) * ((height_) / 2); }

  // Returns the number of bytes per row in the RGBA frame.
  int SrcStrideFrame() const { return width_ * 4; }

  // Returns the number of bytes in the Y plane.
  int DstStrideY() const { return width_; }

  // Returns the number of bytes in the U plane.
  int DstStrideU() const { return (width_ + 1) / 2; }

  // Returns the number of bytes in the V plane.
  int DstStrideV() const { return (width_ + 1) / 2; }

  // Returns the size in bytes of the input RGBA frames.
  int InputFrameSize() const { return width_ * height_ * 4; }

  // Writes the Y, U and V (in this order) planes to the file, thus adding a
  // raw YUV frame to the file.
  bool AddYUVToFile(uint8_t* y_plane,
                    int y_plane_size,
                    uint8_t* u_plane,
                    int u_plane_size,
                    uint8_t* v_plane,
                    int v_plane_size,
                    FILE* output_file);

  // Adds the Y, U or V plane to the file.
  bool AddYUVPlaneToFile(uint8_t* yuv_plane, int yuv_plane_size, FILE* file);

  // Reads a RGBA frame from input_file_name with input_frame_size size in bytes
  // into the buffer.
  bool ReadRGBAFrame(const char* input_file_name,
                     int input_frame_size,
                     unsigned char* buffer);

  // Finds the full path name of the file - concatenates the directory and file
  // names.
  std::string FindFullFileName(std::string dir_name, std::string file_name);

  // Checks if a file exists.
  bool FileExists(std::string file_name_to_check);

  // Returns the name of the file in the form frame_<number>, where <number> is
  // 4 zero padded (i.e. frame_0000, frame_0001, etc.).
  std::string FormFrameName(int width, int number);
};

}  // namespace test
}  // namespace webrtc

#endif  // RTC_TOOLS_CONVERTER_CONVERTER_H_
