/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_SEND_DELAY_STATS_H_
#define WEBRTC_VIDEO_SEND_DELAY_STATS_H_

#include <map>
#include <memory>
#include <set>

#include "webrtc/base/criticalsection.h"
#include "webrtc/base/thread_annotations.h"
#include "webrtc/common_types.h"
#include "webrtc/modules/include/module_common_types.h"
#include "webrtc/system_wrappers/include/clock.h"
#include "webrtc/video/stats_counter.h"
#include "webrtc/video_send_stream.h"

namespace webrtc {

class SendDelayStats : public SendPacketObserver {
 public:
  explicit SendDelayStats(Clock* clock);
  virtual ~SendDelayStats();

  // Adds the configured ssrcs for the rtp streams.
  // Stats will be calculated for these streams.
  void AddSsrcs(const VideoSendStream::Config& config);

  // Called when a packet is sent (leaving socket).
  bool OnSentPacket(int packet_id, int64_t time_ms);

 protected:
  // From SendPacketObserver.
  // Called when a packet is sent to the transport.
  void OnSendPacket(uint16_t packet_id,
                    int64_t capture_time_ms,
                    uint32_t ssrc) override;

 private:
  // Map holding sent packets (mapped by sequence number).
  struct SequenceNumberOlderThan {
    bool operator()(uint16_t seq1, uint16_t seq2) const {
      return IsNewerSequenceNumber(seq2, seq1);
    }
  };
  struct Packet {
    Packet(uint32_t ssrc, int64_t capture_time_ms, int64_t send_time_ms)
        : ssrc(ssrc),
          capture_time_ms(capture_time_ms),
          send_time_ms(send_time_ms) {}
    uint32_t ssrc;
    int64_t capture_time_ms;
    int64_t send_time_ms;
  };
  typedef std::map<uint16_t, Packet, SequenceNumberOlderThan> PacketMap;

  void UpdateHistograms();
  void RemoveOld(int64_t now, PacketMap* packets)
      EXCLUSIVE_LOCKS_REQUIRED(crit_);
  AvgCounter* GetSendDelayCounter(uint32_t ssrc)
      EXCLUSIVE_LOCKS_REQUIRED(crit_);

  Clock* const clock_;
  rtc::CriticalSection crit_;

  PacketMap packets_ GUARDED_BY(crit_);
  size_t num_old_packets_ GUARDED_BY(crit_);
  size_t num_skipped_packets_ GUARDED_BY(crit_);

  std::set<uint32_t> ssrcs_ GUARDED_BY(crit_);

  // Mapped by SSRC.
  std::map<uint32_t, std::unique_ptr<AvgCounter>> send_delay_counters_
      GUARDED_BY(crit_);
};

}  // namespace webrtc
#endif  // WEBRTC_VIDEO_SEND_DELAY_STATS_H_
