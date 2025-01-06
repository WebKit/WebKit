/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "call/receive_time_calculator.h"

#include <stdlib.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <vector>

#include "rtc_base/random.h"
#include "rtc_base/time_utils.h"
#include "test/gtest.h"
#include "test/scoped_key_value_config.h"

namespace webrtc {
namespace test {
namespace {

class EmulatedClock {
 public:
  explicit EmulatedClock(int seed, float drift = 0.0f)
      : random_(seed), clock_us_(random_.Rand<uint32_t>()), drift_(drift) {}
  virtual ~EmulatedClock() = default;
  int64_t GetClockUs() const { return clock_us_; }

 protected:
  int64_t UpdateClock(int64_t time_us) {
    if (!last_query_us_)
      last_query_us_ = time_us;
    int64_t skip_us = time_us - *last_query_us_;
    accumulated_drift_us_ += skip_us * drift_;
    int64_t drift_correction_us = static_cast<int64_t>(accumulated_drift_us_);
    accumulated_drift_us_ -= drift_correction_us;
    clock_us_ += skip_us + drift_correction_us;
    last_query_us_ = time_us;
    return skip_us;
  }
  Random random_;

 private:
  int64_t clock_us_;
  std::optional<int64_t> last_query_us_;
  float drift_;
  float accumulated_drift_us_ = 0;
};

class EmulatedMonotoneousClock : public EmulatedClock {
 public:
  explicit EmulatedMonotoneousClock(int seed) : EmulatedClock(seed) {}
  ~EmulatedMonotoneousClock() = default;

  int64_t Query(int64_t time_us) {
    int64_t skip_us = UpdateClock(time_us);

    // In a stall
    if (stall_recovery_time_us_ > 0) {
      if (GetClockUs() > stall_recovery_time_us_) {
        stall_recovery_time_us_ = 0;
        return GetClockUs();
      } else {
        return stall_recovery_time_us_;
      }
    }

    // Check if we enter a stall
    for (int k = 0; k < skip_us; ++k) {
      if (random_.Rand<double>() < kChanceOfStallPerUs) {
        int64_t stall_duration_us =
            static_cast<int64_t>(random_.Rand<float>() * kMaxStallDurationUs);
        stall_recovery_time_us_ = GetClockUs() + stall_duration_us;
        return stall_recovery_time_us_;
      }
    }
    return GetClockUs();
  }

  void ForceStallUs() {
    int64_t stall_duration_us =
        static_cast<int64_t>(random_.Rand<float>() * kMaxStallDurationUs);
    stall_recovery_time_us_ = GetClockUs() + stall_duration_us;
  }

  bool Stalled() const { return stall_recovery_time_us_ > 0; }

  int64_t GetRemainingStall(int64_t /* time_us */) const {
    return stall_recovery_time_us_ > 0 ? stall_recovery_time_us_ - GetClockUs()
                                       : 0;
  }

  const int64_t kMaxStallDurationUs = rtc::kNumMicrosecsPerSec;

 private:
  const float kChanceOfStallPerUs = 5e-6f;
  int64_t stall_recovery_time_us_ = 0;
};

class EmulatedNonMonotoneousClock : public EmulatedClock {
 public:
  EmulatedNonMonotoneousClock(int seed, int64_t duration_us, float drift = 0)
      : EmulatedClock(seed, drift) {
    Pregenerate(duration_us);
  }
  ~EmulatedNonMonotoneousClock() = default;

  void Pregenerate(int64_t duration_us) {
    int64_t time_since_reset_us = kMinTimeBetweenResetsUs;
    int64_t clock_offset_us = 0;
    for (int64_t time_us = 0; time_us < duration_us; time_us += kResolutionUs) {
      int64_t skip_us = UpdateClock(time_us);
      time_since_reset_us += skip_us;
      int64_t reset_us = 0;
      if (time_since_reset_us >= kMinTimeBetweenResetsUs) {
        for (int k = 0; k < skip_us; ++k) {
          if (random_.Rand<double>() < kChanceOfResetPerUs) {
            reset_us = static_cast<int64_t>(2 * random_.Rand<float>() *
                                            kMaxAbsResetUs) -
                       kMaxAbsResetUs;
            clock_offset_us += reset_us;
            time_since_reset_us = 0;
            break;
          }
        }
      }
      pregenerated_clock_.emplace_back(GetClockUs() + clock_offset_us);
      resets_us_.emplace_back(reset_us);
    }
  }

  int64_t Query(int64_t time_us) {
    size_t ixStart =
        (last_reset_query_time_us_ + (kResolutionUs >> 1)) / kResolutionUs + 1;
    size_t ixEnd = (time_us + (kResolutionUs >> 1)) / kResolutionUs;
    if (ixEnd >= pregenerated_clock_.size())
      return -1;
    last_reset_size_us_ = 0;
    for (size_t ix = ixStart; ix <= ixEnd; ++ix) {
      if (resets_us_[ix] != 0) {
        last_reset_size_us_ = resets_us_[ix];
      }
    }
    last_reset_query_time_us_ = time_us;
    return pregenerated_clock_[ixEnd];
  }

