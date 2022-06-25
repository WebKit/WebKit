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
    packet->set_capture_time_ms(fake_clock_.TimeInMilliseconds());
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
  // Store a packet, but with send-time. It should then not be removed.
  hist_.PutRtpPacket(CreateRtpPacket(kStartSeqNum), absl::nullopt);
  EXPECT_TRUE(hist_.GetPacketState(kStartSeqNum));

  // Changing store status, even to the current one, will clear the history.
  hist_.SetStorePacketsStatus(StorageMode::kStoreAndCull, 10);
  EXPECT_FALSE(hist_.GetPacketState(kStartSeqNum));
}

TEST_P(RtpPacketHistoryTest, StartSeqResetAfterReset) {
  hist_.SetStorePacketsStatus(StorageMode::kStoreAndCull, 10);
  // Store a packet, but with send-time. It should then not be removed.
  hist_.PutRtpPacket(CreateRtpPacket(kStartSeqNum), absl::nullopt);
  EXPECT_TRUE(hist_.GetPacketState(kStartSeqNum));

  // Changing store status, to clear the history.
  hist_.SetStorePacketsStatus(StorageMode::kStoreAndCull, 10);
  EXPECT_FALSE(hist_.GetPacketState(kStartSeqNum));

  // Add a new packet.
  hist_.PutRtpPacket(CreateRtpPacket(To16u(kStartSeqNum + 1)), absl::nullopt);
  EXPECT_TRUE(hist_.GetPacketState(To16u(kStartSeqNum + 1)));

  // Advance time past where packet expires.
  fake_clock_.AdvanceTimeMilliseconds(
      RtpPacketHistory::kPacketCullingDelayFactor *
      RtpPacketHistory::kMinPacketDurationMs);

  // Add one more packet and verify no state left from packet before reset.
  hist_.PutRtpPacket(CreateRtpPacket(To16u(kStartSeqNum + 2)), absl::nullopt);
  EXPECT_FALSE(hist_.GetPacketState(kStartSeqNum));
  EXPECT_TRUE(hist_.GetPacketState(To16u(kStartSeqNum + 1)));
  EXPECT_TRUE(hist_.GetPacketState(To16u(kStartSeqNum + 2)));
}

TEST_P(RtpPacketHistoryTest, NoStoreStatus) {
  EXPECT_EQ(StorageMode::kDisabled, hist_.GetStorageMode());
  std::unique_ptr<RtpPacketToSend> packet = CreateRtpPacket(kStartSeqNum);
  hist_.PutRtpPacket(std::move(packet), absl::nullopt);
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
  hist_.PutRtpPacket(std::move(packet), absl::nullopt);
  EXPECT_TRUE(hist_.GetPacketState(kStartSeqNum));
}

TEST_P(RtpPacketHistoryTest, GetRtpPacket) {
  hist_.SetStorePacketsStatus(StorageMode::kStoreAndCull, 10);
  int64_t capture_time_ms = 1;
  std::unique_ptr<RtpPacketToSend> packet = CreateRtpPacket(kStartSeqNum);
  packet->set_capture_time_ms(capture_time_ms);
  rtc::CopyOnWriteBuffer buffer = packet->Buffer();
  hist_.PutRtpPacket(std::move(packet), absl::nullopt);

  std::unique_ptr<RtpPacketToSend> packet_out =
      hist_.GetPacketAndSetSendTime(kStartSeqNum);
  EXPECT_TRUE(packet_out);
  EXPECT_EQ(buffer, packet_out->Buffer());
  EXPECT_EQ(capture_time_ms, packet_out->capture_time_ms());
}

