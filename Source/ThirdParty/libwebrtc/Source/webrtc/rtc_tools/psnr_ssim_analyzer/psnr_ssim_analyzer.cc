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

#include "rtc_tools/frame_analyzer/video_quality_analysis.h"
#include "rtc_tools/simple_command_line_parser.h"
#include "rtc_tools/video_file_reader.h"

void CompareFiles(
    const rtc::scoped_refptr<webrtc::test::Video>& reference_video,
    const rtc::scoped_refptr<webrtc::test::Video>& test_video,
    const char* results_file_name) {
  FILE* results_file = fopen(results_file_name, "w");

  const size_t num_frames = std::min(reference_video->number_of_frames(),
                                     test_video->number_of_frames());
  for (size_t i = 0; i < num_frames; ++i) {
    const rtc::scoped_refptr<webrtc::I420BufferInterface> ref_buffer =
        reference_video->GetFrame(i);
    const rtc::scoped_refptr<webrtc::I420BufferInterface> test_buffer =
        test_video->GetFrame(i);

    // Calculate the PSNR and SSIM.
    double result_psnr = webrtc::test::Psnr(ref_buffer, test_buffer);
    double result_ssim = webrtc::test::Ssim(ref_buffer, test_buffer);
    fprintf(results_file, "Frame: %zu, PSNR: %f, SSIM: %f\n", i, result_psnr,
            result_ssim);
  }

  fclose(results_file);
}

/*
 * A tool running PSNR and SSIM analysis on two videos - a reference video and a
 * test video. The two videos should be I420 Y4M videos.
 * The tool just runs PSNR and SSIM on the corresponding frames in the test and
 * the reference videos until either the first or the second video runs out of
 * frames. The result is written in a results text file in the format:
 * Frame: <frame_number>, PSNR: <psnr_value>, SSIM: <ssim_value>
 * Frame: <frame_number>, ........
 *
 * The max value for PSNR is 48.0 (between equal frames), as for SSIM it is 1.0.
 *
 * Usage:
 * psnr_ssim_analyzer --reference_file=<name_of_file> --test_file=<name_of_file>
 * --results_file=<name_of_file>
 */
int main(int argc, char* argv[]) {
  std::string program_name = argv[0];
  std::string usage =
      "Runs PSNR and SSIM on two I420 videos and write the"
      "results in a file.\n"
      "Example usage:\n" +
      program_name +
      " --reference_file=ref.yuv "
      "--test_file=test.yuv --results_file=results.txt\n"
      "Command line flags:\n"
      "  - reference_file(string): The reference YUV file to compare against."
      " Default: ref.yuv\n"
      "  - test_file(string): The test YUV file to run the analysis for."
      " Default: test_file.yuv\n"
      "  - results_file(string): The full name of the file where the results "
      "will be written. Default: results.txt\n";

  webrtc::test::CommandLineParser parser;

  // Init the parser and set the usage message
  parser.Init(argc, argv);
  parser.SetUsageMessage(usage);

  parser.SetFlag("results_file", "results.txt");
  parser.SetFlag("reference_file", "ref.yuv");
  parser.SetFlag("test_file", "test.yuv");
  parser.SetFlag("results_file", "results.txt");
  parser.SetFlag("help", "false");

  parser.ProcessFlags();
  if (parser.GetFlag("help") == "true") {
    parser.PrintUsageMessage();
    exit(EXIT_SUCCESS);
  }
  parser.PrintEnteredFlags();

  rtc::scoped_refptr<webrtc::test::Video> reference_video =
      webrtc::test::OpenY4mFile(parser.GetFlag("reference_file"));
  rtc::scoped_refptr<webrtc::test::Video> test_video =
      webrtc::test::OpenY4mFile(parser.GetFlag("test_file"));

  if (!reference_video || !test_video) {
    fprintf(stderr, "Error opening video files\n");
    return 0;
  }
  if (reference_video->width() != test_video->width() ||
      reference_video->height() != test_video->height()) {
    fprintf(stderr,
            "Reference and test video files do not have same size: %dx%d "
            "versus %dx%d\n",
            reference_video->width(), reference_video->height(),
            test_video->width(), test_video->height());
    return 0;
  }

  CompareFiles(reference_video, test_video,
               parser.GetFlag("results_file").c_str());
  return 0;
}
