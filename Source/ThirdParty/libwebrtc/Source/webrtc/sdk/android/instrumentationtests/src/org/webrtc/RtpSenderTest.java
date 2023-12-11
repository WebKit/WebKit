/*
 *  Copyright 2020 The WebRTC project authors. All Rights Reserved.
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
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.SmallTest;
import java.util.Arrays;
import org.junit.Before;
import org.junit.Test;
import org.webrtc.RtpParameters.DegradationPreference;

/** Unit-tests for {@link RtpSender}. */
public class RtpSenderTest {
  private PeerConnectionFactory factory;
  private PeerConnection pc;

  @Before
  public void setUp() {
    PeerConnectionFactory.initialize(PeerConnectionFactory.InitializationOptions
                                         .builder(InstrumentationRegistry.getTargetContext())
                                         .setNativeLibraryName(TestConstants.NATIVE_LIBRARY)
                                         .createInitializationOptions());

    factory = PeerConnectionFactory.builder().createPeerConnectionFactory();

    PeerConnection.RTCConfiguration config = new PeerConnection.RTCConfiguration(Arrays.asList());
    // RtpTranceiver is part of new unified plan semantics.
    config.sdpSemantics = PeerConnection.SdpSemantics.UNIFIED_PLAN;
    pc = factory.createPeerConnection(config, mock(PeerConnection.Observer.class));
  }

  /** Test checking the enum values for DegradationPreference stay consistent */
  @Test
  @SmallTest
  public void testSetDegradationPreference() throws Exception {
    RtpTransceiver transceiver = pc.addTransceiver(MediaStreamTrack.MediaType.MEDIA_TYPE_VIDEO);
    RtpSender sender = transceiver.getSender();

    RtpParameters parameters = sender.getParameters();
    assertNotNull(parameters);
    assertNull(parameters.degradationPreference);

    parameters.degradationPreference = DegradationPreference.MAINTAIN_FRAMERATE;
    assertTrue(sender.setParameters(parameters));
    parameters = sender.getParameters();
    assertEquals(DegradationPreference.MAINTAIN_FRAMERATE, parameters.degradationPreference);

    parameters.degradationPreference = DegradationPreference.MAINTAIN_RESOLUTION;
    assertTrue(sender.setParameters(parameters));
    parameters = sender.getParameters();
    assertEquals(DegradationPreference.MAINTAIN_RESOLUTION, parameters.degradationPreference);

    parameters.degradationPreference = DegradationPreference.BALANCED;
    assertTrue(sender.setParameters(parameters));
    parameters = sender.getParameters();
    assertEquals(DegradationPreference.BALANCED, parameters.degradationPreference);

    parameters.degradationPreference = DegradationPreference.DISABLED;
    assertTrue(sender.setParameters(parameters));
    parameters = sender.getParameters();
    assertEquals(DegradationPreference.DISABLED, parameters.degradationPreference);
  }
}
