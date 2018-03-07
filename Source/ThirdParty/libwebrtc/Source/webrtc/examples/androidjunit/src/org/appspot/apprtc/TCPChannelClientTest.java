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

import static org.junit.Assert.fail;
import static org.mockito.Mockito.timeout;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;

import org.chromium.base.test.util.DisabledTest;
import org.chromium.testing.local.LocalRobolectricTestRunner;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLog;

import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.TimeUnit;

@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TCPChannelClientTest {
  private static final int PORT = 8888;
  /**
   * How long we wait before trying to connect to the server. Chosen quite arbitrarily and
   * could be made smaller if need be.
   */
  private static final int SERVER_WAIT = 10;
  private static final int CONNECT_TIMEOUT = 100;
  private static final int SEND_TIMEOUT = 100;
  private static final int DISCONNECT_TIMEOUT = 100;
  private static final int TERMINATION_TIMEOUT = 1000;
  private static final String TEST_MESSAGE_SERVER = "Hello, Server!";
  private static final String TEST_MESSAGE_CLIENT = "Hello, Client!";

  @Mock TCPChannelClient.TCPChannelEvents serverEvents;
  @Mock TCPChannelClient.TCPChannelEvents clientEvents;

  private ExecutorService executor;
  private TCPChannelClient server;
  private TCPChannelClient client;

  @Before
  public void setUp() {
    ShadowLog.stream = System.out;

    MockitoAnnotations.initMocks(this);

    executor = Executors.newSingleThreadExecutor();
  }

  @After
  public void tearDown() {
    verifyNoMoreEvents();

    executeAndWait(new Runnable() {
      @Override
      public void run() {
        client.disconnect();
        server.disconnect();
      }
    });

    // Stop the executor thread
    executor.shutdown();
    try {
      executor.awaitTermination(TERMINATION_TIMEOUT, TimeUnit.MILLISECONDS);
    } catch (InterruptedException e) {
      fail(e.getMessage());
    }
  }

  @Test
  public void testConnectIPv4() {
    setUpIPv4Server();
    try {
      Thread.sleep(SERVER_WAIT);
    } catch (InterruptedException e) {
      fail(e.getMessage());
    }
    setUpIPv4Client();

    verify(serverEvents, timeout(CONNECT_TIMEOUT)).onTCPConnected(true);
    verify(clientEvents, timeout(CONNECT_TIMEOUT)).onTCPConnected(false);
  }

  @Test
  public void testConnectIPv6() {
    setUpIPv6Server();
    try {
      Thread.sleep(SERVER_WAIT);
    } catch (InterruptedException e) {
      fail(e.getMessage());
    }
    setUpIPv6Client();

    verify(serverEvents, timeout(CONNECT_TIMEOUT)).onTCPConnected(true);
    verify(clientEvents, timeout(CONNECT_TIMEOUT)).onTCPConnected(false);
  }

  @Test
  public void testSendData() {
    testConnectIPv4();

    executeAndWait(new Runnable() {
      @Override
      public void run() {
        client.send(TEST_MESSAGE_SERVER);
        server.send(TEST_MESSAGE_CLIENT);
      }
    });

    verify(serverEvents, timeout(SEND_TIMEOUT)).onTCPMessage(TEST_MESSAGE_SERVER);
    verify(clientEvents, timeout(SEND_TIMEOUT)).onTCPMessage(TEST_MESSAGE_CLIENT);
  }

  @Test
  public void testDisconnectServer() {
    testConnectIPv4();
    executeAndWait(new Runnable() {
      @Override
      public void run() {
        server.disconnect();
      }
    });

    verify(serverEvents, timeout(DISCONNECT_TIMEOUT)).onTCPClose();
    verify(clientEvents, timeout(DISCONNECT_TIMEOUT)).onTCPClose();
  }

  @Test
  public void testDisconnectClient() {
    testConnectIPv4();
    executeAndWait(new Runnable() {
      @Override
      public void run() {
        client.disconnect();
      }
    });

    verify(serverEvents, timeout(DISCONNECT_TIMEOUT)).onTCPClose();
    verify(clientEvents, timeout(DISCONNECT_TIMEOUT)).onTCPClose();
  }

  private void setUpIPv4Server() {
    setUpServer("0.0.0.0", PORT);
  }

  private void setUpIPv4Client() {
    setUpClient("127.0.0.1", PORT);
  }

  private void setUpIPv6Server() {
    setUpServer("::", PORT);
  }

  private void setUpIPv6Client() {
    setUpClient("::1", PORT);
  }

  private void setUpServer(String ip, int port) {
    server = new TCPChannelClient(executor, serverEvents, ip, port);
  }

  private void setUpClient(String ip, int port) {
    client = new TCPChannelClient(executor, clientEvents, ip, port);
  }

  /**
   * Verifies no more server or client events have been issued
   */
  private void verifyNoMoreEvents() {
    verifyNoMoreInteractions(serverEvents);
    verifyNoMoreInteractions(clientEvents);
  }

  /**
   * Queues runnable to be run and waits for it to be executed by the executor thread
   */
  public void executeAndWait(Runnable runnable) {
    try {
      executor.submit(runnable).get();
    } catch (Exception e) {
      fail(e.getMessage());
    }
  }
}
