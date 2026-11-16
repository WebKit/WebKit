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

//! A special pseudo-random function used by TLS up to version 1.2
//!
//! This function is described in [RFC 5246] Section 5.
//!
//! [RFC 5246]: https://datatracker.ietf.org/doc/html/rfc5246#section-5

use core::marker::PhantomData;

use bssl_sys::CRYPTO_tls1_prf;

use crate::{digest, sealed, FfiMutSlice, FfiSlice, ForeignTypeRef};

/// The special pseudo-random function used by TLS 1.2
pub struct Tls12Prf<A>(PhantomData<fn() -> A>);

impl<A: digest::Algorithm> Tls12Prf<A> {
    /// Generate a new secret using the definition in [RFC 5246] Section 5.
    ///
    /// [RFC 5246]: https://datatracker.ietf.org/doc/html/rfc5246#section-5
    // TODO(@xfding) switch to const generics when associated const
    // is stable at generic location.
    pub fn generate_secret(
        secret: &[u8],
        label: &[u8],
        seed1: &[u8],
        seed2: Option<&[u8]>,
        output: &mut [u8],
    ) -> Result<(), ()> {
        let (seed2_ptr, seed2_len) = if let Some(seed2) = seed2 {
            (seed2.as_ffi_ptr(), seed2.len())
        } else {
            (core::ptr::null(), 0)
        };

        let ret = unsafe {
            // Safety:
            // - All pointers are valid and live at this point.
            // - All buffer lengths are verified.
            CRYPTO_tls1_prf(
                A::get_md(sealed::SealedType).as_ptr(),
                output.as_mut_ffi_ptr(),
                output.len(),
                secret.as_ffi_ptr(),
                secret.len(),
                label.as_ffi_ptr(),
                label.len(),
                seed1.as_ffi_ptr(),
                seed1.len(),
                seed2_ptr,
                seed2_len,
            )
        };
        if ret == 1 {
            Ok(())
        } else {
            Err(())
        }
    }
}
