/*
 *  Copyright (c) 2004 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MEDIA_ENGINE_WEBRTCVOE_H_
#define WEBRTC_MEDIA_ENGINE_WEBRTCVOE_H_

#include <memory>

#include "webrtc/base/common.h"
#include "webrtc/media/engine/webrtccommon.h"

#include "webrtc/common_types.h"
#include "webrtc/modules/audio_device/include/audio_device.h"
#include "webrtc/voice_engine/include/voe_audio_processing.h"
#include "webrtc/voice_engine/include/voe_base.h"
#include "webrtc/voice_engine/include/voe_codec.h"
#include "webrtc/voice_engine/include/voe_errors.h"
#include "webrtc/voice_engine/include/voe_hardware.h"
#include "webrtc/voice_engine/include/voe_volume_control.h"

namespace cricket {
// automatically handles lifetime of WebRtc VoiceEngine
class scoped_voe_engine {
 public:
  explicit scoped_voe_engine(webrtc::VoiceEngine* e) : ptr(e) {}
  // VERIFY, to ensure that there are no leaks at shutdown
  ~scoped_voe_engine() { if (ptr) VERIFY(webrtc::VoiceEngine::Delete(ptr)); }
  // Releases the current pointer.
  void reset() {
    if (ptr) {
      VERIFY(webrtc::VoiceEngine::Delete(ptr));
      ptr = NULL;
    }
  }
  webrtc::VoiceEngine* get() const { return ptr; }
 private:
  webrtc::VoiceEngine* ptr;
};

// unique_ptr-like class to handle obtaining and releasing WebRTC interface
// pointers.
template<class T>
class scoped_voe_ptr {
 public:
  explicit scoped_voe_ptr(const scoped_voe_engine& e)
      : ptr(T::GetInterface(e.get())) {}
  explicit scoped_voe_ptr(T* p) : ptr(p) {}
  ~scoped_voe_ptr() { if (ptr) ptr->Release(); }
  T* operator->() const { return ptr; }
  T* get() const { return ptr; }

  // Releases the current pointer.
  void reset() {
    if (ptr) {
      ptr->Release();
      ptr = NULL;
    }
  }

 private:
  T* ptr;
};

// Utility class for aggregating the various WebRTC interface.
// Fake implementations can also be injected for testing.
class VoEWrapper {
 public:
  VoEWrapper()
      : engine_(webrtc::VoiceEngine::Create()), processing_(engine_),
        base_(engine_), codec_(engine_), hw_(engine_),
        volume_(engine_) {
  }
  VoEWrapper(webrtc::VoEAudioProcessing* processing,
             webrtc::VoEBase* base,
             webrtc::VoECodec* codec,
             webrtc::VoEHardware* hw,
             webrtc::VoEVolumeControl* volume)
      : engine_(NULL),
        processing_(processing),
        base_(base),
        codec_(codec),
        hw_(hw),
        volume_(volume) {
  }
  ~VoEWrapper() {}
  webrtc::VoiceEngine* engine() const { return engine_.get(); }
  webrtc::VoEAudioProcessing* processing() const { return processing_.get(); }
  webrtc::VoEBase* base() const { return base_.get(); }
  webrtc::VoECodec* codec() const { return codec_.get(); }
  webrtc::VoEHardware* hw() const { return hw_.get(); }
  webrtc::VoEVolumeControl* volume() const { return volume_.get(); }
  int error() { return base_->LastError(); }

 private:
  scoped_voe_engine engine_;
  scoped_voe_ptr<webrtc::VoEAudioProcessing> processing_;
  scoped_voe_ptr<webrtc::VoEBase> base_;
  scoped_voe_ptr<webrtc::VoECodec> codec_;
  scoped_voe_ptr<webrtc::VoEHardware> hw_;
  scoped_voe_ptr<webrtc::VoEVolumeControl> volume_;
};
}  // namespace cricket

#endif  // WEBRTC_MEDIA_ENGINE_WEBRTCVOE_H_
