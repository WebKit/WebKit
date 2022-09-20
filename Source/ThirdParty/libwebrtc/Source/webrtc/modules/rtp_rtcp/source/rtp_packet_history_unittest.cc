/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtp_packet_history.h"

#include <memory>
#include <utility>

#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/rtp_packet_to_send.h"
#include "system_wrappers/include/clock.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {
// Set a high sequence number so we'll suffer a wrap-around.
constexpr uint16_t kStartSeqNum = 65534u;

// Utility method for truncating sequence numbers to uint16.
uint16_t To16u(size_t sequence_number) {
  return static_cast<uint16_t>(sequence_number & 0xFFFF);
}
}  // namespace

using StorageMode = RtpPacketHistory::StorageMode;

class RtpPacketHistoryTest : public ::testing::TestWithParam<bool> {
 protected:
  RtpPacketHistoryTest()
      : fake_clock_(123456),
        hist_(&fake_clock_, /*enable_padding_prio=*/GetParam()) {}

  SimulatedClock fake_clock_;
  RtpPacketHistory hist_;

  std::unique_ptr<RtpPacketToSend> CreateRtpPacket(uint16_t seq_num) {
    // Payload, ssrc, timestamp and extensions are irrelevant for this tests.
    std::unique_ptr<RtpPacketToSend> packet(new RtpPacketToSend(nullptr));
    packet->SetSequenceNumber(seq_num);
    packet->set_capture_time(fake_clock_.CurrentTime());
    packet->set_allow_retransmission(true);
    return packet;
  }
};

TEST_P(RtpPacketHistoryTest, SetStoreStatus) {
  EXPECT_EQ(StorageMode::kDisabled, hist_.GetStorageMode());
  hist_.SetStorePacketsStatus(StorageMode::kStoreAndCull, 10);
  EXPECT_EQ(StorageMode::kStoreAndCull, hist_.GetStorageMode());
  hist_.SetStorePacketsStatus(StorageMode::kStoreAndCull, 10);
  EXPECT_EQ(StorageMode::kStoreAndCull, hist_.GetStorageMode());
  hist_.SetStorePacketsStatus(StorageMode::kDisabled, 0);
  EXPECT_EQ(StorageMode::kDisabled, hist_.GetStorageMode());
}

TEST_P(RtpPacketHistoryTest, ClearsHistoryAfterSetStoreStatus) {
  hist_.SetStorePacketsStatus(StorageMode::kStoreAndCull, 10);
  hist_.PutRtpPacket(CreateRtpPacket(kStartSeqNum),
                     /*send_time=*/fake_clock_.CurrentTime());
  EXPECT_TRUE(hist_.GetPacketState(kStartSeqNum));

  // Changing store status, even to the current one, will clear the history.
  hist_.SetStorePacketsStatus(StorageMode::kStoreAndCull, 10);
  EXPECT_FALSE(hist_.GetPacketState(kStartSeqNum));
}

TEST_P(RtpPacketHistoryTest, StartSeqResetAfterReset) {
  hist_.SetStorePacketsStatus(StorageMode::kStoreAndCull, 10);
  hist_.PutRtpPacket(CreateRtpPacket(kStartSeqNum),
                     /*send_time=*/fake_clock_.CurrentTime());
  // Mark packet as pending so it won't be removed.
  EXPECT_TRUE(hist_.GetPacketAndMarkAsPending(kStartSeqNum));

  // Changing store status, to clear the history.
  hist_.SetStorePacketsStatus(StorageMode::kStoreAndCull, 10);
  EXPECT_FALSE(hist_.GetPacketState(kStartSeqNum));

  // Add a new packet.
  hist_.PutRtpPacket(CreateRtpPacket(To16u(kStartSeqNum + 1)),
                     /*send_time=*/fake_clock_.CurrentTime());
  EXPECT_TRUE(hist_.GetPacketAndMarkAsPending(To16u(kStartSeqNum + 1)));

  // Advance time past where packet expires.
  fake_clock_.AdvanceTime(RtpPacketHistory::kPacketCullingDelayFactor *
                          RtpPacketHistory::kMinPacketDuration);

  // Add one more packet and verify no state left from packet before reset.
  hist_.PutRtpPacket(CreateRtpPacket(To16u(kStartSeqNum + 2)),
                     /*send_time=*/fake_clock_.CurrentTime());
  EXPECT_FALSE(hist_.GetPacketState(kStartSeqNum));
  EXPECT_TRUE(hist_.GetPacketState(To16u(kStartSeqNum + 1)));
  EXPECT_TRUE(hist_.GetPacketState(To16u(kStartSeqNum + 2)));
}

