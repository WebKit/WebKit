/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <jni.h>

#include "webrtc/api/videosourceproxy.h"
#include "webrtc/base/logging.h"
#include "webrtc/media/engine/webrtcvideodecoderfactory.h"
#include "webrtc/media/engine/webrtcvideoencoderfactory.h"
#include "webrtc/sdk/android/src/jni/androidmediadecoder_jni.h"
#include "webrtc/sdk/android/src/jni/androidmediaencoder_jni.h"
#include "webrtc/sdk/android/src/jni/androidvideotracksource.h"
#include "webrtc/sdk/android/src/jni/classreferenceholder.h"
#include "webrtc/sdk/android/src/jni/ownedfactoryandthreads.h"
#include "webrtc/sdk/android/src/jni/surfacetexturehelper_jni.h"

namespace webrtc_jni {

cricket::WebRtcVideoEncoderFactory* CreateVideoEncoderFactory() {
  return new MediaCodecVideoEncoderFactory();
}

cricket::WebRtcVideoDecoderFactory* CreateVideoDecoderFactory() {
  return new MediaCodecVideoDecoderFactory();
}

jobject GetJavaSurfaceTextureHelper(
    const rtc::scoped_refptr<SurfaceTextureHelper>& surface_texture_helper) {
  return surface_texture_helper
             ? surface_texture_helper->GetJavaSurfaceTextureHelper()
             : nullptr;
}

JOW(jlong, PeerConnectionFactory_nativeCreateVideoSource)
(JNIEnv* jni,
 jclass,
 jlong native_factory,
 jobject j_surface_texture_helper,
 jboolean is_screencast) {
  OwnedFactoryAndThreads* factory =
      reinterpret_cast<OwnedFactoryAndThreads*>(native_factory);

  rtc::scoped_refptr<webrtc::AndroidVideoTrackSource> source(
      new rtc::RefCountedObject<webrtc::AndroidVideoTrackSource>(
          factory->signaling_thread(), jni, j_surface_texture_helper,
          is_screencast));
  rtc::scoped_refptr<webrtc::VideoTrackSourceProxy> proxy_source =
      webrtc::VideoTrackSourceProxy::Create(factory->signaling_thread(),
                                            factory->worker_thread(), source);

  return (jlong)proxy_source.release();
}

JOW(jlong, PeerConnectionFactory_nativeCreateVideoTrack)
(JNIEnv* jni, jclass, jlong native_factory, jstring id, jlong native_source) {
  rtc::scoped_refptr<PeerConnectionFactoryInterface> factory(
      factoryFromJava(native_factory));
  rtc::scoped_refptr<webrtc::VideoTrackInterface> track(
      factory->CreateVideoTrack(
          JavaToStdString(jni, id),
          reinterpret_cast<webrtc::VideoTrackSourceInterface*>(native_source)));
  return (jlong)track.release();
}

JOW(void, PeerConnectionFactory_nativeSetVideoHwAccelerationOptions)
(JNIEnv* jni,
 jclass,
 jlong native_factory,
 jobject local_egl_context,
 jobject remote_egl_context) {
  OwnedFactoryAndThreads* owned_factory =
      reinterpret_cast<OwnedFactoryAndThreads*>(native_factory);

  jclass j_eglbase14_context_class =
      FindClass(jni, "org/webrtc/EglBase14$Context");

  MediaCodecVideoEncoderFactory* encoder_factory =
      static_cast<MediaCodecVideoEncoderFactory*>(
          owned_factory->encoder_factory());
  if (encoder_factory &&
      jni->IsInstanceOf(local_egl_context, j_eglbase14_context_class)) {
    LOG(LS_INFO) << "Set EGL context for HW encoding.";
    encoder_factory->SetEGLContext(jni, local_egl_context);
  }

  MediaCodecVideoDecoderFactory* decoder_factory =
      static_cast<MediaCodecVideoDecoderFactory*>(
          owned_factory->decoder_factory());
  if (decoder_factory) {
    LOG(LS_INFO) << "Set EGL context for HW decoding.";
    decoder_factory->SetEGLContext(jni, remote_egl_context);
  }
}

}  // namespace webrtc_jni
