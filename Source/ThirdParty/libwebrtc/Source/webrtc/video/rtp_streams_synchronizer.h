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

#ifndef VIDEO_RTP_STREAMS_SYNCHRONIZER_H_
#define VIDEO_RTP_STREAMS_SYNCHRONIZER_H_

#include <memory>

#include "modules/include/module.h"
#include "rtc_base/criticalsection.h"
#include "rtc_base/thread_checker.h"
#include "video/stream_synchronization.h"

namespace webrtc {

class Syncable;

namespace vcm {
class VideoReceiver;
}  // namespace vcm

class RtpStreamsSynchronizer : public Module {
 public:
  explicit RtpStreamsSynchronizer(Syncable* syncable_video);

  void ConfigureSync(Syncable* syncable_audio);

  // Implements Module.
  int64_t TimeUntilNextProcess() override;
  void Process() override;

  // Gets the sync offset between the current played out audio frame and the
  // video |frame|. Returns true on success, false otherwise.
  // The estimated frequency is the frequency used in the RTP to NTP timestamp
  // conversion.
  bool GetStreamSyncOffsetInMs(uint32_t timestamp,
                               int64_t render_time_ms,
                               int64_t* stream_offset_ms,
                               double* estimated_freq_khz) const;

 private:
  Syncable* syncable_video_;

  rtc::CriticalSection crit_;
  Syncable* syncable_audio_ RTC_GUARDED_BY(crit_);
  std::unique_ptr<StreamSynchronization> sync_ RTC_GUARDED_BY(crit_);
  StreamSynchronization::Measurements audio_measurement_ RTC_GUARDED_BY(crit_);
  StreamSynchronization::Measurements video_measurement_ RTC_GUARDED_BY(crit_);

  rtc::ThreadChecker process_thread_checker_;
  int64_t last_sync_time_ RTC_ACCESS_ON(&process_thread_checker_);
};

}  // namespace webrtc

#endif  // VIDEO_RTP_STREAMS_SYNCHRONIZER_H_
