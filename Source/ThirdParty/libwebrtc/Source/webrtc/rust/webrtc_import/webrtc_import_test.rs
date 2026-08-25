// Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#[test]
fn test_import_standalone() {
    webrtc::import! {
        "//rust/webrtc_import:webrtc_import_test_lib";
    }
    assert_eq!(webrtc_import_test_lib::hello(), 42);
}