TEST_P(RtpPacketHistoryTest, NoStoreStatus) {
  EXPECT_EQ(StorageMode::kDisabled, hist_.GetStorageMode());
  std::unique_ptr<RtpPacketToSend> packet = CreateRtpPacket(kStartSeqNum);
  hist_.PutRtpPacket(std::move(packet),
                     /*send_time=*/fake_clock_.CurrentTime());
  // Packet should not be stored.
  EXPECT_FALSE(hist_.GetPacketState(kStartSeqNum));
}

TEST_P(RtpPacketHistoryTest, GetRtpPacket_NotStored) {
  hist_.SetStorePacketsStatus(StorageMode::kStoreAndCull, 10);
  EXPECT_FALSE(hist_.GetPacketState(0));
}

TEST_P(RtpPacketHistoryTest, PutRtpPacket) {
  hist_.SetStorePacketsStatus(StorageMode::kStoreAndCull, 10);
  std::unique_ptr<RtpPacketToSend> packet = CreateRtpPacket(kStartSeqNum);

  EXPECT_FALSE(hist_.GetPacketState(kStartSeqNum));
  hist_.PutRtpPacket(std::move(packet),
                     /*send_time=*/fake_clock_.CurrentTime());
  EXPECT_TRUE(hist_.GetPacketState(kStartSeqNum));
}

TEST_P(RtpPacketHistoryTest, GetRtpPacket) {
  hist_.SetStorePacketsStatus(StorageMode::kStoreAndCull, 10);
  Timestamp capture_time = Timestamp::Millis(1);
  std::unique_ptr<RtpPacketToSend> packet = CreateRtpPacket(kStartSeqNum);
  packet->set_capture_time(capture_time);
  rtc::CopyOnWriteBuffer buffer = packet->Buffer();
  hist_.PutRtpPacket(std::move(packet),
                     /*send_time=*/fake_clock_.CurrentTime());

  std::unique_ptr<RtpPacketToSend> packet_out =
      hist_.GetPacketAndMarkAsPending(kStartSeqNum);
  ASSERT_TRUE(packet_out);
  EXPECT_EQ(buffer, packet_out->Buffer());
  EXPECT_EQ(capture_time, packet_out->capture_time());
}

TEST_P(RtpPacketHistoryTest, MinResendTime) {
  static const TimeDelta kMinRetransmitInterval = TimeDelta::Millis(100);

  hist_.SetStorePacketsStatus(StorageMode::kStoreAndCull, 10);
  hist_.SetRtt(kMinRetransmitInterval);
  Timestamp capture_time = fake_clock_.CurrentTime();
  std::unique_ptr<RtpPacketToSend> packet = CreateRtpPacket(kStartSeqNum);
  size_t len = packet->size();
  hist_.PutRtpPacket(std::move(packet), fake_clock_.CurrentTime());

  // First retransmission - allow early retransmission.
  fake_clock_.AdvanceTimeMilliseconds(1);
  packet = hist_.GetPacketAndMarkAsPending(kStartSeqNum);
  ASSERT_TRUE(packet);
  EXPECT_EQ(len, packet->size());
  EXPECT_EQ(packet->capture_time(), capture_time);
  hist_.MarkPacketAsSent(kStartSeqNum);

  // Second retransmission - advance time to just before retransmission OK.
  fake_clock_.AdvanceTime(kMinRetransmitInterval - TimeDelta::Millis(1));
  EXPECT_FALSE(hist_.GetPacketAndMarkAsPending(kStartSeqNum));

  // Advance time to just after retransmission OK.
  fake_clock_.AdvanceTimeMilliseconds(1);
  EXPECT_TRUE(hist_.GetPacketAndMarkAsPending(kStartSeqNum));
}

