/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.appspot.apprtc.test;

import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.SmallTest;

import org.webrtc.PeerConnectionFactory;

// This test is intended to run on ARM and catch LoadLibrary errors when we load the WebRTC
// JNI. It can't really be setting up calls since ARM emulators are too slow, but instantiating
// a peer connection isn't timing-sensitive, so we can at least do that.
public class WebRtcJniBootTest extends InstrumentationTestCase {
  @SmallTest
  public void testJniLoadsWithoutError() throws InterruptedException {
    PeerConnectionFactory.initializeAndroidGlobals(getInstrumentation().getTargetContext(),
        true /* initializeAudio */, true /* initializeVideo */,
        false /* videoCodecHwAcceleration */);

    PeerConnectionFactory.Options options = new PeerConnectionFactory.Options();
    new PeerConnectionFactory(options);
  }
}
