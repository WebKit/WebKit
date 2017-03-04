/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_AUDIO_AUDIO_STATE_H_
#define WEBRTC_AUDIO_AUDIO_STATE_H_

#include "webrtc/audio/audio_transport_proxy.h"
#include "webrtc/audio/scoped_voe_interface.h"
#include "webrtc/base/constructormagic.h"
#include "webrtc/base/criticalsection.h"
#include "webrtc/base/thread_checker.h"
#include "webrtc/call/audio_state.h"
#include "webrtc/voice_engine/include/voe_base.h"

namespace webrtc {
namespace internal {

class AudioState final : public webrtc::AudioState,
                         public webrtc::VoiceEngineObserver {
 public:
  explicit AudioState(const AudioState::Config& config);
  ~AudioState() override;

  VoiceEngine* voice_engine();

  rtc::scoped_refptr<AudioMixer> mixer();
  bool typing_noise_detected() const;

 private:
  // rtc::RefCountInterface implementation.
  int AddRef() const override;
  int Release() const override;

  // webrtc::VoiceEngineObserver implementation.
  void CallbackOnError(int channel_id, int err_code) override;

  rtc::ThreadChecker thread_checker_;
  rtc::ThreadChecker process_thread_checker_;
  const webrtc::AudioState::Config config_;

  // We hold one interface pointer to the VoE to make sure it is kept alive.
  ScopedVoEInterface<VoEBase> voe_base_;

  // The critical section isn't strictly needed in this case, but xSAN bots may
  // trigger on unprotected cross-thread access.
  rtc::CriticalSection crit_sect_;
  bool typing_noise_detected_ GUARDED_BY(crit_sect_) = false;

  // Reference count; implementation copied from rtc::RefCountedObject.
  mutable volatile int ref_count_ = 0;

  // Transports mixed audio from the mixer to the audio device and
  // recorded audio to the VoE AudioTransport.
  AudioTransportProxy audio_transport_proxy_;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(AudioState);
};
}  // namespace internal
}  // namespace webrtc

#endif  // WEBRTC_AUDIO_AUDIO_STATE_H_
