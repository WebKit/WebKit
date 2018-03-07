/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_CODECS_TEST_STATS_H_
#define MODULES_VIDEO_CODING_CODECS_TEST_STATS_H_

#include <vector>

#include "common_types.h"  // NOLINT(build/include)

namespace webrtc {
namespace test {

// Statistics for one processed frame.
struct FrameStatistic {
  explicit FrameStatistic(int frame_number) : frame_number(frame_number) {}
  const int frame_number = 0;

  // Encoding.
  int64_t encode_start_ns = 0;
  int encode_return_code = 0;
  bool encoding_successful = false;
  int encode_time_us = 0;
  int bitrate_kbps = 0;
  size_t encoded_frame_size_bytes = 0;
  webrtc::FrameType frame_type = kVideoFrameDelta;

  // H264 specific.
  rtc::Optional<size_t> max_nalu_length;

  // Decoding.
  int64_t decode_start_ns = 0;
  int decode_return_code = 0;
  bool decoding_successful = false;
  int decode_time_us = 0;
  int decoded_width = 0;
  int decoded_height = 0;

  // Quantization.
  int qp = -1;

  // How many packets were discarded of the encoded frame data (if any).
  int packets_dropped = 0;
  size_t total_packets = 0;
  size_t manipulated_length = 0;

  // Quality.
  float psnr = 0.0;
  float ssim = 0.0;
};

// Statistics for a sequence of processed frames. This class is not thread safe.
class Stats {
 public:
  Stats() = default;
  ~Stats() = default;

  // Creates a FrameStatistic for the next frame to be processed.
  FrameStatistic* AddFrame();

  // Returns the FrameStatistic corresponding to |frame_number|.
  FrameStatistic* GetFrame(int frame_number);

  size_t size() const;

  // TODO(brandtr): Add output as CSV.
  void PrintSummary() const;

 private:
  std::vector<FrameStatistic> stats_;
};

}  // namespace test
}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_CODECS_TEST_STATS_H_
