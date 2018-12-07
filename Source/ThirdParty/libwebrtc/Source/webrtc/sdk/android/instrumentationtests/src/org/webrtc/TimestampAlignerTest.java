/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

import android.support.test.filters.SmallTest;
import org.chromium.base.test.params.BaseJUnit4RunnerDelegate;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.junit.BeforeClass;
import org.junit.Test;

@UseRunnerDelegate(BaseJUnit4RunnerDelegate.class)
public class TimestampAlignerTest {
  @BeforeClass
  public static void setUp() {
    System.loadLibrary(TestConstants.NATIVE_LIBRARY);
  }

  @Test
  @SmallTest
  public void testGetRtcTimeNanos() {
    TimestampAligner.getRtcTimeNanos();
  }

  @Test
  @SmallTest
  public void testDispose() {
    final TimestampAligner timestampAligner = new TimestampAligner();
    timestampAligner.dispose();
  }

  @Test
  @SmallTest
  public void testTranslateTimestamp() {
    final TimestampAligner timestampAligner = new TimestampAligner();
    timestampAligner.translateTimestamp(/* cameraTimeNs= */ 123);
    timestampAligner.dispose();
  }
}
