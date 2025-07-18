/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <cstddef>
#include <memory>
#include <string>

#include "api/rtc_event_log/rtc_event_log.h"
#include "api/test/create_network_emulation_manager.h"
#include "api/test/network_emulation_manager.h"
#include "api/test/simulated_network.h"
#include "api/transport/test/create_feedback_generator.h"
#include "api/transport/test/feedback_generator_interface.h"
#include "api/units/time_delta.h"
#include "logging/rtc_event_log/rtc_event_log_parser.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/logging/log_writer.h"
#include "test/logging/memory_log_writer.h"

namespace webrtc {

using ::testing::Eq;
using ::testing::Field;
using ::testing::SizeIs;

TEST(FeedbackGeneratorTest, ReportsFeedbackForSentPackets) {
  size_t kPacketSize = 1000;
  auto gen = CreateFeedbackGenerator(FeedbackGenerator::Config());
  for (int i = 0; i < 10; ++i) {
    gen->SendPacket(kPacketSize);
    gen->Sleep(TimeDelta::Millis(50));
  }
  auto feedback_list = gen->PopFeedback();
  EXPECT_GT(feedback_list.size(), 0u);
  for (const auto& feedback : feedback_list) {
    EXPECT_GT(feedback.packet_feedbacks.size(), 0u);
    for (const auto& packet : feedback.packet_feedbacks) {
      EXPECT_EQ(packet.sent_packet.size.bytes<size_t>(), kPacketSize);
    }
  }
}

TEST(FeedbackGeneratorTest, FeedbackIncludesLostPackets) {
  size_t kPacketSize = 1000;
  auto gen = CreateFeedbackGenerator(FeedbackGenerator::Config());
  BuiltInNetworkBehaviorConfig send_config_with_loss;
  send_config_with_loss.loss_percent = 50;
  gen->SetSendConfig(send_config_with_loss);
  for (int i = 0; i < 20; ++i) {
    gen->SendPacket(kPacketSize);
    gen->Sleep(TimeDelta::Millis(5));
  }
  auto feedback_list = gen->PopFeedback();
  ASSERT_GT(feedback_list.size(), 0u);
  EXPECT_NEAR(feedback_list[0].LostWithSendInfo().size(),
              feedback_list[0].ReceivedWithSendInfo().size(), 2);
}

TEST(FeedbackGeneratorTest, WritesToEventLog) {
  size_t kPacketSize = 1000;
  auto gen = CreateFeedbackGenerator(FeedbackGenerator::Config());

  MemoryLogStorage log_storage;
  std::unique_ptr<LogWriterFactoryInterface> log_writer_factory =
      log_storage.CreateFactory();

  const std::string rtc_event_log_file_name = "eventlog";
  gen->event_log().StartLogging(
      log_writer_factory->Create(rtc_event_log_file_name),
      RtcEventLog::kImmediateOutput);

  for (int i = 0; i < 10; ++i) {
    gen->SendPacket(kPacketSize);
    gen->Sleep(TimeDelta::Millis(50));
  }
  auto feedback_list = gen->PopFeedback();
  ASSERT_GT(feedback_list.size(), 0u);

  gen->event_log().StopLogging();
  ParsedRtcEventLog parsed_log;
  auto it = log_storage.logs().find(rtc_event_log_file_name);
  ASSERT_TRUE(it != log_storage.logs().end());
  ASSERT_TRUE(parsed_log.ParseString(it->second).ok());

  EXPECT_THAT(parsed_log.GetOutgoingPacketInfos(), SizeIs(10));
  EXPECT_THAT(parsed_log.GetOutgoingPacketInfos(),
              Each(Field(&LoggedPacketInfo::size, Eq(kPacketSize))));
  EXPECT_THAT(parsed_log.transport_feedbacks(PacketDirection::kIncomingPacket),
              SizeIs(feedback_list.size()));
}

TEST(FeedbackGeneratorWithoutNetworkTest, ReportsFeedbackForSentPackets) {
  auto network_emulation_manager =
      CreateNetworkEmulationManager({.time_mode = TimeMode::kSimulated});
  std::unique_ptr<FeedbackGeneratorWithoutNetwork> gen =
      CreateFeedbackGeneratorWithoutNetwork(
          FeedbackGeneratorWithoutNetwork::Config(),
          *network_emulation_manager);
  size_t kTotalPacketSize = 1000;
  size_t kOverhead = 40;

  for (int i = 0; i < 10; ++i) {
    gen->SendPacket(kTotalPacketSize, kOverhead);
    network_emulation_manager->time_controller()->AdvanceTime(
        TimeDelta::Millis(50));
  }
  auto feedback_list = gen->PopFeedback();
  EXPECT_GT(feedback_list.size(), 0u);
  for (const auto& feedback : feedback_list) {
    EXPECT_GT(feedback.packet_feedbacks.size(), 0u);
    for (const auto& packet : feedback.packet_feedbacks) {
      EXPECT_EQ(packet.sent_packet.size.bytes<size_t>(), kTotalPacketSize);
    }
  }
}

}  // namespace webrtc
