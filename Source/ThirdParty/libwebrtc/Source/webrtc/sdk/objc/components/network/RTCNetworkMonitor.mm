/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCNetworkMonitor+Private.h"

#import <Network/Network.h>

#import "base/RTCLogging.h"
#import "helpers/RTCDispatcher+Private.h"

namespace {

rtc::AdapterType AdapterTypeFromInterfaceType(nw_interface_type_t interfaceType) {
  rtc::AdapterType adapterType = rtc::ADAPTER_TYPE_UNKNOWN;
  switch (interfaceType) {
    case nw_interface_type_other:
      adapterType = rtc::ADAPTER_TYPE_UNKNOWN;
      break;
    case nw_interface_type_wifi:
      adapterType = rtc::ADAPTER_TYPE_WIFI;
      break;
    case nw_interface_type_cellular:
      adapterType = rtc::ADAPTER_TYPE_CELLULAR;
      break;
    case nw_interface_type_wired:
      adapterType = rtc::ADAPTER_TYPE_ETHERNET;
      break;
    case nw_interface_type_loopback:
      adapterType = rtc::ADAPTER_TYPE_LOOPBACK;
      break;
    default:
      adapterType = rtc::ADAPTER_TYPE_UNKNOWN;
      break;
  }
  return adapterType;
}

}  // namespace

@implementation RTCNetworkMonitor {
  webrtc::NetworkMonitorObserver *_observer;
  nw_path_monitor_t _pathMonitor;
  dispatch_queue_t _monitorQueue;
}

- (instancetype)initWithObserver:(webrtc::NetworkMonitorObserver *)observer {
  RTC_DCHECK(observer);
  if (self = [super init]) {
    _observer = observer;
    if (@available(iOS 12, *)) {
      _pathMonitor = nw_path_monitor_create();
      if (_pathMonitor == nil) {
        RTCLog(@"nw_path_monitor_create failed.");
        return nil;
      }
      RTCLog(@"NW path monitor created.");
      __weak RTCNetworkMonitor *weakSelf = self;
      nw_path_monitor_set_update_handler(_pathMonitor, ^(nw_path_t path) {
        if (weakSelf == nil) {
          return;
        }
        RTCNetworkMonitor *strongSelf = weakSelf;
        RTCLog(@"NW path monitor: updated.");
        nw_path_status_t status = nw_path_get_status(path);
        if (status == nw_path_status_invalid) {
          RTCLog(@"NW path monitor status: invalid.");
        } else if (status == nw_path_status_unsatisfied) {
          RTCLog(@"NW path monitor status: unsatisfied.");
        } else if (status == nw_path_status_satisfied) {
          RTCLog(@"NW path monitor status: satisfied.");
        } else if (status == nw_path_status_satisfiable) {
          RTCLog(@"NW path monitor status: satisfiable.");
        }
        std::map<std::string, rtc::AdapterType> *map =
            new std::map<std::string, rtc::AdapterType>();
        nw_path_enumerate_interfaces(
            path, (nw_path_enumerate_interfaces_block_t) ^ (nw_interface_t interface) {
              const char *name = nw_interface_get_name(interface);
              nw_interface_type_t interfaceType = nw_interface_get_type(interface);
              RTCLog(@"NW path monitor available interface: %s", name);
              rtc::AdapterType adapterType = AdapterTypeFromInterfaceType(interfaceType);
              map->insert(std::pair<std::string, rtc::AdapterType>(name, adapterType));
            });
        strongSelf->_observer->OnPathUpdate(std::move(*map));
        delete map;
      });
      nw_path_monitor_set_queue(
          _pathMonitor,
          [RTC_OBJC_TYPE(RTCDispatcher) dispatchQueueForType:RTCDispatcherTypeNetworkMonitor]);
      nw_path_monitor_start(_pathMonitor);
    }
  }
  return self;
}

- (void)dealloc {
  if (@available(iOS 12, *)) {
    nw_path_monitor_cancel(_pathMonitor);
  }
}

@end
