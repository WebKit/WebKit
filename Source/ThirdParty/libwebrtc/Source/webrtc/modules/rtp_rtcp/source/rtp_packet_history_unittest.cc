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

class RtpPacketHistoryTest : public ::testing::Test {
 protected:
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
  EXPECT_EQ(StorageMode::kDisabled, hist_.GetStorageMode());
  hist_.SetStorePacketsStatus(StorageMode::kStore, 10);
  EXPECT_EQ(StorageMode::kStore, hist_.GetStorageMode());
  hist_.SetStorePacketsStatus(StorageMode::kStoreAndCull, 10);
  EXPECT_EQ(StorageMode::kStoreAndCull, hist_.GetStorageMode());
  hist_.SetStorePacketsStatus(StorageMode::kDisabled, 0);
  EXPECT_EQ(StorageMode::kDisabled, hist_.GetStorageMode());
}

TEST_F(RtpPacketHistoryTest, ClearsHistoryAfterSetStoreStatus) {
  hist_.SetStorePacketsStatus(StorageMode::kStore, 10);
  // Store a packet, but with send-time. It should then not be removed.
  hist_.PutRtpPacket(CreateRtpPacket(kStartSeqNum), kAllowRetransmission,
                     absl::nullopt);
  EXPECT_TRUE(hist_.GetPacketState(kStartSeqNum, false));

  // Changing store status, even to the current one, will clear the history.
  hist_.SetStorePacketsStatus(StorageMode::kStore, 10);
  EXPECT_FALSE(hist_.GetPacketState(kStartSeqNum, false));
}

TEST_F(RtpPacketHistoryTest, StartSeqResetAfterReset) {
  hist_.SetStorePacketsStatus(StorageMode::kStoreAndCull, 10);
  // Store a packet, but with send-time. It should then not be removed.
  hist_.PutRtpPacket(CreateRtpPacket(kStartSeqNum), kAllowRetransmission,
                     absl::nullopt);
  EXPECT_TRUE(hist_.GetPacketState(kStartSeqNum, false));

  // Changing store status, to clear the history.
  hist_.SetStorePacketsStatus(StorageMode::kStoreAndCull, 10);
  EXPECT_FALSE(hist_.GetPacketState(kStartSeqNum, false));

  // Add a new packet.
  hist_.PutRtpPacket(CreateRtpPacket(kStartSeqNum + 1), kAllowRetransmission,
                     absl::nullopt);
  EXPECT_TRUE(hist_.GetPacketState(kStartSeqNum + 1, false));

  // Advance time past where packet expires.
  fake_clock_.AdvanceTimeMilliseconds(
      RtpPacketHistory::kPacketCullingDelayFactor *
      RtpPacketHistory::kMinPacketDurationMs);

  // Add one more packet and verify no state left from packet before reset.
  hist_.PutRtpPacket(CreateRtpPacket(To16u(kStartSeqNum + 2)),
                     kAllowRetransmission, absl::nullopt);
  EXPECT_FALSE(hist_.GetPacketState(kStartSeqNum, false));
  EXPECT_TRUE(hist_.GetPacketState(kStartSeqNum + 1, false));
  EXPECT_TRUE(hist_.GetPacketState(To16u(kStartSeqNum + 2), false));
}

TEST_F(RtpPacketHistoryTest, NoStoreStatus) {
  EXPECT_EQ(StorageMode::kDisabled, hist_.GetStorageMode());
  std::unique_ptr<RtpPacketToSend> packet = CreateRtpPacket(kStartSeqNum);
  hist_.PutRtpPacket(std::move(packet), kAllowRetransmission, absl::nullopt);
  // Packet should not be stored.
  EXPECT_FALSE(hist_.GetPacketState(kStartSeqNum, false));
}

TEST_F(RtpPacketHistoryTest, GetRtpPacket_NotStored) {
  hist_.SetStorePacketsStatus(StorageMode::kStore, 10);
  EXPECT_FALSE(hist_.GetPacketState(0, false));
}

TEST_F(RtpPacketHistoryTest, PutRtpPacket) {
  hist_.SetStorePacketsStatus(StorageMode::kStore, 10);
  std::unique_ptr<RtpPacketToSend> packet = CreateRtpPacket(kStartSeqNum);

  EXPECT_FALSE(hist_.GetPacketState(kStartSeqNum, false));
  hist_.PutRtpPacket(std::move(packet), kAllowRetransmission, absl::nullopt);
  EXPECT_TRUE(hist_.GetPacketState(kStartSeqNum, false));
}

