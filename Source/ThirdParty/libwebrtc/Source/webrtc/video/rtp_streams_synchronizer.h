/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// RtpStreamsSynchronizer is responsible for synchronization audio and video for
// a given voice engine channel and video receive stream.

#ifndef WEBRTC_VIDEO_RTP_STREAMS_SYNCHRONIZER_H_
#define WEBRTC_VIDEO_RTP_STREAMS_SYNCHRONIZER_H_

#include <memory>

#include "webrtc/base/criticalsection.h"
#include "webrtc/base/thread_checker.h"
#include "webrtc/modules/include/module.h"
#include "webrtc/video/rtp_stream_receiver.h"
#include "webrtc/video/stream_synchronization.h"

namespace webrtc {

class Clock;
class VideoFrame;
class VoEVideoSync;

namespace vcm {
class VideoReceiver;
}  // namespace vcm

class RtpStreamsSynchronizer : public Module {
 public:
  RtpStreamsSynchronizer(vcm::VideoReceiver* vcm,
                         RtpStreamReceiver* rtp_stream_receiver);

  void ConfigureSync(int voe_channel_id,
                     VoEVideoSync* voe_sync_interface);

  // Implements Module.
  int64_t TimeUntilNextProcess() override;
  void Process() override;

  // Gets the sync offset between the current played out audio frame and the
  // video |frame|. Returns true on success, false otherwise.
  // The estimated frequency is the frequency used in the RTP to NTP timestamp
  // conversion.
  bool GetStreamSyncOffsetInMs(const VideoFrame& frame,
                               int64_t* stream_offset_ms,
                               double* estimated_freq_khz) const;

 private:
  Clock* const clock_;
  vcm::VideoReceiver* const video_receiver_;
  RtpReceiver* const video_rtp_receiver_;
  RtpRtcp* const video_rtp_rtcp_;

  rtc::CriticalSection crit_;
  int voe_channel_id_ GUARDED_BY(crit_);
  VoEVideoSync* voe_sync_interface_ GUARDED_BY(crit_);
  RtpReceiver* audio_rtp_receiver_ GUARDED_BY(crit_);
  RtpRtcp* audio_rtp_rtcp_ GUARDED_BY(crit_);
  std::unique_ptr<StreamSynchronization> sync_ GUARDED_BY(crit_);
  StreamSynchronization::Measurements audio_measurement_ GUARDED_BY(crit_);
  StreamSynchronization::Measurements video_measurement_ GUARDED_BY(crit_);

  rtc::ThreadChecker process_thread_checker_;
  int64_t last_sync_time_ ACCESS_ON(&process_thread_checker_);
};

}  // namespace webrtc

#endif  // WEBRTC_VIDEO_RTP_STREAMS_SYNCHRONIZER_H_
