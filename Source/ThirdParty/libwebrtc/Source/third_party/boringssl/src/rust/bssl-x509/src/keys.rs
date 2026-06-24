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

//! Private and public keys
//!
//! The private keys processed here can be paired with certificates to perform further
//! authentication.
//!
//! ```rust
//! # use bssl_x509::certificates::X509Certificate;
//! # use bssl_x509::keys::PrivateKey;
//! # let pem = include_bytes!("tests/BoringSSLTestCA.key");
//! # let crt = include_bytes!("tests/BoringSSLTestCA.crt");
//! # let crt = X509Certificate::parse_one_from_pem(crt).unwrap();
//! let key = PrivateKey::from_pem(
//!     pem, /*password_callback=*/ || b"BoringSSL is awesome!").unwrap();
//! assert!(crt.matches_private_key(&key));
//! ```
//!
//! The public keys can be derived from [`crate::certificates::X509Certificate`] or [`PrivateKey`]s.
//!
//! ```rust
//! # use bssl_x509::certificates::X509Certificate;
//! # use bssl_x509::keys::PrivateKey;
//! # let pem = include_bytes!("tests/BoringSSLTestCA.key");
//! # let crt = include_bytes!("tests/BoringSSLTestCA.crt");
//! # let crt = X509Certificate::parse_one_from_pem(crt).unwrap();
//! # let key = PrivateKey::from_pem(
//! #     pem, /*password_callback=*/ || b"BoringSSL is awesome!").unwrap();
//! assert_eq!(
//!     crt.public_key().unwrap().to_der(),
//!     key.to_public_key().to_der()
//! );
//! ```

use alloc::vec::Vec;
use core::{
    ffi::{c_char, c_int, c_void},
    mem::transmute,
    panic::AssertUnwindSafe,
    ptr::{NonNull, null_mut},
};

use bssl_crypto::{FfiSlice, cbb_to_buffer};
use bssl_macros::bssl_enum;

use crate::ffi::abort_on_panic;
use crate::{errors::PkiError, ffi::Bio};

bssl_enum! {
    /// EVP public key algorithm types.
    #[derive(Debug, Copy, Clone, PartialEq, Eq)]
    pub enum PrivateKeyAlgorithm: i32 {
        /// RSA
        Rsa = bssl_sys::EVP_PKEY_RSA as i32,
        /// RSA-PSS
        RsaPss = bssl_sys::EVP_PKEY_RSA_PSS as i32,
        /// EC
        Ec = bssl_sys::EVP_PKEY_EC as i32,
        /// Ed25519
        Ed25519 = bssl_sys::EVP_PKEY_ED25519 as i32,
        /// X25519
        X25519 = bssl_sys::EVP_PKEY_X25519 as i32,
        /// DSA
        Dsa = bssl_sys::EVP_PKEY_DSA as i32,
        /// DH
        Dh = bssl_sys::EVP_PKEY_DH as i32,
    }
}

/// A private key.
#[repr(transparent)]
pub struct PrivateKey(NonNull<bssl_sys::EVP_PKEY>);
// Safety: `PrivateKey` is locked as immutable at this type state.
unsafe impl Send for PrivateKey {}
unsafe impl Sync for PrivateKey {}

impl PrivateKey {
    /// Parse a [`PrivateKey`] from PEM encoding.
    pub fn from_pem<'a, F: 'a + FnMut() -> &'a [u8]>(
        pem: &[u8],
        mut password_callback: F,
    ) -> Result<Self, PkiError> {
        let mut bio = Bio::from_bytes(pem)?;

        let mut priv_key = null_mut();

        unsafe extern "C" fn write_password<'a, F: 'a + FnMut() -> &'a [u8]>(
            out: *mut c_char,
            size: c_int,
            _rwflag: c_int,
            ctxt: *mut c_void,
        ) -> c_int {
            if size < 0 {
                return -1;
            }
            let password_callback = AssertUnwindSafe(unsafe {
                // Safety: `ctxt` is a valid callback pointer and outlived by `'a`, so it must be
                // valid when invoked.
                &mut *(ctxt as *mut F)
            });
            let get_password = move || {
                let AssertUnwindSafe(pass_callback) = { password_callback };
                let password = pass_callback();
                let Ok(len) = password.len().try_into() else {
                    return -1;
                };
                if len > size {
                    return -1;
                }
                unsafe {
                    // Safety:
                    // - `src` is valid and not larger than `out`.
                    // - `out` is valid per BoringSSL specification.
                    // - `src` and `out` are both 1-aligned.
                    core::ptr::copy(password.as_ffi_void_ptr(), out as _, password.len());
                }
                len
            };
            abort_on_panic(get_password)
        }

        let evp_pkey = unsafe {
            // Safety:
            // - the BIO is still valid.
            // - the `priv_key` pointer is null, so the function will allocate
            //   a new structure.
            bssl_sys::PEM_read_bio_PrivateKey(
                bio.ptr(),
                &raw mut priv_key,
                Some(write_password::<'a, F>),
                &raw mut password_callback as _,
            )
        };
        NonNull::new(evp_pkey)
            .map(Self)
            .ok_or_else(PkiError::extract_lib_err)
    }

    /// Get the algorithm ID of the private key.
    ///
    /// This method returns [`None`] if the key algorithm is unrecognised.
    pub fn algorithm(&self) -> Option<PrivateKeyAlgorithm> {
        let id = unsafe {
            // Safety: self.0 is valid.
            bssl_sys::EVP_PKEY_id(self.ptr())
        };
        let id = i32::try_from(id).ok()?;
        PrivateKeyAlgorithm::try_from(id).ok()
    }

    /// This method releases ownership of the internal key handle.
    ///
    /// # Safety
    /// - This method should only be used for cross-language interoperability,
    ///   so the function that accepts an `EVP_PKEY*` handle must uses exactly the same
    ///   BoringSSL as this crate is linked to.
    pub fn into_raw(self) -> *mut bssl_sys::EVP_PKEY {
        let ptr = self.ptr();
        core::mem::forget(self);
        ptr
    }

    /// This method returns the internal key handle.
    /// # Safety
    /// - This method should only be used for cross-language interoperability,
    ///   so the function that accepts an `EVP_PKEY*` handle must uses exactly the same
    ///   BoringSSL as this crate is linked to.
    /// - The caller must ensure that all future uses of the key handle does not mutate the content.
    /// - All subsequent uses of the returned handle must be outlived by `self`.
    /// - All subsequent uses of the returned handle must not race with uses of `self`.
    ///   unless only refcount is adjusted, one must use locks when necessary.
    pub unsafe fn as_mut_ptr(&mut self) -> *mut bssl_sys::EVP_PKEY {
        self.ptr()
    }

    pub(crate) fn ptr(&self) -> *mut bssl_sys::EVP_PKEY {
        self.0.as_ptr()
    }

    /// Encode the private key to a DER-encoded `PrivateKeyInfo` structure as per [RFC 5208].
    ///
    /// [RFC 5208]: <https://datatracker.ietf.org/doc/html/rfc5208#section-5>
    pub fn private_key_to_der(&self) -> Vec<u8> {
        cbb_to_buffer(0, |cbb| unsafe {
            // Safety: this call does not mutate the private key internals
            // and only allocation happens.
            assert_eq!(1, bssl_sys::EVP_marshal_private_key(cbb, self.ptr()))
        })
        .as_ref()
        .to_vec()
    }

    /// Get the public key.
    pub fn to_public_key(&self) -> &PublicKey {
        unsafe {
            // Safety: `self.0` is still valid.
            debug_assert_eq!(1, bssl_sys::EVP_PKEY_has_public(self.ptr()));
        }
        unsafe {
            // Safety:
            // - the `EVP_PKEY*` in `PrivateKey` always contains a public key.
            // - `PublicKey` is a transparent wrapper of `EVP_PKEY*`
            transmute(self)
        }
    }
}

