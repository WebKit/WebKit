/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef WEBRTC_CALL_AUDIO_STATE_H_
#define WEBRTC_CALL_AUDIO_STATE_H_

#include "webrtc/api/audio/audio_mixer.h"
#include "webrtc/base/refcount.h"
#include "webrtc/base/scoped_ref_ptr.h"

namespace webrtc {

class VoiceEngine;

// WORK IN PROGRESS
// This class is under development and is not yet intended for for use outside
// of WebRtc/Libjingle. Please use the VoiceEngine API instead.
// See: https://bugs.chromium.org/p/webrtc/issues/detail?id=4690

// AudioState holds the state which must be shared between multiple instances of
// webrtc::Call for audio processing purposes.
class AudioState : public rtc::RefCountInterface {
 public:
  struct Config {
    // VoiceEngine used for audio streams and audio/video synchronization.
    // AudioState will tickle the VoE refcount to keep it alive for as long as
    // the AudioState itself.
    VoiceEngine* voice_engine = nullptr;

    // The audio mixer connected to active receive streams. One per
    // AudioState.
    rtc::scoped_refptr<AudioMixer> audio_mixer;
  };

  // TODO(solenberg): Replace scoped_refptr with shared_ptr once we can use it.
  static rtc::scoped_refptr<AudioState> Create(
      const AudioState::Config& config);

  virtual ~AudioState() {}
};
}  // namespace webrtc

#endif  // WEBRTC_CALL_AUDIO_STATE_H_
