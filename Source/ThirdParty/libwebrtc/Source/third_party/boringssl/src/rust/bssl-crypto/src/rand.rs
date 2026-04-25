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

//! Getting random bytes.

use crate::{with_output_array, FfiMutSlice};

/// Fills `buf` with random bytes.
pub fn rand_bytes(buf: &mut [u8]) {
    // Safety: `RAND_bytes` writes exactly `buf.len()` bytes.
    let ret = unsafe { bssl_sys::RAND_bytes(buf.as_mut_ffi_ptr(), buf.len()) };

    // BoringSSL's `RAND_bytes` always succeeds returning 1, or crashes the
    // address space if the PRNG can not provide random data.
    debug_assert!(ret == 1);
}

/// Returns an array of random bytes.
pub fn rand_array<const N: usize>() -> [u8; N] {
    unsafe {
        with_output_array(|out, out_len| {
            // Safety: `RAND_bytes` writes exactly `out_len` bytes, as required.
            let ret = bssl_sys::RAND_bytes(out, out_len);
            // BoringSSL RAND_bytes always succeeds returning 1, or crashes the
            // address space if the PRNG can not provide random data.
            debug_assert!(ret == 1);
        })
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn fill() {
        let mut buf = [0; 32];
        rand_bytes(&mut buf);
    }

    #[test]
    fn fill_empty() {
        let mut buf = [];
        rand_bytes(&mut buf);
    }

    #[test]
    fn array() {
        let _rand: [u8; 32] = rand_array();
    }

    #[test]
    fn empty_array() {
        let _rand: [u8; 0] = rand_array();
    }
}
