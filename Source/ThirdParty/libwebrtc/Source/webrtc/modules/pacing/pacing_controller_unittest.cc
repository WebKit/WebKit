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
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "api/array_view.h"
#include "api/transport/network_types.h"
#include "api/units/data_rate.h"
#include "api/units/data_size.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "modules/pacing/bitrate_prober.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/rtp_packet_to_send.h"
#include "system_wrappers/include/clock.h"
#include "test/explicit_key_value_config.h"
#include "test/gmock.h"
#include "test/gtest.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Field;
using ::testing::NiceMock;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::Return;
using ::testing::WithoutArgs;

using ::webrtc::test::ExplicitKeyValueConfig;

namespace webrtc {
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
  packet->set_capture_time(Timestamp::Millis(capture_time_ms));
  packet->SetPayloadSize(size);
  return packet;
}

class MediaStream {
 public:
  MediaStream(SimulatedClock& clock,
              RtpPacketMediaType type,
              uint32_t ssrc,
              size_t packet_size)
      : clock_(clock), type_(type), ssrc_(ssrc), packet_size_(packet_size) {}

  std::unique_ptr<RtpPacketToSend> BuildNextPacket() {
    return BuildPacket(type_, ssrc_, seq_num_++, clock_.TimeInMilliseconds(),
                       packet_size_);
  }
  std::unique_ptr<RtpPacketToSend> BuildNextPacket(size_t size) {
    return BuildPacket(type_, ssrc_, seq_num_++, clock_.TimeInMilliseconds(),
                       size);
  }

 private:
  SimulatedClock& clock_;
  const RtpPacketMediaType type_;
  const uint32_t ssrc_;
  const size_t packet_size_;
  uint16_t seq_num_ = 1000;
};

