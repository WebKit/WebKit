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

#include <cstddef>
#include <cstdint>
#include <memory>
#include <queue>
#include <vector>

#include "api/rtc_event_log/rtc_event_log.h"
#include "api/test/network_emulation_manager.h"
#include "api/test/simulated_network.h"
#include "api/transport/network_types.h"
#include "api/transport/test/feedback_generator_interface.h"
#include "api/units/data_rate.h"
#include "api/units/data_size.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "modules/rtp_rtcp/include/rtp_header_extension_map.h"
#include "test/network/network_emulation.h"
#include "test/network/simulated_network.h"

namespace webrtc {

class FeedbackGeneratorWithoutNetworkImpl
    : public FeedbackGeneratorWithoutNetwork,
      TwoWayFakeTrafficRoute<SentPacket, std::vector<PacketResult>>::
          TrafficHandlerInterface {
 public:
  FeedbackGeneratorWithoutNetworkImpl(Config config,
                                      NetworkEmulationManager& net);
  ~FeedbackGeneratorWithoutNetworkImpl() override = default;

  void SendPacket(size_t total_size, size_t overhead) override;
  std::vector<TransportPacketsFeedback> PopFeedback() override;

  RtcEventLog& event_log() override { return *event_log_; }
  void OnRequest(SentPacket packet, Timestamp arrival_time) override;
  void OnResponse(std::vector<PacketResult> packet_results,
                  Timestamp arrival_time) override;

  Timestamp Now();

 private:
  NetworkEmulationManager& net_;
  const TimeDelta feedback_interval_;
  const DataSize feedback_packet_size_;

  std::unique_ptr<RtcEventLog> event_log_;
  TwoWayFakeTrafficRoute<SentPacket, std::vector<PacketResult>> route_;

  std::queue<SentPacket> sent_packets_;
  std::vector<PacketResult> received_packets_;
  std::vector<TransportPacketsFeedback> feedback_;
  int64_t sequence_number_ = 1;
  RtpHeaderExtensionMap rtp_extensions_;
};

class FeedbackGeneratorImpl : public FeedbackGenerator {
 public:
  explicit FeedbackGeneratorImpl(Config config);

  Timestamp Now() override;
  void Sleep(TimeDelta duration) override;
  void SendPacket(size_t size) override;
  std::vector<TransportPacketsFeedback> PopFeedback() override;

  void SetSendConfig(BuiltInNetworkBehaviorConfig config) override;
  void SetReturnConfig(BuiltInNetworkBehaviorConfig config) override;

  void SetSendLinkCapacity(DataRate capacity) override;

  RtcEventLog& event_log() override { return delegate_.event_log(); }

 private:
  Config conf_;
  std::unique_ptr<NetworkEmulationManager> net_;
  SimulatedNetwork* const send_link_;
  SimulatedNetwork* const ret_link_;
  FeedbackGeneratorWithoutNetworkImpl delegate_;
};

}  // namespace webrtc
#endif  // TEST_NETWORK_FEEDBACK_GENERATOR_H_
