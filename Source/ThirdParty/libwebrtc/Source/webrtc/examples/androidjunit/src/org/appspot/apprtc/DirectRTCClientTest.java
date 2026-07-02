/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.appspot.apprtc;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.isNotNull;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.timeout;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLog;
import org.robolectric.RobolectricTestRunner;
import org.webrtc.IceCandidate;
import org.webrtc.SessionDescription;

/**
 * Test for DirectRTCClient. Test is very simple and only tests the overall sanity of the class
 * behaviour.
 */
@RunWith(RobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class DirectRTCClientTest {
  private static final String ROOM_URL = "";
  private static final boolean LOOPBACK = false;

  private static final String DUMMY_SDP_MID = "sdpMid";
  private static final String DUMMY_SDP = "sdp";

  public static final int SERVER_WAIT = 100;
  public static final int NETWORK_TIMEOUT = 1000;

  private DirectRTCClient client;
  private DirectRTCClient server;

  AppRTCClient.SignalingEvents clientEvents;
  AppRTCClient.SignalingEvents serverEvents;

  @Before
  public void setUp() {
    ShadowLog.stream = System.out;

    clientEvents = mock(AppRTCClient.SignalingEvents.class);
    serverEvents = mock(AppRTCClient.SignalingEvents.class);

    client = new DirectRTCClient(clientEvents);
    server = new DirectRTCClient(serverEvents);
  }

  @Test
  public void testValidIpPattern() {
    // Strings that should match the pattern.
    // clang-format off
    final String[] ipAddresses = new String[] {
        "0.0.0.0",
        "127.0.0.1",
        "192.168.0.1",
        "0.0.0.0:8888",
        "127.0.0.1:8888",
        "192.168.0.1:8888",
        "::",
        "::1",
        "2001:0db8:85a3:0000:0000:8a2e:0370:7946",
        "[::]",
        "[::1]",
        "[2001:0db8:85a3:0000:0000:8a2e:0370:7946]",
        "[::]:8888",
        "[::1]:8888",
        "[2001:0db8:85a3:0000:0000:8a2e:0370:7946]:8888"
    };
    // clang-format on

    for (String ip : ipAddresses) {
      assertTrue(ip + " didn't match IP_PATTERN even though it should.",
          DirectRTCClient.IP_PATTERN.matcher(ip).matches());
    }
  }

  @Test
  public void testInvalidIpPattern() {
    // Strings that shouldn't match the pattern.
    // clang-format off
    final String[] invalidIpAddresses = new String[] {
        "Hello, World!",
        "aaaa",
        "1111",
        "[hello world]",
        "hello:world"
    };
    // clang-format on

    for (String invalidIp : invalidIpAddresses) {
      assertFalse(invalidIp + " matched IP_PATTERN even though it shouldn't.",
          DirectRTCClient.IP_PATTERN.matcher(invalidIp).matches());
    }
  }

  // TODO(sakal): Replace isNotNull(class) with isNotNull() once Java 8 is used.
  @SuppressWarnings("deprecation")
  @Test
  public void testDirectRTCClient() {
    server.connectToRoom(new AppRTCClient.RoomConnectionParameters(ROOM_URL, "0.0.0.0", LOOPBACK));
    try {
      Thread.sleep(SERVER_WAIT);
    } catch (InterruptedException e) {
      fail(e.getMessage());
    }
    client.connectToRoom(
        new AppRTCClient.RoomConnectionParameters(ROOM_URL, "127.0.0.1", LOOPBACK));
    verify(serverEvents, timeout(NETWORK_TIMEOUT))
        .onConnectedToRoom(any(AppRTCClient.SignalingParameters.class));

    SessionDescription offerSdp = new SessionDescription(SessionDescription.Type.OFFER, DUMMY_SDP);
    server.sendOfferSdp(offerSdp);
    verify(clientEvents, timeout(NETWORK_TIMEOUT))
        .onConnectedToRoom(any(AppRTCClient.SignalingParameters.class));

    SessionDescription answerSdp =
        new SessionDescription(SessionDescription.Type.ANSWER, DUMMY_SDP);
    client.sendAnswerSdp(answerSdp);
    verify(serverEvents, timeout(NETWORK_TIMEOUT))
        .onRemoteDescription(isNotNull(SessionDescription.class));

    IceCandidate candidate = new IceCandidate(DUMMY_SDP_MID, 0, DUMMY_SDP);
    server.sendLocalIceCandidate(candidate);
    verify(clientEvents, timeout(NETWORK_TIMEOUT))
        .onRemoteIceCandidate(isNotNull(IceCandidate.class));

    client.sendLocalIceCandidate(candidate);
    verify(serverEvents, timeout(NETWORK_TIMEOUT))
        .onRemoteIceCandidate(isNotNull(IceCandidate.class));

    client.disconnectFromRoom();
    verify(clientEvents, timeout(NETWORK_TIMEOUT)).onChannelClose();
    verify(serverEvents, timeout(NETWORK_TIMEOUT)).onChannelClose();

    verifyNoMoreInteractions(clientEvents);
    verifyNoMoreInteractions(serverEvents);
  }
}
