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

import android.annotation.SuppressLint;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.net.ConnectivityManager;
import android.net.ConnectivityManager.NetworkCallback;
import android.net.LinkAddress;
import android.net.LinkProperties;
import android.net.Network;
import android.net.NetworkCapabilities;
import android.net.NetworkInfo;
import android.net.NetworkRequest;
import android.net.wifi.WifiInfo;
import android.net.wifi.WifiManager;
import android.net.wifi.p2p.WifiP2pGroup;
import android.net.wifi.p2p.WifiP2pManager;
import android.os.Build;
import android.telephony.TelephonyManager;
import java.net.InetAddress;
import java.net.NetworkInterface;
import java.net.SocketException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * Borrowed from Chromium's
 * src/net/android/java/src/org/chromium/net/NetworkChangeNotifierAutoDetect.java
 *
 * Used by the NetworkMonitor to listen to platform changes in connectivity.
 * Note that use of this class requires that the app have the platform
 * ACCESS_NETWORK_STATE permission.
 */
public class NetworkMonitorAutoDetect extends BroadcastReceiver {
  public static enum ConnectionType {
    CONNECTION_UNKNOWN,
    CONNECTION_ETHERNET,
    CONNECTION_WIFI,
    CONNECTION_4G,
    CONNECTION_3G,
    CONNECTION_2G,
    CONNECTION_UNKNOWN_CELLULAR,
    CONNECTION_BLUETOOTH,
    CONNECTION_NONE
  }

  public static class IPAddress {
    public final byte[] address;
    public IPAddress(byte[] address) {
      this.address = address;
    }

    @CalledByNative("IPAddress")
    private byte[] getAddress() {
      return address;
    }
  }

  /** Java version of NetworkMonitor.NetworkInformation */
  public static class NetworkInformation {
    public final String name;
    public final ConnectionType type;
    public final long handle;
    public final IPAddress[] ipAddresses;
    public NetworkInformation(
        String name, ConnectionType type, long handle, IPAddress[] addresses) {
      this.name = name;
      this.type = type;
      this.handle = handle;
      this.ipAddresses = addresses;
    }

    @CalledByNative("NetworkInformation")
    private IPAddress[] getIpAddresses() {
      return ipAddresses;
    }

    @CalledByNative("NetworkInformation")
    private ConnectionType getConnectionType() {
      return type;
    }

    @CalledByNative("NetworkInformation")
    private long getHandle() {
      return handle;
    }

    @CalledByNative("NetworkInformation")
    private String getName() {
      return name;
    }
  };

  static class NetworkState {
    private final boolean connected;
    // Defined from ConnectivityManager.TYPE_XXX for non-mobile; for mobile, it is
    // further divided into 2G, 3G, or 4G from the subtype.
    private final int type;
    // Defined from NetworkInfo.subtype, which is one of the TelephonyManager.NETWORK_TYPE_XXXs.
    // Will be useful to find the maximum bandwidth.
    private final int subtype;

    public NetworkState(boolean connected, int type, int subtype) {
      this.connected = connected;
      this.type = type;
      this.subtype = subtype;
    }

    public boolean isConnected() {
      return connected;
    }

    public int getNetworkType() {
      return type;
    }

    public int getNetworkSubType() {
      return subtype;
    }
  }
  /**
   * The methods in this class get called when the network changes if the callback
   * is registered with a proper network request. It is only available in Android Lollipop
   * and above.
   */
  @SuppressLint("NewApi")
  private class SimpleNetworkCallback extends NetworkCallback {
    @Override
    public void onAvailable(Network network) {
      Logging.d(TAG, "Network becomes available: " + network.toString());
      onNetworkChanged(network);
    }

    @Override
    public void onCapabilitiesChanged(Network network, NetworkCapabilities networkCapabilities) {
      // A capabilities change may indicate the ConnectionType has changed,
      // so forward the new NetworkInformation along to the observer.
      Logging.d(TAG, "capabilities changed: " + networkCapabilities.toString());
      onNetworkChanged(network);
    }

    @Override
    public void onLinkPropertiesChanged(Network network, LinkProperties linkProperties) {
      // A link property change may indicate the IP address changes.
      // so forward the new NetworkInformation to the observer.
      Logging.d(TAG, "link properties changed: " + linkProperties.toString());
      onNetworkChanged(network);
    }

    @Override
    public void onLosing(Network network, int maxMsToLive) {
      // Tell the network is going to lose in MaxMsToLive milliseconds.
      // We may use this signal later.
      Logging.d(
          TAG, "Network " + network.toString() + " is about to lose in " + maxMsToLive + "ms");
    }

