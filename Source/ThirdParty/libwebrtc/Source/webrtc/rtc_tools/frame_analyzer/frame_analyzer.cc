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

#include <cstddef>
#include <string>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/strings/match.h"
#include "api/scoped_refptr.h"
#include "rtc_base/strings/string_builder.h"
#include "rtc_tools/frame_analyzer/video_color_aligner.h"
#include "rtc_tools/frame_analyzer/video_geometry_aligner.h"
#include "rtc_tools/frame_analyzer/video_quality_analysis.h"
#include "rtc_tools/frame_analyzer/video_temporal_aligner.h"
#include "rtc_tools/video_file_reader.h"
#include "rtc_tools/video_file_writer.h"
#include "test/testsupport/perf_test.h"

ABSL_FLAG(int32_t, width, -1, "The width of the reference and test files");
ABSL_FLAG(int32_t, height, -1, "The height of the reference and test files");
ABSL_FLAG(std::string,
          label,
          "MY_TEST",
          "The label to use for the perf output");
ABSL_FLAG(std::string,
          reference_file,
          "ref.yuv",
          "The reference YUV file to run the analysis against");
ABSL_FLAG(std::string,
          test_file,
          "test.yuv",
          "The test YUV file to run the analysis for");
ABSL_FLAG(std::string,
          aligned_output_file,
          "",
          "Where to write aligned YUV/Y4M output file, f not present, no files "
          "will be written");
ABSL_FLAG(std::string,
          yuv_directory,
          "",
          "Where to write aligned YUV ref+test output files, if not present, "
          "no files will be written");
ABSL_FLAG(std::string,
          chartjson_result_file,
          "",
          "Where to store perf result in chartjson format, if not present, no "
          "perf result will be stored");

namespace {

#ifdef WIN32
const char* const kPathDelimiter = "\\";
#else
const char* const kPathDelimiter = "/";
#endif

std::string JoinFilename(std::string directory, std::string filename) {
  return directory + kPathDelimiter + filename;
}

}  // namespace

/*
 * A command line tool running PSNR and SSIM on a reference video and a test
 * video. The test video is a record of the reference video which can start at
 * an arbitrary point. It is possible that there will be repeated frames or
 * skipped frames as well. The video files should be I420 .y4m or .yuv videos.
 * If both files are .y4m, it's not needed to specify width/height. The tool
 * prints the result to standard output in the Chromium perf format:
 * RESULT <metric>:<label>= <values>
 *
 * The max value for PSNR is 48.0 (between equal frames), as for SSIM it is 1.0.
 *
 * Usage:
 * frame_analyzer --label=<test_label> --reference_file=<name_of_file>
 * --test_file_ref=<name_of_file> --width=<frame_width> --height=<frame_height>
 */
int main(int argc, char* argv[]) {
  absl::ParseCommandLine(argc, argv);

  int width = absl::GetFlag(FLAGS_width);
  int height = absl::GetFlag(FLAGS_height);
  const std::string reference_file_name = absl::GetFlag(FLAGS_reference_file);
  const std::string test_file_name = absl::GetFlag(FLAGS_test_file);

  // .yuv files require explicit resolution.
  if ((absl::EndsWith(reference_file_name, ".yuv") ||
       absl::EndsWith(test_file_name, ".yuv")) &&
      (width <= 0 || height <= 0)) {
    fprintf(stderr,
            "Error: You need to specify width and height when using .yuv "
            "files\n");
    return -1;
  }

  webrtc::test::ResultsContainer results;

  rtc::scoped_refptr<webrtc::test::Video> reference_video =
      webrtc::test::OpenYuvOrY4mFile(reference_file_name, width, height);
  rtc::scoped_refptr<webrtc::test::Video> test_video =
      webrtc::test::OpenYuvOrY4mFile(test_file_name, width, height);

  if (!reference_video || !test_video) {
    fprintf(stderr, "Error opening video files\n");
    return 1;
  }

  const std::vector<size_t> matching_indices =
      webrtc::test::FindMatchingFrameIndices(reference_video, test_video);

  // Align the reference video both temporally and geometrically. I.e. align the
  // frames to match up in order to the test video, and align a crop region of
  // the reference video to match up to the test video.
  const rtc::scoped_refptr<webrtc::test::Video> aligned_reference_video =
      AdjustCropping(ReorderVideo(reference_video, matching_indices),
                     test_video);

  // Calculate if there is any systematic color difference between the reference
  // and test video.
  const webrtc::test::ColorTransformationMatrix color_transformation =
      CalculateColorTransformationMatrix(aligned_reference_video, test_video);

  char buf[256];
  rtc::SimpleStringBuilder string_builder(buf);
  for (int i = 0; i < 3; ++i) {
    string_builder << "\n";
    for (int j = 0; j < 4; ++j)
      string_builder.AppendFormat("%6.2f ", color_transformation[i][j]);
  }
  printf("Adjusting test video with color transformation: %s\n",
         string_builder.str());

  // Adjust all frames in the test video with the calculated color
  // transformation.
  const rtc::scoped_refptr<webrtc::test::Video> color_adjusted_test_video =
      AdjustColors(color_transformation, test_video);

  results.frames = webrtc::test::RunAnalysis(
      aligned_reference_video, color_adjusted_test_video, matching_indices);

  const std::vector<webrtc::test::Cluster> clusters =
      webrtc::test::CalculateFrameClusters(matching_indices);
  results.max_repeated_frames = webrtc::test::GetMaxRepeatedFrames(clusters);
  results.max_skipped_frames = webrtc::test::GetMaxSkippedFrames(clusters);
  results.total_skipped_frames =
      webrtc::test::GetTotalNumberOfSkippedFrames(clusters);
  results.decode_errors_ref = 0;
  results.decode_errors_test = 0;

  webrtc::test::PrintAnalysisResults(absl::GetFlag(FLAGS_label), &results);

  std::string chartjson_result_file =
      absl::GetFlag(FLAGS_chartjson_result_file);
  if (!chartjson_result_file.empty()) {
    if (!webrtc::test::WritePerfResults(chartjson_result_file)) {
      return 1;
    }
  }
  std::string aligned_output_file = absl::GetFlag(FLAGS_aligned_output_file);
  if (!aligned_output_file.empty()) {
    webrtc::test::WriteVideoToFile(aligned_reference_video, aligned_output_file,
                                   /*fps=*/30);
  }
  std::string yuv_directory = absl::GetFlag(FLAGS_yuv_directory);
  if (!yuv_directory.empty()) {
    webrtc::test::WriteVideoToFile(aligned_reference_video,
                                   JoinFilename(yuv_directory, "ref.yuv"),
                                   /*fps=*/30);
    webrtc::test::WriteVideoToFile(color_adjusted_test_video,
                                   JoinFilename(yuv_directory, "test.yuv"),
                                   /*fps=*/30);
  }

  return 0;
}
