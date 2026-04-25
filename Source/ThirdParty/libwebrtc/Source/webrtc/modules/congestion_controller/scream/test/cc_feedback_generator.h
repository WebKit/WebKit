/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_CONGESTION_CONTROLLER_SCREAM_TEST_CC_FEEDBACK_GENERATOR_H_
#define MODULES_CONGESTION_CONTROLLER_SCREAM_TEST_CC_FEEDBACK_GENERATOR_H_

#include <cstdint>
#include <deque>
#include <memory>
#include <optional>

#include "absl/functional/any_invocable.h"
#include "api/test/network_emulation/leaky_bucket_network_queue.h"
#include "api/test/network_emulation/network_queue.h"
#include "api/test/simulated_network.h"
#include "api/transport/network_types.h"
#include "api/units/data_rate.h"
#include "api/units/data_size.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "system_wrappers/include/clock.h"
#include "test/network/simulated_network.h"

namespace webrtc {

// Simulates sending packets with a given send rate over a simulated network and
// generates TransportPacketsFeedback that is supposed to match
// TransportPacketsFeedback from rtcp::CongestionControlFeedback (RFC8888).
class CcFeedbackGenerator {
 public:
  struct Config {
    SimulatedNetwork::Config network_config;
    TimeDelta time_between_feedback = TimeDelta::Millis(25);
    bool send_as_ect1 = true;
    std::unique_ptr<NetworkQueue> queue =
        std::make_unique<LeakyBucketNetworkQueue>(
            LeakyBucketNetworkQueue::Config{
                .max_ect1_sojourn_time = TimeDelta::Millis(8),
                .target_ect1_sojourn_time = TimeDelta::Millis(4)});
    DataSize packet_size = DataSize::Bytes(1000);
  };

  explicit CcFeedbackGenerator(Config config);

  // Processes the simulation until the next feedback message is received.
  // The function will send packets at the given send rate until the next
  // feedback message is generated.
  TransportPacketsFeedback ProcessUntilNextFeedback(DataRate send_rate,
                                                    SimulatedClock& clock) {
    return ProcessUntilNextFeedback(send_rate, clock, nullptr);
  }

  TransportPacketsFeedback ProcessUntilNextFeedback(
      DataRate send_rate,
      SimulatedClock& clock,
      absl::AnyInvocable<void(const SentPacket&)> sent_packet_cb,
      TimeDelta max_time = TimeDelta::Seconds(1));

  static int CountCeMarks(const TransportPacketsFeedback& feedback);

 private:
  std::optional<SentPacket> MaybeSendPackets(Timestamp time,
                                             DataRate send_rate);
  void ProcessNetwork(Timestamp time);
  std::optional<TransportPacketsFeedback> MaybeSendFeedback(Timestamp time);

  const DataSize packet_size_;
  const TimeDelta time_between_feedback_;
  const TimeDelta one_way_delay_;
  const bool send_as_ect1_;
  SimulatedNetwork network_;

  int64_t next_packet_id_ = 0;
  std::deque<PacketInFlightInfo> packets_in_flight_;
  // `packets_received_` are packed that have been received by the remote, but
  // feedback has not yet been received by the sender. Feedback is delivered one
  // way delay later than when the packets were received.
  std::deque<PacketDeliveryInfo> packets_received_;

  Timestamp last_feedback_time_ = Timestamp::MinusInfinity();

  Timestamp last_send_budget_update = Timestamp::MinusInfinity();
  DataSize send_budget_;
};
}  // namespace webrtc

#endif  // MODULES_CONGESTION_CONTROLLER_SCREAM_TEST_CC_FEEDBACK_GENERATOR_H_
