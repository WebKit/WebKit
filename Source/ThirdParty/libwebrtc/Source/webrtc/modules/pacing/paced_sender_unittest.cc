/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <list>
#include <memory>
#include <string>

#include "modules/pacing/paced_sender.h"
#include "system_wrappers/include/clock.h"
#include "system_wrappers/include/field_trial.h"
#include "test/field_trial.h"
#include "test/gmock.h"
#include "test/gtest.h"

using testing::_;
using testing::Field;
using testing::Return;

namespace {
constexpr unsigned kFirstClusterBps = 900000;
constexpr unsigned kSecondClusterBps = 1800000;

// The error stems from truncating the time interval of probe packets to integer
// values. This results in probing slightly higher than the target bitrate.
// For 1.8 Mbps, this comes to be about 120 kbps with 1200 probe packets.
constexpr int kBitrateProbingError = 150000;

const float kPaceMultiplier = 2.5f;
}  // namespace

namespace webrtc {
namespace test {

static const int kTargetBitrateBps = 800000;

class MockPacedSenderCallback : public PacedSender::PacketSender {
 public:
  MOCK_METHOD5(TimeToSendPacket,
               bool(uint32_t ssrc,
                    uint16_t sequence_number,
                    int64_t capture_time_ms,
                    bool retransmission,
                    const PacedPacketInfo& pacing_info));
  MOCK_METHOD2(TimeToSendPadding,
               size_t(size_t bytes, const PacedPacketInfo& pacing_info));
};

class PacedSenderPadding : public PacedSender::PacketSender {
 public:
  PacedSenderPadding() : padding_sent_(0) {}

  bool TimeToSendPacket(uint32_t ssrc,
                        uint16_t sequence_number,
                        int64_t capture_time_ms,
                        bool retransmission,
                        const PacedPacketInfo& pacing_info) override {
    return true;
  }

  size_t TimeToSendPadding(size_t bytes,
                           const PacedPacketInfo& pacing_info) override {
    const size_t kPaddingPacketSize = 224;
    size_t num_packets = (bytes + kPaddingPacketSize - 1) / kPaddingPacketSize;
    padding_sent_ += kPaddingPacketSize * num_packets;
    return kPaddingPacketSize * num_packets;
  }

  size_t padding_sent() { return padding_sent_; }

 private:
  size_t padding_sent_;
};

class PacedSenderProbing : public PacedSender::PacketSender {
 public:
  PacedSenderProbing() : packets_sent_(0), padding_sent_(0) {}

  bool TimeToSendPacket(uint32_t ssrc,
                        uint16_t sequence_number,
                        int64_t capture_time_ms,
                        bool retransmission,
                        const PacedPacketInfo& pacing_info) override {
    packets_sent_++;
    return true;
  }

  size_t TimeToSendPadding(size_t bytes,
                           const PacedPacketInfo& pacing_info) override {
    padding_sent_ += bytes;
    return padding_sent_;
  }

  int packets_sent() const { return packets_sent_; }

  int padding_sent() const { return padding_sent_; }

 private:
  int packets_sent_;
  int padding_sent_;
};

class PacedSenderTest : public testing::TestWithParam<std::string> {
 protected:
  PacedSenderTest() : clock_(123456) {
    srand(0);
    // Need to initialize PacedSender after we initialize clock.
    send_bucket_.reset(new PacedSender(&clock_, &callback_, nullptr));
    send_bucket_->CreateProbeCluster(kFirstClusterBps);
    send_bucket_->CreateProbeCluster(kSecondClusterBps);
    // Default to bitrate probing disabled for testing purposes. Probing tests
    // have to enable probing, either by creating a new PacedSender instance or
    // by calling SetProbingEnabled(true).
    send_bucket_->SetProbingEnabled(false);
    send_bucket_->SetPacingRates(kTargetBitrateBps * kPaceMultiplier, 0);

    clock_.AdvanceTimeMilliseconds(send_bucket_->TimeUntilNextProcess());
  }

  void SendAndExpectPacket(PacedSender::Priority priority,
                           uint32_t ssrc,
                           uint16_t sequence_number,
                           int64_t capture_time_ms,
                           size_t size,
                           bool retransmission) {
    send_bucket_->InsertPacket(priority, ssrc, sequence_number, capture_time_ms,
                               size, retransmission);
    EXPECT_CALL(callback_, TimeToSendPacket(ssrc, sequence_number,
                                            capture_time_ms, retransmission, _))
        .Times(1)
        .WillRepeatedly(Return(true));
  }
  SimulatedClock clock_;
  MockPacedSenderCallback callback_;
  std::unique_ptr<PacedSender> send_bucket_;
};

class PacedSenderFieldTrialTest : public testing::Test {
 protected:
  struct MediaStream {
    const RtpPacketSender::Priority priority;
    const uint32_t ssrc;
    const size_t packet_size;
    uint16_t seq_num;
  };

  const int kProcessIntervalsPerSecond = 1000 / 5;