TEST_P(RtpPacketHistoryTest, RemovesOldestSentPacketWhenAtMaxSize) {
  const size_t kMaxNumPackets = 10;
  hist_.SetStorePacketsStatus(StorageMode::kStoreAndCull, kMaxNumPackets);

  // History does not allow removing packets within kMinPacketDuration,
  // so in order to test capacity, make sure insertion spans this time.
  const TimeDelta kPacketInterval =
      RtpPacketHistory::kMinPacketDuration / kMaxNumPackets;

  // Add packets until the buffer is full.
  for (size_t i = 0; i < kMaxNumPackets; ++i) {
    std::unique_ptr<RtpPacketToSend> packet =
        CreateRtpPacket(To16u(kStartSeqNum + i));
    // Immediate mark packet as sent.
    hist_.PutRtpPacket(std::move(packet), fake_clock_.CurrentTime());
    fake_clock_.AdvanceTime(kPacketInterval);
  }

  // First packet should still be there.
  EXPECT_TRUE(hist_.GetPacketState(kStartSeqNum));

  // History is full, oldest one should be overwritten.
  std::unique_ptr<RtpPacketToSend> packet =
      CreateRtpPacket(To16u(kStartSeqNum + kMaxNumPackets));
  hist_.PutRtpPacket(std::move(packet), fake_clock_.CurrentTime());

  // Oldest packet should be gone, but packet after than one still present.
  EXPECT_FALSE(hist_.GetPacketState(kStartSeqNum));
  EXPECT_TRUE(hist_.GetPacketState(To16u(kStartSeqNum + 1)));
}

TEST_P(RtpPacketHistoryTest, RemovesOldestPacketWhenAtMaxCapacity) {
  // Tests the absolute upper bound on number of stored packets. Don't allow
  // storing more than this, even if packets have not yet been sent.
  const size_t kMaxNumPackets = RtpPacketHistory::kMaxCapacity;
  hist_.SetStorePacketsStatus(StorageMode::kStoreAndCull,
                              RtpPacketHistory::kMaxCapacity);

  // Add packets until the buffer is full.
  for (size_t i = 0; i < kMaxNumPackets; ++i) {
    std::unique_ptr<RtpPacketToSend> packet =
        CreateRtpPacket(To16u(kStartSeqNum + i));
    hist_.PutRtpPacket(std::move(packet),
                       /*send_time=*/fake_clock_.CurrentTime());
    // Mark packets as pending, preventing it from being removed.
    hist_.GetPacketAndMarkAsPending(To16u(kStartSeqNum + i));
  }

  // First packet should still be there.
  EXPECT_TRUE(hist_.GetPacketState(kStartSeqNum));

  // History is full, oldest one should be overwritten.
  std::unique_ptr<RtpPacketToSend> packet =
      CreateRtpPacket(To16u(kStartSeqNum + kMaxNumPackets));
  hist_.PutRtpPacket(std::move(packet), fake_clock_.CurrentTime());

  // Oldest packet should be gone, but packet after than one still present.
  EXPECT_FALSE(hist_.GetPacketState(kStartSeqNum));
  EXPECT_TRUE(hist_.GetPacketState(To16u(kStartSeqNum + 1)));
}

TEST_P(RtpPacketHistoryTest, RemovesLowestPrioPaddingWhenAtMaxCapacity) {
  if (!GetParam()) {
    // Padding prioritization is off, ignore this test.
    return;
  }

  // Tests the absolute upper bound on number of packets in the prioritized
  // set of potential padding packets.
  const size_t kMaxNumPackets = RtpPacketHistory::kMaxPaddingHistory;
  hist_.SetStorePacketsStatus(StorageMode::kStoreAndCull, kMaxNumPackets * 2);
  hist_.SetRtt(TimeDelta::Millis(1));

  // Add packets until the max is reached, and then yet another one.
  for (size_t i = 0; i < kMaxNumPackets + 1; ++i) {
    std::unique_ptr<RtpPacketToSend> packet =
        CreateRtpPacket(To16u(kStartSeqNum + i));
    // Don't mark packets as sent, preventing them from being removed.
    hist_.PutRtpPacket(std::move(packet), fake_clock_.CurrentTime());
  }

  // Advance time to allow retransmission/padding.
  fake_clock_.AdvanceTimeMilliseconds(1);

  // The oldest packet will be least prioritized and has fallen out of the
  // priority set.
  for (size_t i = kMaxNumPackets - 1; i > 0; --i) {
    auto packet = hist_.GetPayloadPaddingPacket();
    ASSERT_TRUE(packet);
    EXPECT_EQ(packet->SequenceNumber(), To16u(kStartSeqNum + i + 1));
  }

  // Wrap around to newest padding packet again.
  auto packet = hist_.GetPayloadPaddingPacket();
  ASSERT_TRUE(packet);
  EXPECT_EQ(packet->SequenceNumber(), To16u(kStartSeqNum + kMaxNumPackets));
}

