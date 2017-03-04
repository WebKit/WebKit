/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_CODECS_TEST_STATS_H_
#define WEBRTC_MODULES_VIDEO_CODING_CODECS_TEST_STATS_H_

#include <vector>

#include "webrtc/common_video/include/video_image.h"

namespace webrtc {
namespace test {

// Contains statistics of a single frame that has been processed.
struct FrameStatistic {
  FrameStatistic();

  bool encoding_successful;
  bool decoding_successful;
  int encode_return_code;
  int decode_return_code;
  int encode_time_in_us;
  int decode_time_in_us;
  int qp;
  int frame_number;
  // How many packets were discarded of the encoded frame data (if any).
  int packets_dropped;
  size_t total_packets;

  // Current bit rate. Calculated out of the size divided with the time
  // interval per frame.
  int bit_rate_in_kbps;

  // Copied from EncodedImage
  size_t encoded_frame_length_in_bytes;
  webrtc::FrameType frame_type;
};

// Handles statistics from a single video processing run.
// Contains calculation methods for interesting metrics from these stats.
class Stats {
 public:
  typedef std::vector<FrameStatistic>::iterator FrameStatisticsIterator;

  Stats();
  virtual ~Stats();

  // Add a new statistic data object.
  // The frame number must be incrementing and start at zero in order to use
  // it as an index for the frame_statistics_ vector.
  // Returns the newly created statistic object.
  FrameStatistic& NewFrame(int frame_number);

  // Prints a summary of all the statistics that have been gathered during the
  // processing
  void PrintSummary();

  std::vector<FrameStatistic> stats_;
};

}  // namespace test
}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_CODING_CODECS_TEST_STATS_H_
