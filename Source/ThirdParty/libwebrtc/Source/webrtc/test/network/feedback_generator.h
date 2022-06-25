/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef TEST_NETWORK_FEEDBACK_GENERATOR_H_
#define TEST_NETWORK_FEEDBACK_GENERATOR_H_

#include <map>
#include <utility>
#include <vector>

#include "api/transport/test/feedback_generator_interface.h"
#include "call/simulated_network.h"
#include "test/network/network_emulation.h"
#include "test/network/network_emulation_manager.h"
#include "test/time_controller/simulated_time_controller.h"

namespace webrtc {

class FeedbackGeneratorImpl
    : public FeedbackGenerator,
      public TwoWayFakeTrafficRoute<SentPacket, TransportPacketsFeedback>::
          TrafficHandlerInterface {
 public:
  explicit FeedbackGeneratorImpl(Config config);
  Timestamp Now() override;
  void Sleep(TimeDelta duration) override;
  void SendPacket(size_t size) override;
  std::vector<TransportPacketsFeedback> PopFeedback() override;

  void SetSendConfig(BuiltInNetworkBehaviorConfig config) override;
  void SetReturnConfig(BuiltInNetworkBehaviorConfig config) override;

  void SetSendLinkCapacity(DataRate capacity) override;

  void OnRequest(SentPacket packet, Timestamp arrival_time) override;
  void OnResponse(TransportPacketsFeedback packet,
                  Timestamp arrival_time) override;

 private:
  Config conf_;
  ::webrtc::test::NetworkEmulationManagerImpl net_;
  SimulatedNetwork* const send_link_;
  SimulatedNetwork* const ret_link_;
  TwoWayFakeTrafficRoute<SentPacket, TransportPacketsFeedback> route_;

  TransportPacketsFeedback builder_;
  std::vector<TransportPacketsFeedback> feedback_;
  int64_t sequence_number_ = 1;
};
}  // namespace webrtc
#endif  // TEST_NETWORK_FEEDBACK_GENERATOR_H_
