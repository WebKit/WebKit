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
import org.chromium.native_test.NativeUnitTest;
import org.webrtc.ContextUtils;

/**
 * Native unit test that calls ContextUtils.initialize for WebRTC.
 */
public class RTCNativeUnitTest extends NativeUnitTest {
  @Override
  public void preCreate(Activity activity) {
    super.preCreate(activity);
    ContextUtils.initialize(activity.getApplicationContext());
  }
}
