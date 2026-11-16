/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <cstddef>
#include <cstdint>
#include <vector>

#include "absl/random/random.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "rtc_base/rate_tracker.h"
#include "rtc_base/rate_tracker_ffi.rs.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

struct RateTrackerUpdate {
  int64_t samples;
  TimeDelta delta;
  TimeDelta interval;
};

void RustAndCppMatch(TimeDelta bucket,
                     size_t bucket_count,
                     Timestamp start_time,
                     const std::vector<RateTrackerUpdate>& updates) {
  RateTracker cpp_tracker(bucket.ms(), bucket_count);
  rust::Box<RustRateTracker> rust_tracker =
      create_rate_tracker(bucket, bucket_count);

  Timestamp current_time = start_time;

  for (const auto& update : updates) {
    current_time += update.delta;
    cpp_tracker.Update(update.samples, current_time);
    rust_tracker->update(update.samples, current_time);

    EXPECT_EQ(cpp_tracker.TotalSampleCount(),
              rust_tracker->total_sample_count());
    EXPECT_NEAR(cpp_tracker.Rate(current_time),
                rust_tracker->rate(current_time), 1e-7);

    EXPECT_NEAR(
        cpp_tracker.ComputeRateForInterval(current_time, update.interval),
        rust_tracker->compute_rate_for_interval(current_time, update.interval),
        1e-6);
  }
}

TEST(RateTrackerComparisonTest, RustVsCppFuzzTest) {
  // This should use FuzzTest instead, but for now we use rng.
  absl::BitGen gen;
  for (int i = 0; i < 100; ++i) {
    TimeDelta bucket = TimeDelta::Millis(absl::Uniform<int64_t>(gen, 1, 1000));
    size_t bucket_count = absl::Uniform<size_t>(gen, 1, 1000);
    Timestamp start_time =
        Timestamp::Millis(absl::Uniform<int64_t>(gen, 0, 1000000));

    size_t num_updates = absl::Uniform<size_t>(gen, 0, 100);
    std::vector<RateTrackerUpdate> updates;
    for (size_t j = 0; j < num_updates; ++j) {
      RateTrackerUpdate update;
      update.samples = absl::Uniform<int64_t>(gen, 0, 1000000);
      update.delta = TimeDelta::Millis(absl::Uniform<int64_t>(gen, 1, 1000));
      update.interval =
          TimeDelta::Millis(absl::Uniform<int64_t>(gen, 1, 10000));
      updates.push_back(update);
    }

    RustAndCppMatch(bucket, bucket_count, start_time, updates);
  }
}

TEST(RateTrackerComparisonTest, LargeSamples) {
  const TimeDelta kBucket = TimeDelta::Millis(100);
  const size_t kBucketCount = 10;

  RateTracker cpp_tracker(kBucket.ms(), kBucketCount);
  rust::Box<RustRateTracker> rust_tracker =
      create_rate_tracker(kBucket, kBucketCount);

  // Use a large sample count that might cause overflow if not handled.
  // microseconds * samples: 100,000 * 1,000,000,000,000 = 10^17 (fits in i64,
  // max i64 is ~9.2 * 10^18) But let's go larger: 10^15 samples. 100,000 *
  // 10^15 = 10^20 (EXCEEDS i64!)
  const int64_t kLargeSamples = 1000000000000000LL;  // 10^15
  Timestamp current_time = Timestamp::Millis(1000);

  cpp_tracker.Update(kLargeSamples, current_time);
  rust_tracker->update(kLargeSamples, current_time);

  // Advance by half a bucket.
  current_time += kBucket / 2;

  // Rate should be (samples / 2) / (bucket / 2) * 1000 = samples * 1000 /
  // bucket available_interval is current_time - initialization_time = 50ms.
  // Wait, C++'s available_interval_milliseconds would be 50ms.
  // total_samples calculation:
  // start_bucket = current_bucket (0)
  // samples_to_count = samples * (100 - 50) + 50 / 100 = samples * 50 / 100 =
  // samples / 2. rate = (samples / 2 * 1000) / 50 = samples * 10.

  EXPECT_NEAR(cpp_tracker.Rate(current_time), rust_tracker->rate(current_time),
              1e-6);
}

}  // namespace
}  // namespace webrtc
