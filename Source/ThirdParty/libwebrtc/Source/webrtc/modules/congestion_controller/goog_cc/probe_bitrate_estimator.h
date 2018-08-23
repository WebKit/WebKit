/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_CONGESTION_CONTROLLER_GOOG_CC_PROBE_BITRATE_ESTIMATOR_H_
#define MODULES_CONGESTION_CONTROLLER_GOOG_CC_PROBE_BITRATE_ESTIMATOR_H_

#include <limits>
#include <map>

#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"

namespace webrtc {
class RtcEventLog;

class ProbeBitrateEstimator {
 public:
  explicit ProbeBitrateEstimator(RtcEventLog* event_log);
  ~ProbeBitrateEstimator();

  // Should be called for every probe packet we receive feedback about.
  // Returns the estimated bitrate if the probe completes a valid cluster.
  int HandleProbeAndEstimateBitrate(const PacketFeedback& packet_feedback);

  absl::optional<int> FetchAndResetLastEstimatedBitrateBps();

 private:
  struct AggregatedCluster {
    int num_probes = 0;
    int64_t first_send_ms = std::numeric_limits<int64_t>::max();
    int64_t last_send_ms = 0;
    int64_t first_receive_ms = std::numeric_limits<int64_t>::max();
    int64_t last_receive_ms = 0;
    int size_last_send = 0;
    int size_first_receive = 0;
    int size_total = 0;
  };

  // Erases old cluster data that was seen before |timestamp_ms|.
  void EraseOldClusters(int64_t timestamp_ms);

  std::map<int, AggregatedCluster> clusters_;
  RtcEventLog* const event_log_;
  absl::optional<int> estimated_bitrate_bps_;
};

}  // namespace webrtc

#endif  // MODULES_CONGESTION_CONTROLLER_GOOG_CC_PROBE_BITRATE_ESTIMATOR_H_
