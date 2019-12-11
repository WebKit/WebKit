/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_TRANSPORT_TEST_MOCK_NETWORK_CONTROL_H_
#define API_TRANSPORT_TEST_MOCK_NETWORK_CONTROL_H_

#include "api/transport/include/network_control.h"
#include "test/gmock.h"

namespace webrtc {
namespace test {
class MockTargetTransferRateObserver : public TargetTransferRateObserver {
 public:
  MOCK_METHOD1(OnTargetTransferRate, void(TargetTransferRate));
};
}  // namespace test
}  // namespace webrtc

#endif  // API_TRANSPORT_TEST_MOCK_NETWORK_CONTROL_H_
