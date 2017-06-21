/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_CALL_CALLFACTORYINTERFACE_H_
#define WEBRTC_CALL_CALLFACTORYINTERFACE_H_

#include <memory>

#include "webrtc/call/call.h"

namespace webrtc {

// This interface exists to allow webrtc to be optionally built without media
// support (i.e., if only being used for data channels). PeerConnectionFactory
// is constructed with a CallFactoryInterface, which may or may not be null.
class CallFactoryInterface {
 public:
  virtual ~CallFactoryInterface() {}

  virtual Call* CreateCall(const Call::Config& config) = 0;
};

std::unique_ptr<CallFactoryInterface> CreateCallFactory();

}  // namespace webrtc

#endif  // WEBRTC_CALL_CALLFACTORYINTERFACE_H_
