/*  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/timing/jitter_estimator.h"

#include <stdint.h>

#include <memory>
#include <utility>
#include <vector>

#include "absl/types/optional.h"
#include "api/array_view.h"
#include "api/units/data_size.h"
#include "api/units/frequency.h"
#include "api/units/time_delta.h"
#include "rtc_base/numerics/histogram_percentile_counter.h"
#include "rtc_base/strings/string_builder.h"
#include "rtc_base/time_utils.h"
#include "system_wrappers/include/clock.h"
#include "test/gtest.h"
#include "test/scoped_key_value_config.h"

namespace webrtc {

class TestJitterEstimator : public ::testing::Test {
 protected:
  TestJitterEstimator() : fake_clock_(0) {}

  virtual void SetUp() {
    estimator_ = std::make_unique<JitterEstimator>(&fake_clock_, field_trials_);
  }

  SimulatedClock fake_clock_;
  test::ScopedKeyValueConfig field_trials_;
  std::unique_ptr<JitterEstimator> estimator_;
};

// Generates some simple test data in the form of a sawtooth wave.
class ValueGenerator {
 public:
  explicit ValueGenerator(int32_t amplitude)
      : amplitude_(amplitude), counter_(0) {}

  virtual ~ValueGenerator() = default;

  TimeDelta Delay() const {
    return TimeDelta::Millis((counter_ % 11) - 5) * amplitude_;
  }

  DataSize FrameSize() const {
    return DataSize::Bytes(1000 + Delay().ms() / 5);
  }

  void Advance() { ++counter_; }

 private:
  const int32_t amplitude_;
  int64_t counter_;
};

TEST_F(TestJitterEstimator, TestLowRate) {
  ValueGenerator gen(10);
  // At 5 fps, we disable jitter delay altogether.
  TimeDelta time_delta = 1 / Frequency::Hertz(5);
  for (int i = 0; i < 60; ++i) {
    estimator_->UpdateEstimate(gen.Delay(), gen.FrameSize());
    fake_clock_.AdvanceTime(time_delta);
    if (i > 2)
      EXPECT_EQ(estimator_->GetJitterEstimate(0, absl::nullopt),
                TimeDelta::Zero());
    gen.Advance();
  }
}

TEST_F(TestJitterEstimator, RttMultAddCap) {
  std::vector<std::pair<TimeDelta, rtc::HistogramPercentileCounter>>
      jitter_by_rtt_mult_cap;
  jitter_by_rtt_mult_cap.emplace_back(
      /*rtt_mult_add_cap=*/TimeDelta::Millis(10), /*long_tail_boundary=*/1000);
  jitter_by_rtt_mult_cap.emplace_back(
      /*rtt_mult_add_cap=*/TimeDelta::Millis(200), /*long_tail_boundary=*/1000);

  for (auto& [rtt_mult_add_cap, jitter] : jitter_by_rtt_mult_cap) {
    SetUp();

    ValueGenerator gen(50);
    TimeDelta time_delta = 1 / Frequency::Hertz(30);
    constexpr TimeDelta kRtt = TimeDelta::Millis(250);
    for (int i = 0; i < 100; ++i) {
      estimator_->UpdateEstimate(gen.Delay(), gen.FrameSize());
      fake_clock_.AdvanceTime(time_delta);
      estimator_->FrameNacked();
      estimator_->UpdateRtt(kRtt);
      jitter.Add(
          estimator_->GetJitterEstimate(/*rtt_mult=*/1.0, rtt_mult_add_cap)
              .ms());
      gen.Advance();
    }
  }

  // 200ms cap should result in at least 25% higher max compared to 10ms.
  EXPECT_GT(*jitter_by_rtt_mult_cap[1].second.GetPercentile(1.0),
            *jitter_by_rtt_mult_cap[0].second.GetPercentile(1.0) * 1.25);
}

}  // namespace webrtc
