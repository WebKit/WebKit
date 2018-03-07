/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <jni.h>

#include "media/engine/videoencodersoftwarefallbackwrapper.h"
#include "sdk/android/src/jni/jni_helpers.h"
#include "sdk/android/src/jni/wrappednativecodec.h"

namespace webrtc {
namespace jni {

JNI_FUNCTION_DECLARATION(jlong,
                         VideoEncoderFallback_createNativeEncoder,
                         JNIEnv* jni,
                         jclass,
                         jobject j_fallback_encoder,
                         jobject j_primary_encoder) {
  std::unique_ptr<VideoEncoder> fallback_encoder =
      JavaToNativeVideoEncoder(jni, j_fallback_encoder);
  std::unique_ptr<VideoEncoder> primary_encoder =
      JavaToNativeVideoEncoder(jni, j_primary_encoder);

  VideoEncoderSoftwareFallbackWrapper* nativeWrapper =
      new VideoEncoderSoftwareFallbackWrapper(std::move(fallback_encoder),
                                              std::move(primary_encoder));

  return jlongFromPointer(nativeWrapper);
}

}  // namespace jni
}  // namespace webrtc
