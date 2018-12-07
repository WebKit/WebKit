/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef CALL_SIMULATED_NETWORK_H_
#define CALL_SIMULATED_NETWORK_H_

#include <deque>
#include <queue>
#include <vector>

#include "absl/memory/memory.h"
#include "absl/types/optional.h"
#include "api/test/simulated_network.h"
#include "rtc_base/criticalsection.h"
#include "rtc_base/random.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc {

// Class simulating a network link. This is a simple and naive solution just
// faking capacity and adding an extra transport delay in addition to the
// capacity introduced delay.
class SimulatedNetwork : public NetworkBehaviorInterface {
 public:
  using Config = BuiltInNetworkBehaviorConfig;
  explicit SimulatedNetwork(Config config, uint64_t random_seed = 1);
  ~SimulatedNetwork() override;

  // Sets a new configuration. This won't affect packets already in the pipe.
  void SetConfig(const Config& config);
  void PauseTransmissionUntil(int64_t until_us);

  // NetworkBehaviorInterface
  bool EnqueuePacket(PacketInFlightInfo packet) override;
  std::vector<PacketDeliveryInfo> DequeueDeliverablePackets(
      int64_t receive_time_us) override;

  absl::optional<int64_t> NextDeliveryTimeUs() const override;

 private:
  struct PacketInfo {
    PacketInFlightInfo packet;
    int64_t arrival_time_us;
  };
  rtc::CriticalSection config_lock_;
  bool reset_capacity_delay_error_ RTC_GUARDED_BY(config_lock_) = false;

  // |process_lock| guards the data structures involved in delay and loss
  // processes, such as the packet queues.
  rtc::CriticalSection process_lock_;
  std::queue<PacketInfo> capacity_link_ RTC_GUARDED_BY(process_lock_);
  Random random_;

  std::deque<PacketInfo> delay_link_ RTC_GUARDED_BY(process_lock_);

  // Link configuration.
  Config config_ RTC_GUARDED_BY(config_lock_);
  absl::optional<int64_t> pause_transmission_until_us_
      RTC_GUARDED_BY(config_lock_);

  // Are we currently dropping a burst of packets?
  bool bursting_;

  // The probability to drop the packet if we are currently dropping a
  // burst of packet
  double prob_loss_bursting_ RTC_GUARDED_BY(config_lock_);

  // The probability to drop a burst of packets.
  double prob_start_bursting_ RTC_GUARDED_BY(config_lock_);
  int64_t capacity_delay_error_bytes_ = 0;
};

}  // namespace webrtc

#endif  // CALL_SIMULATED_NETWORK_H_
