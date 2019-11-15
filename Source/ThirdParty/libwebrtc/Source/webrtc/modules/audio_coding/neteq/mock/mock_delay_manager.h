/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_CODING_NETEQ_MOCK_MOCK_DELAY_MANAGER_H_
#define MODULES_AUDIO_CODING_NETEQ_MOCK_MOCK_DELAY_MANAGER_H_

#include <algorithm>

#include "modules/audio_coding/neteq/delay_manager.h"
#include "modules/audio_coding/neteq/histogram.h"
#include "modules/audio_coding/neteq/statistics_calculator.h"
#include "test/gmock.h"

namespace webrtc {

class MockDelayManager : public DelayManager {
 public:
  MockDelayManager(size_t max_packets_in_buffer,
                   int base_min_target_delay_ms,
                   int histogram_quantile,
                   HistogramMode histogram_mode,
                   bool enable_rtx_handling,
                   DelayPeakDetector* peak_detector,
                   const TickTimer* tick_timer,
                   StatisticsCalculator* stats,
                   std::unique_ptr<Histogram> histogram)
      : DelayManager(max_packets_in_buffer,
                     base_min_target_delay_ms,
                     histogram_quantile,
                     histogram_mode,
                     enable_rtx_handling,
                     peak_detector,
                     tick_timer,
                     stats,
                     std::move(histogram)) {}
  virtual ~MockDelayManager() { Die(); }
  MOCK_METHOD0(Die, void());
  MOCK_METHOD3(Update,
               int(uint16_t sequence_number,
                   uint32_t timestamp,
                   int sample_rate_hz));
  MOCK_METHOD2(CalculateTargetLevel, int(int iat_packets, bool reordered));
  MOCK_METHOD1(SetPacketAudioLength, int(int length_ms));
  MOCK_METHOD0(Reset, void());
  MOCK_CONST_METHOD0(PeakFound, bool());
  MOCK_METHOD0(ResetPacketIatCount, void());
  MOCK_CONST_METHOD2(BufferLimits, void(int* lower_limit, int* higher_limit));
  MOCK_METHOD1(SetBaseMinimumDelay, bool(int delay_ms));
  MOCK_CONST_METHOD0(GetBaseMinimumDelay, int());
  MOCK_CONST_METHOD0(TargetLevel, int());
  MOCK_METHOD0(RegisterEmptyPacket, void());
  MOCK_CONST_METHOD0(base_target_level, int());
  MOCK_CONST_METHOD0(last_pack_cng_or_dtmf, int());
  MOCK_METHOD1(set_last_pack_cng_or_dtmf, void(int value));
};

}  // namespace webrtc
#endif  // MODULES_AUDIO_CODING_NETEQ_MOCK_MOCK_DELAY_MANAGER_H_
