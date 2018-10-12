/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_tools/video_file_writer.h"

#include <string>

#include "api/video/i420_buffer.h"
#include "rtc_base/logging.h"
#include "rtc_base/refcountedobject.h"
#include "rtc_base/stringutils.h"

namespace webrtc {
namespace test {

void WriteVideoToFile(const rtc::scoped_refptr<Video>& video,
                      const std::string& file_name,
                      int fps) {
  FILE* output_file = fopen(file_name.c_str(), "wb");
  if (output_file == nullptr) {
    RTC_LOG(LS_ERROR) << "Could not open file for writing: " << file_name;
    return;
  }

  bool isY4m = rtc::ends_with(file_name.c_str(), ".y4m");
  if (isY4m) {
    fprintf(output_file, "YUV4MPEG2 W%d H%d F%d:1 C420\n", video->width(),
            video->height(), fps);
  }
  for (size_t i = 0; i < video->number_of_frames(); ++i) {
    if (isY4m) {
      std::string frame = "FRAME\n";
      fwrite(frame.c_str(), 1, 6, output_file);
    }
    rtc::scoped_refptr<I420BufferInterface> buffer = video->GetFrame(i);
    const uint8_t* data_y = buffer->DataY();
    int stride = buffer->StrideY();
    for (int i = 0; i < video->height(); ++i) {
      fwrite(data_y + i * stride, /*size=*/1, stride, output_file);
    }
    const uint8_t* data_u = buffer->DataU();
    stride = buffer->StrideU();
    for (int i = 0; i < buffer->ChromaHeight(); ++i) {
      fwrite(data_u + i * stride, /*size=*/1, stride, output_file);
    }
    const uint8_t* data_v = buffer->DataV();
    stride = buffer->StrideV();
    for (int i = 0; i < buffer->ChromaHeight(); ++i) {
      fwrite(data_v + i * stride, /*size=*/1, stride, output_file);
    }
  }
  if (ferror(output_file) != 0) {
    RTC_LOG(LS_ERROR) << "Error writing to file " << file_name;
  }
  fclose(output_file);
}

}  // namespace test
}  // namespace webrtc
