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

#include <algorithm>
#include <string>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include "api/scoped_refptr.h"
#include "api/video/video_frame_buffer.h"
#include "rtc_tools/frame_analyzer/video_quality_analysis.h"
#include "rtc_tools/video_file_reader.h"

ABSL_FLAG(std::string,
          results_file,
          "results.txt",
          "The full name of the file where the results will be written");
ABSL_FLAG(std::string,
          reference_file,
          "ref.yuv",
          "The reference YUV file to compare against");
ABSL_FLAG(std::string,
          test_file,
          "test.yuv",
          "The test YUV file to run the analysis for");

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
  absl::SetProgramUsageMessage(
      "Runs PSNR and SSIM on two I420 videos and write the"
      "results in a file.\n"
      "Example usage:\n"
      "./psnr_ssim_analyzer --reference_file=ref.yuv "
      "--test_file=test.yuv --results_file=results.txt\n");
  absl::ParseCommandLine(argc, argv);

  rtc::scoped_refptr<webrtc::test::Video> reference_video =
      webrtc::test::OpenY4mFile(absl::GetFlag(FLAGS_reference_file));
  rtc::scoped_refptr<webrtc::test::Video> test_video =
      webrtc::test::OpenY4mFile(absl::GetFlag(FLAGS_test_file));

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
               absl::GetFlag(FLAGS_results_file).c_str());
  return 0;
}