TEST_P(RtpPacketHistoryTest, PacketStateIsCorrect) {
  const uint32_t kSsrc = 92384762;
  const int64_t kRttMs = 100;
  hist_.SetStorePacketsStatus(StorageMode::kStoreAndCull, 10);
  hist_.SetRtt(kRttMs);
  std::unique_ptr<RtpPacketToSend> packet = CreateRtpPacket(kStartSeqNum);
  packet->SetSsrc(kSsrc);
  packet->SetPayloadSize(1234);
  const size_t packet_size = packet->size();

  hist_.PutRtpPacket(std::move(packet), fake_clock_.TimeInMilliseconds());

  absl::optional<RtpPacketHistory::PacketState> state =
      hist_.GetPacketState(kStartSeqNum);
  ASSERT_TRUE(state);
  EXPECT_EQ(state->rtp_sequence_number, kStartSeqNum);
  EXPECT_EQ(state->send_time_ms, fake_clock_.TimeInMilliseconds());
  EXPECT_EQ(state->capture_time_ms, fake_clock_.TimeInMilliseconds());
  EXPECT_EQ(state->ssrc, kSsrc);
  EXPECT_EQ(state->packet_size, packet_size);
  EXPECT_EQ(state->times_retransmitted, 0u);

  fake_clock_.AdvanceTimeMilliseconds(1);
  EXPECT_TRUE(hist_.GetPacketAndSetSendTime(kStartSeqNum));
  fake_clock_.AdvanceTimeMilliseconds(kRttMs + 1);

  state = hist_.GetPacketState(kStartSeqNum);
  ASSERT_TRUE(state);
  EXPECT_EQ(state->times_retransmitted, 1u);
}

TEST_P(RtpPacketHistoryTest, MinResendTimeWithPacer) {
  static const int64_t kMinRetransmitIntervalMs = 100;

  hist_.SetStorePacketsStatus(StorageMode::kStoreAndCull, 10);
  hist_.SetRtt(kMinRetransmitIntervalMs);
  int64_t capture_time_ms = fake_clock_.TimeInMilliseconds();
  std::unique_ptr<RtpPacketToSend> packet = CreateRtpPacket(kStartSeqNum);
  size_t len = packet->size();
  hist_.PutRtpPacket(std::move(packet), absl::nullopt);

  // First transmission: TimeToSendPacket() call from pacer.
  EXPECT_TRUE(hist_.GetPacketAndSetSendTime(kStartSeqNum));

  // First retransmission - allow early retransmission.
  fake_clock_.AdvanceTimeMilliseconds(1);

  // With pacer there's two calls to history:
  // 1) When the NACK request arrived, use GetPacketState() to see if the
  //    packet is there and verify RTT constraints. Then we use the ssrc
  //    and sequence number to enqueue the retransmission in the pacer
  // 2) When the pacer determines that it is time to send the packet, it calls
  //    GetPacketAndSetSendTime().
  absl::optional<RtpPacketHistory::PacketState> packet_state =
      hist_.GetPacketState(kStartSeqNum);
  EXPECT_TRUE(packet_state);
  EXPECT_EQ(len, packet_state->packet_size);
  EXPECT_EQ(capture_time_ms, packet_state->capture_time_ms);

  // Retransmission was allowed, next send it from pacer.
  EXPECT_TRUE(hist_.GetPacketAndSetSendTime(kStartSeqNum));

  // Second retransmission - advance time to just before retransmission OK.
  fake_clock_.AdvanceTimeMilliseconds(kMinRetransmitIntervalMs - 1);
  EXPECT_FALSE(hist_.GetPacketState(kStartSeqNum));

  // Advance time to just after retransmission OK.
  fake_clock_.AdvanceTimeMilliseconds(1);
  EXPECT_TRUE(hist_.GetPacketState(kStartSeqNum));
  EXPECT_TRUE(hist_.GetPacketAndSetSendTime(kStartSeqNum));
}

