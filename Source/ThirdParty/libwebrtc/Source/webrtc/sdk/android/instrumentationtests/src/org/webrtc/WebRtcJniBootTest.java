/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;
import android.support.test.runner.AndroidJUnit4;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.webrtc.PeerConnectionFactory;

// This test is intended to run on ARM and catch LoadLibrary errors when we load the WebRTC
// JNI. It can't really be setting up calls since ARM emulators are too slow, but instantiating
// a peer connection isn't timing-sensitive, so we can at least do that.
@RunWith(AndroidJUnit4.class)
public class WebRtcJniBootTest {
  @Test
  @SmallTest
  public void testJniLoadsWithoutError() throws InterruptedException {
    PeerConnectionFactory.initializeAndroidGlobals(InstrumentationRegistry.getTargetContext(),
        true /* initializeAudio */, true /* initializeVideo */,
        false /* videoCodecHwAcceleration */);

    PeerConnectionFactory.Options options = new PeerConnectionFactory.Options();
    new PeerConnectionFactory(options);
  }
}
