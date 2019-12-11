/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "sdk/android/src/jni/pc/audio.h"

namespace webrtc {
namespace jni {

rtc::scoped_refptr<AudioDecoderFactory> CreateAudioDecoderFactory() {
  return nullptr;
}

rtc::scoped_refptr<AudioEncoderFactory> CreateAudioEncoderFactory() {
  return nullptr;
}

rtc::scoped_refptr<AudioProcessing> CreateAudioProcessing() {
  return nullptr;
}

}  // namespace jni
}  // namespace webrtc
