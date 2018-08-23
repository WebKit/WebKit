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

#include <map>
#include <string>
#include <vector>

#include "rtc_tools/converter/converter.h"
#include "rtc_tools/simple_command_line_parser.h"

/*
 * A command-line tool based on libyuv to convert a set of RGBA files to a YUV
 * video.
 * Usage:
 * rgba_to_i420_converter --frames_dir=<directory_to_rgba_frames>
 * --output_file=<output_yuv_file> --width=<width_of_input_frames>
 * --height=<height_of_input_frames>
 */
int main(int argc, char* argv[]) {
  std::string program_name = argv[0];
  std::string usage =
      "Converts RGBA raw image files to I420 frames for YUV.\n"
      "Example usage:\n" +
      program_name +
      " --frames_dir=. --output_file=output.yuv --width=320 --height=240\n"
      "IMPORTANT: If you pass the --delete_frames command line parameter, the "
      "tool will delete the input frames after conversion.\n"
      "Command line flags:\n"
      "  - width(int): Width in pixels of the frames in the input file."
      " Default: -1\n"
      "  - height(int): Height in pixels of the frames in the input file."
      " Default: -1\n"
      "  - frames_dir(string): The path to the directory where the frames "
      "reside."
      " Default: .\n"
      "  - output_file(string): The output file to which frames are written."
      " Default: output.yuv\n"
      "  - delete_frames(bool): Whether or not to delete the input frames after"
      " the conversion. Default: false.\n";

  webrtc::test::CommandLineParser parser;

  // Init the parser and set the usage message
  parser.Init(argc, argv);
  parser.SetUsageMessage(usage);

  parser.SetFlag("width", "-1");
  parser.SetFlag("height", "-1");
  parser.SetFlag("frames_dir", ".");
  parser.SetFlag("output_file", "output.yuv");
  parser.SetFlag("delete_frames", "false");
  parser.SetFlag("help", "false");

  parser.ProcessFlags();
  if (parser.GetFlag("help") == "true") {
    parser.PrintUsageMessage();
    exit(EXIT_SUCCESS);
  }
  parser.PrintEnteredFlags();

  int width = strtol((parser.GetFlag("width")).c_str(), NULL, 10);
  int height = strtol((parser.GetFlag("height")).c_str(), NULL, 10);

  if (width <= 0 || height <= 0) {
    fprintf(stderr, "Error: width or height cannot be <= 0!\n");
    return -1;
  }

  bool del_frames = (parser.GetFlag("delete_frames") == "true") ? true : false;

  webrtc::test::Converter converter(width, height);
  bool success = converter.ConvertRGBAToI420Video(
      parser.GetFlag("frames_dir"), parser.GetFlag("output_file"), del_frames);

  if (success) {
    fprintf(stdout, "Successful conversion of RGBA frames to YUV video!\n");
    return 0;
  } else {
    fprintf(stdout, "Unsuccessful conversion of RGBA frames to YUV video!\n");
    return -1;
  }
}