    @Override
    public void onLost(Network network) {
      Logging.d(TAG, "Network " + network.toString() + " is disconnected");
      observer.onNetworkDisconnect(networkToNetId(network));
    }

    private void onNetworkChanged(Network network) {
      NetworkInformation networkInformation = connectivityManagerDelegate.networkToInfo(network);
      if (networkInformation != null) {
        observer.onNetworkConnect(networkInformation);
      }
    }
  }

  /** Queries the ConnectivityManager for information about the current connection. */
  static class ConnectivityManagerDelegate {
    /**
     *  Note: In some rare Android systems connectivityManager is null.  We handle that
     *  gracefully below.
     */
    private final ConnectivityManager connectivityManager;

    ConnectivityManagerDelegate(Context context) {
      connectivityManager =
          (ConnectivityManager) context.getSystemService(Context.CONNECTIVITY_SERVICE);
    }

    // For testing.
    ConnectivityManagerDelegate() {
      // All the methods below should be overridden.
      connectivityManager = null;
    }

    /**
     * Returns connection type and status information about the current
     * default network.
     */
    NetworkState getNetworkState() {
      if (connectivityManager == null) {
        return new NetworkState(false, -1, -1);
      }
      return getNetworkState(connectivityManager.getActiveNetworkInfo());
    }

    /**
     * Returns connection type and status information about |network|.
     * Only callable on Lollipop and newer releases.
     */
    @SuppressLint("NewApi")
    NetworkState getNetworkState(Network network) {
      if (connectivityManager == null) {
        return new NetworkState(false, -1, -1);
      }
      return getNetworkState(connectivityManager.getNetworkInfo(network));
    }

    /**
     * Returns connection type and status information gleaned from networkInfo.
     */
    NetworkState getNetworkState(NetworkInfo networkInfo) {
      if (networkInfo == null || !networkInfo.isConnected()) {
        return new NetworkState(false, -1, -1);
      }
      return new NetworkState(true, networkInfo.getType(), networkInfo.getSubtype());
    }

    /**
     * Returns all connected networks.
     * Only callable on Lollipop and newer releases.
     */
    @SuppressLint("NewApi")
    Network[] getAllNetworks() {
      if (connectivityManager == null) {
        return new Network[0];
      }
      return connectivityManager.getAllNetworks();
    }

    List<NetworkInformation> getActiveNetworkList() {
      if (!supportNetworkCallback()) {
        return null;
      }
      ArrayList<NetworkInformation> netInfoList = new ArrayList<NetworkInformation>();
      for (Network network : getAllNetworks()) {
        NetworkInformation info = networkToInfo(network);
        if (info != null) {
          netInfoList.add(info);
        }
      }
      return netInfoList;
    }

    /**
     * Returns the NetID of the current default network. Returns
     * INVALID_NET_ID if no current default network connected.
     * Only callable on Lollipop and newer releases.
     */
    @SuppressLint("NewApi")
    long getDefaultNetId() {
      if (!supportNetworkCallback()) {
        return INVALID_NET_ID;
      }
      // Android Lollipop had no API to get the default network; only an
      // API to return the NetworkInfo for the default network. To
      // determine the default network one can find the network with
      // type matching that of the default network.
      final NetworkInfo defaultNetworkInfo = connectivityManager.getActiveNetworkInfo();
      if (defaultNetworkInfo == null) {
        return INVALID_NET_ID;
      }
      final Network[] networks = getAllNetworks();
      long defaultNetId = INVALID_NET_ID;
      for (Network network : networks) {
        if (!hasInternetCapability(network)) {
          continue;
        }
        final NetworkInfo networkInfo = connectivityManager.getNetworkInfo(network);
        if (networkInfo != null && networkInfo.getType() == defaultNetworkInfo.getType()) {
          // There should not be multiple connected networks of the
          // same type. At least as of Android Marshmallow this is
          // not supported. If this becomes supported this assertion
          // may trigger. At that point we could consider using
          // ConnectivityManager.getDefaultNetwork() though this
          // may give confusing results with VPNs and is only
          // available with Android Marshmallow.
          if (defaultNetId != INVALID_NET_ID) {
            throw new RuntimeException(
                "Multiple connected networks of same type are not supported.");
          }
          defaultNetId = networkToNetId(network);
        }
      }
      return defaultNetId;
    }

