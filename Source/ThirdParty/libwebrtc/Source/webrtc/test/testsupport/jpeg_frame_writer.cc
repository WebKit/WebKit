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

#include "common_video/libyuv/include/webrtc_libyuv.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "test/testsupport/frame_writer.h"

extern "C" {
#if defined(USE_SYSTEM_LIBJPEG)
#include <jpeglib.h>
#else
// Include directory supplied by gn
#include "jpeglib.h"  // NOLINT
#endif
}

namespace webrtc {
namespace test {

JpegFrameWriter::JpegFrameWriter(const std::string& output_filename)
    : frame_written_(false),
      output_filename_(output_filename),
      output_file_(nullptr) {}

bool JpegFrameWriter::WriteFrame(const VideoFrame& input_frame, int quality) {
  if (frame_written_) {
    RTC_LOG(LS_ERROR) << "Only a single frame can be saved to Jpeg.";
    return false;
  }
  const int kColorPlanes = 3;  // R, G and B.
  size_t rgb_len = input_frame.height() * input_frame.width() * kColorPlanes;
  std::unique_ptr<uint8_t[]> rgb_buf(new uint8_t[rgb_len]);

  // kRGB24 actually corresponds to FourCC 24BG which is 24-bit BGR.
  if (ConvertFromI420(input_frame, VideoType::kRGB24, 0, rgb_buf.get()) < 0) {
    RTC_LOG(LS_ERROR) << "Could not convert input frame to RGB.";
    return false;
  }
  output_file_ = fopen(output_filename_.c_str(), "wb");
  if (!output_file_) {
    RTC_LOG(LS_ERROR) << "Couldn't open file to write jpeg frame to:"
                      << output_filename_;
    return false;
  }

  // Invoking LIBJPEG
  struct jpeg_compress_struct cinfo;
  struct jpeg_error_mgr jerr;
  JSAMPROW row_pointer[1];
  cinfo.err = jpeg_std_error(&jerr);
  jpeg_create_compress(&cinfo);

  jpeg_stdio_dest(&cinfo, output_file_);

  cinfo.image_width = input_frame.width();
  cinfo.image_height = input_frame.height();
  cinfo.input_components = kColorPlanes;
  cinfo.in_color_space = JCS_EXT_BGR;
  jpeg_set_defaults(&cinfo);
  jpeg_set_quality(&cinfo, quality, TRUE);

  jpeg_start_compress(&cinfo, TRUE);
  int row_stride = input_frame.width() * kColorPlanes;
  while (cinfo.next_scanline < cinfo.image_height) {
    row_pointer[0] = &rgb_buf.get()[cinfo.next_scanline * row_stride];
    jpeg_write_scanlines(&cinfo, row_pointer, 1);
  }

  jpeg_finish_compress(&cinfo);
  jpeg_destroy_compress(&cinfo);
  fclose(output_file_);

  frame_written_ = true;
  return true;
}

}  // namespace test
}  // namespace webrtc
