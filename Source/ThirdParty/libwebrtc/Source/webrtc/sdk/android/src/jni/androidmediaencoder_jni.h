/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SDK_ANDROID_SRC_JNI_ANDROIDMEDIAENCODER_JNI_H_
#define SDK_ANDROID_SRC_JNI_ANDROIDMEDIAENCODER_JNI_H_

#include <vector>

#include "sdk/android/src/jni/jni_helpers.h"
#include "media/engine/webrtcvideoencoderfactory.h"

namespace webrtc {
namespace jni {

// Implementation of Android MediaCodec based encoder factory.
class MediaCodecVideoEncoderFactory
    : public cricket::WebRtcVideoEncoderFactory {
 public:
  MediaCodecVideoEncoderFactory();
  virtual ~MediaCodecVideoEncoderFactory();

  void SetEGLContext(JNIEnv* jni, jobject egl_context);

  // WebRtcVideoEncoderFactory implementation.
  VideoEncoder* CreateVideoEncoder(const cricket::VideoCodec& codec) override;
  const std::vector<cricket::VideoCodec>& supported_codecs() const override;
  void DestroyVideoEncoder(VideoEncoder* encoder) override;

 private:
  jobject egl_context_;

  // Empty if platform support is lacking, const after ctor returns.
  std::vector<cricket::VideoCodec> supported_codecs_;
  std::vector<cricket::VideoCodec> supported_codecs_with_h264_hp_;
};

}  // namespace jni
}  // namespace webrtc

#endif  // SDK_ANDROID_SRC_JNI_ANDROIDMEDIAENCODER_JNI_H_
