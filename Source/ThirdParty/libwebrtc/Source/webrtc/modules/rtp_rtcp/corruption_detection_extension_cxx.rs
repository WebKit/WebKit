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
  "//modules/rtp_rtcp:corruption_detection_extension";
}

// CXX can't create bindings for a type from a different crate.
// This type works around such limitation by wrapping external type.
#[derive(Default)]
struct RustCorruptionDetectionExtension {
    inner: corruption_detection_extension::CorruptionDetectionExtension,
}

#[cxx::bridge(namespace = "webrtc")]
mod ffi {
    extern "Rust" {
        type RustCorruptionDetectionExtension;

        // CXX can't return arbitrary types by value.
        // Wrapping such type into a Box works around such limitation.
        fn create_corruption_detection_message() -> Box<RustCorruptionDetectionExtension>;
        fn parse(&mut self, payload: &[u8]) -> bool;

        fn sequence_index(&self) -> i8;
        fn interpret_sequence_index_as_most_significant_bits(&self) -> bool;
        fn std_dev(&self) -> f64;
        fn luma_error_threshold(&self) -> i8;
        fn chroma_error_threshold(&self) -> i8;
        fn sample_values(&self) -> &[u8];
    }
}

fn create_corruption_detection_message() -> Box<RustCorruptionDetectionExtension> {
    Default::default()
}

impl RustCorruptionDetectionExtension {
    fn parse(&mut self, payload: &[u8]) -> bool {
        self.inner.parse(payload)
    }

    fn sequence_index(&self) -> i8 {
        self.inner.sequence_index
    }

    fn interpret_sequence_index_as_most_significant_bits(&self) -> bool {
        self.inner.interpret_sequence_index_as_most_significant_bits
    }

    fn std_dev(&self) -> f64 {
        self.inner.std_dev
    }

    fn luma_error_threshold(&self) -> i8 {
        self.inner.luma_error_threshold
    }

    fn chroma_error_threshold(&self) -> i8 {
        self.inner.chroma_error_threshold
    }

    fn sample_values(&self) -> &[u8] {
        self.inner.sample_values()
    }
}
