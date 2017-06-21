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

#include <memory>
#include <string>

#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "webrtc/tools/frame_editing/frame_editing_lib.h"
#include "webrtc/typedefs.h"

namespace webrtc {

int EditFrames(const std::string& in_path, int width, int height,
               int first_frame_to_process, int interval,
               int last_frame_to_process, const std::string& out_path) {
  if (last_frame_to_process < first_frame_to_process) {
    fprintf(stderr, "The set of frames to cut is empty! (l < f)\n");
    return -10;
  }

  FILE* in_fid = fopen(in_path.c_str() , "rb");
  if (!in_fid) {
    fprintf(stderr, "Could not read input file: %s.\n", in_path.c_str());
    return -11;
  }

  // Frame size of I420.
  size_t frame_length = CalcBufferSize(VideoType::kI420, width, height);

  std::unique_ptr<uint8_t[]> temp_buffer(new uint8_t[frame_length]);

  FILE* out_fid = fopen(out_path.c_str(), "wb");

  if (!out_fid) {
    fprintf(stderr, "Could not open output file: %s.\n", out_path.c_str());
    fclose(in_fid);
    return -12;
  }

  int num_frames_read = 0;
  int num_frames_read_between = 0;
  size_t num_bytes_read;

  while ((num_bytes_read = fread(temp_buffer.get(), 1, frame_length, in_fid))
      == frame_length) {
    num_frames_read++;
    if ((num_frames_read < first_frame_to_process) ||
        (last_frame_to_process < num_frames_read)) {
      fwrite(temp_buffer.get(), 1, frame_length, out_fid);
    } else {
      num_frames_read_between++;
      if (interval <= 0) {
        if (interval == -1) {
          // Remove all frames.
        } else {
          if (((num_frames_read_between - 1) % interval) == 0) {
            // Keep only every |interval| frame.
            fwrite(temp_buffer.get(), 1, frame_length, out_fid);
          }
        }
      } else if (interval > 0) {
        for (int i = 1; i <= interval; ++i) {
          fwrite(temp_buffer.get(), 1, frame_length, out_fid);
        }
      }
    }
  }
  if (num_bytes_read > 0 && num_bytes_read < frame_length) {
    printf("Frame to small! Last frame truncated.\n");
  }
  fclose(in_fid);
  fclose(out_fid);

  printf("Done editing!\n");
  return 0;
}
}  // namespace webrtc
