/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef CALL_AUDIO_STATE_H_
#define CALL_AUDIO_STATE_H_

#include "api/audio/audio_mixer.h"
#include "rtc_base/refcount.h"
#include "rtc_base/scoped_ref_ptr.h"

namespace webrtc {

class AudioProcessing;
class AudioTransport;
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

    // The audio processing module.
    rtc::scoped_refptr<webrtc::AudioProcessing> audio_processing;
  };

  virtual AudioProcessing* audio_processing() = 0;
  virtual AudioTransport* audio_transport() = 0;

  // Enable/disable playout of the audio channels. Enabled by default.
  // This will stop playout of the underlying audio device but start a task
  // which will poll for audio data every 10ms to ensure that audio processing
  // happens and the audio stats are updated.
  virtual void SetPlayout(bool enabled) = 0;

  // Enable/disable recording of the audio channels. Enabled by default.
  // This will stop recording of the underlying audio device and no audio
  // packets will be encoded or transmitted.
  virtual void SetRecording(bool enabled) = 0;

  // TODO(solenberg): Replace scoped_refptr with shared_ptr once we can use it.
  static rtc::scoped_refptr<AudioState> Create(
      const AudioState::Config& config);

  virtual ~AudioState() {}
};
}  // namespace webrtc

#endif  // CALL_AUDIO_STATE_H_
