/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_CODING_NETEQ_MOCK_MOCK_STATISTICS_CALCULATOR_H_
#define MODULES_AUDIO_CODING_NETEQ_MOCK_MOCK_STATISTICS_CALCULATOR_H_

#include "modules/audio_coding/neteq/statistics_calculator.h"

#include "test/gmock.h"

namespace webrtc {

class MockStatisticsCalculator : public StatisticsCalculator {
 public:
  MOCK_METHOD1(PacketsDiscarded, void(size_t num_packets));
  MOCK_METHOD1(SecondaryPacketsDiscarded, void(size_t num_packets));
};

}  // namespace webrtc
#endif  // MODULES_AUDIO_CODING_NETEQ_MOCK_MOCK_STATISTICS_CALCULATOR_H_
