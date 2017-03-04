/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_BASE_PROXYINFO_H__
#define WEBRTC_BASE_PROXYINFO_H__

namespace rtc {

// TODO(deadbeef): Remove this; it's not used any more but it's referenced in
// some places, including chromium.
enum ProxyType {
  PROXY_NONE,
};

struct ProxyInfo {
};

} // namespace rtc

#endif // WEBRTC_BASE_PROXYINFO_H__