TEST_F(RtpPacketHistoryTest, GetRtpPacket) {
  hist_.SetStorePacketsStatus(StorageMode::kStore, 10);
  int64_t capture_time_ms = 1;
  std::unique_ptr<RtpPacketToSend> packet = CreateRtpPacket(kStartSeqNum);
  packet->set_capture_time_ms(capture_time_ms);
  rtc::CopyOnWriteBuffer buffer = packet->Buffer();
  hist_.PutRtpPacket(std::move(packet), kAllowRetransmission, absl::nullopt);

  std::unique_ptr<RtpPacketToSend> packet_out =
      hist_.GetPacketAndSetSendTime(kStartSeqNum, false);
  EXPECT_TRUE(packet_out);
  EXPECT_EQ(buffer, packet_out->Buffer());
  EXPECT_EQ(capture_time_ms, packet_out->capture_time_ms());
}

TEST_F(RtpPacketHistoryTest, NoCaptureTime) {
  hist_.SetStorePacketsStatus(StorageMode::kStore, 10);
  fake_clock_.AdvanceTimeMilliseconds(1);
  int64_t capture_time_ms = fake_clock_.TimeInMilliseconds();
  std::unique_ptr<RtpPacketToSend> packet = CreateRtpPacket(kStartSeqNum);
  packet->set_capture_time_ms(-1);
  rtc::CopyOnWriteBuffer buffer = packet->Buffer();
  hist_.PutRtpPacket(std::move(packet), kAllowRetransmission, absl::nullopt);

  std::unique_ptr<RtpPacketToSend> packet_out =
      hist_.GetPacketAndSetSendTime(kStartSeqNum, false);
  EXPECT_TRUE(packet_out);
  EXPECT_EQ(buffer, packet_out->Buffer());
  EXPECT_EQ(capture_time_ms, packet_out->capture_time_ms());
}

TEST_F(RtpPacketHistoryTest, DontRetransmit) {
  hist_.SetStorePacketsStatus(StorageMode::kStore, 10);
  int64_t capture_time_ms = fake_clock_.TimeInMilliseconds();
  std::unique_ptr<RtpPacketToSend> packet = CreateRtpPacket(kStartSeqNum);
  rtc::CopyOnWriteBuffer buffer = packet->Buffer();
  hist_.PutRtpPacket(std::move(packet), kDontRetransmit, absl::nullopt);

  // Get the packet and verify data.
  std::unique_ptr<RtpPacketToSend> packet_out;
  packet_out = hist_.GetPacketAndSetSendTime(kStartSeqNum, false);
  ASSERT_TRUE(packet_out);
  EXPECT_EQ(buffer.size(), packet_out->size());
  EXPECT_EQ(capture_time_ms, packet_out->capture_time_ms());

  // Non-retransmittable packets are immediately removed, so getting in again
  // should fail.
  EXPECT_FALSE(hist_.GetPacketAndSetSendTime(kStartSeqNum, false));
}

TEST_F(RtpPacketHistoryTest, PacketStateIsCorrect) {
  const uint32_t kSsrc = 92384762;
  hist_.SetStorePacketsStatus(StorageMode::kStoreAndCull, 10);
  std::unique_ptr<RtpPacketToSend> packet = CreateRtpPacket(kStartSeqNum);
  packet->SetSsrc(kSsrc);
  packet->SetPayloadSize(1234);
  const size_t packet_size = packet->size();

  hist_.PutRtpPacket(std::move(packet), StorageType::kAllowRetransmission,
                     fake_clock_.TimeInMilliseconds());

  absl::optional<RtpPacketHistory::PacketState> state =
      hist_.GetPacketState(kStartSeqNum, false);
  ASSERT_TRUE(state);
  EXPECT_EQ(state->rtp_sequence_number, kStartSeqNum);
  EXPECT_EQ(state->send_time_ms, fake_clock_.TimeInMilliseconds());
  EXPECT_EQ(state->capture_time_ms, fake_clock_.TimeInMilliseconds());
  EXPECT_EQ(state->ssrc, kSsrc);
  EXPECT_EQ(state->payload_size, packet_size);
  EXPECT_EQ(state->times_retransmitted, 0u);

  fake_clock_.AdvanceTimeMilliseconds(1);
  EXPECT_TRUE(hist_.GetPacketAndSetSendTime(kStartSeqNum, false));

  state = hist_.GetPacketState(kStartSeqNum, false);
  ASSERT_TRUE(state);
  EXPECT_EQ(state->times_retransmitted, 1u);
}

