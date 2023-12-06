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

//! `Pkey` and `PkeyCtx` classes for holding asymmetric keys. This module is intended for internal
//! use within this crate only, to create higher-level abstractions suitable to be exposed
//! externally.

use crate::{ec::EcKey, CSliceMut, ForeignType};
use alloc::borrow::ToOwned;
use alloc::string::String;

pub(crate) struct Pkey {
    ptr: *mut bssl_sys::EVP_PKEY,
}

// Safety: Implementation ensures `from_ptr(x).as_ptr == x`
unsafe impl ForeignType for Pkey {
    type CType = bssl_sys::EVP_PKEY;

    unsafe fn from_ptr(ptr: *mut Self::CType) -> Self {
        Self { ptr }
    }

    fn as_ptr(&self) -> *mut Self::CType {
        self.ptr
    }
}

impl From<&EcKey> for Pkey {
    fn from(eckey: &EcKey) -> Self {
        // Safety: EVP_PKEY_new does not have any preconditions
        let pkey = unsafe { bssl_sys::EVP_PKEY_new() };
        assert!(!pkey.is_null());
        // Safety:
        // - pkey is just allocated and is null-checked
        // - EcKey ensures eckey.ptr is valid during its lifetime
        // - EVP_PKEY_set1_EC_KEY doesn't take ownership
        let result = unsafe { bssl_sys::EVP_PKEY_set1_EC_KEY(pkey, eckey.as_ptr()) };
        assert_eq!(result, 1, "bssl_sys::EVP_PKEY_set1_EC_KEY failed");
        Self { ptr: pkey }
    }
}

impl Drop for Pkey {
    fn drop(&mut self) {
        // Safety: `self.ptr` is owned by this struct
        unsafe { bssl_sys::EVP_PKEY_free(self.ptr) }
    }
}

pub(crate) struct PkeyCtx {
    ptr: *mut bssl_sys::EVP_PKEY_CTX,
}

impl PkeyCtx {
    pub fn new(pkey: &Pkey) -> Self {
        // Safety:
        // - `Pkey` ensures `pkey.ptr` is valid, and EVP_PKEY_CTX_new does not take ownership.
        let pkeyctx = unsafe { bssl_sys::EVP_PKEY_CTX_new(pkey.ptr, core::ptr::null_mut()) };
        assert!(!pkeyctx.is_null());
        Self { ptr: pkeyctx }
    }

    #[allow(clippy::panic)]
    pub(crate) fn diffie_hellman(
        self,
        other_public_key: &Pkey,
        mut output: CSliceMut,
    ) -> Result<(), String> {
        let result = unsafe { bssl_sys::EVP_PKEY_derive_init(self.ptr) };
        assert_eq!(result, 1, "bssl_sys::EVP_PKEY_derive_init failed");

        let result = unsafe { bssl_sys::EVP_PKEY_derive_set_peer(self.ptr, other_public_key.ptr) };
        assert_eq!(result, 1, "bssl_sys::EVP_PKEY_derive_set_peer failed");

        let result =
            unsafe { bssl_sys::EVP_PKEY_derive(self.ptr, output.as_mut_ptr(), &mut output.len()) };
        match result {
            0 => Err("bssl_sys::EVP_PKEY_derive failed".to_owned()),
            1 => Ok(()),
            _ => panic!("Unexpected result {result:?} from bssl_sys::EVP_PKEY_derive"),
        }
    }
}

impl Drop for PkeyCtx {
    fn drop(&mut self) {
        // Safety: self.ptr is owned by this struct
        unsafe { bssl_sys::EVP_PKEY_CTX_free(self.ptr) }
    }
}
