/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef AUDIO_AUDIO_STATE_H_
#define AUDIO_AUDIO_STATE_H_

#include <memory>

#include "audio/audio_transport_proxy.h"
#include "audio/null_audio_poller.h"
#include "audio/scoped_voe_interface.h"
#include "call/audio_state.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/criticalsection.h"
#include "rtc_base/refcount.h"
#include "rtc_base/thread_checker.h"
#include "voice_engine/include/voe_base.h"

namespace webrtc {
namespace internal {

class AudioState final : public webrtc::AudioState {
 public:
  explicit AudioState(const AudioState::Config& config);
  ~AudioState() override;

  AudioProcessing* audio_processing() override {
    RTC_DCHECK(config_.audio_processing);
    return config_.audio_processing.get();
  }
  AudioTransport* audio_transport() override {
    return &audio_transport_proxy_;
  }

  void SetPlayout(bool enabled) override;
  void SetRecording(bool enabled) override;

  VoiceEngine* voice_engine();
  rtc::scoped_refptr<AudioMixer> mixer();
  bool typing_noise_detected() const;

 private:
  // rtc::RefCountInterface implementation.
  void AddRef() const override;
  rtc::RefCountReleaseStatus Release() const override;

  rtc::ThreadChecker thread_checker_;
  rtc::ThreadChecker process_thread_checker_;
  const webrtc::AudioState::Config config_;

  // We hold one interface pointer to the VoE to make sure it is kept alive.
  ScopedVoEInterface<VoEBase> voe_base_;

  // Reference count; implementation copied from rtc::RefCountedObject.
  // TODO(nisse): Use RefCountedObject or RefCountedBase instead.
  mutable volatile int ref_count_ = 0;

  // Transports mixed audio from the mixer to the audio device and
  // recorded audio to the VoE AudioTransport.
  AudioTransportProxy audio_transport_proxy_;

  // Null audio poller is used to continue polling the audio streams if audio
  // playout is disabled so that audio processing still happens and the audio
  // stats are still updated.
  std::unique_ptr<NullAudioPoller> null_audio_poller_;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(AudioState);
};
}  // namespace internal
}  // namespace webrtc

#endif  // AUDIO_AUDIO_STATE_H_
