/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "logging/rtc_event_log/events/rtc_event_audio_playout.h"

namespace webrtc {

RtcEventAudioPlayout::RtcEventAudioPlayout(uint32_t ssrc) : ssrc_(ssrc) {}

RtcEvent::Type RtcEventAudioPlayout::GetType() const {
  return RtcEvent::Type::AudioPlayout;
}

bool RtcEventAudioPlayout::IsConfigEvent() const {
  return false;
}

}  // namespace webrtc
