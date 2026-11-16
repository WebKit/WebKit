/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

webrtc::import! {
  "//api/units:time_delta_rs" as time_delta;
  "//api/units:timestamp_rs" as timestamp;
}
use std::cmp;
use time_delta::TimeDelta;
use timestamp::Timestamp;

/// Computes units per second over a given interval by tracking the units over
/// each bucket of a given size and calculating the instantaneous rate assuming
/// that over each bucket the rate was constant.
pub struct RustRateTracker {
    bucket: TimeDelta,
    bucket_count: usize,
    sample_buckets: Vec<i64>,
    total_sample_count: i64,
    current_bucket: usize,
    bucket_start_time: Option<Timestamp>,
    initialization_time: Option<Timestamp>,
}

impl RustRateTracker {
    pub fn new(bucket: TimeDelta, bucket_count: usize) -> Self {
        assert!(bucket > TimeDelta::zero());
        Self {
            bucket,
            bucket_count,
            sample_buckets: vec![0; bucket_count + 1],
            total_sample_count: 0,
            current_bucket: 0,
            bucket_start_time: None,
            initialization_time: None,
        }
    }

    /// Computes the average rate over the most recent `interval_ms`,
    /// or if the first sample was added within this period, computes the rate
    /// since the first sample was added.
    pub fn compute_rate_for_interval(&self, current_time: Timestamp, interval: TimeDelta) -> f64 {
        let bucket_start_time = match self.bucket_start_time {
            Some(time) => time,
            None => return 0.0,
        };

        // Calculate which buckets to sum up given the current time. If the time
        // has passed to a new bucket then we have to skip some of the oldest buckets.
        let mut available_interval = cmp::min(interval, self.bucket * self.bucket_count);

        let buckets_to_skip;
        let duration_to_skip;

        if current_time > self.initialization_time.unwrap() + available_interval {
            let time_to_skip = current_time - bucket_start_time + self.bucket * self.bucket_count
                - available_interval;
            buckets_to_skip = (time_to_skip / self.bucket) as usize;
            duration_to_skip = time_to_skip % self.bucket;
        } else {
            buckets_to_skip = self.bucket_count - self.current_bucket;
            duration_to_skip = TimeDelta::zero();
            available_interval = current_time - self.initialization_time.unwrap();
            // Let one bucket interval pass after initialization before reporting.
            if available_interval < self.bucket {
                return 0.0;
            }
        }

        // If we're skipping all buckets that means that there have been no samples
        // within the sampling interval so report 0.
        if buckets_to_skip > self.bucket_count || available_interval == TimeDelta::zero() {
            return 0.0;
        }

        let start_bucket = self.next_bucket_index(self.current_bucket + buckets_to_skip);
        // Only count a portion of the first bucket according to how much of the
        // first bucket is within the current interval.
        // We use i128 to avoid overflow during the multiplication.
        let mut total_samples = (((self.sample_buckets[start_bucket] as i128
            * (self.bucket.us() - duration_to_skip.us()) as i128)
            + (self.bucket.us() / 2) as i128)
            / self.bucket.us() as i128) as i64;

        // All other buckets in the interval are counted in their entirety.
        let mut i = self.next_bucket_index(start_bucket);
        while i != self.next_bucket_index(self.current_bucket) {
            total_samples += self.sample_buckets[i];
            i = self.next_bucket_index(i);
        }

        // Convert to samples per second.
        total_samples as f64 / available_interval.seconds_f64()
    }

    /// Computes the average rate over the rate tracker's recording interval
    /// of `bucket` * `bucket_count`.
    pub fn rate(&self, current_time: Timestamp) -> f64 {
        self.compute_rate_for_interval(current_time, self.bucket * self.bucket_count)
    }

    /// The total number of samples added.
    pub fn total_sample_count(&self) -> i64 {
        self.total_sample_count
    }

    /// Increment count for bucket at `current_time`.
    pub fn update(&mut self, sample_count: i64, current_time: Timestamp) {
        debug_assert!(sample_count >= 0);
        self.ensure_initialized(current_time);

        let mut bucket_start_time = self.bucket_start_time.unwrap();

        // Advance the current bucket as needed for the current time, and reset
        // bucket counts as we advance.
        let mut i = 0;
        while i <= self.bucket_count && current_time >= bucket_start_time + self.bucket {
            bucket_start_time += self.bucket;
            self.current_bucket = self.next_bucket_index(self.current_bucket);
            self.sample_buckets[self.current_bucket] = 0;
            i += 1;
        }

        // Ensure that bucket_start_time is updated appropriately if
        // the entire buffer of samples has been expired.
        bucket_start_time += self.bucket * ((current_time - bucket_start_time) / self.bucket);

        self.bucket_start_time = Some(bucket_start_time);

        // Add all samples in the bucket that includes the current time.
        self.sample_buckets[self.current_bucket] += sample_count;
        self.total_sample_count += sample_count;
    }

