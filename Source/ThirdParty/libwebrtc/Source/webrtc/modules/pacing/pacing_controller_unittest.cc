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
constexpr DataRate kFirstClusterRate = DataRate::KilobitsPerSec(900);
constexpr DataRate kSecondClusterRate = DataRate::KilobitsPerSec(1800);

// The error stems from truncating the time interval of probe packets to integer
// values. This results in probing slightly higher than the target bitrate.
// For 1.8 Mbps, this comes to be about 120 kbps with 1200 probe packets.
constexpr DataRate kProbingErrorMargin = DataRate::KilobitsPerSec(150);

const float kPaceMultiplier = 2.5f;

constexpr uint32_t kAudioSsrc = 12345;
constexpr uint32_t kVideoSsrc = 234565;
constexpr uint32_t kVideoRtxSsrc = 34567;
constexpr uint32_t kFlexFecSsrc = 45678;

constexpr DataRate kTargetRate = DataRate::KilobitsPerSec(800);

std::unique_ptr<RtpPacketToSend> BuildPacket(RtpPacketMediaType type,
                                             uint32_t ssrc,
                                             uint16_t sequence_number,
                                             int64_t capture_time_ms,
                                             size_t size) {
  auto packet = std::make_unique<RtpPacketToSend>(nullptr);
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
               packet->packet_type() == RtpPacketMediaType::kRetransmission,
               packet->packet_type() == RtpPacketMediaType::kPadding);
  }

  std::vector<std::unique_ptr<RtpPacketToSend>> GeneratePadding(
      DataSize target_size) override {
    std::vector<std::unique_ptr<RtpPacketToSend>> ret;
    size_t padding_size = SendPadding(target_size.bytes());
    if (padding_size > 0) {
      auto packet = std::make_unique<RtpPacketToSend>(nullptr);
      packet->SetPayloadSize(padding_size);
      packet->set_packet_type(RtpPacketMediaType::kPadding);
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

  PacingControllerPadding() : padding_sent_(0), total_bytes_sent_(0) {}

  void SendRtpPacket(std::unique_ptr<RtpPacketToSend> packet,
                     const PacedPacketInfo& pacing_info) override {
    total_bytes_sent_ += packet->payload_size();
  }

  std::vector<std::unique_ptr<RtpPacketToSend>> GeneratePadding(
      DataSize target_size) override {
    size_t num_packets =
        (target_size.bytes() + kPaddingPacketSize - 1) / kPaddingPacketSize;
    std::vector<std::unique_ptr<RtpPacketToSend>> packets;
    for (size_t i = 0; i < num_packets; ++i) {
      packets.emplace_back(std::make_unique<RtpPacketToSend>(nullptr));
      packets.back()->SetPadding(kPaddingPacketSize);
      packets.back()->set_packet_type(RtpPacketMediaType::kPadding);
      padding_sent_ += kPaddingPacketSize;
    }
    return packets;
  }

  size_t padding_sent() { return padding_sent_; }
  size_t total_bytes_sent() { return total_bytes_sent_; }

 private:
  size_t padding_sent_;
  size_t total_bytes_sent_;
};

class PacingControllerProbing : public PacingController::PacketSender {
 public:
  PacingControllerProbing() : packets_sent_(0), padding_sent_(0) {}

  void SendRtpPacket(std::unique_ptr<RtpPacketToSend> packet,
                     const PacedPacketInfo& pacing_info) override {
    if (packet->packet_type() != RtpPacketMediaType::kPadding) {
      ++packets_sent_;
    }
  }

  std::vector<std::unique_ptr<RtpPacketToSend>> GeneratePadding(
      DataSize target_size) override {
    // From RTPSender:
    // Max in the RFC 3550 is 255 bytes, we limit it to be modulus 32 for SRTP.
    const DataSize kMaxPadding = DataSize::Bytes(224);

    std::vector<std::unique_ptr<RtpPacketToSend>> packets;
    while (target_size > DataSize::Zero()) {
      DataSize padding_size = std::min(kMaxPadding, target_size);
      packets.emplace_back(std::make_unique<RtpPacketToSend>(nullptr));
      packets.back()->SetPadding(padding_size.bytes());
      packets.back()->set_packet_type(RtpPacketMediaType::kPadding);
      padding_sent_ += padding_size.bytes();
      target_size -= padding_size;
    }
    return packets;
  }

  int packets_sent() const { return packets_sent_; }

  int padding_sent() const { return padding_sent_; }

 private:
  int packets_sent_;
  int padding_sent_;
};

class PacingControllerTest
    : public ::testing::TestWithParam<PacingController::ProcessMode> {
 protected:
  PacingControllerTest() : clock_(123456) {
    srand(0);
    // Need to initialize PacingController after we initialize clock.
    pacer_ = std::make_unique<PacingController>(&clock_, &callback_, nullptr,
                                                nullptr, GetParam());
    Init();
  }

  bool PeriodicProcess() const {
    return GetParam() == PacingController::ProcessMode::kPeriodic;
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

  void Send(RtpPacketMediaType type,
            uint32_t ssrc,
            uint16_t sequence_number,
            int64_t capture_time_ms,
            size_t size) {
    pacer_->EnqueuePacket(
        BuildPacket(type, ssrc, sequence_number, capture_time_ms, size));
  }

  void SendAndExpectPacket(RtpPacketMediaType type,
                           uint32_t ssrc,
                           uint16_t sequence_number,
                           int64_t capture_time_ms,
                           size_t size) {
    Send(type, ssrc, sequence_number, capture_time_ms, size);
    EXPECT_CALL(callback_,
                SendPacket(ssrc, sequence_number, capture_time_ms,
                           type == RtpPacketMediaType::kRetransmission, false))
        .Times(1);
  }

  std::unique_ptr<RtpPacketToSend> BuildRtpPacket(RtpPacketMediaType type) {
    auto packet = std::make_unique<RtpPacketToSend>(nullptr);
    packet->set_packet_type(type);
    switch (type) {
      case RtpPacketMediaType::kAudio:
        packet->SetSsrc(kAudioSsrc);
        break;
      case RtpPacketMediaType::kVideo:
        packet->SetSsrc(kVideoSsrc);
        break;
      case RtpPacketMediaType::kRetransmission:
      case RtpPacketMediaType::kPadding:
        packet->SetSsrc(kVideoRtxSsrc);
        break;
      case RtpPacketMediaType::kForwardErrorCorrection:
        packet->SetSsrc(kFlexFecSsrc);
        break;
    }

    packet->SetPayloadSize(234);
    return packet;
  }

  TimeDelta TimeUntilNextProcess() {
    Timestamp now = clock_.CurrentTime();
    return std::max(pacer_->NextSendTime() - now, TimeDelta::Zero());
  }

  void AdvanceTimeAndProcess() {
    Timestamp now = clock_.CurrentTime();
    Timestamp next_send_time = pacer_->NextSendTime();
    clock_.AdvanceTime(std::max(TimeDelta::Zero(), next_send_time - now));
    pacer_->ProcessPackets();
  }

  void ConsumeInitialBudget() {
    const uint32_t kSsrc = 54321;
    uint16_t sequence_number = 1234;
    int64_t capture_time_ms = clock_.TimeInMilliseconds();
    const size_t kPacketSize = 250;

    EXPECT_EQ(TimeDelta::Zero(), pacer_->OldestPacketWaitTime());

    // Due to the multiplicative factor we can send 5 packets during a send
    // interval. (network capacity * multiplier / (8 bits per byte *
    // (packet size * #send intervals per second)
    const size_t packets_to_send_per_interval =
        kTargetRate.bps() * kPaceMultiplier / (8 * kPacketSize * 200);
    for (size_t i = 0; i < packets_to_send_per_interval; ++i) {
      SendAndExpectPacket(RtpPacketMediaType::kVideo, kSsrc, sequence_number++,
                          capture_time_ms, kPacketSize);
    }

    while (pacer_->QueueSizePackets() > 0) {
      if (PeriodicProcess()) {
        clock_.AdvanceTime(TimeUntilNextProcess());
        pacer_->ProcessPackets();
      } else {
        AdvanceTimeAndProcess();
      }
    }
  }

  SimulatedClock clock_;
  MockPacingControllerCallback callback_;
  std::unique_ptr<PacingController> pacer_;
};

class PacingControllerFieldTrialTest
    : public ::testing::TestWithParam<PacingController::ProcessMode> {
 protected:
  struct MediaStream {
    const RtpPacketMediaType type;
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
    if (GetParam() == PacingController::ProcessMode::kPeriodic) {
      TimeDelta process_interval = TimeDelta::Millis(5);
      clock_.AdvanceTime(process_interval);
      pacer->ProcessPackets();
      return;
    }

    Timestamp now = clock_.CurrentTime();
    Timestamp next_send_time = pacer->NextSendTime();
    TimeDelta wait_time = std::max(TimeDelta::Zero(), next_send_time - now);
    clock_.AdvanceTime(wait_time);
    pacer->ProcessPackets();
  }
  MediaStream audio{/*type*/ RtpPacketMediaType::kAudio,
                    /*ssrc*/ 3333, /*packet_size*/ 100, /*seq_num*/ 1000};
  MediaStream video{/*type*/ RtpPacketMediaType::kVideo,
                    /*ssrc*/ 4444, /*packet_size*/ 1000, /*seq_num*/ 1000};
  SimulatedClock clock_;
  MockPacingControllerCallback callback_;
};

TEST_P(PacingControllerFieldTrialTest, DefaultNoPaddingInSilence) {
  PacingController pacer(&clock_, &callback_, nullptr, nullptr, GetParam());
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

TEST_P(PacingControllerFieldTrialTest, PaddingInSilenceWithTrial) {
  ScopedFieldTrials trial("WebRTC-Pacer-PadInSilence/Enabled/");
  PacingController pacer(&clock_, &callback_, nullptr, nullptr, GetParam());
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

TEST_P(PacingControllerFieldTrialTest, CongestionWindowAffectsAudioInTrial) {
  ScopedFieldTrials trial("WebRTC-Pacer-BlockAudio/Enabled/");
  EXPECT_CALL(callback_, SendPadding).Times(0);
  PacingController pacer(&clock_, &callback_, nullptr, nullptr, GetParam());
  pacer.SetPacingRates(DataRate::KilobitsPerSec(10000), DataRate::Zero());
  pacer.SetCongestionWindow(DataSize::Bytes(video.packet_size - 100));
  pacer.UpdateOutstandingData(DataSize::Zero());
  // Video packet fills congestion window.
  InsertPacket(&pacer, &video);
  EXPECT_CALL(callback_, SendPacket).Times(1);
  ProcessNext(&pacer);
  // Audio packet blocked due to congestion.
  InsertPacket(&pacer, &audio);
  EXPECT_CALL(callback_, SendPacket).Times(0);
  if (GetParam() == PacingController::ProcessMode::kDynamic) {
    // Without interval budget we'll forward time to where we send keep-alive.
    EXPECT_CALL(callback_, SendPadding(1)).Times(2);
  }
  ProcessNext(&pacer);
  ProcessNext(&pacer);
  // Audio packet unblocked when congestion window clear.
  ::testing::Mock::VerifyAndClearExpectations(&callback_);
  pacer.UpdateOutstandingData(DataSize::Zero());
  EXPECT_CALL(callback_, SendPacket).Times(1);
  ProcessNext(&pacer);
}

TEST_P(PacingControllerFieldTrialTest,
       DefaultCongestionWindowDoesNotAffectAudio) {
  EXPECT_CALL(callback_, SendPadding).Times(0);
  PacingController pacer(&clock_, &callback_, nullptr, nullptr, GetParam());
  pacer.SetPacingRates(DataRate::BitsPerSec(10000000), DataRate::Zero());
  pacer.SetCongestionWindow(DataSize::Bytes(800));
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

TEST_P(PacingControllerFieldTrialTest, BudgetAffectsAudioInTrial) {
  ScopedFieldTrials trial("WebRTC-Pacer-BlockAudio/Enabled/");
  PacingController pacer(&clock_, &callback_, nullptr, nullptr, GetParam());
  DataRate pacing_rate = DataRate::BitsPerSec(video.packet_size / 3 * 8 *
                                              kProcessIntervalsPerSecond);
  pacer.SetPacingRates(pacing_rate, DataRate::Zero());
  // Video fills budget for following process periods.
  InsertPacket(&pacer, &video);
  EXPECT_CALL(callback_, SendPacket).Times(1);
  ProcessNext(&pacer);
  // Audio packet blocked due to budget limit.
  InsertPacket(&pacer, &audio);
  Timestamp wait_start_time = clock_.CurrentTime();
  Timestamp wait_end_time = Timestamp::MinusInfinity();
  EXPECT_CALL(callback_, SendPacket)
      .WillOnce([&](uint32_t ssrc, uint16_t sequence_number,
                    int64_t capture_timestamp, bool retransmission,
                    bool padding) { wait_end_time = clock_.CurrentTime(); });
  while (!wait_end_time.IsFinite()) {
    ProcessNext(&pacer);
  }
  const TimeDelta expected_wait_time =
      DataSize::Bytes(video.packet_size) / pacing_rate;
  // Verify delay is near expectation, within timing margin.
  EXPECT_LT(((wait_end_time - wait_start_time) - expected_wait_time).Abs(),
            GetParam() == PacingController::ProcessMode::kPeriodic
                ? TimeDelta::Millis(5)
                : PacingController::kMinSleepTime);
}

TEST_P(PacingControllerFieldTrialTest, DefaultBudgetDoesNotAffectAudio) {
  EXPECT_CALL(callback_, SendPadding).Times(0);
  PacingController pacer(&clock_, &callback_, nullptr, nullptr, GetParam());
  pacer.SetPacingRates(DataRate::BitsPerSec(video.packet_size / 3 * 8 *
                                            kProcessIntervalsPerSecond),
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

INSTANTIATE_TEST_SUITE_P(WithAndWithoutIntervalBudget,
                         PacingControllerFieldTrialTest,
                         ::testing::Values(false, true));

TEST_P(PacingControllerTest, FirstSentPacketTimeIsSet) {
  uint16_t sequence_number = 1234;
  const uint32_t kSsrc = 12345;
  const size_t kSizeBytes = 250;
  const size_t kPacketToSend = 3;
  const Timestamp kStartTime = clock_.CurrentTime();

  // No packet sent.
  EXPECT_FALSE(pacer_->FirstSentPacketTime().has_value());

  for (size_t i = 0; i < kPacketToSend; ++i) {
    SendAndExpectPacket(RtpPacketMediaType::kVideo, kSsrc, sequence_number++,
                        clock_.TimeInMilliseconds(), kSizeBytes);
    clock_.AdvanceTime(TimeUntilNextProcess());
    pacer_->ProcessPackets();
  }
  EXPECT_EQ(kStartTime, pacer_->FirstSentPacketTime());
}

TEST_P(PacingControllerTest, QueuePacket) {
  if (!PeriodicProcess()) {
    // This test checks behavior applicable only when using interval budget.
    return;
  }

  uint32_t ssrc = 12345;
  uint16_t sequence_number = 1234;
  // Due to the multiplicative factor we can send 5 packets during a 5ms send
  // interval. (network capacity * multiplier / (8 bits per byte *
  // (packet size * #send intervals per second)
  const size_t kPacketsToSend =
      kTargetRate.bps() * kPaceMultiplier / (8 * 250 * 200);
  for (size_t i = 0; i < kPacketsToSend; ++i) {
    SendAndExpectPacket(RtpPacketMediaType::kVideo, ssrc, sequence_number++,
                        clock_.TimeInMilliseconds(), 250);
  }
  EXPECT_CALL(callback_, SendPadding).Times(0);

  // Enqueue one extra packet.
  int64_t queued_packet_timestamp = clock_.TimeInMilliseconds();
  Send(RtpPacketMediaType::kVideo, ssrc, sequence_number,
       queued_packet_timestamp, 250);
  EXPECT_EQ(kPacketsToSend + 1, pacer_->QueueSizePackets());

  // The first kPacketsToSend packets will be sent with budget from the
  // initial 5ms interval.
  pacer_->ProcessPackets();
  EXPECT_EQ(1u, pacer_->QueueSizePackets());

  // Advance time to next interval, make sure the last packet is sent.
  clock_.AdvanceTimeMilliseconds(5);
  EXPECT_CALL(callback_, SendPacket(ssrc, sequence_number++,
                                    queued_packet_timestamp, false, false))
      .Times(1);
  pacer_->ProcessPackets();
  sequence_number++;
  EXPECT_EQ(0u, pacer_->QueueSizePackets());

  // We can send packets_to_send -1 packets of size 250 during the current
  // interval since one packet has already been sent.
  for (size_t i = 0; i < kPacketsToSend - 1; ++i) {
    SendAndExpectPacket(RtpPacketMediaType::kVideo, ssrc, sequence_number++,
                        clock_.TimeInMilliseconds(), 250);
  }
  Send(RtpPacketMediaType::kVideo, ssrc, sequence_number++,
       clock_.TimeInMilliseconds(), 250);
  EXPECT_EQ(kPacketsToSend, pacer_->QueueSizePackets());
  pacer_->ProcessPackets();
  EXPECT_EQ(1u, pacer_->QueueSizePackets());
}

TEST_P(PacingControllerTest, QueueAndPacePackets) {
  if (PeriodicProcess()) {
    // This test checks behavior when not using interval budget.
    return;
  }

  const uint32_t kSsrc = 12345;
  uint16_t sequence_number = 1234;
  const DataSize kPackeSize = DataSize::Bytes(250);
  const TimeDelta kSendInterval = TimeDelta::Millis(5);

  // Due to the multiplicative factor we can send 5 packets during a 5ms send
  // interval. (send interval * network capacity * multiplier / packet size)
  const size_t kPacketsToSend = (kSendInterval * kTargetRate).bytes() *
                                kPaceMultiplier / kPackeSize.bytes();

  for (size_t i = 0; i < kPacketsToSend; ++i) {
    SendAndExpectPacket(RtpPacketMediaType::kVideo, kSsrc, sequence_number++,
                        clock_.TimeInMilliseconds(), kPackeSize.bytes());
  }
  EXPECT_CALL(callback_, SendPadding).Times(0);

  // Enqueue one extra packet.
  int64_t queued_packet_timestamp = clock_.TimeInMilliseconds();
  Send(RtpPacketMediaType::kVideo, kSsrc, sequence_number,
       queued_packet_timestamp, kPackeSize.bytes());
  EXPECT_EQ(kPacketsToSend + 1, pacer_->QueueSizePackets());

  // Send packets until the initial kPacketsToSend packets are done.
  Timestamp start_time = clock_.CurrentTime();
  while (pacer_->QueueSizePackets() > 1) {
    AdvanceTimeAndProcess();
  }
  EXPECT_LT(clock_.CurrentTime() - start_time, kSendInterval);

  // Proceed till last packet can be sent.
  EXPECT_CALL(callback_, SendPacket(kSsrc, sequence_number,
                                    queued_packet_timestamp, false, false))
      .Times(1);
  AdvanceTimeAndProcess();
  EXPECT_GE(clock_.CurrentTime() - start_time, kSendInterval);
  EXPECT_EQ(pacer_->QueueSizePackets(), 0u);
}

TEST_P(PacingControllerTest, PaceQueuedPackets) {
  uint32_t ssrc = 12345;
  uint16_t sequence_number = 1234;
  const size_t kPacketSize = 250;

  // Due to the multiplicative factor we can send 5 packets during a send
  // interval. (network capacity * multiplier / (8 bits per byte *
  // (packet size * #send intervals per second)
  const size_t packets_to_send_per_interval =
      kTargetRate.bps() * kPaceMultiplier / (8 * kPacketSize * 200);
  for (size_t i = 0; i < packets_to_send_per_interval; ++i) {
    SendAndExpectPacket(RtpPacketMediaType::kVideo, ssrc, sequence_number++,
                        clock_.TimeInMilliseconds(), kPacketSize);
  }

  for (size_t j = 0; j < packets_to_send_per_interval * 10; ++j) {
    Send(RtpPacketMediaType::kVideo, ssrc, sequence_number++,
         clock_.TimeInMilliseconds(), kPacketSize);
  }
  EXPECT_EQ(packets_to_send_per_interval + packets_to_send_per_interval * 10,
            pacer_->QueueSizePackets());
  if (PeriodicProcess()) {
    pacer_->ProcessPackets();
  } else {
    while (pacer_->QueueSizePackets() > packets_to_send_per_interval * 10) {
      AdvanceTimeAndProcess();
    }
  }
  EXPECT_EQ(pacer_->QueueSizePackets(), packets_to_send_per_interval * 10);
  EXPECT_CALL(callback_, SendPadding).Times(0);

  EXPECT_CALL(callback_, SendPacket(ssrc, _, _, false, false))
      .Times(pacer_->QueueSizePackets());
  const TimeDelta expected_pace_time =
      DataSize::Bytes(pacer_->QueueSizePackets() * kPacketSize) /
      (kPaceMultiplier * kTargetRate);
  Timestamp start_time = clock_.CurrentTime();
  while (pacer_->QueueSizePackets() > 0) {
    if (PeriodicProcess()) {
      clock_.AdvanceTime(TimeUntilNextProcess());
      pacer_->ProcessPackets();
    } else {
      AdvanceTimeAndProcess();
    }
  }
  const TimeDelta actual_pace_time = clock_.CurrentTime() - start_time;
  EXPECT_LT((actual_pace_time - expected_pace_time).Abs(),
            PeriodicProcess() ? TimeDelta::Millis(5)
                              : PacingController::kMinSleepTime);

  EXPECT_EQ(0u, pacer_->QueueSizePackets());
  clock_.AdvanceTime(TimeUntilNextProcess());
  EXPECT_EQ(0u, pacer_->QueueSizePackets());
  pacer_->ProcessPackets();

  // Send some more packet, just show that we can..?
  for (size_t i = 0; i < packets_to_send_per_interval; ++i) {
    SendAndExpectPacket(RtpPacketMediaType::kVideo, ssrc, sequence_number++,
                        clock_.TimeInMilliseconds(), 250);
  }
  EXPECT_EQ(packets_to_send_per_interval, pacer_->QueueSizePackets());
  if (PeriodicProcess()) {
    pacer_->ProcessPackets();
  } else {
    for (size_t i = 0; i < packets_to_send_per_interval; ++i) {
      AdvanceTimeAndProcess();
    }
  }
  EXPECT_EQ(0u, pacer_->QueueSizePackets());
}

TEST_P(PacingControllerTest, RepeatedRetransmissionsAllowed) {
  // Send one packet, then two retransmissions of that packet.
  for (size_t i = 0; i < 3; i++) {
    constexpr uint32_t ssrc = 333;
    constexpr uint16_t sequence_number = 444;
    constexpr size_t bytes = 250;
    bool is_retransmission = (i != 0);  // Original followed by retransmissions.
    SendAndExpectPacket(is_retransmission ? RtpPacketMediaType::kRetransmission
                                          : RtpPacketMediaType::kVideo,
                        ssrc, sequence_number, clock_.TimeInMilliseconds(),
                        bytes);
    clock_.AdvanceTimeMilliseconds(5);
  }
  if (PeriodicProcess()) {
    pacer_->ProcessPackets();
  } else {
    while (pacer_->QueueSizePackets() > 0) {
      AdvanceTimeAndProcess();
    }
  }
}

TEST_P(PacingControllerTest,
       CanQueuePacketsWithSameSequenceNumberOnDifferentSsrcs) {
  uint32_t ssrc = 12345;
  uint16_t sequence_number = 1234;

  SendAndExpectPacket(RtpPacketMediaType::kVideo, ssrc, sequence_number,
                      clock_.TimeInMilliseconds(), 250);

  // Expect packet on second ssrc to be queued and sent as well.
  SendAndExpectPacket(RtpPacketMediaType::kVideo, ssrc + 1, sequence_number,
                      clock_.TimeInMilliseconds(), 250);

  clock_.AdvanceTimeMilliseconds(1000);
  if (PeriodicProcess()) {
    pacer_->ProcessPackets();
  } else {
    while (pacer_->QueueSizePackets() > 0) {
      AdvanceTimeAndProcess();
    }
  }
}

TEST_P(PacingControllerTest, Padding) {
  uint32_t ssrc = 12345;
  uint16_t sequence_number = 1234;
  const size_t kPacketSize = 250;

  pacer_->SetPacingRates(kTargetRate * kPaceMultiplier, kTargetRate);

  if (PeriodicProcess()) {
    ConsumeInitialBudget();

    // 5 milliseconds later should not send padding since we filled the buffers
    // initially.
    EXPECT_CALL(callback_, SendPadding(kPacketSize)).Times(0);
    clock_.AdvanceTime(TimeUntilNextProcess());
    pacer_->ProcessPackets();

    // 5 milliseconds later we have enough budget to send some padding.
    EXPECT_CALL(callback_, SendPadding(250)).WillOnce(Return(kPacketSize));
    EXPECT_CALL(callback_, SendPacket(_, _, _, _, true)).Times(1);
    clock_.AdvanceTime(TimeUntilNextProcess());
    pacer_->ProcessPackets();
  } else {
    const size_t kPacketsToSend = 20;
    for (size_t i = 0; i < kPacketsToSend; ++i) {
      SendAndExpectPacket(RtpPacketMediaType::kVideo, ssrc, sequence_number++,
                          clock_.TimeInMilliseconds(), kPacketSize);
    }
    const TimeDelta expected_pace_time =
        DataSize::Bytes(pacer_->QueueSizePackets() * kPacketSize) /
        (kPaceMultiplier * kTargetRate);
    EXPECT_CALL(callback_, SendPadding).Times(0);
    // Only the media packets should be sent.
    Timestamp start_time = clock_.CurrentTime();
    while (pacer_->QueueSizePackets() > 0) {
      AdvanceTimeAndProcess();
    }
    const TimeDelta actual_pace_time = clock_.CurrentTime() - start_time;
    EXPECT_LE((actual_pace_time - expected_pace_time).Abs(),
              PacingController::kMinSleepTime);

    // Pacing media happens at 2.5x, but padding was configured with 1.0x
    // factor. We have to wait until the padding debt is gone before we start
    // sending padding.
    const TimeDelta time_to_padding_debt_free =
        (expected_pace_time * kPaceMultiplier) - actual_pace_time;
    clock_.AdvanceTime(time_to_padding_debt_free -
                       PacingController::kMinSleepTime);
    pacer_->ProcessPackets();

    // Send 10 padding packets.
    const size_t kPaddingPacketsToSend = 10;
    DataSize padding_sent = DataSize::Zero();
    size_t packets_sent = 0;
    Timestamp first_send_time = Timestamp::MinusInfinity();
    Timestamp last_send_time = Timestamp::MinusInfinity();

    EXPECT_CALL(callback_, SendPadding)
        .Times(kPaddingPacketsToSend)
        .WillRepeatedly([&](size_t target_size) {
          ++packets_sent;
          if (packets_sent < kPaddingPacketsToSend) {
            // Don't count bytes of last packet, instead just
            // use this as the time the last packet finished
            // sending.
            padding_sent += DataSize::Bytes(target_size);
          }
          if (first_send_time.IsInfinite()) {
            first_send_time = clock_.CurrentTime();
          } else {
            last_send_time = clock_.CurrentTime();
          }
          return target_size;
        });
    EXPECT_CALL(callback_, SendPacket(_, _, _, false, true))
        .Times(kPaddingPacketsToSend);

    while (packets_sent < kPaddingPacketsToSend) {
      AdvanceTimeAndProcess();
    }

    // Verify rate of sent padding.
    TimeDelta padding_duration = last_send_time - first_send_time;
    DataRate padding_rate = padding_sent / padding_duration;
    EXPECT_EQ(padding_rate, kTargetRate);
  }
}

TEST_P(PacingControllerTest, NoPaddingBeforeNormalPacket) {
  pacer_->SetPacingRates(kTargetRate * kPaceMultiplier, kTargetRate);

  EXPECT_CALL(callback_, SendPadding).Times(0);

  pacer_->ProcessPackets();
  clock_.AdvanceTime(TimeUntilNextProcess());

  pacer_->ProcessPackets();
  clock_.AdvanceTime(TimeUntilNextProcess());

  uint32_t ssrc = 12345;
  uint16_t sequence_number = 1234;
  int64_t capture_time_ms = 56789;

  SendAndExpectPacket(RtpPacketMediaType::kVideo, ssrc, sequence_number++,
                      capture_time_ms, 250);
  bool padding_sent = false;
  EXPECT_CALL(callback_, SendPadding).WillOnce([&](size_t padding) {
    padding_sent = true;
    return padding;
  });
  EXPECT_CALL(callback_, SendPacket(_, _, _, _, true)).Times(1);
  if (PeriodicProcess()) {
    pacer_->ProcessPackets();
  } else {
    while (!padding_sent) {
      AdvanceTimeAndProcess();
    }
  }
}

TEST_P(PacingControllerTest, VerifyPaddingUpToBitrate) {
  if (!PeriodicProcess()) {
    // Already tested in PacingControllerTest.Padding.
    return;
  }

  uint32_t ssrc = 12345;
  uint16_t sequence_number = 1234;
  int64_t capture_time_ms = 56789;
  const int kTimeStep = 5;
  const int64_t kBitrateWindow = 100;
  pacer_->SetPacingRates(kTargetRate * kPaceMultiplier, kTargetRate);

  int64_t start_time = clock_.TimeInMilliseconds();
  while (clock_.TimeInMilliseconds() - start_time < kBitrateWindow) {
    SendAndExpectPacket(RtpPacketMediaType::kVideo, ssrc, sequence_number++,
                        capture_time_ms, 250);
    EXPECT_CALL(callback_, SendPadding(250)).WillOnce(Return(250));
    EXPECT_CALL(callback_, SendPacket(_, _, _, _, true)).Times(1);
    pacer_->ProcessPackets();
    clock_.AdvanceTimeMilliseconds(kTimeStep);
  }
}

TEST_P(PacingControllerTest, VerifyAverageBitrateVaryingMediaPayload) {
  uint32_t ssrc = 12345;
  uint16_t sequence_number = 1234;
  int64_t capture_time_ms = 56789;
  const int kTimeStep = 5;
  const TimeDelta kAveragingWindowLength = TimeDelta::Seconds(10);
  PacingControllerPadding callback;
  pacer_ = std::make_unique<PacingController>(&clock_, &callback, nullptr,
                                              nullptr, GetParam());
  pacer_->SetProbingEnabled(false);
  pacer_->SetPacingRates(kTargetRate * kPaceMultiplier, kTargetRate);

  Timestamp start_time = clock_.CurrentTime();
  size_t media_bytes = 0;
  while (clock_.CurrentTime() - start_time < kAveragingWindowLength) {
    // Maybe add some new media packets corresponding to expected send rate.
    int rand_value = rand();  // NOLINT (rand_r instead of rand)
    while (
        media_bytes <
        (kTargetRate * (clock_.CurrentTime() - start_time)).bytes<size_t>()) {
      size_t media_payload = rand_value % 400 + 800;  // [400, 1200] bytes.
      Send(RtpPacketMediaType::kVideo, ssrc, sequence_number++, capture_time_ms,
           media_payload);
      media_bytes += media_payload;
    }

    if (PeriodicProcess()) {
      clock_.AdvanceTimeMilliseconds(kTimeStep);
      pacer_->ProcessPackets();
    } else {
      AdvanceTimeAndProcess();
    }
  }

  EXPECT_NEAR(
      kTargetRate.bps(),
      (DataSize::Bytes(callback.total_bytes_sent()) / kAveragingWindowLength)
          .bps(),
      (kTargetRate * 0.01 /* 1% error marging */).bps());
}

TEST_P(PacingControllerTest, Priority) {
  uint32_t ssrc_low_priority = 12345;
  uint32_t ssrc = 12346;
  uint16_t sequence_number = 1234;
  int64_t capture_time_ms = 56789;
  int64_t capture_time_ms_low_priority = 1234567;

  ConsumeInitialBudget();

  // Expect normal and low priority to be queued and high to pass through.
  Send(RtpPacketMediaType::kVideo, ssrc_low_priority, sequence_number++,
       capture_time_ms_low_priority, 250);

  const size_t packets_to_send_per_interval =
      kTargetRate.bps() * kPaceMultiplier / (8 * 250 * 200);
  for (size_t i = 0; i < packets_to_send_per_interval; ++i) {
    Send(RtpPacketMediaType::kRetransmission, ssrc, sequence_number++,
         capture_time_ms, 250);
  }
  Send(RtpPacketMediaType::kAudio, ssrc, sequence_number++, capture_time_ms,
       250);

  // Expect all high and normal priority to be sent out first.
  EXPECT_CALL(callback_, SendPadding).Times(0);
  EXPECT_CALL(callback_, SendPacket(ssrc, _, capture_time_ms, _, _))
      .Times(packets_to_send_per_interval + 1);

  if (PeriodicProcess()) {
    clock_.AdvanceTime(TimeUntilNextProcess());
    pacer_->ProcessPackets();
  } else {
    while (pacer_->QueueSizePackets() > 1) {
      AdvanceTimeAndProcess();
    }
  }

  EXPECT_EQ(1u, pacer_->QueueSizePackets());

  EXPECT_CALL(callback_, SendPacket(ssrc_low_priority, _,
                                    capture_time_ms_low_priority, _, _))
      .Times(1);
  if (PeriodicProcess()) {
    clock_.AdvanceTime(TimeUntilNextProcess());
    pacer_->ProcessPackets();
  } else {
    AdvanceTimeAndProcess();
  }
}

TEST_P(PacingControllerTest, RetransmissionPriority) {
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
    Send(RtpPacketMediaType::kVideo, ssrc, sequence_number++, capture_time_ms,
         250);
    Send(RtpPacketMediaType::kRetransmission, ssrc, sequence_number++,
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

  if (PeriodicProcess()) {
    clock_.AdvanceTime(TimeUntilNextProcess());
    pacer_->ProcessPackets();
  } else {
    while (pacer_->QueueSizePackets() > packets_to_send_per_interval) {
      AdvanceTimeAndProcess();
    }
  }
  EXPECT_EQ(packets_to_send_per_interval, pacer_->QueueSizePackets());

  // Expect the remaining (non-retransmission) packets to be sent.
  EXPECT_CALL(callback_, SendPadding).Times(0);
  EXPECT_CALL(callback_, SendPacket(_, _, _, true, _)).Times(0);
  EXPECT_CALL(callback_, SendPacket(ssrc, _, capture_time_ms, false, _))
      .Times(packets_to_send_per_interval);

  if (PeriodicProcess()) {
    clock_.AdvanceTime(TimeUntilNextProcess());
    pacer_->ProcessPackets();
  } else {
    while (pacer_->QueueSizePackets() > 0) {
      AdvanceTimeAndProcess();
    }
  }

  EXPECT_EQ(0u, pacer_->QueueSizePackets());
}

TEST_P(PacingControllerTest, HighPrioDoesntAffectBudget) {
  const size_t kPacketSize = 250;
  uint32_t ssrc = 12346;
  uint16_t sequence_number = 1234;
  int64_t capture_time_ms = 56789;

  // As high prio packets doesn't affect the budget, we should be able to send
  // a high number of them at once.
  const size_t kNumAudioPackets = 25;
  for (size_t i = 0; i < kNumAudioPackets; ++i) {
    SendAndExpectPacket(RtpPacketMediaType::kAudio, ssrc, sequence_number++,
                        capture_time_ms, kPacketSize);
  }
  pacer_->ProcessPackets();
  // Low prio packets does affect the budget.
  // Due to the multiplicative factor we can send 5 packets during a send
  // interval. (network capacity * multiplier / (8 bits per byte *
  // (packet size * #send intervals per second)
  const size_t kPacketsToSendPerInterval =
      kTargetRate.bps() * kPaceMultiplier / (8 * kPacketSize * 200);
  for (size_t i = 0; i < kPacketsToSendPerInterval; ++i) {
    SendAndExpectPacket(RtpPacketMediaType::kVideo, ssrc, sequence_number++,
                        clock_.TimeInMilliseconds(), kPacketSize);
  }

  // Send all packets and measure pace time.
  Timestamp start_time = clock_.CurrentTime();
  while (pacer_->QueueSizePackets() > 0) {
    if (PeriodicProcess()) {
      clock_.AdvanceTime(TimeUntilNextProcess());
      pacer_->ProcessPackets();
    } else {
      AdvanceTimeAndProcess();
    }
  }

  // Measure pacing time. Expect only low-prio packets to affect this.
  TimeDelta pacing_time = clock_.CurrentTime() - start_time;
  TimeDelta expected_pacing_time =
      DataSize::Bytes(kPacketsToSendPerInterval * kPacketSize) /
      (kTargetRate * kPaceMultiplier);
  EXPECT_NEAR(pacing_time.us<double>(), expected_pacing_time.us<double>(),
              PeriodicProcess() ? 5000.0
                                : PacingController::kMinSleepTime.us<double>());
}

TEST_P(PacingControllerTest, SendsOnlyPaddingWhenCongested) {
  uint32_t ssrc = 202020;
  uint16_t sequence_number = 1000;
  int kPacketSize = 250;
  int kCongestionWindow = kPacketSize * 10;

  pacer_->UpdateOutstandingData(DataSize::Zero());
  pacer_->SetCongestionWindow(DataSize::Bytes(kCongestionWindow));
  int sent_data = 0;
  while (sent_data < kCongestionWindow) {
    sent_data += kPacketSize;
    SendAndExpectPacket(RtpPacketMediaType::kVideo, ssrc, sequence_number++,
                        clock_.TimeInMilliseconds(), kPacketSize);
    AdvanceTimeAndProcess();
  }
  ::testing::Mock::VerifyAndClearExpectations(&callback_);
  EXPECT_CALL(callback_, SendPacket).Times(0);
  EXPECT_CALL(callback_, SendPadding).Times(0);

  size_t blocked_packets = 0;
  int64_t expected_time_until_padding = 500;
  while (expected_time_until_padding > 5) {
    Send(RtpPacketMediaType::kVideo, ssrc, sequence_number++,
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

TEST_P(PacingControllerTest, DoesNotAllowOveruseAfterCongestion) {
  uint32_t ssrc = 202020;
  uint16_t seq_num = 1000;
  int size = 1000;
  auto now_ms = [this] { return clock_.TimeInMilliseconds(); };
  EXPECT_CALL(callback_, SendPadding).Times(0);
  // The pacing rate is low enough that the budget should not allow two packets
  // to be sent in a row.
  pacer_->SetPacingRates(DataRate::BitsPerSec(400 * 8 * 1000 / 5),
                         DataRate::Zero());
  // The congestion window is small enough to only let one packet through.
  pacer_->SetCongestionWindow(DataSize::Bytes(800));
  pacer_->UpdateOutstandingData(DataSize::Zero());
  // Not yet budget limited or congested, packet is sent.
  Send(RtpPacketMediaType::kVideo, ssrc, seq_num++, now_ms(), size);
  EXPECT_CALL(callback_, SendPacket).Times(1);
  clock_.AdvanceTimeMilliseconds(5);
  pacer_->ProcessPackets();
  // Packet blocked due to congestion.
  Send(RtpPacketMediaType::kVideo, ssrc, seq_num++, now_ms(), size);
  EXPECT_CALL(callback_, SendPacket).Times(0);
  clock_.AdvanceTimeMilliseconds(5);
  pacer_->ProcessPackets();
  // Packet blocked due to congestion.
  Send(RtpPacketMediaType::kVideo, ssrc, seq_num++, now_ms(), size);
  EXPECT_CALL(callback_, SendPacket).Times(0);
  clock_.AdvanceTimeMilliseconds(5);
  pacer_->ProcessPackets();
  // Congestion removed and budget has recovered, packet is sent.
  Send(RtpPacketMediaType::kVideo, ssrc, seq_num++, now_ms(), size);
  EXPECT_CALL(callback_, SendPacket).Times(1);
  clock_.AdvanceTimeMilliseconds(5);
  pacer_->UpdateOutstandingData(DataSize::Zero());
  pacer_->ProcessPackets();
  // Should be blocked due to budget limitation as congestion has be removed.
  Send(RtpPacketMediaType::kVideo, ssrc, seq_num++, now_ms(), size);
  EXPECT_CALL(callback_, SendPacket).Times(0);
  clock_.AdvanceTimeMilliseconds(5);
  pacer_->ProcessPackets();
}

TEST_P(PacingControllerTest, ResumesSendingWhenCongestionEnds) {
  uint32_t ssrc = 202020;
  uint16_t sequence_number = 1000;
  int64_t kPacketSize = 250;
  int64_t kCongestionCount = 10;
  int64_t kCongestionWindow = kPacketSize * kCongestionCount;
  int64_t kCongestionTimeMs = 1000;

  pacer_->UpdateOutstandingData(DataSize::Zero());
  pacer_->SetCongestionWindow(DataSize::Bytes(kCongestionWindow));
  int sent_data = 0;
  while (sent_data < kCongestionWindow) {
    sent_data += kPacketSize;
    SendAndExpectPacket(RtpPacketMediaType::kVideo, ssrc, sequence_number++,
                        clock_.TimeInMilliseconds(), kPacketSize);
    clock_.AdvanceTimeMilliseconds(5);
    pacer_->ProcessPackets();
  }
  ::testing::Mock::VerifyAndClearExpectations(&callback_);
  EXPECT_CALL(callback_, SendPacket).Times(0);
  int unacked_packets = 0;
  for (int duration = 0; duration < kCongestionTimeMs; duration += 5) {
    Send(RtpPacketMediaType::kVideo, ssrc, sequence_number++,
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
      DataSize::Bytes(kCongestionWindow - kPacketSize * ack_count));

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

TEST_P(PacingControllerTest, Pause) {
  uint32_t ssrc_low_priority = 12345;
  uint32_t ssrc = 12346;
  uint32_t ssrc_high_priority = 12347;
  uint16_t sequence_number = 1234;

  EXPECT_EQ(TimeDelta::Zero(), pacer_->OldestPacketWaitTime());

  ConsumeInitialBudget();

  pacer_->Pause();

  int64_t capture_time_ms = clock_.TimeInMilliseconds();
  const size_t packets_to_send_per_interval =
      kTargetRate.bps() * kPaceMultiplier / (8 * 250 * 200);
  for (size_t i = 0; i < packets_to_send_per_interval; ++i) {
    Send(RtpPacketMediaType::kVideo, ssrc_low_priority, sequence_number++,
         capture_time_ms, 250);
    Send(RtpPacketMediaType::kRetransmission, ssrc, sequence_number++,
         capture_time_ms, 250);
    Send(RtpPacketMediaType::kAudio, ssrc_high_priority, sequence_number++,
         capture_time_ms, 250);
  }
  clock_.AdvanceTimeMilliseconds(10000);
  int64_t second_capture_time_ms = clock_.TimeInMilliseconds();
  for (size_t i = 0; i < packets_to_send_per_interval; ++i) {
    Send(RtpPacketMediaType::kVideo, ssrc_low_priority, sequence_number++,
         second_capture_time_ms, 250);
    Send(RtpPacketMediaType::kRetransmission, ssrc, sequence_number++,
         second_capture_time_ms, 250);
    Send(RtpPacketMediaType::kAudio, ssrc_high_priority, sequence_number++,
         second_capture_time_ms, 250);
  }

  // Expect everything to be queued.
  EXPECT_EQ(TimeDelta::Millis(second_capture_time_ms - capture_time_ms),
            pacer_->OldestPacketWaitTime());

  // Process triggers keep-alive packet.
  EXPECT_CALL(callback_, SendPadding).WillOnce([](size_t padding) {
    return padding;
  });
  EXPECT_CALL(callback_, SendPacket(_, _, _, _, true)).Times(1);
  pacer_->ProcessPackets();

  // Verify no packets sent for the rest of the paused process interval.
  const TimeDelta kProcessInterval = TimeDelta::Millis(5);
  TimeDelta expected_time_until_send = PacingController::kPausedProcessInterval;
  EXPECT_CALL(callback_, SendPadding).Times(0);
  while (expected_time_until_send >= kProcessInterval) {
    pacer_->ProcessPackets();
    clock_.AdvanceTime(kProcessInterval);
    expected_time_until_send -= kProcessInterval;
  }

  // New keep-alive packet.
  ::testing::Mock::VerifyAndClearExpectations(&callback_);
  EXPECT_CALL(callback_, SendPadding).WillOnce([](size_t padding) {
    return padding;
  });
  EXPECT_CALL(callback_, SendPacket(_, _, _, _, true)).Times(1);
  clock_.AdvanceTime(kProcessInterval);
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

  if (PeriodicProcess()) {
    // The pacer was resumed directly after the previous process call finished.
    // It will therefore wait 5 ms until next process.
    clock_.AdvanceTime(TimeUntilNextProcess());

    for (size_t i = 0; i < 4; i++) {
      pacer_->ProcessPackets();
      clock_.AdvanceTime(TimeUntilNextProcess());
    }
  } else {
    while (pacer_->QueueSizePackets() > 0) {
      AdvanceTimeAndProcess();
    }
  }

  EXPECT_EQ(TimeDelta::Zero(), pacer_->OldestPacketWaitTime());
}

TEST_P(PacingControllerTest, InactiveFromStart) {
  // Recreate the pacer without the inital time forwarding.
  pacer_ = std::make_unique<PacingController>(&clock_, &callback_, nullptr,
                                              nullptr, GetParam());
  pacer_->SetProbingEnabled(false);
  pacer_->SetPacingRates(kTargetRate * kPaceMultiplier, kTargetRate);

  if (PeriodicProcess()) {
    // In period mode, pause the pacer to check the same idle behavior as
    // dynamic.
    pacer_->Pause();
  }

  // No packets sent, there should be no keep-alives sent either.
  EXPECT_CALL(callback_, SendPadding).Times(0);
  EXPECT_CALL(callback_, SendPacket).Times(0);
  pacer_->ProcessPackets();

  const Timestamp start_time = clock_.CurrentTime();

  // Determine the margin need so we can advance to the last possible moment
  // that will not cause a process event.
  const TimeDelta time_margin =
      (GetParam() == PacingController::ProcessMode::kDynamic
           ? PacingController::kMinSleepTime
           : TimeDelta::Zero()) +
      TimeDelta::Micros(1);

  EXPECT_EQ(pacer_->NextSendTime() - start_time,
            PacingController::kPausedProcessInterval);
  clock_.AdvanceTime(PacingController::kPausedProcessInterval - time_margin);
  pacer_->ProcessPackets();
  EXPECT_EQ(pacer_->NextSendTime() - start_time,
            PacingController::kPausedProcessInterval);

  clock_.AdvanceTime(time_margin);
  pacer_->ProcessPackets();
  EXPECT_EQ(pacer_->NextSendTime() - start_time,
            2 * PacingController::kPausedProcessInterval);
}

TEST_P(PacingControllerTest, ExpectedQueueTimeMs) {
  uint32_t ssrc = 12346;
  uint16_t sequence_number = 1234;
  const size_t kNumPackets = 60;
  const size_t kPacketSize = 1200;
  const int32_t kMaxBitrate = kPaceMultiplier * 30000;
  EXPECT_EQ(TimeDelta::Zero(), pacer_->OldestPacketWaitTime());

  pacer_->SetPacingRates(DataRate::BitsPerSec(30000 * kPaceMultiplier),
                         DataRate::Zero());
  for (size_t i = 0; i < kNumPackets; ++i) {
    SendAndExpectPacket(RtpPacketMediaType::kVideo, ssrc, sequence_number++,
                        clock_.TimeInMilliseconds(), kPacketSize);
  }

  // Queue in ms = 1000 * (bytes in queue) *8 / (bits per second)
  TimeDelta queue_time =
      TimeDelta::Millis(1000 * kNumPackets * kPacketSize * 8 / kMaxBitrate);
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
            TimeDelta::Millis(1000 * kPacketSize * 8 / kMaxBitrate));
}

TEST_P(PacingControllerTest, QueueTimeGrowsOverTime) {
  uint32_t ssrc = 12346;
  uint16_t sequence_number = 1234;
  EXPECT_EQ(TimeDelta::Zero(), pacer_->OldestPacketWaitTime());

  pacer_->SetPacingRates(DataRate::BitsPerSec(30000 * kPaceMultiplier),
                         DataRate::Zero());
  SendAndExpectPacket(RtpPacketMediaType::kVideo, ssrc, sequence_number,
                      clock_.TimeInMilliseconds(), 1200);

  clock_.AdvanceTimeMilliseconds(500);
  EXPECT_EQ(TimeDelta::Millis(500), pacer_->OldestPacketWaitTime());
  pacer_->ProcessPackets();
  EXPECT_EQ(TimeDelta::Zero(), pacer_->OldestPacketWaitTime());
}

TEST_P(PacingControllerTest, ProbingWithInsertedPackets) {
  const size_t kPacketSize = 1200;
  const int kInitialBitrateBps = 300000;
  uint32_t ssrc = 12346;
  uint16_t sequence_number = 1234;

  PacingControllerProbing packet_sender;
  pacer_ = std::make_unique<PacingController>(&clock_, &packet_sender, nullptr,
                                              nullptr, GetParam());
  pacer_->CreateProbeCluster(kFirstClusterRate,
                             /*cluster_id=*/0);
  pacer_->CreateProbeCluster(kSecondClusterRate,
                             /*cluster_id=*/1);
  pacer_->SetPacingRates(
      DataRate::BitsPerSec(kInitialBitrateBps * kPaceMultiplier),
      DataRate::Zero());

  for (int i = 0; i < 10; ++i) {
    Send(RtpPacketMediaType::kVideo, ssrc, sequence_number++,
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

TEST_P(PacingControllerTest, SkipsProbesWhenProcessIntervalTooLarge) {
  const size_t kPacketSize = 1200;
  const int kInitialBitrateBps = 300000;
  uint32_t ssrc = 12346;
  uint16_t sequence_number = 1234;

  PacingControllerProbing packet_sender;
  pacer_ = std::make_unique<PacingController>(&clock_, &packet_sender, nullptr,
                                              nullptr, GetParam());
  pacer_->SetPacingRates(
      DataRate::BitsPerSec(kInitialBitrateBps * kPaceMultiplier),
      DataRate::Zero());

  for (int i = 0; i < 10; ++i) {
    Send(RtpPacketMediaType::kVideo, ssrc, sequence_number++,
         clock_.TimeInMilliseconds(), kPacketSize);
  }
  while (pacer_->QueueSizePackets() > 0) {
    clock_.AdvanceTime(TimeUntilNextProcess());
    pacer_->ProcessPackets();
  }

  // Probe at a very high rate.
  pacer_->CreateProbeCluster(DataRate::KilobitsPerSec(10000),  // 10 Mbps.
                             /*cluster_id=*/3);
  // We need one packet to start the probe.
  Send(RtpPacketMediaType::kVideo, ssrc, sequence_number++,
       clock_.TimeInMilliseconds(), kPacketSize);
  const int packets_sent_before_probe = packet_sender.packets_sent();
  clock_.AdvanceTime(TimeUntilNextProcess());
  pacer_->ProcessPackets();
  EXPECT_EQ(packet_sender.packets_sent(), packets_sent_before_probe + 1);

  // Figure out how long between probe packets.
  Timestamp start_time = clock_.CurrentTime();
  clock_.AdvanceTime(TimeUntilNextProcess());
  TimeDelta time_between_probes = clock_.CurrentTime() - start_time;
  // Advance that distance again + 1ms.
  clock_.AdvanceTime(time_between_probes);

  // Send second probe packet.
  Send(RtpPacketMediaType::kVideo, ssrc, sequence_number++,
       clock_.TimeInMilliseconds(), kPacketSize);
  pacer_->ProcessPackets();
  EXPECT_EQ(packet_sender.packets_sent(), packets_sent_before_probe + 2);

  // We're exactly where we should be for the next probe.
  const Timestamp probe_time = clock_.CurrentTime();
  EXPECT_EQ(pacer_->NextSendTime(), clock_.CurrentTime());

  FieldTrialBasedConfig field_trial_config;
  BitrateProberConfig probing_config(&field_trial_config);
  EXPECT_GT(probing_config.max_probe_delay.Get(), TimeDelta::Zero());
  // Advance to within max probe delay, should still return same target.
  clock_.AdvanceTime(probing_config.max_probe_delay.Get());
  EXPECT_EQ(pacer_->NextSendTime(), probe_time);

  // Too high probe delay, drop it!
  clock_.AdvanceTime(TimeDelta::Micros(1));
  EXPECT_GT(pacer_->NextSendTime(), probe_time);
}

TEST_P(PacingControllerTest, ProbingWithPaddingSupport) {
  const size_t kPacketSize = 1200;
  const int kInitialBitrateBps = 300000;
  uint32_t ssrc = 12346;
  uint16_t sequence_number = 1234;

  PacingControllerProbing packet_sender;
  pacer_ = std::make_unique<PacingController>(&clock_, &packet_sender, nullptr,
                                              nullptr, GetParam());
  pacer_->CreateProbeCluster(kFirstClusterRate,
                             /*cluster_id=*/0);
  pacer_->SetPacingRates(
      DataRate::BitsPerSec(kInitialBitrateBps * kPaceMultiplier),
      DataRate::Zero());

  for (int i = 0; i < 3; ++i) {
    Send(RtpPacketMediaType::kVideo, ssrc, sequence_number++,
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

TEST_P(PacingControllerTest, PaddingOveruse) {
  uint32_t ssrc = 12346;
  uint16_t sequence_number = 1234;
  const size_t kPacketSize = 1200;

  // Initially no padding rate.
  pacer_->ProcessPackets();
  pacer_->SetPacingRates(DataRate::BitsPerSec(60000 * kPaceMultiplier),
                         DataRate::Zero());

  SendAndExpectPacket(RtpPacketMediaType::kVideo, ssrc, sequence_number++,
                      clock_.TimeInMilliseconds(), kPacketSize);
  pacer_->ProcessPackets();

  // Add 30kbit padding. When increasing budget, media budget will increase from
  // negative (overuse) while padding budget will increase from 0.
  clock_.AdvanceTimeMilliseconds(5);
  pacer_->SetPacingRates(DataRate::BitsPerSec(60000 * kPaceMultiplier),
                         DataRate::BitsPerSec(30000));

  SendAndExpectPacket(RtpPacketMediaType::kVideo, ssrc, sequence_number++,
                      clock_.TimeInMilliseconds(), kPacketSize);
  EXPECT_LT(TimeDelta::Millis(5), pacer_->ExpectedQueueTime());
  // Don't send padding if queue is non-empty, even if padding budget > 0.
  EXPECT_CALL(callback_, SendPadding).Times(0);
  if (PeriodicProcess()) {
    pacer_->ProcessPackets();
  } else {
    AdvanceTimeAndProcess();
  }
}

TEST_P(PacingControllerTest, ProbeClusterId) {
  MockPacketSender callback;

  pacer_ = std::make_unique<PacingController>(&clock_, &callback, nullptr,
                                              nullptr, GetParam());
  Init();

  uint32_t ssrc = 12346;
  uint16_t sequence_number = 1234;
  const size_t kPacketSize = 1200;

  pacer_->SetPacingRates(kTargetRate * kPaceMultiplier, kTargetRate);
  pacer_->SetProbingEnabled(true);
  for (int i = 0; i < 10; ++i) {
    Send(RtpPacketMediaType::kVideo, ssrc, sequence_number++,
         clock_.TimeInMilliseconds(), kPacketSize);
  }

  // First probing cluster.
  EXPECT_CALL(callback,
              SendRtpPacket(_, Field(&PacedPacketInfo::probe_cluster_id, 0)))
      .Times(5);

  for (int i = 0; i < 5; ++i) {
    AdvanceTimeAndProcess();
  }

  // Second probing cluster.
  EXPECT_CALL(callback,
              SendRtpPacket(_, Field(&PacedPacketInfo::probe_cluster_id, 1)))
      .Times(5);

  for (int i = 0; i < 5; ++i) {
    AdvanceTimeAndProcess();
  }

  // Needed for the Field comparer below.
  const int kNotAProbe = PacedPacketInfo::kNotAProbe;
  // No more probing packets.
  EXPECT_CALL(callback, GeneratePadding).WillOnce([&](DataSize padding_size) {
    std::vector<std::unique_ptr<RtpPacketToSend>> padding_packets;
    padding_packets.emplace_back(
        BuildPacket(RtpPacketMediaType::kPadding, ssrc, sequence_number++,
                    clock_.TimeInMilliseconds(), padding_size.bytes()));
    return padding_packets;
  });
  bool non_probe_packet_seen = false;
  EXPECT_CALL(callback, SendRtpPacket)
      .WillOnce([&](std::unique_ptr<RtpPacketToSend> packet,
                    const PacedPacketInfo& cluster_info) {
        EXPECT_EQ(cluster_info.probe_cluster_id, kNotAProbe);
        non_probe_packet_seen = true;
      });
  while (!non_probe_packet_seen) {
    AdvanceTimeAndProcess();
  }
}

TEST_P(PacingControllerTest, OwnedPacketPrioritizedOnType) {
  MockPacketSender callback;
  pacer_ = std::make_unique<PacingController>(&clock_, &callback, nullptr,
                                              nullptr, GetParam());
  Init();

  // Insert a packet of each type, from low to high priority. Since priority
  // is weighted higher than insert order, these should come out of the pacer
  // in backwards order with the exception of FEC and Video.
  for (RtpPacketMediaType type :
       {RtpPacketMediaType::kPadding,
        RtpPacketMediaType::kForwardErrorCorrection, RtpPacketMediaType::kVideo,
        RtpPacketMediaType::kRetransmission, RtpPacketMediaType::kAudio}) {
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

  while (pacer_->QueueSizePackets() > 0) {
    if (PeriodicProcess()) {
      clock_.AdvanceTimeMilliseconds(5);
      pacer_->ProcessPackets();
    } else {
      AdvanceTimeAndProcess();
    }
  }
}

TEST_P(PacingControllerTest, SmallFirstProbePacket) {
  ScopedFieldTrials trial("WebRTC-Pacer-SmallFirstProbePacket/Enabled/");
  MockPacketSender callback;
  pacer_ = std::make_unique<PacingController>(&clock_, &callback, nullptr,
                                              nullptr, GetParam());
  pacer_->CreateProbeCluster(kFirstClusterRate, /*cluster_id=*/0);
  pacer_->SetPacingRates(kTargetRate * kPaceMultiplier, DataRate::Zero());

  // Add high prio media.
  pacer_->EnqueuePacket(BuildRtpPacket(RtpPacketMediaType::kAudio));

  // Expect small padding packet to be requested.
  EXPECT_CALL(callback, GeneratePadding(DataSize::Bytes(1)))
      .WillOnce([&](DataSize padding_size) {
        std::vector<std::unique_ptr<RtpPacketToSend>> padding_packets;
        padding_packets.emplace_back(
            BuildPacket(RtpPacketMediaType::kPadding, kAudioSsrc, 1,
                        clock_.TimeInMilliseconds(), 1));
        return padding_packets;
      });

  size_t packets_sent = 0;
  bool media_seen = false;
  EXPECT_CALL(callback, SendRtpPacket)
      .Times(::testing::AnyNumber())
      .WillRepeatedly([&](std::unique_ptr<RtpPacketToSend> packet,
                          const PacedPacketInfo& cluster_info) {
        if (packets_sent == 0) {
          EXPECT_EQ(packet->packet_type(), RtpPacketMediaType::kPadding);
        } else {
          if (packet->packet_type() == RtpPacketMediaType::kAudio) {
            media_seen = true;
          }
        }
        packets_sent++;
      });
  while (!media_seen) {
    pacer_->ProcessPackets();
    clock_.AdvanceTimeMilliseconds(5);
  }
}

TEST_P(PacingControllerTest, TaskLate) {
  if (PeriodicProcess()) {
    // This test applies only when NOT using interval budget.
    return;
  }

  // Set a low send rate to more easily test timing issues.
  DataRate kSendRate = DataRate::KilobitsPerSec(30);
  pacer_->SetPacingRates(kSendRate, DataRate::Zero());

  // Add four packets of equal size and priority.
  pacer_->EnqueuePacket(BuildRtpPacket(RtpPacketMediaType::kVideo));
  pacer_->EnqueuePacket(BuildRtpPacket(RtpPacketMediaType::kVideo));
  pacer_->EnqueuePacket(BuildRtpPacket(RtpPacketMediaType::kVideo));
  pacer_->EnqueuePacket(BuildRtpPacket(RtpPacketMediaType::kVideo));

  // Process packets, only first should be sent.
  EXPECT_CALL(callback_, SendPacket).Times(1);
  pacer_->ProcessPackets();

  Timestamp next_send_time = pacer_->NextSendTime();
  const TimeDelta time_between_packets = next_send_time - clock_.CurrentTime();

  // Simulate a late process call, executed just before we allow sending the
  // fourth packet.
  clock_.AdvanceTime((time_between_packets * 3) -
                     (PacingController::kMinSleepTime + TimeDelta::Millis(1)));

  EXPECT_CALL(callback_, SendPacket).Times(2);
  pacer_->ProcessPackets();

  // Check that next scheduled send time is within sleep-time + 1ms.
  next_send_time = pacer_->NextSendTime();
  EXPECT_LE(next_send_time - clock_.CurrentTime(),
            PacingController::kMinSleepTime + TimeDelta::Millis(1));

  // Advance to within error margin for execution.
  clock_.AdvanceTime(TimeDelta::Millis(1));
  EXPECT_CALL(callback_, SendPacket).Times(1);
  pacer_->ProcessPackets();
}

TEST_P(PacingControllerTest, NoProbingWhilePaused) {
  uint32_t ssrc = 12345;
  uint16_t sequence_number = 1234;

  pacer_->SetProbingEnabled(true);

  // Send at least one packet so probing can initate.
  SendAndExpectPacket(RtpPacketMediaType::kVideo, ssrc, sequence_number,
                      clock_.TimeInMilliseconds(), 250);
  while (pacer_->QueueSizePackets() > 0) {
    AdvanceTimeAndProcess();
  }

  // Trigger probing.
  pacer_->CreateProbeCluster(DataRate::KilobitsPerSec(10000),  // 10 Mbps.
                             /*cluster_id=*/3);

  // Time to next send time should be small.
  EXPECT_LT(pacer_->NextSendTime() - clock_.CurrentTime(),
            PacingController::kPausedProcessInterval);

  // Pause pacer, time to next send time should now be the pause process
  // interval.
  pacer_->Pause();

  EXPECT_EQ(pacer_->NextSendTime() - clock_.CurrentTime(),
            PacingController::kPausedProcessInterval);
}

INSTANTIATE_TEST_SUITE_P(
    WithAndWithoutIntervalBudget,
    PacingControllerTest,
    ::testing::Values(PacingController::ProcessMode::kPeriodic,
                      PacingController::ProcessMode::kDynamic));

}  // namespace test
}  // namespace webrtc
