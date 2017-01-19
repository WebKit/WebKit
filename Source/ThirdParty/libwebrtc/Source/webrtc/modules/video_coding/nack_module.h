/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_NACK_MODULE_H_
#define WEBRTC_MODULES_VIDEO_CODING_NACK_MODULE_H_

#include <map>
#include <vector>
#include <set>

#include "webrtc/base/criticalsection.h"
#include "webrtc/base/thread_annotations.h"
#include "webrtc/modules/include/module.h"
#include "webrtc/modules/video_coding/include/video_coding_defines.h"
#include "webrtc/modules/video_coding/packet.h"
#include "webrtc/modules/video_coding/histogram.h"
#include "webrtc/modules/video_coding/sequence_number_util.h"
#include "webrtc/system_wrappers/include/clock.h"

namespace webrtc {

class NackModule : public Module {
 public:
  NackModule(Clock* clock,
             NackSender* nack_sender,
             KeyFrameRequestSender* keyframe_request_sender);

  int OnReceivedPacket(const VCMPacket& packet);
  void ClearUpTo(uint16_t seq_num);
  void UpdateRtt(int64_t rtt_ms);
  void Clear();
  void Stop();

  // Module implementation
  int64_t TimeUntilNextProcess() override;
  void Process() override;

 private:
  // Which fields to consider when deciding which packet to nack in
  // GetNackBatch.
  enum NackFilterOptions { kSeqNumOnly, kTimeOnly, kSeqNumAndTime };

  // This class holds the sequence number of the packet that is in the nack list
  // as well as the meta data about when it should be nacked and how many times
  // we have tried to nack this packet.
  struct NackInfo {
    NackInfo();
    NackInfo(uint16_t seq_num, uint16_t send_at_seq_num);

    uint16_t seq_num;
    uint16_t send_at_seq_num;
    int64_t sent_at_time;
    int retries;
  };
  void AddPacketsToNack(uint16_t seq_num_start, uint16_t seq_num_end)
      EXCLUSIVE_LOCKS_REQUIRED(crit_);

  // Removes packets from the nack list until the next keyframe. Returns true
  // if packets were removed.
  bool RemovePacketsUntilKeyFrame() EXCLUSIVE_LOCKS_REQUIRED(crit_);
  std::vector<uint16_t> GetNackBatch(NackFilterOptions options)
      EXCLUSIVE_LOCKS_REQUIRED(crit_);

  // Update the reordering distribution.
  void UpdateReorderingStatistics(uint16_t seq_num)
      EXCLUSIVE_LOCKS_REQUIRED(crit_);

  // Returns how many packets we have to wait in order to receive the packet
  // with probability |probabilty| or higher.
  int WaitNumberOfPackets(float probability) const
      EXCLUSIVE_LOCKS_REQUIRED(crit_);

  rtc::CriticalSection crit_;
  Clock* const clock_;
  NackSender* const nack_sender_;
  KeyFrameRequestSender* const keyframe_request_sender_;

  std::map<uint16_t, NackInfo, DescendingSeqNumComp<uint16_t>> nack_list_
      GUARDED_BY(crit_);
  std::set<uint16_t, DescendingSeqNumComp<uint16_t>> keyframe_list_
      GUARDED_BY(crit_);
  video_coding::Histogram reordering_histogram_ GUARDED_BY(crit_);
  bool running_ GUARDED_BY(crit_);
  bool initialized_ GUARDED_BY(crit_);
  int64_t rtt_ms_ GUARDED_BY(crit_);
  uint16_t newest_seq_num_ GUARDED_BY(crit_);
  int64_t next_process_time_ms_ GUARDED_BY(crit_);
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_CODING_NACK_MODULE_H_
