/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/voice_engine/test/auto_test/fakes/loudest_filter.h"

#include "webrtc/base/checks.h"

namespace voetest {

void LoudestFilter::RemoveTimeoutStreams(int64_t time_ms) {
  auto it = stream_levels_.begin();
  while (it != stream_levels_.end()) {
    if (rtc::TimeDiff(time_ms, it->second.last_time_ms) > kStreamTimeOutMs) {
      stream_levels_.erase(it++);
    } else {
      ++it;
    }
  }
}

unsigned int LoudestFilter::FindQuietestStream() {
  int quietest_level = kInvalidAudioLevel;
  unsigned int quietest_ssrc = 0;
  for (auto stream : stream_levels_) {
    // A smaller value if audio level corresponds to a louder sound.
    if (quietest_level == kInvalidAudioLevel ||
        stream.second.audio_level > quietest_level) {
      quietest_level = stream.second.audio_level;
      quietest_ssrc = stream.first;
    }
  }
  return quietest_ssrc;
}

bool LoudestFilter::ForwardThisPacket(const webrtc::RTPHeader& rtp_header) {
  int64_t time_now_ms = rtc::TimeMillis();
  RemoveTimeoutStreams(time_now_ms);

  int source_ssrc = rtp_header.ssrc;
  int audio_level = rtp_header.extension.hasAudioLevel ?
      rtp_header.extension.audioLevel : kInvalidAudioLevel;

  if (audio_level == kInvalidAudioLevel) {
    // Always forward streams with unknown audio level, and don't keep their
    // states.
    return true;
  }

  auto it = stream_levels_.find(source_ssrc);
  if (it != stream_levels_.end()) {
    // Stream has been forwarded. Update and continue to forward.
    it->second.audio_level = audio_level;
    it->second.last_time_ms = time_now_ms;
    return true;
  }

  if (stream_levels_.size() < kMaxMixSize) {
    stream_levels_[source_ssrc].Set(audio_level, time_now_ms);
    return true;
  }

  unsigned int quietest_ssrc = FindQuietestStream();
  RTC_CHECK_NE(0u, quietest_ssrc);
  // A smaller value if audio level corresponds to a louder sound.
  if (audio_level < stream_levels_[quietest_ssrc].audio_level) {
    stream_levels_.erase(quietest_ssrc);
    stream_levels_[source_ssrc].Set(audio_level, time_now_ms);
    return true;
  }
  return false;
}

}  // namespace voetest

