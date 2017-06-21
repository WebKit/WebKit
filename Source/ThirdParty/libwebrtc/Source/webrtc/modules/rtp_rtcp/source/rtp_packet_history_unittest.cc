/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/rtp_rtcp/source/rtp_packet_history.h"

#include <memory>
#include <utility>

#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_packet_to_send.h"
#include "webrtc/system_wrappers/include/clock.h"
#include "webrtc/test/gtest.h"
#include "webrtc/typedefs.h"

namespace webrtc {

class RtpPacketHistoryTest : public ::testing::Test {
 protected:
  static constexpr uint16_t kSeqNum = 88;

  RtpPacketHistoryTest() : fake_clock_(123456), hist_(&fake_clock_) {}

  SimulatedClock fake_clock_;
  RtpPacketHistory hist_;

  std::unique_ptr<RtpPacketToSend> CreateRtpPacket(uint16_t seq_num) {
    // Payload, ssrc, timestamp and extensions are irrelevant for this tests.
    std::unique_ptr<RtpPacketToSend> packet(new RtpPacketToSend(nullptr));
    packet->SetSequenceNumber(seq_num);
    packet->set_capture_time_ms(fake_clock_.TimeInMilliseconds());
    return packet;
  }
};

TEST_F(RtpPacketHistoryTest, SetStoreStatus) {
  EXPECT_FALSE(hist_.StorePackets());
  hist_.SetStorePacketsStatus(true, 10);
  EXPECT_TRUE(hist_.StorePackets());
  hist_.SetStorePacketsStatus(false, 0);
  EXPECT_FALSE(hist_.StorePackets());
}

TEST_F(RtpPacketHistoryTest, NoStoreStatus) {
  EXPECT_FALSE(hist_.StorePackets());
  std::unique_ptr<RtpPacketToSend> packet = CreateRtpPacket(kSeqNum);
  hist_.PutRtpPacket(std::move(packet), kAllowRetransmission, false);
  // Packet should not be stored.
  EXPECT_FALSE(hist_.GetPacketAndSetSendTime(kSeqNum, 0, false));
}

TEST_F(RtpPacketHistoryTest, GetRtpPacket_NotStored) {
  hist_.SetStorePacketsStatus(true, 10);
  EXPECT_FALSE(hist_.GetPacketAndSetSendTime(0, 0, false));
}

TEST_F(RtpPacketHistoryTest, PutRtpPacket) {
  hist_.SetStorePacketsStatus(true, 10);
  std::unique_ptr<RtpPacketToSend> packet = CreateRtpPacket(kSeqNum);

  EXPECT_FALSE(hist_.HasRtpPacket(kSeqNum));
  hist_.PutRtpPacket(std::move(packet), kAllowRetransmission, false);
  EXPECT_TRUE(hist_.HasRtpPacket(kSeqNum));
}

TEST_F(RtpPacketHistoryTest, GetRtpPacket) {
  hist_.SetStorePacketsStatus(true, 10);
  int64_t capture_time_ms = 1;
  std::unique_ptr<RtpPacketToSend> packet = CreateRtpPacket(kSeqNum);
  packet->set_capture_time_ms(capture_time_ms);
  rtc::CopyOnWriteBuffer buffer = packet->Buffer();
  hist_.PutRtpPacket(std::move(packet), kAllowRetransmission, false);

  std::unique_ptr<RtpPacketToSend> packet_out =
      hist_.GetPacketAndSetSendTime(kSeqNum, 0, false);
  EXPECT_TRUE(packet_out);
  EXPECT_EQ(buffer, packet_out->Buffer());
  EXPECT_EQ(capture_time_ms, packet_out->capture_time_ms());
}

TEST_F(RtpPacketHistoryTest, NoCaptureTime) {
  hist_.SetStorePacketsStatus(true, 10);
  fake_clock_.AdvanceTimeMilliseconds(1);
  int64_t capture_time_ms = fake_clock_.TimeInMilliseconds();
  std::unique_ptr<RtpPacketToSend> packet = CreateRtpPacket(kSeqNum);
  packet->set_capture_time_ms(-1);
  rtc::CopyOnWriteBuffer buffer = packet->Buffer();
  hist_.PutRtpPacket(std::move(packet), kAllowRetransmission, false);

  std::unique_ptr<RtpPacketToSend> packet_out =
      hist_.GetPacketAndSetSendTime(kSeqNum, 0, false);
  EXPECT_TRUE(packet_out);
  EXPECT_EQ(buffer, packet_out->Buffer());
  EXPECT_EQ(capture_time_ms, packet_out->capture_time_ms());
}

TEST_F(RtpPacketHistoryTest, DontRetransmit) {
  hist_.SetStorePacketsStatus(true, 10);
  int64_t capture_time_ms = fake_clock_.TimeInMilliseconds();
  std::unique_ptr<RtpPacketToSend> packet = CreateRtpPacket(kSeqNum);
  rtc::CopyOnWriteBuffer buffer = packet->Buffer();
  hist_.PutRtpPacket(std::move(packet), kDontRetransmit, false);

  std::unique_ptr<RtpPacketToSend> packet_out;
  packet_out = hist_.GetPacketAndSetSendTime(kSeqNum, 0, true);
  EXPECT_FALSE(packet_out);

  packet_out = hist_.GetPacketAndSetSendTime(kSeqNum, 0, false);
  EXPECT_TRUE(packet_out);

  EXPECT_EQ(buffer.size(), packet_out->size());
  EXPECT_EQ(capture_time_ms, packet_out->capture_time_ms());
}

TEST_F(RtpPacketHistoryTest, MinResendTime) {
  static const int64_t kMinRetransmitIntervalMs = 100;

  hist_.SetStorePacketsStatus(true, 10);
  int64_t capture_time_ms = fake_clock_.TimeInMilliseconds();
  std::unique_ptr<RtpPacketToSend> packet = CreateRtpPacket(kSeqNum);
  size_t len = packet->size();
  hist_.PutRtpPacket(std::move(packet), kAllowRetransmission, false);

  // First transmission: TimeToSendPacket() call from pacer.
  EXPECT_TRUE(hist_.GetPacketAndSetSendTime(kSeqNum, 0, false));

  fake_clock_.AdvanceTimeMilliseconds(kMinRetransmitIntervalMs);
  // Time has elapsed.
  std::unique_ptr<RtpPacketToSend> packet_out =
      hist_.GetPacketAndSetSendTime(kSeqNum, kMinRetransmitIntervalMs, true);
  EXPECT_TRUE(packet_out);
  EXPECT_EQ(len, packet_out->size());
  EXPECT_EQ(capture_time_ms, packet_out->capture_time_ms());

  fake_clock_.AdvanceTimeMilliseconds(kMinRetransmitIntervalMs - 1);
  // Time has not elapsed. Packet should be found, but no bytes copied.
  EXPECT_TRUE(hist_.HasRtpPacket(kSeqNum));
  EXPECT_FALSE(
      hist_.GetPacketAndSetSendTime(kSeqNum, kMinRetransmitIntervalMs, true));
}

TEST_F(RtpPacketHistoryTest, EarlyFirstResend) {
  static const int64_t kMinRetransmitIntervalMs = 100;

  hist_.SetStorePacketsStatus(true, 10);
  int64_t capture_time_ms = fake_clock_.TimeInMilliseconds();
  std::unique_ptr<RtpPacketToSend> packet = CreateRtpPacket(kSeqNum);
  rtc::CopyOnWriteBuffer buffer = packet->Buffer();
  hist_.PutRtpPacket(std::move(packet), kAllowRetransmission, false);

  // First transmission: TimeToSendPacket() call from pacer.
  EXPECT_TRUE(hist_.GetPacketAndSetSendTime(kSeqNum, 0, false));

  fake_clock_.AdvanceTimeMilliseconds(kMinRetransmitIntervalMs - 1);
  // Time has not elapsed, but this is the first retransmission request so
  // allow anyway.
  std::unique_ptr<RtpPacketToSend> packet_out =
      hist_.GetPacketAndSetSendTime(kSeqNum, kMinRetransmitIntervalMs, true);
  EXPECT_TRUE(packet_out);
  EXPECT_EQ(buffer, packet_out->Buffer());
  EXPECT_EQ(capture_time_ms, packet_out->capture_time_ms());

  fake_clock_.AdvanceTimeMilliseconds(kMinRetransmitIntervalMs - 1);
  // Time has not elapsed. Packet should be found, but no bytes copied.
  EXPECT_TRUE(hist_.HasRtpPacket(kSeqNum));
  EXPECT_FALSE(
      hist_.GetPacketAndSetSendTime(kSeqNum, kMinRetransmitIntervalMs, true));
}

TEST_F(RtpPacketHistoryTest, DynamicExpansion) {
  hist_.SetStorePacketsStatus(true, 10);

  // Add 4 packets, and then send them.
  for (int i = 0; i < 4; ++i) {
    std::unique_ptr<RtpPacketToSend> packet = CreateRtpPacket(kSeqNum + i);
    hist_.PutRtpPacket(std::move(packet), kAllowRetransmission, false);
  }
  for (int i = 0; i < 4; ++i) {
    EXPECT_TRUE(hist_.GetPacketAndSetSendTime(kSeqNum + i, 100, false));
  }
  fake_clock_.AdvanceTimeMilliseconds(33);

  // Add 16 packets, and then send them. History should expand to make this
  // work.
  for (int i = 4; i < 20; ++i) {
    std::unique_ptr<RtpPacketToSend> packet = CreateRtpPacket(kSeqNum + i);
    hist_.PutRtpPacket(std::move(packet), kAllowRetransmission, false);
  }
  for (int i = 4; i < 20; ++i) {
    EXPECT_TRUE(hist_.GetPacketAndSetSendTime(kSeqNum + i, 100, false));
  }

  fake_clock_.AdvanceTimeMilliseconds(100);

  // Retransmit last 16 packets.
  for (int i = 4; i < 20; ++i) {
    EXPECT_TRUE(hist_.GetPacketAndSetSendTime(kSeqNum + i, 100, false));
  }
}

TEST_F(RtpPacketHistoryTest, FullExpansion) {
  static const int kSendSidePacketHistorySize = 600;
  hist_.SetStorePacketsStatus(true, kSendSidePacketHistorySize);
  for (size_t i = 0; i < RtpPacketHistory::kMaxCapacity + 1; ++i) {
    std::unique_ptr<RtpPacketToSend> packet = CreateRtpPacket(kSeqNum + i);
    hist_.PutRtpPacket(std::move(packet), kAllowRetransmission, false);
  }

  fake_clock_.AdvanceTimeMilliseconds(100);

  // Retransmit all packets currently in buffer.
  for (size_t i = 1; i < RtpPacketHistory::kMaxCapacity + 1; ++i) {
    EXPECT_TRUE(hist_.GetPacketAndSetSendTime(kSeqNum + i, 100, false));
  }
}

}  // namespace webrtc
