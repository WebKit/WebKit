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

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.SmallTest;
import org.junit.Test;
import org.webrtc.PeerConnectionFactory;

// This test is intended to run on ARM and catch LoadLibrary errors when we load the WebRTC
// JNI. It can't really be setting up calls since ARM emulators are too slow, but instantiating
// a peer connection isn't timing-sensitive, so we can at least do that.
public class WebRtcJniBootTest {
  @Test
  @SmallTest
  public void testJniLoadsWithoutError() throws InterruptedException {
    PeerConnectionFactory.initialize(PeerConnectionFactory.InitializationOptions
                                         .builder(InstrumentationRegistry.getTargetContext())
                                         .setNativeLibraryName(TestConstants.NATIVE_LIBRARY)
                                         .createInitializationOptions());
    PeerConnectionFactory.builder().createPeerConnectionFactory();
  }
}
