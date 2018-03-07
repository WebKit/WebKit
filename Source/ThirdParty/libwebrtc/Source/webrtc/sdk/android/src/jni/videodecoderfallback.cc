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

#include "media/engine/videodecodersoftwarefallbackwrapper.h"
#include "sdk/android/src/jni/jni_helpers.h"
#include "sdk/android/src/jni/wrappednativecodec.h"

namespace webrtc {
namespace jni {

JNI_FUNCTION_DECLARATION(jlong,
                         VideoDecoderFallback_createNativeDecoder,
                         JNIEnv* jni,
                         jclass,
                         jobject j_fallback_decoder,
                         jobject j_primary_decoder) {
  std::unique_ptr<VideoDecoder> fallback_decoder =
      JavaToNativeVideoDecoder(jni, j_fallback_decoder);
  std::unique_ptr<VideoDecoder> primary_decoder =
      JavaToNativeVideoDecoder(jni, j_primary_decoder);

  VideoDecoderSoftwareFallbackWrapper* nativeWrapper =
      new VideoDecoderSoftwareFallbackWrapper(std::move(fallback_decoder),
                                              std::move(primary_decoder));

  return jlongFromPointer(nativeWrapper);
}

}  // namespace jni
}  // namespace webrtc
