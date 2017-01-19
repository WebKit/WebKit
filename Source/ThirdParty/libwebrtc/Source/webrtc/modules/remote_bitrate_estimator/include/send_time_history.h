/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_INCLUDE_SEND_TIME_HISTORY_H_
#define WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_INCLUDE_SEND_TIME_HISTORY_H_

#include <map>

#include "webrtc/base/basictypes.h"
#include "webrtc/base/constructormagic.h"
#include "webrtc/modules/include/module_common_types.h"

namespace webrtc {
class Clock;
struct PacketInfo;

class SendTimeHistory {
 public:
  SendTimeHistory(Clock* clock, int64_t packet_age_limit_ms);
  ~SendTimeHistory();

  void Clear();

  // Cleanup old entries, then add new packet info with provided parameters.
  void AddAndRemoveOld(uint16_t sequence_number,
                       size_t payload_size,
                       int probe_cluster_id);

  // Updates packet info identified by |sequence_number| with |send_time_ms|.
  // Return false if not found.
  bool OnSentPacket(uint16_t sequence_number, int64_t send_time_ms);

  // Look up PacketInfo for a sent packet, based on the sequence number, and
  // populate all fields except for arrival_time. The packet parameter must
  // thus be non-null and have the sequence_number field set.
  bool GetInfo(PacketInfo* packet_info, bool remove);

 private:
  Clock* const clock_;
  const int64_t packet_age_limit_ms_;
  SequenceNumberUnwrapper seq_num_unwrapper_;
  std::map<int64_t, PacketInfo> history_;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(SendTimeHistory);
};

}  // namespace webrtc
#endif  // WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_INCLUDE_SEND_TIME_HISTORY_H_
