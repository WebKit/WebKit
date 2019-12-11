/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/pacing/pacing_controller.h"

#include <algorithm>
#include <list>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "api/units/data_rate.h"
#include "modules/pacing/packet_router.h"
#include "system_wrappers/include/clock.h"
#include "test/field_trial.h"
#include "test/gmock.h"
#include "test/gtest.h"

using ::testing::_;
using ::testing::Field;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::Return;

namespace webrtc {
namespace test {
namespace {
constexpr DataRate kFirstClusterRate = DataRate::KilobitsPerSec<900>();
constexpr DataRate kSecondClusterRate = DataRate::KilobitsPerSec<1800>();

// The error stems from truncating the time interval of probe packets to integer
// values. This results in probing slightly higher than the target bitrate.
// For 1.8 Mbps, this comes to be about 120 kbps with 1200 probe packets.
constexpr DataRate kProbingErrorMargin = DataRate::KilobitsPerSec<150>();

const float kPaceMultiplier = 2.5f;

constexpr uint32_t kAudioSsrc = 12345;
constexpr uint32_t kVideoSsrc = 234565;
constexpr uint32_t kVideoRtxSsrc = 34567;
constexpr uint32_t kFlexFecSsrc = 45678;

constexpr DataRate kTargetRate = DataRate::KilobitsPerSec<800>();

std::unique_ptr<RtpPacketToSend> BuildPacket(RtpPacketToSend::Type type,
                                             uint32_t ssrc,
                                             uint16_t sequence_number,
                                             int64_t capture_time_ms,
                                             size_t size) {
  auto packet = absl::make_unique<RtpPacketToSend>(nullptr);
  packet->set_packet_type(type);
  packet->SetSsrc(ssrc);
  packet->SetSequenceNumber(sequence_number);
  packet->set_capture_time_ms(capture_time_ms);
  packet->SetPayloadSize(size);
  return packet;
}
}  // namespace

// Mock callback proxy, where both new and old api redirects to common mock
// methods that focus on core aspects.
class MockPacingControllerCallback : public PacingController::PacketSender {
 public:
  void SendRtpPacket(std::unique_ptr<RtpPacketToSend> packet,
                     const PacedPacketInfo& cluster_info) override {
    SendPacket(packet->Ssrc(), packet->SequenceNumber(),
               packet->capture_time_ms(),
               packet->packet_type() == RtpPacketToSend::Type::kRetransmission,
               packet->packet_type() == RtpPacketToSend::Type::kPadding);
  }

  std::vector<std::unique_ptr<RtpPacketToSend>> GeneratePadding(
      DataSize target_size) override {
    std::vector<std::unique_ptr<RtpPacketToSend>> ret;
    size_t padding_size = SendPadding(target_size.bytes());
    if (padding_size > 0) {
      auto packet = absl::make_unique<RtpPacketToSend>(nullptr);
      packet->SetPayloadSize(padding_size);
      packet->set_packet_type(RtpPacketToSend::Type::kPadding);
      ret.emplace_back(std::move(packet));
    }
    return ret;
  }

  MOCK_METHOD5(SendPacket,
               void(uint32_t ssrc,
                    uint16_t sequence_number,
                    int64_t capture_timestamp,
                    bool retransmission,
                    bool padding));
  MOCK_METHOD1(SendPadding, size_t(size_t target_size));
};

// Mock callback implementing the raw api.
class MockPacketSender : public PacingController::PacketSender {
 public:
  MOCK_METHOD2(SendRtpPacket,
               void(std::unique_ptr<RtpPacketToSend> packet,
                    const PacedPacketInfo& cluster_info));
  MOCK_METHOD1(
      GeneratePadding,
      std::vector<std::unique_ptr<RtpPacketToSend>>(DataSize target_size));
};

class PacingControllerPadding : public PacingController::PacketSender {
 public:
  static const size_t kPaddingPacketSize = 224;

  PacingControllerPadding() : padding_sent_(0) {}

  void SendRtpPacket(std::unique_ptr<RtpPacketToSend> packet,
                     const PacedPacketInfo& pacing_info) override {}

  std::vector<std::unique_ptr<RtpPacketToSend>> GeneratePadding(
      DataSize target_size) override {
    size_t num_packets =
        (target_size.bytes() + kPaddingPacketSize - 1) / kPaddingPacketSize;
    std::vector<std::unique_ptr<RtpPacketToSend>> packets;
    for (size_t i = 0; i < num_packets; ++i) {
      packets.emplace_back(absl::make_unique<RtpPacketToSend>(nullptr));
      packets.back()->SetPadding(kPaddingPacketSize);
      packets.back()->set_packet_type(RtpPacketToSend::Type::kPadding);
      padding_sent_ += kPaddingPacketSize;
    }
    return packets;
  }

  size_t padding_sent() { return padding_sent_; }

 private:
  size_t padding_sent_;
};

class PacingControllerProbing : public PacingController::PacketSender {
 public:
  PacingControllerProbing() : packets_sent_(0), padding_sent_(0) {}

  void SendRtpPacket(std::unique_ptr<RtpPacketToSend> packet,
                     const PacedPacketInfo& pacing_info) override {
    if (packet->packet_type() != RtpPacketToSend::Type::kPadding) {
      ++packets_sent_;
    }
  }

  std::vector<std::unique_ptr<RtpPacketToSend>> GeneratePadding(
      DataSize target_size) override {
    std::vector<std::unique_ptr<RtpPacketToSend>> packets;
    packets.emplace_back(absl::make_unique<RtpPacketToSend>(nullptr));
    packets.back()->SetPadding(target_size.bytes());
    packets.back()->set_packet_type(RtpPacketToSend::Type::kPadding);
    padding_sent_ += target_size.bytes();
    return packets;
  }

  int packets_sent() const { return packets_sent_; }

  int padding_sent() const { return padding_sent_; }

 private:
  int packets_sent_;
  int padding_sent_;
};

class PacingControllerTest : public ::testing::Test {
 protected:
  PacingControllerTest() : clock_(123456) {
    srand(0);
    // Need to initialize PacingController after we initialize clock.
    pacer_ = absl::make_unique<PacingController>(&clock_, &callback_, nullptr,
                                                 nullptr);
    Init();
  }

  void Init() {
    pacer_->CreateProbeCluster(kFirstClusterRate, /*cluster_id=*/0);
    pacer_->CreateProbeCluster(kSecondClusterRate, /*cluster_id=*/1);
    // Default to bitrate probing disabled for testing purposes. Probing tests
    // have to enable probing, either by creating a new PacingController
    // instance or by calling SetProbingEnabled(true).
    pacer_->SetProbingEnabled(false);
    pacer_->SetPacingRates(kTargetRate * kPaceMultiplier, DataRate::Zero());

    clock_.AdvanceTime(TimeUntilNextProcess());
  }

  void Send(RtpPacketToSend::Type type,
            uint32_t ssrc,
            uint16_t sequence_number,
            int64_t capture_time_ms,
            size_t size) {
    pacer_->EnqueuePacket(
        BuildPacket(type, ssrc, sequence_number, capture_time_ms, size));
  }

