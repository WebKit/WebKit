/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc.native_test;

import android.app.Activity;
import android.os.Bundle;

/**
 * Activity that uses RTCNativeUnitTest to run the tests.
 */
public class RTCNativeUnitTestActivity extends Activity {
  private RTCNativeUnitTest mTest = new RTCNativeUnitTest();

  @Override
  public void onCreate(Bundle savedInstanceState) {
    mTest.preCreate(this);
    super.onCreate(savedInstanceState);
    mTest.postCreate(this);
  }

  @Override
  public void onStart() {
    super.onStart();
    mTest.postStart(this, false);
  }
}
