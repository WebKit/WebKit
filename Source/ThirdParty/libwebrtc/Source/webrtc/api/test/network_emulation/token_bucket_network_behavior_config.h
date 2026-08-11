/*
 *  Copyright 2025 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_TEST_NETWORK_EMULATION_TOKEN_BUCKET_NETWORK_BEHAVIOR_CONFIG_H_
#define API_TEST_NETWORK_EMULATION_TOKEN_BUCKET_NETWORK_BEHAVIOR_CONFIG_H_

#include "api/units/data_rate.h"
#include "api/units/data_size.h"

namespace webrtc {

// Note that the default config results in dropping all packets.
struct TokenBucketNetworkBehaviorConfig {
  // Size of the token bucket. This is the amount of data that can be sent in
  // a burst and needs to be at least the size of a packet to let any packets
  // through.
  DataSize burst = DataSize::Zero();
  // Refill rate of the bucket. This is the average send rate of the node.
  DataRate rate = DataRate::Zero();
};

}  // namespace webrtc

#endif  // API_TEST_NETWORK_EMULATION_TOKEN_BUCKET_NETWORK_BEHAVIOR_CONFIG_H_
