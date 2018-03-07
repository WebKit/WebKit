/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.webrtc.NetworkMonitorAutoDetect.ConnectionType;
import static org.webrtc.NetworkMonitorAutoDetect.ConnectivityManagerDelegate;
import static org.webrtc.NetworkMonitorAutoDetect.INVALID_NET_ID;
import static org.webrtc.NetworkMonitorAutoDetect.NetworkInformation;
import static org.webrtc.NetworkMonitorAutoDetect.NetworkState;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.Intent;
import android.net.ConnectivityManager;
import android.net.Network;
import android.net.wifi.WifiManager;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.support.test.InstrumentationRegistry;
import android.support.test.annotation.UiThreadTest;
import android.support.test.filters.MediumTest;
import android.support.test.filters.SmallTest;
import android.support.test.rule.UiThreadTestRule;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * Tests for org.webrtc.NetworkMonitor.
 *
 * TODO(deadbeef): These tests don't cover the interaction between
 * NetworkManager.java and androidnetworkmonitor_jni.cc, which is how this
 * class is used in practice in WebRTC.
 */
@SuppressLint("NewApi")
@RunWith(BaseJUnit4ClassRunner.class)
public class NetworkMonitorTest {
  @Rule public UiThreadTestRule uiThreadTestRule = new UiThreadTestRule();

  /**
   * Listens for alerts fired by the NetworkMonitor when network status changes.
   */
  private static class NetworkMonitorTestObserver implements NetworkMonitor.NetworkObserver {
    private boolean receivedNotification = false;

    @Override
    public void onConnectionTypeChanged(ConnectionType connectionType) {
      receivedNotification = true;
    }

    public boolean hasReceivedNotification() {
      return receivedNotification;
    }

    public void resetHasReceivedNotification() {
      receivedNotification = false;
    }
  }

  /**
   * Mocks out calls to the ConnectivityManager.
   */
  private static class MockConnectivityManagerDelegate extends ConnectivityManagerDelegate {
    private boolean activeNetworkExists;
    private int networkType;
    private int networkSubtype;

    @Override
    public NetworkState getNetworkState() {
      return new NetworkState(activeNetworkExists, networkType, networkSubtype);
    }

    // Dummy implementations to avoid NullPointerExceptions in default implementations:

    @Override
    public long getDefaultNetId() {
      return INVALID_NET_ID;
    }

    @Override
    public Network[] getAllNetworks() {
      return new Network[0];
    }

    @Override
    public NetworkState getNetworkState(Network network) {
      return new NetworkState(false, -1, -1);
    }

    public void setActiveNetworkExists(boolean networkExists) {
      activeNetworkExists = networkExists;
    }

    public void setNetworkType(int networkType) {
      this.networkType = networkType;
    }

    public void setNetworkSubtype(int networkSubtype) {
      this.networkSubtype = networkSubtype;
    }
  }

  /**
   * Mocks out calls to the WifiManager.
   */
  private static class MockWifiManagerDelegate
      extends NetworkMonitorAutoDetect.WifiManagerDelegate {
    private String wifiSSID;

    @Override
    public String getWifiSSID() {
      return wifiSSID;
    }

    public void setWifiSSID(String wifiSSID) {
      this.wifiSSID = wifiSSID;
    }
  }

  // A dummy NetworkMonitorAutoDetect.Observer.
  private static class TestNetworkMonitorAutoDetectObserver
      implements NetworkMonitorAutoDetect.Observer {
    @Override
    public void onConnectionTypeChanged(ConnectionType newConnectionType) {}

    @Override
    public void onNetworkConnect(NetworkInformation networkInfo) {}

    @Override
    public void onNetworkDisconnect(long networkHandle) {}
  }

  private static final Object lock = new Object();
  private static Handler uiThreadHandler = null;

  private NetworkMonitorAutoDetect receiver;
  private MockConnectivityManagerDelegate connectivityDelegate;
  private MockWifiManagerDelegate wifiDelegate;

  private static Handler getUiThreadHandler() {
    synchronized (lock) {
      if (uiThreadHandler == null) {
        uiThreadHandler = new Handler(Looper.getMainLooper());
      }
      return uiThreadHandler;
    }
  }

  /**
   * Helper method to create a network monitor and delegates for testing.
   */
  private void createTestMonitor() {
    Context context = InstrumentationRegistry.getTargetContext();
    NetworkMonitor.resetInstanceForTests();
    NetworkMonitor.createAutoDetectorForTest();
    receiver = NetworkMonitor.getAutoDetectorForTest();
    assertNotNull(receiver);

    connectivityDelegate = new MockConnectivityManagerDelegate();
    connectivityDelegate.setActiveNetworkExists(true);
    receiver.setConnectivityManagerDelegateForTests(connectivityDelegate);

    wifiDelegate = new MockWifiManagerDelegate();
    receiver.setWifiManagerDelegateForTests(wifiDelegate);
    wifiDelegate.setWifiSSID("foo");
  }

