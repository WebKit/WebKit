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

#include "webrtc/tools/frame_analyzer/video_quality_analysis.h"
#include "webrtc/tools/simple_command_line_parser.h"

/*
 * A command line tool running PSNR and SSIM on a reference video and a test
 * video. The test video is a record of the reference video which can start at
 * an arbitrary point. It is possible that there will be repeated frames or
 * skipped frames as well. In order to have a way to compare corresponding
 * frames from the two videos, two stats files should be provided. One for the
 * reference video and one for the test video. The stats file
 * is a text file assumed to be in the format:
 * frame_xxxx yyyy where xxxx is the frame number in and yyyy is the
 * corresponding barcode. The video files should be 1420 YUV videos.
 * The tool prints the result to standard output in the Chromium perf format:
 * RESULT <metric>:<label>= <values>
 *
 * The max value for PSNR is 48.0 (between equal frames), as for SSIM it is 1.0.
 *
 * Usage:
 * frame_analyzer --label=<test_label> --reference_file=<name_of_file>
 * --test_file_ref=<name_of_file> --stats_file_test=<name_of_file>
 * --stats_file=<name_of_file> --width=<frame_width>
 * --height=<frame_height>
 */
int main(int argc, char** argv) {
  std::string program_name = argv[0];
  std::string usage =
      "Compares the output video with the initially sent video."
      "\nExample usage:\n" +
      program_name +
      " --reference_file=ref.yuv --test_file=test.yuv --width=320 "
      "--height=240\n"
      "Command line flags:\n"
      "  - width(int): The width of the reference and test files. Default: -1\n"
      "  - height(int): The height of the reference and test files. "
      " Default: -1\n"
      "  - label(string): The label to use for the perf output."
      " Default: MY_TEST\n"
      "  - stats_file_ref(string): The path to the stats file that will be"
      " produced for the reference video file."
      " Default: stats_ref.txt\n"
      "  - stats_file_test(string): The path to the stats file that will be"
      " produced for the test video file."
      " Default: stats_test.txt\n"
      "  - reference_file(string): The reference YUV file to compare against."
      " Default: ref.yuv\n"
      "  - test_file(string): The test YUV file to run the analysis for."
      " Default: test_file.yuv\n";

  webrtc::test::CommandLineParser parser;

  // Init the parser and set the usage message
  parser.Init(argc, argv);
  parser.SetUsageMessage(usage);

  parser.SetFlag("width", "-1");
  parser.SetFlag("height", "-1");
  parser.SetFlag("label", "MY_TEST");
  parser.SetFlag("stats_file_ref", "stats_ref.txt");
  parser.SetFlag("stats_file_test", "stats_test.txt");
  parser.SetFlag("reference_file", "ref.yuv");
  parser.SetFlag("test_file", "test.yuv");
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

  webrtc::test::ResultsContainer results;

  webrtc::test::RunAnalysis(parser.GetFlag("reference_file").c_str(),
                            parser.GetFlag("test_file").c_str(),
                            parser.GetFlag("stats_file_ref").c_str(),
                            parser.GetFlag("stats_file_test").c_str(), width,
                            height, &results);

  std::string label = parser.GetFlag("label");
  webrtc::test::PrintAnalysisResults(label, &results);
  webrtc::test::PrintMaxRepeatedAndSkippedFrames(
      label, parser.GetFlag("stats_file_ref"),
      parser.GetFlag("stats_file_test"));
}
