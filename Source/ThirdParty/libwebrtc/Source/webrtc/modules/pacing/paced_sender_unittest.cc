/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/pacing/paced_sender.h"

#include <list>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "modules/pacing/packet_router.h"
#include "modules/utility/include/mock/mock_process_thread.h"
#include "system_wrappers/include/clock.h"
#include "system_wrappers/include/field_trial.h"
#include "test/field_trial.h"
#include "test/gmock.h"
#include "test/gtest.h"

using ::testing::_;
using ::testing::Return;
using ::testing::SaveArg;

namespace webrtc {
namespace {
constexpr uint32_t kAudioSsrc = 12345;
constexpr uint32_t kVideoSsrc = 234565;
constexpr uint32_t kVideoRtxSsrc = 34567;
constexpr uint32_t kFlexFecSsrc = 45678;
constexpr size_t kDefaultPacketSize = 234;

// Mock callback implementing the raw api.
class MockCallback : public PacketRouter {
 public:
  MOCK_METHOD(void,
              SendPacket,
              (std::unique_ptr<RtpPacketToSend> packet,
               const PacedPacketInfo& cluster_info),
              (override));
  MOCK_METHOD(std::vector<std::unique_ptr<RtpPacketToSend>>,
              GeneratePadding,
              (DataSize target_size),
              (override));
};

class ProcessModeTrials : public WebRtcKeyValueConfig {
 public:
  explicit ProcessModeTrials(bool dynamic_process) : mode_(dynamic_process) {}

  std::string Lookup(absl::string_view key) const override {
    if (key == "WebRTC-Pacer-DynamicProcess") {
      return mode_ ? "Enabled" : "Disabled";
    }
    return "";
  }

 private:
  const bool mode_;
};
}  // namespace

namespace test {

class PacedSenderTest
    : public ::testing::TestWithParam<PacingController::ProcessMode> {
 public:
  PacedSenderTest()
      : clock_(0),
        paced_module_(nullptr),
        trials_(GetParam() == PacingController::ProcessMode::kDynamic) {}

  void SetUp() override {
    EXPECT_CALL(process_thread_, RegisterModule)
        .WillOnce(SaveArg<0>(&paced_module_));

    pacer_ = std::make_unique<PacedSender>(&clock_, &callback_, nullptr,
                                           &trials_, &process_thread_);
    EXPECT_CALL(process_thread_, WakeUp).WillRepeatedly([&](Module* module) {
      clock_.AdvanceTimeMilliseconds(module->TimeUntilNextProcess());
    });
    EXPECT_CALL(process_thread_, DeRegisterModule(paced_module_)).Times(1);
  }

 protected:
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

    packet->SetPayloadSize(kDefaultPacketSize);
    return packet;
  }

  SimulatedClock clock_;
  MockCallback callback_;
  MockProcessThread process_thread_;
  Module* paced_module_;
  ProcessModeTrials trials_;
  std::unique_ptr<PacedSender> pacer_;
};

TEST_P(PacedSenderTest, PacesPackets) {
  // Insert a number of packets, covering one second.
  static constexpr size_t kPacketsToSend = 42;
  pacer_->SetPacingRates(
      DataRate::BitsPerSec(kDefaultPacketSize * 8 * kPacketsToSend),
      DataRate::Zero());
  std::vector<std::unique_ptr<RtpPacketToSend>> packets;
  for (size_t i = 0; i < kPacketsToSend; ++i) {
    packets.emplace_back(BuildRtpPacket(RtpPacketMediaType::kVideo));
  }
  pacer_->EnqueuePackets(std::move(packets));

  // Expect all of them to be sent.
  size_t packets_sent = 0;
  EXPECT_CALL(callback_, SendPacket)
      .WillRepeatedly(
          [&](std::unique_ptr<RtpPacketToSend> packet,
              const PacedPacketInfo& cluster_info) { ++packets_sent; });

  const Timestamp start_time = clock_.CurrentTime();

  while (packets_sent < kPacketsToSend) {
    clock_.AdvanceTimeMilliseconds(paced_module_->TimeUntilNextProcess());
    paced_module_->Process();
  }

  // Packets should be sent over a period of close to 1s. Expect a little lower
  // than this since initial probing is a bit quicker.
  TimeDelta duration = clock_.CurrentTime() - start_time;
  EXPECT_GT(duration, TimeDelta::Millis(900));
}

INSTANTIATE_TEST_SUITE_P(
    WithAndWithoutDynamicProcess,
    PacedSenderTest,
    ::testing::Values(PacingController::ProcessMode::kPeriodic,
                      PacingController::ProcessMode::kDynamic));

}  // namespace test
}  // namespace webrtc