TEST_P(RtpPacketHistoryTest, DontRemoveTooRecentlyTransmittedPackets) {
  // Set size to remove old packets as soon as possible.
  hist_.SetStorePacketsStatus(StorageMode::kStoreAndCull, 1);

  // Add a packet, marked as send, and advance time to just before removal time.
  hist_.PutRtpPacket(CreateRtpPacket(kStartSeqNum), fake_clock_.CurrentTime());
  fake_clock_.AdvanceTime(RtpPacketHistory::kMinPacketDuration -
                          TimeDelta::Millis(1));

  // Add a new packet to trigger culling.
  hist_.PutRtpPacket(CreateRtpPacket(To16u(kStartSeqNum + 1)),
                     fake_clock_.CurrentTime());
  // First packet should still be there.
  EXPECT_TRUE(hist_.GetPacketState(kStartSeqNum));

  // Advance time to where packet will be eligible for removal and try again.
  fake_clock_.AdvanceTimeMilliseconds(1);
  hist_.PutRtpPacket(CreateRtpPacket(To16u(kStartSeqNum + 2)),
                     fake_clock_.CurrentTime());
  // First packet should no be gone, but next one still there.
  EXPECT_FALSE(hist_.GetPacketState(kStartSeqNum));
  EXPECT_TRUE(hist_.GetPacketState(To16u(kStartSeqNum + 1)));
}

TEST_P(RtpPacketHistoryTest, DontRemoveTooRecentlyTransmittedPacketsHighRtt) {
  const TimeDelta kRtt = RtpPacketHistory::kMinPacketDuration * 2;
  const TimeDelta kPacketTimeout =
      kRtt * RtpPacketHistory::kMinPacketDurationRtt;

  // Set size to remove old packets as soon as possible.
  hist_.SetStorePacketsStatus(StorageMode::kStoreAndCull, 1);
  hist_.SetRtt(kRtt);

  // Add a packet, marked as send, and advance time to just before removal time.
  hist_.PutRtpPacket(CreateRtpPacket(kStartSeqNum), fake_clock_.CurrentTime());
  fake_clock_.AdvanceTime(kPacketTimeout - TimeDelta::Millis(1));

  // Add a new packet to trigger culling.
  hist_.PutRtpPacket(CreateRtpPacket(To16u(kStartSeqNum + 1)),
                     fake_clock_.CurrentTime());
  // First packet should still be there.
  EXPECT_TRUE(hist_.GetPacketState(kStartSeqNum));

  // Advance time to where packet will be eligible for removal and try again.
  fake_clock_.AdvanceTimeMilliseconds(1);
  hist_.PutRtpPacket(CreateRtpPacket(To16u(kStartSeqNum + 2)),
                     fake_clock_.CurrentTime());
  // First packet should no be gone, but next one still there.
  EXPECT_FALSE(hist_.GetPacketState(kStartSeqNum));
  EXPECT_TRUE(hist_.GetPacketState(To16u(kStartSeqNum + 1)));
}

TEST_P(RtpPacketHistoryTest, RemovesOldWithCulling) {
  const size_t kMaxNumPackets = 10;
  // Enable culling. Even without feedback, this can trigger early removal.
  hist_.SetStorePacketsStatus(StorageMode::kStoreAndCull, kMaxNumPackets);

  hist_.PutRtpPacket(CreateRtpPacket(kStartSeqNum), fake_clock_.CurrentTime());

  TimeDelta kMaxPacketDuration = RtpPacketHistory::kMinPacketDuration *
                                 RtpPacketHistory::kPacketCullingDelayFactor;
  fake_clock_.AdvanceTime(kMaxPacketDuration - TimeDelta::Millis(1));

  // First packet should still be there.
  EXPECT_TRUE(hist_.GetPacketState(kStartSeqNum));

  // Advance to where packet can be culled, even if buffer is not full.
  fake_clock_.AdvanceTimeMilliseconds(1);
  hist_.PutRtpPacket(CreateRtpPacket(To16u(kStartSeqNum + 1)),
                     fake_clock_.CurrentTime());

  EXPECT_FALSE(hist_.GetPacketState(kStartSeqNum));
}

