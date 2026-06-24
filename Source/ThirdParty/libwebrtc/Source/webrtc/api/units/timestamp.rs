// Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

webrtc::import! {
  "//api/units:time_delta_rs" as time_delta;
}
use cxx::{type_id, ExternType};
use time_delta::TimeDelta;

#[derive(Copy, Clone, Debug, PartialEq, Eq, PartialOrd, Ord)]
#[repr(transparent)]
pub struct Timestamp {
    pub(crate) microseconds: i64,
}

unsafe impl ExternType for Timestamp {
    type Id = type_id!("webrtc::Timestamp");
    type Kind = cxx::kind::Trivial;
}

impl Timestamp {
    pub const fn ms(&self) -> i64 {
        self.microseconds / 1000
    }

    pub const fn us(&self) -> i64 {
        self.microseconds
    }

    pub const fn from_millis(value: i64) -> Self {
        Self { microseconds: value * 1000 }
    }

    pub const fn from_micros(value: i64) -> Self {
        Self { microseconds: value }
    }
}

impl std::ops::Add<TimeDelta> for Timestamp {
    type Output = Self;

    fn add(self, rhs: TimeDelta) -> Self {
        Self { microseconds: self.microseconds + rhs.us() }
    }
}

impl std::ops::AddAssign<TimeDelta> for Timestamp {
    fn add_assign(&mut self, rhs: TimeDelta) {
        self.microseconds += rhs.us();
    }
}

impl std::ops::Sub<TimeDelta> for Timestamp {
    type Output = Self;

    fn sub(self, rhs: TimeDelta) -> Self {
        Self { microseconds: self.microseconds - rhs.us() }
    }
}

impl std::ops::Sub for Timestamp {
    type Output = TimeDelta;

    fn sub(self, rhs: Self) -> TimeDelta {
        TimeDelta::from_micros(self.microseconds - rhs.microseconds)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_millis() {
        let ts = Timestamp::from_millis(42);
        assert_eq!(ts.ms(), 42);
        assert_eq!(ts.us(), 42000);
    }

    #[test]
    fn test_micros() {
        let ts = Timestamp::from_micros(42);
        assert_eq!(ts.us(), 42);
    }

    #[test]
    fn test_add_time_delta() {
        let ts = Timestamp::from_millis(10);
        let td = TimeDelta::from_millis(20);
        let ts2 = ts + td;
        assert_eq!(ts2.ms(), 30);
    }

    #[test]
    fn test_sub_time_delta() {
        let ts = Timestamp::from_millis(30);
        let td = TimeDelta::from_millis(10);
        let ts2 = ts - td;
        assert_eq!(ts2.ms(), 20);
    }

    #[test]
    fn test_sub_timestamp() {
        let ts1 = Timestamp::from_millis(30);
        let ts2 = Timestamp::from_millis(10);
        let td = ts1 - ts2;
        assert_eq!(td.ms(), 20);
    }
}
