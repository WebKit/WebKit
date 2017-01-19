/*
 *  Copyright 2004 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <assert.h>
#include "webrtc/pc/audiomonitor.h"
#include "webrtc/pc/voicechannel.h"

namespace cricket {

const uint32_t MSG_MONITOR_POLL = 1;
const uint32_t MSG_MONITOR_START = 2;
const uint32_t MSG_MONITOR_STOP = 3;
const uint32_t MSG_MONITOR_SIGNAL = 4;

AudioMonitor::AudioMonitor(VoiceChannel *voice_channel,
                           rtc::Thread *monitor_thread) {
  voice_channel_ = voice_channel;
  monitoring_thread_ = monitor_thread;
  monitoring_ = false;
}

AudioMonitor::~AudioMonitor() {
  voice_channel_->worker_thread()->Clear(this);
  monitoring_thread_->Clear(this);
}

void AudioMonitor::Start(int milliseconds) {
  rate_ = milliseconds;
  if (rate_ < 100)
    rate_ = 100;
  voice_channel_->worker_thread()->Post(RTC_FROM_HERE, this, MSG_MONITOR_START);
}

void AudioMonitor::Stop() {
  voice_channel_->worker_thread()->Post(RTC_FROM_HERE, this, MSG_MONITOR_STOP);
}

void AudioMonitor::OnMessage(rtc::Message *message) {
  rtc::CritScope cs(&crit_);

  switch (message->message_id) {
  case MSG_MONITOR_START:
    assert(rtc::Thread::Current() == voice_channel_->worker_thread());
    if (!monitoring_) {
      monitoring_ = true;
      PollVoiceChannel();
    }
    break;

  case MSG_MONITOR_STOP:
    assert(rtc::Thread::Current() == voice_channel_->worker_thread());
    if (monitoring_) {
      monitoring_ = false;
      voice_channel_->worker_thread()->Clear(this);
    }
    break;

  case MSG_MONITOR_POLL:
    assert(rtc::Thread::Current() == voice_channel_->worker_thread());
    PollVoiceChannel();
    break;

  case MSG_MONITOR_SIGNAL:
    {
      assert(rtc::Thread::Current() == monitoring_thread_);
      AudioInfo info = audio_info_;
      crit_.Leave();
      SignalUpdate(this, info);
      crit_.Enter();
    }
    break;
  }
}

void AudioMonitor::PollVoiceChannel() {
  rtc::CritScope cs(&crit_);
  assert(rtc::Thread::Current() == voice_channel_->worker_thread());

  // Gather connection infos
  audio_info_.input_level = voice_channel_->GetInputLevel_w();
  audio_info_.output_level = voice_channel_->GetOutputLevel_w();
  voice_channel_->GetActiveStreams_w(&audio_info_.active_streams);

  // Signal the monitoring thread, start another poll timer
  monitoring_thread_->Post(RTC_FROM_HERE, this, MSG_MONITOR_SIGNAL);
  voice_channel_->worker_thread()->PostDelayed(RTC_FROM_HERE, rate_, this,
                                               MSG_MONITOR_POLL);
}

VoiceChannel *AudioMonitor::voice_channel() {
  return voice_channel_;
}

rtc::Thread *AudioMonitor::monitor_thread() {
  return monitoring_thread_;
}

}  // namespace cricket
