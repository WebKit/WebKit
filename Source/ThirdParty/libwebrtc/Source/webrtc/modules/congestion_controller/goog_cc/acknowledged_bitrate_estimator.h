/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_CONGESTION_CONTROLLER_GOOG_CC_ACKNOWLEDGED_BITRATE_ESTIMATOR_H_
#define MODULES_CONGESTION_CONTROLLER_GOOG_CC_ACKNOWLEDGED_BITRATE_ESTIMATOR_H_

#include <memory>
#include <vector>

#include "absl/types/optional.h"
#include "modules/congestion_controller/goog_cc/bitrate_estimator.h"

namespace webrtc {

struct PacketFeedback;

class AcknowledgedBitrateEstimator {
 public:
  explicit AcknowledgedBitrateEstimator(
      std::unique_ptr<BitrateEstimator> bitrate_estimator);

  AcknowledgedBitrateEstimator();
  ~AcknowledgedBitrateEstimator();

  void IncomingPacketFeedbackVector(
      const std::vector<PacketFeedback>& packet_feedback_vector);
  absl::optional<uint32_t> bitrate_bps() const;
  void SetAlrEndedTimeMs(int64_t alr_ended_time_ms);
  void SetAllocatedBitrateWithoutFeedback(uint32_t bitrate_bps);

 private:
  void MaybeExpectFastRateChange(int64_t packet_arrival_time_ms);
  absl::optional<int64_t> alr_ended_time_ms_;
  std::unique_ptr<BitrateEstimator> bitrate_estimator_;
  absl::optional<uint32_t> allocated_bitrate_without_feedback_bps_;
};

}  // namespace webrtc

#endif  // MODULES_CONGESTION_CONTROLLER_GOOG_CC_ACKNOWLEDGED_BITRATE_ESTIMATOR_H_
