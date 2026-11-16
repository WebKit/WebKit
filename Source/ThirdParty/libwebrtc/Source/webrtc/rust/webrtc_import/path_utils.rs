// Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

//! Path manipulation utilities for the WebRTC Rust macro.
//!
//! This module contains the pure string transformation logic used by the import
//! macro. It is deliberately separated from the `proc_macro` crate file so that
//! it can be cross-compiled and validated via native target-platform unit
//! tests.

/// Re-routes a source-relative GN absolute path string based on the build root
/// prefix.
///
/// # Arguments
/// * `prefix` - The current build context prefix (e.g., `"//"` or
///   `"//third_party/webrtc"`).
/// * `path_str` - The raw GN label string literal passed into the macro.
///
/// # Behavior
/// - If `path_str` is a first-party absolute label (starts with `"//"`):
///   - If `prefix` is `"//"`, the project is standalone; the path is left
///     untouched.
///   - If `prefix` is a subfolder (e.g., `"//third_party/webrtc"`):
///     - If the remainder starts with a colon (e.g., `"//:target"`), it maps
///       directly to the parent directory, so it concatenates without adding a
///       slash (`//prefix:target`).
///     - Otherwise, it maps down a nested subdirectory folder
///       (`//prefix/path/to/target`).
/// - Third-party or relative paths are returned completely unmodified.
pub fn adjust_path(prefix: &str, path_str: &str) -> String {
    if path_str.starts_with("//") {
        if prefix == "//" {
            path_str.to_string()
        } else {
            // Remove trailing slash from prefix if present.
            let prefix = prefix.strip_suffix("/").unwrap_or(prefix);
            let remainder = &path_str[2..];
            if remainder.starts_with(':') {
                format!("{}{}", prefix, remainder)
            } else {
                format!("{}/{}", prefix, remainder)
            }
        }
    } else {
        path_str.to_string()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    /// Checks Standalone mode behavior where `WEBRTC_GN_PREFIX = "//"`.
    /// Verifies that absolute paths remain completely untouched.
    #[test]
    fn test_adjust_path_standalone() {
        assert_eq!(adjust_path("//", "//api/units:time_delta_rs"), "//api/units:time_delta_rs");
    }

    /// Checks Vendor Prefix mode behavior (e.g., nested under Chromium).
    /// Verifies that `//third_party/webrtc` is successfully prepended onto
    /// first-party paths.
    #[test]
    fn test_adjust_path_chromium() {
        assert_eq!(
            adjust_path("//third_party/webrtc", "//api/units:time_delta_rs"),
            "//third_party/webrtc/api/units:time_delta_rs"
        );
    }

    /// Checks third-party crate or target-relative imports.
    /// Verifies that paths not beginning with `//` pass through without
    /// alterations.
    #[test]
    fn test_adjust_path_relative() {
        assert_eq!(
            adjust_path("//third_party/webrtc", "api/units:time_delta_rs"),
            "api/units:time_delta_rs"
        );
    }

    /// Checks the open-source explicit root-colon syntax (e.g., `"//:target"`).
    /// Verifies that the trailing slash is omitted right before the colon,
    /// avoiding invalid GN path expansion errors in the underlying token
    /// parser.
    #[test]
    fn test_adjust_path_with_colon() {
        assert_eq!(
            adjust_path("//third_party/webrtc", "//:webrtc_import_prefix_lib"),
            "//third_party/webrtc:webrtc_import_prefix_lib"
        );
    }

    /// Removes trailing slash from a path string.
    #[test]
    fn test_remove_trailing_slash() {
        assert_eq!(
            adjust_path("//third_party/webrtc/", "//:webrtc_import_prefix_lib"),
            "//third_party/webrtc:webrtc_import_prefix_lib"
        );
    }
}
