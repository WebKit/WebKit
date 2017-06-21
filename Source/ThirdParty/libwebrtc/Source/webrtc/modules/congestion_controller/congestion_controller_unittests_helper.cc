/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/congestion_controller/congestion_controller_unittests_helper.h"

#include "webrtc/base/checks.h"
#include "webrtc/test/gtest.h"

namespace webrtc {
void ComparePacketFeedbackVectors(const std::vector<PacketFeedback>& truth,
                                  const std::vector<PacketFeedback>& input) {
  ASSERT_EQ(truth.size(), input.size());
  size_t len = truth.size();
  // truth contains the input data for the test, and input is what will be
  // sent to the bandwidth estimator. truth.arrival_tims_ms is used to
  // populate the transport feedback messages. As these times may be changed
  // (because of resolution limits in the packets, and because of the time
  // base adjustment performed by the TransportFeedbackAdapter at the first
  // packet, the truth[x].arrival_time and input[x].arrival_time may not be
  // equal. However, the difference must be the same for all x.
  int64_t arrival_time_delta =
      truth[0].arrival_time_ms - input[0].arrival_time_ms;
  for (size_t i = 0; i < len; ++i) {
    RTC_CHECK(truth[i].arrival_time_ms != PacketFeedback::kNotReceived);
    if (input[i].arrival_time_ms != PacketFeedback::kNotReceived) {
      EXPECT_EQ(truth[i].arrival_time_ms,
                input[i].arrival_time_ms + arrival_time_delta);
    }
    EXPECT_EQ(truth[i].send_time_ms, input[i].send_time_ms);
    EXPECT_EQ(truth[i].sequence_number, input[i].sequence_number);
    EXPECT_EQ(truth[i].payload_size, input[i].payload_size);
    EXPECT_EQ(truth[i].pacing_info, input[i].pacing_info);
  }
}
}  // namespace webrtc
