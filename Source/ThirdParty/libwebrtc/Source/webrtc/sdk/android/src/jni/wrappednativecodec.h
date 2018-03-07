/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SDK_ANDROID_SRC_JNI_WRAPPEDNATIVECODEC_H_
#define SDK_ANDROID_SRC_JNI_WRAPPEDNATIVECODEC_H_

#include <jni.h>
#include <memory>

#include "api/video_codecs/video_decoder.h"
#include "api/video_codecs/video_encoder.h"

namespace webrtc {
namespace jni {

/* If the j_decoder is a wrapped native decoder, unwrap it. If it is not,
 * wrap it in a VideoDecoderWrapper.
 */
std::unique_ptr<VideoDecoder> JavaToNativeVideoDecoder(JNIEnv* jni,
                                                       jobject j_decoder);

/* If the j_encoder is a wrapped native encoder, unwrap it. If it is not,
 * wrap it in a VideoEncoderWrapper.
 */
std::unique_ptr<VideoEncoder> JavaToNativeVideoEncoder(JNIEnv* jni,
                                                       jobject j_encoder);

bool IsWrappedSoftwareEncoder(JNIEnv* jni, jobject j_encoder);

}  // namespace jni
}  // namespace webrtc

#endif  // SDK_ANDROID_SRC_JNI_WRAPPEDNATIVECODEC_H_