impl Clone for PrivateKey {
    fn clone(&self) -> Self {
        unsafe {
            // Safety: `self.0` is still valid at cloning.
            bssl_sys::EVP_PKEY_up_ref(self.ptr());
        }
        Self(self.0)
    }
}

impl Drop for PrivateKey {
    fn drop(&mut self) {
        unsafe {
            // Safety: `self.0` is still valid at dropping.
            bssl_sys::EVP_PKEY_free(self.ptr());
        }
    }
}

/// A public key.
#[repr(transparent)]
pub struct PublicKey(pub(crate) NonNull<bssl_sys::EVP_PKEY>);
// Safety: `PublicKey` is locked as immutable at this type state.
unsafe impl Send for PublicKey {}
unsafe impl Sync for PublicKey {}

impl PublicKey {
    /// Parse a [`PublicKey`] from PEM encoding.
    pub fn from_pem(pem: &[u8]) -> Result<Self, PkiError> {
        let mut bio = Bio::from_bytes(pem)?;

        let mut pub_key = null_mut();
        let evp_pkey = unsafe {
            // Safety:
            // - the BIO is still valid.
            // - the `pub_key` pointer is null, so the function will allocate
            //   a new structure.
            bssl_sys::PEM_read_bio_PUBKEY(bio.ptr(), &raw mut pub_key, None, null_mut())
        };
        NonNull::new(evp_pkey)
            .map(Self)
            .ok_or_else(PkiError::extract_lib_err)
    }

    /// Serialize the public key into a DER-encoded `SubjectPublicKeyInfo` structure.
    pub fn to_der(&self) -> Vec<u8> {
        cbb_to_buffer(0, |cbb| unsafe {
            // Safety: this call does not mutate the private key internals
            // and only allocation happens.
            assert_eq!(1, bssl_sys::EVP_marshal_public_key(cbb, self.ptr()))
        })
        .as_ref()
        .to_vec()
    }

    pub(crate) fn ptr(&self) -> *mut bssl_sys::EVP_PKEY {
        self.0.as_ptr()
    }

    /// This method returns the internal key handle.
    /// # Safety
    /// - This method should only be used for cross-language interoperability,
    ///   so the function that accepts an `EVP_PKEY*` handle must uses exactly the same
    ///   BoringSSL as this crate is linked to.
    /// - The caller must ensure that all future uses of the key handle does not mutate the content.
    /// - All subsequent uses of the returned handle must be outlived by `self`.
    /// - All subsequent uses of the returned handle must not race with uses of `self`.
    ///   unless only refcount is adjusted, one must use locks when necessary.
    pub unsafe fn as_mut_ptr(&mut self) -> *mut bssl_sys::EVP_PKEY {
        self.ptr()
    }
}

impl Clone for PublicKey {
    fn clone(&self) -> Self {
        unsafe {
            // Safety: `self.0` is still valid at cloning.
            bssl_sys::EVP_PKEY_up_ref(self.ptr());
        }
        Self(self.0)
    }
}

impl Drop for PublicKey {
    fn drop(&mut self) {
        unsafe {
            // Safety: `self.0` is still valid at dropping.
            bssl_sys::EVP_PKEY_free(self.ptr());
        }
    }
}
