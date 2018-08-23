/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_RECEIVER_H_
#define MODULES_VIDEO_CODING_RECEIVER_H_

#include <memory>
#include <vector>

#include "modules/video_coding/include/video_coding.h"
#include "modules/video_coding/include/video_coding_defines.h"
#include "modules/video_coding/jitter_buffer.h"
#include "modules/video_coding/packet.h"
#include "modules/video_coding/timing.h"
#include "rtc_base/criticalsection.h"

namespace webrtc {

class Clock;
class VCMEncodedFrame;

class VCMReceiver {
 public:
  // Constructor for current interface, will be removed when the
  // new jitter buffer is in place.
  VCMReceiver(VCMTiming* timing, Clock* clock, EventFactory* event_factory);

  // Create method for the new jitter buffer.
  VCMReceiver(VCMTiming* timing,
              Clock* clock,
              EventFactory* event_factory,
              NackSender* nack_sender,
              KeyFrameRequestSender* keyframe_request_sender);

  // Using this constructor, you can specify a different event factory for the
  // jitter buffer. Useful for unit tests when you want to simulate incoming
  // packets, in which case the jitter buffer's wait event is different from
  // that of VCMReceiver itself.
  //
  // Constructor for current interface, will be removed when the
  // new jitter buffer is in place.
  VCMReceiver(VCMTiming* timing,
              Clock* clock,
              std::unique_ptr<EventWrapper> receiver_event,
              std::unique_ptr<EventWrapper> jitter_buffer_event);

  // Create method for the new jitter buffer.
  VCMReceiver(VCMTiming* timing,
              Clock* clock,
              std::unique_ptr<EventWrapper> receiver_event,
              std::unique_ptr<EventWrapper> jitter_buffer_event,
              NackSender* nack_sender,
              KeyFrameRequestSender* keyframe_request_sender);

  ~VCMReceiver();

  void Reset();
  void UpdateRtt(int64_t rtt);
  int32_t InsertPacket(const VCMPacket& packet);
  VCMEncodedFrame* FrameForDecoding(uint16_t max_wait_time_ms,
                                    bool prefer_late_decoding);
  void ReleaseFrame(VCMEncodedFrame* frame);
  void ReceiveStatistics(uint32_t* bitrate, uint32_t* framerate);

  // NACK.
  void SetNackMode(VCMNackMode nackMode,
                   int64_t low_rtt_nack_threshold_ms,
                   int64_t high_rtt_nack_threshold_ms);
  void SetNackSettings(size_t max_nack_list_size,
                       int max_packet_age_to_nack,
                       int max_incomplete_time_ms);
  VCMNackMode NackMode() const;
  std::vector<uint16_t> NackList(bool* request_key_frame);

  // Decoding with errors.
  void SetDecodeErrorMode(VCMDecodeErrorMode decode_error_mode);
  VCMDecodeErrorMode DecodeErrorMode() const;

  void RegisterStatsCallback(VCMReceiveStatisticsCallback* callback);

  void TriggerDecoderShutdown();

 private:
  rtc::CriticalSection crit_sect_;
  Clock* const clock_;
  VCMJitterBuffer jitter_buffer_;
  VCMTiming* timing_;
  std::unique_ptr<EventWrapper> render_wait_event_;
  int max_video_delay_ms_;
};

}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_RECEIVER_H_
