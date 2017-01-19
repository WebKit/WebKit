/*
 *  Copyright 2005 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Class to collect statistics from a media channel

#ifndef WEBRTC_PC_MEDIAMONITOR_H_
#define WEBRTC_PC_MEDIAMONITOR_H_

#include "webrtc/base/criticalsection.h"
#include "webrtc/base/sigslot.h"
#include "webrtc/base/thread.h"
#include "webrtc/base/thread_annotations.h"
#include "webrtc/media/base/mediachannel.h"

namespace cricket {

// The base MediaMonitor class, independent of voice and video.
class MediaMonitor : public rtc::MessageHandler,
    public sigslot::has_slots<> {
 public:
  MediaMonitor(rtc::Thread* worker_thread,
               rtc::Thread* monitor_thread);
  ~MediaMonitor();

  void Start(uint32_t milliseconds);
  void Stop();

 protected:
  void OnMessage(rtc::Message *message);
  void PollMediaChannel();
  virtual void GetStats() = 0;
  virtual void Update() = 0;

  rtc::CriticalSection crit_;
  rtc::Thread* worker_thread_;
  rtc::Thread* monitor_thread_;
  bool monitoring_;
  uint32_t rate_;
};

// Templatized MediaMonitor that can deal with different kinds of media.
template<class MC, class MI>
class MediaMonitorT : public MediaMonitor {
 public:
  MediaMonitorT(MC* media_channel, rtc::Thread* worker_thread,
                rtc::Thread* monitor_thread)
      : MediaMonitor(worker_thread, monitor_thread),
        media_channel_(media_channel) {}
  sigslot::signal2<MC*, const MI&> SignalUpdate;

 protected:
  // These routines assume the crit_ lock is held by the calling thread.
  virtual void GetStats() {
    media_info_.Clear();
    media_channel_->GetStats(&media_info_);
  }
  virtual void Update() EXCLUSIVE_LOCKS_REQUIRED(crit_) {
    MI stats(media_info_);
    crit_.Leave();
    SignalUpdate(media_channel_, stats);
    crit_.Enter();
  }

 private:
  MC* media_channel_;
  MI media_info_;
};

typedef MediaMonitorT<VoiceMediaChannel, VoiceMediaInfo> VoiceMediaMonitor;
typedef MediaMonitorT<VideoMediaChannel, VideoMediaInfo> VideoMediaMonitor;
typedef MediaMonitorT<DataMediaChannel, DataMediaInfo> DataMediaMonitor;

}  // namespace cricket

#endif  // WEBRTC_PC_MEDIAMONITOR_H_
