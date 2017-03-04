/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "webrtc/tools/frame_analyzer/reference_less_video_analysis_lib.h"
#include "webrtc/tools/simple_command_line_parser.h"

int main(int argc, char** argv) {
  // This captures the freezing metrics for reference less video analysis.
  std::string program_name = argv[0];
  std::string usage = "Outputs the freezing score by comparing current frame "
      "with the previous frame.\nExample usage:\n" + program_name +
      " --video_file=video_file.y4m\n"
      "Command line flags:\n"
      "  - video_file(string): Path of the video "
      "file to be analyzed. Only y4m file format is supported.\n";

  webrtc::test::CommandLineParser parser;

  // Init the parser and set the usage message.
  parser.Init(argc, argv);
  parser.SetUsageMessage(usage);

  parser.SetFlag("video_file", "");
  parser.ProcessFlags();
  if (parser.GetFlag("video_file").empty()) {
    parser.PrintUsageMessage();
    exit(EXIT_SUCCESS);
  }
  std::string video_file = parser.GetFlag("video_file");

  return run_analysis(video_file);
}
