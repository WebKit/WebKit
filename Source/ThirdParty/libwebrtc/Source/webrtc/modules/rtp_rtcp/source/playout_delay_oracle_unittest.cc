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
constexpr int kSsrc = 100;
constexpr int kSequenceNumber = 100;
constexpr int kMinPlayoutDelay = 0;
constexpr int kMaxPlayoutDelay = 150;
}  // namespace

class PlayoutDelayOracleTest : public ::testing::Test {
 protected:
  void ReportRTCPFeedback(int ssrc, int seq_num) {
    RTCPReportBlock report_block;
    report_block.source_ssrc = ssrc;
    report_block.extended_highest_sequence_number = seq_num;
    report_blocks_.push_back(report_block);
    playout_delay_oracle_.OnReceivedRtcpReportBlocks(report_blocks_);
  }

  ReportBlockList report_blocks_;
  PlayoutDelayOracle playout_delay_oracle_;
};

TEST_F(PlayoutDelayOracleTest, DisabledByDefault) {
  EXPECT_FALSE(playout_delay_oracle_.send_playout_delay());
  EXPECT_EQ(-1, playout_delay_oracle_.playout_delay().min_ms);
  EXPECT_EQ(-1, playout_delay_oracle_.playout_delay().max_ms);
}

TEST_F(PlayoutDelayOracleTest, SendPlayoutDelayUntilSeqNumberExceeds) {
  PlayoutDelay playout_delay = {kMinPlayoutDelay, kMaxPlayoutDelay};
  playout_delay_oracle_.UpdateRequest(kSsrc, playout_delay, kSequenceNumber);
  EXPECT_TRUE(playout_delay_oracle_.send_playout_delay());
  EXPECT_EQ(kMinPlayoutDelay, playout_delay_oracle_.playout_delay().min_ms);
  EXPECT_EQ(kMaxPlayoutDelay, playout_delay_oracle_.playout_delay().max_ms);

  // Oracle indicates playout delay should be sent if highest sequence number
  // acked is lower than the sequence number of the first packet containing
  // playout delay.
  ReportRTCPFeedback(kSsrc, kSequenceNumber - 1);
  EXPECT_TRUE(playout_delay_oracle_.send_playout_delay());

  // An invalid ssrc feedback report is dropped by the oracle.
  ReportRTCPFeedback(kSsrc + 1, kSequenceNumber + 1);
  EXPECT_TRUE(playout_delay_oracle_.send_playout_delay());

  // Oracle indicates playout delay should not be sent if sequence number
  // acked on a matching ssrc indicates the receiver has received the playout
  // delay values.
  ReportRTCPFeedback(kSsrc, kSequenceNumber + 1);
  EXPECT_FALSE(playout_delay_oracle_.send_playout_delay());
}

}  // namespace webrtc
