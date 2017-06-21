/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_SDK_ANDROID_SRC_JNI_ANDROIDNETWORKMONITOR_JNI_H_
#define WEBRTC_SDK_ANDROID_SRC_JNI_ANDROIDNETWORKMONITOR_JNI_H_

#include "webrtc/base/networkmonitor.h"

#include <stdint.h>

#include <map>

#include "webrtc/sdk/android/src/jni/jni_helpers.h"
#include "webrtc/base/thread_checker.h"

namespace webrtc_jni {

typedef int64_t NetworkHandle;

// c++ equivalent of java NetworkMonitorAutoDetect.ConnectionType.
enum NetworkType {
  NETWORK_UNKNOWN,
  NETWORK_ETHERNET,
  NETWORK_WIFI,
  NETWORK_4G,
  NETWORK_3G,
  NETWORK_2G,
  NETWORK_UNKNOWN_CELLULAR,
  NETWORK_BLUETOOTH,
  NETWORK_NONE
};

// The information is collected from Android OS so that the native code can get
// the network type and handle (Android network ID) for each interface.
struct NetworkInformation {
  std::string interface_name;
  NetworkHandle handle;
  NetworkType type;
  std::vector<rtc::IPAddress> ip_addresses;

  std::string ToString() const;
};

class AndroidNetworkMonitor : public rtc::NetworkMonitorBase,
                              public rtc::NetworkBinderInterface {
 public:
  AndroidNetworkMonitor();

  // TODO(sakal): Remove once down stream dependencies have been updated.
  static void SetAndroidContext(JNIEnv* jni, jobject context) {}

  void Start() override;
  void Stop() override;

  rtc::NetworkBindingResult BindSocketToNetwork(
      int socket_fd,
      const rtc::IPAddress& address) override;
  rtc::AdapterType GetAdapterType(const std::string& if_name) override;
  void OnNetworkConnected(const NetworkInformation& network_info);
  void OnNetworkDisconnected(NetworkHandle network_handle);
  void SetNetworkInfos(const std::vector<NetworkInformation>& network_infos);

 private:
  static jobject application_context_;
  static int android_sdk_int_;
  JNIEnv* jni() { return AttachCurrentThreadIfNeeded(); }

  void OnNetworkConnected_w(const NetworkInformation& network_info);
  void OnNetworkDisconnected_w(NetworkHandle network_handle);

  ScopedGlobalRef<jclass> j_network_monitor_class_;
  ScopedGlobalRef<jobject> j_network_monitor_;
  rtc::ThreadChecker thread_checker_;
  bool started_ = false;
  std::map<std::string, rtc::AdapterType> adapter_type_by_name_;
  std::map<rtc::IPAddress, NetworkHandle> network_handle_by_address_;
  std::map<NetworkHandle, NetworkInformation> network_info_by_handle_;
};

class AndroidNetworkMonitorFactory : public rtc::NetworkMonitorFactory {
 public:
  AndroidNetworkMonitorFactory() {}

  rtc::NetworkMonitorInterface* CreateNetworkMonitor() override;
};

}  // namespace webrtc_jni

#endif  // WEBRTC_SDK_ANDROID_SRC_JNI_ANDROIDNETWORKMONITOR_JNI_H_
