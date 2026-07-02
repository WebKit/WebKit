/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>

#include "api/environment/environment.h"
#include "api/transport/ecn_marking.h"
#include "api/transport/goog_cc_factory.h"
#include "api/transport/network_control.h"
#include "api/transport/network_types.h"
#include "api/units/data_rate.h"
#include "api/units/data_size.h"
#include "test/create_test_environment.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

std::unique_ptr<NetworkControllerInterface> CreateController(
    Environment env,
    bool rfc_8888_feedback_negotiated) {
  NetworkControllerConfig config(env);
  config.constraints.at_time = env.clock().CurrentTime();
  config.constraints.starting_rate = DataRate::KilobitsPerSec(100);
  GoogCcNetworkControllerFactory factory(
      {.rfc_8888_feedback_negotiated = rfc_8888_feedback_negotiated});
  return factory.Create(config);
}

TEST(GoogCcScreamNetworkControllerTest, CreateWithoutFieldTrial) {
  Environment env = CreateTestEnvironment();
  std::unique_ptr<NetworkControllerInterface> controller =
      CreateController(env, /*rfc_8888_feedback_negotiated*/ true);
  EXPECT_FALSE(controller->SupportsEcnAdaptation());
}

TEST(GoogCcScreamNetworkControllerTest, CreateWithScreamAlways) {
  Environment env = CreateTestEnvironment(
      {.field_trials = "WebRTC-Bwe-ScreamV2/mode:always/"});
  NetworkControllerConfig config(env);
  std::unique_ptr<NetworkControllerInterface> controller =
      CreateController(env, /*rfc_8888_feedback_negotiated*/ false);
  EXPECT_TRUE(controller->SupportsEcnAdaptation());
}

TEST(GoogCcScreamNetworkControllerTest, CreateWithScreamAfterCeWithTwcc) {
  Environment env = CreateTestEnvironment(
      {.field_trials = "WebRTC-Bwe-ScreamV2/mode:only_after_ce/"});
  std::unique_ptr<NetworkControllerInterface> controller =
      CreateController(env, /*rfc_8888_feedback_negotiated*/ false);
  EXPECT_FALSE(controller->SupportsEcnAdaptation());
}

TEST(GoogCcScreamNetworkControllerTest, CreateWithScreamAfterCe) {
  Environment env = CreateTestEnvironment(
      {.field_trials = "WebRTC-Bwe-ScreamV2/mode:only_after_ce/"});
  std::unique_ptr<NetworkControllerInterface> controller =
      CreateController(env, /*rfc_8888_feedback_negotiated*/ true);
  EXPECT_TRUE(controller->SupportsEcnAdaptation());

  // Send a feedback with ECN CE.
  TransportPacketsFeedback feedback;
  feedback.feedback_time = env.clock().CurrentTime();
  PacketResult packet_result;
  packet_result.sent_packet.send_time = env.clock().CurrentTime();
  packet_result.sent_packet.sequence_number = 1;
  packet_result.sent_packet.size = DataSize::Bytes(100);
  packet_result.receive_time = env.clock().CurrentTime();
  packet_result.ecn = EcnMarking::kCe;
  feedback.packet_feedbacks.push_back(packet_result);
  auto ignore = controller->OnTransportPacketsFeedback(feedback);

  EXPECT_TRUE(controller->SupportsEcnAdaptation());
}

TEST(GoogCcScreamNetworkControllerTest, CreateWithGoogCcWithEct1) {
  Environment env = CreateTestEnvironment(
      {.field_trials = "WebRTC-Bwe-ScreamV2/mode:goog_cc_with_ect1/"});
  std::unique_ptr<NetworkControllerInterface> controller =
      CreateController(env, /*rfc_8888_feedback_negotiated*/ true);
  EXPECT_TRUE(controller->SupportsEcnAdaptation());

  // Send a feedback with ECN CE.
  TransportPacketsFeedback feedback;
  feedback.feedback_time = env.clock().CurrentTime();
  PacketResult packet_result;
  packet_result.sent_packet.send_time = env.clock().CurrentTime();
  packet_result.sent_packet.sequence_number = 1;
  packet_result.sent_packet.size = DataSize::Bytes(100);
  packet_result.receive_time = env.clock().CurrentTime();
  packet_result.ecn = EcnMarking::kCe;
  feedback.packet_feedbacks.push_back(packet_result);
  auto ignore = controller->OnTransportPacketsFeedback(feedback);

  EXPECT_FALSE(controller->SupportsEcnAdaptation());
}

}  // namespace
}  // namespace webrtc
