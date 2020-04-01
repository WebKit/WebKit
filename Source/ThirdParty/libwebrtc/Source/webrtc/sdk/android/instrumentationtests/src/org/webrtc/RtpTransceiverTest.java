/*
 *  Copyright 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.mock;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.webrtc.RtpParameters.Encoding;
import org.webrtc.RtpTransceiver.RtpTransceiverInit;

/** Unit-tests for {@link RtpTransceiver}. */
@RunWith(BaseJUnit4ClassRunner.class)
public class RtpTransceiverTest {
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

  /** Test that RIDs get set in the RTP sender when passed in through an RtpTransceiverInit. */
  @Test
  @SmallTest
  public void testSetRidInSimulcast() throws Exception {
    List<Encoding> encodings = new ArrayList<Encoding>();
    encodings.add(new Encoding("F", true, null));
    encodings.add(new Encoding("H", true, null));

    RtpTransceiverInit init = new RtpTransceiverInit(
        RtpTransceiver.RtpTransceiverDirection.SEND_ONLY, Collections.emptyList(), encodings);
    RtpTransceiver transceiver =
        pc.addTransceiver(MediaStreamTrack.MediaType.MEDIA_TYPE_VIDEO, init);

    RtpSender sender = transceiver.getSender();
    RtpParameters parameters = sender.getParameters();
    List<Encoding> sendEncodings = parameters.getEncodings();
    assertEquals(2, sendEncodings.size());
    assertEquals("F", sendEncodings.get(0).getRid());
    assertEquals("H", sendEncodings.get(1).getRid());
  }
}