TEST_F(RtpPacketHistoryTest, MinResendTimeWithPacer) {
  static const int64_t kMinRetransmitIntervalMs = 100;

  hist_.SetStorePacketsStatus(StorageMode::kStore, 10);
  hist_.SetRtt(kMinRetransmitIntervalMs);
  int64_t capture_time_ms = fake_clock_.TimeInMilliseconds();
  std::unique_ptr<RtpPacketToSend> packet = CreateRtpPacket(kStartSeqNum);
  size_t len = packet->size();
  hist_.PutRtpPacket(std::move(packet), kAllowRetransmission, absl::nullopt);

  // First transmission: TimeToSendPacket() call from pacer.
  EXPECT_TRUE(hist_.GetPacketAndSetSendTime(kStartSeqNum, false));

  // First retransmission - allow early retransmission.
  fake_clock_.AdvanceTimeMilliseconds(1);

  // With pacer there's two calls to history:
  // 1) When the NACK request arrived, use GetPacketState() to see if the
  //    packet is there and verify RTT constraints. Then we use the ssrc
  //    and sequence number to enqueue the retransmission in the pacer
  // 2) When the pacer determines that it is time to send the packet, it calls
  //    GetPacketAndSetSendTime(). This time we do not need to verify RTT as
  //    has that has already been done.
  absl::optional<RtpPacketHistory::PacketState> packet_state =
      hist_.GetPacketState(kStartSeqNum, /*verify_rtt=*/true);
  EXPECT_TRUE(packet_state);
  EXPECT_EQ(len, packet_state->payload_size);
  EXPECT_EQ(capture_time_ms, packet_state->capture_time_ms);

  // Retransmission was allowed, next send it from pacer.
  EXPECT_TRUE(hist_.GetPacketAndSetSendTime(kStartSeqNum,
                                            /*verify_rtt=*/false));

  // Second retransmission - advance time to just before retransmission OK.
  fake_clock_.AdvanceTimeMilliseconds(kMinRetransmitIntervalMs - 1);
  EXPECT_FALSE(hist_.GetPacketState(kStartSeqNum, /*verify_rtt=*/true));

  // Advance time to just after retransmission OK.
  fake_clock_.AdvanceTimeMilliseconds(1);
  EXPECT_TRUE(hist_.GetPacketState(kStartSeqNum, /*verify_rtt=*/true));
  EXPECT_TRUE(hist_.GetPacketAndSetSendTime(kStartSeqNum, false));
}

TEST_F(RtpPacketHistoryTest, MinResendTimeWithoutPacer) {
  static const int64_t kMinRetransmitIntervalMs = 100;

  hist_.SetStorePacketsStatus(StorageMode::kStore, 10);
  hist_.SetRtt(kMinRetransmitIntervalMs);
  int64_t capture_time_ms = fake_clock_.TimeInMilliseconds();
  std::unique_ptr<RtpPacketToSend> packet = CreateRtpPacket(kStartSeqNum);
  size_t len = packet->size();
  hist_.PutRtpPacket(std::move(packet), kAllowRetransmission,
                     fake_clock_.TimeInMilliseconds());

  // First retransmission - allow early retransmission.
  fake_clock_.AdvanceTimeMilliseconds(1);
  packet = hist_.GetPacketAndSetSendTime(kStartSeqNum, true);
  EXPECT_TRUE(packet);
  EXPECT_EQ(len, packet->size());
  EXPECT_EQ(capture_time_ms, packet->capture_time_ms());

  // Second retransmission - advance time to just before retransmission OK.
  fake_clock_.AdvanceTimeMilliseconds(kMinRetransmitIntervalMs - 1);
  EXPECT_FALSE(hist_.GetPacketAndSetSendTime(kStartSeqNum, true));

  // Advance time to just after retransmission OK.
  fake_clock_.AdvanceTimeMilliseconds(1);
  EXPECT_TRUE(hist_.GetPacketAndSetSendTime(kStartSeqNum, true));
}

