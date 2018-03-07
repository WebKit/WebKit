/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "p2p/base/icetransportinternal.h"

namespace cricket {

IceTransportInternal::IceTransportInternal() = default;

IceTransportInternal::~IceTransportInternal() = default;

void IceTransportInternal::SetIceCredentials(const std::string& ice_ufrag,
                                             const std::string& ice_pwd) {
  SetIceParameters(IceParameters(ice_ufrag, ice_pwd, false));
}

void IceTransportInternal::SetRemoteIceCredentials(const std::string& ice_ufrag,
                                                   const std::string& ice_pwd) {
  SetRemoteIceParameters(IceParameters(ice_ufrag, ice_pwd, false));
}

}  // namespace cricket
