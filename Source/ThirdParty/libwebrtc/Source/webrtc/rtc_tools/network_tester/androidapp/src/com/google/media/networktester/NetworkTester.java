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

public class NetworkTester extends Thread {
  private native static long CreateTestController();
  private native static void TestControllerConnect(long testController);
  private native static void TestControllerRun(long testController);
  private native static boolean TestControllerIsDone(long testController);
  private native static void DestroyTestController(long testController);
  static {
    System.loadLibrary("network_tester_so");
  }

  @Override
  public void run() {
    final long testController = CreateTestController();
    TestControllerConnect(testController);
    while (!Thread.interrupted() && !TestControllerIsDone(testController)) {
      TestControllerRun(testController);
    }
    DestroyTestController(testController);
  }
}
