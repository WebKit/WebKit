/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <utility>
#include <vector>

#include "webrtc/logging/rtc_event_log/mock/mock_rtc_event_log.h"
#include "webrtc/modules/audio_coding/audio_network_adaptor/audio_network_adaptor_impl.h"
#include "webrtc/modules/audio_coding/audio_network_adaptor/mock/mock_controller.h"
#include "webrtc/modules/audio_coding/audio_network_adaptor/mock/mock_controller_manager.h"
#include "webrtc/modules/audio_coding/audio_network_adaptor/mock/mock_debug_dump_writer.h"
#include "webrtc/test/gtest.h"

namespace webrtc {

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SetArgPointee;

namespace {

constexpr size_t kNumControllers = 2;

constexpr int64_t kClockInitialTimeMs = 12345678;

MATCHER_P(NetworkMetricsIs, metric, "") {
  return arg.uplink_bandwidth_bps == metric.uplink_bandwidth_bps &&
         arg.target_audio_bitrate_bps == metric.target_audio_bitrate_bps &&
         arg.rtt_ms == metric.rtt_ms &&
         arg.overhead_bytes_per_packet == metric.overhead_bytes_per_packet &&
         arg.uplink_packet_loss_fraction == metric.uplink_packet_loss_fraction;
}

MATCHER_P(EncoderRuntimeConfigIs, config, "") {
  return arg.bitrate_bps == config.bitrate_bps &&
         arg.frame_length_ms == config.frame_length_ms &&
         arg.uplink_packet_loss_fraction ==
             config.uplink_packet_loss_fraction &&
         arg.enable_fec == config.enable_fec &&
         arg.enable_dtx == config.enable_dtx &&
         arg.num_channels == config.num_channels;
}

struct AudioNetworkAdaptorStates {
  std::unique_ptr<AudioNetworkAdaptorImpl> audio_network_adaptor;
  std::vector<std::unique_ptr<MockController>> mock_controllers;
  std::unique_ptr<SimulatedClock> simulated_clock;
  std::unique_ptr<MockRtcEventLog> event_log;
  MockDebugDumpWriter* mock_debug_dump_writer;
};

AudioNetworkAdaptorStates CreateAudioNetworkAdaptor() {
  AudioNetworkAdaptorStates states;
  std::vector<Controller*> controllers;
  for (size_t i = 0; i < kNumControllers; ++i) {
    auto controller =
        std::unique_ptr<MockController>(new NiceMock<MockController>());
    EXPECT_CALL(*controller, Die());
    controllers.push_back(controller.get());
    states.mock_controllers.push_back(std::move(controller));
  }

  auto controller_manager = std::unique_ptr<MockControllerManager>(
      new NiceMock<MockControllerManager>());

  EXPECT_CALL(*controller_manager, Die());
  EXPECT_CALL(*controller_manager, GetControllers())
      .WillRepeatedly(Return(controllers));
  EXPECT_CALL(*controller_manager, GetSortedControllers(_))
      .WillRepeatedly(Return(controllers));

  states.simulated_clock.reset(new SimulatedClock(kClockInitialTimeMs * 1000));
  states.event_log.reset(new NiceMock<MockRtcEventLog>());

  auto debug_dump_writer =
      std::unique_ptr<MockDebugDumpWriter>(new NiceMock<MockDebugDumpWriter>());
  EXPECT_CALL(*debug_dump_writer, Die());
  states.mock_debug_dump_writer = debug_dump_writer.get();

  AudioNetworkAdaptorImpl::Config config;
  config.clock = states.simulated_clock.get();
  config.event_log = states.event_log.get();
  // AudioNetworkAdaptorImpl governs the lifetime of controller manager.
  states.audio_network_adaptor.reset(new AudioNetworkAdaptorImpl(
      config,
      std::move(controller_manager), std::move(debug_dump_writer)));

  return states;
}

void SetExpectCallToUpdateNetworkMetrics(
    const std::vector<std::unique_ptr<MockController>>& controllers,
    const Controller::NetworkMetrics& check) {
  for (auto& mock_controller : controllers) {
    EXPECT_CALL(*mock_controller,
                UpdateNetworkMetrics(NetworkMetricsIs(check)));
  }
}

}  // namespace

TEST(AudioNetworkAdaptorImplTest,
     UpdateNetworkMetricsIsCalledOnSetUplinkBandwidth) {
  auto states = CreateAudioNetworkAdaptor();
  constexpr int kBandwidth = 16000;
  Controller::NetworkMetrics check;
  check.uplink_bandwidth_bps = rtc::Optional<int>(kBandwidth);
  SetExpectCallToUpdateNetworkMetrics(states.mock_controllers, check);
  states.audio_network_adaptor->SetUplinkBandwidth(kBandwidth);
}

TEST(AudioNetworkAdaptorImplTest,
     UpdateNetworkMetricsIsCalledOnSetUplinkPacketLossFraction) {
  auto states = CreateAudioNetworkAdaptor();
  constexpr float kPacketLoss = 0.7f;
  Controller::NetworkMetrics check;
  check.uplink_packet_loss_fraction = rtc::Optional<float>(kPacketLoss);
  SetExpectCallToUpdateNetworkMetrics(states.mock_controllers, check);
  states.audio_network_adaptor->SetUplinkPacketLossFraction(kPacketLoss);
}

TEST(AudioNetworkAdaptorImplTest, UpdateNetworkMetricsIsCalledOnSetRtt) {
  auto states = CreateAudioNetworkAdaptor();
  constexpr int kRtt = 100;
  Controller::NetworkMetrics check;
  check.rtt_ms = rtc::Optional<int>(kRtt);
  SetExpectCallToUpdateNetworkMetrics(states.mock_controllers, check);
  states.audio_network_adaptor->SetRtt(kRtt);
}

TEST(AudioNetworkAdaptorImplTest,
     UpdateNetworkMetricsIsCalledOnSetTargetAudioBitrate) {
  auto states = CreateAudioNetworkAdaptor();
  constexpr int kTargetAudioBitrate = 15000;
  Controller::NetworkMetrics check;
  check.target_audio_bitrate_bps = rtc::Optional<int>(kTargetAudioBitrate);
  SetExpectCallToUpdateNetworkMetrics(states.mock_controllers, check);
  states.audio_network_adaptor->SetTargetAudioBitrate(kTargetAudioBitrate);
}

TEST(AudioNetworkAdaptorImplTest, UpdateNetworkMetricsIsCalledOnSetOverhead) {
  auto states = CreateAudioNetworkAdaptor();
  constexpr size_t kOverhead = 64;
  Controller::NetworkMetrics check;
  check.overhead_bytes_per_packet = rtc::Optional<size_t>(kOverhead);
  SetExpectCallToUpdateNetworkMetrics(states.mock_controllers, check);
  states.audio_network_adaptor->SetOverhead(kOverhead);
}

TEST(AudioNetworkAdaptorImplTest,
     MakeDecisionIsCalledOnGetEncoderRuntimeConfig) {
  auto states = CreateAudioNetworkAdaptor();
  for (auto& mock_controller : states.mock_controllers)
    EXPECT_CALL(*mock_controller, MakeDecision(_));
  states.audio_network_adaptor->GetEncoderRuntimeConfig();
}

TEST(AudioNetworkAdaptorImplTest,
     DumpEncoderRuntimeConfigIsCalledOnGetEncoderRuntimeConfig) {
  auto states = CreateAudioNetworkAdaptor();

  AudioNetworkAdaptor::EncoderRuntimeConfig config;
  config.bitrate_bps = rtc::Optional<int>(32000);
  config.enable_fec = rtc::Optional<bool>(true);

  EXPECT_CALL(*states.mock_controllers[0], MakeDecision(_))
      .WillOnce(SetArgPointee<0>(config));

  EXPECT_CALL(*states.mock_debug_dump_writer,
              DumpEncoderRuntimeConfig(EncoderRuntimeConfigIs(config),
                                       kClockInitialTimeMs));
  states.audio_network_adaptor->GetEncoderRuntimeConfig();
}

TEST(AudioNetworkAdaptorImplTest,
     DumpNetworkMetricsIsCalledOnSetNetworkMetrics) {
  auto states = CreateAudioNetworkAdaptor();

  constexpr int kBandwidth = 16000;
  constexpr float kPacketLoss = 0.7f;
  constexpr int kRtt = 100;
  constexpr int kTargetAudioBitrate = 15000;
  constexpr size_t kOverhead = 64;

  Controller::NetworkMetrics check;
  check.uplink_bandwidth_bps = rtc::Optional<int>(kBandwidth);
  int64_t timestamp_check = kClockInitialTimeMs;

  EXPECT_CALL(*states.mock_debug_dump_writer,
              DumpNetworkMetrics(NetworkMetricsIs(check), timestamp_check));
  states.audio_network_adaptor->SetUplinkBandwidth(kBandwidth);

  states.simulated_clock->AdvanceTimeMilliseconds(100);
  timestamp_check += 100;
  check.uplink_packet_loss_fraction = rtc::Optional<float>(kPacketLoss);
  EXPECT_CALL(*states.mock_debug_dump_writer,
              DumpNetworkMetrics(NetworkMetricsIs(check), timestamp_check));
  states.audio_network_adaptor->SetUplinkPacketLossFraction(kPacketLoss);

  states.simulated_clock->AdvanceTimeMilliseconds(200);
  timestamp_check += 200;
  check.rtt_ms = rtc::Optional<int>(kRtt);
  EXPECT_CALL(*states.mock_debug_dump_writer,
              DumpNetworkMetrics(NetworkMetricsIs(check), timestamp_check));
  states.audio_network_adaptor->SetRtt(kRtt);

  states.simulated_clock->AdvanceTimeMilliseconds(150);
  timestamp_check += 150;
  check.target_audio_bitrate_bps = rtc::Optional<int>(kTargetAudioBitrate);
  EXPECT_CALL(*states.mock_debug_dump_writer,
              DumpNetworkMetrics(NetworkMetricsIs(check), timestamp_check));
  states.audio_network_adaptor->SetTargetAudioBitrate(kTargetAudioBitrate);

  states.simulated_clock->AdvanceTimeMilliseconds(50);
  timestamp_check += 50;
  check.overhead_bytes_per_packet = rtc::Optional<size_t>(kOverhead);
  EXPECT_CALL(*states.mock_debug_dump_writer,
              DumpNetworkMetrics(NetworkMetricsIs(check), timestamp_check));
  states.audio_network_adaptor->SetOverhead(kOverhead);
}

TEST(AudioNetworkAdaptorImplTest, LogRuntimeConfigOnGetEncoderRuntimeConfig) {
  auto states = CreateAudioNetworkAdaptor();

  AudioNetworkAdaptor::EncoderRuntimeConfig config;
  config.bitrate_bps = rtc::Optional<int>(32000);
  config.enable_fec = rtc::Optional<bool>(true);

  EXPECT_CALL(*states.mock_controllers[0], MakeDecision(_))
      .WillOnce(SetArgPointee<0>(config));

  EXPECT_CALL(*states.event_log,
              LogAudioNetworkAdaptation(EncoderRuntimeConfigIs(config)))
      .Times(1);
  states.audio_network_adaptor->GetEncoderRuntimeConfig();
}

}  // namespace webrtc
