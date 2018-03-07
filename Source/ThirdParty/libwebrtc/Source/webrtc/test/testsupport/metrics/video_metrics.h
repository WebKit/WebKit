/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TESTSUPPORT_METRICS_VIDEO_METRICS_H_
#define TESTSUPPORT_METRICS_VIDEO_METRICS_H_

#include <limits>
#include <vector>

namespace webrtc {
namespace test {

// The highest PSNR value our algorithms will return.
extern double kMetricsPerfectPSNR;

// Contains video quality metrics result for a single frame.
struct FrameResult {
  int frame_number;
  double value;
};

// Result from a PSNR/SSIM calculation operation.
// The frames in this data structure are 0-indexed.
struct QualityMetricsResult {
  QualityMetricsResult() :
    average(0.0),
    min(std::numeric_limits<double>::max()),
    max(std::numeric_limits<double>::min()),
    min_frame_number(-1),
    max_frame_number(-1)
  {};
  double average;
  double min;
  double max;
  int min_frame_number;
  int max_frame_number;
  std::vector<FrameResult> frames;
};

// Calculates PSNR and SSIM values for the reference and test video files
// (must be in I420 format). All calculated values are filled into the
// QualityMetricsResult structs.
//
// PSNR values have the unit decibel (dB) where a high value means the test file
// is similar to the reference file. The higher value, the more similar. The
// maximum PSNR value is kMetricsInfinitePSNR. For more info about PSNR, see
// http://en.wikipedia.org/wiki/PSNR.
//
// SSIM values range between -1.0 and 1.0, where 1.0 means the files are
// identical. For more info about SSIM, see http://en.wikipedia.org/wiki/SSIM
// This function only compares video frames up to the point when the shortest
// video ends.
// Return value:
//  0 if successful, negative on errors:
// -1 if the source file cannot be opened
// -2 if the test file cannot be opened
// -3 if any of the files are empty
// -4 if any arguments are invalid.
int I420MetricsFromFiles(const char* ref_filename,
                         const char* test_filename,
                         int width,
                         int height,
                         QualityMetricsResult* psnr_result,
                         QualityMetricsResult* ssim_result);

// Calculates PSNR values for the reference and test video files (must be in
// I420 format). All calculated values are filled into the QualityMetricsResult
// struct.
//
// PSNR values have the unit decibel (dB) where a high value means the test file
// is similar to the reference file. The higher value, the more similar. The
// maximum PSNR value is kMetricsInfinitePSNR. For more info about PSNR, see
// http://en.wikipedia.org/wiki/PSNR.
//
// This function only compares video frames up to the point when the shortest
// video ends.
//
// Return value:
//  0 if successful, negative on errors:
// -1 if the source file cannot be opened
// -2 if the test file cannot be opened
// -3 if any of the files are empty
// -4 if any arguments are invalid.
int I420PSNRFromFiles(const char* ref_filename,
                      const char* test_filename,
                      int width,
                      int height,
                      QualityMetricsResult* result);

// Calculates SSIM values for the reference and test video files (must be in
// I420 format). All calculated values are filled into the QualityMetricsResult
// struct.
// SSIM values range between -1.0 and 1.0, where 1.0 means the files are
// identical.
// This function only compares video frames up to the point when the shortest
// video ends.
// For more info about SSIM, see http://en.wikipedia.org/wiki/SSIM
//
// Return value:
//  0 if successful, negative on errors:
// -1 if the source file cannot be opened
// -2 if the test file cannot be opened
// -3 if any of the files are empty
// -4 if any arguments are invalid.
int I420SSIMFromFiles(const char* ref_filename,
                      const char* test_filename,
                      int width,
                      int height,
                      QualityMetricsResult* result);

}  // namespace test
}  // namespace webrtc

#endif // TESTSUPPORT_METRICS_VIDEO_METRICS_H_
