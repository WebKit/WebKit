// Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

use cxx::{type_id, ExternType};

#[derive(Copy, Clone, Debug, PartialEq, Eq, PartialOrd, Ord)]
#[repr(transparent)]
pub struct TimeDelta {
    pub(crate) microseconds: i64,
}

unsafe impl ExternType for TimeDelta {
    type Id = type_id!("webrtc::TimeDelta");
    type Kind = cxx::kind::Trivial;
}

impl Default for TimeDelta {
    fn default() -> Self {
        Self::zero()
    }
}

impl TimeDelta {
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

    pub const fn zero() -> Self {
        Self { microseconds: 0 }
    }

    pub const fn seconds_f64(&self) -> f64 {
        self.microseconds as f64 / 1_000_000.0
    }
}

impl std::ops::Add for TimeDelta {
    type Output = Self;

    fn add(self, rhs: Self) -> Self {
        Self { microseconds: self.microseconds + rhs.microseconds }
    }
}

impl std::ops::Sub for TimeDelta {
    type Output = Self;

    fn sub(self, rhs: Self) -> Self {
        Self { microseconds: self.microseconds - rhs.microseconds }
    }
}

impl std::ops::Mul<i64> for TimeDelta {
    type Output = Self;

    fn mul(self, rhs: i64) -> Self {
        Self { microseconds: self.microseconds * rhs }
    }
}

impl std::ops::Mul<usize> for TimeDelta {
    type Output = Self;

    fn mul(self, rhs: usize) -> Self {
        Self {
            microseconds: self.microseconds * i64::try_from(rhs).expect("usize too large for i64"),
        }
    }
}

impl std::ops::Div<TimeDelta> for TimeDelta {
    type Output = i64;

    fn div(self, rhs: Self) -> i64 {
        assert!(rhs.microseconds != 0);
        self.microseconds / rhs.microseconds
    }
}

impl std::ops::Div<i64> for TimeDelta {
    type Output = Self;

    fn div(self, rhs: i64) -> Self {
        assert!(rhs != 0);
        Self { microseconds: self.microseconds / rhs }
    }
}

impl std::ops::Rem<TimeDelta> for TimeDelta {
    type Output = Self;

    fn rem(self, rhs: Self) -> Self {
        assert!(rhs.microseconds != 0);
        Self { microseconds: self.microseconds % rhs.microseconds }
    }
}

impl std::ops::Shr<i32> for TimeDelta {
    type Output = Self;

    fn shr(self, rhs: i32) -> Self {
        Self { microseconds: self.microseconds >> rhs }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_millis() {
        let td = TimeDelta::from_millis(42);
        assert_eq!(td.ms(), 42);
        assert_eq!(td.us(), 42000);
    }

    #[test]
    fn test_micros() {
        let td = TimeDelta::from_micros(42);
        assert_eq!(td.us(), 42);
    }

    #[test]
    fn test_zero() {
        let td = TimeDelta::zero();
        assert_eq!(td.us(), 0);
    }

    #[test]
    fn test_add() {
        let td1 = TimeDelta::from_millis(10);
        let td2 = TimeDelta::from_millis(20);
        let td3 = td1 + td2;
        assert_eq!(td3.ms(), 30);
    }

    #[test]
    fn test_sub() {
        let td1 = TimeDelta::from_millis(30);
        let td2 = TimeDelta::from_millis(10);
        let td3 = td1 - td2;
        assert_eq!(td3.ms(), 20);
    }
}
