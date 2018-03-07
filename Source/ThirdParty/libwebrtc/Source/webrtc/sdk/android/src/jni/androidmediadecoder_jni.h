/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SDK_ANDROID_SRC_JNI_ANDROIDMEDIADECODER_JNI_H_
#define SDK_ANDROID_SRC_JNI_ANDROIDMEDIADECODER_JNI_H_

#include "sdk/android/src/jni/jni_helpers.h"
#include "media/engine/webrtcvideodecoderfactory.h"

namespace webrtc {
namespace jni {

// Implementation of Android MediaCodec based decoder factory.
class MediaCodecVideoDecoderFactory
    : public cricket::WebRtcVideoDecoderFactory {
 public:
  MediaCodecVideoDecoderFactory();
  virtual ~MediaCodecVideoDecoderFactory();

  void SetEGLContext(JNIEnv* jni, jobject render_egl_context);

  // WebRtcVideoDecoderFactory implementation.
  VideoDecoder* CreateVideoDecoder(VideoCodecType type) override;

  void DestroyVideoDecoder(VideoDecoder* decoder) override;

 private:
  jobject egl_context_;
  std::vector<VideoCodecType> supported_codec_types_;
};

}  // namespace jni
}  // namespace webrtc

#endif  // SDK_ANDROID_SRC_JNI_ANDROIDMEDIADECODER_JNI_H_