TEST_P(RtpPacketHistoryTest, RemovesOldWithCullingHighRtt) {
  const size_t kMaxNumPackets = 10;
  const TimeDelta kRtt = RtpPacketHistory::kMinPacketDuration * 2;
  // Enable culling. Even without feedback, this can trigger early removal.
  hist_.SetStorePacketsStatus(StorageMode::kStoreAndCull, kMaxNumPackets);
  hist_.SetRtt(kRtt);

  hist_.PutRtpPacket(CreateRtpPacket(kStartSeqNum), fake_clock_.CurrentTime());

  TimeDelta kMaxPacketDuration = kRtt *
                                 RtpPacketHistory::kMinPacketDurationRtt *
                                 RtpPacketHistory::kPacketCullingDelayFactor;
  fake_clock_.AdvanceTime(kMaxPacketDuration - TimeDelta::Millis(1));

  // First packet should still be there.
  EXPECT_TRUE(hist_.GetPacketState(kStartSeqNum));

  // Advance to where packet can be culled, even if buffer is not full.
  fake_clock_.AdvanceTimeMilliseconds(1);
  hist_.PutRtpPacket(CreateRtpPacket(To16u(kStartSeqNum + 1)),
                     fake_clock_.CurrentTime());

  EXPECT_FALSE(hist_.GetPacketState(kStartSeqNum));
}

TEST_P(RtpPacketHistoryTest, CullWithAcks) {
  const TimeDelta kPacketLifetime = RtpPacketHistory::kMinPacketDuration *
                                    RtpPacketHistory::kPacketCullingDelayFactor;

  const Timestamp start_time = fake_clock_.CurrentTime();
  hist_.SetStorePacketsStatus(StorageMode::kStoreAndCull, 10);

  // Insert three packets 33ms apart, immediately mark them as sent.
  std::unique_ptr<RtpPacketToSend> packet = CreateRtpPacket(kStartSeqNum);
  packet->SetPayloadSize(50);
  hist_.PutRtpPacket(std::move(packet),
                     /*send_time=*/fake_clock_.CurrentTime());
  fake_clock_.AdvanceTimeMilliseconds(33);
  packet = CreateRtpPacket(To16u(kStartSeqNum + 1));
  packet->SetPayloadSize(50);
  hist_.PutRtpPacket(std::move(packet),
                     /*send_time=*/fake_clock_.CurrentTime());
  fake_clock_.AdvanceTimeMilliseconds(33);
  packet = CreateRtpPacket(To16u(kStartSeqNum + 2));
  packet->SetPayloadSize(50);
  hist_.PutRtpPacket(std::move(packet),
                     /*send_time=*/fake_clock_.CurrentTime());

  EXPECT_TRUE(hist_.GetPacketState(kStartSeqNum));
  EXPECT_TRUE(hist_.GetPacketState(To16u(kStartSeqNum + 1)));
  EXPECT_TRUE(hist_.GetPacketState(To16u(kStartSeqNum + 2)));

  // Remove middle one using ack, check that only that one is gone.
  std::vector<uint16_t> acked_sequence_numbers = {To16u(kStartSeqNum + 1)};
  hist_.CullAcknowledgedPackets(acked_sequence_numbers);

  EXPECT_TRUE(hist_.GetPacketState(kStartSeqNum));
  EXPECT_FALSE(hist_.GetPacketState(To16u(kStartSeqNum + 1)));
  EXPECT_TRUE(hist_.GetPacketState(To16u(kStartSeqNum + 2)));

  // Advance time to where second packet would have expired, verify first packet
  // is removed.
  Timestamp second_packet_expiry_time =
      start_time + kPacketLifetime + TimeDelta::Millis(33 + 1);
  fake_clock_.AdvanceTime(second_packet_expiry_time -
                          fake_clock_.CurrentTime());
  hist_.SetRtt(TimeDelta::Millis(1));  // Trigger culling of old packets.
  EXPECT_FALSE(hist_.GetPacketState(kStartSeqNum));
  EXPECT_FALSE(hist_.GetPacketState(To16u(kStartSeqNum + 1)));
  EXPECT_TRUE(hist_.GetPacketState(To16u(kStartSeqNum + 2)));

  // Advance to where last packet expires, verify all gone.
  fake_clock_.AdvanceTimeMilliseconds(33);
  hist_.SetRtt(TimeDelta::Millis(1));  // Trigger culling of old packets.
  EXPECT_FALSE(hist_.GetPacketState(kStartSeqNum));
  EXPECT_FALSE(hist_.GetPacketState(To16u(kStartSeqNum + 1)));
  EXPECT_FALSE(hist_.GetPacketState(To16u(kStartSeqNum + 2)));
}

