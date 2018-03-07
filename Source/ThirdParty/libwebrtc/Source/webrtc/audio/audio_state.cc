/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "audio/audio_state.h"

#include "modules/audio_device/include/audio_device.h"
#include "rtc_base/atomicops.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/ptr_util.h"
#include "rtc_base/thread.h"
#include "voice_engine/transmit_mixer.h"

namespace webrtc {
namespace internal {

AudioState::AudioState(const AudioState::Config& config)
    : config_(config),
      voe_base_(config.voice_engine),
      audio_transport_proxy_(voe_base_->audio_transport(),
                             config_.audio_processing.get(),
                             config_.audio_mixer) {
  process_thread_checker_.DetachFromThread();
  RTC_DCHECK(config_.audio_mixer);
}

AudioState::~AudioState() {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
}

VoiceEngine* AudioState::voice_engine() {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  return config_.voice_engine;
}

rtc::scoped_refptr<AudioMixer> AudioState::mixer() {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  return config_.audio_mixer;
}

bool AudioState::typing_noise_detected() const {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  // TODO(solenberg): Remove const_cast once AudioState owns transmit mixer
  //                  functionality.
  voe::TransmitMixer* transmit_mixer =
      const_cast<AudioState*>(this)->voe_base_->transmit_mixer();
  return transmit_mixer->typing_noise_detected();
}

void AudioState::SetPlayout(bool enabled) {
  RTC_LOG(INFO) << "SetPlayout(" << enabled << ")";
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  const bool currently_enabled = (null_audio_poller_ == nullptr);
  if (enabled == currently_enabled) {
    return;
  }
  VoEBase* const voe = VoEBase::GetInterface(voice_engine());
  RTC_DCHECK(voe);
  if (enabled) {
    null_audio_poller_.reset();
  }
  // Will stop/start playout of the underlying device, if necessary, and
  // remember the setting for when it receives subsequent calls of
  // StartPlayout.
  voe->SetPlayout(enabled);
  if (!enabled) {
    null_audio_poller_ =
        rtc::MakeUnique<NullAudioPoller>(&audio_transport_proxy_);
  }
  voe->Release();
}

void AudioState::SetRecording(bool enabled) {
  RTC_LOG(INFO) << "SetRecording(" << enabled << ")";
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  // TODO(henrika): keep track of state as in SetPlayout().
  VoEBase* const voe = VoEBase::GetInterface(voice_engine());
  RTC_DCHECK(voe);
  // Will stop/start recording of the underlying device, if necessary, and
  // remember the setting for when it receives subsequent calls of
  // StartPlayout.
  voe->SetRecording(enabled);
  voe->Release();
}

// Reference count; implementation copied from rtc::RefCountedObject.
void AudioState::AddRef() const {
  rtc::AtomicOps::Increment(&ref_count_);
}

// Reference count; implementation copied from rtc::RefCountedObject.
rtc::RefCountReleaseStatus AudioState::Release() const {
  if (rtc::AtomicOps::Decrement(&ref_count_) == 0) {
    delete this;
    return rtc::RefCountReleaseStatus::kDroppedLastRef;
  }
  return rtc::RefCountReleaseStatus::kOtherRefsRemained;
}
}  // namespace internal

rtc::scoped_refptr<AudioState> AudioState::Create(
    const AudioState::Config& config) {
  return rtc::scoped_refptr<AudioState>(new internal::AudioState(config));
}
}  // namespace webrtc
