/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCPeerConnectionFactoryOptions+Private.h"

#include "rtc_base/network_constants.h"

namespace {

void setNetworkBit(webrtc::PeerConnectionFactoryInterface::Options* options,
                   rtc::AdapterType type,
                   bool ignore) {
  if (ignore) {
    options->network_ignore_mask |= type;
  } else {
    options->network_ignore_mask &= ~type;
  }
}
}  // namespace

@implementation RTCPeerConnectionFactoryOptions

@synthesize disableEncryption = _disableEncryption;
@synthesize disableNetworkMonitor = _disableNetworkMonitor;
@synthesize ignoreLoopbackNetworkAdapter = _ignoreLoopbackNetworkAdapter;
@synthesize ignoreVPNNetworkAdapter = _ignoreVPNNetworkAdapter;
@synthesize ignoreCellularNetworkAdapter = _ignoreCellularNetworkAdapter;
@synthesize ignoreWiFiNetworkAdapter = _ignoreWiFiNetworkAdapter;
@synthesize ignoreEthernetNetworkAdapter = _ignoreEthernetNetworkAdapter;

- (instancetype)init {
  return [super init];
}

- (webrtc::PeerConnectionFactoryInterface::Options)nativeOptions {
  webrtc::PeerConnectionFactoryInterface::Options options;
  options.disable_encryption = self.disableEncryption;
  options.disable_network_monitor = self.disableNetworkMonitor;

  setNetworkBit(&options, rtc::ADAPTER_TYPE_LOOPBACK, self.ignoreLoopbackNetworkAdapter);
  setNetworkBit(&options, rtc::ADAPTER_TYPE_VPN, self.ignoreVPNNetworkAdapter);
  setNetworkBit(&options, rtc::ADAPTER_TYPE_CELLULAR, self.ignoreCellularNetworkAdapter);
  setNetworkBit(&options, rtc::ADAPTER_TYPE_WIFI, self.ignoreWiFiNetworkAdapter);
  setNetworkBit(&options, rtc::ADAPTER_TYPE_ETHERNET, self.ignoreEthernetNetworkAdapter);

  return options;
}

@end
