/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef CALL_CALLFACTORY_H_
#define CALL_CALLFACTORY_H_

#include "call/callfactoryinterface.h"

namespace webrtc {

class CallFactory : public CallFactoryInterface {
  ~CallFactory() override {}

  Call* CreateCall(const Call::Config& config) override;
};

}  // namespace webrtc

#endif  // CALL_CALLFACTORY_H_