  private NetworkMonitorAutoDetect.ConnectionType getCurrentConnectionType() {
    final NetworkMonitorAutoDetect.NetworkState networkState = receiver.getCurrentNetworkState();
    return NetworkMonitorAutoDetect.getConnectionType(networkState);
  }

  @Before
  public void setUp() {
    ContextUtils.initialize(InstrumentationRegistry.getTargetContext());
    createTestMonitor();
  }

  /**
   * Tests that the receiver registers for connectivity intents during construction.
   */
  @Test
  @UiThreadTest
  @SmallTest
  public void testNetworkMonitorRegistersInConstructor() throws InterruptedException {
    Context context = InstrumentationRegistry.getTargetContext();

    NetworkMonitorAutoDetect.Observer observer = new TestNetworkMonitorAutoDetectObserver();

    NetworkMonitorAutoDetect receiver = new NetworkMonitorAutoDetect(observer, context);

    assertTrue(receiver.isReceiverRegisteredForTesting());
  }

  /**
   * Tests that when there is an intent indicating a change in network connectivity, it sends a
   * notification to Java observers.
   */
  @Test
  @UiThreadTest
  @MediumTest
  public void testNetworkMonitorJavaObservers() throws InterruptedException {
    // Initialize the NetworkMonitor with a connection.
    Intent connectivityIntent = new Intent(ConnectivityManager.CONNECTIVITY_ACTION);
    receiver.onReceive(InstrumentationRegistry.getTargetContext(), connectivityIntent);

    // We shouldn't be re-notified if the connection hasn't actually changed.
    NetworkMonitorTestObserver observer = new NetworkMonitorTestObserver();
    NetworkMonitor.addNetworkObserver(observer);
    receiver.onReceive(InstrumentationRegistry.getTargetContext(), connectivityIntent);
    assertFalse(observer.hasReceivedNotification());

    // We shouldn't be notified if we're connected to non-Wifi and the Wifi SSID changes.
    wifiDelegate.setWifiSSID("bar");
    receiver.onReceive(InstrumentationRegistry.getTargetContext(), connectivityIntent);
    assertFalse(observer.hasReceivedNotification());

    // We should be notified when we change to Wifi.
    connectivityDelegate.setNetworkType(ConnectivityManager.TYPE_WIFI);
    receiver.onReceive(InstrumentationRegistry.getTargetContext(), connectivityIntent);
    assertTrue(observer.hasReceivedNotification());
    observer.resetHasReceivedNotification();

    // We should be notified when the Wifi SSID changes.
    wifiDelegate.setWifiSSID("foo");
    receiver.onReceive(InstrumentationRegistry.getTargetContext(), connectivityIntent);
    assertTrue(observer.hasReceivedNotification());
    observer.resetHasReceivedNotification();

    // We shouldn't be re-notified if the Wifi SSID hasn't actually changed.
    receiver.onReceive(InstrumentationRegistry.getTargetContext(), connectivityIntent);
    assertFalse(observer.hasReceivedNotification());

    // Mimic that connectivity has been lost and ensure that the observer gets the notification.
    connectivityDelegate.setActiveNetworkExists(false);
    Intent noConnectivityIntent = new Intent(ConnectivityManager.CONNECTIVITY_ACTION);
    receiver.onReceive(InstrumentationRegistry.getTargetContext(), noConnectivityIntent);
    assertTrue(observer.hasReceivedNotification());
  }

  /**
   * Tests that ConnectivityManagerDelegate doesn't crash. This test cannot rely on having any
   * active network connections so it cannot usefully check results, but it can at least check
   * that the functions don't crash.
   */
  @Test
  @UiThreadTest
  @SmallTest
  public void testConnectivityManagerDelegateDoesNotCrash() {
    ConnectivityManagerDelegate delegate =
        new ConnectivityManagerDelegate(InstrumentationRegistry.getTargetContext());
    delegate.getNetworkState();
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
      Network[] networks = delegate.getAllNetworks();
      if (networks.length >= 1) {
        delegate.getNetworkState(networks[0]);
        delegate.hasInternetCapability(networks[0]);
      }
      delegate.getDefaultNetId();
    }
  }

  /**
   * Tests that NetworkMonitorAutoDetect queryable APIs don't crash. This test cannot rely
   * on having any active network connections so it cannot usefully check results, but it can at
   * least check that the functions don't crash.
   */
  @Test
  @UiThreadTest
  @SmallTest
  public void testQueryableAPIsDoNotCrash() {
    NetworkMonitorAutoDetect.Observer observer = new TestNetworkMonitorAutoDetectObserver();
    NetworkMonitorAutoDetect ncn =
        new NetworkMonitorAutoDetect(observer, InstrumentationRegistry.getTargetContext());
    ncn.getDefaultNetId();
  }
}
