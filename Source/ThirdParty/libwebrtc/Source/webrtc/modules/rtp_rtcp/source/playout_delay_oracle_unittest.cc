/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/playout_delay_oracle.h"

#include "rtc_base/logging.h"
#include "test/gtest.h"

namespace webrtc {

namespace {
constexpr int kSequenceNumber = 100;
constexpr int kMinPlayoutDelay = 0;
constexpr int kMaxPlayoutDelay = 150;
}  // namespace

TEST(PlayoutDelayOracleTest, DisabledByDefault) {
  PlayoutDelayOracle playout_delay_oracle;
  EXPECT_FALSE(playout_delay_oracle.PlayoutDelayToSend({-1, -1}));
}

TEST(PlayoutDelayOracleTest, SendPlayoutDelayUntilSeqNumberExceeds) {
  PlayoutDelayOracle playout_delay_oracle;
  PlayoutDelay playout_delay = {kMinPlayoutDelay, kMaxPlayoutDelay};
  playout_delay_oracle.OnSentPacket(kSequenceNumber, playout_delay);
  absl::optional<PlayoutDelay> delay_to_send =
      playout_delay_oracle.PlayoutDelayToSend({-1, -1});
  ASSERT_TRUE(delay_to_send.has_value());
  EXPECT_EQ(kMinPlayoutDelay, delay_to_send->min_ms);
  EXPECT_EQ(kMaxPlayoutDelay, delay_to_send->max_ms);

  // Oracle indicates playout delay should be sent if highest sequence number
  // acked is lower than the sequence number of the first packet containing
  // playout delay.
  playout_delay_oracle.OnReceivedAck(kSequenceNumber - 1);
  EXPECT_TRUE(playout_delay_oracle.PlayoutDelayToSend({-1, -1}));

  // Oracle indicates playout delay should not be sent if sequence number
  // acked on a matching ssrc indicates the receiver has received the playout
  // delay values.
  playout_delay_oracle.OnReceivedAck(kSequenceNumber + 1);
  EXPECT_FALSE(playout_delay_oracle.PlayoutDelayToSend({-1, -1}));
}

}  // namespace webrtc