TEST_F(RtpPacketHistoryTest, RemovesOldestSentPacketWhenAtMaxSize) {
  const size_t kMaxNumPackets = 10;
  hist_.SetStorePacketsStatus(StorageMode::kStore, kMaxNumPackets);

  // History does not allow removing packets within kMinPacketDurationMs,
  // so in order to test capacity, make sure insertion spans this time.
  const int64_t kPacketIntervalMs =
      RtpPacketHistory::kMinPacketDurationMs / kMaxNumPackets;

  // Add packets until the buffer is full.
  for (size_t i = 0; i < kMaxNumPackets; ++i) {
    std::unique_ptr<RtpPacketToSend> packet = CreateRtpPacket(kStartSeqNum + i);
    // Immediate mark packet as sent.
    hist_.PutRtpPacket(std::move(packet), kAllowRetransmission,
                       fake_clock_.TimeInMilliseconds());
    fake_clock_.AdvanceTimeMilliseconds(kPacketIntervalMs);
  }

  // First packet should still be there.
  EXPECT_TRUE(hist_.GetPacketState(kStartSeqNum, false));

  // History is full, oldest one should be overwritten.
  std::unique_ptr<RtpPacketToSend> packet =
      CreateRtpPacket(To16u(kStartSeqNum + kMaxNumPackets));
  hist_.PutRtpPacket(std::move(packet), kAllowRetransmission,
                     fake_clock_.TimeInMilliseconds());

  // Oldest packet should be gone, but packet after than one still present.
  EXPECT_FALSE(hist_.GetPacketState(kStartSeqNum, false));
  EXPECT_TRUE(hist_.GetPacketState(kStartSeqNum + 1, false));
}

TEST_F(RtpPacketHistoryTest, RemovesOldestPacketWhenAtMaxCapacity) {
  // Tests the absolute upper bound on number of stored packets. Don't allow
  // storing more than this, even if packets have not yet been sent.
  const size_t kMaxNumPackets = RtpPacketHistory::kMaxCapacity;
  hist_.SetStorePacketsStatus(StorageMode::kStore,
                              RtpPacketHistory::kMaxCapacity);

  // Add packets until the buffer is full.
  for (size_t i = 0; i < kMaxNumPackets; ++i) {
    std::unique_ptr<RtpPacketToSend> packet = CreateRtpPacket(kStartSeqNum + i);
    // Don't mark packets as sent, preventing them from being removed.
    hist_.PutRtpPacket(std::move(packet), kAllowRetransmission, absl::nullopt);
  }

  // First packet should still be there.
  EXPECT_TRUE(hist_.GetPacketState(kStartSeqNum, false));

  // History is full, oldest one should be overwritten.
  std::unique_ptr<RtpPacketToSend> packet =
      CreateRtpPacket(To16u(kStartSeqNum + kMaxNumPackets));
  hist_.PutRtpPacket(std::move(packet), kAllowRetransmission,
                     fake_clock_.TimeInMilliseconds());

  // Oldest packet should be gone, but packet after than one still present.
  EXPECT_FALSE(hist_.GetPacketState(kStartSeqNum, false));
  EXPECT_TRUE(hist_.GetPacketState(kStartSeqNum + 1, false));
}

TEST_F(RtpPacketHistoryTest, DontRemoveUnsentPackets) {
  const size_t kMaxNumPackets = 10;
  hist_.SetStorePacketsStatus(StorageMode::kStore, kMaxNumPackets);

  // Add packets until the buffer is full.
  for (size_t i = 0; i < kMaxNumPackets; ++i) {
    // Mark packets as unsent.
    hist_.PutRtpPacket(CreateRtpPacket(To16u(kStartSeqNum + i)),
                       kAllowRetransmission, absl::nullopt);
  }
  fake_clock_.AdvanceTimeMilliseconds(RtpPacketHistory::kMinPacketDurationMs);

  // First packet should still be there.
  EXPECT_TRUE(hist_.GetPacketState(kStartSeqNum, false));

  // History is full, but old packets not sent, so allow expansion.
  hist_.PutRtpPacket(CreateRtpPacket(To16u(kStartSeqNum + kMaxNumPackets)),
                     kAllowRetransmission, fake_clock_.TimeInMilliseconds());
  EXPECT_TRUE(hist_.GetPacketState(kStartSeqNum, false));

  // Set all packet as sent and advance time past min packet duration time,
  // otherwise packets till still be prevented from being removed.
  for (size_t i = 0; i <= kMaxNumPackets; ++i) {
    EXPECT_TRUE(hist_.GetPacketAndSetSendTime(To16u(kStartSeqNum + i), false));
  }
  fake_clock_.AdvanceTimeMilliseconds(RtpPacketHistory::kMinPacketDurationMs);
  // Add a new packet, this means the two oldest ones will be culled.
  hist_.PutRtpPacket(CreateRtpPacket(To16u(kStartSeqNum + kMaxNumPackets + 1)),
                     kAllowRetransmission, fake_clock_.TimeInMilliseconds());
  EXPECT_FALSE(hist_.GetPacketState(kStartSeqNum, false));
  EXPECT_FALSE(hist_.GetPacketState(kStartSeqNum + 1, false));
  EXPECT_TRUE(hist_.GetPacketState(To16u(kStartSeqNum + 2), false));
}

