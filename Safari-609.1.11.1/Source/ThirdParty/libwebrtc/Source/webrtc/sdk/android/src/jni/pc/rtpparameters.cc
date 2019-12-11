/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "sdk/android/src/jni/pc/rtpparameters.h"

#include "sdk/android/generated_peerconnection_jni/jni/RtpParameters_jni.h"
#include "sdk/android/native_api/jni/java_types.h"
#include "sdk/android/src/jni/jni_helpers.h"
#include "sdk/android/src/jni/pc/mediastreamtrack.h"

namespace webrtc {
namespace jni {

namespace {

ScopedJavaLocalRef<jobject> NativeToJavaRtpEncodingParameter(
    JNIEnv* env,
    const RtpEncodingParameters& encoding) {
  return Java_Encoding_Constructor(
      env, encoding.active, NativeToJavaInteger(env, encoding.max_bitrate_bps),
      NativeToJavaInteger(env, encoding.min_bitrate_bps),
      NativeToJavaInteger(env, encoding.max_framerate),
      NativeToJavaInteger(env, encoding.num_temporal_layers),
      encoding.ssrc ? NativeToJavaLong(env, *encoding.ssrc) : nullptr);
}

ScopedJavaLocalRef<jobject> NativeToJavaRtpCodecParameter(
    JNIEnv* env,
    const RtpCodecParameters& codec) {
  return Java_Codec_Constructor(env, codec.payload_type,
                                NativeToJavaString(env, codec.name),
                                NativeToJavaMediaType(env, codec.kind),
                                NativeToJavaInteger(env, codec.clock_rate),
                                NativeToJavaInteger(env, codec.num_channels),
                                NativeToJavaStringMap(env, codec.parameters));
}

ScopedJavaLocalRef<jobject> NativeToJavaRtpRtcpParameters(
    JNIEnv* env,
    const RtcpParameters& rtcp) {
  return Java_Rtcp_Constructor(env, NativeToJavaString(env, rtcp.cname),
                               rtcp.reduced_size);
}

ScopedJavaLocalRef<jobject> NativeToJavaRtpHeaderExtensionParameter(
    JNIEnv* env,
    const RtpExtension& extension) {
  return Java_HeaderExtension_Constructor(
      env, NativeToJavaString(env, extension.uri), extension.id,
      extension.encrypt);
}

}  // namespace

RtpEncodingParameters JavaToNativeRtpEncodingParameters(
    JNIEnv* jni,
    const JavaRef<jobject>& j_encoding_parameters) {
  RtpEncodingParameters encoding;
  encoding.active = Java_Encoding_getActive(jni, j_encoding_parameters);
  ScopedJavaLocalRef<jobject> j_max_bitrate =
      Java_Encoding_getMaxBitrateBps(jni, j_encoding_parameters);
  encoding.max_bitrate_bps = JavaToNativeOptionalInt(jni, j_max_bitrate);
  ScopedJavaLocalRef<jobject> j_min_bitrate =
      Java_Encoding_getMinBitrateBps(jni, j_encoding_parameters);
  encoding.min_bitrate_bps = JavaToNativeOptionalInt(jni, j_min_bitrate);
  ScopedJavaLocalRef<jobject> j_max_framerate =
      Java_Encoding_getMaxFramerate(jni, j_encoding_parameters);
  encoding.max_framerate = JavaToNativeOptionalInt(jni, j_max_framerate);
  ScopedJavaLocalRef<jobject> j_num_temporal_layers =
      Java_Encoding_getNumTemporalLayers(jni, j_encoding_parameters);
  encoding.num_temporal_layers =
      JavaToNativeOptionalInt(jni, j_num_temporal_layers);
  ScopedJavaLocalRef<jobject> j_ssrc =
      Java_Encoding_getSsrc(jni, j_encoding_parameters);
  if (!IsNull(jni, j_ssrc))
    encoding.ssrc = JavaToNativeLong(jni, j_ssrc);
  return encoding;
}

RtpParameters JavaToNativeRtpParameters(JNIEnv* jni,
                                        const JavaRef<jobject>& j_parameters) {
  RtpParameters parameters;

  ScopedJavaLocalRef<jstring> j_transaction_id =
      Java_RtpParameters_getTransactionId(jni, j_parameters);
  parameters.transaction_id = JavaToNativeString(jni, j_transaction_id);

  ScopedJavaLocalRef<jobject> j_rtcp =
      Java_RtpParameters_getRtcp(jni, j_parameters);
  ScopedJavaLocalRef<jstring> j_rtcp_cname = Java_Rtcp_getCname(jni, j_rtcp);
  jboolean j_rtcp_reduced_size = Java_Rtcp_getReducedSize(jni, j_rtcp);
  parameters.rtcp.cname = JavaToNativeString(jni, j_rtcp_cname);
  parameters.rtcp.reduced_size = j_rtcp_reduced_size;

  ScopedJavaLocalRef<jobject> j_header_extensions =
      Java_RtpParameters_getHeaderExtensions(jni, j_parameters);
  for (const JavaRef<jobject>& j_header_extension :
       Iterable(jni, j_header_extensions)) {
    RtpExtension header_extension;
    header_extension.uri = JavaToStdString(
        jni, Java_HeaderExtension_getUri(jni, j_header_extension));
    header_extension.id = Java_HeaderExtension_getId(jni, j_header_extension);
    header_extension.encrypt =
        Java_HeaderExtension_getEncrypted(jni, j_header_extension);
    parameters.header_extensions.push_back(header_extension);
  }

  // Convert encodings.
  ScopedJavaLocalRef<jobject> j_encodings =
      Java_RtpParameters_getEncodings(jni, j_parameters);
  for (const JavaRef<jobject>& j_encoding_parameters :
       Iterable(jni, j_encodings)) {
    RtpEncodingParameters encoding =
        JavaToNativeRtpEncodingParameters(jni, j_encoding_parameters);
    parameters.encodings.push_back(encoding);
  }

  // Convert codecs.
  ScopedJavaLocalRef<jobject> j_codecs =
      Java_RtpParameters_getCodecs(jni, j_parameters);
  for (const JavaRef<jobject>& j_codec : Iterable(jni, j_codecs)) {
    RtpCodecParameters codec;
    codec.payload_type = Java_Codec_getPayloadType(jni, j_codec);
    codec.name = JavaToStdString(jni, Java_Codec_getName(jni, j_codec));
    codec.kind = JavaToNativeMediaType(jni, Java_Codec_getKind(jni, j_codec));
    codec.clock_rate =
        JavaToNativeOptionalInt(jni, Java_Codec_getClockRate(jni, j_codec));
    codec.num_channels =
        JavaToNativeOptionalInt(jni, Java_Codec_getNumChannels(jni, j_codec));
    auto parameters_map =
        JavaToNativeStringMap(jni, Java_Codec_getParameters(jni, j_codec));
    codec.parameters.insert(parameters_map.begin(), parameters_map.end());
    parameters.codecs.push_back(codec);
  }
  return parameters;
}

ScopedJavaLocalRef<jobject> NativeToJavaRtpParameters(
    JNIEnv* env,
    const RtpParameters& parameters) {
  return Java_RtpParameters_Constructor(
      env, NativeToJavaString(env, parameters.transaction_id),
      NativeToJavaRtpRtcpParameters(env, parameters.rtcp),
      NativeToJavaList(env, parameters.header_extensions,
                       &NativeToJavaRtpHeaderExtensionParameter),
      NativeToJavaList(env, parameters.encodings,
                       &NativeToJavaRtpEncodingParameter),
      NativeToJavaList(env, parameters.codecs, &NativeToJavaRtpCodecParameter));
}

}  // namespace jni
}  // namespace webrtc