  PacedSenderFieldTrialTest() : clock_(123456) {}
  void InsertPacket(PacedSender* pacer, MediaStream* stream) {
    pacer->InsertPacket(stream->priority, stream->ssrc, stream->seq_num++,
                        clock_.TimeInMilliseconds(), stream->packet_size,
                        false);
  }
  void ProcessNext(PacedSender* pacer) {
    clock_.AdvanceTimeMilliseconds(5);
    pacer->Process();
  }
  MediaStream audio{/*priority*/ PacedSender::kHighPriority,
                    /*ssrc*/ 3333, /*packet_size*/ 100, /*seq_num*/ 1000};
  MediaStream video{/*priority*/ PacedSender::kNormalPriority,
                    /*ssrc*/ 4444, /*packet_size*/ 1000, /*seq_num*/ 1000};
  SimulatedClock clock_;
  MockPacedSenderCallback callback_;
};

TEST_F(PacedSenderFieldTrialTest, DefaultNoPaddingInSilence) {
  PacedSender pacer(&clock_, &callback_, nullptr);
  pacer.SetPacingRates(kTargetBitrateBps, 0);
  // Video packet to reset last send time and provide padding data.
  InsertPacket(&pacer, &video);
  EXPECT_CALL(callback_, TimeToSendPacket).WillOnce(Return(true));
  clock_.AdvanceTimeMilliseconds(5);
  pacer.Process();
  EXPECT_CALL(callback_, TimeToSendPadding).Times(0);
  // Waiting 500 ms should not trigger sending of padding.
  clock_.AdvanceTimeMilliseconds(500);
  pacer.Process();
}

TEST_F(PacedSenderFieldTrialTest, PaddingInSilenceWithTrial) {
  ScopedFieldTrials trial("WebRTC-Pacer-PadInSilence/Enabled/");
  PacedSender pacer(&clock_, &callback_, nullptr);
  pacer.SetPacingRates(kTargetBitrateBps, 0);
  // Video packet to reset last send time and provide padding data.
  InsertPacket(&pacer, &video);
  EXPECT_CALL(callback_, TimeToSendPacket).WillOnce(Return(true));
  clock_.AdvanceTimeMilliseconds(5);
  pacer.Process();
  EXPECT_CALL(callback_, TimeToSendPadding).WillOnce(Return(1000));
  // Waiting 500 ms should trigger sending of padding.
  clock_.AdvanceTimeMilliseconds(500);
  pacer.Process();
}

TEST_F(PacedSenderFieldTrialTest, DefaultCongestionWindowAffectsAudio) {
  EXPECT_CALL(callback_, TimeToSendPadding).Times(0);
  PacedSender pacer(&clock_, &callback_, nullptr);
  pacer.SetPacingRates(10000000, 0);
  pacer.SetCongestionWindow(800);
  pacer.UpdateOutstandingData(0);
  // Video packet fills congestion window.
  InsertPacket(&pacer, &video);
  EXPECT_CALL(callback_, TimeToSendPacket).WillOnce(Return(true));
  ProcessNext(&pacer);
  // Audio packet blocked due to congestion.
  InsertPacket(&pacer, &audio);
  EXPECT_CALL(callback_, TimeToSendPacket).Times(0);
  ProcessNext(&pacer);
  ProcessNext(&pacer);
  // Audio packet unblocked when congestion window clear.
  testing::Mock::VerifyAndClearExpectations(&callback_);
  pacer.UpdateOutstandingData(0);
  EXPECT_CALL(callback_, TimeToSendPacket).WillOnce(Return(true));
  ProcessNext(&pacer);
}

TEST_F(PacedSenderFieldTrialTest, CongestionWindowDoesNotAffectAudioInTrial) {
  ScopedFieldTrials trial("WebRTC-Pacer-BlockAudio/Disabled/");
  EXPECT_CALL(callback_, TimeToSendPadding).Times(0);
  PacedSender pacer(&clock_, &callback_, nullptr);
  pacer.SetPacingRates(10000000, 0);
  pacer.SetCongestionWindow(800);
  pacer.UpdateOutstandingData(0);
  // Video packet fills congestion window.
  InsertPacket(&pacer, &video);
  EXPECT_CALL(callback_, TimeToSendPacket).WillOnce(Return(true));
  ProcessNext(&pacer);
  // Audio not blocked due to congestion.
  InsertPacket(&pacer, &audio);
  EXPECT_CALL(callback_, TimeToSendPacket).WillOnce(Return(true));
  ProcessNext(&pacer);
}

TEST_F(PacedSenderFieldTrialTest, DefaultBudgetAffectsAudio) {
  PacedSender pacer(&clock_, &callback_, nullptr);
  pacer.SetPacingRates(video.packet_size / 3 * 8 * kProcessIntervalsPerSecond,
                       0);
  // Video fills budget for following process periods.
  InsertPacket(&pacer, &video);
  EXPECT_CALL(callback_, TimeToSendPacket).WillOnce(Return(true));
  ProcessNext(&pacer);
  // Audio packet blocked due to budget limit.
  EXPECT_CALL(callback_, TimeToSendPacket).Times(0);
  InsertPacket(&pacer, &audio);
  ProcessNext(&pacer);
  ProcessNext(&pacer);
  testing::Mock::VerifyAndClearExpectations(&callback_);
  // Audio packet unblocked when the budget has recovered.
  EXPECT_CALL(callback_, TimeToSendPacket).WillOnce(Return(true));
  ProcessNext(&pacer);
  ProcessNext(&pacer);
}

TEST_F(PacedSenderFieldTrialTest, BudgetDoesNotAffectAudioInTrial) {
  ScopedFieldTrials trial("WebRTC-Pacer-BlockAudio/Disabled/");
  EXPECT_CALL(callback_, TimeToSendPadding).Times(0);
  PacedSender pacer(&clock_, &callback_, nullptr);
  pacer.SetPacingRates(video.packet_size / 3 * 8 * kProcessIntervalsPerSecond,
                       0);
  // Video fills budget for following process periods.
  InsertPacket(&pacer, &video);
  EXPECT_CALL(callback_, TimeToSendPacket).WillOnce(Return(true));
  ProcessNext(&pacer);
  // Audio packet not blocked due to budget limit.
  EXPECT_CALL(callback_, TimeToSendPacket).WillOnce(Return(true));
  InsertPacket(&pacer, &audio);
  ProcessNext(&pacer);
}

TEST_F(PacedSenderTest, FirstSentPacketTimeIsSet) {
  uint16_t sequence_number = 1234;
  const uint32_t kSsrc = 12345;
  const size_t kSizeBytes = 250;
  const size_t kPacketToSend = 3;
  const int64_t kStartMs = clock_.TimeInMilliseconds();

  // No packet sent.
  EXPECT_EQ(-1, send_bucket_->FirstSentPacketTimeMs());

  for (size_t i = 0; i < kPacketToSend; ++i) {
    SendAndExpectPacket(PacedSender::kNormalPriority, kSsrc, sequence_number++,
                        clock_.TimeInMilliseconds(), kSizeBytes, false);
    send_bucket_->Process();
    clock_.AdvanceTimeMilliseconds(send_bucket_->TimeUntilNextProcess());
  }
  EXPECT_EQ(kStartMs, send_bucket_->FirstSentPacketTimeMs());
}

TEST_F(PacedSenderTest, QueuePacket) {
  uint32_t ssrc = 12345;
  uint16_t sequence_number = 1234;
  // Due to the multiplicative factor we can send 5 packets during a send
  // interval. (network capacity * multiplier / (8 bits per byte *
  // (packet size * #send intervals per second)
  const size_t packets_to_send =
      kTargetBitrateBps * kPaceMultiplier / (8 * 250 * 200);
  for (size_t i = 0; i < packets_to_send; ++i) {
    SendAndExpectPacket(PacedSender::kNormalPriority, ssrc, sequence_number++,
                        clock_.TimeInMilliseconds(), 250, false);
  }

  int64_t queued_packet_timestamp = clock_.TimeInMilliseconds();
  send_bucket_->InsertPacket(PacedSender::kNormalPriority, ssrc,
                             sequence_number, queued_packet_timestamp, 250,
                             false);
  EXPECT_EQ(packets_to_send + 1, send_bucket_->QueueSizePackets());
  send_bucket_->Process();
  EXPECT_EQ(5, send_bucket_->TimeUntilNextProcess());
  EXPECT_CALL(callback_, TimeToSendPadding(_, _)).Times(0);
  clock_.AdvanceTimeMilliseconds(4);
  EXPECT_EQ(1, send_bucket_->TimeUntilNextProcess());
  clock_.AdvanceTimeMilliseconds(1);
  EXPECT_EQ(0, send_bucket_->TimeUntilNextProcess());
  EXPECT_EQ(1u, send_bucket_->QueueSizePackets());
  EXPECT_CALL(callback_, TimeToSendPacket(ssrc, sequence_number++,
                                          queued_packet_timestamp, false, _))
      .Times(1)
      .WillRepeatedly(Return(true));
  send_bucket_->Process();
  sequence_number++;
  EXPECT_EQ(0u, send_bucket_->QueueSizePackets());

  // We can send packets_to_send -1 packets of size 250 during the current
  // interval since one packet has already been sent.
  for (size_t i = 0; i < packets_to_send - 1; ++i) {
    SendAndExpectPacket(PacedSender::kNormalPriority, ssrc, sequence_number++,
                        clock_.TimeInMilliseconds(), 250, false);
  }
  send_bucket_->InsertPacket(PacedSender::kNormalPriority, ssrc,
                             sequence_number++, clock_.TimeInMilliseconds(),
                             250, false);
  EXPECT_EQ(packets_to_send, send_bucket_->QueueSizePackets());
  send_bucket_->Process();
  EXPECT_EQ(1u, send_bucket_->QueueSizePackets());
}

TEST_F(PacedSenderTest, PaceQueuedPackets) {
  uint32_t ssrc = 12345;
  uint16_t sequence_number = 1234;

  // Due to the multiplicative factor we can send 5 packets during a send
  // interval. (network capacity * multiplier / (8 bits per byte *
  // (packet size * #send intervals per second)
  const size_t packets_to_send_per_interval =
      kTargetBitrateBps * kPaceMultiplier / (8 * 250 * 200);
  for (size_t i = 0; i < packets_to_send_per_interval; ++i) {
    SendAndExpectPacket(PacedSender::kNormalPriority, ssrc, sequence_number++,
                        clock_.TimeInMilliseconds(), 250, false);
  }

  for (size_t j = 0; j < packets_to_send_per_interval * 10; ++j) {
    send_bucket_->InsertPacket(PacedSender::kNormalPriority, ssrc,
                               sequence_number++, clock_.TimeInMilliseconds(),
                               250, false);
  }
  EXPECT_EQ(packets_to_send_per_interval + packets_to_send_per_interval * 10,
            send_bucket_->QueueSizePackets());
  send_bucket_->Process();
  EXPECT_EQ(packets_to_send_per_interval * 10,
            send_bucket_->QueueSizePackets());
  EXPECT_CALL(callback_, TimeToSendPadding(_, _)).Times(0);
  for (int k = 0; k < 10; ++k) {
    EXPECT_EQ(5, send_bucket_->TimeUntilNextProcess());
    clock_.AdvanceTimeMilliseconds(5);
    EXPECT_CALL(callback_, TimeToSendPacket(ssrc, _, _, false, _))
        .Times(packets_to_send_per_interval)
        .WillRepeatedly(Return(true));
    EXPECT_EQ(0, send_bucket_->TimeUntilNextProcess());
    send_bucket_->Process();
  }
  EXPECT_EQ(0u, send_bucket_->QueueSizePackets());
  EXPECT_EQ(5, send_bucket_->TimeUntilNextProcess());
  clock_.AdvanceTimeMilliseconds(5);
  EXPECT_EQ(0, send_bucket_->TimeUntilNextProcess());
  EXPECT_EQ(0u, send_bucket_->QueueSizePackets());
  send_bucket_->Process();

  for (size_t i = 0; i < packets_to_send_per_interval; ++i) {
    SendAndExpectPacket(PacedSender::kNormalPriority, ssrc, sequence_number++,
                        clock_.TimeInMilliseconds(), 250, false);
  }
  send_bucket_->InsertPacket(PacedSender::kNormalPriority, ssrc,
                             sequence_number, clock_.TimeInMilliseconds(), 250,
                             false);
  send_bucket_->Process();
  EXPECT_EQ(1u, send_bucket_->QueueSizePackets());
}

TEST_F(PacedSenderTest, RepeatedRetransmissionsAllowed) {
  // Send one packet, then two retransmissions of that packet.
  for (size_t i = 0; i < 3; i++) {
    constexpr uint32_t ssrc = 333;
    constexpr uint16_t sequence_number = 444;
    constexpr size_t bytes = 250;
    bool is_retransmission = (i != 0);  // Original followed by retransmissions.
    SendAndExpectPacket(PacedSender::kNormalPriority, ssrc, sequence_number,
                        clock_.TimeInMilliseconds(), bytes, is_retransmission);
    clock_.AdvanceTimeMilliseconds(5);
  }
  send_bucket_->Process();
}

TEST_F(PacedSenderTest, CanQueuePacketsWithSameSequenceNumberOnDifferentSsrcs) {
  uint32_t ssrc = 12345;
  uint16_t sequence_number = 1234;

  SendAndExpectPacket(PacedSender::kNormalPriority, ssrc, sequence_number,
                      clock_.TimeInMilliseconds(), 250, false);

  // Expect packet on second ssrc to be queued and sent as well.
  SendAndExpectPacket(PacedSender::kNormalPriority, ssrc + 1, sequence_number,
                      clock_.TimeInMilliseconds(), 250, false);

  clock_.AdvanceTimeMilliseconds(1000);
  send_bucket_->Process();
}

TEST_F(PacedSenderTest, Padding) {
  uint32_t ssrc = 12345;
  uint16_t sequence_number = 1234;

  send_bucket_->SetPacingRates(kTargetBitrateBps * kPaceMultiplier,
                               kTargetBitrateBps);

  // Due to the multiplicative factor we can send 5 packets during a send
  // interval. (network capacity * multiplier / (8 bits per byte *
  // (packet size * #send intervals per second)
  const size_t packets_to_send_per_interval =
      kTargetBitrateBps * kPaceMultiplier / (8 * 250 * 200);
  for (size_t i = 0; i < packets_to_send_per_interval; ++i) {
    SendAndExpectPacket(PacedSender::kNormalPriority, ssrc, sequence_number++,
                        clock_.TimeInMilliseconds(), 250, false);
  }
  // No padding is expected since we have sent too much already.
  EXPECT_CALL(callback_, TimeToSendPadding(_, _)).Times(0);
  EXPECT_EQ(0, send_bucket_->TimeUntilNextProcess());
  send_bucket_->Process();
  EXPECT_EQ(0u, send_bucket_->QueueSizePackets());

  // 5 milliseconds later should not send padding since we filled the buffers
  // initially.
  EXPECT_CALL(callback_, TimeToSendPadding(250, _)).Times(0);
  EXPECT_EQ(5, send_bucket_->TimeUntilNextProcess());
  clock_.AdvanceTimeMilliseconds(5);
  EXPECT_EQ(0, send_bucket_->TimeUntilNextProcess());
  send_bucket_->Process();

  // 5 milliseconds later we have enough budget to send some padding.
  EXPECT_CALL(callback_, TimeToSendPadding(250, _))
      .Times(1)
      .WillOnce(Return(250));
  EXPECT_EQ(5, send_bucket_->TimeUntilNextProcess());
  clock_.AdvanceTimeMilliseconds(5);
  EXPECT_EQ(0, send_bucket_->TimeUntilNextProcess());
  send_bucket_->Process();
}

TEST_F(PacedSenderTest, NoPaddingBeforeNormalPacket) {
  send_bucket_->SetPacingRates(kTargetBitrateBps * kPaceMultiplier,
                               kTargetBitrateBps);

  EXPECT_CALL(callback_, TimeToSendPadding(_, _)).Times(0);
  send_bucket_->Process();
  clock_.AdvanceTimeMilliseconds(send_bucket_->TimeUntilNextProcess());

  send_bucket_->Process();
  clock_.AdvanceTimeMilliseconds(send_bucket_->TimeUntilNextProcess());

  uint32_t ssrc = 12345;
  uint16_t sequence_number = 1234;
  int64_t capture_time_ms = 56789;

  SendAndExpectPacket(PacedSender::kNormalPriority, ssrc, sequence_number++,
                      capture_time_ms, 250, false);
  EXPECT_CALL(callback_, TimeToSendPadding(250, _))
      .Times(1)
      .WillOnce(Return(250));
  send_bucket_->Process();
}

TEST_F(PacedSenderTest, VerifyPaddingUpToBitrate) {
  uint32_t ssrc = 12345;
  uint16_t sequence_number = 1234;
  int64_t capture_time_ms = 56789;
  const int kTimeStep = 5;
  const int64_t kBitrateWindow = 100;
  send_bucket_->SetPacingRates(kTargetBitrateBps * kPaceMultiplier,
                               kTargetBitrateBps);

  int64_t start_time = clock_.TimeInMilliseconds();
  while (clock_.TimeInMilliseconds() - start_time < kBitrateWindow) {
    SendAndExpectPacket(PacedSender::kNormalPriority, ssrc, sequence_number++,
                        capture_time_ms, 250, false);
    EXPECT_CALL(callback_, TimeToSendPadding(250, _))
        .Times(1)
        .WillOnce(Return(250));
    send_bucket_->Process();
    clock_.AdvanceTimeMilliseconds(kTimeStep);
  }
}

TEST_F(PacedSenderTest, VerifyAverageBitrateVaryingMediaPayload) {
  uint32_t ssrc = 12345;
  uint16_t sequence_number = 1234;
  int64_t capture_time_ms = 56789;
  const int kTimeStep = 5;
  const int64_t kBitrateWindow = 10000;
  PacedSenderPadding callback;
  send_bucket_.reset(new PacedSender(&clock_, &callback, nullptr));
  send_bucket_->SetProbingEnabled(false);
  send_bucket_->SetPacingRates(kTargetBitrateBps * kPaceMultiplier,
                               kTargetBitrateBps);

  int64_t start_time = clock_.TimeInMilliseconds();
  size_t media_bytes = 0;
  while (clock_.TimeInMilliseconds() - start_time < kBitrateWindow) {
    int rand_value = rand();  // NOLINT (rand_r instead of rand)
    size_t media_payload = rand_value % 100 + 200;  // [200, 300] bytes.
    send_bucket_->InsertPacket(PacedSender::kNormalPriority, ssrc,
                               sequence_number++, capture_time_ms,
                               media_payload, false);
    media_bytes += media_payload;
    clock_.AdvanceTimeMilliseconds(kTimeStep);
    send_bucket_->Process();
  }
  EXPECT_NEAR(kTargetBitrateBps / 1000,
              static_cast<int>(8 * (media_bytes + callback.padding_sent()) /
                               kBitrateWindow),
              1);
}

TEST_F(PacedSenderTest, Priority) {
  uint32_t ssrc_low_priority = 12345;
  uint32_t ssrc = 12346;
  uint16_t sequence_number = 1234;
  int64_t capture_time_ms = 56789;
  int64_t capture_time_ms_low_priority = 1234567;

  // Due to the multiplicative factor we can send 5 packets during a send
  // interval. (network capacity * multiplier / (8 bits per byte *
  // (packet size * #send intervals per second)
  const size_t packets_to_send_per_interval =
      kTargetBitrateBps * kPaceMultiplier / (8 * 250 * 200);
  for (size_t i = 0; i < packets_to_send_per_interval; ++i) {
    SendAndExpectPacket(PacedSender::kNormalPriority, ssrc, sequence_number++,
                        clock_.TimeInMilliseconds(), 250, false);
  }
  send_bucket_->Process();
  EXPECT_EQ(0u, send_bucket_->QueueSizePackets());

  // Expect normal and low priority to be queued and high to pass through.
  send_bucket_->InsertPacket(PacedSender::kLowPriority, ssrc_low_priority,
                             sequence_number++, capture_time_ms_low_priority,
                             250, false);

  for (size_t i = 0; i < packets_to_send_per_interval; ++i) {
    send_bucket_->InsertPacket(PacedSender::kNormalPriority, ssrc,
                               sequence_number++, capture_time_ms, 250, false);
  }
  send_bucket_->InsertPacket(PacedSender::kHighPriority, ssrc,
                             sequence_number++, capture_time_ms, 250, false);

  // Expect all high and normal priority to be sent out first.
  EXPECT_CALL(callback_, TimeToSendPadding(_, _)).Times(0);
  EXPECT_CALL(callback_, TimeToSendPacket(ssrc, _, capture_time_ms, false, _))
      .Times(packets_to_send_per_interval + 1)
      .WillRepeatedly(Return(true));

  EXPECT_EQ(5, send_bucket_->TimeUntilNextProcess());
  clock_.AdvanceTimeMilliseconds(5);
  EXPECT_EQ(0, send_bucket_->TimeUntilNextProcess());
  send_bucket_->Process();
  EXPECT_EQ(1u, send_bucket_->QueueSizePackets());

  EXPECT_CALL(callback_,
              TimeToSendPacket(ssrc_low_priority, _,
                               capture_time_ms_low_priority, false, _))
      .Times(1)
      .WillRepeatedly(Return(true));

  EXPECT_EQ(5, send_bucket_->TimeUntilNextProcess());
  clock_.AdvanceTimeMilliseconds(5);
  EXPECT_EQ(0, send_bucket_->TimeUntilNextProcess());
  send_bucket_->Process();
}

TEST_F(PacedSenderTest, RetransmissionPriority) {
  uint32_t ssrc = 12345;
  uint16_t sequence_number = 1234;
  int64_t capture_time_ms = 45678;
  int64_t capture_time_ms_retransmission = 56789;

  // Due to the multiplicative factor we can send 5 packets during a send
  // interval. (network capacity * multiplier / (8 bits per byte *
  // (packet size * #send intervals per second)
  const size_t packets_to_send_per_interval =
      kTargetBitrateBps * kPaceMultiplier / (8 * 250 * 200);
  send_bucket_->Process();
  EXPECT_EQ(0u, send_bucket_->QueueSizePackets());

  // Alternate retransmissions and normal packets.
  for (size_t i = 0; i < packets_to_send_per_interval; ++i) {
    send_bucket_->InsertPacket(PacedSender::kNormalPriority, ssrc,
                               sequence_number++,
                               capture_time_ms_retransmission, 250, true);
    send_bucket_->InsertPacket(PacedSender::kNormalPriority, ssrc,
                               sequence_number++, capture_time_ms, 250, false);
  }
  EXPECT_EQ(2 * packets_to_send_per_interval, send_bucket_->QueueSizePackets());

  // Expect all retransmissions to be sent out first despite having a later
  // capture time.
  EXPECT_CALL(callback_, TimeToSendPadding(_, _)).Times(0);
  EXPECT_CALL(callback_, TimeToSendPacket(_, _, _, false, _)).Times(0);
  EXPECT_CALL(callback_, TimeToSendPacket(
                             ssrc, _, capture_time_ms_retransmission, true, _))
      .Times(packets_to_send_per_interval)
      .WillRepeatedly(Return(true));

  EXPECT_EQ(5, send_bucket_->TimeUntilNextProcess());
  clock_.AdvanceTimeMilliseconds(5);
  EXPECT_EQ(0, send_bucket_->TimeUntilNextProcess());
  send_bucket_->Process();
  EXPECT_EQ(packets_to_send_per_interval, send_bucket_->QueueSizePackets());

  // Expect the remaining (non-retransmission) packets to be sent.
  EXPECT_CALL(callback_, TimeToSendPadding(_, _)).Times(0);
  EXPECT_CALL(callback_, TimeToSendPacket(_, _, _, true, _)).Times(0);
  EXPECT_CALL(callback_, TimeToSendPacket(ssrc, _, capture_time_ms, false, _))
      .Times(packets_to_send_per_interval)
      .WillRepeatedly(Return(true));

  EXPECT_EQ(5, send_bucket_->TimeUntilNextProcess());
  clock_.AdvanceTimeMilliseconds(5);
  EXPECT_EQ(0, send_bucket_->TimeUntilNextProcess());
  send_bucket_->Process();

  EXPECT_EQ(0u, send_bucket_->QueueSizePackets());
}

TEST_F(PacedSenderTest, HighPrioDoesntAffectBudget) {
  uint32_t ssrc = 12346;
  uint16_t sequence_number = 1234;
  int64_t capture_time_ms = 56789;

  // As high prio packets doesn't affect the budget, we should be able to send
  // a high number of them at once.
  for (int i = 0; i < 25; ++i) {
    SendAndExpectPacket(PacedSender::kHighPriority, ssrc, sequence_number++,
                        capture_time_ms, 250, false);
  }
  send_bucket_->Process();
  // Low prio packets does affect the budget.
  // Due to the multiplicative factor we can send 5 packets during a send
  // interval. (network capacity * multiplier / (8 bits per byte *
  // (packet size * #send intervals per second)
  const size_t packets_to_send_per_interval =
      kTargetBitrateBps * kPaceMultiplier / (8 * 250 * 200);
  for (size_t i = 0; i < packets_to_send_per_interval; ++i) {
    SendAndExpectPacket(PacedSender::kLowPriority, ssrc, sequence_number++,
                        clock_.TimeInMilliseconds(), 250, false);
  }
  send_bucket_->InsertPacket(PacedSender::kLowPriority, ssrc, sequence_number,
                             capture_time_ms, 250, false);
  EXPECT_EQ(5, send_bucket_->TimeUntilNextProcess());
  clock_.AdvanceTimeMilliseconds(5);
  send_bucket_->Process();
  EXPECT_EQ(1u, send_bucket_->QueueSizePackets());
  EXPECT_CALL(callback_, TimeToSendPacket(ssrc, sequence_number++,
                                          capture_time_ms, false, _))
      .Times(1)
      .WillRepeatedly(Return(true));
  EXPECT_EQ(5, send_bucket_->TimeUntilNextProcess());
  clock_.AdvanceTimeMilliseconds(5);
  send_bucket_->Process();
  EXPECT_EQ(0u, send_bucket_->QueueSizePackets());
}

TEST_F(PacedSenderTest, SendsOnlyPaddingWhenCongested) {
  uint32_t ssrc = 202020;
  uint16_t sequence_number = 1000;
  int kPacketSize = 250;
  int kCongestionWindow = kPacketSize * 10;

  send_bucket_->UpdateOutstandingData(0);
  send_bucket_->SetCongestionWindow(kCongestionWindow);
  int sent_data = 0;
  while (sent_data < kCongestionWindow) {
    sent_data += kPacketSize;
    SendAndExpectPacket(PacedSender::kNormalPriority, ssrc, sequence_number++,
                        clock_.TimeInMilliseconds(), kPacketSize, false);
    clock_.AdvanceTimeMilliseconds(5);
    send_bucket_->Process();
  }
  testing::Mock::VerifyAndClearExpectations(&callback_);
  EXPECT_CALL(callback_, TimeToSendPacket(_, _, _, _, _)).Times(0);
  EXPECT_CALL(callback_, TimeToSendPadding(_, _)).Times(0);

  size_t blocked_packets = 0;
  int64_t expected_time_until_padding = 500;
  while (expected_time_until_padding > 5) {
    send_bucket_->InsertPacket(PacedSender::kNormalPriority, ssrc,
                               sequence_number++, clock_.TimeInMilliseconds(),
                               kPacketSize, false);
    blocked_packets++;
    clock_.AdvanceTimeMilliseconds(5);
    send_bucket_->Process();
    expected_time_until_padding -= 5;
  }
  testing::Mock::VerifyAndClearExpectations(&callback_);
  EXPECT_CALL(callback_, TimeToSendPadding(1, _)).Times(1);
  clock_.AdvanceTimeMilliseconds(5);
  send_bucket_->Process();
  EXPECT_EQ(blocked_packets, send_bucket_->QueueSizePackets());
}

TEST_F(PacedSenderTest, DoesNotAllowOveruseAfterCongestion) {
  uint32_t ssrc = 202020;
  uint16_t seq_num = 1000;
  RtpPacketSender::Priority prio = PacedSender::kNormalPriority;
  int size = 1000;
  auto now_ms = [this] { return clock_.TimeInMilliseconds(); };
  EXPECT_CALL(callback_, TimeToSendPadding).Times(0);
  // The pacing rate is low enough that the budget should not allow two packets
  // to be sent in a row.
  send_bucket_->SetPacingRates(400 * 8 * 1000 / 5, 0);
  // The congestion window is small enough to only let one packet through.
  send_bucket_->SetCongestionWindow(800);
  send_bucket_->UpdateOutstandingData(0);
  // Not yet budget limited or congested, packet is sent.
  send_bucket_->InsertPacket(prio, ssrc, seq_num++, now_ms(), size, false);
  EXPECT_CALL(callback_, TimeToSendPacket).WillOnce(Return(true));
  clock_.AdvanceTimeMilliseconds(5);
  send_bucket_->Process();
  // Packet blocked due to congestion.
  send_bucket_->InsertPacket(prio, ssrc, seq_num++, now_ms(), size, false);
  EXPECT_CALL(callback_, TimeToSendPacket).Times(0);
  clock_.AdvanceTimeMilliseconds(5);
  send_bucket_->Process();
  // Packet blocked due to congestion.
  send_bucket_->InsertPacket(prio, ssrc, seq_num++, now_ms(), size, false);
  EXPECT_CALL(callback_, TimeToSendPacket).Times(0);
  clock_.AdvanceTimeMilliseconds(5);
  send_bucket_->Process();
  send_bucket_->UpdateOutstandingData(0);
  // Congestion removed and budget has recovered, packet is sent.
  send_bucket_->InsertPacket(prio, ssrc, seq_num++, now_ms(), size, false);
  EXPECT_CALL(callback_, TimeToSendPacket).WillOnce(Return(true));
  clock_.AdvanceTimeMilliseconds(5);
  send_bucket_->Process();
  send_bucket_->UpdateOutstandingData(0);
  // Should be blocked due to budget limitation as congestion has be removed.
  send_bucket_->InsertPacket(prio, ssrc, seq_num++, now_ms(), size, false);
  EXPECT_CALL(callback_, TimeToSendPacket).Times(0);
  clock_.AdvanceTimeMilliseconds(5);
  send_bucket_->Process();
}

TEST_F(PacedSenderTest, ResumesSendingWhenCongestionEnds) {
  uint32_t ssrc = 202020;
  uint16_t sequence_number = 1000;
  int64_t kPacketSize = 250;
  int64_t kCongestionCount = 10;
  int64_t kCongestionWindow = kPacketSize * kCongestionCount;
  int64_t kCongestionTimeMs = 1000;

  send_bucket_->UpdateOutstandingData(0);
  send_bucket_->SetCongestionWindow(kCongestionWindow);
  int sent_data = 0;
  while (sent_data < kCongestionWindow) {
    sent_data += kPacketSize;
    SendAndExpectPacket(PacedSender::kNormalPriority, ssrc, sequence_number++,
                        clock_.TimeInMilliseconds(), kPacketSize, false);
    clock_.AdvanceTimeMilliseconds(5);
    send_bucket_->Process();
  }
  testing::Mock::VerifyAndClearExpectations(&callback_);
  EXPECT_CALL(callback_, TimeToSendPacket(_, _, _, _, _)).Times(0);
  int unacked_packets = 0;
  for (int duration = 0; duration < kCongestionTimeMs; duration += 5) {
    send_bucket_->InsertPacket(PacedSender::kNormalPriority, ssrc,
                               sequence_number++, clock_.TimeInMilliseconds(),
                               kPacketSize, false);
    unacked_packets++;
    clock_.AdvanceTimeMilliseconds(5);
    send_bucket_->Process();
  }
  testing::Mock::VerifyAndClearExpectations(&callback_);

  // First mark half of the congested packets as cleared and make sure that just
  // as many are sent
  int ack_count = kCongestionCount / 2;
  EXPECT_CALL(callback_, TimeToSendPacket(ssrc, _, _, false, _))
      .Times(ack_count)
      .WillRepeatedly(Return(true));
  send_bucket_->UpdateOutstandingData(kCongestionWindow -
                                      kPacketSize * ack_count);

  for (int duration = 0; duration < kCongestionTimeMs; duration += 5) {
    clock_.AdvanceTimeMilliseconds(5);
    send_bucket_->Process();
  }
  unacked_packets -= ack_count;
  testing::Mock::VerifyAndClearExpectations(&callback_);

  // Second make sure all packets are sent if sent packets are continuously
  // marked as acked.
  EXPECT_CALL(callback_, TimeToSendPacket(ssrc, _, _, false, _))
      .Times(unacked_packets)
      .WillRepeatedly(Return(true));
  for (int duration = 0; duration < kCongestionTimeMs; duration += 5) {
    send_bucket_->UpdateOutstandingData(0);
    clock_.AdvanceTimeMilliseconds(5);
    send_bucket_->Process();
  }
}

TEST_F(PacedSenderTest, Pause) {
  uint32_t ssrc_low_priority = 12345;
  uint32_t ssrc = 12346;
  uint32_t ssrc_high_priority = 12347;
  uint16_t sequence_number = 1234;
  int64_t capture_time_ms = clock_.TimeInMilliseconds();

  EXPECT_EQ(0, send_bucket_->QueueInMs());

  // Due to the multiplicative factor we can send 5 packets during a send
  // interval. (network capacity * multiplier / (8 bits per byte *
  // (packet size * #send intervals per second)
  const size_t packets_to_send_per_interval =
      kTargetBitrateBps * kPaceMultiplier / (8 * 250 * 200);
  for (size_t i = 0; i < packets_to_send_per_interval; ++i) {
    SendAndExpectPacket(PacedSender::kNormalPriority, ssrc, sequence_number++,
                        clock_.TimeInMilliseconds(), 250, false);
  }

  send_bucket_->Process();

  send_bucket_->Pause();

  for (size_t i = 0; i < packets_to_send_per_interval; ++i) {
    send_bucket_->InsertPacket(PacedSender::kLowPriority, ssrc_low_priority,
                               sequence_number++, capture_time_ms, 250, false);
    send_bucket_->InsertPacket(PacedSender::kNormalPriority, ssrc,
                               sequence_number++, capture_time_ms, 250, false);
    send_bucket_->InsertPacket(PacedSender::kHighPriority, ssrc_high_priority,
                               sequence_number++, capture_time_ms, 250, false);
  }
  clock_.AdvanceTimeMilliseconds(10000);
  int64_t second_capture_time_ms = clock_.TimeInMilliseconds();
  for (size_t i = 0; i < packets_to_send_per_interval; ++i) {
    send_bucket_->InsertPacket(PacedSender::kLowPriority, ssrc_low_priority,
                               sequence_number++, second_capture_time_ms, 250,
                               false);
    send_bucket_->InsertPacket(PacedSender::kNormalPriority, ssrc,
                               sequence_number++, second_capture_time_ms, 250,
                               false);
    send_bucket_->InsertPacket(PacedSender::kHighPriority, ssrc_high_priority,
                               sequence_number++, second_capture_time_ms, 250,
                               false);
  }

  // Expect everything to be queued.
  EXPECT_EQ(second_capture_time_ms - capture_time_ms,
            send_bucket_->QueueInMs());

  EXPECT_EQ(0, send_bucket_->TimeUntilNextProcess());
  EXPECT_CALL(callback_, TimeToSendPadding(1, _)).Times(1);
  send_bucket_->Process();

  int64_t expected_time_until_send = 500;
  EXPECT_CALL(callback_, TimeToSendPadding(_, _)).Times(0);
  while (expected_time_until_send >= 5) {
    send_bucket_->Process();
    clock_.AdvanceTimeMilliseconds(5);
    expected_time_until_send -= 5;
  }
  testing::Mock::VerifyAndClearExpectations(&callback_);
  EXPECT_CALL(callback_, TimeToSendPadding(1, _)).Times(1);
  clock_.AdvanceTimeMilliseconds(5);
  send_bucket_->Process();
  testing::Mock::VerifyAndClearExpectations(&callback_);

  // Expect high prio packets to come out first followed by normal
  // prio packets and low prio packets (all in capture order).
  {
    ::testing::InSequence sequence;
    EXPECT_CALL(callback_,
                TimeToSendPacket(ssrc_high_priority, _, capture_time_ms, _, _))
        .Times(packets_to_send_per_interval)
        .WillRepeatedly(Return(true));
    EXPECT_CALL(callback_, TimeToSendPacket(ssrc_high_priority, _,
                                            second_capture_time_ms, _, _))
        .Times(packets_to_send_per_interval)
        .WillRepeatedly(Return(true));

    for (size_t i = 0; i < packets_to_send_per_interval; ++i) {
      EXPECT_CALL(callback_, TimeToSendPacket(ssrc, _, capture_time_ms, _, _))
          .Times(1)
          .WillRepeatedly(Return(true));
    }
    for (size_t i = 0; i < packets_to_send_per_interval; ++i) {
      EXPECT_CALL(callback_,
                  TimeToSendPacket(ssrc, _, second_capture_time_ms, _, _))
          .Times(1)
          .WillRepeatedly(Return(true));
    }
    for (size_t i = 0; i < packets_to_send_per_interval; ++i) {
      EXPECT_CALL(callback_,
                  TimeToSendPacket(ssrc_low_priority, _, capture_time_ms, _, _))
          .Times(1)
          .WillRepeatedly(Return(true));
    }
    for (size_t i = 0; i < packets_to_send_per_interval; ++i) {
      EXPECT_CALL(callback_, TimeToSendPacket(ssrc_low_priority, _,
                                              second_capture_time_ms, _, _))
          .Times(1)
          .WillRepeatedly(Return(true));
    }
  }
  send_bucket_->Resume();

  // The pacer was resumed directly after the previous process call finished. It
  // will therefore wait 5 ms until next process.
  EXPECT_EQ(5, send_bucket_->TimeUntilNextProcess());
  clock_.AdvanceTimeMilliseconds(5);

  for (size_t i = 0; i < 4; i++) {
    EXPECT_EQ(0, send_bucket_->TimeUntilNextProcess());
    send_bucket_->Process();
    EXPECT_EQ(5, send_bucket_->TimeUntilNextProcess());
    clock_.AdvanceTimeMilliseconds(5);
  }

  EXPECT_EQ(0, send_bucket_->QueueInMs());
}

TEST_F(PacedSenderTest, ResendPacket) {
  uint32_t ssrc = 12346;
  uint16_t sequence_number = 1234;
  int64_t capture_time_ms = clock_.TimeInMilliseconds();
  EXPECT_EQ(0, send_bucket_->QueueInMs());

  send_bucket_->InsertPacket(PacedSender::kNormalPriority, ssrc,
                             sequence_number, capture_time_ms, 250, false);
  clock_.AdvanceTimeMilliseconds(1);
  send_bucket_->InsertPacket(PacedSender::kNormalPriority, ssrc,
                             sequence_number + 1, capture_time_ms + 1, 250,
                             false);
  clock_.AdvanceTimeMilliseconds(9999);
  EXPECT_EQ(clock_.TimeInMilliseconds() - capture_time_ms,
            send_bucket_->QueueInMs());
  // Fails to send first packet so only one call.
  EXPECT_CALL(callback_, TimeToSendPacket(ssrc, sequence_number,
                                          capture_time_ms, false, _))
      .Times(1)
      .WillOnce(Return(false));
  clock_.AdvanceTimeMilliseconds(10000);
  send_bucket_->Process();

  // Queue remains unchanged.
  EXPECT_EQ(clock_.TimeInMilliseconds() - capture_time_ms,
            send_bucket_->QueueInMs());

  // Fails to send second packet.
  EXPECT_CALL(callback_, TimeToSendPacket(ssrc, sequence_number,
                                          capture_time_ms, false, _))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(callback_, TimeToSendPacket(ssrc, sequence_number + 1,
                                          capture_time_ms + 1, false, _))
      .Times(1)
      .WillOnce(Return(false));
  clock_.AdvanceTimeMilliseconds(10000);
  send_bucket_->Process();

  // Queue is reduced by 1 packet.
  EXPECT_EQ(clock_.TimeInMilliseconds() - capture_time_ms - 1,
            send_bucket_->QueueInMs());

  // Send second packet and queue becomes empty.
  EXPECT_CALL(callback_, TimeToSendPacket(ssrc, sequence_number + 1,
                                          capture_time_ms + 1, false, _))
      .Times(1)
      .WillOnce(Return(true));
  clock_.AdvanceTimeMilliseconds(10000);
  send_bucket_->Process();
  EXPECT_EQ(0, send_bucket_->QueueInMs());
}

TEST_F(PacedSenderTest, ExpectedQueueTimeMs) {
  uint32_t ssrc = 12346;
  uint16_t sequence_number = 1234;
  const size_t kNumPackets = 60;
  const size_t kPacketSize = 1200;
  const int32_t kMaxBitrate = kPaceMultiplier * 30000;
  EXPECT_EQ(0, send_bucket_->ExpectedQueueTimeMs());

  send_bucket_->SetPacingRates(30000 * kPaceMultiplier, 0);
  for (size_t i = 0; i < kNumPackets; ++i) {
    SendAndExpectPacket(PacedSender::kNormalPriority, ssrc, sequence_number++,
                        clock_.TimeInMilliseconds(), kPacketSize, false);
  }

  // Queue in ms = 1000 * (bytes in queue) *8 / (bits per second)
  int64_t queue_in_ms =
      static_cast<int64_t>(1000 * kNumPackets * kPacketSize * 8 / kMaxBitrate);
  EXPECT_EQ(queue_in_ms, send_bucket_->ExpectedQueueTimeMs());

  int64_t time_start = clock_.TimeInMilliseconds();
  while (send_bucket_->QueueSizePackets() > 0) {
    int time_until_process = send_bucket_->TimeUntilNextProcess();
    if (time_until_process <= 0) {
      send_bucket_->Process();
    } else {
      clock_.AdvanceTimeMilliseconds(time_until_process);
    }
  }
  int64_t duration = clock_.TimeInMilliseconds() - time_start;

  EXPECT_EQ(0, send_bucket_->ExpectedQueueTimeMs());

  // Allow for aliasing, duration should be within one pack of max time limit.
  EXPECT_NEAR(duration, PacedSender::kMaxQueueLengthMs,
              static_cast<int64_t>(1000 * kPacketSize * 8 / kMaxBitrate));
}

TEST_F(PacedSenderTest, QueueTimeGrowsOverTime) {
  uint32_t ssrc = 12346;
  uint16_t sequence_number = 1234;
  EXPECT_EQ(0, send_bucket_->QueueInMs());

  send_bucket_->SetPacingRates(30000 * kPaceMultiplier, 0);
  SendAndExpectPacket(PacedSender::kNormalPriority, ssrc, sequence_number,
                      clock_.TimeInMilliseconds(), 1200, false);

  clock_.AdvanceTimeMilliseconds(500);
  EXPECT_EQ(500, send_bucket_->QueueInMs());
  send_bucket_->Process();
  EXPECT_EQ(0, send_bucket_->QueueInMs());
}

TEST_F(PacedSenderTest, ProbingWithInsertedPackets) {
  const size_t kPacketSize = 1200;
  const int kInitialBitrateBps = 300000;
  uint32_t ssrc = 12346;
  uint16_t sequence_number = 1234;

  PacedSenderProbing packet_sender;
  send_bucket_.reset(new PacedSender(&clock_, &packet_sender, nullptr));
  send_bucket_->CreateProbeCluster(kFirstClusterBps);
  send_bucket_->CreateProbeCluster(kSecondClusterBps);
  send_bucket_->SetPacingRates(kInitialBitrateBps * kPaceMultiplier, 0);

  for (int i = 0; i < 10; ++i) {
    send_bucket_->InsertPacket(PacedSender::kNormalPriority, ssrc,
                               sequence_number++, clock_.TimeInMilliseconds(),
                               kPacketSize, false);
  }

  int64_t start = clock_.TimeInMilliseconds();
  while (packet_sender.packets_sent() < 5) {
    int time_until_process = send_bucket_->TimeUntilNextProcess();
    clock_.AdvanceTimeMilliseconds(time_until_process);
    send_bucket_->Process();
  }
  int packets_sent = packet_sender.packets_sent();
  // Validate first cluster bitrate. Note that we have to account for number
  // of intervals and hence (packets_sent - 1) on the first cluster.
  EXPECT_NEAR((packets_sent - 1) * kPacketSize * 8000 /
                  (clock_.TimeInMilliseconds() - start),
              kFirstClusterBps, kBitrateProbingError);
  EXPECT_EQ(0, packet_sender.padding_sent());

  clock_.AdvanceTimeMilliseconds(send_bucket_->TimeUntilNextProcess());
  start = clock_.TimeInMilliseconds();
  while (packet_sender.packets_sent() < 10) {
    int time_until_process = send_bucket_->TimeUntilNextProcess();
    clock_.AdvanceTimeMilliseconds(time_until_process);
    send_bucket_->Process();
  }
  packets_sent = packet_sender.packets_sent() - packets_sent;
  // Validate second cluster bitrate.
  EXPECT_NEAR((packets_sent - 1) * kPacketSize * 8000 /
                  (clock_.TimeInMilliseconds() - start),
              kSecondClusterBps, kBitrateProbingError);
}

TEST_F(PacedSenderTest, ProbingWithPaddingSupport) {
  const size_t kPacketSize = 1200;
  const int kInitialBitrateBps = 300000;
  uint32_t ssrc = 12346;
  uint16_t sequence_number = 1234;

  PacedSenderProbing packet_sender;
  send_bucket_.reset(new PacedSender(&clock_, &packet_sender, nullptr));
  send_bucket_->CreateProbeCluster(kFirstClusterBps);
  send_bucket_->SetPacingRates(kInitialBitrateBps * kPaceMultiplier, 0);

  for (int i = 0; i < 3; ++i) {
    send_bucket_->InsertPacket(PacedSender::kNormalPriority, ssrc,
                               sequence_number++, clock_.TimeInMilliseconds(),
                               kPacketSize, false);
  }

  int64_t start = clock_.TimeInMilliseconds();
  int process_count = 0;
  while (process_count < 5) {
    int time_until_process = send_bucket_->TimeUntilNextProcess();
    clock_.AdvanceTimeMilliseconds(time_until_process);
    send_bucket_->Process();
    ++process_count;
  }
  int packets_sent = packet_sender.packets_sent();
  int padding_sent = packet_sender.padding_sent();
  EXPECT_GT(packets_sent, 0);
  EXPECT_GT(padding_sent, 0);
  // Note that the number of intervals here for kPacketSize is
  // packets_sent due to padding in the same cluster.
  EXPECT_NEAR((packets_sent * kPacketSize * 8000 + padding_sent) /
                  (clock_.TimeInMilliseconds() - start),
              kFirstClusterBps, kBitrateProbingError);
}

TEST_F(PacedSenderTest, PaddingOveruse) {
  uint32_t ssrc = 12346;
  uint16_t sequence_number = 1234;
  const size_t kPacketSize = 1200;

  send_bucket_->Process();
  send_bucket_->SetPacingRates(60000 * kPaceMultiplier, 0);

  SendAndExpectPacket(PacedSender::kNormalPriority, ssrc, sequence_number++,
                      clock_.TimeInMilliseconds(), kPacketSize, false);
  send_bucket_->Process();

  // Add 30kbit padding. When increasing budget, media budget will increase from
  // negative (overuse) while padding budget will increase from 0.
  clock_.AdvanceTimeMilliseconds(5);
  send_bucket_->SetPacingRates(60000 * kPaceMultiplier, 30000);

  SendAndExpectPacket(PacedSender::kNormalPriority, ssrc, sequence_number++,
                      clock_.TimeInMilliseconds(), kPacketSize, false);
  EXPECT_LT(5u, send_bucket_->ExpectedQueueTimeMs());
  // Don't send padding if queue is non-empty, even if padding budget > 0.
  EXPECT_CALL(callback_, TimeToSendPadding(_, _)).Times(0);
  send_bucket_->Process();
}

// TODO(philipel): Move to PacketQueue2 unittests.
#if 0
TEST_F(PacedSenderTest, AverageQueueTime) {
  uint32_t ssrc = 12346;
  uint16_t sequence_number = 1234;
  const size_t kPacketSize = 1200;
  const int kBitrateBps = 10 * kPacketSize * 8;  // 10 packets per second.

  send_bucket_->SetPacingRates(kBitrateBps * kPaceMultiplier, 0);

  EXPECT_EQ(0, send_bucket_->AverageQueueTimeMs());

  int64_t first_capture_time = clock_.TimeInMilliseconds();
  send_bucket_->InsertPacket(PacedSender::kNormalPriority, ssrc,
                             sequence_number, first_capture_time, kPacketSize,
                             false);
  clock_.AdvanceTimeMilliseconds(10);
  send_bucket_->InsertPacket(PacedSender::kNormalPriority, ssrc,
                             sequence_number + 1, clock_.TimeInMilliseconds(),
                             kPacketSize, false);
  clock_.AdvanceTimeMilliseconds(10);

  EXPECT_EQ((20 + 10) / 2, send_bucket_->AverageQueueTimeMs());

  // Only first packet (queued for 20ms) should be removed, leave the second
  // packet (queued for 10ms) alone in the queue.
  EXPECT_CALL(callback_, TimeToSendPacket(ssrc, sequence_number,
                                          first_capture_time, false, _))
      .Times(1)
      .WillRepeatedly(Return(true));
  send_bucket_->Process();

  EXPECT_EQ(10, send_bucket_->AverageQueueTimeMs());

  clock_.AdvanceTimeMilliseconds(10);
  EXPECT_CALL(callback_, TimeToSendPacket(ssrc, sequence_number + 1,
                                          first_capture_time + 10, false, _))
      .Times(1)
      .WillRepeatedly(Return(true));
  for (int i = 0; i < 3; ++i) {
    clock_.AdvanceTimeMilliseconds(30);  // Max delta.
    send_bucket_->Process();
  }

  EXPECT_EQ(0, send_bucket_->AverageQueueTimeMs());
}
#endif

TEST_F(PacedSenderTest, ProbeClusterId) {
  uint32_t ssrc = 12346;
  uint16_t sequence_number = 1234;
  const size_t kPacketSize = 1200;

  send_bucket_->SetPacingRates(kTargetBitrateBps * kPaceMultiplier,
                               kTargetBitrateBps);
  send_bucket_->SetProbingEnabled(true);
  for (int i = 0; i < 10; ++i) {
    send_bucket_->InsertPacket(PacedSender::kNormalPriority, ssrc,
                               sequence_number + i, clock_.TimeInMilliseconds(),
                               kPacketSize, false);
  }

  // First probing cluster.
  EXPECT_CALL(callback_,
              TimeToSendPacket(_, _, _, _,
                               Field(&PacedPacketInfo::probe_cluster_id, 0)))
      .Times(5)
      .WillRepeatedly(Return(true));
  for (int i = 0; i < 5; ++i) {
    clock_.AdvanceTimeMilliseconds(20);
    send_bucket_->Process();
  }

  // Second probing cluster.
  EXPECT_CALL(callback_,
              TimeToSendPacket(_, _, _, _,
                               Field(&PacedPacketInfo::probe_cluster_id, 1)))
      .Times(5)
      .WillRepeatedly(Return(true));
  for (int i = 0; i < 5; ++i) {
    clock_.AdvanceTimeMilliseconds(20);
    send_bucket_->Process();
  }

  // Needed for the Field comparer below.
  const int kNotAProbe = PacedPacketInfo::kNotAProbe;
  // No more probing packets.
  EXPECT_CALL(callback_,
              TimeToSendPadding(
                  _, Field(&PacedPacketInfo::probe_cluster_id, kNotAProbe)))
      .Times(1)
      .WillRepeatedly(Return(500));
  send_bucket_->Process();
}

TEST_F(PacedSenderTest, AvoidBusyLoopOnSendFailure) {
  uint32_t ssrc = 12346;
  uint16_t sequence_number = 1234;
  const size_t kPacketSize = kFirstClusterBps / (8000 / 10);

  send_bucket_->SetPacingRates(kTargetBitrateBps * kPaceMultiplier,
                               kTargetBitrateBps);
  send_bucket_->SetProbingEnabled(true);
  send_bucket_->InsertPacket(PacedSender::kNormalPriority, ssrc,
                             sequence_number, clock_.TimeInMilliseconds(),
                             kPacketSize, false);

  EXPECT_CALL(callback_, TimeToSendPacket(_, _, _, _, _))
      .WillOnce(Return(true));
  send_bucket_->Process();
  EXPECT_EQ(10, send_bucket_->TimeUntilNextProcess());
  clock_.AdvanceTimeMilliseconds(9);

  EXPECT_CALL(callback_, TimeToSendPadding(_, _))
      .Times(2)
      .WillRepeatedly(Return(0));
  send_bucket_->Process();
  EXPECT_EQ(1, send_bucket_->TimeUntilNextProcess());
  clock_.AdvanceTimeMilliseconds(1);
  send_bucket_->Process();
  EXPECT_EQ(5, send_bucket_->TimeUntilNextProcess());
}

// TODO(philipel): Move to PacketQueue2 unittests.
#if 0
TEST_F(PacedSenderTest, QueueTimeWithPause) {
  const size_t kPacketSize = 1200;
  const uint32_t kSsrc = 12346;
  uint16_t sequence_number = 1234;

  send_bucket_->InsertPacket(PacedSender::kNormalPriority, kSsrc,
      sequence_number++, clock_.TimeInMilliseconds(),
      kPacketSize, false);
  send_bucket_->InsertPacket(PacedSender::kNormalPriority, kSsrc,
      sequence_number++, clock_.TimeInMilliseconds(),
      kPacketSize, false);

  clock_.AdvanceTimeMilliseconds(100);
  EXPECT_EQ(100, send_bucket_->AverageQueueTimeMs());

  send_bucket_->Pause();
  EXPECT_EQ(100, send_bucket_->AverageQueueTimeMs());

  // In paused state, queue time should not increase.
  clock_.AdvanceTimeMilliseconds(100);
  EXPECT_EQ(100, send_bucket_->AverageQueueTimeMs());

  send_bucket_->Resume();
  EXPECT_EQ(100, send_bucket_->AverageQueueTimeMs());

  clock_.AdvanceTimeMilliseconds(100);
  EXPECT_EQ(200, send_bucket_->AverageQueueTimeMs());
}

TEST_F(PacedSenderTest, QueueTimePausedDuringPush) {
  const size_t kPacketSize = 1200;
  const uint32_t kSsrc = 12346;
  uint16_t sequence_number = 1234;

  send_bucket_->InsertPacket(PacedSender::kNormalPriority, kSsrc,
      sequence_number++, clock_.TimeInMilliseconds(),
      kPacketSize, false);
  clock_.AdvanceTimeMilliseconds(100);
  send_bucket_->Pause();
  clock_.AdvanceTimeMilliseconds(100);
  EXPECT_EQ(100, send_bucket_->AverageQueueTimeMs());

  // Add a new packet during paused phase.
  send_bucket_->InsertPacket(PacedSender::kNormalPriority, kSsrc,
      sequence_number++, clock_.TimeInMilliseconds(),
      kPacketSize, false);
  // From a queue time perspective, packet inserted during pause will have zero
  // queue time. Average queue time will then be (0 + 100) / 2 = 50.
  EXPECT_EQ(50, send_bucket_->AverageQueueTimeMs());

  clock_.AdvanceTimeMilliseconds(100);
  EXPECT_EQ(50, send_bucket_->AverageQueueTimeMs());

  send_bucket_->Resume();
  EXPECT_EQ(50, send_bucket_->AverageQueueTimeMs());

  clock_.AdvanceTimeMilliseconds(100);
  EXPECT_EQ(150, send_bucket_->AverageQueueTimeMs());
}
#endif

// TODO(sprang): Extract PacketQueue from PacedSender so that we can test
// removing elements while paused. (This is possible, but only because of semi-
// racy condition so can't easily be tested).

}  // namespace test
}  // namespace webrtc
