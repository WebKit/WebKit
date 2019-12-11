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

#include <string>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include "rtc_tools/converter/converter.h"

ABSL_FLAG(int, width, -1, "Width in pixels of the frames in the input file");
ABSL_FLAG(int, height, -1, "Height in pixels of the frames in the input file");
ABSL_FLAG(std::string,
          frames_dir,
          ".",
          "The path to the directory where the frames reside");
ABSL_FLAG(std::string,
          output_file,
          "output.yuv",
          " The output file to which frames are written");
ABSL_FLAG(bool,
          delete_frames,
          false,
          " Whether or not to delete the input frames after the conversion");

/*
 * A command-line tool based on libyuv to convert a set of RGBA files to a YUV
 * video.
 * Usage:
 * rgba_to_i420_converter --frames_dir=<directory_to_rgba_frames>
 * --output_file=<output_yuv_file> --width=<width_of_input_frames>
 * --height=<height_of_input_frames>
 */
int main(int argc, char* argv[]) {
  absl::SetProgramUsageMessage(
      "Converts RGBA raw image files to I420 frames "
      "for YUV.\n"
      "Example usage:\n"
      "./rgba_to_i420_converter --frames_dir=. "
      "--output_file=output.yuv --width=320 "
      "--height=240\n"
      "IMPORTANT: If you pass the --delete_frames "
      "command line parameter, the tool will delete "
      "the input frames after conversion.\n");
  absl::ParseCommandLine(argc, argv);

  int width = absl::GetFlag(FLAGS_width);
  int height = absl::GetFlag(FLAGS_height);

  if (width <= 0 || height <= 0) {
    fprintf(stderr, "Error: width or height cannot be <= 0!\n");
    return -1;
  }

  bool del_frames = absl::GetFlag(FLAGS_delete_frames);

  webrtc::test::Converter converter(width, height);
  bool success = converter.ConvertRGBAToI420Video(
      absl::GetFlag(FLAGS_frames_dir), absl::GetFlag(FLAGS_output_file),
      del_frames);

  if (success) {
    fprintf(stdout, "Successful conversion of RGBA frames to YUV video!\n");
    return 0;
  } else {
    fprintf(stdout, "Unsuccessful conversion of RGBA frames to YUV video!\n");
    return -1;
  }
}