TEST_P(RtpPacketHistoryTest, GetPacketAndSetSent) {
  const TimeDelta kRtt = RtpPacketHistory::kMinPacketDuration * 2;
  hist_.SetRtt(kRtt);

  // Set size to remove old packets as soon as possible.
  hist_.SetStorePacketsStatus(StorageMode::kStoreAndCull, 1);

  // Add a sent packet to the history.
  hist_.PutRtpPacket(CreateRtpPacket(kStartSeqNum), fake_clock_.CurrentTime());

  // Retransmission request, first retransmission is allowed immediately.
  EXPECT_TRUE(hist_.GetPacketAndMarkAsPending(kStartSeqNum));

  // Packet not yet sent, new retransmission not allowed.
  fake_clock_.AdvanceTime(kRtt);
  EXPECT_FALSE(hist_.GetPacketAndMarkAsPending(kStartSeqNum));

  // Mark as sent, but too early for retransmission.
  hist_.MarkPacketAsSent(kStartSeqNum);
  EXPECT_FALSE(hist_.GetPacketAndMarkAsPending(kStartSeqNum));

  // Enough time has passed, retransmission is allowed again.
  fake_clock_.AdvanceTime(kRtt);
  EXPECT_TRUE(hist_.GetPacketAndMarkAsPending(kStartSeqNum));
}

TEST_P(RtpPacketHistoryTest, GetPacketWithEncapsulation) {
  const uint32_t kSsrc = 92384762;
  const TimeDelta kRtt = RtpPacketHistory::kMinPacketDuration * 2;
  hist_.SetRtt(kRtt);

  // Set size to remove old packets as soon as possible.
  hist_.SetStorePacketsStatus(StorageMode::kStoreAndCull, 1);

  // Add a sent packet to the history, with a set SSRC.
  std::unique_ptr<RtpPacketToSend> packet = CreateRtpPacket(kStartSeqNum);
  packet->SetSsrc(kSsrc);
  hist_.PutRtpPacket(std::move(packet), fake_clock_.CurrentTime());

  // Retransmission request, simulate an RTX-like encapsulation, were the packet
  // is sent on a different SSRC.
  std::unique_ptr<RtpPacketToSend> retransmit_packet =
      hist_.GetPacketAndMarkAsPending(
          kStartSeqNum, [](const RtpPacketToSend& packet) {
            auto encapsulated_packet =
                std::make_unique<RtpPacketToSend>(packet);
            encapsulated_packet->SetSsrc(packet.Ssrc() + 1);
            return encapsulated_packet;
          });
  ASSERT_TRUE(retransmit_packet);
  EXPECT_EQ(retransmit_packet->Ssrc(), kSsrc + 1);
}

TEST_P(RtpPacketHistoryTest, GetPacketWithEncapsulationAbortOnNullptr) {
  hist_.SetStorePacketsStatus(StorageMode::kStoreAndCull, 1);

  hist_.PutRtpPacket(CreateRtpPacket(kStartSeqNum), fake_clock_.CurrentTime());

  // Retransmission request, but the encapsulator determines that this packet is
  // not suitable for retransmission (bandwidth exhausted?) so the retransmit is
  // aborted and the packet is not marked as pending.
  EXPECT_FALSE(hist_.GetPacketAndMarkAsPending(
      kStartSeqNum, [](const RtpPacketToSend&) { return nullptr; }));

  // New try, this time getting the packet should work, and it should not be
  // blocked due to any pending status.
  EXPECT_TRUE(hist_.GetPacketAndMarkAsPending(kStartSeqNum));
}

TEST_P(RtpPacketHistoryTest, DontRemovePendingTransmissions) {
  const TimeDelta kRtt = RtpPacketHistory::kMinPacketDuration * 2;
  const TimeDelta kPacketTimeout =
      kRtt * RtpPacketHistory::kMinPacketDurationRtt;

  // Set size to remove old packets as soon as possible.
  hist_.SetStorePacketsStatus(StorageMode::kStoreAndCull, 1);
  hist_.SetRtt(kRtt);

  // Add a sent packet.
  hist_.PutRtpPacket(CreateRtpPacket(kStartSeqNum), fake_clock_.CurrentTime());

  // Advance clock to just before packet timeout.
  fake_clock_.AdvanceTime(kPacketTimeout - TimeDelta::Millis(1));
  // Mark as enqueued in pacer.
  EXPECT_TRUE(hist_.GetPacketAndMarkAsPending(kStartSeqNum));

  // Advance clock to where packet would have timed out. It should still
  // be there and pending.
  fake_clock_.AdvanceTimeMilliseconds(1);
  EXPECT_TRUE(hist_.GetPacketState(kStartSeqNum));

  // Packet sent. Now it can be removed.
  hist_.MarkPacketAsSent(kStartSeqNum);
  hist_.SetRtt(kRtt);  // Force culling of old packets.
  EXPECT_FALSE(hist_.GetPacketState(kStartSeqNum));
}

