/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef API_TRANSPORT_TEST_FEEDBACK_GENERATOR_INTERFACE_H_
#define API_TRANSPORT_TEST_FEEDBACK_GENERATOR_INTERFACE_H_

#include <cstddef>
#include <vector>

#include "api/rtc_event_log/rtc_event_log.h"
#include "api/test/network_emulation_manager.h"
#include "api/test/simulated_network.h"
#include "api/transport/network_types.h"
#include "api/units/data_rate.h"
#include "api/units/data_size.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"

namespace webrtc {
class FeedbackGenerator {
 public:
  struct Config {
    BuiltInNetworkBehaviorConfig send_link;
    BuiltInNetworkBehaviorConfig return_link;
    TimeDelta feedback_interval = TimeDelta::Millis(50);
    DataSize feedback_packet_size = DataSize::Bytes(20);
  };
  virtual ~FeedbackGenerator() = default;
  virtual Timestamp Now() = 0;
  virtual void Sleep(TimeDelta duration) = 0;
  // Send a packet of the given size over the simulated network.
  virtual void SendPacket(size_t size) = 0;
  virtual std::vector<TransportPacketsFeedback> PopFeedback() = 0;
  virtual void SetSendConfig(BuiltInNetworkBehaviorConfig config) = 0;
  virtual void SetReturnConfig(BuiltInNetworkBehaviorConfig config) = 0;
  virtual void SetSendLinkCapacity(DataRate capacity) = 0;
  virtual RtcEventLog& event_log() = 0;
};

// Same as FeedbackGenerator, but NetworkEmulationManager is owned externally.
// Packets can be sent and received via multiple nodes.
class FeedbackGeneratorWithoutNetwork {
 public:
  struct Config {
    std::vector<EmulatedNetworkNode*> sent_via_nodes;
    std::vector<EmulatedNetworkNode*> received_via_nodes;
    TimeDelta feedback_interval = TimeDelta::Millis(50);
    DataSize feedback_packet_size = DataSize::Bytes(20);
  };

  virtual ~FeedbackGeneratorWithoutNetwork() = default;

  // Send a packet of the given size over the simulated network.
  // The packet size logged in the event log is `total_size` - `overhead`.
  // This allows a user to ensure that LoggedPacketInfo.size +
  // LoggedPacketInfo.overhead  in the event log is `total_size`.
  // Note that ParsedRtcEventLog estimate the overhead depending on the
  // selected ice candidate.
  virtual void SendPacket(size_t total_size, size_t overhead) = 0;
  virtual std::vector<TransportPacketsFeedback> PopFeedback() = 0;
  virtual RtcEventLog& event_log() = 0;
};

}  // namespace webrtc
#endif  // API_TRANSPORT_TEST_FEEDBACK_GENERATOR_INTERFACE_H_
