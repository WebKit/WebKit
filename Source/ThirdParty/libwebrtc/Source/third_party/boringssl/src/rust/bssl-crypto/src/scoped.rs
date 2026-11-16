// Copyright 2024 The BoringSSL Authors
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

//! Helpers to ensure that some temporary objects are always freed.

use crate::{initialized_struct, FfiSlice};

/// A scoped `EC_KEY`.
pub struct EvpPkey(*mut bssl_sys::EVP_PKEY);

impl EvpPkey {
    pub fn new() -> Self {
        let ptr = unsafe { bssl_sys::EVP_PKEY_new() };
        // `ptr` is only NULL if we're out of memory, which this crate
        // doesn't handle.
        assert!(!ptr.is_null());
        EvpPkey(ptr)
    }

    pub fn from_ptr(ptr: *mut bssl_sys::EVP_PKEY) -> Self {
        EvpPkey(ptr)
    }

    pub fn from_ptr_or_null(ptr: *mut bssl_sys::EVP_PKEY) -> Option<Self> {
        if ptr.is_null() {
            None
        } else {
            Some(EvpPkey::from_ptr(ptr))
        }
    }

    pub fn from_der_subject_public_key_info(
        spki: &[u8],
        algs: &[*const bssl_sys::EVP_PKEY_ALG],
    ) -> Option<Self> {
        EvpPkey::from_ptr_or_null(
            // Safety: Both input buffers are valid.
            unsafe {
                bssl_sys::EVP_PKEY_from_subject_public_key_info(
                    spki.as_ffi_ptr(),
                    spki.len(),
                    algs.as_ffi_ptr(),
                    algs.len(),
                )
            },
        )
    }

    pub fn from_der_private_key_info(
        spki: &[u8],
        algs: &[*const bssl_sys::EVP_PKEY_ALG],
    ) -> Option<Self> {
        EvpPkey::from_ptr_or_null(
            // Safety: Both input buffers are valid.
            unsafe {
                bssl_sys::EVP_PKEY_from_private_key_info(
                    spki.as_ffi_ptr(),
                    spki.len(),
                    algs.as_ffi_ptr(),
                    algs.len(),
                )
            },
        )
    }

    pub fn as_ffi_ptr(&mut self) -> *mut bssl_sys::EVP_PKEY {
        self.0
    }
}

impl Drop for EvpPkey {
    fn drop(&mut self) {
        unsafe { bssl_sys::EVP_PKEY_free(self.0) }
    }
}

/// A scoped `EC_KEY`.
pub struct EcKey(*mut bssl_sys::EC_KEY);

impl EcKey {
    pub fn new() -> Self {
        let ptr = unsafe { bssl_sys::EC_KEY_new() };
        // `ptr` is only NULL if we're out of memory, which this crate
        // doesn't handle.
        assert!(!ptr.is_null());
        EcKey(ptr)
    }

    pub fn as_ffi_ptr(&mut self) -> *mut bssl_sys::EC_KEY {
        self.0
    }
}

impl Drop for EcKey {
    fn drop(&mut self) {
        unsafe { bssl_sys::EC_KEY_free(self.0) }
    }
}

/// A scoped `EVP_HPKE_CTX`.
pub struct EvpHpkeCtx(*mut bssl_sys::EVP_HPKE_CTX);
// bssl_sys::EVP_HPKE_CTX is heap-allocated and safe to transfer
// between threads.
unsafe impl Send for EvpHpkeCtx {}

impl EvpHpkeCtx {
    pub fn new() -> Self {
        let ptr = unsafe { bssl_sys::EVP_HPKE_CTX_new() };
        // `ptr` is only NULL if we're out of memory, which this crate
        // doesn't handle.
        assert!(!ptr.is_null());
        EvpHpkeCtx(ptr)
    }

    pub fn as_ffi_ptr(&self) -> *const bssl_sys::EVP_HPKE_CTX {
        self.0
    }

    pub fn as_mut_ffi_ptr(&mut self) -> *mut bssl_sys::EVP_HPKE_CTX {
        self.0
    }
}

impl Drop for EvpHpkeCtx {
    fn drop(&mut self) {
        unsafe { bssl_sys::EVP_HPKE_CTX_free(self.0) }
    }
}

/// A scoped `EVP_HPKE_KEY`.
pub struct EvpHpkeKey(bssl_sys::EVP_HPKE_KEY);

impl EvpHpkeKey {
    pub fn new() -> Self {
        EvpHpkeKey(unsafe { initialized_struct(|ptr| bssl_sys::EVP_HPKE_KEY_zero(ptr)) })
    }

    pub fn as_ffi_ptr(&self) -> *const bssl_sys::EVP_HPKE_KEY {
        &self.0
    }

    pub fn as_mut_ffi_ptr(&mut self) -> *mut bssl_sys::EVP_HPKE_KEY {
        &mut self.0
    }
}

impl Drop for EvpHpkeKey {
    fn drop(&mut self) {
        // unsafe: the only way to create a `EvpHpkeKey` is via `new` and that
        // ensures that this structure is initialized.
        unsafe { bssl_sys::EVP_HPKE_KEY_cleanup(&mut self.0) }
    }
}

/// A scoped `BIGNUM`.
pub struct Bignum(bssl_sys::BIGNUM);

impl Bignum {
    pub fn from_u64(value: u64) -> Self {
        let mut ret = Bignum(unsafe { initialized_struct(|ptr| bssl_sys::BN_init(ptr)) });
        assert_eq!(1, unsafe { bssl_sys::BN_set_u64(&mut ret.0, value) });
        ret
    }

    pub fn as_ffi_ptr(&self) -> *const bssl_sys::BIGNUM {
        &self.0
    }
}

impl Drop for Bignum {
    fn drop(&mut self) {
        unsafe { bssl_sys::BN_free(&mut self.0) }
    }
}
