/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <algorithm>

#include "webrtc/modules/remote_bitrate_estimator/test/estimators/tcp.h"

#include "webrtc/base/common.h"
#include "webrtc/modules/bitrate_controller/include/bitrate_controller.h"
#include "webrtc/modules/remote_bitrate_estimator/test/bwe_test_logging.h"
#include "webrtc/modules/rtp_rtcp/include/receive_statistics.h"
#include "webrtc/test/gtest.h"

namespace webrtc {
namespace testing {
namespace bwe {

TcpBweReceiver::TcpBweReceiver(int flow_id)
    : BweReceiver(flow_id),
      last_feedback_ms_(0),
      latest_owd_ms_(0) {
}

TcpBweReceiver::~TcpBweReceiver() {
}

void TcpBweReceiver::ReceivePacket(int64_t arrival_time_ms,
                                   const MediaPacket& media_packet) {
  latest_owd_ms_ = arrival_time_ms - media_packet.sender_timestamp_ms() / 1000;
  acks_.push_back(media_packet.header().sequenceNumber);

  // Log received packet information.
  BweReceiver::ReceivePacket(arrival_time_ms, media_packet);
}

FeedbackPacket* TcpBweReceiver::GetFeedback(int64_t now_ms) {
  int64_t corrected_send_time_ms = now_ms - latest_owd_ms_;
  FeedbackPacket* fb =
      new TcpFeedback(flow_id_, now_ms * 1000, corrected_send_time_ms, acks_);
  last_feedback_ms_ = now_ms;
  acks_.clear();
  return fb;
}

}  // namespace bwe
}  // namespace testing
}  // namespace webrtc
