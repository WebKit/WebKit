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

#include <cstdint>
#include <string>

#include "rtc_base/thread.h"
#include "rtc_tools/network_tester/generated_jni/NetworkTester_jni.h"
#include "rtc_tools/network_tester/test_controller.h"
#include "test/create_test_environment.h"
#include "third_party/jni_zero/jni_zero.h"

namespace webrtc {

static jlong JNI_NetworkTester_CreateTestController(JNIEnv* jni) {
  ThreadManager::Instance()->WrapCurrentThread();
  return reinterpret_cast<intptr_t>(
      new TestController(CreateTestEnvironment(), 0, 0,
                         "/mnt/sdcard/network_tester_client_config.dat",
                         "/mnt/sdcard/network_tester_client_packet_log.dat"));
}

static void JNI_NetworkTester_TestControllerConnect(JNIEnv* jni,
                                                    jlong native_pointer) {
  reinterpret_cast<TestController*>(native_pointer)
      ->SendConnectTo("85.195.237.107", 9090);
}

static jboolean JNI_NetworkTester_TestControllerIsDone(JNIEnv* jni,
                                                       jlong native_pointer) {
  return reinterpret_cast<TestController*>(native_pointer)->IsTestDone();
}

static void JNI_NetworkTester_TestControllerRun(JNIEnv* jni,
                                                jlong native_pointer) {
  // 100 ms arbitrary chosen, but it works well.
  Thread::Current()->ProcessMessages(/*cms=*/100);
}

static void JNI_NetworkTester_DestroyTestController(JNIEnv* jni,
                                                    jlong native_pointer) {
  TestController* test_controller =
      reinterpret_cast<TestController*>(native_pointer);
  if (test_controller) {
    delete test_controller;
  }
  ThreadManager::Instance()->UnwrapCurrentThread();
}

}  // namespace webrtc

DEFINE_JNI(NetworkTester)