    fn ensure_initialized(&mut self, current_time: Timestamp) {
        if self.bucket_start_time.is_none() {
            self.initialization_time = Some(current_time);
            self.bucket_start_time = Some(current_time);
            self.current_bucket = 0;
            // We only need to initialize the first bucket because we reset buckets when
            // current_bucket increments.
            self.sample_buckets[self.current_bucket] = 0;
        }
    }

    fn next_bucket_index(&self, bucket_index: usize) -> usize {
        (bucket_index + 1) % (self.bucket_count + 1)
    }
}

pub fn create_rate_tracker(bucket: TimeDelta, bucket_count: usize) -> Box<RustRateTracker> {
    Box::new(RustRateTracker::new(bucket, bucket_count))
}

#[cfg(test)]
mod tests {
    use super::*;

    const BUCKET_INTERVAL: TimeDelta = TimeDelta::from_millis(100);

    struct RateTrackerForTest {
        rate_tracker: RustRateTracker,
        time: Timestamp,
    }

    impl RateTrackerForTest {
        fn new() -> Self {
            Self {
                rate_tracker: RustRateTracker::new(BUCKET_INTERVAL, 10),
                time: Timestamp::from_millis(0),
            }
        }

        fn advance_time(&mut self, delta: TimeDelta) {
            self.time += delta;
        }

        fn compute_rate(&self) -> f64 {
            self.rate_tracker.rate(self.time)
        }

        fn compute_rate_for_interval(&self, interval: TimeDelta) -> f64 {
            self.rate_tracker.compute_rate_for_interval(self.time, interval)
        }

        fn total_sample_count(&self) -> i64 {
            self.rate_tracker.total_sample_count()
        }

        fn add_samples(&mut self, samples_count: i64) {
            self.rate_tracker.update(samples_count, self.time);
        }
    }

    #[test]
    fn test_30_fps() {
        let mut tracker = RateTrackerForTest::new();
        for i in 0..300 {
            tracker.add_samples(1);
            tracker.advance_time(TimeDelta::from_millis(33));
            if i % 3 == 0 {
                tracker.advance_time(TimeDelta::from_millis(1));
            }
        }
        assert_eq!(tracker.compute_rate_for_interval(TimeDelta::from_millis(50000)), 30.0);
    }

    #[test]
    fn test_60_fps() {
        let mut tracker = RateTrackerForTest::new();
        for i in 0..300 {
            tracker.add_samples(1);
            tracker.advance_time(TimeDelta::from_millis(16));
            if i % 3 != 0 {
                tracker.advance_time(TimeDelta::from_millis(1));
            }
        }
        assert_eq!(tracker.compute_rate_for_interval(TimeDelta::from_millis(1000)), 60.0);
    }