TEST_P(RtpPacketHistoryTest, MinResendTimeWithoutPacer) {
  static const int64_t kMinRetransmitIntervalMs = 100;

  hist_.SetStorePacketsStatus(StorageMode::kStoreAndCull, 10);
  hist_.SetRtt(kMinRetransmitIntervalMs);
  int64_t capture_time_ms = fake_clock_.TimeInMilliseconds();
  std::unique_ptr<RtpPacketToSend> packet = CreateRtpPacket(kStartSeqNum);
  size_t len = packet->size();
  hist_.PutRtpPacket(std::move(packet), fake_clock_.TimeInMilliseconds());

  // First retransmission - allow early retransmission.
  fake_clock_.AdvanceTimeMilliseconds(1);
  packet = hist_.GetPacketAndSetSendTime(kStartSeqNum);
  EXPECT_TRUE(packet);
  EXPECT_EQ(len, packet->size());
  EXPECT_EQ(capture_time_ms, packet->capture_time_ms());

  // Second retransmission - advance time to just before retransmission OK.
  fake_clock_.AdvanceTimeMilliseconds(kMinRetransmitIntervalMs - 1);
  EXPECT_FALSE(hist_.GetPacketAndSetSendTime(kStartSeqNum));

  // Advance time to just after retransmission OK.
  fake_clock_.AdvanceTimeMilliseconds(1);
  EXPECT_TRUE(hist_.GetPacketAndSetSendTime(kStartSeqNum));
}

