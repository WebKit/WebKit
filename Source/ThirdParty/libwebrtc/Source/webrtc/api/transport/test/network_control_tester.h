/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_TRANSPORT_TEST_NETWORK_CONTROL_TESTER_H_
#define API_TRANSPORT_TEST_NETWORK_CONTROL_TESTER_H_

#include <deque>
#include <functional>
#include <memory>

#include "absl/types/optional.h"
#include "api/transport/network_control.h"

namespace webrtc {
namespace test {

// Produces one packet per time delta
class SimpleTargetRateProducer {
 public:
  static SentPacket ProduceNext(const NetworkControlUpdate& state,
                                Timestamp current_time,
                                TimeDelta time_delta);
};

class NetworkControllerTester {
 public:
  // A PacketProducer is a function that takes a network control state, a
  // timestamp representing the expected send time and a time delta of the send
  // times (This allows the PacketProducer to be stateless). It returns a
  // SentPacket struct with actual send time and packet size.
  using PacketProducer = std::function<
      SentPacket(const NetworkControlUpdate&, Timestamp, TimeDelta)>;
  NetworkControllerTester(NetworkControllerFactoryInterface* factory,
                          NetworkControllerConfig initial_config);
  ~NetworkControllerTester();

  // Runs the simulations for the given duration, the PacketProducer will be
  // called repeatedly based on the given packet interval and the network will
  // be simulated using given bandwidth and propagation delay. The simulation
  // will call the controller under test with OnSentPacket and
  // OnTransportPacketsFeedback.

  // Note that OnTransportPacketsFeedback will only be called for
  // packets with resulting feedback time within the simulated duration. Packets
  // with later feedback time are saved and used in the next call to
  // RunSimulation where enough simulated time has passed.
  void RunSimulation(TimeDelta duration,
                     TimeDelta packet_interval,
                     DataRate actual_bandwidth,
                     TimeDelta propagation_delay,
                     PacketProducer next_packet);
  NetworkControlUpdate GetState() { return state_; }

 private:
  std::unique_ptr<NetworkControllerInterface> controller_;
  TimeDelta process_interval_ = TimeDelta::PlusInfinity();
  Timestamp current_time_;
  int64_t packet_sequence_number_;
  TimeDelta accumulated_buffer_;
  std::deque<PacketResult> outstanding_packets_;
  NetworkControlUpdate state_;
};
}  // namespace test
}  // namespace webrtc

#endif  // API_TRANSPORT_TEST_NETWORK_CONTROL_TESTER_H_
