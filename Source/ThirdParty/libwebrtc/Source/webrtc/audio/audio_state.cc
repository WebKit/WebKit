/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/audio/audio_state.h"

#include "webrtc/base/atomicops.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/voice_engine/include/voe_errors.h"

namespace webrtc {
namespace internal {

AudioState::AudioState(const AudioState::Config& config)
    : config_(config), voe_base_(config.voice_engine) {
  process_thread_checker_.DetachFromThread();
  // Only one AudioState should be created per VoiceEngine.
  RTC_CHECK(voe_base_->RegisterVoiceEngineObserver(*this) != -1);
}

AudioState::~AudioState() {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  voe_base_->DeRegisterVoiceEngineObserver();
}

VoiceEngine* AudioState::voice_engine() {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  return config_.voice_engine;
}

bool AudioState::typing_noise_detected() const {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  rtc::CritScope lock(&crit_sect_);
  return typing_noise_detected_;
}

// Reference count; implementation copied from rtc::RefCountedObject.
int AudioState::AddRef() const {
  return rtc::AtomicOps::Increment(&ref_count_);
}

// Reference count; implementation copied from rtc::RefCountedObject.
int AudioState::Release() const {
  int count = rtc::AtomicOps::Decrement(&ref_count_);
  if (!count) {
    delete this;
  }
  return count;
}

void AudioState::CallbackOnError(int channel_id, int err_code) {
  RTC_DCHECK(process_thread_checker_.CalledOnValidThread());

  // All call sites in VoE, as of this writing, specify -1 as channel_id.
  RTC_DCHECK(channel_id == -1);
  LOG(LS_INFO) << "VoiceEngine error " << err_code << " reported on channel "
               << channel_id << ".";
  if (err_code == VE_TYPING_NOISE_WARNING) {
    rtc::CritScope lock(&crit_sect_);
    typing_noise_detected_ = true;
  } else if (err_code == VE_TYPING_NOISE_OFF_WARNING) {
    rtc::CritScope lock(&crit_sect_);
    typing_noise_detected_ = false;
  }
}
}  // namespace internal

rtc::scoped_refptr<AudioState> AudioState::Create(
    const AudioState::Config& config) {
  return rtc::scoped_refptr<AudioState>(new internal::AudioState(config));
}
}  // namespace webrtc
