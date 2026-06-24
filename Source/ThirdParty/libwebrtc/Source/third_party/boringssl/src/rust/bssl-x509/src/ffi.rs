// Copyright 2026 The BoringSSL Authors
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

use core::{
    marker::PhantomData,
    ptr::{NonNull, null},
    slice::from_raw_parts,
};

use bssl_crypto::FfiSlice;

use crate::errors::{PkiError, X509Error};

#[must_use]
pub(crate) fn abort_on_panic<T>(work: impl FnOnce() -> T) -> T {
    let assert_unwind_safe = core::panic::AssertUnwindSafe(work);
    let call = move || {
        let core::panic::AssertUnwindSafe(work) = { assert_unwind_safe };
        work()
    };
    #[cfg(feature = "std")]
    let res = match std::panic::catch_unwind(call) {
        Ok(res) => res,
        Err(e) => {
            eprintln!("panic about to cross FFI: {:?}", e);
            std::process::abort()
        }
    };
    #[cfg(not(feature = "std"))]
    let res = call();
    res
}

pub(crate) fn slice_into_ffi_raw_parts<T>(slice: &[T]) -> (*const T, usize) {
    if slice.is_empty() {
        (null(), 0)
    } else {
        (slice.as_ptr(), slice.len())
    }
}

/// Sanitize the data pointer and length and reconstitute the slice.
///
/// This method returns an empty slice if the length is 0 or the pointer is NULL.
/// # Safety
/// Caller must ensure that `'a` outlives `input`.
#[inline]
pub(crate) unsafe fn sanitize_slice<'a, T>(input: *const T, len: usize) -> Option<&'a [T]> {
    if len == 0 || input.is_null() {
        return Some(&[]);
    }
    if !input.is_aligned() || len.checked_mul(size_of::<T>())? > isize::MAX as usize {
        return None;
    }
    unsafe {
        // Safety: the pointer and the size has been sanitised.
        Some(from_raw_parts(input, len))
    }
}

/// BIO wrapper only for internal use.
pub(crate) struct Bio<'a>(NonNull<bssl_sys::BIO>, PhantomData<&'a ()>);

impl<'a> Bio<'a> {
    /// # Safety
    /// Caller must ensure that the lifetime of this BIO outlives the backing object.
    /// It is strongly recommended to call the builder functions.
    pub(crate) unsafe fn new(bio: NonNull<bssl_sys::BIO>) -> Self {
        Bio(bio, PhantomData)
    }

    pub fn from_bytes(buf: &'a [u8]) -> Result<Self, PkiError> {
        let len = if let Ok(len) = buf.len().try_into() {
            len
        } else {
            return Err(PkiError::X509(X509Error::PemTooLong));
        };
        let mem_buf = unsafe {
            // Safety: buf is still valid
            bssl_sys::BIO_new_mem_buf(buf.as_ffi_void_ptr(), len)
        };
        let Some(mem_buf) = NonNull::new(mem_buf) else {
            panic!("bio: allocation failure");
        };
        Ok(unsafe {
            // Safety: our returned object is outlived by the input buffer.
            Self::new(mem_buf)
        })
    }

    pub fn ptr(&mut self) -> *mut bssl_sys::BIO {
        self.0.as_ptr()
    }
}

impl<'a> Drop for Bio<'a> {
    fn drop(&mut self) {
        unsafe {
            // Safety: the BIO handle should still be valid
            bssl_sys::BIO_free(self.0.as_ptr());
        }
    }
}
