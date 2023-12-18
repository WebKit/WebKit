/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_ASYNC_RESOLVER_FACTORY_H_
#define API_ASYNC_RESOLVER_FACTORY_H_

#include "rtc_base/async_resolver_interface.h"

namespace webrtc {

// An abstract factory for creating AsyncResolverInterfaces. This allows
// client applications to provide WebRTC with their own mechanism for
// performing DNS resolution.
// TODO(bugs.webrtc.org/12598): Deprecate and remove.
class [[deprecated("Use AsyncDnsResolverFactory")]] AsyncResolverFactory {
 public:
  AsyncResolverFactory() = default;
  virtual ~AsyncResolverFactory() = default;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
  // The caller should call Destroy on the returned object to delete it.
  virtual rtc::AsyncResolverInterface* Create() = 0;
#pragma clang diagnostic pop
};

}  // namespace webrtc

#endif  // API_ASYNC_RESOLVER_FACTORY_H_
