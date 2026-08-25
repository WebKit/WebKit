// Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#[test]
fn test_import_prefix() {
    webrtc::import! {
        // The macro will see WEBRTC_GN_PREFIX=//rust/webrtc_import/test_subdir
        // and turn this into "//rust/webrtc_import/test_subdir:webrtc_import_prefix_lib" via the fixed colon logic
        "//:webrtc_import_prefix_lib";
    }
    assert_eq!(webrtc_import_prefix_lib::hello_from_subdir(), "hello from subdir");
}
