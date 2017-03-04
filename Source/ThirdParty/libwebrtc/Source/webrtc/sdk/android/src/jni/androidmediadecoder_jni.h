/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_SDK_ANDROID_SRC_JNI_ANDROIDMEDIADECODER_JNI_H_
#define WEBRTC_SDK_ANDROID_SRC_JNI_ANDROIDMEDIADECODER_JNI_H_

#include "webrtc/sdk/android/src/jni/jni_helpers.h"
#include "webrtc/media/engine/webrtcvideodecoderfactory.h"

namespace webrtc_jni {

// Implementation of Android MediaCodec based decoder factory.
class MediaCodecVideoDecoderFactory
    : public cricket::WebRtcVideoDecoderFactory {
 public:
  MediaCodecVideoDecoderFactory();
  virtual ~MediaCodecVideoDecoderFactory();

  void SetEGLContext(JNIEnv* jni, jobject render_egl_context);

  // WebRtcVideoDecoderFactory implementation.
  webrtc::VideoDecoder* CreateVideoDecoder(webrtc::VideoCodecType type)
      override;

  void DestroyVideoDecoder(webrtc::VideoDecoder* decoder) override;

 private:
  jobject egl_context_;
  std::vector<webrtc::VideoCodecType> supported_codec_types_;
};

}  // namespace webrtc_jni

#endif  // WEBRTC_SDK_ANDROID_SRC_JNI_ANDROIDMEDIADECODER_JNI_H_
