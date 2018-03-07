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

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;
import android.support.test.filters.SmallTest;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

/** Unit tests for {@link DefaultAudioProcessingFactory}. */
@RunWith(BaseJUnit4ClassRunner.class)
public final class DefaultAudioProcessingFactoryTest {
  @Before
  public void setUp() {
    PeerConnectionFactory.initialize(
        PeerConnectionFactory.InitializationOptions.builder(InstrumentationRegistry.getContext())
            .createInitializationOptions());
  }

  /**
   * Tests that a PeerConnectionFactory can be initialized with an AudioProcessingFactory without
   * crashing.
   */
  @Test
  @MediumTest
  public void testInitializePeerConnectionFactory() {
    AudioProcessingFactory audioProcessingFactory = new DefaultAudioProcessingFactory();
    PeerConnectionFactory peerConnectionFactory = new PeerConnectionFactory(null /* options */,
        null /* encoderFactory */, null /* decoderFactory */, audioProcessingFactory);
    peerConnectionFactory.dispose();
  }
}