  void SendAndExpectPacket(RtpPacketToSend::Type type,
                           uint32_t ssrc,
                           uint16_t sequence_number,
                           int64_t capture_time_ms,
                           size_t size) {
    Send(type, ssrc, sequence_number, capture_time_ms, size);
    EXPECT_CALL(
        callback_,
        SendPacket(ssrc, sequence_number, capture_time_ms,
                   type == RtpPacketToSend::Type::kRetransmission, false))
        .Times(1);
  }

  std::unique_ptr<RtpPacketToSend> BuildRtpPacket(RtpPacketToSend::Type type) {
    auto packet = absl::make_unique<RtpPacketToSend>(nullptr);
    packet->set_packet_type(type);
    switch (type) {
      case RtpPacketToSend::Type::kAudio:
        packet->SetSsrc(kAudioSsrc);
        break;
      case RtpPacketToSend::Type::kVideo:
        packet->SetSsrc(kVideoSsrc);
        break;
      case RtpPacketToSend::Type::kRetransmission:
      case RtpPacketToSend::Type::kPadding:
        packet->SetSsrc(kVideoRtxSsrc);
        break;
      case RtpPacketToSend::Type::kForwardErrorCorrection:
        packet->SetSsrc(kFlexFecSsrc);
        break;
    }

    packet->SetPayloadSize(234);
    return packet;
  }

  TimeDelta TimeUntilNextProcess() {
    // TODO(bugs.webrtc.org/10809): Replace this with TimeUntilAvailableBudget()
    // once ported from WIP code. For now, emulate PacedSender method.

    TimeDelta elapsed_time = pacer_->TimeElapsedSinceLastProcess();
    if (pacer_->IsPaused()) {
      return std::max(PacingController::kPausedProcessInterval - elapsed_time,
                      TimeDelta::Zero());
    }

    auto next_probe = pacer_->TimeUntilNextProbe();
    if (next_probe) {
      return *next_probe;
    }

    const TimeDelta min_packet_limit = TimeDelta::ms(5);
    return std::max(min_packet_limit - elapsed_time, TimeDelta::Zero());
  }

  SimulatedClock clock_;
  MockPacingControllerCallback callback_;
  std::unique_ptr<PacingController> pacer_;
};

class PacingControllerFieldTrialTest : public ::testing::Test {
 protected:
  struct MediaStream {
    const RtpPacketToSend::Type type;
    const uint32_t ssrc;
    const size_t packet_size;
    uint16_t seq_num;
  };

  const int kProcessIntervalsPerSecond = 1000 / 5;

