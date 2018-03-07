/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>

#include "modules/audio_processing/include/audio_processing.h"
#include "rtc_base/scoped_ref_ptr.h"
#include "sdk/android/src/jni/jni_helpers.h"

namespace webrtc {
namespace jni {

JNI_FUNCTION_DECLARATION(
    jlong,
    DefaultAudioProcessingFactory_nativeCreateAudioProcessing,
    JNIEnv*,
    jclass,
    jlong native_post_processor) {
  std::unique_ptr<PostProcessing> post_processor(
      reinterpret_cast<PostProcessing*>(native_post_processor));
  rtc::scoped_refptr<AudioProcessing> audio_processing =
      AudioProcessing::Create(webrtc::Config(), std::move(post_processor),
                              nullptr /* echo_control_factory */,
                              nullptr /* beamformer */);
  return jlongFromPointer(audio_processing.release());
}

}  // namespace jni
}  // namespace webrtc
