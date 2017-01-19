/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/tools/frame_editing/frame_editing_lib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "webrtc/tools/simple_command_line_parser.h"

// A command-line tool to edit a YUV-video (I420 sub-sampled).
int main(int argc, char** argv) {
  std::string program_name = argv[0];
  std::string usage = "Deletes a series of frames in a yuv file."
    " Only I420 is supported!\n"
    "Example usage:\n" + program_name +
    " --in_path=input.yuv --width=320 --height=240 --f=60 --interval=1 --l=120"
    " --out_path=edited_clip.yuv\n"
    "Command line flags:\n"
    "--in_path(string): Path and filename to the input file\n"
    "--width(int): Width in pixels of the frames in the input file."
    " Default: -1\n"
    "--height(int): Height in pixels of the frames in the input file."
    " Default: -1\n"
    "--f(int): First frame to process. Default: -1\n"
    "--l(int): Last frame to process.  Default: -1\n"
    "Frame numbering starts at 1. The set of frames to be processed includes "
    "the frame with the number <f> and <l>.\n"
    "--interval(int): Interval specifies with what ratio the number of frames "
    "should be increased or decreased with.\n"
    "If you set <interval> to a positive number, frames between <f> and <l> "
    "will be inserted <interval> times."
    " If you set <interval> to a negative number then the amount of frames "
    "between <f> and <l> will be decreased with a ratio of abs(interval)."
    " Set interval=-1 if every frame between <f> and <l> should be "
    "deleted. Set interval=-2 if every second frame should be deleted, and so "
    "on. Frame numbering between <f> and <l> starts with 1 and frames with"
    " number n where (n - 1)  % interval == 0 will be kept.\n"
    "Example 1:\n"
    "If one clip has 10 frames (1 to 10) and you specify <f>=4, <l>=7 and "
    "interval=2, then you will get a clip that contains frame "
    "1, 2, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 9 and 10.\n"
    "Example 2:\n"
    "If you specify f=4, l=7 and interval=-1, then you will get a clip that"
    " contains frame 1, 2, 3, 8, 9 and 10.\n"
    "Example 3:\n"
    "If one clip has 10 frames (1 to 10), and you specify f=1, l=10 and "
    " interval=-4, then you will get a clip that contains frame "
    "1, 5 and 9.\n"
    "No interpolation is done when up-sampling."
    " Default: -1\n"
    "--out_path(string): The output file to which frames are written."
    " Default: output.yuv\n";

  webrtc::test::CommandLineParser parser;

  // Init the parser and set the usage message
  parser.Init(argc, argv);
  parser.SetUsageMessage(usage);
  parser.SetFlag("in_path", "-1");
  parser.SetFlag("width", "-1");
  parser.SetFlag("height", "-1");
  parser.SetFlag("f", "-1");
  parser.SetFlag("interval", "-1");
  parser.SetFlag("l", "-1");
  parser.SetFlag("out_path", "edited_output.yuv");
  parser.SetFlag("help", "false");

  parser.ProcessFlags();
  if (parser.GetFlag("help") == "true") {
    parser.PrintUsageMessage();
    exit(EXIT_SUCCESS);
  }
  parser.PrintEnteredFlags();

  const char* in_path = parser.GetFlag("in_path").c_str();
  int width = strtol((parser.GetFlag("width")).c_str(), NULL, 10);
  int height = strtol((parser.GetFlag("height")).c_str(), NULL, 10);
  int first_frame_to_cut = strtol((parser.GetFlag("f")).c_str(), NULL, 10);
  int interval = strtol((parser.GetFlag("interval")).c_str(), NULL, 10);
  int last_frame_to_cut = strtol((parser.GetFlag("l")).c_str(), NULL, 10);

  const char* out_path = parser.GetFlag("out_path").c_str();

  if (!strcmp(in_path, "-1")) {
    fprintf(stderr, "You must specify a file to edit\n");
    return -1;
  }

  if (first_frame_to_cut <= 0 || last_frame_to_cut <= 0) {
    fprintf(stderr, "Error: You must specify which frames to cut!\n");
    return -2;
  }

  if (width <= 0 || height <= 0) {
    fprintf(stderr, "Error: width or height cannot be <= 0!\n");
    return -3;
  }
  return webrtc::EditFrames(in_path, width, height, first_frame_to_cut,
                            interval, last_frame_to_cut, out_path);
}
