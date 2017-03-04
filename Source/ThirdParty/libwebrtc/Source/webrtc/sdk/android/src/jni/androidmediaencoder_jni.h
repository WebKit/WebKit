/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_SDK_ANDROID_SRC_JNI_ANDROIDMEDIAENCODER_JNI_H_
#define WEBRTC_SDK_ANDROID_SRC_JNI_ANDROIDMEDIAENCODER_JNI_H_

#include <vector>

#include "webrtc/sdk/android/src/jni/jni_helpers.h"
#include "webrtc/media/engine/webrtcvideoencoderfactory.h"

namespace webrtc_jni {

// Implementation of Android MediaCodec based encoder factory.
class MediaCodecVideoEncoderFactory
    : public cricket::WebRtcVideoEncoderFactory {
 public:
  MediaCodecVideoEncoderFactory();
  virtual ~MediaCodecVideoEncoderFactory();

  void SetEGLContext(JNIEnv* jni, jobject egl_context);

  // WebRtcVideoEncoderFactory implementation.
  webrtc::VideoEncoder* CreateVideoEncoder(
      const cricket::VideoCodec& codec) override;
  const std::vector<cricket::VideoCodec>& supported_codecs() const override;
  void DestroyVideoEncoder(webrtc::VideoEncoder* encoder) override;

 private:
  // Disable overloaded virtual function warning. TODO(magjed): Remove once
  // http://crbug/webrtc/6402 is fixed.
  using cricket::WebRtcVideoEncoderFactory::CreateVideoEncoder;

  jobject egl_context_;

  // Empty if platform support is lacking, const after ctor returns.
  std::vector<cricket::VideoCodec> supported_codecs_;
  std::vector<cricket::VideoCodec> supported_codecs_with_h264_hp_;
};

}  // namespace webrtc_jni

#endif  // WEBRTC_SDK_ANDROID_SRC_JNI_ANDROIDMEDIAENCODER_JNI_H_
