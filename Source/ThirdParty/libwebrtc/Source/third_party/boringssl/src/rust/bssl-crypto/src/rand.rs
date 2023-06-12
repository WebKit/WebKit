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

use crate::CSliceMut;

/// Fills buf with random bytes. In the event that sufficient random data can not be obtained,
/// BoringSSL will abort, so the assert will never be hit.
pub fn rand_bytes(buf: &mut [u8]) {
    let mut ffi_buf = CSliceMut::from(buf);
    let result = unsafe { bssl_sys::RAND_bytes(ffi_buf.as_mut_ptr(), ffi_buf.len()) };
    assert_eq!(result, 1, "BoringSSL RAND_bytes API failed unexpectedly");
}

#[cfg(test)]
mod tests {
    use super::rand_bytes;

    #[test]
    fn test_rand_bytes() {
        let mut buf = [0; 32];
        rand_bytes(&mut buf);
    }

    #[test]
    fn test_rand_bytes_empty() {
        let mut buf = [];
        rand_bytes(&mut buf);
    }
}
