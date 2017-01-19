/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_TOOLS_FRAME_ANALYZER_VIDEO_QUALITY_ANALYSIS_H_
#define WEBRTC_TOOLS_FRAME_ANALYZER_VIDEO_QUALITY_ANALYSIS_H_

#include <string>
#include <vector>

#include "libyuv/compare.h"  // NOLINT
#include "libyuv/convert.h"  // NOLINT

namespace webrtc {
namespace test {

struct AnalysisResult {
  AnalysisResult() {}
  AnalysisResult(int frame_number, double psnr_value, double ssim_value)
      : frame_number(frame_number),
        psnr_value(psnr_value),
        ssim_value(ssim_value) {}
  int frame_number;
  double psnr_value;
  double ssim_value;
};

struct ResultsContainer {
  ResultsContainer();
  ~ResultsContainer();

  std::vector<AnalysisResult> frames;
};

enum VideoAnalysisMetricsType {kPSNR, kSSIM};

// A function to run the PSNR and SSIM analysis on the test file. The test file
// comprises the frames that were captured during the quality measurement test.
// There may be missing or duplicate frames. Also the frames start at a random
// position in the original video. We should provide a statistics file along
// with the test video. The stats file contains the connection between the
// actual frames in the test file and their position in the reference video, so
// that the analysis could run with the right frames from both videos. The stats
// file should be in the form 'frame_xxxx yyyy', where xxxx is the consecutive
// number of the frame in the test video, and yyyy is the equivalent frame in
// the reference video. The stats file could be produced by
// tools/barcode_tools/barcode_decoder.py. This script decodes the barcodes
// integrated in every video and generates the stats file. If three was some
// problem with the decoding there would be 'Barcode error' instead of yyyy.
void RunAnalysis(const char* reference_file_name, const char* test_file_name,
                 const char* stats_file_name, int width, int height,
                 ResultsContainer* results);

// Compute PSNR or SSIM for an I420 frame (all planes). When we are calculating
// PSNR values, the max return value (in the case where the test and reference
// frames are exactly the same) will be 48. In the case of SSIM the max return
// value will be 1.
double CalculateMetrics(VideoAnalysisMetricsType video_metrics_type,
                        const uint8_t* ref_frame,
                        const uint8_t* test_frame,
                        int width,
                        int height);

// Prints the result from the analysis in Chromium performance
// numbers compatible format to stdout. If the results object contains no frames
// no output will be written.
void PrintAnalysisResults(const std::string& label, ResultsContainer* results);

// Similar to the above, but will print to the specified file handle.
void PrintAnalysisResults(FILE* output, const std::string& label,
                          ResultsContainer* results);

// Calculates max repeated and skipped frames and prints them to stdout in a
// format that is compatible with Chromium performance numbers.
void PrintMaxRepeatedAndSkippedFrames(const std::string& label,
                                      const std::string& stats_file_name);

// Similar to the above, but will print to the specified file handle.
void PrintMaxRepeatedAndSkippedFrames(FILE* output, const std::string& label,
                                      const std::string& stats_file_name);

// Gets the next line from an open stats file.
bool GetNextStatsLine(FILE* stats_file, char* line);

// Calculates the size of a I420 frame if given the width and height.
int GetI420FrameSize(int width, int height);

// Extract the sequence of the frame in the video. I.e. if line is
// frame_0023 0284, we will get 23.
int ExtractFrameSequenceNumber(std::string line);

// Checks if there is 'Barcode error' for the given line.
bool IsThereBarcodeError(std::string line);

// Extract the frame number in the reference video. I.e. if line is
// frame_0023 0284, we will get 284.
int ExtractDecodedFrameNumber(std::string line);

// Extracts an I420 frame at position frame_number from the raw YUV file.
bool ExtractFrameFromYuvFile(const char* i420_file_name,
                             int width,
                             int height,
                             int frame_number,
                             uint8_t* result_frame);

// Extracts an I420 frame at position frame_number from the Y4M file. The first
// frame has corresponded |frame_number| 0.
bool ExtractFrameFromY4mFile(const char* i420_file_name,
                             int width,
                             int height,
                             int frame_number,
                             uint8_t* result_frame);

}  // namespace test
}  // namespace webrtc

#endif  // WEBRTC_TOOLS_FRAME_ANALYZER_VIDEO_QUALITY_ANALYSIS_H_
