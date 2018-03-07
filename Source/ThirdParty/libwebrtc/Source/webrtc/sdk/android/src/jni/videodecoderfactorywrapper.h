/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SDK_ANDROID_SRC_JNI_VIDEODECODERFACTORYWRAPPER_H_
#define SDK_ANDROID_SRC_JNI_VIDEODECODERFACTORYWRAPPER_H_

#include <jni.h>

#include "api/video_codecs/video_decoder_factory.h"
#include "sdk/android/src/jni/jni_helpers.h"

namespace webrtc {
namespace jni {

// Wrapper for Java VideoDecoderFactory class. Delegates method calls through
// JNI and wraps the decoder inside VideoDecoderWrapper.
class VideoDecoderFactoryWrapper : public VideoDecoderFactory {
 public:
  VideoDecoderFactoryWrapper(JNIEnv* jni, jobject decoder_factory);

  std::vector<SdpVideoFormat> GetSupportedFormats() const override;
  std::unique_ptr<VideoDecoder> CreateVideoDecoder(
      const SdpVideoFormat& format) override;

 private:
  const ScopedGlobalRef<jobject> decoder_factory_;
  jmethodID create_decoder_method_;
};

}  // namespace jni
}  // namespace webrtc

#endif  // SDK_ANDROID_SRC_JNI_VIDEODECODERFACTORYWRAPPER_H_
