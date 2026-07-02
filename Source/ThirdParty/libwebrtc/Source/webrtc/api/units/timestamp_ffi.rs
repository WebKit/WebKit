// Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

webrtc::import! {
  "//api/units:timestamp_rs" as timestamp;
}

#[cxx::bridge(namespace = "webrtc")]
pub mod ffi {
    unsafe extern "C++" {
        include!("api/units/timestamp.h");

        type Timestamp = super::timestamp::Timestamp;
    }
}