    @SuppressLint("NewApi")
    private NetworkInformation networkToInfo(Network network) {
      LinkProperties linkProperties = connectivityManager.getLinkProperties(network);
      // getLinkProperties will return null if the network is unknown.
      if (linkProperties == null) {
        Logging.w(TAG, "Detected unknown network: " + network.toString());
        return null;
      }
      if (linkProperties.getInterfaceName() == null) {
        Logging.w(TAG, "Null interface name for network " + network.toString());
        return null;
      }

      NetworkState networkState = getNetworkState(network);
      if (networkState.connected && networkState.getNetworkType() == ConnectivityManager.TYPE_VPN) {
        // If a VPN network is in place, we can find the underlying network type via querying the
        // active network info thanks to
        // https://android.googlesource.com/platform/frameworks/base/+/d6a7980d
        networkState = getNetworkState();
      }
      ConnectionType connectionType = getConnectionType(networkState);
      if (connectionType == ConnectionType.CONNECTION_NONE) {
        // This may not be an error. The OS may signal a network event with connection type
        // NONE when the network disconnects.
        Logging.d(TAG, "Network " + network.toString() + " is disconnected");
        return null;
      }

      // Some android device may return a CONNECTION_UNKNOWN_CELLULAR or CONNECTION_UNKNOWN type,
      // which appears to be usable. Just log them here.
      if (connectionType == ConnectionType.CONNECTION_UNKNOWN
          || connectionType == ConnectionType.CONNECTION_UNKNOWN_CELLULAR) {
        Logging.d(TAG, "Network " + network.toString() + " connection type is " + connectionType
                + " because it has type " + networkState.getNetworkType() + " and subtype "
                + networkState.getNetworkSubType());
      }

      NetworkInformation networkInformation =
          new NetworkInformation(linkProperties.getInterfaceName(), connectionType,
              networkToNetId(network), getIPAddresses(linkProperties));
      return networkInformation;
    }

    /**
     * Returns true if {@code network} can provide Internet access. Can be used to
     * ignore specialized networks (e.g. IMS, FOTA).
     */
    @SuppressLint("NewApi")
    boolean hasInternetCapability(Network network) {
      if (connectivityManager == null) {
        return false;
      }
      final NetworkCapabilities capabilities = connectivityManager.getNetworkCapabilities(network);
      return capabilities != null
          && capabilities.hasCapability(NetworkCapabilities.NET_CAPABILITY_INTERNET);
    }

    /** Only callable on Lollipop and newer releases. */
    @SuppressLint("NewApi")
    public void registerNetworkCallback(NetworkCallback networkCallback) {
      connectivityManager.registerNetworkCallback(
          new NetworkRequest.Builder()
              .addCapability(NetworkCapabilities.NET_CAPABILITY_INTERNET)
              .build(),
          networkCallback);
    }

    /** Only callable on Lollipop and newer releases. */
    @SuppressLint("NewApi")
    public void requestMobileNetwork(NetworkCallback networkCallback) {
      NetworkRequest.Builder builder = new NetworkRequest.Builder();
      builder.addCapability(NetworkCapabilities.NET_CAPABILITY_INTERNET)
          .addTransportType(NetworkCapabilities.TRANSPORT_CELLULAR);
      connectivityManager.requestNetwork(builder.build(), networkCallback);
    }

    @SuppressLint("NewApi")
    IPAddress[] getIPAddresses(LinkProperties linkProperties) {
      IPAddress[] ipAddresses = new IPAddress[linkProperties.getLinkAddresses().size()];
      int i = 0;
      for (LinkAddress linkAddress : linkProperties.getLinkAddresses()) {
        ipAddresses[i] = new IPAddress(linkAddress.getAddress().getAddress());
        ++i;
      }
      return ipAddresses;
    }

    @SuppressLint("NewApi")
    public void releaseCallback(NetworkCallback networkCallback) {
      if (supportNetworkCallback()) {
        Logging.d(TAG, "Unregister network callback");
        connectivityManager.unregisterNetworkCallback(networkCallback);
      }
    }

    public boolean supportNetworkCallback() {
      return Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP && connectivityManager != null;
    }
  }

  /** Queries the WifiManager for SSID of the current Wifi connection. */
  static class WifiManagerDelegate {
    private final Context context;
    WifiManagerDelegate(Context context) {
      this.context = context;
    }

    // For testing.
    WifiManagerDelegate() {
      // All the methods below should be overridden.
      context = null;
    }

