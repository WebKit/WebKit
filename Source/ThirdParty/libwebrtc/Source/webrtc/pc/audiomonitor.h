/*
 *  Copyright 2004 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_AUDIOMONITOR_H_
#define PC_AUDIOMONITOR_H_

#include <vector>
#include <utility>

#include "p2p/base/port.h"
#include "rtc_base/sigslot.h"
#include "rtc_base/thread.h"

namespace cricket {

class VoiceChannel;

struct AudioInfo {
  int input_level;
  int output_level;
  typedef std::vector<std::pair<uint32_t, int> > StreamList;
  StreamList active_streams;  // ssrcs contributing to output_level
};

class AudioMonitor : public rtc::MessageHandler,
    public sigslot::has_slots<> {
 public:
  AudioMonitor(VoiceChannel* voice_channel, rtc::Thread *monitor_thread);
  ~AudioMonitor();

  void Start(int cms);
  void Stop();

  VoiceChannel* voice_channel();
  rtc::Thread *monitor_thread();

  sigslot::signal2<AudioMonitor*, const AudioInfo&> SignalUpdate;

 protected:
  void OnMessage(rtc::Message *message);
  void PollVoiceChannel();

  AudioInfo audio_info_;
  VoiceChannel* voice_channel_;
  rtc::Thread* monitoring_thread_;
  rtc::CriticalSection crit_;
  uint32_t rate_;
  bool monitoring_;
};

}  // namespace cricket

#endif  // PC_AUDIOMONITOR_H_
