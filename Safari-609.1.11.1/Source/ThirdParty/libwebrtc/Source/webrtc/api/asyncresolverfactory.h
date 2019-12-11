/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_ASYNCRESOLVERFACTORY_H_
#define API_ASYNCRESOLVERFACTORY_H_

#include "rtc_base/asyncresolverinterface.h"

namespace webrtc {

// An abstract factory for creating AsyncResolverInterfaces. This allows
// client applications to provide WebRTC with their own mechanism for
// performing DNS resolution.
class AsyncResolverFactory {
 public:
  AsyncResolverFactory() = default;
  virtual ~AsyncResolverFactory() = default;

  // The caller should call Destroy on the returned object to delete it.
  virtual rtc::AsyncResolverInterface* Create() = 0;
};

}  // namespace webrtc

#endif  // API_ASYNCRESOLVERFACTORY_H_
