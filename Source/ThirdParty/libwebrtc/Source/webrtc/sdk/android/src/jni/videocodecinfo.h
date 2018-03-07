/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SDK_ANDROID_SRC_JNI_VIDEOCODECINFO_H_
#define SDK_ANDROID_SRC_JNI_VIDEOCODECINFO_H_

#include <jni.h>

#include "api/video_codecs/sdp_video_format.h"

namespace webrtc {
namespace jni {

SdpVideoFormat VideoCodecInfoToSdpVideoFormat(JNIEnv* jni, jobject info);
jobject SdpVideoFormatToVideoCodecInfo(JNIEnv* jni,
                                       const SdpVideoFormat& format);

}  // namespace jni
}  // namespace webrtc

#endif  // SDK_ANDROID_SRC_JNI_VIDEOCODECINFO_H_