  bool WasReset() const { return last_reset_size_us_ != 0; }
  bool WasNegativeReset() const { return last_reset_size_us_ < 0; }
  int64_t GetLastResetUs() const { return last_reset_size_us_; }

 private:
  const float kChanceOfResetPerUs = 1e-6f;
  const int64_t kMaxAbsResetUs = rtc::kNumMicrosecsPerSec;
  const int64_t kMinTimeBetweenResetsUs = 3 * rtc::kNumMicrosecsPerSec;
  const int64_t kResolutionUs = rtc::kNumMicrosecsPerMillisec;
  int64_t last_reset_query_time_us_ = 0;
  int64_t last_reset_size_us_ = 0;
  std::vector<int64_t> pregenerated_clock_;
  std::vector<int64_t> resets_us_;
};

TEST(ClockRepair, NoClockDrift) {
  webrtc::test::ScopedKeyValueConfig field_trials;
  const int kSeeds = 10;
  const int kFirstSeed = 1;
  const int64_t kRuntimeUs = 10 * rtc::kNumMicrosecsPerSec;
  const float kDrift = 0.0f;
  const int64_t kMaxPacketInterarrivalUs = 50 * rtc::kNumMicrosecsPerMillisec;
  for (int seed = kFirstSeed; seed < kSeeds + kFirstSeed; ++seed) {
    EmulatedMonotoneousClock monotone_clock(seed);
    EmulatedNonMonotoneousClock non_monotone_clock(
        seed + 1, kRuntimeUs + rtc::kNumMicrosecsPerSec, kDrift);
    ReceiveTimeCalculator reception_time_tracker(field_trials);
    int64_t corrected_clock_0 = 0;
    int64_t reset_during_stall_tol_us = 0;
    bool initial_clock_stall = true;
    int64_t accumulated_upper_bound_tolerance_us = 0;
    int64_t accumulated_lower_bound_tolerance_us = 0;
    Random random(1);
    monotone_clock.ForceStallUs();
    int64_t last_time_us = 0;
    bool add_tolerance_on_next_packet = false;
    int64_t monotone_noise_us = 1000;

    for (int64_t time_us = 0; time_us < kRuntimeUs;
         time_us += static_cast<int64_t>(random.Rand<float>() *
                                         kMaxPacketInterarrivalUs)) {
      int64_t socket_time_us = non_monotone_clock.Query(time_us);
      int64_t monotone_us = monotone_clock.Query(time_us) +
                            2 * random.Rand<float>() * monotone_noise_us -
                            monotone_noise_us;
      int64_t system_time_us = non_monotone_clock.Query(
          time_us + monotone_clock.GetRemainingStall(time_us));

      int64_t corrected_clock_us = reception_time_tracker.ReconcileReceiveTimes(
          socket_time_us, system_time_us, monotone_us);
      if (time_us == 0)
        corrected_clock_0 = corrected_clock_us;

      if (add_tolerance_on_next_packet)
        accumulated_lower_bound_tolerance_us -= (time_us - last_time_us);

      // Perfect repair cannot be achiveved if non-monotone clock resets during
      // a monotone clock stall.
      add_tolerance_on_next_packet = false;
      if (monotone_clock.Stalled() && non_monotone_clock.WasReset()) {
        reset_during_stall_tol_us =
            std::max(reset_during_stall_tol_us, time_us - last_time_us);
        if (non_monotone_clock.WasNegativeReset()) {
          add_tolerance_on_next_packet = true;
        }
        if (initial_clock_stall && !non_monotone_clock.WasNegativeReset()) {
          // Positive resets during an initial clock stall cannot be repaired
          // and error will propagate through rest of trace.
          accumulated_upper_bound_tolerance_us +=
              std::abs(non_monotone_clock.GetLastResetUs());
        }
      } else {
        reset_during_stall_tol_us = 0;
        initial_clock_stall = false;
      }
      int64_t err = corrected_clock_us - corrected_clock_0 - time_us;

      // Resets during stalls may lead to small errors temporarily.
      int64_t lower_tol_us = accumulated_lower_bound_tolerance_us -
                             reset_during_stall_tol_us - monotone_noise_us -
                             2 * rtc::kNumMicrosecsPerMillisec;
      EXPECT_GE(err, lower_tol_us);
      int64_t upper_tol_us = accumulated_upper_bound_tolerance_us +
                             monotone_noise_us +
                             2 * rtc::kNumMicrosecsPerMillisec;
      EXPECT_LE(err, upper_tol_us);

      last_time_us = time_us;
    }
  }
}
}  // namespace
}  // namespace test
}  // namespace webrtc
