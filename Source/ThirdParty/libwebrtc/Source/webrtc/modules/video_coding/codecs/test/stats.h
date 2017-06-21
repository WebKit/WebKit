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

#include "webrtc/common_types.h"

namespace webrtc {
namespace test {

// Contains statistics of a single processed frame.
struct FrameStatistic {
  bool encoding_successful = false;
  bool decoding_successful = false;
  int encode_return_code = 0;
  int decode_return_code = 0;
  int encode_time_in_us = 0;
  int decode_time_in_us = 0;
  int qp = -1;
  int frame_number = 0;
  // How many packets were discarded of the encoded frame data (if any).
  int packets_dropped = 0;
  size_t total_packets = 0;

  // Current bit rate. Calculated out of the size divided with the time
  // interval per frame.
  int bit_rate_in_kbps = 0;

  // Copied from EncodedImage.
  size_t encoded_frame_length_in_bytes = 0;
  webrtc::FrameType frame_type = kVideoFrameDelta;
};

// Handles statistics from a single video processing run.
// Contains calculation methods for interesting metrics from these stats.
class Stats {
 public:
  typedef std::vector<FrameStatistic>::iterator FrameStatisticsIterator;

  Stats();
  virtual ~Stats();

  // Add a new statistic data object.
  // The |frame_number| must be incrementing and start at zero in order to use
  // it as an index for the FrameStatistic vector.
  // Returns the newly created statistic object.
  FrameStatistic& NewFrame(int frame_number);

  // Prints a summary of all the statistics that have been gathered during the
  // processing.
  void PrintSummary();

  std::vector<FrameStatistic> stats_;
};

}  // namespace test
}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_CODING_CODECS_TEST_STATS_H_