TEST_P(RtpPacketHistoryTest, PrioritizedPayloadPadding) {
  if (!GetParam()) {
    // Padding prioritization is off, ignore this test.
    return;
  }

  hist_.SetStorePacketsStatus(StorageMode::kStoreAndCull, 1);

  // Add two sent packets, one millisecond apart.
  hist_.PutRtpPacket(CreateRtpPacket(kStartSeqNum), fake_clock_.CurrentTime());
  fake_clock_.AdvanceTimeMilliseconds(1);

  hist_.PutRtpPacket(CreateRtpPacket(kStartSeqNum + 1),
                     fake_clock_.CurrentTime());
  fake_clock_.AdvanceTimeMilliseconds(1);

  // Latest packet given equal retransmission count.
  EXPECT_EQ(hist_.GetPayloadPaddingPacket()->SequenceNumber(),
            kStartSeqNum + 1);

  // Older packet has lower retransmission count.
  EXPECT_EQ(hist_.GetPayloadPaddingPacket()->SequenceNumber(), kStartSeqNum);

  // Equal retransmission count again, use newest packet.
  EXPECT_EQ(hist_.GetPayloadPaddingPacket()->SequenceNumber(),
            kStartSeqNum + 1);

  // Older packet has lower retransmission count.
  EXPECT_EQ(hist_.GetPayloadPaddingPacket()->SequenceNumber(), kStartSeqNum);

  // Remove newest packet.
  hist_.CullAcknowledgedPackets(std::vector<uint16_t>{kStartSeqNum + 1});

  // Only older packet left.
  EXPECT_EQ(hist_.GetPayloadPaddingPacket()->SequenceNumber(), kStartSeqNum);

  hist_.CullAcknowledgedPackets(std::vector<uint16_t>{kStartSeqNum});

  EXPECT_EQ(hist_.GetPayloadPaddingPacket(), nullptr);
}

TEST_P(RtpPacketHistoryTest, NoPendingPacketAsPadding) {
  hist_.SetStorePacketsStatus(StorageMode::kStoreAndCull, 1);

  hist_.PutRtpPacket(CreateRtpPacket(kStartSeqNum), fake_clock_.CurrentTime());
  fake_clock_.AdvanceTimeMilliseconds(1);

  EXPECT_EQ(hist_.GetPayloadPaddingPacket()->SequenceNumber(), kStartSeqNum);

  // If packet is pending retransmission, don't try to use it as padding.
  hist_.GetPacketAndMarkAsPending(kStartSeqNum);
  EXPECT_EQ(nullptr, hist_.GetPayloadPaddingPacket());

  // Market it as no longer pending, should be usable as padding again.
  hist_.MarkPacketAsSent(kStartSeqNum);
  EXPECT_EQ(hist_.GetPayloadPaddingPacket()->SequenceNumber(), kStartSeqNum);
}

TEST_P(RtpPacketHistoryTest, PayloadPaddingWithEncapsulation) {
  hist_.SetStorePacketsStatus(StorageMode::kStoreAndCull, 1);

  hist_.PutRtpPacket(CreateRtpPacket(kStartSeqNum), fake_clock_.CurrentTime());
  fake_clock_.AdvanceTimeMilliseconds(1);

  // Aborted padding.
  EXPECT_EQ(nullptr, hist_.GetPayloadPaddingPacket(
                         [](const RtpPacketToSend&) { return nullptr; }));

  // Get copy of packet, but with sequence number modified.
  auto padding_packet =
      hist_.GetPayloadPaddingPacket([&](const RtpPacketToSend& packet) {
        auto encapsulated_packet = std::make_unique<RtpPacketToSend>(packet);
        encapsulated_packet->SetSequenceNumber(kStartSeqNum + 1);
        return encapsulated_packet;
      });
  ASSERT_TRUE(padding_packet);
  EXPECT_EQ(padding_packet->SequenceNumber(), kStartSeqNum + 1);
}