TEST_P(RtpPacketHistoryTest, RemovesOldestSentPacketWhenAtMaxSize) {
  const size_t kMaxNumPackets = 10;
  hist_.SetStorePacketsStatus(StorageMode::kStoreAndCull, kMaxNumPackets);

  // History does not allow removing packets within kMinPacketDurationMs,
  // so in order to test capacity, make sure insertion spans this time.
  const int64_t kPacketIntervalMs =
      RtpPacketHistory::kMinPacketDurationMs / kMaxNumPackets;

  // Add packets until the buffer is full.
  for (size_t i = 0; i < kMaxNumPackets; ++i) {
    std::unique_ptr<RtpPacketToSend> packet =
        CreateRtpPacket(To16u(kStartSeqNum + i));
    // Immediate mark packet as sent.
    hist_.PutRtpPacket(std::move(packet), fake_clock_.TimeInMilliseconds());
    fake_clock_.AdvanceTimeMilliseconds(kPacketIntervalMs);
  }

  // First packet should still be there.
  EXPECT_TRUE(hist_.GetPacketState(kStartSeqNum));

  // History is full, oldest one should be overwritten.
  std::unique_ptr<RtpPacketToSend> packet =
      CreateRtpPacket(To16u(kStartSeqNum + kMaxNumPackets));
  hist_.PutRtpPacket(std::move(packet), fake_clock_.TimeInMilliseconds());

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
    // Don't mark packets as sent, preventing them from being removed.
    hist_.PutRtpPacket(std::move(packet), absl::nullopt);
  }

  // First packet should still be there.
  EXPECT_TRUE(hist_.GetPacketState(kStartSeqNum));

  // History is full, oldest one should be overwritten.
  std::unique_ptr<RtpPacketToSend> packet =
      CreateRtpPacket(To16u(kStartSeqNum + kMaxNumPackets));
  hist_.PutRtpPacket(std::move(packet), fake_clock_.TimeInMilliseconds());

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
  hist_.SetRtt(1);

  // Add packets until the max is reached, and then yet another one.
  for (size_t i = 0; i < kMaxNumPackets + 1; ++i) {
    std::unique_ptr<RtpPacketToSend> packet =
        CreateRtpPacket(To16u(kStartSeqNum + i));
    // Don't mark packets as sent, preventing them from being removed.
    hist_.PutRtpPacket(std::move(packet), fake_clock_.TimeInMilliseconds());
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

TEST_P(RtpPacketHistoryTest, DontRemoveUnsentPackets) {
  const size_t kMaxNumPackets = 10;
  hist_.SetStorePacketsStatus(StorageMode::kStoreAndCull, kMaxNumPackets);

  // Add packets until the buffer is full.
  for (size_t i = 0; i < kMaxNumPackets; ++i) {
    // Mark packets as unsent.
    hist_.PutRtpPacket(CreateRtpPacket(To16u(kStartSeqNum + i)), absl::nullopt);
  }
  fake_clock_.AdvanceTimeMilliseconds(RtpPacketHistory::kMinPacketDurationMs);

  // First packet should still be there.
  EXPECT_TRUE(hist_.GetPacketState(kStartSeqNum));

  // History is full, but old packets not sent, so allow expansion.
  hist_.PutRtpPacket(CreateRtpPacket(To16u(kStartSeqNum + kMaxNumPackets)),
                     fake_clock_.TimeInMilliseconds());
  EXPECT_TRUE(hist_.GetPacketState(kStartSeqNum));

  // Set all packet as sent and advance time past min packet duration time,
  // otherwise packets till still be prevented from being removed.
  for (size_t i = 0; i <= kMaxNumPackets; ++i) {
    EXPECT_TRUE(hist_.GetPacketAndSetSendTime(To16u(kStartSeqNum + i)));
  }
  fake_clock_.AdvanceTimeMilliseconds(RtpPacketHistory::kMinPacketDurationMs);
  // Add a new packet, this means the two oldest ones will be culled.
  hist_.PutRtpPacket(CreateRtpPacket(To16u(kStartSeqNum + kMaxNumPackets + 1)),
                     fake_clock_.TimeInMilliseconds());
  EXPECT_FALSE(hist_.GetPacketState(kStartSeqNum));
  EXPECT_FALSE(hist_.GetPacketState(To16u(kStartSeqNum + 1)));
  EXPECT_TRUE(hist_.GetPacketState(To16u(kStartSeqNum + 2)));
}

TEST_P(RtpPacketHistoryTest, DontRemoveTooRecentlyTransmittedPackets) {
  // Set size to remove old packets as soon as possible.
  hist_.SetStorePacketsStatus(StorageMode::kStoreAndCull, 1);

  // Add a packet, marked as send, and advance time to just before removal time.
  hist_.PutRtpPacket(CreateRtpPacket(kStartSeqNum),
                     fake_clock_.TimeInMilliseconds());
  fake_clock_.AdvanceTimeMilliseconds(RtpPacketHistory::kMinPacketDurationMs -
                                      1);

  // Add a new packet to trigger culling.
  hist_.PutRtpPacket(CreateRtpPacket(To16u(kStartSeqNum + 1)),
                     fake_clock_.TimeInMilliseconds());
  // First packet should still be there.
  EXPECT_TRUE(hist_.GetPacketState(kStartSeqNum));

  // Advance time to where packet will be eligible for removal and try again.
  fake_clock_.AdvanceTimeMilliseconds(1);
  hist_.PutRtpPacket(CreateRtpPacket(To16u(kStartSeqNum + 2)),
                     fake_clock_.TimeInMilliseconds());
  // First packet should no be gone, but next one still there.
  EXPECT_FALSE(hist_.GetPacketState(kStartSeqNum));
  EXPECT_TRUE(hist_.GetPacketState(To16u(kStartSeqNum + 1)));
}

TEST_P(RtpPacketHistoryTest, DontRemoveTooRecentlyTransmittedPacketsHighRtt) {
  const int64_t kRttMs = RtpPacketHistory::kMinPacketDurationMs * 2;
  const int64_t kPacketTimeoutMs =
      kRttMs * RtpPacketHistory::kMinPacketDurationRtt;

  // Set size to remove old packets as soon as possible.
  hist_.SetStorePacketsStatus(StorageMode::kStoreAndCull, 1);
  hist_.SetRtt(kRttMs);

  // Add a packet, marked as send, and advance time to just before removal time.
  hist_.PutRtpPacket(CreateRtpPacket(kStartSeqNum),
                     fake_clock_.TimeInMilliseconds());
  fake_clock_.AdvanceTimeMilliseconds(kPacketTimeoutMs - 1);

  // Add a new packet to trigger culling.
  hist_.PutRtpPacket(CreateRtpPacket(To16u(kStartSeqNum + 1)),
                     fake_clock_.TimeInMilliseconds());
  // First packet should still be there.
  EXPECT_TRUE(hist_.GetPacketState(kStartSeqNum));

  // Advance time to where packet will be eligible for removal and try again.
  fake_clock_.AdvanceTimeMilliseconds(1);
  hist_.PutRtpPacket(CreateRtpPacket(To16u(kStartSeqNum + 2)),
                     fake_clock_.TimeInMilliseconds());
  // First packet should no be gone, but next one still there.
  EXPECT_FALSE(hist_.GetPacketState(kStartSeqNum));
  EXPECT_TRUE(hist_.GetPacketState(To16u(kStartSeqNum + 1)));
}

TEST_P(RtpPacketHistoryTest, RemovesOldWithCulling) {
  const size_t kMaxNumPackets = 10;
  // Enable culling. Even without feedback, this can trigger early removal.
  hist_.SetStorePacketsStatus(StorageMode::kStoreAndCull, kMaxNumPackets);

  hist_.PutRtpPacket(CreateRtpPacket(kStartSeqNum),
                     fake_clock_.TimeInMilliseconds());

  int64_t kMaxPacketDurationMs = RtpPacketHistory::kMinPacketDurationMs *
                                 RtpPacketHistory::kPacketCullingDelayFactor;
  fake_clock_.AdvanceTimeMilliseconds(kMaxPacketDurationMs - 1);

  // First packet should still be there.
  EXPECT_TRUE(hist_.GetPacketState(kStartSeqNum));

  // Advance to where packet can be culled, even if buffer is not full.
  fake_clock_.AdvanceTimeMilliseconds(1);
  hist_.PutRtpPacket(CreateRtpPacket(To16u(kStartSeqNum + 1)),
                     fake_clock_.TimeInMilliseconds());

  EXPECT_FALSE(hist_.GetPacketState(kStartSeqNum));
}

TEST_P(RtpPacketHistoryTest, RemovesOldWithCullingHighRtt) {
  const size_t kMaxNumPackets = 10;
  const int64_t kRttMs = RtpPacketHistory::kMinPacketDurationMs * 2;
  // Enable culling. Even without feedback, this can trigger early removal.
  hist_.SetStorePacketsStatus(StorageMode::kStoreAndCull, kMaxNumPackets);
  hist_.SetRtt(kRttMs);

  hist_.PutRtpPacket(CreateRtpPacket(kStartSeqNum),
                     fake_clock_.TimeInMilliseconds());

  int64_t kMaxPacketDurationMs = kRttMs *
                                 RtpPacketHistory::kMinPacketDurationRtt *
                                 RtpPacketHistory::kPacketCullingDelayFactor;
  fake_clock_.AdvanceTimeMilliseconds(kMaxPacketDurationMs - 1);

  // First packet should still be there.
  EXPECT_TRUE(hist_.GetPacketState(kStartSeqNum));

  // Advance to where packet can be culled, even if buffer is not full.
  fake_clock_.AdvanceTimeMilliseconds(1);
  hist_.PutRtpPacket(CreateRtpPacket(To16u(kStartSeqNum + 1)),
                     fake_clock_.TimeInMilliseconds());

  EXPECT_FALSE(hist_.GetPacketState(kStartSeqNum));
}

TEST_P(RtpPacketHistoryTest, CullWithAcks) {
  const int64_t kPacketLifetime = RtpPacketHistory::kMinPacketDurationMs *
                                  RtpPacketHistory::kPacketCullingDelayFactor;

  const int64_t start_time = fake_clock_.TimeInMilliseconds();
  hist_.SetStorePacketsStatus(StorageMode::kStoreAndCull, 10);

  // Insert three packets 33ms apart, immediately mark them as sent.
  std::unique_ptr<RtpPacketToSend> packet = CreateRtpPacket(kStartSeqNum);
  packet->SetPayloadSize(50);
  hist_.PutRtpPacket(std::move(packet), fake_clock_.TimeInMilliseconds());
  hist_.GetPacketAndSetSendTime(kStartSeqNum);
  fake_clock_.AdvanceTimeMilliseconds(33);
  packet = CreateRtpPacket(To16u(kStartSeqNum + 1));
  packet->SetPayloadSize(50);
  hist_.PutRtpPacket(std::move(packet), fake_clock_.TimeInMilliseconds());
  hist_.GetPacketAndSetSendTime(To16u(kStartSeqNum + 1));
  fake_clock_.AdvanceTimeMilliseconds(33);
  packet = CreateRtpPacket(To16u(kStartSeqNum + 2));
  packet->SetPayloadSize(50);
  hist_.PutRtpPacket(std::move(packet), fake_clock_.TimeInMilliseconds());
  hist_.GetPacketAndSetSendTime(To16u(kStartSeqNum + 2));

  EXPECT_TRUE(hist_.GetPacketState(kStartSeqNum).has_value());
  EXPECT_TRUE(hist_.GetPacketState(To16u(kStartSeqNum + 1)).has_value());
  EXPECT_TRUE(hist_.GetPacketState(To16u(kStartSeqNum + 2)).has_value());

  // Remove middle one using ack, check that only that one is gone.
  std::vector<uint16_t> acked_sequence_numbers = {To16u(kStartSeqNum + 1)};
  hist_.CullAcknowledgedPackets(acked_sequence_numbers);

  EXPECT_TRUE(hist_.GetPacketState(kStartSeqNum).has_value());
  EXPECT_FALSE(hist_.GetPacketState(To16u(kStartSeqNum + 1)).has_value());
  EXPECT_TRUE(hist_.GetPacketState(To16u(kStartSeqNum + 2)).has_value());

  // Advance time to where second packet would have expired, verify first packet
  // is removed.
  int64_t second_packet_expiry_time = start_time + kPacketLifetime + 33 + 1;
  fake_clock_.AdvanceTimeMilliseconds(second_packet_expiry_time -
                                      fake_clock_.TimeInMilliseconds());
  hist_.SetRtt(1);  // Trigger culling of old packets.
  EXPECT_FALSE(hist_.GetPacketState(kStartSeqNum).has_value());
  EXPECT_FALSE(hist_.GetPacketState(To16u(kStartSeqNum + 1)).has_value());
  EXPECT_TRUE(hist_.GetPacketState(To16u(kStartSeqNum + 2)).has_value());

  // Advance to where last packet expires, verify all gone.
  fake_clock_.AdvanceTimeMilliseconds(33);
  hist_.SetRtt(1);  // Trigger culling of old packets.
  EXPECT_FALSE(hist_.GetPacketState(kStartSeqNum).has_value());
  EXPECT_FALSE(hist_.GetPacketState(To16u(kStartSeqNum + 1)).has_value());
  EXPECT_FALSE(hist_.GetPacketState(To16u(kStartSeqNum + 2)).has_value());
}

TEST_P(RtpPacketHistoryTest, SetsPendingTransmissionState) {
  const int64_t kRttMs = RtpPacketHistory::kMinPacketDurationMs * 2;
  hist_.SetRtt(kRttMs);

  // Set size to remove old packets as soon as possible.
  hist_.SetStorePacketsStatus(StorageMode::kStoreAndCull, 1);

  // Add a packet, without send time, indicating it's in pacer queue.
  hist_.PutRtpPacket(CreateRtpPacket(kStartSeqNum),
                     /* send_time_ms = */ absl::nullopt);

  // Packet is pending transmission.
  absl::optional<RtpPacketHistory::PacketState> packet_state =
      hist_.GetPacketState(kStartSeqNum);
  ASSERT_TRUE(packet_state.has_value());
  EXPECT_TRUE(packet_state->pending_transmission);

  // Packet sent, state should be back to non-pending.
  EXPECT_TRUE(hist_.GetPacketAndSetSendTime(kStartSeqNum));
  packet_state = hist_.GetPacketState(kStartSeqNum);
  ASSERT_TRUE(packet_state.has_value());
  EXPECT_FALSE(packet_state->pending_transmission);

  // Time for a retransmission.
  fake_clock_.AdvanceTimeMilliseconds(kRttMs);
  EXPECT_TRUE(hist_.SetPendingTransmission(kStartSeqNum));
  packet_state = hist_.GetPacketState(kStartSeqNum);
  ASSERT_TRUE(packet_state.has_value());
  EXPECT_TRUE(packet_state->pending_transmission);

  // Packet sent.
  EXPECT_TRUE(hist_.GetPacketAndSetSendTime(kStartSeqNum));
  // Too early for retransmission.
  ASSERT_FALSE(hist_.GetPacketState(kStartSeqNum).has_value());

  // Retransmission allowed again, it's not in a pending state.
  fake_clock_.AdvanceTimeMilliseconds(kRttMs);
  packet_state = hist_.GetPacketState(kStartSeqNum);
  ASSERT_TRUE(packet_state.has_value());
  EXPECT_FALSE(packet_state->pending_transmission);
}

TEST_P(RtpPacketHistoryTest, GetPacketAndSetSent) {
  const int64_t kRttMs = RtpPacketHistory::kMinPacketDurationMs * 2;
  hist_.SetRtt(kRttMs);

  // Set size to remove old packets as soon as possible.
  hist_.SetStorePacketsStatus(StorageMode::kStoreAndCull, 1);

  // Add a sent packet to the history.
  hist_.PutRtpPacket(CreateRtpPacket(kStartSeqNum),
                     fake_clock_.TimeInMicroseconds());

  // Retransmission request, first retransmission is allowed immediately.
  EXPECT_TRUE(hist_.GetPacketAndMarkAsPending(kStartSeqNum));

  // Packet not yet sent, new retransmission not allowed.
  fake_clock_.AdvanceTimeMilliseconds(kRttMs);
  EXPECT_FALSE(hist_.GetPacketAndMarkAsPending(kStartSeqNum));

  // Mark as sent, but too early for retransmission.
  hist_.MarkPacketAsSent(kStartSeqNum);
  EXPECT_FALSE(hist_.GetPacketAndMarkAsPending(kStartSeqNum));

  // Enough time has passed, retransmission is allowed again.
  fake_clock_.AdvanceTimeMilliseconds(kRttMs);
  EXPECT_TRUE(hist_.GetPacketAndMarkAsPending(kStartSeqNum));
}

TEST_P(RtpPacketHistoryTest, GetPacketWithEncapsulation) {
  const uint32_t kSsrc = 92384762;
  const int64_t kRttMs = RtpPacketHistory::kMinPacketDurationMs * 2;
  hist_.SetRtt(kRttMs);

  // Set size to remove old packets as soon as possible.
  hist_.SetStorePacketsStatus(StorageMode::kStoreAndCull, 1);

  // Add a sent packet to the history, with a set SSRC.
  std::unique_ptr<RtpPacketToSend> packet = CreateRtpPacket(kStartSeqNum);
  packet->SetSsrc(kSsrc);
  hist_.PutRtpPacket(std::move(packet), fake_clock_.TimeInMicroseconds());

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

  hist_.PutRtpPacket(CreateRtpPacket(kStartSeqNum),
                     fake_clock_.TimeInMicroseconds());

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
  const int64_t kRttMs = RtpPacketHistory::kMinPacketDurationMs * 2;
  const int64_t kPacketTimeoutMs =
      kRttMs * RtpPacketHistory::kMinPacketDurationRtt;

  // Set size to remove old packets as soon as possible.
  hist_.SetStorePacketsStatus(StorageMode::kStoreAndCull, 1);
  hist_.SetRtt(kRttMs);

  // Add a sent packet.
  hist_.PutRtpPacket(CreateRtpPacket(kStartSeqNum),
                     fake_clock_.TimeInMilliseconds());

  // Advance clock to just before packet timeout.
  fake_clock_.AdvanceTimeMilliseconds(kPacketTimeoutMs - 1);
  // Mark as enqueued in pacer.
  EXPECT_TRUE(hist_.SetPendingTransmission(kStartSeqNum));

  // Advance clock to where packet would have timed out. It should still
  // be there and pending.
  fake_clock_.AdvanceTimeMilliseconds(1);
  absl::optional<RtpPacketHistory::PacketState> packet_state =
      hist_.GetPacketState(kStartSeqNum);
  ASSERT_TRUE(packet_state.has_value());
  EXPECT_TRUE(packet_state->pending_transmission);

  // Packet sent. Now it can be removed.
  EXPECT_TRUE(hist_.GetPacketAndSetSendTime(kStartSeqNum));
  hist_.SetRtt(kRttMs);  // Force culling of old packets.
  packet_state = hist_.GetPacketState(kStartSeqNum);
  ASSERT_FALSE(packet_state.has_value());
}

TEST_P(RtpPacketHistoryTest, PrioritizedPayloadPadding) {
  if (!GetParam()) {
    // Padding prioritization is off, ignore this test.
    return;
  }

  hist_.SetStorePacketsStatus(StorageMode::kStoreAndCull, 1);

  // Add two sent packets, one millisecond apart.
  hist_.PutRtpPacket(CreateRtpPacket(kStartSeqNum),
                     fake_clock_.TimeInMilliseconds());
  fake_clock_.AdvanceTimeMilliseconds(1);

  hist_.PutRtpPacket(CreateRtpPacket(kStartSeqNum + 1),
                     fake_clock_.TimeInMilliseconds());
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

  hist_.PutRtpPacket(CreateRtpPacket(kStartSeqNum),
                     fake_clock_.TimeInMilliseconds());
  fake_clock_.AdvanceTimeMilliseconds(1);

  EXPECT_EQ(hist_.GetPayloadPaddingPacket()->SequenceNumber(), kStartSeqNum);

  // If packet is pending retransmission, don't try to use it as padding.
  hist_.SetPendingTransmission(kStartSeqNum);
  EXPECT_EQ(nullptr, hist_.GetPayloadPaddingPacket());

  // Market it as no longer pending, should be usable as padding again.
  hist_.GetPacketAndSetSendTime(kStartSeqNum);
  EXPECT_EQ(hist_.GetPayloadPaddingPacket()->SequenceNumber(), kStartSeqNum);
}

TEST_P(RtpPacketHistoryTest, PayloadPaddingWithEncapsulation) {
  hist_.SetStorePacketsStatus(StorageMode::kStoreAndCull, 1);

  hist_.PutRtpPacket(CreateRtpPacket(kStartSeqNum),
                     fake_clock_.TimeInMilliseconds());
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
  hist_.PutRtpPacket(CreateRtpPacket(kStartSeqNum),
                     fake_clock_.TimeInMilliseconds());
  hist_.PutRtpPacket(CreateRtpPacket(kStartSeqNum + 1),
                     fake_clock_.TimeInMilliseconds());
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
  const int64_t start_time_ms = fake_clock_.TimeInMilliseconds();

  for (int offset : seq_offsets) {
    uint16_t seq_no = To16u(kStartSeqNum + offset);
    std::unique_ptr<RtpPacketToSend> packet = CreateRtpPacket(seq_no);
    packet->SetPayloadSize(50);
    hist_.PutRtpPacket(std::move(packet), fake_clock_.TimeInMilliseconds());
    hist_.GetPacketAndSetSendTime(seq_no);
    fake_clock_.AdvanceTimeMilliseconds(33);
  }

  // Check packet are there and remove them in the same out-of-order fashion.
  int64_t expected_time_offset_ms = 0;
  for (int offset : seq_offsets) {
    uint16_t seq_no = To16u(kStartSeqNum + offset);
    absl::optional<RtpPacketHistory::PacketState> packet_state =
        hist_.GetPacketState(seq_no);
    ASSERT_TRUE(packet_state.has_value());
    EXPECT_EQ(packet_state->send_time_ms,
              start_time_ms + expected_time_offset_ms);
    std::vector<uint16_t> acked_sequence_numbers = {seq_no};
    hist_.CullAcknowledgedPackets(acked_sequence_numbers);
    expected_time_offset_ms += 33;
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
                       fake_clock_.TimeInMilliseconds());
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
