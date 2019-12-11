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
#include "api/transport/network_types.h"
#include "api/transport/webrtc_key_value_config.h"
#include "api/units/data_rate.h"
#include "modules/congestion_controller/goog_cc/bitrate_estimator.h"

namespace webrtc {

class AcknowledgedBitrateEstimator {
 public:
  AcknowledgedBitrateEstimator(
      const WebRtcKeyValueConfig* key_value_config,
      std::unique_ptr<BitrateEstimator> bitrate_estimator);

  explicit AcknowledgedBitrateEstimator(
      const WebRtcKeyValueConfig* key_value_config);
  ~AcknowledgedBitrateEstimator();

  void IncomingPacketFeedbackVector(
      const std::vector<PacketResult>& packet_feedback_vector);
  absl::optional<DataRate> bitrate() const;
  absl::optional<DataRate> PeekRate() const;
  void SetAlr(bool in_alr);
  void SetAlrEndedTime(Timestamp alr_ended_time);

 private:
  absl::optional<Timestamp> alr_ended_time_;
  bool in_alr_;
  std::unique_ptr<BitrateEstimator> bitrate_estimator_;
};

}  // namespace webrtc

#endif  // MODULES_CONGESTION_CONTROLLER_GOOG_CC_ACKNOWLEDGED_BITRATE_ESTIMATOR_H_