TEST_P(RtpPacketHistoryTest, NackAfterAckIsNoop) {
  hist_.SetStorePacketsStatus(StorageMode::kStoreAndCull, 2);
  // Add two sent packets.
  hist_.PutRtpPacket(CreateRtpPacket(kStartSeqNum), fake_clock_.CurrentTime());
  hist_.PutRtpPacket(CreateRtpPacket(kStartSeqNum + 1),
                     fake_clock_.CurrentTime());
  // Remove newest one.
  hist_.CullAcknowledgedPackets(std::vector<uint16_t>{kStartSeqNum + 1});
  // Retransmission request for already acked packet, should be noop.
  auto packet = hist_.GetPacketAndMarkAsPending(kStartSeqNum + 1);
  EXPECT_EQ(packet.get(), nullptr);
}

TEST_P(RtpPacketHistoryTest, OutOfOrderInsertRemoval) {
  hist_.SetStorePacketsStatus(StorageMode::kStoreAndCull, 10);

  // Insert packets, out of order, including both forwards and backwards
  // sequence number wraps.
  const int seq_offsets[] = {0, 1, -1, 2, -2, 3, -3};

  for (int offset : seq_offsets) {
    uint16_t seq_no = To16u(kStartSeqNum + offset);
    std::unique_ptr<RtpPacketToSend> packet = CreateRtpPacket(seq_no);
    packet->SetPayloadSize(50);
    hist_.PutRtpPacket(std::move(packet), fake_clock_.CurrentTime());
    fake_clock_.AdvanceTimeMilliseconds(33);
  }

  // Check packet are there and remove them in the same out-of-order fashion.
  for (int offset : seq_offsets) {
    uint16_t seq_no = To16u(kStartSeqNum + offset);
    EXPECT_TRUE(hist_.GetPacketState(seq_no));
    std::vector<uint16_t> acked_sequence_numbers = {seq_no};
    hist_.CullAcknowledgedPackets(acked_sequence_numbers);
    EXPECT_FALSE(hist_.GetPacketState(seq_no));
  }
}

TEST_P(RtpPacketHistoryTest, UsesLastPacketAsPaddingWithPrioOff) {
  if (GetParam()) {
    // Padding prioritization is enabled, ignore this test.
    return;
  }

  const size_t kHistorySize = 10;
  hist_.SetStorePacketsStatus(StorageMode::kStoreAndCull, kHistorySize);

  EXPECT_EQ(hist_.GetPayloadPaddingPacket(), nullptr);

  for (size_t i = 0; i < kHistorySize; ++i) {
    hist_.PutRtpPacket(CreateRtpPacket(To16u(kStartSeqNum + i)),
                       fake_clock_.CurrentTime());
    hist_.MarkPacketAsSent(To16u(kStartSeqNum + i));
    fake_clock_.AdvanceTimeMilliseconds(1);

    // Last packet always returned.
    EXPECT_EQ(hist_.GetPayloadPaddingPacket()->SequenceNumber(),
              To16u(kStartSeqNum + i));
    EXPECT_EQ(hist_.GetPayloadPaddingPacket()->SequenceNumber(),
              To16u(kStartSeqNum + i));
    EXPECT_EQ(hist_.GetPayloadPaddingPacket()->SequenceNumber(),
              To16u(kStartSeqNum + i));
  }

  // Remove packets from the end, last in the list should be returned.
  for (size_t i = kHistorySize - 1; i > 0; --i) {
    hist_.CullAcknowledgedPackets(
        std::vector<uint16_t>{To16u(kStartSeqNum + i)});

    EXPECT_EQ(hist_.GetPayloadPaddingPacket()->SequenceNumber(),
              To16u(kStartSeqNum + i - 1));
    EXPECT_EQ(hist_.GetPayloadPaddingPacket()->SequenceNumber(),
              To16u(kStartSeqNum + i - 1));
    EXPECT_EQ(hist_.GetPayloadPaddingPacket()->SequenceNumber(),
              To16u(kStartSeqNum + i - 1));
  }

  hist_.CullAcknowledgedPackets(std::vector<uint16_t>{kStartSeqNum});
  EXPECT_EQ(hist_.GetPayloadPaddingPacket(), nullptr);
}

INSTANTIATE_TEST_SUITE_P(WithAndWithoutPaddingPrio,
                         RtpPacketHistoryTest,
                         ::testing::Bool());
}  // namespace webrtc
