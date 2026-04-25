/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef COMMON_AUDIO_MOCKS_MOCK_SMOOTHING_FILTER_H_
#define COMMON_AUDIO_MOCKS_MOCK_SMOOTHING_FILTER_H_

#include <optional>

#include "api/units/timestamp.h"
#include "common_audio/smoothing_filter.h"
#include "test/gmock.h"

namespace webrtc {

class MockSmoothingFilter : public SmoothingFilter {
 public:
  MOCK_METHOD(void, AddSample, (float, Timestamp), (override));
  MOCK_METHOD(std::optional<float>, GetAverage, (Timestamp), (override));
  MOCK_METHOD(bool, SetTimeConstantMs, (int), (override));
};

}  // namespace webrtc

#endif  // COMMON_AUDIO_MOCKS_MOCK_SMOOTHING_FILTER_H_