    String getWifiSSID() {
      final Intent intent = context.registerReceiver(
          null, new IntentFilter(WifiManager.NETWORK_STATE_CHANGED_ACTION));
      if (intent != null) {
        final WifiInfo wifiInfo = intent.getParcelableExtra(WifiManager.EXTRA_WIFI_INFO);
        if (wifiInfo != null) {
          final String ssid = wifiInfo.getSSID();
          if (ssid != null) {
            return ssid;
          }
        }
      }
      return "";
    }
  }

  /** Maintains the information about wifi direct (aka WifiP2p) networks. */
  static class WifiDirectManagerDelegate extends BroadcastReceiver {
    // Network "handle" for the Wifi P2p network. We have to bind to the default network id
    // (NETWORK_UNSPECIFIED) for these addresses.
    private static final int WIFI_P2P_NETWORK_HANDLE = 0;
    private final Context context;
    private final Observer observer;
    // Network information about a WifiP2p (aka WiFi-Direct) network, or null if no such network is
    // connected.
    private NetworkInformation wifiP2pNetworkInfo = null;

    WifiDirectManagerDelegate(Observer observer, Context context) {
      this.context = context;
      this.observer = observer;
      IntentFilter intentFilter = new IntentFilter();
      intentFilter.addAction(WifiP2pManager.WIFI_P2P_STATE_CHANGED_ACTION);
      intentFilter.addAction(WifiP2pManager.WIFI_P2P_CONNECTION_CHANGED_ACTION);
      context.registerReceiver(this, intentFilter);
    }

    // BroadcastReceiver
    @Override
    @SuppressLint("InlinedApi")
    public void onReceive(Context context, Intent intent) {
      if (WifiP2pManager.WIFI_P2P_CONNECTION_CHANGED_ACTION.equals(intent.getAction())) {
        WifiP2pGroup wifiP2pGroup = intent.getParcelableExtra(WifiP2pManager.EXTRA_WIFI_P2P_GROUP);
        onWifiP2pGroupChange(wifiP2pGroup);
      } else if (WifiP2pManager.WIFI_P2P_STATE_CHANGED_ACTION.equals(intent.getAction())) {
        int state = intent.getIntExtra(WifiP2pManager.EXTRA_WIFI_STATE, 0 /* default to unknown */);
        onWifiP2pStateChange(state);
      }
    }

    /** Releases the broadcast receiver. */
    public void release() {
      context.unregisterReceiver(this);
    }

    public List<NetworkInformation> getActiveNetworkList() {
      if (wifiP2pNetworkInfo != null) {
        return Collections.singletonList(wifiP2pNetworkInfo);
      }

      return Collections.emptyList();
    }

    /** Handle a change notification about the wifi p2p group. */
    private void onWifiP2pGroupChange(WifiP2pGroup wifiP2pGroup) {
      if (wifiP2pGroup == null || wifiP2pGroup.getInterface() == null) {
        return;
      }

      NetworkInterface wifiP2pInterface;
      try {
        wifiP2pInterface = NetworkInterface.getByName(wifiP2pGroup.getInterface());
      } catch (SocketException e) {
        Logging.e(TAG, "Unable to get WifiP2p network interface", e);
        return;
      }

      List<InetAddress> interfaceAddresses = Collections.list(wifiP2pInterface.getInetAddresses());
      IPAddress[] ipAddresses = new IPAddress[interfaceAddresses.size()];
      for (int i = 0; i < interfaceAddresses.size(); ++i) {
        ipAddresses[i] = new IPAddress(interfaceAddresses.get(i).getAddress());
      }

      wifiP2pNetworkInfo =
          new NetworkInformation(
              wifiP2pGroup.getInterface(),
              ConnectionType.CONNECTION_WIFI,
              WIFI_P2P_NETWORK_HANDLE,
              ipAddresses);
      observer.onNetworkConnect(wifiP2pNetworkInfo);
    }

    /** Handle a state change notification about wifi p2p. */
    private void onWifiP2pStateChange(int state) {
      if (state == WifiP2pManager.WIFI_P2P_STATE_DISABLED) {
        wifiP2pNetworkInfo = null;
        observer.onNetworkDisconnect(WIFI_P2P_NETWORK_HANDLE);
      }
    }
  }

  static final long INVALID_NET_ID = -1;
  private static final String TAG = "NetworkMonitorAutoDetect";

