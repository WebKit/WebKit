/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/rtpmediautils.h"
#include "test/gtest.h"

namespace webrtc {

using ::testing::Values;

class EnumerateAllDirectionsTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<RtpTransceiverDirection> {};

// Test that converting the direction to send/recv and back again results in the
// same direction.
TEST_P(EnumerateAllDirectionsTest, TestIdentity) {
  RtpTransceiverDirection direction = GetParam();

  bool send = RtpTransceiverDirectionHasSend(direction);
  bool recv = RtpTransceiverDirectionHasRecv(direction);

  EXPECT_EQ(direction, RtpTransceiverDirectionFromSendRecv(send, recv));
}

// Test that reversing the direction is equivalent to swapping send/recv.
TEST_P(EnumerateAllDirectionsTest, TestReversedSwapped) {
  RtpTransceiverDirection direction = GetParam();

  bool send = RtpTransceiverDirectionHasSend(direction);
  bool recv = RtpTransceiverDirectionHasRecv(direction);

  EXPECT_EQ(RtpTransceiverDirectionFromSendRecv(recv, send),
            RtpTransceiverDirectionReversed(direction));
}

// Test that reversing the direction twice results in the same direction.
TEST_P(EnumerateAllDirectionsTest, TestReversedIdentity) {
  RtpTransceiverDirection direction = GetParam();

  EXPECT_EQ(direction, RtpTransceiverDirectionReversed(
                           RtpTransceiverDirectionReversed(direction)));
}

INSTANTIATE_TEST_CASE_P(RtpTransceiverDirectionTest,
                        EnumerateAllDirectionsTest,
                        Values(RtpTransceiverDirection::kSendRecv,
                               RtpTransceiverDirection::kSendOnly,
                               RtpTransceiverDirection::kRecvOnly,
                               RtpTransceiverDirection::kInactive));

}  // namespace webrtc