    #[test]
    fn test_rate_tracker_basics() {
        let mut tracker = RateTrackerForTest::new();
        assert_eq!(tracker.compute_rate_for_interval(TimeDelta::from_millis(1000)), 0.0);

        // Add a sample.
        tracker.add_samples(1234);
        // Advance the clock by less than one bucket interval (no rate returned).
        tracker.advance_time(BUCKET_INTERVAL - TimeDelta::from_millis(1));
        assert_eq!(tracker.compute_rate(), 0.0);
        // Advance the clock by 100 ms (one bucket interval).
        tracker.advance_time(TimeDelta::from_millis(1));
        assert_eq!(tracker.compute_rate_for_interval(TimeDelta::from_millis(1000)), 12340.0);
        assert_eq!(tracker.compute_rate(), 12340.0);
        assert_eq!(tracker.total_sample_count(), 1234);

        // Repeat.
        tracker.add_samples(1234);
        tracker.advance_time(TimeDelta::from_millis(100));
        assert_eq!(tracker.compute_rate_for_interval(TimeDelta::from_millis(1000)), 12340.0);
        assert_eq!(tracker.compute_rate(), 12340.0);
        assert_eq!(tracker.total_sample_count(), 1234 * 2);

        // Advance the clock by 800 ms, so we've elapsed a full second.
        tracker.advance_time(TimeDelta::from_millis(800));
        assert_eq!(tracker.compute_rate_for_interval(TimeDelta::from_millis(1000)), 1234.0 * 2.0);
        assert_eq!(tracker.compute_rate(), 1234.0 * 2.0);
        assert_eq!(tracker.total_sample_count(), 1234 * 2);

        // Poll the tracker again immediately. The reported rate should stay the same.
        assert_eq!(tracker.compute_rate_for_interval(TimeDelta::from_millis(1000)), 1234.0 * 2.0);
        assert_eq!(tracker.compute_rate(), 1234.0 * 2.0);
        assert_eq!(tracker.total_sample_count(), 1234 * 2);

        // Do nothing and advance by a second. We should drop down to zero.
        tracker.advance_time(TimeDelta::from_millis(1000));
        assert_eq!(tracker.compute_rate_for_interval(TimeDelta::from_millis(1000)), 0.0);
        assert_eq!(tracker.compute_rate(), 0.0);
        assert_eq!(tracker.total_sample_count(), 1234 * 2);

        // Send a bunch of data at a constant rate for 5.5 "seconds".
        for _ in (0..5500).step_by(100) {
            tracker.add_samples(9876);
            tracker.advance_time(TimeDelta::from_millis(100));
        }
        assert_eq!(tracker.compute_rate_for_interval(TimeDelta::from_millis(1000)), 9876.0 * 10.0);
        assert_eq!(tracker.compute_rate(), 9876.0 * 10.0);
        assert_eq!(tracker.total_sample_count(), 1234 * 2 + 9876 * 55);

        // Advance the clock by 500 ms.
        tracker.advance_time(TimeDelta::from_millis(500));
        assert_eq!(tracker.compute_rate_for_interval(TimeDelta::from_millis(1000)), 9876.0 * 5.0);
        assert_eq!(tracker.compute_rate(), 9876.0 * 5.0);
        assert_eq!(tracker.total_sample_count(), 1234 * 2 + 9876 * 55);

        // Rate over the last half second should be zero.
        assert_eq!(tracker.compute_rate_for_interval(TimeDelta::from_millis(500)), 0.0);
    }

    #[test]
    fn test_long_period_between_samples() {
        let mut tracker = RateTrackerForTest::new();
        tracker.add_samples(1);
        tracker.advance_time(TimeDelta::from_millis(1000));
        assert_eq!(tracker.compute_rate(), 1.0);

        tracker.advance_time(TimeDelta::from_millis(2000));
        assert_eq!(tracker.compute_rate(), 0.0);

        tracker.advance_time(TimeDelta::from_millis(2000));
        tracker.add_samples(1);
        assert_eq!(tracker.compute_rate(), 1.0);
    }

    #[test]
    fn test_rolloff() {
        let mut tracker = RateTrackerForTest::new();
        for _ in 0..10 {
            tracker.add_samples(1);
            tracker.advance_time(TimeDelta::from_millis(100));
        }
        assert_eq!(tracker.compute_rate(), 10.0);

        for _ in 0..10 {
            tracker.add_samples(1);
            tracker.advance_time(TimeDelta::from_millis(50));
        }
        assert_eq!(tracker.compute_rate(), 15.0);
        assert_eq!(tracker.compute_rate_for_interval(TimeDelta::from_millis(500)), 20.0);

        for _ in 0..10 {
            tracker.add_samples(1);
            tracker.advance_time(TimeDelta::from_millis(50));
        }
        assert_eq!(tracker.compute_rate(), 20.0);
    }

    #[test]
    fn test_get_unit_seconds_after_initial_value() {
        let mut tracker = RateTrackerForTest::new();
        tracker.add_samples(1234);
        tracker.advance_time(TimeDelta::from_millis(1000));
        assert_eq!(tracker.compute_rate_for_interval(TimeDelta::from_millis(1000)), 1234.0);
    }

    #[test]
    fn test_large_numbers() {
        let mut tracker = RateTrackerForTest::new();
        let large_number = 0x100000000i64;
        tracker.add_samples(large_number);
        tracker.advance_time(TimeDelta::from_millis(1000));
        tracker.add_samples(large_number);
        assert_eq!(tracker.compute_rate(), (large_number * 2) as f64);
    }
}