  PacingControllerFieldTrialTest() : clock_(123456) {}
  void InsertPacket(PacingController* pacer, MediaStream* stream) {
    pacer->EnqueuePacket(
        BuildPacket(stream->type, stream->ssrc, stream->seq_num++,
                    clock_.TimeInMilliseconds(), stream->packet_size));
  }
  void ProcessNext(PacingController* pacer) {
    clock_.AdvanceTimeMilliseconds(5);
    pacer->ProcessPackets();
  }
  MediaStream audio{/*type*/ RtpPacketToSend::Type::kAudio,
                    /*ssrc*/ 3333, /*packet_size*/ 100, /*seq_num*/ 1000};
  MediaStream video{/*type*/ RtpPacketToSend::Type::kVideo,
                    /*ssrc*/ 4444, /*packet_size*/ 1000, /*seq_num*/ 1000};
  SimulatedClock clock_;
  MockPacingControllerCallback callback_;
};

TEST_F(PacingControllerFieldTrialTest, DefaultNoPaddingInSilence) {
  PacingController pacer(&clock_, &callback_, nullptr, nullptr);
  pacer.SetPacingRates(kTargetRate, DataRate::Zero());
  // Video packet to reset last send time and provide padding data.
  InsertPacket(&pacer, &video);
  EXPECT_CALL(callback_, SendPacket).Times(1);
  clock_.AdvanceTimeMilliseconds(5);
  pacer.ProcessPackets();
  EXPECT_CALL(callback_, SendPadding).Times(0);
  // Waiting 500 ms should not trigger sending of padding.
  clock_.AdvanceTimeMilliseconds(500);
  pacer.ProcessPackets();
}

TEST_F(PacingControllerFieldTrialTest, PaddingInSilenceWithTrial) {
  ScopedFieldTrials trial("WebRTC-Pacer-PadInSilence/Enabled/");
  PacingController pacer(&clock_, &callback_, nullptr, nullptr);
  pacer.SetPacingRates(kTargetRate, DataRate::Zero());
  // Video packet to reset last send time and provide padding data.
  InsertPacket(&pacer, &video);
  EXPECT_CALL(callback_, SendPacket).Times(2);
  clock_.AdvanceTimeMilliseconds(5);
  pacer.ProcessPackets();
  EXPECT_CALL(callback_, SendPadding).WillOnce(Return(1000));
  // Waiting 500 ms should trigger sending of padding.
  clock_.AdvanceTimeMilliseconds(500);
  pacer.ProcessPackets();
}

TEST_F(PacingControllerFieldTrialTest, DefaultCongestionWindowAffectsAudio) {
  EXPECT_CALL(callback_, SendPadding).Times(0);
  PacingController pacer(&clock_, &callback_, nullptr, nullptr);
  pacer.SetPacingRates(DataRate::bps(10000000), DataRate::Zero());
  pacer.SetCongestionWindow(DataSize::bytes(800));
  pacer.UpdateOutstandingData(DataSize::Zero());
  // Video packet fills congestion window.
  InsertPacket(&pacer, &video);
  EXPECT_CALL(callback_, SendPacket).Times(1);
  ProcessNext(&pacer);
  // Audio packet blocked due to congestion.
  InsertPacket(&pacer, &audio);
  EXPECT_CALL(callback_, SendPacket).Times(0);
  ProcessNext(&pacer);
  ProcessNext(&pacer);
  // Audio packet unblocked when congestion window clear.
  ::testing::Mock::VerifyAndClearExpectations(&callback_);
  pacer.UpdateOutstandingData(DataSize::Zero());
  EXPECT_CALL(callback_, SendPacket).Times(1);
  ProcessNext(&pacer);
}

TEST_F(PacingControllerFieldTrialTest,
       CongestionWindowDoesNotAffectAudioInTrial) {
  ScopedFieldTrials trial("WebRTC-Pacer-BlockAudio/Disabled/");
  EXPECT_CALL(callback_, SendPadding).Times(0);
  PacingController pacer(&clock_, &callback_, nullptr, nullptr);
  pacer.SetPacingRates(DataRate::bps(10000000), DataRate::Zero());
  pacer.SetCongestionWindow(DataSize::bytes(800));
  pacer.UpdateOutstandingData(DataSize::Zero());
  // Video packet fills congestion window.
  InsertPacket(&pacer, &video);
  EXPECT_CALL(callback_, SendPacket).Times(1);
  ProcessNext(&pacer);
  // Audio not blocked due to congestion.
  InsertPacket(&pacer, &audio);
  EXPECT_CALL(callback_, SendPacket).Times(1);
  ProcessNext(&pacer);
}

TEST_F(PacingControllerFieldTrialTest, DefaultBudgetAffectsAudio) {
  PacingController pacer(&clock_, &callback_, nullptr, nullptr);
  pacer.SetPacingRates(
      DataRate::bps(video.packet_size / 3 * 8 * kProcessIntervalsPerSecond),
      DataRate::Zero());
  // Video fills budget for following process periods.
  InsertPacket(&pacer, &video);
  EXPECT_CALL(callback_, SendPacket).Times(1);
  ProcessNext(&pacer);
  // Audio packet blocked due to budget limit.
  EXPECT_CALL(callback_, SendPacket).Times(0);
  InsertPacket(&pacer, &audio);
  ProcessNext(&pacer);
  ProcessNext(&pacer);
  ::testing::Mock::VerifyAndClearExpectations(&callback_);
  // Audio packet unblocked when the budget has recovered.
  EXPECT_CALL(callback_, SendPacket).Times(1);
  ProcessNext(&pacer);
  ProcessNext(&pacer);
}

TEST_F(PacingControllerFieldTrialTest, BudgetDoesNotAffectAudioInTrial) {
  ScopedFieldTrials trial("WebRTC-Pacer-BlockAudio/Disabled/");
  EXPECT_CALL(callback_, SendPadding).Times(0);
  PacingController pacer(&clock_, &callback_, nullptr, nullptr);
  pacer.SetPacingRates(
      DataRate::bps(video.packet_size / 3 * 8 * kProcessIntervalsPerSecond),
      DataRate::Zero());
  // Video fills budget for following process periods.
  InsertPacket(&pacer, &video);
  EXPECT_CALL(callback_, SendPacket).Times(1);
  ProcessNext(&pacer);
  // Audio packet not blocked due to budget limit.
  EXPECT_CALL(callback_, SendPacket).Times(1);
  InsertPacket(&pacer, &audio);
  ProcessNext(&pacer);
}

TEST_F(PacingControllerTest, FirstSentPacketTimeIsSet) {
  uint16_t sequence_number = 1234;
  const uint32_t kSsrc = 12345;
  const size_t kSizeBytes = 250;
  const size_t kPacketToSend = 3;
  const Timestamp kStartTime = clock_.CurrentTime();

  // No packet sent.
  EXPECT_FALSE(pacer_->FirstSentPacketTime().has_value());

  for (size_t i = 0; i < kPacketToSend; ++i) {
    SendAndExpectPacket(RtpPacketToSend::Type::kVideo, kSsrc, sequence_number++,
                        clock_.TimeInMilliseconds(), kSizeBytes);
    pacer_->ProcessPackets();
    clock_.AdvanceTime(TimeUntilNextProcess());
  }
  EXPECT_EQ(kStartTime, pacer_->FirstSentPacketTime());
}

TEST_F(PacingControllerTest, QueuePacket) {
  uint32_t ssrc = 12345;
  uint16_t sequence_number = 1234;
  // Due to the multiplicative factor we can send 5 packets during a send
  // interval. (network capacity * multiplier / (8 bits per byte *
  // (packet size * #send intervals per second)
  const size_t packets_to_send =
      kTargetRate.bps() * kPaceMultiplier / (8 * 250 * 200);
  for (size_t i = 0; i < packets_to_send; ++i) {
    SendAndExpectPacket(RtpPacketToSend::Type::kVideo, ssrc, sequence_number++,
                        clock_.TimeInMilliseconds(), 250);
  }

  int64_t queued_packet_timestamp = clock_.TimeInMilliseconds();
  Send(RtpPacketToSend::Type::kVideo, ssrc, sequence_number,
       queued_packet_timestamp, 250);
  EXPECT_EQ(packets_to_send + 1, pacer_->QueueSizePackets());
  pacer_->ProcessPackets();
  EXPECT_CALL(callback_, SendPadding).Times(0);
  clock_.AdvanceTimeMilliseconds(5);
  EXPECT_EQ(1u, pacer_->QueueSizePackets());
  EXPECT_CALL(callback_, SendPacket(ssrc, sequence_number++,
                                    queued_packet_timestamp, false, false))
      .Times(1);
  pacer_->ProcessPackets();
  sequence_number++;
  EXPECT_EQ(0u, pacer_->QueueSizePackets());

  // We can send packets_to_send -1 packets of size 250 during the current
  // interval since one packet has already been sent.
  for (size_t i = 0; i < packets_to_send - 1; ++i) {
    SendAndExpectPacket(RtpPacketToSend::Type::kVideo, ssrc, sequence_number++,
                        clock_.TimeInMilliseconds(), 250);
  }
  Send(RtpPacketToSend::Type::kVideo, ssrc, sequence_number++,
       clock_.TimeInMilliseconds(), 250);
  EXPECT_EQ(packets_to_send, pacer_->QueueSizePackets());
  pacer_->ProcessPackets();
  EXPECT_EQ(1u, pacer_->QueueSizePackets());
}

TEST_F(PacingControllerTest, PaceQueuedPackets) {
  uint32_t ssrc = 12345;
  uint16_t sequence_number = 1234;

  // Due to the multiplicative factor we can send 5 packets during a send
  // interval. (network capacity * multiplier / (8 bits per byte *
  // (packet size * #send intervals per second)
  const size_t packets_to_send_per_interval =
      kTargetRate.bps() * kPaceMultiplier / (8 * 250 * 200);
  for (size_t i = 0; i < packets_to_send_per_interval; ++i) {
    SendAndExpectPacket(RtpPacketToSend::Type::kVideo, ssrc, sequence_number++,
                        clock_.TimeInMilliseconds(), 250);
  }

  for (size_t j = 0; j < packets_to_send_per_interval * 10; ++j) {
    Send(RtpPacketToSend::Type::kVideo, ssrc, sequence_number++,
         clock_.TimeInMilliseconds(), 250);
  }
  EXPECT_EQ(packets_to_send_per_interval + packets_to_send_per_interval * 10,
            pacer_->QueueSizePackets());
  pacer_->ProcessPackets();
  EXPECT_EQ(packets_to_send_per_interval * 10, pacer_->QueueSizePackets());
  EXPECT_CALL(callback_, SendPadding).Times(0);
  for (int k = 0; k < 10; ++k) {
    clock_.AdvanceTime(TimeUntilNextProcess());
    EXPECT_CALL(callback_, SendPacket(ssrc, _, _, false, false))
        .Times(packets_to_send_per_interval);
    pacer_->ProcessPackets();
  }
  EXPECT_EQ(0u, pacer_->QueueSizePackets());
  clock_.AdvanceTime(TimeUntilNextProcess());
  EXPECT_EQ(0u, pacer_->QueueSizePackets());
  pacer_->ProcessPackets();

  for (size_t i = 0; i < packets_to_send_per_interval; ++i) {
    SendAndExpectPacket(RtpPacketToSend::Type::kVideo, ssrc, sequence_number++,
                        clock_.TimeInMilliseconds(), 250);
  }
  Send(RtpPacketToSend::Type::kVideo, ssrc, sequence_number,
       clock_.TimeInMilliseconds(), 250);
  pacer_->ProcessPackets();
  EXPECT_EQ(1u, pacer_->QueueSizePackets());
}

TEST_F(PacingControllerTest, RepeatedRetransmissionsAllowed) {
  // Send one packet, then two retransmissions of that packet.
  for (size_t i = 0; i < 3; i++) {
    constexpr uint32_t ssrc = 333;
    constexpr uint16_t sequence_number = 444;
    constexpr size_t bytes = 250;
    bool is_retransmission = (i != 0);  // Original followed by retransmissions.
    SendAndExpectPacket(
        is_retransmission ? RtpPacketToSend::Type::kRetransmission
                          : RtpPacketToSend::Type::kVideo,
        ssrc, sequence_number, clock_.TimeInMilliseconds(), bytes);
    clock_.AdvanceTimeMilliseconds(5);
  }
  pacer_->ProcessPackets();
}

TEST_F(PacingControllerTest,
       CanQueuePacketsWithSameSequenceNumberOnDifferentSsrcs) {
  uint32_t ssrc = 12345;
  uint16_t sequence_number = 1234;

  SendAndExpectPacket(RtpPacketToSend::Type::kVideo, ssrc, sequence_number,
                      clock_.TimeInMilliseconds(), 250);

  // Expect packet on second ssrc to be queued and sent as well.
  SendAndExpectPacket(RtpPacketToSend::Type::kVideo, ssrc + 1, sequence_number,
                      clock_.TimeInMilliseconds(), 250);

  clock_.AdvanceTimeMilliseconds(1000);
  pacer_->ProcessPackets();
}

TEST_F(PacingControllerTest, Padding) {
  uint32_t ssrc = 12345;
  uint16_t sequence_number = 1234;

  pacer_->SetPacingRates(kTargetRate * kPaceMultiplier, kTargetRate);

  // Due to the multiplicative factor we can send 5 packets during a send
  // interval. (network capacity * multiplier / (8 bits per byte *
  // (packet size * #send intervals per second)
  const size_t packets_to_send_per_interval =
      kTargetRate.bps() * kPaceMultiplier / (8 * 250 * 200);
  for (size_t i = 0; i < packets_to_send_per_interval; ++i) {
    SendAndExpectPacket(RtpPacketToSend::Type::kVideo, ssrc, sequence_number++,
                        clock_.TimeInMilliseconds(), 250);
  }
  // No padding is expected since we have sent too much already.
  EXPECT_CALL(callback_, SendPadding).Times(0);
  pacer_->ProcessPackets();
  EXPECT_EQ(0u, pacer_->QueueSizePackets());

  // 5 milliseconds later should not send padding since we filled the buffers
  // initially.
  EXPECT_CALL(callback_, SendPadding(250)).Times(0);
  clock_.AdvanceTime(TimeUntilNextProcess());
  pacer_->ProcessPackets();

  // 5 milliseconds later we have enough budget to send some padding.
  EXPECT_CALL(callback_, SendPadding(250)).WillOnce(Return(250));
  EXPECT_CALL(callback_, SendPacket(_, _, _, _, true)).Times(1);
  clock_.AdvanceTime(TimeUntilNextProcess());
  pacer_->ProcessPackets();
}

TEST_F(PacingControllerTest, NoPaddingBeforeNormalPacket) {
  pacer_->SetPacingRates(kTargetRate * kPaceMultiplier, kTargetRate);

  EXPECT_CALL(callback_, SendPadding).Times(0);
  pacer_->ProcessPackets();
  clock_.AdvanceTime(TimeUntilNextProcess());

  pacer_->ProcessPackets();
  clock_.AdvanceTime(TimeUntilNextProcess());

  uint32_t ssrc = 12345;
  uint16_t sequence_number = 1234;
  int64_t capture_time_ms = 56789;

  SendAndExpectPacket(RtpPacketToSend::Type::kVideo, ssrc, sequence_number++,
                      capture_time_ms, 250);
  EXPECT_CALL(callback_, SendPadding(250)).WillOnce(Return(250));
  EXPECT_CALL(callback_, SendPacket(_, _, _, _, true)).Times(1);
  pacer_->ProcessPackets();
}

TEST_F(PacingControllerTest, VerifyPaddingUpToBitrate) {
  uint32_t ssrc = 12345;
  uint16_t sequence_number = 1234;
  int64_t capture_time_ms = 56789;
  const int kTimeStep = 5;
  const int64_t kBitrateWindow = 100;
  pacer_->SetPacingRates(kTargetRate * kPaceMultiplier, kTargetRate);

  int64_t start_time = clock_.TimeInMilliseconds();
  while (clock_.TimeInMilliseconds() - start_time < kBitrateWindow) {
    SendAndExpectPacket(RtpPacketToSend::Type::kVideo, ssrc, sequence_number++,
                        capture_time_ms, 250);
    EXPECT_CALL(callback_, SendPadding(250)).WillOnce(Return(250));
    EXPECT_CALL(callback_, SendPacket(_, _, _, _, true)).Times(1);
    pacer_->ProcessPackets();
    clock_.AdvanceTimeMilliseconds(kTimeStep);
  }
}

TEST_F(PacingControllerTest, VerifyAverageBitrateVaryingMediaPayload) {
  uint32_t ssrc = 12345;
  uint16_t sequence_number = 1234;
  int64_t capture_time_ms = 56789;
  const int kTimeStep = 5;
  const int64_t kBitrateWindow = 10000;
  PacingControllerPadding callback;
  pacer_ =
      absl::make_unique<PacingController>(&clock_, &callback, nullptr, nullptr);
  pacer_->SetProbingEnabled(false);
  pacer_->SetPacingRates(kTargetRate * kPaceMultiplier, kTargetRate);

  int64_t start_time = clock_.TimeInMilliseconds();
  size_t media_bytes = 0;
  while (clock_.TimeInMilliseconds() - start_time < kBitrateWindow) {
    int rand_value = rand();  // NOLINT (rand_r instead of rand)
    size_t media_payload = rand_value % 100 + 200;  // [200, 300] bytes.
    Send(RtpPacketToSend::Type::kVideo, ssrc, sequence_number++,
         capture_time_ms, media_payload);
    media_bytes += media_payload;
    clock_.AdvanceTimeMilliseconds(kTimeStep);
    pacer_->ProcessPackets();
  }
  EXPECT_NEAR(kTargetRate.kbps(),
              static_cast<int>(8 * (media_bytes + callback.padding_sent()) /
                               kBitrateWindow),
              1);
}

TEST_F(PacingControllerTest, Priority) {
  uint32_t ssrc_low_priority = 12345;
  uint32_t ssrc = 12346;
  uint16_t sequence_number = 1234;
  int64_t capture_time_ms = 56789;
  int64_t capture_time_ms_low_priority = 1234567;

  // Due to the multiplicative factor we can send 5 packets during a send
  // interval. (network capacity * multiplier / (8 bits per byte *
  // (packet size * #send intervals per second)
  const size_t packets_to_send_per_interval =
      kTargetRate.bps() * kPaceMultiplier / (8 * 250 * 200);
  for (size_t i = 0; i < packets_to_send_per_interval; ++i) {
    SendAndExpectPacket(RtpPacketToSend::Type::kRetransmission, ssrc,
                        sequence_number++, clock_.TimeInMilliseconds(), 250);
  }
  pacer_->ProcessPackets();
  EXPECT_EQ(0u, pacer_->QueueSizePackets());

  // Expect normal and low priority to be queued and high to pass through.
  Send(RtpPacketToSend::Type::kVideo, ssrc_low_priority, sequence_number++,
       capture_time_ms_low_priority, 250);

  for (size_t i = 0; i < packets_to_send_per_interval; ++i) {
    Send(RtpPacketToSend::Type::kRetransmission, ssrc, sequence_number++,
         capture_time_ms, 250);
  }
  Send(RtpPacketToSend::Type::kAudio, ssrc, sequence_number++, capture_time_ms,
       250);

  // Expect all high and normal priority to be sent out first.
  EXPECT_CALL(callback_, SendPadding).Times(0);
  EXPECT_CALL(callback_, SendPacket(ssrc, _, capture_time_ms, _, _))
      .Times(packets_to_send_per_interval + 1);

  clock_.AdvanceTime(TimeUntilNextProcess());
  pacer_->ProcessPackets();
  EXPECT_EQ(1u, pacer_->QueueSizePackets());

  EXPECT_CALL(callback_, SendPacket(ssrc_low_priority, _,
                                    capture_time_ms_low_priority, _, _))
      .Times(1);

  clock_.AdvanceTime(TimeUntilNextProcess());
  pacer_->ProcessPackets();
}

TEST_F(PacingControllerTest, RetransmissionPriority) {
  uint32_t ssrc = 12345;
  uint16_t sequence_number = 1234;
  int64_t capture_time_ms = 45678;
  int64_t capture_time_ms_retransmission = 56789;

  // Due to the multiplicative factor we can send 5 packets during a send
  // interval. (network capacity * multiplier / (8 bits per byte *
  // (packet size * #send intervals per second)
  const size_t packets_to_send_per_interval =
      kTargetRate.bps() * kPaceMultiplier / (8 * 250 * 200);
  pacer_->ProcessPackets();
  EXPECT_EQ(0u, pacer_->QueueSizePackets());

  // Alternate retransmissions and normal packets.
  for (size_t i = 0; i < packets_to_send_per_interval; ++i) {
    Send(RtpPacketToSend::Type::kVideo, ssrc, sequence_number++,
         capture_time_ms, 250);
    Send(RtpPacketToSend::Type::kRetransmission, ssrc, sequence_number++,
         capture_time_ms_retransmission, 250);
  }
  EXPECT_EQ(2 * packets_to_send_per_interval, pacer_->QueueSizePackets());

  // Expect all retransmissions to be sent out first despite having a later
  // capture time.
  EXPECT_CALL(callback_, SendPadding).Times(0);
  EXPECT_CALL(callback_, SendPacket(_, _, _, false, _)).Times(0);
  EXPECT_CALL(callback_,
              SendPacket(ssrc, _, capture_time_ms_retransmission, true, _))
      .Times(packets_to_send_per_interval);

  clock_.AdvanceTime(TimeUntilNextProcess());
  pacer_->ProcessPackets();
  EXPECT_EQ(packets_to_send_per_interval, pacer_->QueueSizePackets());

  // Expect the remaining (non-retransmission) packets to be sent.
  EXPECT_CALL(callback_, SendPadding).Times(0);
  EXPECT_CALL(callback_, SendPacket(_, _, _, true, _)).Times(0);
  EXPECT_CALL(callback_, SendPacket(ssrc, _, capture_time_ms, false, _))
      .Times(packets_to_send_per_interval);

  clock_.AdvanceTime(TimeUntilNextProcess());
  pacer_->ProcessPackets();

  EXPECT_EQ(0u, pacer_->QueueSizePackets());
}

TEST_F(PacingControllerTest, HighPrioDoesntAffectBudget) {
  uint32_t ssrc = 12346;
  uint16_t sequence_number = 1234;
  int64_t capture_time_ms = 56789;

  // As high prio packets doesn't affect the budget, we should be able to send
  // a high number of them at once.
  for (int i = 0; i < 25; ++i) {
    SendAndExpectPacket(RtpPacketToSend::Type::kAudio, ssrc, sequence_number++,
                        capture_time_ms, 250);
  }
  pacer_->ProcessPackets();
  // Low prio packets does affect the budget.
  // Due to the multiplicative factor we can send 5 packets during a send
  // interval. (network capacity * multiplier / (8 bits per byte *
  // (packet size * #send intervals per second)
  const size_t packets_to_send_per_interval =
      kTargetRate.bps() * kPaceMultiplier / (8 * 250 * 200);
  for (size_t i = 0; i < packets_to_send_per_interval; ++i) {
    SendAndExpectPacket(RtpPacketToSend::Type::kVideo, ssrc, sequence_number++,
                        clock_.TimeInMilliseconds(), 250);
  }
  Send(RtpPacketToSend::Type::kVideo, ssrc, sequence_number, capture_time_ms,
       250);
  clock_.AdvanceTime(TimeUntilNextProcess());
  pacer_->ProcessPackets();
  EXPECT_EQ(1u, pacer_->QueueSizePackets());
  EXPECT_CALL(callback_,
              SendPacket(ssrc, sequence_number++, capture_time_ms, false, _))
      .Times(1);
  clock_.AdvanceTime(TimeUntilNextProcess());
  pacer_->ProcessPackets();
  EXPECT_EQ(0u, pacer_->QueueSizePackets());
}

TEST_F(PacingControllerTest, SendsOnlyPaddingWhenCongested) {
  uint32_t ssrc = 202020;
  uint16_t sequence_number = 1000;
  int kPacketSize = 250;
  int kCongestionWindow = kPacketSize * 10;

  pacer_->UpdateOutstandingData(DataSize::Zero());
  pacer_->SetCongestionWindow(DataSize::bytes(kCongestionWindow));
  int sent_data = 0;
  while (sent_data < kCongestionWindow) {
    sent_data += kPacketSize;
    SendAndExpectPacket(RtpPacketToSend::Type::kVideo, ssrc, sequence_number++,
                        clock_.TimeInMilliseconds(), kPacketSize);
    clock_.AdvanceTimeMilliseconds(5);
    pacer_->ProcessPackets();
  }
  ::testing::Mock::VerifyAndClearExpectations(&callback_);
  EXPECT_CALL(callback_, SendPacket).Times(0);
  EXPECT_CALL(callback_, SendPadding).Times(0);

  size_t blocked_packets = 0;
  int64_t expected_time_until_padding = 500;
  while (expected_time_until_padding > 5) {
    Send(RtpPacketToSend::Type::kVideo, ssrc, sequence_number++,
         clock_.TimeInMilliseconds(), kPacketSize);
    blocked_packets++;
    clock_.AdvanceTimeMilliseconds(5);
    pacer_->ProcessPackets();
    expected_time_until_padding -= 5;
  }
  ::testing::Mock::VerifyAndClearExpectations(&callback_);
  EXPECT_CALL(callback_, SendPadding(1)).WillOnce(Return(1));
  EXPECT_CALL(callback_, SendPacket(_, _, _, _, true)).Times(1);
  clock_.AdvanceTimeMilliseconds(5);
  pacer_->ProcessPackets();
  EXPECT_EQ(blocked_packets, pacer_->QueueSizePackets());
}

TEST_F(PacingControllerTest, DoesNotAllowOveruseAfterCongestion) {
  uint32_t ssrc = 202020;
  uint16_t seq_num = 1000;
  int size = 1000;
  auto now_ms = [this] { return clock_.TimeInMilliseconds(); };
  EXPECT_CALL(callback_, SendPadding).Times(0);
  // The pacing rate is low enough that the budget should not allow two packets
  // to be sent in a row.
  pacer_->SetPacingRates(DataRate::bps(400 * 8 * 1000 / 5), DataRate::Zero());
  // The congestion window is small enough to only let one packet through.
  pacer_->SetCongestionWindow(DataSize::bytes(800));
  pacer_->UpdateOutstandingData(DataSize::Zero());
  // Not yet budget limited or congested, packet is sent.
  Send(RtpPacketToSend::Type::kVideo, ssrc, seq_num++, now_ms(), size);
  EXPECT_CALL(callback_, SendPacket).Times(1);
  clock_.AdvanceTimeMilliseconds(5);
  pacer_->ProcessPackets();
  // Packet blocked due to congestion.
  Send(RtpPacketToSend::Type::kVideo, ssrc, seq_num++, now_ms(), size);
  EXPECT_CALL(callback_, SendPacket).Times(0);
  clock_.AdvanceTimeMilliseconds(5);
  pacer_->ProcessPackets();
  // Packet blocked due to congestion.
  Send(RtpPacketToSend::Type::kVideo, ssrc, seq_num++, now_ms(), size);
  EXPECT_CALL(callback_, SendPacket).Times(0);
  clock_.AdvanceTimeMilliseconds(5);
  pacer_->ProcessPackets();
  pacer_->UpdateOutstandingData(DataSize::Zero());
  // Congestion removed and budget has recovered, packet is sent.
  Send(RtpPacketToSend::Type::kVideo, ssrc, seq_num++, now_ms(), size);
  EXPECT_CALL(callback_, SendPacket).Times(1);
  clock_.AdvanceTimeMilliseconds(5);
  pacer_->ProcessPackets();
  pacer_->UpdateOutstandingData(DataSize::Zero());
  // Should be blocked due to budget limitation as congestion has be removed.
  Send(RtpPacketToSend::Type::kVideo, ssrc, seq_num++, now_ms(), size);
  EXPECT_CALL(callback_, SendPacket).Times(0);
  clock_.AdvanceTimeMilliseconds(5);
  pacer_->ProcessPackets();
}

TEST_F(PacingControllerTest, ResumesSendingWhenCongestionEnds) {
  uint32_t ssrc = 202020;
  uint16_t sequence_number = 1000;
  int64_t kPacketSize = 250;
  int64_t kCongestionCount = 10;
  int64_t kCongestionWindow = kPacketSize * kCongestionCount;
  int64_t kCongestionTimeMs = 1000;

  pacer_->UpdateOutstandingData(DataSize::Zero());
  pacer_->SetCongestionWindow(DataSize::bytes(kCongestionWindow));
  int sent_data = 0;
  while (sent_data < kCongestionWindow) {
    sent_data += kPacketSize;
    SendAndExpectPacket(RtpPacketToSend::Type::kVideo, ssrc, sequence_number++,
                        clock_.TimeInMilliseconds(), kPacketSize);
    clock_.AdvanceTimeMilliseconds(5);
    pacer_->ProcessPackets();
  }
  ::testing::Mock::VerifyAndClearExpectations(&callback_);
  EXPECT_CALL(callback_, SendPacket).Times(0);
  int unacked_packets = 0;
  for (int duration = 0; duration < kCongestionTimeMs; duration += 5) {
    Send(RtpPacketToSend::Type::kVideo, ssrc, sequence_number++,
         clock_.TimeInMilliseconds(), kPacketSize);
    unacked_packets++;
    clock_.AdvanceTimeMilliseconds(5);
    pacer_->ProcessPackets();
  }
  ::testing::Mock::VerifyAndClearExpectations(&callback_);

  // First mark half of the congested packets as cleared and make sure that just
  // as many are sent
  int ack_count = kCongestionCount / 2;
  EXPECT_CALL(callback_, SendPacket(ssrc, _, _, false, _)).Times(ack_count);
  pacer_->UpdateOutstandingData(
      DataSize::bytes(kCongestionWindow - kPacketSize * ack_count));

  for (int duration = 0; duration < kCongestionTimeMs; duration += 5) {
    clock_.AdvanceTimeMilliseconds(5);
    pacer_->ProcessPackets();
  }
  unacked_packets -= ack_count;
  ::testing::Mock::VerifyAndClearExpectations(&callback_);

  // Second make sure all packets are sent if sent packets are continuously
  // marked as acked.
  EXPECT_CALL(callback_, SendPacket(ssrc, _, _, false, _))
      .Times(unacked_packets);
  for (int duration = 0; duration < kCongestionTimeMs; duration += 5) {
    pacer_->UpdateOutstandingData(DataSize::Zero());
    clock_.AdvanceTimeMilliseconds(5);
    pacer_->ProcessPackets();
  }
}

TEST_F(PacingControllerTest, Pause) {
  uint32_t ssrc_low_priority = 12345;
  uint32_t ssrc = 12346;
  uint32_t ssrc_high_priority = 12347;
  uint16_t sequence_number = 1234;
  int64_t capture_time_ms = clock_.TimeInMilliseconds();

  EXPECT_EQ(TimeDelta::Zero(), pacer_->OldestPacketWaitTime());

  // Due to the multiplicative factor we can send 5 packets during a send
  // interval. (network capacity * multiplier / (8 bits per byte *
  // (packet size * #send intervals per second)
  const size_t packets_to_send_per_interval =
      kTargetRate.bps() * kPaceMultiplier / (8 * 250 * 200);
  for (size_t i = 0; i < packets_to_send_per_interval; ++i) {
    SendAndExpectPacket(RtpPacketToSend::Type::kVideo, ssrc, sequence_number++,
                        clock_.TimeInMilliseconds(), 250);
  }

  pacer_->ProcessPackets();

  pacer_->Pause();

  for (size_t i = 0; i < packets_to_send_per_interval; ++i) {
    Send(RtpPacketToSend::Type::kVideo, ssrc_low_priority, sequence_number++,
         capture_time_ms, 250);
    Send(RtpPacketToSend::Type::kRetransmission, ssrc, sequence_number++,
         capture_time_ms, 250);
    Send(RtpPacketToSend::Type::kAudio, ssrc_high_priority, sequence_number++,
         capture_time_ms, 250);
  }
  clock_.AdvanceTimeMilliseconds(10000);
  int64_t second_capture_time_ms = clock_.TimeInMilliseconds();
  for (size_t i = 0; i < packets_to_send_per_interval; ++i) {
    Send(RtpPacketToSend::Type::kVideo, ssrc_low_priority, sequence_number++,
         second_capture_time_ms, 250);
    Send(RtpPacketToSend::Type::kRetransmission, ssrc, sequence_number++,
         second_capture_time_ms, 250);
    Send(RtpPacketToSend::Type::kAudio, ssrc_high_priority, sequence_number++,
         second_capture_time_ms, 250);
  }

  // Expect everything to be queued.
  EXPECT_EQ(TimeDelta::ms(second_capture_time_ms - capture_time_ms),
            pacer_->OldestPacketWaitTime());

  EXPECT_CALL(callback_, SendPadding(1)).WillOnce(Return(1));
  EXPECT_CALL(callback_, SendPacket(_, _, _, _, true)).Times(1);
  pacer_->ProcessPackets();

  int64_t expected_time_until_send = 500;
  EXPECT_CALL(callback_, SendPadding).Times(0);
  while (expected_time_until_send >= 5) {
    pacer_->ProcessPackets();
    clock_.AdvanceTimeMilliseconds(5);
    expected_time_until_send -= 5;
  }

  ::testing::Mock::VerifyAndClearExpectations(&callback_);
  EXPECT_CALL(callback_, SendPadding(1)).WillOnce(Return(1));
  EXPECT_CALL(callback_, SendPacket(_, _, _, _, true)).Times(1);
  clock_.AdvanceTimeMilliseconds(5);
  pacer_->ProcessPackets();
  ::testing::Mock::VerifyAndClearExpectations(&callback_);

  // Expect high prio packets to come out first followed by normal
  // prio packets and low prio packets (all in capture order).
  {
    ::testing::InSequence sequence;
    EXPECT_CALL(callback_,
                SendPacket(ssrc_high_priority, _, capture_time_ms, _, _))
        .Times(packets_to_send_per_interval);
    EXPECT_CALL(callback_,
                SendPacket(ssrc_high_priority, _, second_capture_time_ms, _, _))
        .Times(packets_to_send_per_interval);

    for (size_t i = 0; i < packets_to_send_per_interval; ++i) {
      EXPECT_CALL(callback_, SendPacket(ssrc, _, capture_time_ms, _, _))
          .Times(1);
    }
    for (size_t i = 0; i < packets_to_send_per_interval; ++i) {
      EXPECT_CALL(callback_, SendPacket(ssrc, _, second_capture_time_ms, _, _))
          .Times(1);
    }
    for (size_t i = 0; i < packets_to_send_per_interval; ++i) {
      EXPECT_CALL(callback_,
                  SendPacket(ssrc_low_priority, _, capture_time_ms, _, _))
          .Times(1);
    }
    for (size_t i = 0; i < packets_to_send_per_interval; ++i) {
      EXPECT_CALL(callback_, SendPacket(ssrc_low_priority, _,
                                        second_capture_time_ms, _, _))
          .Times(1);
    }
  }
  pacer_->Resume();

  // The pacer was resumed directly after the previous process call finished. It
  // will therefore wait 5 ms until next process.
  clock_.AdvanceTime(TimeUntilNextProcess());

  for (size_t i = 0; i < 4; i++) {
    pacer_->ProcessPackets();
    clock_.AdvanceTime(TimeUntilNextProcess());
  }

  EXPECT_EQ(TimeDelta::Zero(), pacer_->OldestPacketWaitTime());
}

TEST_F(PacingControllerTest, ExpectedQueueTimeMs) {
  uint32_t ssrc = 12346;
  uint16_t sequence_number = 1234;
  const size_t kNumPackets = 60;
  const size_t kPacketSize = 1200;
  const int32_t kMaxBitrate = kPaceMultiplier * 30000;
  EXPECT_EQ(TimeDelta::Zero(), pacer_->OldestPacketWaitTime());

  pacer_->SetPacingRates(DataRate::bps(30000 * kPaceMultiplier),
                         DataRate::Zero());
  for (size_t i = 0; i < kNumPackets; ++i) {
    SendAndExpectPacket(RtpPacketToSend::Type::kVideo, ssrc, sequence_number++,
                        clock_.TimeInMilliseconds(), kPacketSize);
  }

  // Queue in ms = 1000 * (bytes in queue) *8 / (bits per second)
  TimeDelta queue_time =
      TimeDelta::ms(1000 * kNumPackets * kPacketSize * 8 / kMaxBitrate);
  EXPECT_EQ(queue_time, pacer_->ExpectedQueueTime());

  const Timestamp time_start = clock_.CurrentTime();
  while (pacer_->QueueSizePackets() > 0) {
    clock_.AdvanceTime(TimeUntilNextProcess());
    pacer_->ProcessPackets();
  }
  TimeDelta duration = clock_.CurrentTime() - time_start;

  EXPECT_EQ(TimeDelta::Zero(), pacer_->ExpectedQueueTime());

  // Allow for aliasing, duration should be within one pack of max time limit.
  const TimeDelta deviation =
      duration - PacingController::kMaxExpectedQueueLength;
  EXPECT_LT(deviation.Abs(),
            TimeDelta::ms(1000 * kPacketSize * 8 / kMaxBitrate));
}

TEST_F(PacingControllerTest, QueueTimeGrowsOverTime) {
  uint32_t ssrc = 12346;
  uint16_t sequence_number = 1234;
  EXPECT_EQ(TimeDelta::Zero(), pacer_->OldestPacketWaitTime());

  pacer_->SetPacingRates(DataRate::bps(30000 * kPaceMultiplier),
                         DataRate::Zero());
  SendAndExpectPacket(RtpPacketToSend::Type::kVideo, ssrc, sequence_number,
                      clock_.TimeInMilliseconds(), 1200);

  clock_.AdvanceTimeMilliseconds(500);
  EXPECT_EQ(TimeDelta::ms(500), pacer_->OldestPacketWaitTime());
  pacer_->ProcessPackets();
  EXPECT_EQ(TimeDelta::Zero(), pacer_->OldestPacketWaitTime());
}

TEST_F(PacingControllerTest, ProbingWithInsertedPackets) {
  const size_t kPacketSize = 1200;
  const int kInitialBitrateBps = 300000;
  uint32_t ssrc = 12346;
  uint16_t sequence_number = 1234;

  PacingControllerProbing packet_sender;
  pacer_ = absl::make_unique<PacingController>(&clock_, &packet_sender, nullptr,
                                               nullptr);
  pacer_->CreateProbeCluster(kFirstClusterRate,
                             /*cluster_id=*/0);
  pacer_->CreateProbeCluster(kSecondClusterRate,
                             /*cluster_id=*/1);
  pacer_->SetPacingRates(DataRate::bps(kInitialBitrateBps * kPaceMultiplier),
                         DataRate::Zero());

  for (int i = 0; i < 10; ++i) {
    Send(RtpPacketToSend::Type::kVideo, ssrc, sequence_number++,
         clock_.TimeInMilliseconds(), kPacketSize);
  }

  int64_t start = clock_.TimeInMilliseconds();
  while (packet_sender.packets_sent() < 5) {
    clock_.AdvanceTime(TimeUntilNextProcess());
    pacer_->ProcessPackets();
  }
  int packets_sent = packet_sender.packets_sent();
  // Validate first cluster bitrate. Note that we have to account for number
  // of intervals and hence (packets_sent - 1) on the first cluster.
  EXPECT_NEAR((packets_sent - 1) * kPacketSize * 8000 /
                  (clock_.TimeInMilliseconds() - start),
              kFirstClusterRate.bps(), kProbingErrorMargin.bps());
  EXPECT_EQ(0, packet_sender.padding_sent());

  clock_.AdvanceTime(TimeUntilNextProcess());
  start = clock_.TimeInMilliseconds();
  while (packet_sender.packets_sent() < 10) {
    clock_.AdvanceTime(TimeUntilNextProcess());
    pacer_->ProcessPackets();
  }
  packets_sent = packet_sender.packets_sent() - packets_sent;
  // Validate second cluster bitrate.
  EXPECT_NEAR((packets_sent - 1) * kPacketSize * 8000 /
                  (clock_.TimeInMilliseconds() - start),
              kSecondClusterRate.bps(), kProbingErrorMargin.bps());
}

TEST_F(PacingControllerTest, ProbingWithPaddingSupport) {
  const size_t kPacketSize = 1200;
  const int kInitialBitrateBps = 300000;
  uint32_t ssrc = 12346;
  uint16_t sequence_number = 1234;

  PacingControllerProbing packet_sender;
  pacer_ = absl::make_unique<PacingController>(&clock_, &packet_sender, nullptr,
                                               nullptr);
  pacer_->CreateProbeCluster(kFirstClusterRate,
                             /*cluster_id=*/0);
  pacer_->SetPacingRates(DataRate::bps(kInitialBitrateBps * kPaceMultiplier),
                         DataRate::Zero());

  for (int i = 0; i < 3; ++i) {
    Send(RtpPacketToSend::Type::kVideo, ssrc, sequence_number++,
         clock_.TimeInMilliseconds(), kPacketSize);
  }

  int64_t start = clock_.TimeInMilliseconds();
  int process_count = 0;
  while (process_count < 5) {
    clock_.AdvanceTime(TimeUntilNextProcess());
    pacer_->ProcessPackets();
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
              kFirstClusterRate.bps(), kProbingErrorMargin.bps());
}

TEST_F(PacingControllerTest, PaddingOveruse) {
  uint32_t ssrc = 12346;
  uint16_t sequence_number = 1234;
  const size_t kPacketSize = 1200;

  pacer_->ProcessPackets();
  pacer_->SetPacingRates(DataRate::bps(60000 * kPaceMultiplier),
                         DataRate::Zero());

  SendAndExpectPacket(RtpPacketToSend::Type::kVideo, ssrc, sequence_number++,
                      clock_.TimeInMilliseconds(), kPacketSize);
  pacer_->ProcessPackets();

  // Add 30kbit padding. When increasing budget, media budget will increase from
  // negative (overuse) while padding budget will increase from 0.
  clock_.AdvanceTimeMilliseconds(5);
  pacer_->SetPacingRates(DataRate::bps(60000 * kPaceMultiplier),
                         DataRate::bps(30000));

  SendAndExpectPacket(RtpPacketToSend::Type::kVideo, ssrc, sequence_number++,
                      clock_.TimeInMilliseconds(), kPacketSize);
  EXPECT_LT(TimeDelta::ms(5), pacer_->ExpectedQueueTime());
  // Don't send padding if queue is non-empty, even if padding budget > 0.
  EXPECT_CALL(callback_, SendPadding).Times(0);
  pacer_->ProcessPackets();
}

TEST_F(PacingControllerTest, ProbeClusterId) {
  MockPacketSender callback;

  pacer_ =
      absl::make_unique<PacingController>(&clock_, &callback, nullptr, nullptr);
  Init();

  uint32_t ssrc = 12346;
  uint16_t sequence_number = 1234;
  const size_t kPacketSize = 1200;

  pacer_->SetPacingRates(kTargetRate * kPaceMultiplier, kTargetRate);
  pacer_->SetProbingEnabled(true);
  for (int i = 0; i < 10; ++i) {
    Send(RtpPacketToSend::Type::kVideo, ssrc, sequence_number++,
         clock_.TimeInMilliseconds(), kPacketSize);
  }

  // First probing cluster.
  EXPECT_CALL(callback,
              SendRtpPacket(_, Field(&PacedPacketInfo::probe_cluster_id, 0)))
      .Times(5);

  for (int i = 0; i < 5; ++i) {
    clock_.AdvanceTimeMilliseconds(20);
    pacer_->ProcessPackets();
  }

  // Second probing cluster.
  EXPECT_CALL(callback,
              SendRtpPacket(_, Field(&PacedPacketInfo::probe_cluster_id, 1)))
      .Times(5);

  for (int i = 0; i < 5; ++i) {
    clock_.AdvanceTimeMilliseconds(20);
    pacer_->ProcessPackets();
  }

  // Needed for the Field comparer below.
  const int kNotAProbe = PacedPacketInfo::kNotAProbe;
  // No more probing packets.
  EXPECT_CALL(callback, GeneratePadding).WillOnce([&](DataSize padding_size) {
    std::vector<std::unique_ptr<RtpPacketToSend>> padding_packets;
    padding_packets.emplace_back(
        BuildPacket(RtpPacketToSend::Type::kPadding, ssrc, sequence_number++,
                    clock_.TimeInMilliseconds(), padding_size.bytes()));
    return padding_packets;
  });
  EXPECT_CALL(
      callback,
      SendRtpPacket(_, Field(&PacedPacketInfo::probe_cluster_id, kNotAProbe)))
      .Times(1);
  pacer_->ProcessPackets();
}

TEST_F(PacingControllerTest, OwnedPacketPrioritizedOnType) {
  MockPacketSender callback;
  pacer_ =
      absl::make_unique<PacingController>(&clock_, &callback, nullptr, nullptr);
  Init();

  // Insert a packet of each type, from low to high priority. Since priority
  // is weighted higher than insert order, these should come out of the pacer
  // in backwards order with the exception of FEC and Video.
  for (RtpPacketToSend::Type type :
       {RtpPacketToSend::Type::kPadding,
        RtpPacketToSend::Type::kForwardErrorCorrection,
        RtpPacketToSend::Type::kVideo, RtpPacketToSend::Type::kRetransmission,
        RtpPacketToSend::Type::kAudio}) {
    pacer_->EnqueuePacket(BuildRtpPacket(type));
  }

  ::testing::InSequence seq;
  EXPECT_CALL(
      callback,
      SendRtpPacket(Pointee(Property(&RtpPacketToSend::Ssrc, kAudioSsrc)), _));
  EXPECT_CALL(callback,
              SendRtpPacket(
                  Pointee(Property(&RtpPacketToSend::Ssrc, kVideoRtxSsrc)), _));

  // FEC and video actually have the same priority, so will come out in
  // insertion order.
  EXPECT_CALL(callback,
              SendRtpPacket(
                  Pointee(Property(&RtpPacketToSend::Ssrc, kFlexFecSsrc)), _));
  EXPECT_CALL(
      callback,
      SendRtpPacket(Pointee(Property(&RtpPacketToSend::Ssrc, kVideoSsrc)), _));

  EXPECT_CALL(callback,
              SendRtpPacket(
                  Pointee(Property(&RtpPacketToSend::Ssrc, kVideoRtxSsrc)), _));

  clock_.AdvanceTimeMilliseconds(200);
  pacer_->ProcessPackets();
}
}  // namespace test
}  // namespace webrtc
