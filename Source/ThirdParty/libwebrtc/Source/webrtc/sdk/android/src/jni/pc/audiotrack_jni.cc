/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/mediastreaminterface.h"
#include "sdk/android/src/jni/jni_helpers.h"

namespace webrtc {
namespace jni {

JNI_FUNCTION_DECLARATION(void,
                         AudioTrack_nativeSetVolume,
                         JNIEnv*,
                         jclass,
                         jlong j_p,
                         jdouble volume) {
  rtc::scoped_refptr<AudioSourceInterface> source(
      reinterpret_cast<AudioTrackInterface*>(j_p)->GetSource());
  source->SetVolume(volume);
}

}  // namespace jni
}  // namespace webrtc
