/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#if defined(WEBRTC_ANDROID)
#include "modules/audio_device/android/audio_device_template.h"
#include "modules/audio_device/android/audio_record_jni.h"
#include "modules/audio_device/android/audio_track_jni.h"
#endif

#include "modules/audio_coding/include/audio_coding_module.h"
#include "rtc_base/checks.h"
#include "voice_engine/channel_proxy.h"
#include "voice_engine/voice_engine_impl.h"

namespace webrtc {

// Counter to be ensure that we can add a correct ID in all static trace
// methods. It is not the nicest solution, especially not since we already
// have a counter in VoEBaseImpl. In other words, there is room for
// improvement here.
static int32_t gVoiceEngineInstanceCounter = 0;

VoiceEngine* GetVoiceEngine() {
  VoiceEngineImpl* self = new VoiceEngineImpl();
  if (self != NULL) {
    self->AddRef();  // First reference.  Released in VoiceEngine::Delete.
    gVoiceEngineInstanceCounter++;
  }
  return self;
}

int VoiceEngineImpl::AddRef() {
  return ++_ref_count;
}

// This implements the Release() method for all the inherited interfaces.
int VoiceEngineImpl::Release() {
  int new_ref = --_ref_count;
  assert(new_ref >= 0);
  if (new_ref == 0) {
    // Clear any pointers before starting destruction. Otherwise worker-
    // threads will still have pointers to a partially destructed object.
    // Example: AudioDeviceBuffer::RequestPlayoutData() can access a
    // partially deconstructed |_ptrCbAudioTransport| during destruction
    // if we don't call Terminate here.
    Terminate();
    delete this;
  }

  return new_ref;
}

std::unique_ptr<voe::ChannelProxy> VoiceEngineImpl::GetChannelProxy(
    int channel_id) {
  RTC_DCHECK(channel_id >= 0);
  rtc::CritScope cs(crit_sec());
  return std::unique_ptr<voe::ChannelProxy>(
      new voe::ChannelProxy(channel_manager().GetChannel(channel_id)));
}

VoiceEngine* VoiceEngine::Create() {
  return GetVoiceEngine();
}

bool VoiceEngine::Delete(VoiceEngine*& voiceEngine) {
  if (voiceEngine == NULL)
    return false;

  VoiceEngineImpl* s = static_cast<VoiceEngineImpl*>(voiceEngine);
  s->Release();
  voiceEngine = NULL;
  return true;
}
}  // namespace webrtc
