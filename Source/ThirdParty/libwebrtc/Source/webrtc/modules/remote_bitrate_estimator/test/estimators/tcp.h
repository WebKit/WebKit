/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_REMOTE_BITRATE_ESTIMATOR_TEST_ESTIMATORS_TCP_H_
#define MODULES_REMOTE_BITRATE_ESTIMATOR_TEST_ESTIMATORS_TCP_H_

#include <vector>

#include "modules/remote_bitrate_estimator/test/bwe.h"

namespace webrtc {
namespace testing {
namespace bwe {
class TcpBweReceiver : public BweReceiver {
 public:
  explicit TcpBweReceiver(int flow_id);
  virtual ~TcpBweReceiver();

  void ReceivePacket(int64_t arrival_time_ms,
                     const MediaPacket& media_packet) override;
  FeedbackPacket* GetFeedback(int64_t now_ms) override;

 private:
  int64_t last_feedback_ms_;
  int64_t latest_owd_ms_;
  std::vector<uint16_t> acks_;
};
}  // namespace bwe
}  // namespace testing
}  // namespace webrtc
#endif  // MODULES_REMOTE_BITRATE_ESTIMATOR_TEST_ESTIMATORS_TCP_H_
