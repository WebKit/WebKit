/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#include <iomanip>
#include <sstream>

#include "rtc_tools/converter/converter.h"

#ifdef WIN32
#define SEPARATOR '\\'
#define STAT _stat
#else
#define SEPARATOR '/'
#define STAT stat
#endif

namespace webrtc {
namespace test {

Converter::Converter(int width, int height) : width_(width), height_(height) {}

bool Converter::ConvertRGBAToI420Video(std::string frames_dir,
                                       std::string output_file_name,
                                       bool delete_frames) {
  FILE* output_file = fopen(output_file_name.c_str(), "wb");

  // Open output file in append mode.
  if (output_file == NULL) {
    fprintf(stderr, "Couldn't open input file for reading: %s\n",
            output_file_name.c_str());
    return false;
  }

  int input_frame_size = InputFrameSize();
  uint8_t* rgba_buffer = new uint8_t[input_frame_size];
  int y_plane_size = YPlaneSize();
  uint8_t* dst_y = new uint8_t[y_plane_size];
  int u_plane_size = UPlaneSize();
  uint8_t* dst_u = new uint8_t[u_plane_size];
  int v_plane_size = VPlaneSize();
  uint8_t* dst_v = new uint8_t[v_plane_size];

  int counter = 0;       // Counter to form frame names.
  bool success = false;  // Is conversion successful.

  while (true) {
    std::string file_name = FormFrameName(4, counter);
    // Get full path file name.
    std::string input_file_name = FindFullFileName(frames_dir, file_name);

    if (FileExists(input_file_name)) {
      ++counter;  // Update counter for the next round.
    } else {
      fprintf(stdout, "Reached end of frames list\n");
      break;
    }

    // Read the RGBA frame into rgba_buffer.
    ReadRGBAFrame(input_file_name.c_str(), input_frame_size, rgba_buffer);

    // Delete the input frame.
    if (delete_frames) {
      if (remove(input_file_name.c_str()) != 0) {
        fprintf(stderr, "Cannot delete file %s\n", input_file_name.c_str());
      }
    }

    // Convert to I420 frame.
    libyuv::ABGRToI420(rgba_buffer, SrcStrideFrame(), dst_y, DstStrideY(),
                       dst_u, DstStrideU(), dst_v, DstStrideV(), width_,
                       height_);

    // Add the I420 frame to the YUV video file.
    success = AddYUVToFile(dst_y, y_plane_size, dst_u, u_plane_size, dst_v,
                           v_plane_size, output_file);

    if (!success) {
      fprintf(stderr, "LibYUV error during RGBA to I420 frame conversion\n");
      break;
    }
  }

  delete[] rgba_buffer;
  delete[] dst_y;
  delete[] dst_u;
  delete[] dst_v;

  fclose(output_file);

  return success;
}

bool Converter::AddYUVToFile(uint8_t* y_plane,
                             int y_plane_size,
                             uint8_t* u_plane,
                             int u_plane_size,
                             uint8_t* v_plane,
                             int v_plane_size,
                             FILE* output_file) {
  bool success = AddYUVPlaneToFile(y_plane, y_plane_size, output_file) &&
                 AddYUVPlaneToFile(u_plane, u_plane_size, output_file) &&
                 AddYUVPlaneToFile(v_plane, v_plane_size, output_file);
  return success;
}

bool Converter::AddYUVPlaneToFile(uint8_t* yuv_plane,
                                  int yuv_plane_size,
                                  FILE* file) {
  size_t bytes_written = fwrite(yuv_plane, 1, yuv_plane_size, file);

  if (bytes_written != static_cast<size_t>(yuv_plane_size)) {
    fprintf(stderr,
            "Number of bytes written (%d) doesn't match size of y plane"
            " (%d)\n",
            static_cast<int>(bytes_written), yuv_plane_size);
    return false;
  }
  return true;
}

bool Converter::ReadRGBAFrame(const char* input_file_name,
                              int input_frame_size,
                              unsigned char* buffer) {
  FILE* input_file = fopen(input_file_name, "rb");
  if (input_file == NULL) {
    fprintf(stderr, "Couldn't open input file for reading: %s\n",
            input_file_name);
    return false;
  }

  size_t nbr_read = fread(buffer, 1, input_frame_size, input_file);
  fclose(input_file);

  if (nbr_read != static_cast<size_t>(input_frame_size)) {
    fprintf(stderr, "Error reading from input file: %s\n", input_file_name);
    return false;
  }

  return true;
}

std::string Converter::FindFullFileName(std::string dir_name,
                                        std::string file_name) {
  return dir_name + SEPARATOR + file_name;
}

bool Converter::FileExists(std::string file_name_to_check) {
  struct STAT file_info;
  int result = STAT(file_name_to_check.c_str(), &file_info);
  return (result == 0);
}

std::string Converter::FormFrameName(int width, int number) {
  std::stringstream tmp;

  // Zero-pad number to a string.
  tmp << std::setfill('0') << std::setw(width) << number;

  return "frame_" + tmp.str();
}

}  // namespace test
}  // namespace webrtc
