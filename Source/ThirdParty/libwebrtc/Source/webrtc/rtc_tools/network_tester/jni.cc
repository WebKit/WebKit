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
#undef JNIEXPORT
#define JNIEXPORT __attribute__((visibility("default")))
#include <string>

#include "rtc_base/logging.h"
#include "rtc_base/thread.h"
#include "rtc_tools/network_tester/test_controller.h"

extern "C" JNIEXPORT jlong JNICALL
Java_com_google_media_networktester_NetworkTester_CreateTestController(
    JNIEnv* jni,
    jclass) {
  rtc::ThreadManager::Instance()->WrapCurrentThread();
  return reinterpret_cast<intptr_t>(new webrtc::TestController(
      0, 0, "/mnt/sdcard/network_tester_client_config.dat",
      "/mnt/sdcard/network_tester_client_packet_log.dat"));
}

extern "C" JNIEXPORT void JNICALL
Java_com_google_media_networktester_NetworkTester_TestControllerConnect(
    JNIEnv* jni,
    jclass,
    jlong native_pointer) {
  reinterpret_cast<webrtc::TestController*>(native_pointer)
      ->SendConnectTo("85.195.237.107", 9090);
}

extern "C" JNIEXPORT bool JNICALL
Java_com_google_media_networktester_NetworkTester_TestControllerIsDone(
    JNIEnv* jni,
    jclass,
    jlong native_pointer) {
  return reinterpret_cast<webrtc::TestController*>(native_pointer)
      ->IsTestDone();
}

extern "C" JNIEXPORT void JNICALL
Java_com_google_media_networktester_NetworkTester_TestControllerRun(
    JNIEnv* jni,
    jclass,
    jlong native_pointer) {
  // 100 ms arbitrary chosen, but it works well.
  rtc::Thread::Current()->ProcessMessages(/*cms=*/100);
}

extern "C" JNIEXPORT void JNICALL
Java_com_google_media_networktester_NetworkTester_DestroyTestController(
    JNIEnv* jni,
    jclass,
    jlong native_pointer) {
  webrtc::TestController* test_controller =
      reinterpret_cast<webrtc::TestController*>(native_pointer);
  if (test_controller) {
    delete test_controller;
  }
  rtc::ThreadManager::Instance()->UnwrapCurrentThread();
}