  // Observer for the connection type change.
  private final Observer observer;
  private final IntentFilter intentFilter;
  private final Context context;
  // Used to request mobile network. It does not do anything except for keeping
  // the callback for releasing the request.
  private final NetworkCallback mobileNetworkCallback;
  // Used to receive updates on all networks.
  private final NetworkCallback allNetworkCallback;
  // connectivityManagerDelegate and wifiManagerDelegate are only non-final for testing.
  private ConnectivityManagerDelegate connectivityManagerDelegate;
  private WifiManagerDelegate wifiManagerDelegate;
  private WifiDirectManagerDelegate wifiDirectManagerDelegate;

  private boolean isRegistered;
  private ConnectionType connectionType;
  private String wifiSSID;

  /**
   * Observer interface by which observer is notified of network changes.
   */
  public static interface Observer {
    /**
     * Called when default network changes.
     */
    public void onConnectionTypeChanged(ConnectionType newConnectionType);
    public void onNetworkConnect(NetworkInformation networkInfo);
    public void onNetworkDisconnect(long networkHandle);
  }

  /**
   * Constructs a NetworkMonitorAutoDetect. Should only be called on UI thread.
   */
  @SuppressLint("NewApi")
  public NetworkMonitorAutoDetect(Observer observer, Context context) {
    this.observer = observer;
    this.context = context;
    connectivityManagerDelegate = new ConnectivityManagerDelegate(context);
    wifiManagerDelegate = new WifiManagerDelegate(context);

    final NetworkState networkState = connectivityManagerDelegate.getNetworkState();
    connectionType = getConnectionType(networkState);
    wifiSSID = getWifiSSID(networkState);
    intentFilter = new IntentFilter(ConnectivityManager.CONNECTIVITY_ACTION);

    if (PeerConnectionFactory.fieldTrialsFindFullName("IncludeWifiDirect").equals("Enabled")) {
      wifiDirectManagerDelegate = new WifiDirectManagerDelegate(observer, context);
    }

    registerReceiver();
    if (connectivityManagerDelegate.supportNetworkCallback()) {
      // On Android 6.0.0, the WRITE_SETTINGS permission is necessary for
      // requestNetwork, so it will fail. This was fixed in Android 6.0.1.
      NetworkCallback tempNetworkCallback = new NetworkCallback();
      try {
        connectivityManagerDelegate.requestMobileNetwork(tempNetworkCallback);
      } catch (java.lang.SecurityException e) {
        Logging.w(TAG, "Unable to obtain permission to request a cellular network.");
        tempNetworkCallback = null;
      }
      mobileNetworkCallback = tempNetworkCallback;
      allNetworkCallback = new SimpleNetworkCallback();
      connectivityManagerDelegate.registerNetworkCallback(allNetworkCallback);
    } else {
      mobileNetworkCallback = null;
      allNetworkCallback = null;
    }
  }

  public boolean supportNetworkCallback() {
    return connectivityManagerDelegate.supportNetworkCallback();
  }

  /**
   * Allows overriding the ConnectivityManagerDelegate for tests.
   */
  void setConnectivityManagerDelegateForTests(ConnectivityManagerDelegate delegate) {
    connectivityManagerDelegate = delegate;
  }

  /**
   * Allows overriding the WifiManagerDelegate for tests.
   */
  void setWifiManagerDelegateForTests(WifiManagerDelegate delegate) {
    wifiManagerDelegate = delegate;
  }

  /**
   * Returns whether the object has registered to receive network connectivity intents.
   * Visible for testing.
   */
  boolean isReceiverRegisteredForTesting() {
    return isRegistered;
  }

  List<NetworkInformation> getActiveNetworkList() {
    List<NetworkInformation> connectivityManagerList =
        connectivityManagerDelegate.getActiveNetworkList();
    if (connectivityManagerList == null) {
      return null;
    }
    ArrayList<NetworkInformation> result =
        new ArrayList<NetworkInformation>(connectivityManagerList);
    if (wifiDirectManagerDelegate != null) {
      result.addAll(wifiDirectManagerDelegate.getActiveNetworkList());
    }
    return result;
  }

  public void destroy() {
    if (allNetworkCallback != null) {
      connectivityManagerDelegate.releaseCallback(allNetworkCallback);
    }
    if (mobileNetworkCallback != null) {
      connectivityManagerDelegate.releaseCallback(mobileNetworkCallback);
    }
    if (wifiDirectManagerDelegate != null) {
      wifiDirectManagerDelegate.release();
    }
    unregisterReceiver();
  }

  /**
   * Registers a BroadcastReceiver in the given context.
   */
  private void registerReceiver() {
    if (isRegistered)
      return;

    isRegistered = true;
    context.registerReceiver(this, intentFilter);
  }

