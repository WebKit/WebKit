// Copyright 2023 The BoringSSL Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

use crate::FfiSlice;

/// Returns true iff `a` and `b` contain the same bytes. It takes an amount of time dependent on the
/// lengths, but independent of the contents of the slices `a` and `b`. The return type is a `bool`,
/// since unlike `memcmp` in C this function cannot be used to put elements into a defined order.
pub fn constant_time_compare(a: &[u8], b: &[u8]) -> bool {
    if a.len() != b.len() {
        return false;
    }
    if a.is_empty() {
        // Avoid FFI issues with empty slices that may potentially cause UB
        return true;
    }
    // Safety:
    // - The lengths of a and b are checked above.
    let result =
        unsafe { bssl_sys::CRYPTO_memcmp(a.as_ffi_void_ptr(), b.as_ffi_void_ptr(), a.len()) };
    result == 0
}

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn test_different_length() {
        assert!(!constant_time_compare(&[0, 1, 2], &[0]))
    }

    #[test]
    fn test_same_length_different_content() {
        assert!(!constant_time_compare(&[0, 1, 2], &[1, 2, 3]))
    }

    #[test]
    fn test_same_content() {
        assert!(constant_time_compare(&[0, 1, 2], &[0, 1, 2]))
    }

    #[test]
    fn test_empty_slices() {
        assert!(constant_time_compare(&[], &[]))
    }

    #[test]
    fn test_empty_slices_different() {
        assert!(!constant_time_compare(&[], &[0, 1, 2]))
    }
}
