/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "call/callfactory.h"

#include <memory>

namespace webrtc {

Call* CallFactory::CreateCall(const Call::Config& config) {
  return Call::Create(config);
}

std::unique_ptr<CallFactoryInterface> CreateCallFactory() {
  return std::unique_ptr<CallFactoryInterface>(new CallFactory());
}

}  // namespace webrtc
