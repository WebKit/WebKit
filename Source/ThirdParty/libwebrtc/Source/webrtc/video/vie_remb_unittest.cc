/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>
#include <vector>

#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp.h"
#include "webrtc/modules/rtp_rtcp/mocks/mock_rtp_rtcp.h"
#include "webrtc/modules/utility/include/mock/mock_process_thread.h"
#include "webrtc/test/gmock.h"
#include "webrtc/test/gtest.h"
#include "webrtc/video/vie_remb.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::NiceMock;
using ::testing::Return;

namespace webrtc {

class ViERembTest : public ::testing::Test {
 public:
  ViERembTest() : fake_clock_(12345) {}

 protected:
  virtual void SetUp() {
    process_thread_.reset(new NiceMock<MockProcessThread>);
    vie_remb_.reset(new VieRemb(&fake_clock_));
  }
  SimulatedClock fake_clock_;
  std::unique_ptr<MockProcessThread> process_thread_;
  std::unique_ptr<VieRemb> vie_remb_;
};

TEST_F(ViERembTest, OneModuleTestForSendingRemb) {
  MockRtpRtcp rtp;
  vie_remb_->AddReceiveChannel(&rtp);
  vie_remb_->AddRembSender(&rtp);

  const uint32_t bitrate_estimate = 456;
  uint32_t ssrc = 1234;
  std::vector<uint32_t> ssrcs(&ssrc, &ssrc + 1);

  vie_remb_->OnReceiveBitrateChanged(ssrcs, bitrate_estimate);

  fake_clock_.AdvanceTimeMilliseconds(1000);
  EXPECT_CALL(rtp, SetREMBData(bitrate_estimate, ssrcs))
      .Times(1);
  vie_remb_->OnReceiveBitrateChanged(ssrcs, bitrate_estimate);

  // Lower bitrate to send another REMB packet.
  EXPECT_CALL(rtp, SetREMBData(bitrate_estimate - 100, ssrcs))
        .Times(1);
  vie_remb_->OnReceiveBitrateChanged(ssrcs, bitrate_estimate - 100);

  vie_remb_->RemoveReceiveChannel(&rtp);
  vie_remb_->RemoveRembSender(&rtp);
}

TEST_F(ViERembTest, LowerEstimateToSendRemb) {
  MockRtpRtcp rtp;
  vie_remb_->AddReceiveChannel(&rtp);
  vie_remb_->AddRembSender(&rtp);

  uint32_t bitrate_estimate = 456;
  uint32_t ssrc = 1234;
  std::vector<uint32_t> ssrcs(&ssrc, &ssrc + 1);

  vie_remb_->OnReceiveBitrateChanged(ssrcs, bitrate_estimate);
  // Call OnReceiveBitrateChanged twice to get a first estimate.
  fake_clock_.AdvanceTimeMilliseconds(1000);
  EXPECT_CALL(rtp, SetREMBData(bitrate_estimate, ssrcs))
        .Times(1);
  vie_remb_->OnReceiveBitrateChanged(ssrcs, bitrate_estimate);

  // Lower the estimate with more than 3% to trigger a call to SetREMBData right
  // away.
  bitrate_estimate = bitrate_estimate - 100;
  EXPECT_CALL(rtp, SetREMBData(bitrate_estimate, ssrcs))
      .Times(1);
  vie_remb_->OnReceiveBitrateChanged(ssrcs, bitrate_estimate);
}

TEST_F(ViERembTest, VerifyIncreasingAndDecreasing) {
  MockRtpRtcp rtp_0;
  MockRtpRtcp rtp_1;
  vie_remb_->AddReceiveChannel(&rtp_0);
  vie_remb_->AddRembSender(&rtp_0);
  vie_remb_->AddReceiveChannel(&rtp_1);

  uint32_t bitrate_estimate[] = {456, 789};
  uint32_t ssrc[] = {1234, 5678};
  std::vector<uint32_t> ssrcs(ssrc, ssrc + sizeof(ssrc) / sizeof(ssrc[0]));

  vie_remb_->OnReceiveBitrateChanged(ssrcs, bitrate_estimate[0]);

  // Call OnReceiveBitrateChanged twice to get a first estimate.
  EXPECT_CALL(rtp_0, SetREMBData(bitrate_estimate[0], ssrcs))
        .Times(1);
  fake_clock_.AdvanceTimeMilliseconds(1000);
  vie_remb_->OnReceiveBitrateChanged(ssrcs, bitrate_estimate[0]);

  vie_remb_->OnReceiveBitrateChanged(ssrcs, bitrate_estimate[1] + 100);

  // Lower the estimate to trigger a callback.
  EXPECT_CALL(rtp_0, SetREMBData(bitrate_estimate[1], ssrcs))
      .Times(1);
  vie_remb_->OnReceiveBitrateChanged(ssrcs, bitrate_estimate[1]);

  vie_remb_->RemoveReceiveChannel(&rtp_0);
  vie_remb_->RemoveRembSender(&rtp_0);
  vie_remb_->RemoveReceiveChannel(&rtp_1);
}

TEST_F(ViERembTest, NoRembForIncreasedBitrate) {
  MockRtpRtcp rtp_0;
  MockRtpRtcp rtp_1;
  vie_remb_->AddReceiveChannel(&rtp_0);
  vie_remb_->AddRembSender(&rtp_0);
  vie_remb_->AddReceiveChannel(&rtp_1);

  uint32_t bitrate_estimate = 456;
  uint32_t ssrc[] = {1234, 5678};
  std::vector<uint32_t> ssrcs(ssrc, ssrc + sizeof(ssrc) / sizeof(ssrc[0]));

  vie_remb_->OnReceiveBitrateChanged(ssrcs, bitrate_estimate);
  // Call OnReceiveBitrateChanged twice to get a first estimate.
  fake_clock_.AdvanceTimeMilliseconds(1000);
  EXPECT_CALL(rtp_0, SetREMBData(bitrate_estimate, ssrcs))
      .Times(1);
  vie_remb_->OnReceiveBitrateChanged(ssrcs, bitrate_estimate);

  // Increased estimate shouldn't trigger a callback right away.
  EXPECT_CALL(rtp_0, SetREMBData(_, _))
      .Times(0);
  vie_remb_->OnReceiveBitrateChanged(ssrcs, bitrate_estimate + 1);

  // Decreasing the estimate less than 3% shouldn't trigger a new callback.
  EXPECT_CALL(rtp_0, SetREMBData(_, _))
      .Times(0);
  int lower_estimate = bitrate_estimate * 98 / 100;
  vie_remb_->OnReceiveBitrateChanged(ssrcs, lower_estimate);

  vie_remb_->RemoveReceiveChannel(&rtp_1);
  vie_remb_->RemoveReceiveChannel(&rtp_0);
  vie_remb_->RemoveRembSender(&rtp_0);
}

TEST_F(ViERembTest, ChangeSendRtpModule) {
  MockRtpRtcp rtp_0;
  MockRtpRtcp rtp_1;
  vie_remb_->AddReceiveChannel(&rtp_0);
  vie_remb_->AddRembSender(&rtp_0);
  vie_remb_->AddReceiveChannel(&rtp_1);

  uint32_t bitrate_estimate = 456;
  uint32_t ssrc[] = {1234, 5678};
  std::vector<uint32_t> ssrcs(ssrc, ssrc + sizeof(ssrc) / sizeof(ssrc[0]));

  vie_remb_->OnReceiveBitrateChanged(ssrcs, bitrate_estimate);
  // Call OnReceiveBitrateChanged twice to get a first estimate.
  fake_clock_.AdvanceTimeMilliseconds(1000);
  EXPECT_CALL(rtp_0, SetREMBData(bitrate_estimate, ssrcs))
      .Times(1);
  vie_remb_->OnReceiveBitrateChanged(ssrcs, bitrate_estimate);

  // Decrease estimate to trigger a REMB.
  bitrate_estimate = bitrate_estimate - 100;
  EXPECT_CALL(rtp_0, SetREMBData(bitrate_estimate, ssrcs))
      .Times(1);
  vie_remb_->OnReceiveBitrateChanged(ssrcs, bitrate_estimate);

  // Remove the sending module, add it again -> should get remb on the second
  // module.
  vie_remb_->RemoveRembSender(&rtp_0);
  vie_remb_->AddRembSender(&rtp_1);
  vie_remb_->OnReceiveBitrateChanged(ssrcs, bitrate_estimate);

  bitrate_estimate = bitrate_estimate - 100;
  EXPECT_CALL(rtp_1, SetREMBData(bitrate_estimate, ssrcs))
        .Times(1);
  vie_remb_->OnReceiveBitrateChanged(ssrcs, bitrate_estimate);

  vie_remb_->RemoveReceiveChannel(&rtp_0);
  vie_remb_->RemoveReceiveChannel(&rtp_1);
}

TEST_F(ViERembTest, OnlyOneRembForDoubleProcess) {
  MockRtpRtcp rtp;
  uint32_t bitrate_estimate = 456;
  uint32_t ssrc = 1234;
  std::vector<uint32_t> ssrcs(&ssrc, &ssrc + 1);

  vie_remb_->AddReceiveChannel(&rtp);
  vie_remb_->AddRembSender(&rtp);
  vie_remb_->OnReceiveBitrateChanged(ssrcs, bitrate_estimate);
  // Call OnReceiveBitrateChanged twice to get a first estimate.
  fake_clock_.AdvanceTimeMilliseconds(1000);
  EXPECT_CALL(rtp, SetREMBData(_, _))
        .Times(1);
  vie_remb_->OnReceiveBitrateChanged(ssrcs, bitrate_estimate);

  // Lower the estimate, should trigger a call to SetREMBData right away.
  bitrate_estimate = bitrate_estimate - 100;
  EXPECT_CALL(rtp, SetREMBData(bitrate_estimate, ssrcs))
      .Times(1);
  vie_remb_->OnReceiveBitrateChanged(ssrcs, bitrate_estimate);

  // Call OnReceiveBitrateChanged again, this should not trigger a new callback.
  EXPECT_CALL(rtp, SetREMBData(_, _))
      .Times(0);
  vie_remb_->OnReceiveBitrateChanged(ssrcs, bitrate_estimate);
  vie_remb_->RemoveReceiveChannel(&rtp);
  vie_remb_->RemoveRembSender(&rtp);
}

// Only register receiving modules and make sure we fallback to trigger a REMB
// packet on this one.
TEST_F(ViERembTest, NoSendingRtpModule) {
  MockRtpRtcp rtp;
  vie_remb_->AddReceiveChannel(&rtp);

  uint32_t bitrate_estimate = 456;
  uint32_t ssrc = 1234;
  std::vector<uint32_t> ssrcs(&ssrc, &ssrc + 1);

  vie_remb_->OnReceiveBitrateChanged(ssrcs, bitrate_estimate);

  // Call OnReceiveBitrateChanged twice to get a first estimate.
  fake_clock_.AdvanceTimeMilliseconds(1000);
  EXPECT_CALL(rtp, SetREMBData(_, _))
      .Times(1);
  vie_remb_->OnReceiveBitrateChanged(ssrcs, bitrate_estimate);

  // Lower the estimate to trigger a new packet REMB packet.
  bitrate_estimate = bitrate_estimate - 100;
  EXPECT_CALL(rtp, SetREMBData(_, _))
      .Times(1);
  vie_remb_->OnReceiveBitrateChanged(ssrcs, bitrate_estimate);
}

}  // namespace webrtc
