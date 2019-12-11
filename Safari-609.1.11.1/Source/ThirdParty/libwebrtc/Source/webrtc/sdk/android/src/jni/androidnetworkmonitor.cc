/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "sdk/android/src/jni/androidnetworkmonitor.h"

#include <dlfcn.h>
#ifndef RTLD_NOLOAD
// This was added in Lollipop to dlfcn.h
#define RTLD_NOLOAD 4
#endif

#include "rtc_base/bind.h"
#include "rtc_base/checks.h"
#include "rtc_base/ipaddress.h"
#include "rtc_base/logging.h"
#include "rtc_base/strings/string_builder.h"
#include "sdk/android/generated_base_jni/jni/NetworkMonitorAutoDetect_jni.h"
#include "sdk/android/generated_base_jni/jni/NetworkMonitor_jni.h"
#include "sdk/android/native_api/jni/java_types.h"
#include "sdk/android/src/jni/jni_helpers.h"

namespace webrtc {
namespace jni {

enum AndroidSdkVersion {
  SDK_VERSION_LOLLIPOP = 21,
  SDK_VERSION_MARSHMALLOW = 23
};

static NetworkType GetNetworkTypeFromJava(
    JNIEnv* jni,
    const JavaRef<jobject>& j_network_type) {
  std::string enum_name = GetJavaEnumName(jni, j_network_type);
  if (enum_name == "CONNECTION_UNKNOWN") {
    return NetworkType::NETWORK_UNKNOWN;
  }
  if (enum_name == "CONNECTION_ETHERNET") {
    return NetworkType::NETWORK_ETHERNET;
  }
  if (enum_name == "CONNECTION_WIFI") {
    return NetworkType::NETWORK_WIFI;
  }
  if (enum_name == "CONNECTION_4G") {
    return NetworkType::NETWORK_4G;
  }
  if (enum_name == "CONNECTION_3G") {
    return NetworkType::NETWORK_3G;
  }
  if (enum_name == "CONNECTION_2G") {
    return NetworkType::NETWORK_2G;
  }
  if (enum_name == "CONNECTION_UNKNOWN_CELLULAR") {
    return NetworkType::NETWORK_UNKNOWN_CELLULAR;
  }
  if (enum_name == "CONNECTION_BLUETOOTH") {
    return NetworkType::NETWORK_BLUETOOTH;
  }
  if (enum_name == "CONNECTION_VPN") {
    return NetworkType::NETWORK_VPN;
  }
  if (enum_name == "CONNECTION_NONE") {
    return NetworkType::NETWORK_NONE;
  }
  RTC_NOTREACHED();
  return NetworkType::NETWORK_UNKNOWN;
}

static rtc::AdapterType AdapterTypeFromNetworkType(NetworkType network_type) {
  switch (network_type) {
    case NETWORK_UNKNOWN:
      return rtc::ADAPTER_TYPE_UNKNOWN;
    case NETWORK_ETHERNET:
      return rtc::ADAPTER_TYPE_ETHERNET;
    case NETWORK_WIFI:
      return rtc::ADAPTER_TYPE_WIFI;
    case NETWORK_4G:
    case NETWORK_3G:
    case NETWORK_2G:
    case NETWORK_UNKNOWN_CELLULAR:
      return rtc::ADAPTER_TYPE_CELLULAR;
    case NETWORK_VPN:
    case NETWORK_BLUETOOTH:
      // There is no corresponding mapping for bluetooth networks.
      // Map it to VPN for now.
      return rtc::ADAPTER_TYPE_VPN;
    default:
      RTC_NOTREACHED() << "Invalid network type " << network_type;
      return rtc::ADAPTER_TYPE_UNKNOWN;
  }
}

static rtc::IPAddress JavaToNativeIpAddress(
    JNIEnv* jni,
    const JavaRef<jobject>& j_ip_address) {
  ScopedJavaLocalRef<jbyteArray> j_addresses =
      Java_IPAddress_getAddress(jni, j_ip_address);
  size_t address_length = jni->GetArrayLength(j_addresses.obj());
  jbyte* addr_array = jni->GetByteArrayElements(j_addresses.obj(), nullptr);
  CHECK_EXCEPTION(jni) << "Error during JavaToNativeIpAddress";
  if (address_length == 4) {
    // IP4
    struct in_addr ip4_addr;
    memcpy(&ip4_addr.s_addr, addr_array, 4);
    jni->ReleaseByteArrayElements(j_addresses.obj(), addr_array, JNI_ABORT);
    return rtc::IPAddress(ip4_addr);
  }
  // IP6
  RTC_CHECK(address_length == 16);
  struct in6_addr ip6_addr;
  memcpy(ip6_addr.s6_addr, addr_array, address_length);
  jni->ReleaseByteArrayElements(j_addresses.obj(), addr_array, JNI_ABORT);
  return rtc::IPAddress(ip6_addr);
}

static NetworkInformation GetNetworkInformationFromJava(
    JNIEnv* jni,
    const JavaRef<jobject>& j_network_info) {
  NetworkInformation network_info;
  network_info.interface_name = JavaToStdString(
      jni, Java_NetworkInformation_getName(jni, j_network_info));
  network_info.handle = static_cast<NetworkHandle>(
      Java_NetworkInformation_getHandle(jni, j_network_info));
  network_info.type = GetNetworkTypeFromJava(
      jni, Java_NetworkInformation_getConnectionType(jni, j_network_info));
  network_info.underlying_type_for_vpn = GetNetworkTypeFromJava(
      jni, Java_NetworkInformation_getUnderlyingConnectionTypeForVpn(
               jni, j_network_info));
  ScopedJavaLocalRef<jobjectArray> j_ip_addresses =
      Java_NetworkInformation_getIpAddresses(jni, j_network_info);
  network_info.ip_addresses = JavaToNativeVector<rtc::IPAddress>(
      jni, j_ip_addresses, &JavaToNativeIpAddress);
  return network_info;
}

NetworkInformation::NetworkInformation() = default;

NetworkInformation::NetworkInformation(const NetworkInformation&) = default;

NetworkInformation::NetworkInformation(NetworkInformation&&) = default;

NetworkInformation::~NetworkInformation() = default;

NetworkInformation& NetworkInformation::operator=(const NetworkInformation&) =
    default;

NetworkInformation& NetworkInformation::operator=(NetworkInformation&&) =
    default;

std::string NetworkInformation::ToString() const {
  rtc::StringBuilder ss;
  ss << "NetInfo[name " << interface_name << "; handle " << handle << "; type "
     << type;
  if (type == NETWORK_VPN) {
    ss << "; underlying_type_for_vpn " << underlying_type_for_vpn;
  }
  ss << "; address";
  for (const rtc::IPAddress address : ip_addresses) {
    ss << " " << address.ToString();
  }
  ss << "]";
  return ss.Release();
}

AndroidNetworkMonitor::AndroidNetworkMonitor(
    JNIEnv* env,
    const JavaRef<jobject>& j_application_context)
    : android_sdk_int_(Java_NetworkMonitor_androidSdkInt(env)),
      j_application_context_(env, j_application_context),
      j_network_monitor_(env, Java_NetworkMonitor_getInstance(env)) {}

AndroidNetworkMonitor::~AndroidNetworkMonitor() = default;

void AndroidNetworkMonitor::Start() {
  RTC_CHECK(thread_checker_.CalledOnValidThread());
  if (started_) {
    return;
  }
  started_ = true;

  // This is kind of magic behavior, but doing this allows the SocketServer to
  // use this as a NetworkBinder to bind sockets on a particular network when
  // it creates sockets.
  worker_thread()->socketserver()->set_network_binder(this);

  JNIEnv* env = AttachCurrentThreadIfNeeded();
  Java_NetworkMonitor_startMonitoring(
      env, j_network_monitor_, j_application_context_, jlongFromPointer(this));
}

void AndroidNetworkMonitor::Stop() {
  RTC_CHECK(thread_checker_.CalledOnValidThread());
  if (!started_) {
    return;
  }
  started_ = false;

  // Once the network monitor stops, it will clear all network information and
  // it won't find the network handle to bind anyway.
  if (worker_thread()->socketserver()->network_binder() == this) {
    worker_thread()->socketserver()->set_network_binder(nullptr);
  }

  JNIEnv* env = AttachCurrentThreadIfNeeded();
  Java_NetworkMonitor_stopMonitoring(env, j_network_monitor_,
                                     jlongFromPointer(this));

  network_handle_by_address_.clear();
  network_info_by_handle_.clear();
}

// The implementation is largely taken from UDPSocketPosix::BindToNetwork in
// https://cs.chromium.org/chromium/src/net/udp/udp_socket_posix.cc
rtc::NetworkBindingResult AndroidNetworkMonitor::BindSocketToNetwork(
    int socket_fd,
    const rtc::IPAddress& address) {
  RTC_CHECK(thread_checker_.CalledOnValidThread());

  // Android prior to Lollipop didn't have support for binding sockets to
  // networks. This may also occur if there is no connectivity manager service.
  JNIEnv* env = AttachCurrentThreadIfNeeded();
  const bool network_binding_supported =
      Java_NetworkMonitor_networkBindingSupported(env, j_network_monitor_);
  if (!network_binding_supported) {
    RTC_LOG(LS_WARNING)
        << "BindSocketToNetwork is not supported on this platform "
        << "(Android SDK: " << android_sdk_int_ << ")";
    return rtc::NetworkBindingResult::NOT_IMPLEMENTED;
  }

  auto iter = network_handle_by_address_.find(address);
  if (iter == network_handle_by_address_.end()) {
    return rtc::NetworkBindingResult::ADDRESS_NOT_FOUND;
  }
  NetworkHandle network_handle = iter->second;

  if (network_handle == 0 /* NETWORK_UNSPECIFIED */) {
    return rtc::NetworkBindingResult::NOT_IMPLEMENTED;
  }

  int rv = 0;
  if (android_sdk_int_ >= SDK_VERSION_MARSHMALLOW) {
    // See declaration of android_setsocknetwork() here:
    // http://androidxref.com/6.0.0_r1/xref/development/ndk/platforms/android-M/include/android/multinetwork.h#65
    // Function cannot be called directly as it will cause app to fail to load
    // on pre-marshmallow devices.
    typedef int (*MarshmallowSetNetworkForSocket)(NetworkHandle net,
                                                  int socket);
    static MarshmallowSetNetworkForSocket marshmallowSetNetworkForSocket;
    // This is not thread-safe, but we are running this only on the worker
    // thread.
    if (!marshmallowSetNetworkForSocket) {
      const std::string android_native_lib_path = "libandroid.so";
      void* lib = dlopen(android_native_lib_path.c_str(), RTLD_NOW);
      if (lib == nullptr) {
        RTC_LOG(LS_ERROR) << "Library " << android_native_lib_path
                          << " not found!";
        return rtc::NetworkBindingResult::NOT_IMPLEMENTED;
      }
      marshmallowSetNetworkForSocket =
          reinterpret_cast<MarshmallowSetNetworkForSocket>(
              dlsym(lib, "android_setsocknetwork"));
    }
    if (!marshmallowSetNetworkForSocket) {
      RTC_LOG(LS_ERROR) << "Symbol marshmallowSetNetworkForSocket is not found";
      return rtc::NetworkBindingResult::NOT_IMPLEMENTED;
    }
    rv = marshmallowSetNetworkForSocket(network_handle, socket_fd);
  } else {
    // NOTE: This relies on Android implementation details, but it won't change
    // because Lollipop is already released.
    typedef int (*LollipopSetNetworkForSocket)(unsigned net, int socket);
    static LollipopSetNetworkForSocket lollipopSetNetworkForSocket;
    // This is not threadsafe, but we are running this only on the worker
    // thread.
    if (!lollipopSetNetworkForSocket) {
      // Android's netd client library should always be loaded in our address
      // space as it shims libc functions like connect().
      const std::string net_library_path = "libnetd_client.so";
      // Use RTLD_NOW to match Android's prior loading of the library:
      // http://androidxref.com/6.0.0_r5/xref/bionic/libc/bionic/NetdClient.cpp#37
      // Use RTLD_NOLOAD to assert that the library is already loaded and
      // avoid doing any disk IO.
      void* lib = dlopen(net_library_path.c_str(), RTLD_NOW | RTLD_NOLOAD);
      if (lib == nullptr) {
        RTC_LOG(LS_ERROR) << "Library " << net_library_path << " not found!";
        return rtc::NetworkBindingResult::NOT_IMPLEMENTED;
      }
      lollipopSetNetworkForSocket =
          reinterpret_cast<LollipopSetNetworkForSocket>(
              dlsym(lib, "setNetworkForSocket"));
    }
    if (!lollipopSetNetworkForSocket) {
      RTC_LOG(LS_ERROR) << "Symbol lollipopSetNetworkForSocket is not found ";
      return rtc::NetworkBindingResult::NOT_IMPLEMENTED;
    }
    rv = lollipopSetNetworkForSocket(network_handle, socket_fd);
  }

  // If |network| has since disconnected, |rv| will be ENONET.  Surface this as
  // ERR_NETWORK_CHANGED, rather than MapSystemError(ENONET) which gives back
  // the less descriptive ERR_FAILED.
  if (rv == 0) {
    return rtc::NetworkBindingResult::SUCCESS;
  }
  if (rv == ENONET) {
    return rtc::NetworkBindingResult::NETWORK_CHANGED;
  }
  return rtc::NetworkBindingResult::FAILURE;
}

void AndroidNetworkMonitor::OnNetworkConnected(
    const NetworkInformation& network_info) {
  worker_thread()->Invoke<void>(
      RTC_FROM_HERE, rtc::Bind(&AndroidNetworkMonitor::OnNetworkConnected_w,
                               this, network_info));
  // Fire SignalNetworksChanged to update the list of networks.
  OnNetworksChanged();
}

void AndroidNetworkMonitor::OnNetworkConnected_w(
    const NetworkInformation& network_info) {
  RTC_LOG(LS_INFO) << "Network connected: " << network_info.ToString();
  adapter_type_by_name_[network_info.interface_name] =
      AdapterTypeFromNetworkType(network_info.type);
  if (network_info.type == NETWORK_VPN) {
    vpn_underlying_adapter_type_by_name_[network_info.interface_name] =
        AdapterTypeFromNetworkType(network_info.underlying_type_for_vpn);
  }
  network_info_by_handle_[network_info.handle] = network_info;
  for (const rtc::IPAddress& address : network_info.ip_addresses) {
    network_handle_by_address_[address] = network_info.handle;
  }
}

void AndroidNetworkMonitor::OnNetworkDisconnected(NetworkHandle handle) {
  RTC_LOG(LS_INFO) << "Network disconnected for handle " << handle;
  worker_thread()->Invoke<void>(
      RTC_FROM_HERE,
      rtc::Bind(&AndroidNetworkMonitor::OnNetworkDisconnected_w, this, handle));
}

void AndroidNetworkMonitor::OnNetworkDisconnected_w(NetworkHandle handle) {
  auto iter = network_info_by_handle_.find(handle);
  if (iter != network_info_by_handle_.end()) {
    for (const rtc::IPAddress& address : iter->second.ip_addresses) {
      network_handle_by_address_.erase(address);
    }
    network_info_by_handle_.erase(iter);
  }
}

void AndroidNetworkMonitor::SetNetworkInfos(
    const std::vector<NetworkInformation>& network_infos) {
  RTC_CHECK(thread_checker_.CalledOnValidThread());
  network_handle_by_address_.clear();
  network_info_by_handle_.clear();
  RTC_LOG(LS_INFO) << "Android network monitor found " << network_infos.size()
                   << " networks";
  for (NetworkInformation network : network_infos) {
    OnNetworkConnected_w(network);
  }
}

rtc::AdapterType AndroidNetworkMonitor::GetAdapterType(
    const std::string& if_name) {
  auto iter = adapter_type_by_name_.find(if_name);
  rtc::AdapterType type = (iter == adapter_type_by_name_.end())
                              ? rtc::ADAPTER_TYPE_UNKNOWN
                              : iter->second;
  if (type == rtc::ADAPTER_TYPE_UNKNOWN) {
    RTC_LOG(LS_WARNING) << "Get an unknown type for the interface " << if_name;
  }
  return type;
}

rtc::AdapterType AndroidNetworkMonitor::GetVpnUnderlyingAdapterType(
    const std::string& if_name) {
  auto iter = vpn_underlying_adapter_type_by_name_.find(if_name);
  rtc::AdapterType type = (iter == vpn_underlying_adapter_type_by_name_.end())
                              ? rtc::ADAPTER_TYPE_UNKNOWN
                              : iter->second;
  return type;
}

AndroidNetworkMonitorFactory::AndroidNetworkMonitorFactory()
    : j_application_context_(nullptr) {}

AndroidNetworkMonitorFactory::AndroidNetworkMonitorFactory(
    JNIEnv* env,
    const JavaRef<jobject>& j_application_context)
    : j_application_context_(env, j_application_context) {}

AndroidNetworkMonitorFactory::~AndroidNetworkMonitorFactory() = default;

rtc::NetworkMonitorInterface*
AndroidNetworkMonitorFactory::CreateNetworkMonitor() {
  return new AndroidNetworkMonitor(AttachCurrentThreadIfNeeded(),
                                   j_application_context_);
}

void AndroidNetworkMonitor::NotifyConnectionTypeChanged(
    JNIEnv* env,
    const JavaRef<jobject>& j_caller) {
  OnNetworksChanged();
}

void AndroidNetworkMonitor::NotifyOfActiveNetworkList(
    JNIEnv* env,
    const JavaRef<jobject>& j_caller,
    const JavaRef<jobjectArray>& j_network_infos) {
  std::vector<NetworkInformation> network_infos =
      JavaToNativeVector<NetworkInformation>(env, j_network_infos,
                                             &GetNetworkInformationFromJava);
  SetNetworkInfos(network_infos);
}

void AndroidNetworkMonitor::NotifyOfNetworkConnect(
    JNIEnv* env,
    const JavaRef<jobject>& j_caller,
    const JavaRef<jobject>& j_network_info) {
  NetworkInformation network_info =
      GetNetworkInformationFromJava(env, j_network_info);
  OnNetworkConnected(network_info);
}

void AndroidNetworkMonitor::NotifyOfNetworkDisconnect(
    JNIEnv* env,
    const JavaRef<jobject>& j_caller,
    jlong network_handle) {
  OnNetworkDisconnected(static_cast<NetworkHandle>(network_handle));
}

}  // namespace jni
}  // namespace webrtc
