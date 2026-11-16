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
}

use time_delta::TimeDelta;
use timestamp::Timestamp;

#[cxx::bridge(namespace = "webrtc")]
mod ffi {
    unsafe extern "C++" {
        include!("api/units/timestamp.h");
        type Timestamp = super::timestamp::Timestamp;
        include!("api/units/time_delta.h");
        type TimeDelta = super::time_delta::TimeDelta;
    }
    extern "Rust" {
        fn add_timestamp_with_time_delta(ts: Timestamp, td: TimeDelta) -> Timestamp;
        fn half_time_delta(td: TimeDelta) -> TimeDelta;
    }
}

pub fn add_timestamp_with_time_delta(ts: Timestamp, td: TimeDelta) -> Timestamp {
    ts + td
}

pub fn half_time_delta(td: TimeDelta) -> TimeDelta {
    td / 2
}
