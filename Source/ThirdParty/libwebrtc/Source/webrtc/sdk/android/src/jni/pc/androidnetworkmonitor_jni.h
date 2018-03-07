/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SDK_ANDROID_SRC_JNI_PC_ANDROIDNETWORKMONITOR_JNI_H_
#define SDK_ANDROID_SRC_JNI_PC_ANDROIDNETWORKMONITOR_JNI_H_

#include <stdint.h>
#include <map>
#include <string>
#include <vector>

#include "rtc_base/networkmonitor.h"
#include "rtc_base/thread_checker.h"
#include "sdk/android/src/jni/jni_helpers.h"

namespace webrtc {
namespace jni {

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
  explicit AndroidNetworkMonitor(JNIEnv* env);

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
  // Always expected to be called on the network thread.
  void SetNetworkInfos(const std::vector<NetworkInformation>& network_infos);

  void NotifyConnectionTypeChanged(JNIEnv* env, jobject j_caller);
  void NotifyOfNetworkConnect(JNIEnv* env,
                              jobject j_caller,
                              jobject j_network_info);
  void NotifyOfNetworkDisconnect(JNIEnv* env,
                                 jobject j_caller,
                                 jlong network_handle);
  void NotifyOfActiveNetworkList(JNIEnv* env,
                                 jobject j_caller,
                                 jobjectArray j_network_infos);

 private:
  void OnNetworkConnected_w(const NetworkInformation& network_info);
  void OnNetworkDisconnected_w(NetworkHandle network_handle);

  const int android_sdk_int_;
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

}  // namespace jni
}  // namespace webrtc

// TODO(magjed): Remove once external clients are updated.
namespace webrtc_jni {

using webrtc::jni::AndroidNetworkMonitor;
using webrtc::jni::AndroidNetworkMonitorFactory;

}  // namespace webrtc_jni

#endif  // SDK_ANDROID_SRC_JNI_PC_ANDROIDNETWORKMONITOR_JNI_H_
