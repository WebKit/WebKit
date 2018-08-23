/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_CONGESTION_CONTROLLER_RTP_SEND_TIME_HISTORY_H_
#define MODULES_CONGESTION_CONTROLLER_RTP_SEND_TIME_HISTORY_H_

#include <map>
#include <utility>

#include "modules/include/module_common_types.h"
#include "rtc_base/constructormagic.h"

namespace webrtc {
class Clock;
struct PacketFeedback;

class SendTimeHistory {
 public:
  SendTimeHistory(const Clock* clock, int64_t packet_age_limit_ms);
  ~SendTimeHistory();

  // Cleanup old entries, then add new packet info with provided parameters.
  void AddAndRemoveOld(const PacketFeedback& packet);

  // Updates packet info identified by |sequence_number| with |send_time_ms|.
  // Return false if not found.
  bool OnSentPacket(uint16_t sequence_number, int64_t send_time_ms);

  // Retrieves packet info identified by |sequence_number|.
  absl::optional<PacketFeedback> GetPacket(uint16_t sequence_number) const;

  // Look up PacketFeedback for a sent packet, based on the sequence number, and
  // populate all fields except for arrival_time. The packet parameter must
  // thus be non-null and have the sequence_number field set.
  bool GetFeedback(PacketFeedback* packet_feedback, bool remove);

  size_t GetOutstandingBytes(uint16_t local_net_id,
                             uint16_t remote_net_id) const;

 private:
  using RemoteAndLocalNetworkId = std::pair<uint16_t, uint16_t>;

  void AddPacketBytes(const PacketFeedback& packet);
  void RemovePacketBytes(const PacketFeedback& packet);
  void UpdateAckedSeqNum(int64_t acked_seq_num);
  const Clock* const clock_;
  const int64_t packet_age_limit_ms_;
  SequenceNumberUnwrapper seq_num_unwrapper_;
  std::map<int64_t, PacketFeedback> history_;
  absl::optional<int64_t> last_ack_seq_num_;
  std::map<RemoteAndLocalNetworkId, size_t> in_flight_bytes_;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(SendTimeHistory);
};

}  // namespace webrtc
#endif  // MODULES_CONGESTION_CONTROLLER_RTP_SEND_TIME_HISTORY_H_
