/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_BASE_PACKETLOSSESTIMATOR_H_
#define P2P_BASE_PACKETLOSSESTIMATOR_H_

#include <stdint.h>
#include <string>
#include <unordered_map>

namespace cricket {

// Estimates the response rate for a series of messages expecting responses.
// Messages and their corresponding responses are identified by a string id.
//
// Responses are considered lost if they are not received within
// |consider_lost_after_ms|. If a response is received after
// |consider_lost_after_ms|, it is still considered as a response.
// Messages sent more than |forget_after_ms| ago are not considered
// anymore. The response rate is initially assumed to be 1.0.
//
// If the current time is 7, |forget_after_ms| is 6, and
// |consider_lost_after_ms| is 2, the response rate considers messages sent
// between times 1 and 5, so only messages in the following window can be
// considered lost:
//
// Time: 0 1 2 3 4 5 6 7
// Wind:   <------->   |
//
// Responses received to the right of the window are still counted.
class PacketLossEstimator {
 public:
  explicit PacketLossEstimator(int64_t consider_lost_after_ms,
                               int64_t forget_after_ms);
  ~PacketLossEstimator();

  // Registers that a message with the given |id| was sent at |sent_time|.
  void ExpectResponse(std::string id, int64_t sent_time);

  // Registers a response with the given |id| was received at |received_time|.
  void ReceivedResponse(std::string id, int64_t received_time);

  // Calculates the current response rate based on the expected and received
  // messages. Messages sent more than |forget_after| ms ago will be forgotten.
  void UpdateResponseRate(int64_t now);

  // Gets the current response rate as updated by UpdateResponseRate.
  double get_response_rate() const { return response_rate_; }

  std::size_t tracked_packet_count_for_testing() const;

 private:
  struct PacketInfo {
    int64_t sent_time;
    bool response_received;
  };

  // Called periodically by ExpectResponse and ReceivedResponse to manage memory
  // usage.
  void MaybeForgetOldRequests(int64_t now);

  bool ConsiderLost(const PacketInfo&, int64_t now) const;
  bool Forget(const PacketInfo&, int64_t now) const;

  int64_t consider_lost_after_ms_;
  int64_t forget_after_ms_;

  int64_t last_forgot_at_ = 0;

  std::unordered_map<std::string, PacketInfo> tracked_packets_;

  double response_rate_ = 1.0;
};

}  // namespace cricket

#endif  // P2P_BASE_PACKETLOSSESTIMATOR_H_
