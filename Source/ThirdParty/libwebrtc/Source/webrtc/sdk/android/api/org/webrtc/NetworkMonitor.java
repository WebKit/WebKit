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

import static org.webrtc.NetworkMonitorAutoDetect.INVALID_NET_ID;

import android.content.Context;
import android.os.Build;
import java.util.ArrayList;
import java.util.List;
import javax.annotation.Nullable;
import org.webrtc.NetworkMonitorAutoDetect;

/**
 * Borrowed from Chromium's
 * src/net/android/java/src/org/chromium/net/NetworkChangeNotifier.java
 *
 * <p>Triggers updates to the underlying network state from OS networking events.
 *
 * <p>This class is thread-safe.
 */
public class NetworkMonitor {
  /**
   * Alerted when the connection type of the network changes. The alert is fired on the UI thread.
   */
  public interface NetworkObserver {
    public void onConnectionTypeChanged(NetworkMonitorAutoDetect.ConnectionType connectionType);
  }

  private static final String TAG = "NetworkMonitor";

  // Lazy initialization holder class idiom for static fields.
  private static class InstanceHolder {
    // We are storing application context so it is okay.
    static final NetworkMonitor instance = new NetworkMonitor();
  }

  // Native observers of the connection type changes.
  private final ArrayList<Long> nativeNetworkObservers;
  // Java observers of the connection type changes.
  private final ArrayList<NetworkObserver> networkObservers;

  private final Object autoDetectLock = new Object();
  // Object that detects the connection type changes and brings up mobile networks.
  @Nullable private NetworkMonitorAutoDetect autoDetect;
  // Also guarded by autoDetectLock.
  private int numObservers;

  private volatile NetworkMonitorAutoDetect.ConnectionType currentConnectionType;

  private NetworkMonitor() {
    nativeNetworkObservers = new ArrayList<Long>();
    networkObservers = new ArrayList<NetworkObserver>();
    numObservers = 0;
    currentConnectionType = NetworkMonitorAutoDetect.ConnectionType.CONNECTION_UNKNOWN;
  }

  // TODO(sakal): Remove once downstream dependencies have been updated.
  @Deprecated
  public static void init(Context context) {}

  /** Returns the singleton instance. This may be called from native or from Java code. */
  @CalledByNative
  public static NetworkMonitor getInstance() {
    return InstanceHolder.instance;
  }

  private static void assertIsTrue(boolean condition) {
    if (!condition) {
      throw new AssertionError("Expected to be true");
    }
  }

  /**
   * Enables auto detection of the network state change and brings up mobile networks for using
   * multi-networking. This requires the embedding app have the platform ACCESS_NETWORK_STATE and
   * CHANGE_NETWORK_STATE permission.
   */
  public void startMonitoring(Context applicationContext) {
    synchronized (autoDetectLock) {
      ++numObservers;
      if (autoDetect == null) {
        autoDetect = createAutoDetect(applicationContext);
      }
      currentConnectionType =
          NetworkMonitorAutoDetect.getConnectionType(autoDetect.getCurrentNetworkState());
    }
  }

  /** Deprecated, pass in application context in startMonitoring instead. */
  @Deprecated
  public void startMonitoring() {
    startMonitoring(ContextUtils.getApplicationContext());
  }

  /**
   * Enables auto detection of the network state change and brings up mobile networks for using
   * multi-networking. This requires the embedding app have the platform ACCESS_NETWORK_STATE and
   * CHANGE_NETWORK_STATE permission.
   */
  @CalledByNative
  private void startMonitoring(@Nullable Context applicationContext, long nativeObserver) {
    Logging.d(TAG, "Start monitoring with native observer " + nativeObserver);

    startMonitoring(
        applicationContext != null ? applicationContext : ContextUtils.getApplicationContext());
    // The native observers expect a network list update after they call startMonitoring.
    synchronized (nativeNetworkObservers) {
      nativeNetworkObservers.add(nativeObserver);
    }
    updateObserverActiveNetworkList(nativeObserver);
    // currentConnectionType was updated in startMonitoring().
    // Need to notify the native observers here.
    notifyObserversOfConnectionTypeChange(currentConnectionType);
  }

  /** Stop network monitoring. If no one is monitoring networks, destroy and reset autoDetect. */
  public void stopMonitoring() {
    synchronized (autoDetectLock) {
      if (--numObservers == 0) {
        autoDetect.destroy();
        autoDetect = null;
      }
    }
  }

  @CalledByNative
  private void stopMonitoring(long nativeObserver) {
    Logging.d(TAG, "Stop monitoring with native observer " + nativeObserver);
    stopMonitoring();
    synchronized (nativeNetworkObservers) {
      nativeNetworkObservers.remove(nativeObserver);
    }
  }

  // Returns true if network binding is supported on this platform.
  @CalledByNative
  private boolean networkBindingSupported() {
    synchronized (autoDetectLock) {
      return autoDetect != null && autoDetect.supportNetworkCallback();
    }
  }

  @CalledByNative
  private static int androidSdkInt() {
    return Build.VERSION.SDK_INT;
  }

  private NetworkMonitorAutoDetect.ConnectionType getCurrentConnectionType() {
    return currentConnectionType;
  }

  private long getCurrentDefaultNetId() {
    synchronized (autoDetectLock) {
      return autoDetect == null ? INVALID_NET_ID : autoDetect.getDefaultNetId();
    }
  }

