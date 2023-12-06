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

use crate::{CSlice, ForeignType};

pub(crate) struct BigNum {
    ptr: *mut bssl_sys::BIGNUM,
}

// Safety: Implementation ensures `from_ptr(x).as_ptr() == x`
unsafe impl ForeignType for BigNum {
    type CType = bssl_sys::BIGNUM;

    unsafe fn from_ptr(ptr: *mut Self::CType) -> Self {
        Self { ptr }
    }

    fn as_ptr(&self) -> *mut Self::CType {
        self.ptr
    }
}

impl BigNum {
    pub(crate) fn new() -> Self {
        // Safety: There are no preconditions for BN_new()
        unsafe { Self::from_ptr(bssl_sys::BN_new()) }
    }
}

impl From<&[u8]> for BigNum {
    fn from(value: &[u8]) -> Self {
        let value_ffi = CSlice(value);
        // Safety:
        // - `value` is a CSlice from safe Rust.
        // - The `ret` argument can be null to request allocating a new result.
        let ptr = unsafe {
            bssl_sys::BN_bin2bn(value_ffi.as_ptr(), value_ffi.len(), core::ptr::null_mut())
        };
        assert!(!ptr.is_null());
        Self { ptr }
    }
}

impl Drop for BigNum {
    fn drop(&mut self) {
        // Safety: `self.ptr` is owned by `self`.
        unsafe { bssl_sys::BN_free(self.ptr) }
    }
}
