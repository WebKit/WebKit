/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "sdk/android/src/jni/videocodecinfo.h"

#include "sdk/android/src/jni/classreferenceholder.h"
#include "sdk/android/src/jni/jni_helpers.h"

namespace webrtc {
namespace jni {

SdpVideoFormat VideoCodecInfoToSdpVideoFormat(JNIEnv* jni, jobject j_info) {
  jclass video_codec_info_class = FindClass(jni, "org/webrtc/VideoCodecInfo");
  jfieldID name_field =
      GetFieldID(jni, video_codec_info_class, "name", "Ljava/lang/String;");
  jfieldID params_field =
      GetFieldID(jni, video_codec_info_class, "params", "Ljava/util/Map;");

  jobject j_params = jni->GetObjectField(j_info, params_field);
  jstring j_name =
      static_cast<jstring>(jni->GetObjectField(j_info, name_field));

  return SdpVideoFormat(JavaToStdString(jni, j_name),
                        JavaToStdMapStrings(jni, j_params));
}

jobject SdpVideoFormatToVideoCodecInfo(JNIEnv* jni,
                                       const SdpVideoFormat& format) {
  jclass hash_map_class = jni->FindClass("java/util/HashMap");
  jmethodID hash_map_constructor =
      jni->GetMethodID(hash_map_class, "<init>", "()V");
  jmethodID put_method = jni->GetMethodID(
      hash_map_class, "put",
      "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;");
  jclass video_codec_info_class = FindClass(jni, "org/webrtc/VideoCodecInfo");
  jmethodID video_codec_info_constructor = jni->GetMethodID(
      video_codec_info_class, "<init>", "(Ljava/lang/String;Ljava/util/Map;)V");

  jobject j_params = jni->NewObject(hash_map_class, hash_map_constructor);
  for (auto const& param : format.parameters) {
    jni->CallObjectMethod(j_params, put_method,
                          NativeToJavaString(jni, param.first),
                          NativeToJavaString(jni, param.second));
  }
  return jni->NewObject(video_codec_info_class, video_codec_info_constructor,
                        NativeToJavaString(jni, format.name), j_params);
}

}  // namespace jni
}  // namespace webrtc