// Mock callback proxy, where both new and old api redirects to common mock
// methods that focus on core aspects.
class MockPacingControllerCallback : public PacingController::PacketSender {
 public:
  void SendPacket(std::unique_ptr<RtpPacketToSend> packet,
                  const PacedPacketInfo& /* cluster_info */) override {
    SendPacket(packet->Ssrc(), packet->SequenceNumber(),
               packet->capture_time().ms(),
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

  MOCK_METHOD(void,
              SendPacket,
              (uint32_t ssrc,
               uint16_t sequence_number,
               int64_t capture_timestamp,
               bool retransmission,
               bool padding));
  MOCK_METHOD(std::vector<std::unique_ptr<RtpPacketToSend>>,
              FetchFec,
              (),
              (override));
  MOCK_METHOD(size_t, SendPadding, (size_t target_size));
  MOCK_METHOD(void,
              OnAbortedRetransmissions,
              (uint32_t, rtc::ArrayView<const uint16_t>),
              (override));
  MOCK_METHOD(std::optional<uint32_t>,
              GetRtxSsrcForMedia,
              (uint32_t),
              (const, override));
  MOCK_METHOD(void, OnBatchComplete, (), (override));
};

// Mock callback implementing the raw api.
class MockPacketSender : public PacingController::PacketSender {
 public:
  MOCK_METHOD(void,
              SendPacket,
              (std::unique_ptr<RtpPacketToSend> packet,
               const PacedPacketInfo& cluster_info),
              (override));
  MOCK_METHOD(std::vector<std::unique_ptr<RtpPacketToSend>>,
              FetchFec,
              (),
              (override));

  MOCK_METHOD(std::vector<std::unique_ptr<RtpPacketToSend>>,
              GeneratePadding,
              (DataSize target_size),
              (override));
  MOCK_METHOD(void,
              OnAbortedRetransmissions,
              (uint32_t, rtc::ArrayView<const uint16_t>),
              (override));
  MOCK_METHOD(std::optional<uint32_t>,
              GetRtxSsrcForMedia,
              (uint32_t),
              (const, override));
  MOCK_METHOD(void, OnBatchComplete, (), (override));
};

class PacingControllerPadding : public PacingController::PacketSender {
 public:
  static const size_t kPaddingPacketSize = 224;

  PacingControllerPadding() : padding_sent_(0), total_bytes_sent_(0) {}

  void SendPacket(std::unique_ptr<RtpPacketToSend> packet,
                  const PacedPacketInfo& /* pacing_info */) override {
    total_bytes_sent_ += packet->payload_size();
  }

  std::vector<std::unique_ptr<RtpPacketToSend>> FetchFec() override {
    return {};
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

  void OnAbortedRetransmissions(uint32_t,
                                rtc::ArrayView<const uint16_t>) override {}
  std::optional<uint32_t> GetRtxSsrcForMedia(uint32_t) const override {
    return std::nullopt;
  }

  void OnBatchComplete() override {}

  size_t padding_sent() { return padding_sent_; }
  size_t total_bytes_sent() { return total_bytes_sent_; }

 private:
  size_t padding_sent_;
  size_t total_bytes_sent_;
};

class PacingControllerProbing : public PacingController::PacketSender {
 public:
  PacingControllerProbing() = default;
  // Controls if padding can be generated or not.
  // In real implementation, padding can only be generated after a sent
  // media packet, or if the sender support RTX.
  void SetCanGeneratePadding(bool can_generate) {
    can_generate_padding_ = can_generate;
  }

  void SendPacket(std::unique_ptr<RtpPacketToSend> packet,
                  const PacedPacketInfo& pacing_info) override {
    if (packet->packet_type() != RtpPacketMediaType::kPadding) {
      ++packets_sent_;
    } else {
      ++padding_packets_sent_;
    }
    last_pacing_info_ = pacing_info;
  }

  std::vector<std::unique_ptr<RtpPacketToSend>> FetchFec() override {
    return {};
  }

  std::vector<std::unique_ptr<RtpPacketToSend>> GeneratePadding(
      DataSize target_size) override {
    if (!can_generate_padding_) {
      return {};
    }
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

  void OnAbortedRetransmissions(uint32_t,
                                rtc::ArrayView<const uint16_t>) override {}
  std::optional<uint32_t> GetRtxSsrcForMedia(uint32_t) const override {
    return std::nullopt;
  }
  void OnBatchComplete() override {}

  int packets_sent() const { return packets_sent_; }
  int padding_packets_sent() const { return padding_packets_sent_; }
  int padding_sent() const { return padding_sent_; }
  int total_packets_sent() const { return packets_sent_ + padding_sent_; }
  PacedPacketInfo last_pacing_info() const { return last_pacing_info_; }

 private:
  bool can_generate_padding_ = true;
  int packets_sent_ = 0;
  int padding_packets_sent_ = 0;
  int padding_sent_ = 0;
  PacedPacketInfo last_pacing_info_;
};

class PacingControllerTest : public ::testing::Test {
 protected:
  PacingControllerTest() : clock_(123456), trials_("") {}

  void SendAndExpectPacket(PacingController* pacer,
                           RtpPacketMediaType type,
                           uint32_t ssrc,
                           uint16_t sequence_number,
                           int64_t capture_time_ms,
                           size_t size) {
    pacer->EnqueuePacket(
        BuildPacket(type, ssrc, sequence_number, capture_time_ms, size));

    EXPECT_CALL(callback_,
                SendPacket(ssrc, sequence_number, capture_time_ms,
                           type == RtpPacketMediaType::kRetransmission, false));
  }

  void AdvanceTimeUntil(webrtc::Timestamp time) {
    Timestamp now = clock_.CurrentTime();
    clock_.AdvanceTime(std::max(TimeDelta::Zero(), time - now));
  }

  void ConsumeInitialBudget(PacingController* pacer) {
    const uint32_t kSsrc = 54321;
    uint16_t sequence_number = 1234;
    int64_t capture_time_ms = clock_.TimeInMilliseconds();
    const size_t kPacketSize = 250;

    EXPECT_TRUE(pacer->OldestPacketEnqueueTime().IsInfinite());

    // Due to the multiplicative factor we can send 5 packets during a send
    // interval. (network capacity * multiplier / (8 bits per byte *
    // (packet size * #send intervals per second)
    const size_t packets_to_send_per_interval =
        kTargetRate.bps() * kPaceMultiplier / (8 * kPacketSize * 200);
    for (size_t i = 0; i < packets_to_send_per_interval; ++i) {
      SendAndExpectPacket(pacer, RtpPacketMediaType::kVideo, kSsrc,
                          sequence_number++, capture_time_ms, kPacketSize);
    }

    while (pacer->QueueSizePackets() > 0) {
      AdvanceTimeUntil(pacer->NextSendTime());
      pacer->ProcessPackets();
    }
  }

  SimulatedClock clock_;

  MediaStream audio_ = MediaStream(clock_,
                                   /*type*/ RtpPacketMediaType::kAudio,
                                   /*ssrc*/ kAudioSsrc,
                                   /*packet_size*/ 100);
  MediaStream video_ = MediaStream(clock_,
                                   /*type*/ RtpPacketMediaType::kVideo,
                                   /*ssrc*/ kVideoSsrc,
                                   /*packet_size*/ 1000);

  ::testing::NiceMock<MockPacingControllerCallback> callback_;
  ExplicitKeyValueConfig trials_;
};

TEST_F(PacingControllerTest, DefaultNoPaddingInSilence) {
  const test::ExplicitKeyValueConfig trials("");
  PacingController pacer(&clock_, &callback_, trials);
  pacer.SetPacingRates(kTargetRate, DataRate::Zero());
  // Video packet to reset last send time and provide padding data.
  pacer.EnqueuePacket(video_.BuildNextPacket());
  EXPECT_CALL(callback_, SendPacket).Times(1);
  clock_.AdvanceTimeMilliseconds(5);
  pacer.ProcessPackets();
  EXPECT_CALL(callback_, SendPadding).Times(0);
  // Waiting 500 ms should not trigger sending of padding.
  clock_.AdvanceTimeMilliseconds(500);
  pacer.ProcessPackets();
}

TEST_F(PacingControllerTest, PaddingInSilenceWithTrial) {
  const test::ExplicitKeyValueConfig trials(
      "WebRTC-Pacer-PadInSilence/Enabled/");
  PacingController pacer(&clock_, &callback_, trials);
  pacer.SetPacingRates(kTargetRate, DataRate::Zero());
  // Video packet to reset last send time and provide padding data.
  pacer.EnqueuePacket(video_.BuildNextPacket());
  EXPECT_CALL(callback_, SendPacket).Times(2);
  clock_.AdvanceTimeMilliseconds(5);
  pacer.ProcessPackets();
  EXPECT_CALL(callback_, SendPadding).WillOnce(Return(1000));
  // Waiting 500 ms should trigger sending of padding.
  clock_.AdvanceTimeMilliseconds(500);
  pacer.ProcessPackets();
}

TEST_F(PacingControllerTest, CongestionWindowAffectsAudioInTrial) {
  const test::ExplicitKeyValueConfig trials("WebRTC-Pacer-BlockAudio/Enabled/");
  EXPECT_CALL(callback_, SendPadding).Times(0);
  PacingController pacer(&clock_, &callback_, trials);
  pacer.SetPacingRates(DataRate::KilobitsPerSec(10000), DataRate::Zero());
  // Video packet fills congestion window.
  pacer.EnqueuePacket(video_.BuildNextPacket());
  EXPECT_CALL(callback_, SendPacket).Times(1);
  AdvanceTimeUntil(pacer.NextSendTime());
  pacer.ProcessPackets();
  pacer.SetCongested(true);
  // Audio packet blocked due to congestion.
  pacer.EnqueuePacket(audio_.BuildNextPacket());
  EXPECT_CALL(callback_, SendPacket).Times(0);
  // Forward time to where we send keep-alive.
  EXPECT_CALL(callback_, SendPadding(1)).Times(2);
  AdvanceTimeUntil(pacer.NextSendTime());
  pacer.ProcessPackets();
  AdvanceTimeUntil(pacer.NextSendTime());
  pacer.ProcessPackets();
  // Audio packet unblocked when congestion window clear.
  ::testing::Mock::VerifyAndClearExpectations(&callback_);
  pacer.SetCongested(false);
  EXPECT_CALL(callback_, SendPacket).Times(1);
  AdvanceTimeUntil(pacer.NextSendTime());
  pacer.ProcessPackets();
}

TEST_F(PacingControllerTest, DefaultCongestionWindowDoesNotAffectAudio) {
  EXPECT_CALL(callback_, SendPadding).Times(0);
  const test::ExplicitKeyValueConfig trials("");
  PacingController pacer(&clock_, &callback_, trials);
  pacer.SetPacingRates(DataRate::BitsPerSec(10000000), DataRate::Zero());
  // Video packet fills congestion window.
  pacer.EnqueuePacket(video_.BuildNextPacket());
  EXPECT_CALL(callback_, SendPacket).Times(1);
  AdvanceTimeUntil(pacer.NextSendTime());
  pacer.ProcessPackets();
  pacer.SetCongested(true);
  // Audio not blocked due to congestion.
  pacer.EnqueuePacket(audio_.BuildNextPacket());
  EXPECT_CALL(callback_, SendPacket).Times(1);
  AdvanceTimeUntil(pacer.NextSendTime());
  pacer.ProcessPackets();
}

TEST_F(PacingControllerTest, BudgetAffectsAudioInTrial) {
  ExplicitKeyValueConfig trials("WebRTC-Pacer-BlockAudio/Enabled/");
  PacingController pacer(&clock_, &callback_, trials);
  const size_t kPacketSize = 1000;
  const int kProcessIntervalsPerSecond = 1000 / 5;
  DataRate pacing_rate =
      DataRate::BitsPerSec(kPacketSize / 3 * 8 * kProcessIntervalsPerSecond);
  pacer.SetPacingRates(pacing_rate, DataRate::Zero());
  pacer.SetSendBurstInterval(TimeDelta::Zero());
  // Video fills budget for following process periods.
  pacer.EnqueuePacket(video_.BuildNextPacket(kPacketSize));
  EXPECT_CALL(callback_, SendPacket).Times(1);
  AdvanceTimeUntil(pacer.NextSendTime());
  pacer.ProcessPackets();
  // Audio packet blocked due to budget limit.
  pacer.EnqueuePacket(audio_.BuildNextPacket());
  Timestamp wait_start_time = clock_.CurrentTime();
  Timestamp wait_end_time = Timestamp::MinusInfinity();
  EXPECT_CALL(callback_, SendPacket).WillOnce(WithoutArgs([&]() {
    wait_end_time = clock_.CurrentTime();
  }));
  while (!wait_end_time.IsFinite()) {
    AdvanceTimeUntil(pacer.NextSendTime());
    pacer.ProcessPackets();
  }
  const TimeDelta expected_wait_time =
      DataSize::Bytes(kPacketSize) / pacing_rate;
  // Verify delay is near expectation, within timing margin.
  EXPECT_LT(((wait_end_time - wait_start_time) - expected_wait_time).Abs(),
            PacingController::kMinSleepTime);
}

TEST_F(PacingControllerTest, DefaultBudgetDoesNotAffectAudio) {
  const size_t kPacketSize = 1000;
  EXPECT_CALL(callback_, SendPadding).Times(0);
  const test::ExplicitKeyValueConfig trials("");
  PacingController pacer(&clock_, &callback_, trials);
  const int kProcessIntervalsPerSecond = 1000 / 5;
  pacer.SetPacingRates(
      DataRate::BitsPerSec(kPacketSize / 3 * 8 * kProcessIntervalsPerSecond),
      DataRate::Zero());
  // Video fills budget for following process periods.
  pacer.EnqueuePacket(video_.BuildNextPacket(kPacketSize));
  EXPECT_CALL(callback_, SendPacket).Times(1);
  AdvanceTimeUntil(pacer.NextSendTime());
  pacer.ProcessPackets();
  // Audio packet not blocked due to budget limit.
  EXPECT_CALL(callback_, SendPacket).Times(1);
  pacer.EnqueuePacket(audio_.BuildNextPacket());
  AdvanceTimeUntil(pacer.NextSendTime());
  pacer.ProcessPackets();
}

TEST_F(PacingControllerTest, FirstSentPacketTimeIsSet) {
  const Timestamp kStartTime = clock_.CurrentTime();
  auto pacer = std::make_unique<PacingController>(&clock_, &callback_, trials_);
  pacer->SetPacingRates(kTargetRate * kPaceMultiplier, DataRate::Zero());

  // No packet sent.
  EXPECT_FALSE(pacer->FirstSentPacketTime().has_value());
  pacer->EnqueuePacket(video_.BuildNextPacket());
  AdvanceTimeUntil(pacer->NextSendTime());
  pacer->ProcessPackets();
  EXPECT_EQ(kStartTime, pacer->FirstSentPacketTime());
}

TEST_F(PacingControllerTest, QueueAndPacePacketsWithZeroBurstPeriod) {
  const uint32_t kSsrc = 12345;
  uint16_t sequence_number = 1234;
  const DataSize kPackeSize = DataSize::Bytes(250);
  const TimeDelta kSendInterval = TimeDelta::Millis(5);

  // Due to the multiplicative factor we can send 5 packets during a 5ms send
  // interval. (send interval * network capacity * multiplier / packet size)
  const size_t kPacketsToSend = (kSendInterval * kTargetRate).bytes() *
                                kPaceMultiplier / kPackeSize.bytes();
  auto pacer = std::make_unique<PacingController>(&clock_, &callback_, trials_);
  pacer->SetSendBurstInterval(TimeDelta::Zero());
  pacer->SetPacingRates(kTargetRate * kPaceMultiplier, DataRate::Zero());

  for (size_t i = 0; i < kPacketsToSend; ++i) {
    SendAndExpectPacket(pacer.get(), RtpPacketMediaType::kVideo, kSsrc,
                        sequence_number++, clock_.TimeInMilliseconds(),
                        kPackeSize.bytes());
  }
  EXPECT_CALL(callback_, SendPadding).Times(0);

  // Enqueue one extra packet.
  int64_t queued_packet_timestamp = clock_.TimeInMilliseconds();
  pacer->EnqueuePacket(BuildPacket(RtpPacketMediaType::kVideo, kSsrc,
                                   sequence_number, queued_packet_timestamp,
                                   kPackeSize.bytes()));
  EXPECT_EQ(kPacketsToSend + 1, pacer->QueueSizePackets());

  // Send packets until the initial kPacketsToSend packets are done.
  Timestamp start_time = clock_.CurrentTime();
  while (pacer->QueueSizePackets() > 1) {
    AdvanceTimeUntil(pacer->NextSendTime());
    pacer->ProcessPackets();
  }
  EXPECT_LT(clock_.CurrentTime() - start_time, kSendInterval);

  // Proceed till last packet can be sent.
  EXPECT_CALL(callback_, SendPacket(kSsrc, sequence_number,
                                    queued_packet_timestamp, false, false))
      .Times(1);
  AdvanceTimeUntil(pacer->NextSendTime());
  pacer->ProcessPackets();
  EXPECT_GE(clock_.CurrentTime() - start_time, kSendInterval);
  EXPECT_EQ(pacer->QueueSizePackets(), 0u);
}

TEST_F(PacingControllerTest, PaceQueuedPackets) {
  uint32_t ssrc = 12345;
  uint16_t sequence_number = 1234;
  const size_t kPacketSize = 250;
  auto pacer = std::make_unique<PacingController>(&clock_, &callback_, trials_);
  pacer->SetPacingRates(kTargetRate * kPaceMultiplier, DataRate::Zero());

  const size_t packets_to_send_per_burst_interval =
      (kTargetRate * kPaceMultiplier * PacingController::kDefaultBurstInterval)
          .bytes() /
      kPacketSize;
  for (size_t i = 0; i < packets_to_send_per_burst_interval; ++i) {
    SendAndExpectPacket(pacer.get(), RtpPacketMediaType::kVideo, ssrc,
                        sequence_number++, clock_.TimeInMilliseconds(),
                        kPacketSize);
  }

  for (size_t j = 0; j < packets_to_send_per_burst_interval * 10; ++j) {
    pacer->EnqueuePacket(BuildPacket(RtpPacketMediaType::kVideo, ssrc,
                                     sequence_number++,
                                     clock_.TimeInMilliseconds(), kPacketSize));
  }
  EXPECT_EQ(packets_to_send_per_burst_interval +
                packets_to_send_per_burst_interval * 10,
            pacer->QueueSizePackets());

  while (pacer->QueueSizePackets() > packets_to_send_per_burst_interval * 10) {
    AdvanceTimeUntil(pacer->NextSendTime());
    pacer->ProcessPackets();
  }
  EXPECT_EQ(pacer->QueueSizePackets(), packets_to_send_per_burst_interval * 10);
  EXPECT_CALL(callback_, SendPadding).Times(0);

  EXPECT_CALL(callback_, SendPacket(ssrc, _, _, false, false))
      .Times(pacer->QueueSizePackets());
  const TimeDelta expected_pace_time =
      DataSize::Bytes(pacer->QueueSizePackets() * kPacketSize) /
      (kPaceMultiplier * kTargetRate);
  Timestamp start_time = clock_.CurrentTime();
  while (pacer->QueueSizePackets() > 0) {
    AdvanceTimeUntil(pacer->NextSendTime());
    pacer->ProcessPackets();
  }
  const TimeDelta actual_pace_time = clock_.CurrentTime() - start_time;
  EXPECT_LT((actual_pace_time - expected_pace_time).Abs(),
            PacingController::kMinSleepTime);

  EXPECT_EQ(0u, pacer->QueueSizePackets());
  AdvanceTimeUntil(pacer->NextSendTime());
  EXPECT_EQ(0u, pacer->QueueSizePackets());
  pacer->ProcessPackets();

  // Send some more packet, just show that we can..?
  for (size_t i = 0; i < packets_to_send_per_burst_interval; ++i) {
    SendAndExpectPacket(pacer.get(), RtpPacketMediaType::kVideo, ssrc,
                        sequence_number++, clock_.TimeInMilliseconds(), 250);
  }
  EXPECT_EQ(packets_to_send_per_burst_interval, pacer->QueueSizePackets());
  for (size_t i = 0; i < packets_to_send_per_burst_interval; ++i) {
    AdvanceTimeUntil(pacer->NextSendTime());
    pacer->ProcessPackets();
  }
  EXPECT_EQ(0u, pacer->QueueSizePackets());
}

TEST_F(PacingControllerTest, RepeatedRetransmissionsAllowed) {
  auto pacer = std::make_unique<PacingController>(&clock_, &callback_, trials_);
  pacer->SetPacingRates(kTargetRate * kPaceMultiplier, DataRate::Zero());

  // Send one packet, then two retransmissions of that packet.
  for (size_t i = 0; i < 3; i++) {
    constexpr uint32_t ssrc = 333;
    constexpr uint16_t sequence_number = 444;
    constexpr size_t bytes = 250;
    bool is_retransmission = (i != 0);  // Original followed by retransmissions.
    SendAndExpectPacket(pacer.get(),
                        is_retransmission ? RtpPacketMediaType::kRetransmission
                                          : RtpPacketMediaType::kVideo,
                        ssrc, sequence_number, clock_.TimeInMilliseconds(),
                        bytes);
    clock_.AdvanceTimeMilliseconds(5);
  }
  while (pacer->QueueSizePackets() > 0) {
    AdvanceTimeUntil(pacer->NextSendTime());
    pacer->ProcessPackets();
  }
}

TEST_F(PacingControllerTest,
       CanQueuePacketsWithSameSequenceNumberOnDifferentSsrcs) {
  uint32_t ssrc = 12345;
  uint16_t sequence_number = 1234;
  auto pacer = std::make_unique<PacingController>(&clock_, &callback_, trials_);
  pacer->SetPacingRates(kTargetRate * kPaceMultiplier, DataRate::Zero());

  SendAndExpectPacket(pacer.get(), RtpPacketMediaType::kVideo, ssrc,
                      sequence_number, clock_.TimeInMilliseconds(), 250);

  // Expect packet on second ssrc to be queued and sent as well.
  SendAndExpectPacket(pacer.get(), RtpPacketMediaType::kVideo, ssrc + 1,
                      sequence_number, clock_.TimeInMilliseconds(), 250);

  clock_.AdvanceTimeMilliseconds(1000);
  while (pacer->QueueSizePackets() > 0) {
    AdvanceTimeUntil(pacer->NextSendTime());
    pacer->ProcessPackets();
  }
}

TEST_F(PacingControllerTest, Padding) {
  uint32_t ssrc = 12345;
  uint16_t sequence_number = 1234;
  const size_t kPacketSize = 1000;
  auto pacer = std::make_unique<PacingController>(&clock_, &callback_, trials_);
  pacer->SetPacingRates(kTargetRate * kPaceMultiplier, kTargetRate);

  const size_t kPacketsToSend = 30;
  for (size_t i = 0; i < kPacketsToSend; ++i) {
    SendAndExpectPacket(pacer.get(), RtpPacketMediaType::kVideo, ssrc,
                        sequence_number++, clock_.TimeInMilliseconds(),
                        kPacketSize);
  }

  int expected_bursts =
      floor(DataSize::Bytes(pacer->QueueSizePackets() * kPacketSize) /
            (kPaceMultiplier * kTargetRate) /
            PacingController::kDefaultBurstInterval);
  const TimeDelta expected_pace_time =
      (expected_bursts - 1) * PacingController::kDefaultBurstInterval;
  EXPECT_CALL(callback_, SendPadding).Times(0);
  // Only the media packets should be sent.
  Timestamp start_time = clock_.CurrentTime();
  while (pacer->QueueSizePackets() > 0) {
    AdvanceTimeUntil(pacer->NextSendTime());
    pacer->ProcessPackets();
  }
  const TimeDelta actual_pace_time = clock_.CurrentTime() - start_time;
  EXPECT_LE((actual_pace_time - expected_pace_time).Abs(),
            PacingController::kDefaultBurstInterval);

  // Pacing media happens at 2.5x, but padding was configured with 1.0x
  // factor. We have to wait until the padding debt is gone before we start
  // sending padding.
  const TimeDelta time_to_padding_debt_free =
      (expected_pace_time * kPaceMultiplier) - actual_pace_time;
  clock_.AdvanceTime(time_to_padding_debt_free -
                     PacingController::kMinSleepTime);
  pacer->ProcessPackets();

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
    AdvanceTimeUntil(pacer->NextSendTime());
    pacer->ProcessPackets();
  }

  // Verify rate of sent padding.
  TimeDelta padding_duration = last_send_time - first_send_time;
  DataRate padding_rate = padding_sent / padding_duration;
  EXPECT_EQ(padding_rate, kTargetRate);
}

TEST_F(PacingControllerTest, NoPaddingBeforeNormalPacket) {
  auto pacer = std::make_unique<PacingController>(&clock_, &callback_, trials_);
  pacer->SetPacingRates(kTargetRate * kPaceMultiplier, kTargetRate);

  EXPECT_CALL(callback_, SendPadding).Times(0);

  pacer->ProcessPackets();
  AdvanceTimeUntil(pacer->NextSendTime());

  pacer->ProcessPackets();
  AdvanceTimeUntil(pacer->NextSendTime());

  uint32_t ssrc = 12345;
  uint16_t sequence_number = 1234;
  int64_t capture_time_ms = 56789;

  SendAndExpectPacket(pacer.get(), RtpPacketMediaType::kVideo, ssrc,
                      sequence_number++, capture_time_ms, 250);
  bool padding_sent = false;
  EXPECT_CALL(callback_, SendPadding).WillOnce([&](size_t padding) {
    padding_sent = true;
    return padding;
  });
  EXPECT_CALL(callback_, SendPacket(_, _, _, _, true)).Times(1);
  while (!padding_sent) {
    AdvanceTimeUntil(pacer->NextSendTime());
    pacer->ProcessPackets();
  }
}

TEST_F(PacingControllerTest, VerifyAverageBitrateVaryingMediaPayload) {
  uint32_t ssrc = 12345;
  uint16_t sequence_number = 1234;
  int64_t capture_time_ms = 56789;
  const TimeDelta kAveragingWindowLength = TimeDelta::Seconds(10);
  PacingControllerPadding callback;
  auto pacer = std::make_unique<PacingController>(&clock_, &callback, trials_);
  pacer->SetProbingEnabled(false);
  pacer->SetPacingRates(kTargetRate * kPaceMultiplier, kTargetRate);

  Timestamp start_time = clock_.CurrentTime();
  size_t media_bytes = 0;
  while (clock_.CurrentTime() - start_time < kAveragingWindowLength) {
    // Maybe add some new media packets corresponding to expected send rate.
    int rand_value = rand();  // NOLINT (rand_r instead of rand)
    while (
        media_bytes <
        (kTargetRate * (clock_.CurrentTime() - start_time)).bytes<size_t>()) {
      size_t media_payload = rand_value % 400 + 800;  // [400, 1200] bytes.
      pacer->EnqueuePacket(BuildPacket(RtpPacketMediaType::kVideo, ssrc,
                                       sequence_number++, capture_time_ms,
                                       media_payload));
      media_bytes += media_payload;
    }
    AdvanceTimeUntil(std::min(clock_.CurrentTime() + TimeDelta::Millis(20),
                              pacer->NextSendTime()));
    pacer->ProcessPackets();
  }

  EXPECT_NEAR(
      kTargetRate.bps(),
      (DataSize::Bytes(callback.total_bytes_sent()) / kAveragingWindowLength)
          .bps(),
      (kTargetRate * 0.01 /* 1% error marging */).bps());
}

TEST_F(PacingControllerTest, Priority) {
  uint32_t ssrc_low_priority = 12345;
  uint32_t ssrc = 12346;
  uint16_t sequence_number = 1234;
  int64_t capture_time_ms = 56789;
  int64_t capture_time_ms_low_priority = 1234567;
  auto pacer = std::make_unique<PacingController>(&clock_, &callback_, trials_);
  pacer->SetPacingRates(kTargetRate * kPaceMultiplier, DataRate::Zero());

  ConsumeInitialBudget(pacer.get());

  // Expect normal and low priority to be queued and high to pass through.
  pacer->EnqueuePacket(BuildPacket(RtpPacketMediaType::kVideo,
                                   ssrc_low_priority, sequence_number++,
                                   capture_time_ms_low_priority, 250));

  const size_t packets_to_send_per_interval =
      kTargetRate.bps() * kPaceMultiplier / (8 * 250 * 200);
  for (size_t i = 0; i < packets_to_send_per_interval; ++i) {
    pacer->EnqueuePacket(BuildPacket(RtpPacketMediaType::kRetransmission, ssrc,
                                     sequence_number++, capture_time_ms, 250));
  }
  pacer->EnqueuePacket(BuildPacket(RtpPacketMediaType::kAudio, ssrc,
                                   sequence_number++, capture_time_ms, 250));

  // Expect all high and normal priority to be sent out first.
  EXPECT_CALL(callback_, SendPadding).Times(0);
  testing::Sequence s;
  EXPECT_CALL(callback_, SendPacket(ssrc, _, capture_time_ms, _, _))
      .Times(packets_to_send_per_interval + 1)
      .InSequence(s);
  EXPECT_CALL(callback_, SendPacket(ssrc_low_priority, _,
                                    capture_time_ms_low_priority, _, _))
      .InSequence(s);

  while (pacer->QueueSizePackets() > 0) {
    AdvanceTimeUntil(pacer->NextSendTime());
    pacer->ProcessPackets();
  }
}

TEST_F(PacingControllerTest, RetransmissionPriority) {
  uint32_t ssrc = 12345;
  uint16_t sequence_number = 1234;
  int64_t capture_time_ms = 45678;
  int64_t capture_time_ms_retransmission = 56789;
  auto pacer = std::make_unique<PacingController>(&clock_, &callback_, trials_);
  pacer->SetPacingRates(kTargetRate * kPaceMultiplier, DataRate::Zero());

  const size_t packets_to_send_per_burst_interval =
      (kTargetRate * kPaceMultiplier * PacingController::kDefaultBurstInterval)
          .bytes() /
      250;
  pacer->ProcessPackets();
  EXPECT_EQ(0u, pacer->QueueSizePackets());

  // Alternate retransmissions and normal packets.
  for (size_t i = 0; i < packets_to_send_per_burst_interval; ++i) {
    pacer->EnqueuePacket(BuildPacket(RtpPacketMediaType::kVideo, ssrc,
                                     sequence_number++, capture_time_ms, 250));
    pacer->EnqueuePacket(BuildPacket(RtpPacketMediaType::kRetransmission, ssrc,
                                     sequence_number++,
                                     capture_time_ms_retransmission, 250));
  }
  EXPECT_EQ(2 * packets_to_send_per_burst_interval, pacer->QueueSizePackets());

  // Expect all retransmissions to be sent out first despite having a later
  // capture time.
  EXPECT_CALL(callback_, SendPadding).Times(0);
  EXPECT_CALL(callback_, SendPacket(_, _, _, false, _)).Times(0);
  EXPECT_CALL(callback_,
              SendPacket(ssrc, _, capture_time_ms_retransmission, true, _))
      .Times(packets_to_send_per_burst_interval);

  while (pacer->QueueSizePackets() > packets_to_send_per_burst_interval) {
    AdvanceTimeUntil(pacer->NextSendTime());
    pacer->ProcessPackets();
  }
  EXPECT_EQ(packets_to_send_per_burst_interval, pacer->QueueSizePackets());

  // Expect the remaining (non-retransmission) packets to be sent.
  EXPECT_CALL(callback_, SendPadding).Times(0);
  EXPECT_CALL(callback_, SendPacket(_, _, _, true, _)).Times(0);
  EXPECT_CALL(callback_, SendPacket(ssrc, _, capture_time_ms, false, _))
      .Times(packets_to_send_per_burst_interval);

  while (pacer->QueueSizePackets() > 0) {
    AdvanceTimeUntil(pacer->NextSendTime());
    pacer->ProcessPackets();
  }
  EXPECT_EQ(0u, pacer->QueueSizePackets());
}

TEST_F(PacingControllerTest, HighPrioDoesntAffectBudget) {
  const size_t kPacketSize = 250;
  uint32_t ssrc = 12346;
  uint16_t sequence_number = 1234;
  int64_t capture_time_ms = 56789;
  auto pacer = std::make_unique<PacingController>(&clock_, &callback_, trials_);
  pacer->SetPacingRates(kTargetRate * kPaceMultiplier, DataRate::Zero());

  // As high prio packets doesn't affect the budget, we should be able to send
  // a high number of them at once.
  const size_t kNumAudioPackets = 25;
  for (size_t i = 0; i < kNumAudioPackets; ++i) {
    SendAndExpectPacket(pacer.get(), RtpPacketMediaType::kAudio, ssrc,
                        sequence_number++, capture_time_ms, kPacketSize);
  }
  pacer->ProcessPackets();
  EXPECT_EQ(pacer->QueueSizePackets(), 0u);
  // Low prio packets does affect the budget.
  const size_t kPacketsToSendPerBurstInterval =
      (kTargetRate * kPaceMultiplier * PacingController::kDefaultBurstInterval)
          .bytes() /
      kPacketSize;
  for (size_t i = 0; i < kPacketsToSendPerBurstInterval; ++i) {
    SendAndExpectPacket(pacer.get(), RtpPacketMediaType::kVideo, ssrc,
                        sequence_number++, clock_.TimeInMilliseconds(),
                        kPacketSize);
  }

  // Send all packets and measure pace time.
  Timestamp start_time = clock_.CurrentTime();
  EXPECT_EQ(pacer->NextSendTime(), clock_.CurrentTime());
  while (pacer->QueueSizePackets() > 0) {
    AdvanceTimeUntil(pacer->NextSendTime());
    pacer->ProcessPackets();
  }

  // Measure pacing time.
  TimeDelta pacing_time = clock_.CurrentTime() - start_time;
  // All packets sent in one burst since audio packets are not accounted for.
  TimeDelta expected_pacing_time = TimeDelta::Zero();
  EXPECT_NEAR(pacing_time.us<double>(), expected_pacing_time.us<double>(),
              PacingController::kMinSleepTime.us<double>());
}

TEST_F(PacingControllerTest, SendsOnlyPaddingWhenCongested) {
  uint32_t ssrc = 202020;
  uint16_t sequence_number = 1000;
  int kPacketSize = 250;
  auto pacer = std::make_unique<PacingController>(&clock_, &callback_, trials_);
  pacer->SetPacingRates(kTargetRate * kPaceMultiplier, DataRate::Zero());

  // Send an initial packet so we have a last send time.
  SendAndExpectPacket(pacer.get(), RtpPacketMediaType::kVideo, ssrc,
                      sequence_number++, clock_.TimeInMilliseconds(),
                      kPacketSize);
  AdvanceTimeUntil(pacer->NextSendTime());
  pacer->ProcessPackets();
  ::testing::Mock::VerifyAndClearExpectations(&callback_);

  // Set congested state, we should not send anything until the 500ms since
  // last send time limit for keep-alives is triggered.
  EXPECT_CALL(callback_, SendPacket).Times(0);
  EXPECT_CALL(callback_, SendPadding).Times(0);
  pacer->SetCongested(true);
  size_t blocked_packets = 0;
  int64_t expected_time_until_padding = 500;
  while (expected_time_until_padding > 5) {
    pacer->EnqueuePacket(BuildPacket(RtpPacketMediaType::kVideo, ssrc,
                                     sequence_number++,
                                     clock_.TimeInMilliseconds(), kPacketSize));
    blocked_packets++;
    clock_.AdvanceTimeMilliseconds(5);
    pacer->ProcessPackets();
    expected_time_until_padding -= 5;
  }

  ::testing::Mock::VerifyAndClearExpectations(&callback_);
  EXPECT_CALL(callback_, SendPadding(1)).WillOnce(Return(1));
  EXPECT_CALL(callback_, SendPacket(_, _, _, _, true)).Times(1);
  clock_.AdvanceTimeMilliseconds(5);
  pacer->ProcessPackets();
  EXPECT_EQ(blocked_packets, pacer->QueueSizePackets());
}

TEST_F(PacingControllerTest, DoesNotAllowOveruseAfterCongestion) {
  uint32_t ssrc = 202020;
  uint16_t seq_num = 1000;
  int size = 1000;
  auto now_ms = [this] { return clock_.TimeInMilliseconds(); };
  auto pacer = std::make_unique<PacingController>(&clock_, &callback_, trials_);
  pacer->SetPacingRates(kTargetRate * kPaceMultiplier, DataRate::Zero());
  pacer->SetSendBurstInterval(TimeDelta::Zero());
  EXPECT_CALL(callback_, SendPadding).Times(0);
  // The pacing rate is low enough that the budget should not allow two packets
  // to be sent in a row.
  pacer->SetPacingRates(DataRate::BitsPerSec(400 * 8 * 1000 / 5),
                        DataRate::Zero());
  // Not yet budget limited or congested, packet is sent.
  pacer->EnqueuePacket(
      BuildPacket(RtpPacketMediaType::kVideo, ssrc, seq_num++, now_ms(), size));
  EXPECT_CALL(callback_, SendPacket).Times(1);
  clock_.AdvanceTimeMilliseconds(5);
  pacer->ProcessPackets();
  // Packet blocked due to congestion.
  pacer->SetCongested(true);
  pacer->EnqueuePacket(
      BuildPacket(RtpPacketMediaType::kVideo, ssrc, seq_num++, now_ms(), size));
  EXPECT_CALL(callback_, SendPacket).Times(0);
  clock_.AdvanceTimeMilliseconds(5);
  pacer->ProcessPackets();
  // Packet blocked due to congestion.
  pacer->EnqueuePacket(
      BuildPacket(RtpPacketMediaType::kVideo, ssrc, seq_num++, now_ms(), size));
  EXPECT_CALL(callback_, SendPacket).Times(0);
  clock_.AdvanceTimeMilliseconds(5);
  pacer->ProcessPackets();
  // Congestion removed and budget has recovered, packet is sent.
  pacer->EnqueuePacket(
      BuildPacket(RtpPacketMediaType::kVideo, ssrc, seq_num++, now_ms(), size));
  EXPECT_CALL(callback_, SendPacket).Times(1);
  clock_.AdvanceTimeMilliseconds(5);
  pacer->SetCongested(false);
  pacer->ProcessPackets();
  // Should be blocked due to budget limitation as congestion has be removed.
  pacer->EnqueuePacket(
      BuildPacket(RtpPacketMediaType::kVideo, ssrc, seq_num++, now_ms(), size));
  EXPECT_CALL(callback_, SendPacket).Times(0);
  clock_.AdvanceTimeMilliseconds(5);
  pacer->ProcessPackets();
}

TEST_F(PacingControllerTest, Pause) {
  uint32_t ssrc_low_priority = 12345;
  uint32_t ssrc = 12346;
  uint32_t ssrc_high_priority = 12347;
  uint16_t sequence_number = 1234;
  auto pacer = std::make_unique<PacingController>(&clock_, &callback_, trials_);
  pacer->SetPacingRates(kTargetRate * kPaceMultiplier, DataRate::Zero());

  EXPECT_TRUE(pacer->OldestPacketEnqueueTime().IsInfinite());

  ConsumeInitialBudget(pacer.get());

  pacer->Pause();

  int64_t capture_time_ms = clock_.TimeInMilliseconds();
  const size_t packets_to_send_per_interval =
      kTargetRate.bps() * kPaceMultiplier / (8 * 250 * 200);
  for (size_t i = 0; i < packets_to_send_per_interval; ++i) {
    pacer->EnqueuePacket(BuildPacket(RtpPacketMediaType::kVideo,
                                     ssrc_low_priority, sequence_number++,
                                     capture_time_ms, 250));
    pacer->EnqueuePacket(BuildPacket(RtpPacketMediaType::kRetransmission, ssrc,
                                     sequence_number++, capture_time_ms, 250));
    pacer->EnqueuePacket(BuildPacket(RtpPacketMediaType::kAudio,
                                     ssrc_high_priority, sequence_number++,
                                     capture_time_ms, 250));
  }
  clock_.AdvanceTimeMilliseconds(10000);
  int64_t second_capture_time_ms = clock_.TimeInMilliseconds();
  for (size_t i = 0; i < packets_to_send_per_interval; ++i) {
    pacer->EnqueuePacket(BuildPacket(RtpPacketMediaType::kVideo,
                                     ssrc_low_priority, sequence_number++,
                                     second_capture_time_ms, 250));
    pacer->EnqueuePacket(BuildPacket(RtpPacketMediaType::kRetransmission, ssrc,
                                     sequence_number++, second_capture_time_ms,
                                     250));
    pacer->EnqueuePacket(BuildPacket(RtpPacketMediaType::kAudio,
                                     ssrc_high_priority, sequence_number++,
                                     second_capture_time_ms, 250));
  }

  // Expect everything to be queued.
  EXPECT_EQ(capture_time_ms, pacer->OldestPacketEnqueueTime().ms());

  // Process triggers keep-alive packet.
  EXPECT_CALL(callback_, SendPadding).WillOnce([](size_t padding) {
    return padding;
  });
  EXPECT_CALL(callback_, SendPacket(_, _, _, _, true)).Times(1);
  pacer->ProcessPackets();

  // Verify no packets sent for the rest of the paused process interval.
  const TimeDelta kProcessInterval = TimeDelta::Millis(5);
  TimeDelta expected_time_until_send = PacingController::kPausedProcessInterval;
  EXPECT_CALL(callback_, SendPadding).Times(0);
  while (expected_time_until_send >= kProcessInterval) {
    pacer->ProcessPackets();
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
  pacer->ProcessPackets();
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
  pacer->Resume();
  while (pacer->QueueSizePackets() > 0) {
    AdvanceTimeUntil(pacer->NextSendTime());
    pacer->ProcessPackets();
  }

  EXPECT_TRUE(pacer->OldestPacketEnqueueTime().IsInfinite());
}

TEST_F(PacingControllerTest, InactiveFromStart) {
  // Recreate the pacer without the inital time forwarding.
  auto pacer = std::make_unique<PacingController>(&clock_, &callback_, trials_);
  pacer->SetProbingEnabled(false);
  pacer->SetPacingRates(kTargetRate * kPaceMultiplier, kTargetRate);

  // No packets sent, there should be no keep-alives sent either.
  EXPECT_CALL(callback_, SendPadding).Times(0);
  EXPECT_CALL(callback_, SendPacket).Times(0);
  pacer->ProcessPackets();

  const Timestamp start_time = clock_.CurrentTime();

  // Determine the margin need so we can advance to the last possible moment
  // that will not cause a process event.
  const TimeDelta time_margin =
      PacingController::kMinSleepTime + TimeDelta::Micros(1);

  EXPECT_EQ(pacer->NextSendTime() - start_time,
            PacingController::kPausedProcessInterval);
  clock_.AdvanceTime(PacingController::kPausedProcessInterval - time_margin);
  pacer->ProcessPackets();
  EXPECT_EQ(pacer->NextSendTime() - start_time,
            PacingController::kPausedProcessInterval);

  clock_.AdvanceTime(time_margin);
  pacer->ProcessPackets();
  EXPECT_EQ(pacer->NextSendTime() - start_time,
            2 * PacingController::kPausedProcessInterval);
}

TEST_F(PacingControllerTest, QueueTimeGrowsOverTime) {
  uint32_t ssrc = 12346;
  uint16_t sequence_number = 1234;
  auto pacer = std::make_unique<PacingController>(&clock_, &callback_, trials_);
  pacer->SetPacingRates(kTargetRate * kPaceMultiplier, DataRate::Zero());
  EXPECT_TRUE(pacer->OldestPacketEnqueueTime().IsInfinite());

  pacer->SetPacingRates(DataRate::BitsPerSec(30000 * kPaceMultiplier),
                        DataRate::Zero());
  SendAndExpectPacket(pacer.get(), RtpPacketMediaType::kVideo, ssrc,
                      sequence_number, clock_.TimeInMilliseconds(), 1200);

  clock_.AdvanceTimeMilliseconds(500);
  EXPECT_EQ(clock_.TimeInMilliseconds() - 500,
            pacer->OldestPacketEnqueueTime().ms());
  pacer->ProcessPackets();
  EXPECT_TRUE(pacer->OldestPacketEnqueueTime().IsInfinite());
}

TEST_F(PacingControllerTest, ProbingWithInsertedPackets) {
  const size_t kPacketSize = 1200;
  const int kInitialBitrateBps = 300000;
  uint32_t ssrc = 12346;
  uint16_t sequence_number = 1234;

  PacingControllerProbing packet_sender;
  auto pacer =
      std::make_unique<PacingController>(&clock_, &packet_sender, trials_);
  std::vector<ProbeClusterConfig> probe_clusters = {
      {.at_time = clock_.CurrentTime(),
       .target_data_rate = kFirstClusterRate,
       .target_duration = TimeDelta::Millis(15),
       .target_probe_count = 5,
       .id = 0},
      {.at_time = clock_.CurrentTime(),
       .target_data_rate = kSecondClusterRate,
       .target_duration = TimeDelta::Millis(15),
       .target_probe_count = 5,
       .id = 1}};
  pacer->CreateProbeClusters(probe_clusters);
  pacer->SetPacingRates(
      DataRate::BitsPerSec(kInitialBitrateBps * kPaceMultiplier),
      DataRate::Zero());

  for (int i = 0; i < 10; ++i) {
    pacer->EnqueuePacket(BuildPacket(RtpPacketMediaType::kVideo, ssrc,
                                     sequence_number++,
                                     clock_.TimeInMilliseconds(), kPacketSize));
  }

  int64_t start = clock_.TimeInMilliseconds();
  while (packet_sender.packets_sent() < 5) {
    AdvanceTimeUntil(pacer->NextSendTime());
    pacer->ProcessPackets();
  }
  int packets_sent = packet_sender.packets_sent();
  // Validate first cluster bitrate. Note that we have to account for number
  // of intervals and hence (packets_sent - 1) on the first cluster.
  EXPECT_NEAR((packets_sent - 1) * kPacketSize * 8000 /
                  (clock_.TimeInMilliseconds() - start),
              kFirstClusterRate.bps(), kProbingErrorMargin.bps());
  // Probing always starts with a small padding packet.
  EXPECT_EQ(1, packet_sender.padding_sent());

  AdvanceTimeUntil(pacer->NextSendTime());
  start = clock_.TimeInMilliseconds();
  while (packet_sender.packets_sent() < 10) {
    AdvanceTimeUntil(pacer->NextSendTime());
    pacer->ProcessPackets();
  }
  packets_sent = packet_sender.packets_sent() - packets_sent;
  // Validate second cluster bitrate.
  EXPECT_NEAR((packets_sent - 1) * kPacketSize * 8000 /
                  (clock_.TimeInMilliseconds() - start),
              kSecondClusterRate.bps(), kProbingErrorMargin.bps());
}

TEST_F(PacingControllerTest, SkipsProbesWhenProcessIntervalTooLarge) {
  const size_t kPacketSize = 1200;
  const int kInitialBitrateBps = 300000;
  const uint32_t ssrc = 12346;
  const int kProbeClusterId = 3;

  uint16_t sequence_number = 1234;

  PacingControllerProbing packet_sender;

  const test::ExplicitKeyValueConfig trials(
      "WebRTC-Bwe-ProbingBehavior/max_probe_delay:2ms/");
  auto pacer =
      std::make_unique<PacingController>(&clock_, &packet_sender, trials);
  pacer->SetPacingRates(
      DataRate::BitsPerSec(kInitialBitrateBps * kPaceMultiplier),
      DataRate::BitsPerSec(kInitialBitrateBps));

  for (int i = 0; i < 10; ++i) {
    pacer->EnqueuePacket(BuildPacket(RtpPacketMediaType::kVideo, ssrc,
                                     sequence_number++,
                                     clock_.TimeInMilliseconds(), kPacketSize));
  }
  while (pacer->QueueSizePackets() > 0) {
    AdvanceTimeUntil(pacer->NextSendTime());
    pacer->ProcessPackets();
  }

  // Probe at a very high rate.
  std::vector<ProbeClusterConfig> probe_clusters = {
      {.at_time = clock_.CurrentTime(),
       .target_data_rate = DataRate::KilobitsPerSec(10000),  // 10 Mbps,
       .target_duration = TimeDelta::Millis(15),
       .target_probe_count = 5,
       .id = kProbeClusterId}};
  pacer->CreateProbeClusters(probe_clusters);

  // We need one packet to start the probe.
  pacer->EnqueuePacket(BuildPacket(RtpPacketMediaType::kVideo, ssrc,
                                   sequence_number++,
                                   clock_.TimeInMilliseconds(), kPacketSize));
  const int packets_sent_before_probe = packet_sender.packets_sent();
  AdvanceTimeUntil(pacer->NextSendTime());
  pacer->ProcessPackets();
  EXPECT_EQ(packet_sender.packets_sent(), packets_sent_before_probe + 1);

  // Figure out how long between probe packets.
  Timestamp start_time = clock_.CurrentTime();
  AdvanceTimeUntil(pacer->NextSendTime());
  TimeDelta time_between_probes = clock_.CurrentTime() - start_time;
  // Advance that distance again + 1ms.
  clock_.AdvanceTime(time_between_probes);

  // Send second probe packet.
  pacer->EnqueuePacket(BuildPacket(RtpPacketMediaType::kVideo, ssrc,
                                   sequence_number++,
                                   clock_.TimeInMilliseconds(), kPacketSize));
  pacer->ProcessPackets();
  EXPECT_EQ(packet_sender.packets_sent(), packets_sent_before_probe + 2);
  PacedPacketInfo last_pacing_info = packet_sender.last_pacing_info();
  EXPECT_EQ(last_pacing_info.probe_cluster_id, kProbeClusterId);

  // We're exactly where we should be for the next probe.
  const Timestamp probe_time = clock_.CurrentTime();
  EXPECT_EQ(pacer->NextSendTime(), clock_.CurrentTime());

  BitrateProberConfig probing_config(&trials);
  EXPECT_GT(probing_config.max_probe_delay.Get(), TimeDelta::Zero());
  // Advance to within max probe delay, should still return same target.
  clock_.AdvanceTime(probing_config.max_probe_delay.Get());
  EXPECT_EQ(pacer->NextSendTime(), probe_time);

  // Too high probe delay, drop it!
  clock_.AdvanceTime(TimeDelta::Micros(1));

  int packets_sent_before_timeout = packet_sender.total_packets_sent();
  // Expected next process time is unchanged, but calling should not
  // generate new packets.
  EXPECT_EQ(pacer->NextSendTime(), probe_time);
  pacer->ProcessPackets();
  EXPECT_EQ(packet_sender.total_packets_sent(), packets_sent_before_timeout);

  // Next packet sent is not part of probe.
  AdvanceTimeUntil(pacer->NextSendTime());
  pacer->ProcessPackets();
  const int expected_probe_id = PacedPacketInfo::kNotAProbe;
  EXPECT_EQ(packet_sender.last_pacing_info().probe_cluster_id,
            expected_probe_id);
}

TEST_F(PacingControllerTest, ProbingWithPaddingSupport) {
  const size_t kPacketSize = 1200;
  const int kInitialBitrateBps = 300000;
  uint32_t ssrc = 12346;
  uint16_t sequence_number = 1234;

  PacingControllerProbing packet_sender;
  auto pacer =
      std::make_unique<PacingController>(&clock_, &packet_sender, trials_);
  std::vector<ProbeClusterConfig> probe_clusters = {
      {.at_time = clock_.CurrentTime(),
       .target_data_rate = kFirstClusterRate,
       .target_duration = TimeDelta::Millis(15),
       .target_probe_count = 5,
       .id = 0}};
  pacer->CreateProbeClusters(probe_clusters);

  pacer->SetPacingRates(
      DataRate::BitsPerSec(kInitialBitrateBps * kPaceMultiplier),
      DataRate::Zero());

  for (int i = 0; i < 3; ++i) {
    pacer->EnqueuePacket(BuildPacket(RtpPacketMediaType::kVideo, ssrc,
                                     sequence_number++,
                                     clock_.TimeInMilliseconds(), kPacketSize));
  }

  int64_t start = clock_.TimeInMilliseconds();
  int process_count = 0;
  while (process_count < 5) {
    AdvanceTimeUntil(pacer->NextSendTime());
    pacer->ProcessPackets();
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

TEST_F(PacingControllerTest, CanProbeWithPaddingBeforeFirstMediaPacket) {
  // const size_t kPacketSize = 1200;
  const int kInitialBitrateBps = 300000;

  PacingControllerProbing packet_sender;
  auto pacer =
      std::make_unique<PacingController>(&clock_, &packet_sender, trials_);
  pacer->SetAllowProbeWithoutMediaPacket(true);
  std::vector<ProbeClusterConfig> probe_clusters = {
      {.at_time = clock_.CurrentTime(),
       .target_data_rate = kFirstClusterRate,
       .target_duration = TimeDelta::Millis(15),
       .target_probe_count = 5,
       .id = 0}};
  pacer->CreateProbeClusters(probe_clusters);

  pacer->SetPacingRates(
      DataRate::BitsPerSec(kInitialBitrateBps * kPaceMultiplier),
      DataRate::Zero());

  Timestamp start = clock_.CurrentTime();
  Timestamp next_process = pacer->NextSendTime();
  while (clock_.CurrentTime() < start + TimeDelta::Millis(100) &&
         next_process.IsFinite()) {
    AdvanceTimeUntil(next_process);
    pacer->ProcessPackets();
    next_process = pacer->NextSendTime();
  }
  EXPECT_GT(packet_sender.padding_packets_sent(), 5);
}

TEST_F(PacingControllerTest, ProbeSentAfterSetAllowProbeWithoutMediaPacket) {
  const int kInitialBitrateBps = 300000;

  PacingControllerProbing packet_sender;
  auto pacer =
      std::make_unique<PacingController>(&clock_, &packet_sender, trials_);
  std::vector<ProbeClusterConfig> probe_clusters = {
      {.at_time = clock_.CurrentTime(),
       .target_data_rate = kFirstClusterRate,
       .target_duration = TimeDelta::Millis(15),
       .target_probe_count = 5,
       .id = 0}};
  pacer->CreateProbeClusters(probe_clusters);

  pacer->SetPacingRates(
      DataRate::BitsPerSec(kInitialBitrateBps * kPaceMultiplier),
      DataRate::Zero());

  pacer->SetAllowProbeWithoutMediaPacket(true);

  Timestamp start = clock_.CurrentTime();
  Timestamp next_process = pacer->NextSendTime();
  while (clock_.CurrentTime() < start + TimeDelta::Millis(100) &&
         next_process.IsFinite()) {
    AdvanceTimeUntil(next_process);
    pacer->ProcessPackets();
    next_process = pacer->NextSendTime();
  }
  EXPECT_GT(packet_sender.padding_packets_sent(), 5);
}

TEST_F(PacingControllerTest, CanNotProbeWithPaddingIfGeneratePaddingFails) {
  // const size_t kPacketSize = 1200;
  const int kInitialBitrateBps = 300000;

  PacingControllerProbing packet_sender;
  packet_sender.SetCanGeneratePadding(false);
  auto pacer =
      std::make_unique<PacingController>(&clock_, &packet_sender, trials_);
  pacer->SetAllowProbeWithoutMediaPacket(true);
  std::vector<ProbeClusterConfig> probe_clusters = {
      {.at_time = clock_.CurrentTime(),
       .target_data_rate = kFirstClusterRate,
       .target_duration = TimeDelta::Millis(15),
       .target_probe_count = 5,
       .id = 0}};
  pacer->CreateProbeClusters(probe_clusters);

  pacer->SetPacingRates(
      DataRate::BitsPerSec(kInitialBitrateBps * kPaceMultiplier),
      DataRate::Zero());

  Timestamp start = clock_.CurrentTime();
  int process_count = 0;
  Timestamp next_process = pacer->NextSendTime();
  while (clock_.CurrentTime() < start + TimeDelta::Millis(100) &&
         next_process.IsFinite()) {
    AdvanceTimeUntil(next_process);
    pacer->ProcessPackets();
    ++process_count;
    next_process = pacer->NextSendTime();
  }

  EXPECT_LT(process_count, 10);
  EXPECT_EQ(packet_sender.padding_packets_sent(), 0);
}

TEST_F(PacingControllerTest, PaddingOveruse) {
  uint32_t ssrc = 12346;
  uint16_t sequence_number = 1234;
  const size_t kPacketSize = 1200;
  auto pacer = std::make_unique<PacingController>(&clock_, &callback_, trials_);
  pacer->SetPacingRates(kTargetRate * kPaceMultiplier, DataRate::Zero());

  // Initially no padding rate.
  pacer->ProcessPackets();
  pacer->SetPacingRates(DataRate::BitsPerSec(60000 * kPaceMultiplier),
                        DataRate::Zero());

  SendAndExpectPacket(pacer.get(), RtpPacketMediaType::kVideo, ssrc,
                      sequence_number++, clock_.TimeInMilliseconds(),
                      kPacketSize);
  pacer->ProcessPackets();

  // Add 30kbit padding. When increasing budget, media budget will increase from
  // negative (overuse) while padding budget will increase from 0.
  clock_.AdvanceTimeMilliseconds(5);
  pacer->SetPacingRates(DataRate::BitsPerSec(60000 * kPaceMultiplier),
                        DataRate::BitsPerSec(30000));

  SendAndExpectPacket(pacer.get(), RtpPacketMediaType::kVideo, ssrc,
                      sequence_number++, clock_.TimeInMilliseconds(),
                      kPacketSize);
  EXPECT_LT(TimeDelta::Millis(5), pacer->ExpectedQueueTime());
  // Don't send padding if queue is non-empty, even if padding budget > 0.
  EXPECT_CALL(callback_, SendPadding).Times(0);
  AdvanceTimeUntil(pacer->NextSendTime());
  pacer->ProcessPackets();
}

TEST_F(PacingControllerTest, ProvidesOnBatchCompleteToPacketSender) {
  MockPacketSender callback;
  auto pacer = std::make_unique<PacingController>(&clock_, &callback, trials_);
  EXPECT_CALL(callback, OnBatchComplete);
  pacer->ProcessPackets();
}

TEST_F(PacingControllerTest, ProbeClusterId) {
  MockPacketSender callback;
  uint32_t ssrc = 12346;
  uint16_t sequence_number = 1234;
  const size_t kPacketSize = 1200;

  auto pacer = std::make_unique<PacingController>(&clock_, &callback, trials_);
  pacer->CreateProbeClusters(std::vector<ProbeClusterConfig>(
      {{.at_time = clock_.CurrentTime(),
        .target_data_rate = kFirstClusterRate,
        .target_duration = TimeDelta::Millis(15),
        .target_probe_count = 5,
        .id = 0},
       {.at_time = clock_.CurrentTime(),
        .target_data_rate = kSecondClusterRate,
        .target_duration = TimeDelta::Millis(15),
        .target_probe_count = 5,
        .id = 1}}));
  pacer->SetPacingRates(kTargetRate * kPaceMultiplier, kTargetRate);
  pacer->SetProbingEnabled(true);
  for (int i = 0; i < 10; ++i) {
    pacer->EnqueuePacket(BuildPacket(RtpPacketMediaType::kVideo, ssrc,
                                     sequence_number++,
                                     clock_.TimeInMilliseconds(), kPacketSize));
  }

  // First probing cluster.
  EXPECT_CALL(callback,
              SendPacket(_, Field(&PacedPacketInfo::probe_cluster_id, 0)))
      .Times(5);

  for (int i = 0; i < 5; ++i) {
    AdvanceTimeUntil(pacer->NextSendTime());
    pacer->ProcessPackets();
  }

  // Second probing cluster.
  EXPECT_CALL(callback,
              SendPacket(_, Field(&PacedPacketInfo::probe_cluster_id, 1)))
      .Times(5);

  for (int i = 0; i < 5; ++i) {
    AdvanceTimeUntil(pacer->NextSendTime());
    pacer->ProcessPackets();
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
  EXPECT_CALL(callback, SendPacket)
      .WillOnce([&](std::unique_ptr<RtpPacketToSend> /* packet */,
                    const PacedPacketInfo& cluster_info) {
        EXPECT_EQ(cluster_info.probe_cluster_id, kNotAProbe);
        non_probe_packet_seen = true;
      });
  while (!non_probe_packet_seen) {
    AdvanceTimeUntil(pacer->NextSendTime());
    pacer->ProcessPackets();
  }
}

TEST_F(PacingControllerTest, OwnedPacketPrioritizedOnType) {
  MockPacketSender callback;
  uint32_t ssrc = 123;

  auto pacer = std::make_unique<PacingController>(&clock_, &callback, trials_);
  pacer->SetPacingRates(kTargetRate * kPaceMultiplier, DataRate::Zero());

  // Insert a packet of each type, from low to high priority. Since priority
  // is weighted higher than insert order, these should come out of the pacer
  // in backwards order with the exception of FEC and Video.

  for (RtpPacketMediaType type :
       {RtpPacketMediaType::kPadding,
        RtpPacketMediaType::kForwardErrorCorrection, RtpPacketMediaType::kVideo,
        RtpPacketMediaType::kRetransmission, RtpPacketMediaType::kAudio}) {
    pacer->EnqueuePacket(BuildPacket(type, ++ssrc, /*sequence_number=*/123,
                                     clock_.TimeInMilliseconds(),
                                     /*size=*/150));
  }

  ::testing::InSequence seq;
  EXPECT_CALL(callback,
              SendPacket(Pointee(Property(&RtpPacketToSend::packet_type,
                                          RtpPacketMediaType::kAudio)),
                         _));
  EXPECT_CALL(callback,
              SendPacket(Pointee(Property(&RtpPacketToSend::packet_type,
                                          RtpPacketMediaType::kRetransmission)),
                         _));

  // FEC and video actually have the same priority, so will come out in
  // insertion order.
  EXPECT_CALL(
      callback,
      SendPacket(Pointee(Property(&RtpPacketToSend::packet_type,
                                  RtpPacketMediaType::kForwardErrorCorrection)),
                 _));
  EXPECT_CALL(callback,
              SendPacket(Pointee(Property(&RtpPacketToSend::packet_type,
                                          RtpPacketMediaType::kVideo)),
                         _));

  EXPECT_CALL(callback,
              SendPacket(Pointee(Property(&RtpPacketToSend::packet_type,
                                          RtpPacketMediaType::kPadding)),
                         _));

  while (pacer->QueueSizePackets() > 0) {
    AdvanceTimeUntil(pacer->NextSendTime());
    pacer->ProcessPackets();
  }
}

TEST_F(PacingControllerTest, SmallFirstProbePacket) {
  MockPacketSender callback;
  auto pacer = std::make_unique<PacingController>(&clock_, &callback, trials_);
  std::vector<ProbeClusterConfig> probe_clusters = {
      {.at_time = clock_.CurrentTime(),
       .target_data_rate = kFirstClusterRate,
       .target_duration = TimeDelta::Millis(15),
       .target_probe_count = 5,
       .id = 0}};
  pacer->CreateProbeClusters(probe_clusters);

  pacer->SetPacingRates(kTargetRate * kPaceMultiplier, DataRate::Zero());

  // Add high prio media.
  pacer->EnqueuePacket(audio_.BuildNextPacket(234));

  // Expect small padding packet to be requested.
  EXPECT_CALL(callback, GeneratePadding(DataSize::Bytes(1)))
      .WillOnce([&](DataSize /* padding_size */) {
        std::vector<std::unique_ptr<RtpPacketToSend>> padding_packets;
        padding_packets.emplace_back(
            BuildPacket(RtpPacketMediaType::kPadding, kAudioSsrc, 1,
                        clock_.TimeInMilliseconds(), 1));
        return padding_packets;
      });

  size_t packets_sent = 0;
  bool media_seen = false;
  EXPECT_CALL(callback, SendPacket)
      .Times(AnyNumber())
      .WillRepeatedly([&](std::unique_ptr<RtpPacketToSend> packet,
                          const PacedPacketInfo& /* cluster_info */) {
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
    pacer->ProcessPackets();
    clock_.AdvanceTimeMilliseconds(5);
  }
}

TEST_F(PacingControllerTest, TaskLate) {
  // Set a low send rate to more easily test timing issues.
  DataRate kSendRate = DataRate::KilobitsPerSec(30);
  auto pacer = std::make_unique<PacingController>(&clock_, &callback_, trials_);
  pacer->SetPacingRates(kSendRate, DataRate::Zero());

  // Add four packets of equal size and priority.
  pacer->EnqueuePacket(video_.BuildNextPacket(1000));
  pacer->EnqueuePacket(video_.BuildNextPacket(1000));
  pacer->EnqueuePacket(video_.BuildNextPacket(1000));
  pacer->EnqueuePacket(video_.BuildNextPacket(1000));

  // Process packets, only first should be sent.
  EXPECT_CALL(callback_, SendPacket).Times(1);
  pacer->ProcessPackets();

  Timestamp next_send_time = pacer->NextSendTime();
  // Determine time between packets (ca 62ms)
  const TimeDelta time_between_packets = next_send_time - clock_.CurrentTime();

  // Simulate a late process call, executed just before we allow sending the
  // fourth packet.
  const TimeDelta kOffset = TimeDelta::Millis(1);
  clock_.AdvanceTime((time_between_packets * 3) - kOffset);

  EXPECT_CALL(callback_, SendPacket).Times(2);
  pacer->ProcessPackets();

  // Check that next scheduled send time is in ca 1ms.
  next_send_time = pacer->NextSendTime();
  const TimeDelta time_left = next_send_time - clock_.CurrentTime();
  EXPECT_EQ(time_left.RoundTo(TimeDelta::Millis(1)), kOffset);

  clock_.AdvanceTime(time_left);
  EXPECT_CALL(callback_, SendPacket);
  pacer->ProcessPackets();
}

TEST_F(PacingControllerTest, NoProbingWhilePaused) {
  uint32_t ssrc = 12345;
  uint16_t sequence_number = 1234;
  auto pacer = std::make_unique<PacingController>(&clock_, &callback_, trials_);
  pacer->SetProbingEnabled(true);
  pacer->SetPacingRates(kTargetRate * kPaceMultiplier, DataRate::Zero());
  pacer->CreateProbeClusters(std::vector<ProbeClusterConfig>(
      {{.at_time = clock_.CurrentTime(),
        .target_data_rate = kFirstClusterRate,
        .target_duration = TimeDelta::Millis(15),
        .target_probe_count = 5,
        .id = 0},
       {.at_time = clock_.CurrentTime(),
        .target_data_rate = kSecondClusterRate,
        .target_duration = TimeDelta::Millis(15),
        .target_probe_count = 5,
        .id = 1}}));

  // Send at least one packet so probing can initate.
  SendAndExpectPacket(pacer.get(), RtpPacketMediaType::kVideo, ssrc,
                      sequence_number, clock_.TimeInMilliseconds(), 250);
  while (pacer->QueueSizePackets() > 0) {
    AdvanceTimeUntil(pacer->NextSendTime());
    pacer->ProcessPackets();
  }

  // Trigger probing.
  std::vector<ProbeClusterConfig> probe_clusters = {
      {.at_time = clock_.CurrentTime(),
       .target_data_rate = DataRate::KilobitsPerSec(10000),  // 10 Mbps.
       .target_duration = TimeDelta::Millis(15),
       .target_probe_count = 5,
       .id = 3}};
  pacer->CreateProbeClusters(probe_clusters);

  // Time to next send time should be small.
  EXPECT_LT(pacer->NextSendTime() - clock_.CurrentTime(),
            PacingController::kPausedProcessInterval);

  // Pause pacer, time to next send time should now be the pause process
  // interval.
  pacer->Pause();

  EXPECT_EQ(pacer->NextSendTime() - clock_.CurrentTime(),
            PacingController::kPausedProcessInterval);
}

TEST_F(PacingControllerTest, AudioNotPacedEvenWhenAccountedFor) {
  const uint32_t kSsrc = 12345;
  uint16_t sequence_number = 1234;
  const size_t kPacketSize = 123;
  auto pacer = std::make_unique<PacingController>(&clock_, &callback_, trials_);

  // Account for audio - so that audio packets can cause pushback on other
  // types such as video. Audio packet should still be immediated passed
  // through though ("WebRTC-Pacer-BlockAudio" needs to be enabled in order
  // to pace audio packets).
  pacer->SetAccountForAudioPackets(true);

  // Set pacing rate to 1 packet/s, no padding.
  pacer->SetPacingRates(DataSize::Bytes(kPacketSize) / TimeDelta::Seconds(1),
                        DataRate::Zero());

  // Add and send an audio packet.
  SendAndExpectPacket(pacer.get(), RtpPacketMediaType::kAudio, kSsrc,
                      sequence_number++, clock_.TimeInMilliseconds(),
                      kPacketSize);
  pacer->ProcessPackets();

  // Advance time, add another audio packet and process. It should be sent
  // immediately.
  clock_.AdvanceTimeMilliseconds(5);
  SendAndExpectPacket(pacer.get(), RtpPacketMediaType::kAudio, kSsrc,
                      sequence_number++, clock_.TimeInMilliseconds(),
                      kPacketSize);
  pacer->ProcessPackets();
}

TEST_F(PacingControllerTest,
       PaddingResumesAfterSaturationEvenWithConcurrentAudio) {
  const uint32_t kSsrc = 12345;
  const DataRate kPacingDataRate = DataRate::KilobitsPerSec(125);
  const DataRate kPaddingDataRate = DataRate::KilobitsPerSec(100);
  const TimeDelta kMaxBufferInTime = TimeDelta::Millis(500);
  const DataSize kPacketSize = DataSize::Bytes(130);
  const TimeDelta kAudioPacketInterval = TimeDelta::Millis(20);

  // In this test, we fist send a burst of video in order to saturate the
  // padding debt level.
  // We then proceed to send audio at a bitrate that is slightly lower than
  // the padding rate, meaning there will be a period with audio but no
  // padding sent while the debt is draining, then audio and padding will
  // be interlieved.

  // Verify both with and without accounting for audio.
  for (bool account_for_audio : {false, true}) {
    uint16_t sequence_number = 1234;
    MockPacketSender callback;
    EXPECT_CALL(callback, SendPacket).Times(AnyNumber());
    auto pacer =
        std::make_unique<PacingController>(&clock_, &callback, trials_);
    pacer->SetAccountForAudioPackets(account_for_audio);

    // First, saturate the padding budget.
    pacer->SetPacingRates(kPacingDataRate, kPaddingDataRate);

    const TimeDelta kPaddingSaturationTime =
        kMaxBufferInTime * kPaddingDataRate /
        (kPacingDataRate - kPaddingDataRate);
    const DataSize kVideoToSend = kPaddingSaturationTime * kPacingDataRate;
    const DataSize kVideoPacketSize = DataSize::Bytes(1200);
    DataSize video_sent = DataSize::Zero();
    while (video_sent < kVideoToSend) {
      pacer->EnqueuePacket(
          BuildPacket(RtpPacketMediaType::kVideo, kSsrc, sequence_number++,
                      clock_.TimeInMilliseconds(), kVideoPacketSize.bytes()));
      video_sent += kVideoPacketSize;
    }
    while (pacer->QueueSizePackets() > 0) {
      AdvanceTimeUntil(pacer->NextSendTime());
      pacer->ProcessPackets();
    }

    // Add a stream of audio packets at a rate slightly lower than the padding
    // rate, once the padding debt is paid off we expect padding to be
    // generated.
    pacer->SetPacingRates(kPacingDataRate, kPaddingDataRate);
    bool padding_seen = false;
    EXPECT_CALL(callback, GeneratePadding).WillOnce([&](DataSize padding_size) {
      padding_seen = true;
      std::vector<std::unique_ptr<RtpPacketToSend>> padding_packets;
      padding_packets.emplace_back(
          BuildPacket(RtpPacketMediaType::kPadding, kSsrc, sequence_number++,
                      clock_.TimeInMilliseconds(), padding_size.bytes()));
      return padding_packets;
    });

    Timestamp start_time = clock_.CurrentTime();
    Timestamp last_audio_time = start_time;
    while (!padding_seen) {
      Timestamp now = clock_.CurrentTime();
      Timestamp next_send_time = pacer->NextSendTime();
      TimeDelta sleep_time =
          std::min(next_send_time, last_audio_time + kAudioPacketInterval) -
          now;
      clock_.AdvanceTime(sleep_time);
      while (clock_.CurrentTime() >= last_audio_time + kAudioPacketInterval) {
        pacer->EnqueuePacket(
            BuildPacket(RtpPacketMediaType::kAudio, kSsrc, sequence_number++,
                        clock_.TimeInMilliseconds(), kPacketSize.bytes()));
        last_audio_time += kAudioPacketInterval;
      }
      pacer->ProcessPackets();
    }

    // Verify how long it took to drain the padding debt. Allow 2% error margin.
    const DataRate kAudioDataRate = kPacketSize / kAudioPacketInterval;
    const TimeDelta expected_drain_time =
        account_for_audio ? (kMaxBufferInTime * kPaddingDataRate /
                             (kPaddingDataRate - kAudioDataRate))
                          : kMaxBufferInTime;
    const TimeDelta actual_drain_time = clock_.CurrentTime() - start_time;
    EXPECT_NEAR(actual_drain_time.ms(), expected_drain_time.ms(),
                expected_drain_time.ms() * 0.02)
        << " where account_for_audio = "
        << (account_for_audio ? "true" : "false");
  }
}

TEST_F(PacingControllerTest, AccountsForAudioEnqueueTime) {
  const uint32_t kSsrc = 12345;
  const DataRate kPacingDataRate = DataRate::KilobitsPerSec(125);
  const DataRate kPaddingDataRate = DataRate::Zero();
  const DataSize kPacketSize = DataSize::Bytes(130);
  const TimeDelta kPacketPacingTime = kPacketSize / kPacingDataRate;
  uint32_t sequnce_number = 1;
  auto pacer = std::make_unique<PacingController>(&clock_, &callback_, trials_);
  // Audio not paced, but still accounted for in budget.
  pacer->SetAccountForAudioPackets(true);
  pacer->SetPacingRates(kPacingDataRate, kPaddingDataRate);
  pacer->SetSendBurstInterval(TimeDelta::Zero());

  // Enqueue two audio packets, advance clock to where one packet
  // should have drained the buffer already, has they been sent
  // immediately.
  SendAndExpectPacket(pacer.get(), RtpPacketMediaType::kAudio, kSsrc,
                      sequnce_number++, clock_.TimeInMilliseconds(),
                      kPacketSize.bytes());
  SendAndExpectPacket(pacer.get(), RtpPacketMediaType::kAudio, kSsrc,
                      sequnce_number++, clock_.TimeInMilliseconds(),
                      kPacketSize.bytes());
  clock_.AdvanceTime(kPacketPacingTime);
  // Now process and make sure both packets were sent.
  pacer->ProcessPackets();
  ::testing::Mock::VerifyAndClearExpectations(&callback_);

  // Add a video packet. I can't be sent until debt from audio
  // packets have been drained.
  pacer->EnqueuePacket(
      BuildPacket(RtpPacketMediaType::kVideo, kSsrc + 1, sequnce_number++,
                  clock_.TimeInMilliseconds(), kPacketSize.bytes()));
  EXPECT_EQ(pacer->NextSendTime() - clock_.CurrentTime(), kPacketPacingTime);
}

TEST_F(PacingControllerTest, NextSendTimeAccountsForPadding) {
  const uint32_t kSsrc = 12345;
  const DataRate kPacingDataRate = DataRate::KilobitsPerSec(125);
  const DataSize kPacketSize = DataSize::Bytes(130);
  const TimeDelta kPacketPacingTime = kPacketSize / kPacingDataRate;
  uint32_t sequnce_number = 1;
  auto pacer = std::make_unique<PacingController>(&clock_, &callback_, trials_);

  // Start with no padding.
  pacer->SetPacingRates(kPacingDataRate, DataRate::Zero());

  // Send a single packet.
  SendAndExpectPacket(pacer.get(), RtpPacketMediaType::kVideo, kSsrc,
                      sequnce_number++, clock_.TimeInMilliseconds(),
                      kPacketSize.bytes());
  pacer->ProcessPackets();
  ::testing::Mock::VerifyAndClearExpectations(&callback_);

  // With current conditions, no need to wake until next keep-alive.
  EXPECT_EQ(pacer->NextSendTime() - clock_.CurrentTime(),
            PacingController::kPausedProcessInterval);

  // Enqueue a new packet, that can be sent immediately due to default burst
  // rate is 40ms.
  SendAndExpectPacket(pacer.get(), RtpPacketMediaType::kVideo, kSsrc,
                      sequnce_number++, clock_.TimeInMilliseconds(),
                      kPacketSize.bytes());
  EXPECT_EQ(pacer->NextSendTime() - clock_.CurrentTime(), TimeDelta::Zero());
  pacer->ProcessPackets();
  ::testing::Mock::VerifyAndClearExpectations(&callback_);

  // With current conditions, again no need to wake until next keep-alive.
  EXPECT_EQ(pacer->NextSendTime() - clock_.CurrentTime(),
            PacingController::kPausedProcessInterval);

  // Set a non-zero padding rate. Padding also can't be sent until
  // previous debt has cleared. Since padding was disabled before, there
  // currently is no padding debt.
  pacer->SetPacingRates(kPacingDataRate, kPacingDataRate / 2);
  EXPECT_EQ(pacer->QueueSizePackets(), 0u);
  EXPECT_LT(pacer->NextSendTime() - clock_.CurrentTime(),
            PacingController::kDefaultBurstInterval);

  // Advance time, expect padding.
  EXPECT_CALL(callback_, SendPadding).WillOnce(Return(kPacketSize.bytes()));
  clock_.AdvanceTime(pacer->NextSendTime() - clock_.CurrentTime());
  pacer->ProcessPackets();
  ::testing::Mock::VerifyAndClearExpectations(&callback_);

  // Since padding rate is half of pacing rate, next time we can send
  // padding is double the packet pacing time.
  EXPECT_EQ(pacer->NextSendTime() - clock_.CurrentTime(),
            kPacketPacingTime * 2);

  // Insert a packet to be sent, this take precedence again.
  pacer->EnqueuePacket(
      BuildPacket(RtpPacketMediaType::kVideo, kSsrc, sequnce_number++,
                  clock_.TimeInMilliseconds(), kPacketSize.bytes()));
  EXPECT_EQ(pacer->NextSendTime(), clock_.CurrentTime());
}

TEST_F(PacingControllerTest, PaddingTargetAccountsForPaddingRate) {
  // Target size for a padding packet is 5ms * padding rate.
  const TimeDelta kPaddingTarget = TimeDelta::Millis(5);
  srand(0);
  // Need to initialize PacingController after we initialize clock.
  auto pacer = std::make_unique<PacingController>(&clock_, &callback_, trials_);

  const uint32_t kSsrc = 12345;
  const DataRate kPacingDataRate = DataRate::KilobitsPerSec(125);
  const DataSize kPacketSize = DataSize::Bytes(130);

  uint32_t sequnce_number = 1;

  // Start with pacing and padding rate equal.
  pacer->SetPacingRates(kPacingDataRate, kPacingDataRate);

  // Send a single packet.
  SendAndExpectPacket(pacer.get(), RtpPacketMediaType::kVideo, kSsrc,
                      sequnce_number++, clock_.TimeInMilliseconds(),
                      kPacketSize.bytes());
  AdvanceTimeUntil(pacer->NextSendTime());
  pacer->ProcessPackets();
  ::testing::Mock::VerifyAndClearExpectations(&callback_);

  size_t expected_padding_target_bytes =
      (kPaddingTarget * kPacingDataRate).bytes();
  EXPECT_CALL(callback_, SendPadding(expected_padding_target_bytes))
      .WillOnce(Return(expected_padding_target_bytes));
  AdvanceTimeUntil(pacer->NextSendTime());
  pacer->ProcessPackets();

  // Half the padding rate - expect half the padding target.
  pacer->SetPacingRates(kPacingDataRate, kPacingDataRate / 2);
  EXPECT_CALL(callback_, SendPadding(expected_padding_target_bytes / 2))
      .WillOnce(Return(expected_padding_target_bytes / 2));
  AdvanceTimeUntil(pacer->NextSendTime());
  pacer->ProcessPackets();
}

TEST_F(PacingControllerTest, SendsFecPackets) {
  const uint32_t kSsrc = 12345;
  const uint32_t kFlexSsrc = 54321;
  uint16_t sequence_number = 1234;
  uint16_t flexfec_sequence_number = 4321;
  const size_t kPacketSize = 123;
  auto pacer = std::make_unique<PacingController>(&clock_, &callback_, trials_);

  // Set pacing rate to 1000 packet/s, no padding.
  pacer->SetPacingRates(
      DataSize::Bytes(1000 * kPacketSize) / TimeDelta::Seconds(1),
      DataRate::Zero());

  int64_t now = clock_.TimeInMilliseconds();
  pacer->EnqueuePacket(BuildPacket(RtpPacketMediaType::kVideo, kSsrc,
                                   sequence_number, now, kPacketSize));
  EXPECT_CALL(callback_, SendPacket(kSsrc, sequence_number, now, false, false));
  EXPECT_CALL(callback_, FetchFec).WillOnce([&]() {
    EXPECT_CALL(callback_, SendPacket(kFlexSsrc, flexfec_sequence_number, now,
                                      false, false));
    EXPECT_CALL(callback_, FetchFec);
    std::vector<std::unique_ptr<RtpPacketToSend>> fec_packets;
    fec_packets.push_back(
        BuildPacket(RtpPacketMediaType::kForwardErrorCorrection, kFlexSsrc,
                    flexfec_sequence_number, now, kPacketSize));
    return fec_packets;
  });
  AdvanceTimeUntil(pacer->NextSendTime());
  pacer->ProcessPackets();
  AdvanceTimeUntil(pacer->NextSendTime());
  pacer->ProcessPackets();
}

TEST_F(PacingControllerTest, GapInPacingDoesntAccumulateBudget) {
  const uint32_t kSsrc = 12345;
  uint16_t sequence_number = 1234;
  const DataSize kPackeSize = DataSize::Bytes(1000);
  const TimeDelta kPacketSendTime = TimeDelta::Millis(25);
  auto pacer = std::make_unique<PacingController>(&clock_, &callback_, trials_);

  pacer->SetPacingRates(kPackeSize / kPacketSendTime,
                        /*padding_rate=*/DataRate::Zero());

  // Send an initial packet.
  SendAndExpectPacket(pacer.get(), RtpPacketMediaType::kVideo, kSsrc,
                      sequence_number++, clock_.TimeInMilliseconds(),
                      kPackeSize.bytes());
  pacer->ProcessPackets();
  ::testing::Mock::VerifyAndClearExpectations(&callback_);

  // Advance time kPacketSendTime past where the media debt should be 0.
  clock_.AdvanceTime(2 * kPacketSendTime);

  // Enqueue three new packets. Expect only two to be sent one ProcessPackets()
  // since the default burst interval is 40ms.
  SendAndExpectPacket(pacer.get(), RtpPacketMediaType::kVideo, kSsrc,
                      sequence_number++, clock_.TimeInMilliseconds(),
                      kPackeSize.bytes());
  SendAndExpectPacket(pacer.get(), RtpPacketMediaType::kVideo, kSsrc,
                      sequence_number++, clock_.TimeInMilliseconds(),
                      kPackeSize.bytes());
  EXPECT_CALL(callback_, SendPacket(kSsrc, sequence_number + 1, _, _, _))
      .Times(0);
  pacer->EnqueuePacket(
      BuildPacket(RtpPacketMediaType::kVideo, kSsrc, sequence_number + 1,
                  clock_.TimeInMilliseconds(), kPackeSize.bytes()));

  pacer->ProcessPackets();
}

TEST_F(PacingControllerTest, HandlesSubMicrosecondSendIntervals) {
  static constexpr DataSize kPacketSize = DataSize::Bytes(1);
  static constexpr TimeDelta kPacketSendTime = TimeDelta::Micros(1);
  auto pacer = std::make_unique<PacingController>(&clock_, &callback_, trials_);
  pacer->SetSendBurstInterval(TimeDelta::Zero());

  // Set pacing rate such that a packet is sent in 0.5us.
  pacer->SetPacingRates(/*pacing_rate=*/2 * kPacketSize / kPacketSendTime,
                        /*padding_rate=*/DataRate::Zero());

  // Enqueue three packets, the first two should be sent immediately - the third
  // should cause a non-zero delta to the next process time.
  EXPECT_CALL(callback_, SendPacket).Times(2);
  for (int i = 0; i < 3; ++i) {
    pacer->EnqueuePacket(BuildPacket(
        RtpPacketMediaType::kVideo, /*ssrc=*/12345, /*sequence_number=*/i,
        clock_.TimeInMilliseconds(), kPacketSize.bytes()));
  }
  pacer->ProcessPackets();

  EXPECT_GT(pacer->NextSendTime(), clock_.CurrentTime());
}

TEST_F(PacingControllerTest, HandlesSubMicrosecondPaddingInterval) {
  static constexpr DataSize kPacketSize = DataSize::Bytes(1);
  static constexpr TimeDelta kPacketSendTime = TimeDelta::Micros(1);
  auto pacer = std::make_unique<PacingController>(&clock_, &callback_, trials_);

  // Set both pacing and padding rates to 1 byte per 0.5us.
  pacer->SetPacingRates(/*pacing_rate=*/2 * kPacketSize / kPacketSendTime,
                        /*padding_rate=*/2 * kPacketSize / kPacketSendTime);

  // Enqueue and send one packet.
  EXPECT_CALL(callback_, SendPacket);
  pacer->EnqueuePacket(BuildPacket(
      RtpPacketMediaType::kVideo, /*ssrc=*/12345, /*sequence_number=*/1234,
      clock_.TimeInMilliseconds(), kPacketSize.bytes()));
  pacer->ProcessPackets();

  // The padding debt is now 1 byte, and the pacing time for that is lower than
  // the precision of a TimeStamp tick. Make sure the pacer still indicates a
  // non-zero sleep time is needed until the next process.
  EXPECT_GT(pacer->NextSendTime(), clock_.CurrentTime());
}

TEST_F(PacingControllerTest, SendsPacketsInBurstImmediately) {
  constexpr TimeDelta kMaxDelay = TimeDelta::Millis(20);
  PacingController pacer(&clock_, &callback_, trials_);
  pacer.SetSendBurstInterval(kMaxDelay);
  pacer.SetPacingRates(DataRate::BytesPerSec(10000), DataRate::Zero());

  // Max allowed send burst size is 100000*20/1000) = 200byte
  pacer.EnqueuePacket(video_.BuildNextPacket(100));
  pacer.EnqueuePacket(video_.BuildNextPacket(100));
  pacer.EnqueuePacket(video_.BuildNextPacket(100));
  pacer.ProcessPackets();
  EXPECT_EQ(pacer.QueueSizePackets(), 1u);
  EXPECT_EQ(pacer.NextSendTime(), clock_.CurrentTime() + kMaxDelay);

  AdvanceTimeUntil(pacer.NextSendTime());
  pacer.ProcessPackets();
  EXPECT_EQ(pacer.QueueSizePackets(), 0u);
}

TEST_F(PacingControllerTest, SendsPacketsInBurstEvenIfNotEnqueedAtSameTime) {
  constexpr TimeDelta kMaxDelay = TimeDelta::Millis(20);
  PacingController pacer(&clock_, &callback_, trials_);
  pacer.SetSendBurstInterval(kMaxDelay);
  pacer.SetPacingRates(DataRate::BytesPerSec(10000), DataRate::Zero());
  pacer.EnqueuePacket(video_.BuildNextPacket(200));
  EXPECT_EQ(pacer.NextSendTime(), clock_.CurrentTime());
  pacer.ProcessPackets();
  clock_.AdvanceTime(TimeDelta::Millis(1));
  pacer.EnqueuePacket(video_.BuildNextPacket(200));
  EXPECT_EQ(pacer.NextSendTime(), clock_.CurrentTime());
  pacer.ProcessPackets();
  EXPECT_EQ(pacer.QueueSizePackets(), 0u);
}

TEST_F(PacingControllerTest, RespectsTargetRateWhenSendingPacketsInBursts) {
  PacingController pacer(&clock_, &callback_, trials_);
  pacer.SetSendBurstInterval(TimeDelta::Millis(20));
  pacer.SetAccountForAudioPackets(true);
  pacer.SetPacingRates(DataRate::KilobitsPerSec(1000), DataRate::Zero());
  Timestamp start_time = clock_.CurrentTime();
  // Inject 100 packets, with size 1000bytes over 100ms.
  // Expect only 1Mbps / (8*1000) / 10 =  12 packets to be sent.
  // Packets are sent in burst. Each burst is then 3 packets * 1000bytes at
  // 1Mbits = 24ms long. Thus, expect 4 bursts.
  EXPECT_CALL(callback_, SendPacket).Times(12);
  int number_of_bursts = 0;
  while (clock_.CurrentTime() < start_time + TimeDelta::Millis(100)) {
    pacer.EnqueuePacket(video_.BuildNextPacket(1000));
    pacer.EnqueuePacket(video_.BuildNextPacket(1000));
    pacer.EnqueuePacket(video_.BuildNextPacket(1000));
    pacer.EnqueuePacket(video_.BuildNextPacket(1000));
    pacer.EnqueuePacket(video_.BuildNextPacket(1000));
    if (pacer.NextSendTime() <= clock_.CurrentTime()) {
      pacer.ProcessPackets();
      ++number_of_bursts;
    }
    clock_.AdvanceTime(TimeDelta::Millis(5));
  }
  EXPECT_EQ(pacer.QueueSizePackets(), 88u);
  EXPECT_EQ(number_of_bursts, 4);
}

TEST_F(PacingControllerTest,
       MaxBurstSizeLimitedAtHighPacingRateWhenSendingPacketsInBursts) {
  NiceMock<MockPacketSender> callback;
  PacingController pacer(&clock_, &callback, trials_);
  pacer.SetSendBurstInterval(TimeDelta::Millis(100));
  pacer.SetPacingRates(DataRate::KilobitsPerSec(10'000), DataRate::Zero());

  size_t sent_size_in_burst = 0;
  EXPECT_CALL(callback, SendPacket)
      .WillRepeatedly([&](std::unique_ptr<RtpPacketToSend> packet,
                          const PacedPacketInfo& /* cluster_info */) {
        sent_size_in_burst += packet->size();
      });

  // Enqueue 200 packets from a 200Kb encoded frame.
  for (int i = 0; i < 200; ++i) {
    pacer.EnqueuePacket(video_.BuildNextPacket(1000));
  }

  while (pacer.QueueSizePackets() > 70) {
    pacer.ProcessPackets();
    EXPECT_NEAR(sent_size_in_burst, PacingController::kMaxBurstSize.bytes(),
                1000);
    sent_size_in_burst = 0;
    TimeDelta time_to_next = pacer.NextSendTime() - clock_.CurrentTime();
    EXPECT_NEAR(time_to_next.ms(), 50, 2);
    clock_.AdvanceTime(time_to_next);
  }
}

TEST_F(PacingControllerTest, RespectsQueueTimeLimit) {
  static constexpr DataSize kPacketSize = DataSize::Bytes(100);
  static constexpr DataRate kNominalPacingRate = DataRate::KilobitsPerSec(200);
  static constexpr TimeDelta kPacketPacingTime =
      kPacketSize / kNominalPacingRate;
  static constexpr TimeDelta kQueueTimeLimit = TimeDelta::Millis(1000);

  PacingController pacer(&clock_, &callback_, trials_);
  pacer.SetPacingRates(kNominalPacingRate, /*padding_rate=*/DataRate::Zero());
  pacer.SetQueueTimeLimit(kQueueTimeLimit);

  // Fill pacer up to queue time limit.
  static constexpr int kNumPackets = kQueueTimeLimit / kPacketPacingTime;
  for (int i = 0; i < kNumPackets; ++i) {
    pacer.EnqueuePacket(video_.BuildNextPacket(kPacketSize.bytes()));
  }
  EXPECT_EQ(pacer.ExpectedQueueTime(), kQueueTimeLimit);
  EXPECT_EQ(pacer.pacing_rate(), kNominalPacingRate);

  // Double the amount of packets in the queue, the queue time limit should
  // effectively double the pacing rate in response.
  for (int i = 0; i < kNumPackets; ++i) {
    pacer.EnqueuePacket(video_.BuildNextPacket(kPacketSize.bytes()));
  }
  EXPECT_EQ(pacer.ExpectedQueueTime(), kQueueTimeLimit);
  EXPECT_EQ(pacer.pacing_rate(), 2 * kNominalPacingRate);

  // Send all the packets, should take as long as the queue time limit.
  Timestamp start_time = clock_.CurrentTime();
  while (pacer.QueueSizePackets() > 0) {
    AdvanceTimeUntil(pacer.NextSendTime());
    pacer.ProcessPackets();
  }
  EXPECT_EQ(clock_.CurrentTime() - start_time, kQueueTimeLimit);

  // We're back in a normal state - pacing rate should be back to previous
  // levels.
  EXPECT_EQ(pacer.pacing_rate(), kNominalPacingRate);
}

TEST_F(PacingControllerTest, BudgetDoesNotAffectRetransmissionInsTrial) {
  const DataSize kPacketSize = DataSize::Bytes(1000);

  EXPECT_CALL(callback_, SendPadding).Times(0);
  const test::ExplicitKeyValueConfig trials(
      "WebRTC-Pacer-FastRetransmissions/Enabled/");
  PacingController pacer(&clock_, &callback_, trials);
  pacer.SetPacingRates(kTargetRate, /*padding_rate=*/DataRate::Zero());

  // Send a video packet so that we have a bit debt.
  pacer.EnqueuePacket(BuildPacket(RtpPacketMediaType::kVideo, kVideoSsrc,
                                  /*sequence_number=*/1,
                                  /*capture_time=*/1, kPacketSize.bytes()));
  EXPECT_CALL(callback_, SendPacket);
  pacer.ProcessPackets();
  EXPECT_GT(pacer.NextSendTime(), clock_.CurrentTime());

  // A retransmission packet should still be immediately processed.
  EXPECT_CALL(callback_, SendPacket);
  pacer.EnqueuePacket(BuildPacket(RtpPacketMediaType::kRetransmission,
                                  kVideoSsrc,
                                  /*sequence_number=*/1,
                                  /*capture_time=*/1, kPacketSize.bytes()));
  pacer.ProcessPackets();
}

TEST_F(PacingControllerTest, AbortsAfterReachingCircuitBreakLimit) {
  const DataSize kPacketSize = DataSize::Bytes(1000);

  EXPECT_CALL(callback_, SendPadding).Times(0);
  PacingController pacer(&clock_, &callback_, trials_);
  pacer.SetPacingRates(kTargetRate, /*padding_rate=*/DataRate::Zero());

  // Set the circuit breaker to abort after one iteration of the main
  // sending loop.
  pacer.SetCircuitBreakerThreshold(1);
  EXPECT_CALL(callback_, SendPacket).Times(1);

  // Send two packets.
  pacer.EnqueuePacket(BuildPacket(RtpPacketMediaType::kVideo, kVideoSsrc,
                                  /*sequence_number=*/1,
                                  /*capture_time=*/1, kPacketSize.bytes()));
  pacer.EnqueuePacket(BuildPacket(RtpPacketMediaType::kVideo, kVideoSsrc,
                                  /*sequence_number=*/2,
                                  /*capture_time=*/2, kPacketSize.bytes()));

  // Advance time to way past where both should be eligible for sending.
  clock_.AdvanceTime(TimeDelta::Seconds(1));

  pacer.ProcessPackets();
}

TEST_F(PacingControllerTest, DoesNotPadIfProcessThreadIsBorked) {
  PacingControllerPadding callback;
  PacingController pacer(&clock_, &callback, trials_);

  // Set both pacing and padding rate to be non-zero.
  pacer.SetPacingRates(kTargetRate, /*padding_rate=*/kTargetRate);

  // Add one packet to the queue, but do not send it yet.
  pacer.EnqueuePacket(BuildPacket(RtpPacketMediaType::kVideo, kVideoSsrc,
                                  /*sequence_number=*/1,
                                  /*capture_time=*/1,
                                  /*size=*/1000));

  // Advance time to waaay after the packet should have been sent.
  clock_.AdvanceTime(TimeDelta::Seconds(42));

  // `ProcessPackets()` should send the delayed packet, followed by a small
  // amount of missed padding.
  pacer.ProcessPackets();

  // The max padding window is the max replay duration + the target padding
  // duration.
  const DataSize kMaxPadding = (PacingController::kMaxPaddingReplayDuration +
                                PacingController::kTargetPaddingDuration) *
                               kTargetRate;

  EXPECT_LE(callback.padding_sent(), kMaxPadding.bytes<size_t>());
}

TEST_F(PacingControllerTest, FlushesPacketsOnKeyFrames) {
  const uint32_t kSsrc = 12345;
  const uint32_t kRtxSsrc = 12346;

  const test::ExplicitKeyValueConfig trials(
      "WebRTC-Pacer-KeyframeFlushing/Enabled/");
  auto pacer = std::make_unique<PacingController>(&clock_, &callback_, trials);
  EXPECT_CALL(callback_, GetRtxSsrcForMedia(kSsrc))
      .WillRepeatedly(Return(kRtxSsrc));
  pacer->SetPacingRates(kTargetRate, DataRate::Zero());

  // Enqueue a video packet and a retransmission of that video stream.
  pacer->EnqueuePacket(BuildPacket(RtpPacketMediaType::kVideo, kSsrc,
                                   /*sequence_number=*/1, /*capture_time=*/1,
                                   /*size_bytes=*/100));
  pacer->EnqueuePacket(BuildPacket(RtpPacketMediaType::kRetransmission,
                                   kRtxSsrc,
                                   /*sequence_number=*/10, /*capture_time=*/1,
                                   /*size_bytes=*/100));
  EXPECT_EQ(pacer->QueueSizePackets(), 2u);

  // Enqueue the first packet of a keyframe for said stream.
  auto packet = BuildPacket(RtpPacketMediaType::kVideo, kSsrc,
                            /*sequence_number=*/2, /*capture_time=*/2,
                            /*size_bytes=*/1000);
  packet->set_is_key_frame(true);
  packet->set_first_packet_of_frame(true);
  pacer->EnqueuePacket(std::move(packet));

  // Only they new keyframe packet should be left in the queue.
  EXPECT_EQ(pacer->QueueSizePackets(), 1u);

  EXPECT_CALL(callback_, SendPacket(kSsrc, /*sequence_number=*/2,
                                    /*timestamp=*/2, /*is_retrnamission=*/false,
                                    /*is_padding=*/false));
  AdvanceTimeUntil(pacer->NextSendTime());
  pacer->ProcessPackets();
}

TEST_F(PacingControllerTest, CanControlQueueSizeUsingTtl) {
  const uint32_t kSsrc = 12345;
  const uint32_t kAudioSsrc = 2345;
  uint16_t sequence_number = 1234;

  PacingController::Configuration config;
  config.drain_large_queues = false;
  config.packet_queue_ttl.video = TimeDelta::Millis(500);
  auto pacer =
      std::make_unique<PacingController>(&clock_, &callback_, trials_, config);
  pacer->SetPacingRates(DataRate::BitsPerSec(100'000), DataRate::Zero());

  Timestamp send_time = Timestamp::Zero();
  for (int i = 0; i < 100; ++i) {
    // Enqueue a new audio and video frame every 33ms.
    if (clock_.CurrentTime() - send_time > TimeDelta::Millis(33)) {
      for (int j = 0; j < 3; ++j) {
        auto packet = BuildPacket(RtpPacketMediaType::kVideo, kSsrc,
                                  /*sequence_number=*/++sequence_number,
                                  /*capture_time_ms=*/2,
                                  /*size_bytes=*/1000);
        pacer->EnqueuePacket(std::move(packet));
      }
      auto packet = BuildPacket(RtpPacketMediaType::kAudio, kAudioSsrc,
                                /*sequence_number=*/++sequence_number,
                                /*capture_time_ms=*/2,
                                /*size_bytes=*/100);
      pacer->EnqueuePacket(std::move(packet));
      send_time = clock_.CurrentTime();
    }

    EXPECT_LE(clock_.CurrentTime() - pacer->OldestPacketEnqueueTime(),
              TimeDelta::Millis(500));
    clock_.AdvanceTime(pacer->NextSendTime() - clock_.CurrentTime());
    pacer->ProcessPackets();
  }
}

}  // namespace
}  // namespace webrtc
