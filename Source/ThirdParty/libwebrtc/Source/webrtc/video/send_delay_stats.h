/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_SEND_DELAY_STATS_H_
#define VIDEO_SEND_DELAY_STATS_H_

#include <stddef.h>
#include <stdint.h>

#include <map>

#include "api/sequence_checker.h"
#include "api/units/timestamp.h"
#include "call/video_send_stream.h"
#include "modules/include/module_common_types_public.h"
#include "rtc_base/system/no_unique_address.h"
#include "rtc_base/thread_annotations.h"
#include "system_wrappers/include/clock.h"
#include "video/stats_counter.h"

namespace webrtc {

// Used to collect delay stats for video streams.
class SendDelayStats {
 public:
  explicit SendDelayStats(Clock* clock);
  ~SendDelayStats();

  // Adds the configured ssrcs for the rtp streams.
  // Stats will be calculated for these streams.
  void AddSsrcs(const VideoSendStream::Config& config);

  // Called when a packet is sent (leaving socket).
  bool OnSentPacket(int packet_id, Timestamp time);

  // Called when a packet is sent to the transport.
  void OnSendPacket(uint16_t packet_id, Timestamp capture_time, uint32_t ssrc);

 private:
  // Map holding sent packets (mapped by sequence number).
  struct SequenceNumberOlderThan {
    bool operator()(uint16_t seq1, uint16_t seq2) const {
      return IsNewerSequenceNumber(seq2, seq1);
    }
  };
  struct Packet {
    AvgCounter* send_delay;
    Timestamp capture_time;
    Timestamp send_time;
  };

  void UpdateHistograms();
  void RemoveOld(Timestamp now) RTC_RUN_ON(worker_sequence_checker_);

  Clock* const clock_;

  RTC_NO_UNIQUE_ADDRESS SequenceChecker worker_sequence_checker_;

  std::map<uint16_t, Packet, SequenceNumberOlderThan> packets_
      RTC_GUARDED_BY(worker_sequence_checker_);
  size_t num_old_packets_ RTC_GUARDED_BY(worker_sequence_checker_);
  size_t num_skipped_packets_ RTC_GUARDED_BY(worker_sequence_checker_);

  // Mapped by SSRC.
  std::map<uint32_t, AvgCounter> send_delay_counters_
      RTC_GUARDED_BY(worker_sequence_checker_);
};

}  // namespace webrtc
#endif  // VIDEO_SEND_DELAY_STATS_H_
