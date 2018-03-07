/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/turncustomizer.h"
#include "sdk/android/src/jni/jni_helpers.h"

namespace webrtc {
namespace jni {

JNI_FUNCTION_DECLARATION(void,
                         TurnCustomizer_nativeFreeTurnCustomizer,
                         JNIEnv* jni,
                         jclass,
                         jlong j_turn_customizer_pointer) {
  delete reinterpret_cast<TurnCustomizer*>(j_turn_customizer_pointer);
}

}  // namespace jni
}  // namespace webrtc