  private NetworkMonitorAutoDetect createAutoDetect(Context appContext) {
    return new NetworkMonitorAutoDetect(new NetworkMonitorAutoDetect.Observer() {

      @Override
      public void onConnectionTypeChanged(
          NetworkMonitorAutoDetect.ConnectionType newConnectionType) {
        updateCurrentConnectionType(newConnectionType);
      }

      @Override
      public void onNetworkConnect(NetworkMonitorAutoDetect.NetworkInformation networkInfo) {
        notifyObserversOfNetworkConnect(networkInfo);
      }

      @Override
      public void onNetworkDisconnect(long networkHandle) {
        notifyObserversOfNetworkDisconnect(networkHandle);
      }
    }, appContext);
  }

  private void updateCurrentConnectionType(
      NetworkMonitorAutoDetect.ConnectionType newConnectionType) {
    currentConnectionType = newConnectionType;
    notifyObserversOfConnectionTypeChange(newConnectionType);
  }

  /** Alerts all observers of a connection change. */
  private void notifyObserversOfConnectionTypeChange(
      NetworkMonitorAutoDetect.ConnectionType newConnectionType) {
    List<Long> nativeObservers = getNativeNetworkObserversSync();
    for (Long nativeObserver : nativeObservers) {
      nativeNotifyConnectionTypeChanged(nativeObserver);
    }
    // This avoids calling external methods while locking on an object.
    List<NetworkObserver> javaObservers;
    synchronized (networkObservers) {
      javaObservers = new ArrayList<>(networkObservers);
    }
    for (NetworkObserver observer : javaObservers) {
      observer.onConnectionTypeChanged(newConnectionType);
    }
  }

  private void notifyObserversOfNetworkConnect(
      NetworkMonitorAutoDetect.NetworkInformation networkInfo) {
    List<Long> nativeObservers = getNativeNetworkObserversSync();
    for (Long nativeObserver : nativeObservers) {
      nativeNotifyOfNetworkConnect(nativeObserver, networkInfo);
    }
  }

  private void notifyObserversOfNetworkDisconnect(long networkHandle) {
    List<Long> nativeObservers = getNativeNetworkObserversSync();
    for (Long nativeObserver : nativeObservers) {
      nativeNotifyOfNetworkDisconnect(nativeObserver, networkHandle);
    }
  }

  private void updateObserverActiveNetworkList(long nativeObserver) {
    List<NetworkMonitorAutoDetect.NetworkInformation> networkInfoList;
    synchronized (autoDetectLock) {
      networkInfoList = (autoDetect == null) ? null : autoDetect.getActiveNetworkList();
    }
    if (networkInfoList == null || networkInfoList.size() == 0) {
      return;
    }

    NetworkMonitorAutoDetect.NetworkInformation[] networkInfos =
        new NetworkMonitorAutoDetect.NetworkInformation[networkInfoList.size()];
    networkInfos = networkInfoList.toArray(networkInfos);
    nativeNotifyOfActiveNetworkList(nativeObserver, networkInfos);
  }

  private List<Long> getNativeNetworkObserversSync() {
    synchronized (nativeNetworkObservers) {
      return new ArrayList<>(nativeNetworkObservers);
    }
  }

  /**
   * Adds an observer for any connection type changes.
   *
   * @deprecated Use getInstance(appContext).addObserver instead.
   */
  @Deprecated
  public static void addNetworkObserver(NetworkObserver observer) {
    getInstance().addObserver(observer);
  }

  public void addObserver(NetworkObserver observer) {
    synchronized (networkObservers) {
      networkObservers.add(observer);
    }
  }

  /**
   * Removes an observer for any connection type changes.
   *
   * @deprecated Use getInstance(appContext).removeObserver instead.
   */
  @Deprecated
  public static void removeNetworkObserver(NetworkObserver observer) {
    getInstance().removeObserver(observer);
  }

  public void removeObserver(NetworkObserver observer) {
    synchronized (networkObservers) {
      networkObservers.remove(observer);
    }
  }

  /** Checks if there currently is connectivity. */
  public static boolean isOnline() {
    NetworkMonitorAutoDetect.ConnectionType connectionType =
        getInstance().getCurrentConnectionType();
    return connectionType != NetworkMonitorAutoDetect.ConnectionType.CONNECTION_NONE;
  }

  private native void nativeNotifyConnectionTypeChanged(long nativeAndroidNetworkMonitor);
  private native void nativeNotifyOfNetworkConnect(
      long nativeAndroidNetworkMonitor, NetworkMonitorAutoDetect.NetworkInformation networkInfo);
  private native void nativeNotifyOfNetworkDisconnect(
      long nativeAndroidNetworkMonitor, long networkHandle);
  private native void nativeNotifyOfActiveNetworkList(
      long nativeAndroidNetworkMonitor, NetworkMonitorAutoDetect.NetworkInformation[] networkInfos);

  // For testing only.
  @Nullable
  NetworkMonitorAutoDetect getNetworkMonitorAutoDetect() {
    synchronized (autoDetectLock) {
      return autoDetect;
    }
  }

  // For testing only.
  int getNumObservers() {
    synchronized (autoDetectLock) {
      return numObservers;
    }
  }

  // For testing only.
  static NetworkMonitorAutoDetect createAndSetAutoDetectForTest(Context context) {
    NetworkMonitor networkMonitor = getInstance();
    NetworkMonitorAutoDetect autoDetect = networkMonitor.createAutoDetect(context);
    return networkMonitor.autoDetect = autoDetect;
  }
}
