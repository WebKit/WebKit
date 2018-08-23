/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef EXAMPLES_ANDROIDNATIVEAPI_JNI_ANDROIDCALLCLIENT_H_
#define EXAMPLES_ANDROIDNATIVEAPI_JNI_ANDROIDCALLCLIENT_H_

#include <jni.h>

#include <memory>
#include <string>

#include "api/peerconnectioninterface.h"
#include "rtc_base/criticalsection.h"
#include "rtc_base/scoped_ref_ptr.h"
#include "rtc_base/thread_checker.h"
#include "sdk/android/native_api/jni/scoped_java_ref.h"
#include "sdk/android/native_api/video/videosource.h"

namespace webrtc_examples {

class AndroidCallClient {
 public:
  AndroidCallClient();
  ~AndroidCallClient();

  void Call(JNIEnv* env,
            const webrtc::JavaRef<jobject>& cls,
            const webrtc::JavaRef<jobject>& local_sink,
            const webrtc::JavaRef<jobject>& remote_sink);
  void Hangup(JNIEnv* env, const webrtc::JavaRef<jobject>& cls);
  // A helper method for Java code to delete this object. Calls delete this.
  void Delete(JNIEnv* env, const webrtc::JavaRef<jobject>& cls);

  webrtc::ScopedJavaLocalRef<jobject> GetJavaVideoCapturerObserver(
      JNIEnv* env,
      const webrtc::JavaRef<jobject>& cls);

 private:
  class PCObserver;

  void CreatePeerConnectionFactory() RTC_RUN_ON(thread_checker_);
  void CreatePeerConnection() RTC_RUN_ON(thread_checker_);
  void Connect() RTC_RUN_ON(thread_checker_);

  rtc::ThreadChecker thread_checker_;

  bool call_started_ RTC_GUARDED_BY(thread_checker_);

  const std::unique_ptr<PCObserver> pc_observer_;

  rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> pcf_
      RTC_GUARDED_BY(thread_checker_);
  std::unique_ptr<rtc::Thread> network_thread_ RTC_GUARDED_BY(thread_checker_);
  std::unique_ptr<rtc::Thread> worker_thread_ RTC_GUARDED_BY(thread_checker_);
  std::unique_ptr<rtc::Thread> signaling_thread_
      RTC_GUARDED_BY(thread_checker_);

  std::unique_ptr<rtc::VideoSinkInterface<webrtc::VideoFrame>> local_sink_
      RTC_GUARDED_BY(thread_checker_);
  std::unique_ptr<rtc::VideoSinkInterface<webrtc::VideoFrame>> remote_sink_
      RTC_GUARDED_BY(thread_checker_);
  rtc::scoped_refptr<webrtc::JavaVideoTrackSourceInterface> video_source_
      RTC_GUARDED_BY(thread_checker_);

  rtc::CriticalSection pc_mutex_;
  rtc::scoped_refptr<webrtc::PeerConnectionInterface> pc_
      RTC_GUARDED_BY(pc_mutex_);
};

}  // namespace webrtc_examples

#endif  // EXAMPLES_ANDROIDNATIVEAPI_JNI_ANDROIDCALLCLIENT_H_
