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

import static org.webrtc.NetworkMonitorAutoDetect.ConnectionType;
import static org.webrtc.NetworkMonitorAutoDetect.INVALID_NET_ID;
import static org.webrtc.NetworkMonitorAutoDetect.NetworkInformation;

import android.content.Context;
import android.os.Build;

import java.util.ArrayList;
import java.util.List;

/**
 * Borrowed from Chromium's src/net/android/java/src/org/chromium/net/NetworkChangeNotifier.java
 *
 * Triggers updates to the underlying network state from OS networking events.
 *
 * WARNING: This class is not thread-safe.
 */
public class NetworkMonitor {
  /**
   * Alerted when the connection type of the network changes.
   * The alert is fired on the UI thread.
   */
  public interface NetworkObserver {
    public void onConnectionTypeChanged(ConnectionType connectionType);
  }

  private static final String TAG = "NetworkMonitor";
  private static NetworkMonitor instance;

  private final Context applicationContext;

  // Native observers of the connection type changes.
  private final ArrayList<Long> nativeNetworkObservers;
  // Java observers of the connection type changes.
  private final ArrayList<NetworkObserver> networkObservers;

  // Object that detects the connection type changes.
  private NetworkMonitorAutoDetect autoDetector;

  private ConnectionType currentConnectionType = ConnectionType.CONNECTION_UNKNOWN;

  private NetworkMonitor(Context context) {
    assertIsTrue(context != null);
    applicationContext =
        context.getApplicationContext() == null ? context : context.getApplicationContext();

    nativeNetworkObservers = new ArrayList<Long>();
    networkObservers = new ArrayList<NetworkObserver>();
  }

  /**
   * Initializes the singleton once.
   * Called from the native code.
   */
  public static NetworkMonitor init(Context context) {
    if (!isInitialized()) {
      instance = new NetworkMonitor(context);
    }
    return instance;
  }

  public static boolean isInitialized() {
    return instance != null;
  }

  /**
   * Returns the singleton instance.
   */
  public static NetworkMonitor getInstance() {
    return instance;
  }

  /**
   * Enables auto detection of the current network state based on notifications from the system.
   * Note that passing true here requires the embedding app have the platform ACCESS_NETWORK_STATE
   * permission.
   *
   * @param shouldAutoDetect true if the NetworkMonitor should listen for system changes in
   *  network connectivity.
   */
  public static void setAutoDetectConnectivityState(boolean shouldAutoDetect) {
    getInstance().setAutoDetectConnectivityStateInternal(shouldAutoDetect);
  }

  private static void assertIsTrue(boolean condition) {
    if (!condition) {
      throw new AssertionError("Expected to be true");
    }
  }

  // Called by the native code.
  private void startMonitoring(long nativeObserver) {
    Logging.d(TAG, "Start monitoring from native observer " + nativeObserver);
    nativeNetworkObservers.add(nativeObserver);
    setAutoDetectConnectivityStateInternal(true);
  }

  // Called by the native code.
  private void stopMonitoring(long nativeObserver) {
    Logging.d(TAG, "Stop monitoring from native observer " + nativeObserver);
    setAutoDetectConnectivityStateInternal(false);
    nativeNetworkObservers.remove(nativeObserver);
  }

  // Called by the native code to get the Android SDK version.
  private static int androidSdkInt() {
    return Build.VERSION.SDK_INT;
  }

  private ConnectionType getCurrentConnectionType() {
    return currentConnectionType;
  }

  private long getCurrentDefaultNetId() {
    return autoDetector == null ? INVALID_NET_ID : autoDetector.getDefaultNetId();
  }

  private void destroyAutoDetector() {
    if (autoDetector != null) {
      autoDetector.destroy();
      autoDetector = null;
    }
  }

