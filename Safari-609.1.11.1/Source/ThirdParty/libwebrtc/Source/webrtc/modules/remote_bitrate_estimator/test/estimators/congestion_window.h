/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

#ifndef MODULES_REMOTE_BITRATE_ESTIMATOR_TEST_ESTIMATORS_CONGESTION_WINDOW_H_
#define MODULES_REMOTE_BITRATE_ESTIMATOR_TEST_ESTIMATORS_CONGESTION_WINDOW_H_

#include "modules/remote_bitrate_estimator/test/estimators/bbr.h"

#include "absl/types/optional.h"

namespace webrtc {
namespace testing {
namespace bwe {
class CongestionWindow {
 public:
  CongestionWindow();
  ~CongestionWindow();
  int GetCongestionWindow(BbrBweSender::Mode mode,
                          int64_t bandwidth_estimate,
                          absl::optional<int64_t> min_rtt,
                          float gain);
  int GetTargetCongestionWindow(int64_t bandwidth_estimate,
                                absl::optional<int64_t> min_rtt,
                                float gain);
  // Packet sent from sender, meaning it is inflight until we receive it and we
  // should add packet's size to data_inflight.
  void PacketSent(size_t sent_packet_size);

  // Ack was received by sender, meaning packet is no longer inflight.
  void AckReceived(size_t received_packet_size);

  size_t data_inflight() { return data_inflight_bytes_; }

 private:
  size_t data_inflight_bytes_;
};
}  // namespace bwe
}  // namespace testing
}  // namespace webrtc

#endif  // MODULES_REMOTE_BITRATE_ESTIMATOR_TEST_ESTIMATORS_CONGESTION_WINDOW_H_
