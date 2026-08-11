// Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

webrtc::import! {
  "//api/units:time_delta_rs" as time_delta;
  "//api/units:timestamp_rs" as timestamp;
  "//rtc_base:rate_tracker_rust" as rate_tracker;
}

use time_delta::TimeDelta;
use timestamp::Timestamp;

pub struct RustRateTracker {
    inner: rate_tracker::RustRateTracker,
}

#[cxx::bridge(namespace = "webrtc")]
mod ffi {
    unsafe extern "C++" {
        include!("api/units/time_delta.h");
        include!("api/units/timestamp.h");

        type TimeDelta = super::TimeDelta;
        type Timestamp = super::Timestamp;
    }

    extern "Rust" {
        type RustRateTracker;

        fn create_rate_tracker(bucket: TimeDelta, bucket_count: usize) -> Box<RustRateTracker>;
        fn compute_rate_for_interval(
            self: &RustRateTracker,
            current_time: Timestamp,
            interval: TimeDelta,
        ) -> f64;
        fn rate(self: &RustRateTracker, current_time: Timestamp) -> f64;
        fn total_sample_count(self: &RustRateTracker) -> i64;
        fn update(self: &mut RustRateTracker, sample_count: i64, current_time: Timestamp);
    }
}

pub fn create_rate_tracker(bucket: TimeDelta, bucket_count: usize) -> Box<RustRateTracker> {
    Box::new(RustRateTracker { inner: rate_tracker::RustRateTracker::new(bucket, bucket_count) })
}

impl RustRateTracker {
    pub fn compute_rate_for_interval(&self, current_time: Timestamp, interval: TimeDelta) -> f64 {
        self.inner.compute_rate_for_interval(current_time, interval)
    }

    pub fn rate(&self, current_time: Timestamp) -> f64 {
        self.inner.rate(current_time)
    }

    pub fn total_sample_count(&self) -> i64 {
        self.inner.total_sample_count()
    }

    pub fn update(&mut self, sample_count: i64, current_time: Timestamp) {
        self.inner.update(sample_count, current_time);
    }
}
