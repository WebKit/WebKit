/*
 *  Copyright 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SDK_OBJC_NATIVE_SRC_OBJC_NETWORK_MONITOR_H_
#define SDK_OBJC_NATIVE_SRC_OBJC_NETWORK_MONITOR_H_

#include <vector>

#include "sdk/objc/components/network/RTCNetworkMonitor+Private.h"
#include "sdk/objc/native/src/network_monitor_observer.h"

#include "rtc_base/async_invoker.h"
#include "rtc_base/network_monitor.h"
#include "rtc_base/network_monitor_factory.h"
#include "rtc_base/synchronization/sequence_checker.h"
#include "rtc_base/thread.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc {

class ObjCNetworkMonitorFactory : public rtc::NetworkMonitorFactory {
 public:
  ObjCNetworkMonitorFactory() = default;
  ~ObjCNetworkMonitorFactory() override = default;

  rtc::NetworkMonitorInterface* CreateNetworkMonitor() override;
};

class ObjCNetworkMonitor : public rtc::NetworkMonitorInterface,
                           public NetworkMonitorObserver {
 public:
  ObjCNetworkMonitor() = default;
  ~ObjCNetworkMonitor() override;

  void Start() override;
  void Stop() override;

  rtc::AdapterType GetAdapterType(const std::string& interface_name) override;
  rtc::AdapterType GetVpnUnderlyingAdapterType(
      const std::string& interface_name) override;
  rtc::NetworkPreference GetNetworkPreference(
      const std::string& interface_name) override;
  bool IsAdapterAvailable(const std::string& interface_name) override;

  // NetworkMonitorObserver override.
  // Fans out updates to observers on the correct thread.
  void OnPathUpdate(
      std::map<std::string, rtc::AdapterType> adapter_type_by_name) override;

 private:
  rtc::Thread* thread_ = nullptr;
  bool started_ = false;
  std::map<std::string, rtc::AdapterType> adapter_type_by_name_
      RTC_GUARDED_BY(thread_);
  rtc::AsyncInvoker invoker_;
  RTCNetworkMonitor* network_monitor_ = nil;
};

}  // namespace webrtc

#endif  // SDK_OBJC_NATIVE_SRC_OBJC_NETWORK_MONITOR_H_
