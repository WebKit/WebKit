/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/sdk/android/src/jni/ownedfactoryandthreads.h"

#include "webrtc/base/logging.h"
#include "webrtc/sdk/android/src/jni/classreferenceholder.h"
#include "webrtc/sdk/android/src/jni/jni_helpers.h"

namespace webrtc_jni {

PeerConnectionFactoryInterface* factoryFromJava(jlong j_p) {
  return reinterpret_cast<OwnedFactoryAndThreads*>(j_p)->factory();
}

OwnedFactoryAndThreads::~OwnedFactoryAndThreads() {
  CHECK_RELEASE(factory_);
  if (network_monitor_factory_ != nullptr) {
    rtc::NetworkMonitorFactory::ReleaseFactory(network_monitor_factory_);
  }
}

void OwnedFactoryAndThreads::JavaCallbackOnFactoryThreads() {
  JNIEnv* jni = AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(jni);
  jclass j_factory_class = FindClass(jni, "org/webrtc/PeerConnectionFactory");
  jmethodID m = nullptr;
  if (network_thread_->IsCurrent()) {
    LOG(LS_INFO) << "Network thread JavaCallback";
    m = GetStaticMethodID(jni, j_factory_class, "onNetworkThreadReady", "()V");
  }
  if (worker_thread_->IsCurrent()) {
    LOG(LS_INFO) << "Worker thread JavaCallback";
    m = GetStaticMethodID(jni, j_factory_class, "onWorkerThreadReady", "()V");
  }
  if (signaling_thread_->IsCurrent()) {
    LOG(LS_INFO) << "Signaling thread JavaCallback";
    m = GetStaticMethodID(jni, j_factory_class, "onSignalingThreadReady",
                          "()V");
  }
  if (m != nullptr) {
    jni->CallStaticVoidMethod(j_factory_class, m);
    CHECK_EXCEPTION(jni) << "error during JavaCallback::CallStaticVoidMethod";
  }
}

void OwnedFactoryAndThreads::InvokeJavaCallbacksOnFactoryThreads() {
  LOG(LS_INFO) << "InvokeJavaCallbacksOnFactoryThreads.";
  network_thread_->Invoke<void>(RTC_FROM_HERE,
                                [this] { JavaCallbackOnFactoryThreads(); });
  worker_thread_->Invoke<void>(RTC_FROM_HERE,
                               [this] { JavaCallbackOnFactoryThreads(); });
  signaling_thread_->Invoke<void>(RTC_FROM_HERE,
                                  [this] { JavaCallbackOnFactoryThreads(); });
}

}  // namespace webrtc_jni
