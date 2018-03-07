/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef AUDIO_SCOPED_VOE_INTERFACE_H_
#define AUDIO_SCOPED_VOE_INTERFACE_H_

#include "rtc_base/checks.h"

namespace webrtc {

class VoiceEngine;

namespace internal {

// Utility template for obtaining and holding a reference to a VoiceEngine
// interface and making sure it is released when this object goes out of scope.
template<class T> class ScopedVoEInterface {
 public:
  explicit ScopedVoEInterface(webrtc::VoiceEngine* e)
      : ptr_(T::GetInterface(e)) {
    RTC_DCHECK(ptr_);
  }
  ~ScopedVoEInterface() {
    if (ptr_) {
      ptr_->Release();
    }
  }
  T* operator->() {
    RTC_DCHECK(ptr_);
    return ptr_;
  }
 private:
  T* ptr_;
};
}  // namespace internal
}  // namespace webrtc

#endif  // AUDIO_SCOPED_VOE_INTERFACE_H_