  private void setAutoDetectConnectivityStateInternal(boolean shouldAutoDetect) {
    if (!shouldAutoDetect) {
      destroyAutoDetector();
      return;
    }
    if (autoDetector == null) {
      autoDetector = new NetworkMonitorAutoDetect(new NetworkMonitorAutoDetect.Observer() {

        @Override
        public void onConnectionTypeChanged(ConnectionType newConnectionType) {
          updateCurrentConnectionType(newConnectionType);
        }

        @Override
        public void onNetworkConnect(NetworkInformation networkInfo) {
          notifyObserversOfNetworkConnect(networkInfo);
        }

        @Override
        public void onNetworkDisconnect(long networkHandle) {
          notifyObserversOfNetworkDisconnect(networkHandle);
        }
      }, applicationContext);
      final NetworkMonitorAutoDetect.NetworkState networkState =
          autoDetector.getCurrentNetworkState();
      updateCurrentConnectionType(NetworkMonitorAutoDetect.getConnectionType(networkState));
      updateActiveNetworkList();
    }
  }

  private void updateCurrentConnectionType(ConnectionType newConnectionType) {
    currentConnectionType = newConnectionType;
    notifyObserversOfConnectionTypeChange(newConnectionType);
  }

  /**
   * Alerts all observers of a connection change.
   */
  private void notifyObserversOfConnectionTypeChange(ConnectionType newConnectionType) {
    for (long nativeObserver : nativeNetworkObservers) {
      nativeNotifyConnectionTypeChanged(nativeObserver);
    }
    for (NetworkObserver observer : networkObservers) {
      observer.onConnectionTypeChanged(newConnectionType);
    }
  }

  private void notifyObserversOfNetworkConnect(NetworkInformation networkInfo) {
    for (long nativeObserver : nativeNetworkObservers) {
      nativeNotifyOfNetworkConnect(nativeObserver, networkInfo);
    }
  }

  private void notifyObserversOfNetworkDisconnect(long networkHandle) {
    for (long nativeObserver : nativeNetworkObservers) {
      nativeNotifyOfNetworkDisconnect(nativeObserver, networkHandle);
    }
  }

  private void updateActiveNetworkList() {
    List<NetworkInformation> networkInfoList = autoDetector.getActiveNetworkList();
    if (networkInfoList == null || networkInfoList.size() == 0) {
      return;
    }

    NetworkInformation[] networkInfos = new NetworkInformation[networkInfoList.size()];
    networkInfos = networkInfoList.toArray(networkInfos);
    for (long nativeObserver : nativeNetworkObservers) {
      nativeNotifyOfActiveNetworkList(nativeObserver, networkInfos);
    }
  }

  /**
   * Adds an observer for any connection type changes.
   */
  public static void addNetworkObserver(NetworkObserver observer) {
    getInstance().addNetworkObserverInternal(observer);
  }

  private void addNetworkObserverInternal(NetworkObserver observer) {
    networkObservers.add(observer);
  }

  /**
   * Removes an observer for any connection type changes.
   */
  public static void removeNetworkObserver(NetworkObserver observer) {
    getInstance().removeNetworkObserverInternal(observer);
  }

  private void removeNetworkObserverInternal(NetworkObserver observer) {
    networkObservers.remove(observer);
  }

  /**
   * Checks if there currently is connectivity.
   */
  public static boolean isOnline() {
    ConnectionType connectionType = getInstance().getCurrentConnectionType();
    return connectionType != ConnectionType.CONNECTION_NONE;
  }

  private native void nativeNotifyConnectionTypeChanged(long nativePtr);
  private native void nativeNotifyOfNetworkConnect(long nativePtr, NetworkInformation networkInfo);
  private native void nativeNotifyOfNetworkDisconnect(long nativePtr, long networkHandle);
  private native void nativeNotifyOfActiveNetworkList(
      long nativePtr, NetworkInformation[] networkInfos);

  // For testing only.
  static void resetInstanceForTests(Context context) {
    instance = new NetworkMonitor(context);
  }

  // For testing only.
  public static NetworkMonitorAutoDetect getAutoDetectorForTest() {
    return getInstance().autoDetector;
  }
}