TEST_F(RtpPacketHistoryTest, DontRemoveTooRecentlyTransmittedPackets) {
  // Set size to remove old packets as soon as possible.
  hist_.SetStorePacketsStatus(StorageMode::kStore, 1);

  // Add a packet, marked as send, and advance time to just before removal time.
  hist_.PutRtpPacket(CreateRtpPacket(kStartSeqNum), kAllowRetransmission,
                     fake_clock_.TimeInMilliseconds());
  fake_clock_.AdvanceTimeMilliseconds(RtpPacketHistory::kMinPacketDurationMs -
                                      1);

  // Add a new packet to trigger culling.
  hist_.PutRtpPacket(CreateRtpPacket(kStartSeqNum + 1), kAllowRetransmission,
                     fake_clock_.TimeInMilliseconds());
  // First packet should still be there.
  EXPECT_TRUE(hist_.GetPacketState(kStartSeqNum, false));

  // Advance time to where packet will be eligible for removal and try again.
  fake_clock_.AdvanceTimeMilliseconds(1);
  hist_.PutRtpPacket(CreateRtpPacket(To16u(kStartSeqNum + 2)),
                     kAllowRetransmission, fake_clock_.TimeInMilliseconds());
  // First packet should no be gone, but next one still there.
  EXPECT_FALSE(hist_.GetPacketState(kStartSeqNum, false));
  EXPECT_TRUE(hist_.GetPacketState(kStartSeqNum + 1, false));
}

TEST_F(RtpPacketHistoryTest, DontRemoveTooRecentlyTransmittedPacketsHighRtt) {
  const int64_t kRttMs = RtpPacketHistory::kMinPacketDurationMs * 2;
  const int64_t kPacketTimeoutMs =
      kRttMs * RtpPacketHistory::kMinPacketDurationRtt;

  // Set size to remove old packets as soon as possible.
  hist_.SetStorePacketsStatus(StorageMode::kStore, 1);
  hist_.SetRtt(kRttMs);

  // Add a packet, marked as send, and advance time to just before removal time.
  hist_.PutRtpPacket(CreateRtpPacket(kStartSeqNum), kAllowRetransmission,
                     fake_clock_.TimeInMilliseconds());
  fake_clock_.AdvanceTimeMilliseconds(kPacketTimeoutMs - 1);

  // Add a new packet to trigger culling.
  hist_.PutRtpPacket(CreateRtpPacket(kStartSeqNum + 1), kAllowRetransmission,
                     fake_clock_.TimeInMilliseconds());
  // First packet should still be there.
  EXPECT_TRUE(hist_.GetPacketState(kStartSeqNum, false));

  // Advance time to where packet will be eligible for removal and try again.
  fake_clock_.AdvanceTimeMilliseconds(1);
  hist_.PutRtpPacket(CreateRtpPacket(To16u(kStartSeqNum + 2)),
                     kAllowRetransmission, fake_clock_.TimeInMilliseconds());
  // First packet should no be gone, but next one still there.
  EXPECT_FALSE(hist_.GetPacketState(kStartSeqNum, false));
  EXPECT_TRUE(hist_.GetPacketState(kStartSeqNum + 1, false));
}

