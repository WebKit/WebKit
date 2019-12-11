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

#include "api/units/data_size.h"
#include "modules/include/module_common_types.h"
#include "rtc_base/constructor_magic.h"

namespace webrtc {
struct PacketFeedback;

class SendTimeHistory {
 public:
  enum class Status { kNotAdded, kOk, kDuplicate };

  explicit SendTimeHistory(int64_t packet_age_limit_ms);
  ~SendTimeHistory();

  // Cleanup old entries, then add new packet info with provided parameters.
  void RemoveOld(int64_t at_time_ms);
  void AddNewPacket(PacketFeedback packet);

  void AddUntracked(size_t packet_size, int64_t send_time_ms);

  // Updates packet info identified by |sequence_number| with |send_time_ms|.
  // Returns a PacketSendState indicating if the packet was not found, sent,
  // or if it was previously already marked as sent.
  Status OnSentPacket(uint16_t sequence_number, int64_t send_time_ms);

  // Retrieves packet info identified by |sequence_number|.
  absl::optional<PacketFeedback> GetPacket(uint16_t sequence_number) const;

  // Look up PacketFeedback for a sent packet, based on the sequence number, and
  // populate all fields except for arrival_time. The packet parameter must
  // thus be non-null and have the sequence_number field set.
  bool GetFeedback(PacketFeedback* packet_feedback, bool remove);

  DataSize GetOutstandingData(uint16_t local_net_id,
                              uint16_t remote_net_id) const;

  absl::optional<int64_t> GetFirstUnackedSendTime() const;

 private:
  using RemoteAndLocalNetworkId = std::pair<uint16_t, uint16_t>;

  void AddPacketBytes(const PacketFeedback& packet);
  void RemovePacketBytes(const PacketFeedback& packet);
  void UpdateAckedSeqNum(int64_t acked_seq_num);
  const int64_t packet_age_limit_ms_;
  size_t pending_untracked_size_ = 0;
  int64_t last_send_time_ms_ = -1;
  int64_t last_untracked_send_time_ms_ = -1;
  SequenceNumberUnwrapper seq_num_unwrapper_;
  std::map<int64_t, PacketFeedback> history_;
  absl::optional<int64_t> last_ack_seq_num_;
  std::map<RemoteAndLocalNetworkId, size_t> in_flight_bytes_;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(SendTimeHistory);
};

}  // namespace webrtc
#endif  // MODULES_CONGESTION_CONTROLLER_RTP_SEND_TIME_HISTORY_H_
