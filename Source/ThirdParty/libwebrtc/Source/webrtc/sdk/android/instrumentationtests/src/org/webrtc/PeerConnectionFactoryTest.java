/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
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

@RunWith(AndroidJUnit4.class)
public class PeerConnectionFactoryTest {
  @SmallTest
  @Test
  public void testInitialize() {
    PeerConnectionFactory.initialize(PeerConnectionFactory.InitializationOptions
                                         .builder(InstrumentationRegistry.getTargetContext())
                                         .createInitializationOptions());
  }

  @SmallTest
  @Test
  public void testInitializeTwice() {
    PeerConnectionFactory.initialize(PeerConnectionFactory.InitializationOptions
                                         .builder(InstrumentationRegistry.getTargetContext())
                                         .createInitializationOptions());
    PeerConnectionFactory.initialize(PeerConnectionFactory.InitializationOptions
                                         .builder(InstrumentationRegistry.getTargetContext())
                                         .createInitializationOptions());
  }

  @SmallTest
  @Test
  public void testInitializeTwiceWithTracer() {
    PeerConnectionFactory.initialize(PeerConnectionFactory.InitializationOptions
                                         .builder(InstrumentationRegistry.getTargetContext())
                                         .setEnableInternalTracer(true)
                                         .createInitializationOptions());
    PeerConnectionFactory.initialize(PeerConnectionFactory.InitializationOptions
                                         .builder(InstrumentationRegistry.getTargetContext())
                                         .setEnableInternalTracer(true)
                                         .createInitializationOptions());
  }

  @SmallTest
  @Test
  public void testInitializeWithTracerAndShutdown() {
    PeerConnectionFactory.initialize(PeerConnectionFactory.InitializationOptions
                                         .builder(InstrumentationRegistry.getTargetContext())
                                         .setEnableInternalTracer(true)
                                         .createInitializationOptions());
    PeerConnectionFactory.shutdownInternalTracer();
  }
}