TEST_F(RtpPacketHistoryTest, RemovesOldWithCulling) {
  const size_t kMaxNumPackets = 10;
  // Enable culling. Even without feedback, this can trigger early removal.
  hist_.SetStorePacketsStatus(StorageMode::kStoreAndCull, kMaxNumPackets);

  hist_.PutRtpPacket(CreateRtpPacket(kStartSeqNum), kAllowRetransmission,
                     fake_clock_.TimeInMilliseconds());

  int64_t kMaxPacketDurationMs = RtpPacketHistory::kMinPacketDurationMs *
                                 RtpPacketHistory::kPacketCullingDelayFactor;
  fake_clock_.AdvanceTimeMilliseconds(kMaxPacketDurationMs - 1);

  // First packet should still be there.
  EXPECT_TRUE(hist_.GetPacketState(kStartSeqNum, false));

  // Advance to where packet can be culled, even if buffer is not full.
  fake_clock_.AdvanceTimeMilliseconds(1);
  hist_.PutRtpPacket(CreateRtpPacket(kStartSeqNum + 1), kAllowRetransmission,
                     fake_clock_.TimeInMilliseconds());

  EXPECT_FALSE(hist_.GetPacketState(kStartSeqNum, false));
}

TEST_F(RtpPacketHistoryTest, RemovesOldWithCullingHighRtt) {
  const size_t kMaxNumPackets = 10;
  const int64_t kRttMs = RtpPacketHistory::kMinPacketDurationMs * 2;
  // Enable culling. Even without feedback, this can trigger early removal.
  hist_.SetStorePacketsStatus(StorageMode::kStoreAndCull, kMaxNumPackets);
  hist_.SetRtt(kRttMs);

  hist_.PutRtpPacket(CreateRtpPacket(kStartSeqNum), kAllowRetransmission,
                     fake_clock_.TimeInMilliseconds());

  int64_t kMaxPacketDurationMs = kRttMs *
                                 RtpPacketHistory::kMinPacketDurationRtt *
                                 RtpPacketHistory::kPacketCullingDelayFactor;
  fake_clock_.AdvanceTimeMilliseconds(kMaxPacketDurationMs - 1);

  // First packet should still be there.
  EXPECT_TRUE(hist_.GetPacketState(kStartSeqNum, false));

  // Advance to where packet can be culled, even if buffer is not full.
  fake_clock_.AdvanceTimeMilliseconds(1);
  hist_.PutRtpPacket(CreateRtpPacket(kStartSeqNum + 1), kAllowRetransmission,
                     fake_clock_.TimeInMilliseconds());

  EXPECT_FALSE(hist_.GetPacketState(kStartSeqNum, false));
}

TEST_F(RtpPacketHistoryTest, GetBestFittingPacket) {
  const size_t kTargetSize = 500;
  hist_.SetStorePacketsStatus(StorageMode::kStore, 10);

  // Add three packets of various sizes.
  std::unique_ptr<RtpPacketToSend> packet = CreateRtpPacket(kStartSeqNum);
  packet->SetPayloadSize(kTargetSize);
  const size_t target_packet_size = packet->size();
  hist_.PutRtpPacket(std::move(packet), kAllowRetransmission,
                     fake_clock_.TimeInMilliseconds());
  packet = CreateRtpPacket(kStartSeqNum + 1);
  packet->SetPayloadSize(kTargetSize - 1);
  hist_.PutRtpPacket(std::move(packet), kAllowRetransmission,
                     fake_clock_.TimeInMilliseconds());
  packet = CreateRtpPacket(To16u(kStartSeqNum + 2));
  packet->SetPayloadSize(kTargetSize + 1);
  hist_.PutRtpPacket(std::move(packet), kAllowRetransmission,
                     fake_clock_.TimeInMilliseconds());

  EXPECT_EQ(target_packet_size,
            hist_.GetBestFittingPacket(target_packet_size)->size());
}

