/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package com.google.media.networktester;

import org.jni_zero.NativeMethods;

public class NetworkTester extends Thread {
  static {
    System.loadLibrary("network_tester_so");
  }

  @Override
  public void run() {
    final long testController = NetworkTesterJni.get().createTestController();
    NetworkTesterJni.get().testControllerConnect(testController);
    while (!Thread.interrupted() && !NetworkTesterJni.get().testControllerIsDone(testController)) {
      NetworkTesterJni.get().testControllerRun(testController);
    }
    NetworkTesterJni.get().destroyTestController(testController);
  }

  @NativeMethods
  interface Natives {
    long createTestController();

    void testControllerConnect(long testController);

    void testControllerRun(long testController);

    boolean testControllerIsDone(long testController);

    void destroyTestController(long testController);
  }
}
