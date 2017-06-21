/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_SDK_ANDROID_SRC_JNI_WRAPPED_NATIVE_I420_BUFFER_H_
#define WEBRTC_SDK_ANDROID_SRC_JNI_WRAPPED_NATIVE_I420_BUFFER_H_

#include <jni.h>

#include "webrtc/api/video/video_frame_buffer.h"

namespace webrtc_jni {

// This function wraps the C++ I420 buffer and returns a Java
// VideoFrame.I420Buffer as a jobject.
jobject WrapI420Buffer(
    const rtc::scoped_refptr<webrtc::I420BufferInterface>& i420_buffer);

}  // namespace webrtc_jni

#endif  // WEBRTC_SDK_ANDROID_SRC_JNI_WRAPPED_NATIVE_I420_BUFFER_H_