  /**
   * Unregisters the BroadcastReceiver in the given context.
   */
  private void unregisterReceiver() {
    if (!isRegistered)
      return;

    isRegistered = false;
    context.unregisterReceiver(this);
  }

  public NetworkState getCurrentNetworkState() {
    return connectivityManagerDelegate.getNetworkState();
  }

  /**
   * Returns NetID of device's current default connected network used for
   * communication.
   * Only implemented on Lollipop and newer releases, returns INVALID_NET_ID
   * when not implemented.
   */
  public long getDefaultNetId() {
    return connectivityManagerDelegate.getDefaultNetId();
  }

  public static ConnectionType getConnectionType(NetworkState networkState) {
    if (!networkState.isConnected()) {
      return ConnectionType.CONNECTION_NONE;
    }

    switch (networkState.getNetworkType()) {
      case ConnectivityManager.TYPE_ETHERNET:
        return ConnectionType.CONNECTION_ETHERNET;
      case ConnectivityManager.TYPE_WIFI:
        return ConnectionType.CONNECTION_WIFI;
      case ConnectivityManager.TYPE_WIMAX:
        return ConnectionType.CONNECTION_4G;
      case ConnectivityManager.TYPE_BLUETOOTH:
        return ConnectionType.CONNECTION_BLUETOOTH;
      case ConnectivityManager.TYPE_MOBILE:
        // Use information from TelephonyManager to classify the connection.
        switch (networkState.getNetworkSubType()) {
          case TelephonyManager.NETWORK_TYPE_GPRS:
          case TelephonyManager.NETWORK_TYPE_EDGE:
          case TelephonyManager.NETWORK_TYPE_CDMA:
          case TelephonyManager.NETWORK_TYPE_1xRTT:
          case TelephonyManager.NETWORK_TYPE_IDEN:
            return ConnectionType.CONNECTION_2G;
          case TelephonyManager.NETWORK_TYPE_UMTS:
          case TelephonyManager.NETWORK_TYPE_EVDO_0:
          case TelephonyManager.NETWORK_TYPE_EVDO_A:
          case TelephonyManager.NETWORK_TYPE_HSDPA:
          case TelephonyManager.NETWORK_TYPE_HSUPA:
          case TelephonyManager.NETWORK_TYPE_HSPA:
          case TelephonyManager.NETWORK_TYPE_EVDO_B:
          case TelephonyManager.NETWORK_TYPE_EHRPD:
          case TelephonyManager.NETWORK_TYPE_HSPAP:
            return ConnectionType.CONNECTION_3G;
          case TelephonyManager.NETWORK_TYPE_LTE:
            return ConnectionType.CONNECTION_4G;
          default:
            return ConnectionType.CONNECTION_UNKNOWN_CELLULAR;
        }
      default:
        return ConnectionType.CONNECTION_UNKNOWN;
    }
  }

  private String getWifiSSID(NetworkState networkState) {
    if (getConnectionType(networkState) != ConnectionType.CONNECTION_WIFI)
      return "";
    return wifiManagerDelegate.getWifiSSID();
  }

  // BroadcastReceiver
  @Override
  public void onReceive(Context context, Intent intent) {
    final NetworkState networkState = getCurrentNetworkState();
    if (ConnectivityManager.CONNECTIVITY_ACTION.equals(intent.getAction())) {
      connectionTypeChanged(networkState);
    }
  }

  private void connectionTypeChanged(NetworkState networkState) {
    ConnectionType newConnectionType = getConnectionType(networkState);
    String newWifiSSID = getWifiSSID(networkState);
    if (newConnectionType == connectionType && newWifiSSID.equals(wifiSSID))
      return;

    connectionType = newConnectionType;
    wifiSSID = newWifiSSID;
    Logging.d(TAG, "Network connectivity changed, type is: " + connectionType);
    observer.onConnectionTypeChanged(newConnectionType);
  }

  /**
   * Extracts NetID of network on Lollipop and NetworkHandle (which is mungled
   * NetID) on Marshmallow and newer releases. Only available on Lollipop and
   * newer releases. Returns long since getNetworkHandle returns long.
   */
  @SuppressLint("NewApi")
  private static long networkToNetId(Network network) {
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
      return network.getNetworkHandle();
    }

    // NOTE(honghaiz): This depends on Android framework implementation details.
    // These details cannot change because Lollipop has been released.
    return Integer.parseInt(network.toString());
  }
}