TEST_F(RtpPacketHistoryTest,
       GetBestFittingPacketReturnsNextPacketWhenBestPacketHasBeenCulled) {
  hist_.SetStorePacketsStatus(StorageMode::kStoreAndCull, 10);
  std::unique_ptr<RtpPacketToSend> packet = CreateRtpPacket(kStartSeqNum);
  packet->SetPayloadSize(50);
  const size_t target_packet_size = packet->size();
  hist_.PutRtpPacket(std::move(packet), kAllowRetransmission,
                     fake_clock_.TimeInMilliseconds());

  packet = hist_.GetBestFittingPacket(target_packet_size + 2);
  ASSERT_THAT(packet, ::testing::NotNull());

  // Send the packet and advance time past where packet expires.
  ASSERT_THAT(hist_.GetPacketAndSetSendTime(kStartSeqNum, false),
              ::testing::NotNull());
  fake_clock_.AdvanceTimeMilliseconds(
      RtpPacketHistory::kPacketCullingDelayFactor *
      RtpPacketHistory::kMinPacketDurationMs);

  packet = CreateRtpPacket(kStartSeqNum + 1);
  packet->SetPayloadSize(100);
  hist_.PutRtpPacket(std::move(packet), kAllowRetransmission,
                     fake_clock_.TimeInMilliseconds());
  ASSERT_FALSE(hist_.GetPacketState(kStartSeqNum, false));

  auto best_packet = hist_.GetBestFittingPacket(target_packet_size + 2);
  ASSERT_THAT(best_packet, ::testing::NotNull());
  EXPECT_EQ(best_packet->SequenceNumber(), kStartSeqNum + 1);
}

TEST_F(RtpPacketHistoryTest, GetBestFittingPacketReturnLastPacketWhenSameSize) {
  const size_t kTargetSize = 500;
  hist_.SetStorePacketsStatus(StorageMode::kStore, 10);

  // Add two packets of same size.
  std::unique_ptr<RtpPacketToSend> packet = CreateRtpPacket(kStartSeqNum);
  packet->SetPayloadSize(kTargetSize);
  hist_.PutRtpPacket(std::move(packet), kAllowRetransmission,
                     fake_clock_.TimeInMilliseconds());
  packet = CreateRtpPacket(kStartSeqNum + 1);
  packet->SetPayloadSize(kTargetSize);
  hist_.PutRtpPacket(std::move(packet), kAllowRetransmission,
                     fake_clock_.TimeInMilliseconds());

  auto best_packet = hist_.GetBestFittingPacket(123);
  ASSERT_THAT(best_packet, ::testing::NotNull());
  EXPECT_EQ(best_packet->SequenceNumber(), kStartSeqNum + 1);
}

TEST_F(RtpPacketHistoryTest,
       GetBestFittingPacketReturnsPacketWithSmallestDiff) {
  const size_t kTargetSize = 500;
  hist_.SetStorePacketsStatus(StorageMode::kStore, 10);

  // Add two packets of very different size.
  std::unique_ptr<RtpPacketToSend> small_packet = CreateRtpPacket(kStartSeqNum);
  small_packet->SetPayloadSize(kTargetSize);
  hist_.PutRtpPacket(std::move(small_packet), kAllowRetransmission,
                     fake_clock_.TimeInMilliseconds());

  auto large_packet = CreateRtpPacket(kStartSeqNum + 1);
  large_packet->SetPayloadSize(kTargetSize * 2);
  hist_.PutRtpPacket(std::move(large_packet), kAllowRetransmission,
                     fake_clock_.TimeInMilliseconds());

  ASSERT_THAT(hist_.GetBestFittingPacket(kTargetSize), ::testing::NotNull());
  EXPECT_EQ(hist_.GetBestFittingPacket(kTargetSize)->SequenceNumber(),
            kStartSeqNum);

  ASSERT_THAT(hist_.GetBestFittingPacket(kTargetSize * 2),
              ::testing::NotNull());
  EXPECT_EQ(hist_.GetBestFittingPacket(kTargetSize * 2)->SequenceNumber(),
            kStartSeqNum + 1);
}

TEST_F(RtpPacketHistoryTest,
       GetBestFittingPacketIgnoresNoneRetransmitablePackets) {
  hist_.SetStorePacketsStatus(StorageMode::kStoreAndCull, 10);
  std::unique_ptr<RtpPacketToSend> packet = CreateRtpPacket(kStartSeqNum);
  packet->SetPayloadSize(50);
  hist_.PutRtpPacket(std::move(packet), kDontRetransmit,
                     fake_clock_.TimeInMilliseconds());
  EXPECT_THAT(hist_.GetBestFittingPacket(50), ::testing::IsNull());
  EXPECT_THAT(hist_.GetPacketAndSetSendTime(kStartSeqNum, false),
              ::testing::NotNull());
}

}  // namespace webrtc
