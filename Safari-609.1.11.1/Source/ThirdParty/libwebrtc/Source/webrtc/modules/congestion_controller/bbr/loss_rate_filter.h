/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef MODULES_CONGESTION_CONTROLLER_BBR_LOSS_RATE_FILTER_H_
#define MODULES_CONGESTION_CONTROLLER_BBR_LOSS_RATE_FILTER_H_

#include <stdint.h>

namespace webrtc {
namespace bbr {

// Loss rate filter based on the implementation in SendSideBandwidthEstimation
// and the RTCPSender receiver report interval for video.
class LossRateFilter {
 public:
  LossRateFilter();
  void UpdateWithLossStatus(int64_t feedback_time_ms,
                            int packets_sent,
                            int packets_lost);
  double GetLossRate() const;

 private:
  int lost_packets_since_last_loss_update_;
  int expected_packets_since_last_loss_update_;
  double loss_rate_estimate_;
  int64_t next_loss_update_ms_;
};

}  // namespace bbr
}  // namespace webrtc

#endif  // MODULES_CONGESTION_CONTROLLER_BBR_LOSS_RATE_FILTER_H_
