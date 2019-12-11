/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_CONGESTION_CONTROLLER_INCLUDE_MOCK_MOCK_CONGESTION_OBSERVER_H_
#define MODULES_CONGESTION_CONTROLLER_INCLUDE_MOCK_MOCK_CONGESTION_OBSERVER_H_

#include "modules/congestion_controller/include/network_changed_observer.h"
#include "test/gmock.h"

namespace webrtc {
namespace test {

class MockCongestionObserver : public NetworkChangedObserver {
 public:
  MOCK_METHOD4(OnNetworkChanged,
               void(uint32_t bitrate_bps,
                    uint8_t fraction_loss,
                    int64_t rtt_ms,
                    int64_t probing_interval_ms));
};

}  // namespace test
}  // namespace webrtc
#endif  // MODULES_CONGESTION_CONTROLLER_INCLUDE_MOCK_MOCK_CONGESTION_OBSERVER_H_
