/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_INCLUDE_MOCK_MOCK_AIMD_RATE_CONTROL_H_
#define WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_INCLUDE_MOCK_MOCK_AIMD_RATE_CONTROL_H_

#include "webrtc/modules/remote_bitrate_estimator/aimd_rate_control.h"
#include "webrtc/test/gmock.h"

namespace webrtc {

class MockAimdRateControl : public AimdRateControl {
 public:
  MOCK_CONST_METHOD0(GetNearMaxIncreaseRateBps, int());
  MOCK_CONST_METHOD0(GetLastBitrateDecreaseBps, rtc::Optional<int>());
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_INCLUDE_MOCK_MOCK_AIMD_RATE_CONTROL_H_
