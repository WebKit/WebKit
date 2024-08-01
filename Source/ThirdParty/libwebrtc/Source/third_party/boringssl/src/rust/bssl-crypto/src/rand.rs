/* Copyright (c) 2023, Google Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

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
