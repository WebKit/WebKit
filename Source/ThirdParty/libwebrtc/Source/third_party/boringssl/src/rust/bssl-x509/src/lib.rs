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

#![deny(
    missing_docs,
    unsafe_op_in_unsafe_fn,
    clippy::missing_safety_doc,
    clippy::indexing_slicing,
    clippy::unwrap_used,
    clippy::panic,
    clippy::expect_used,
    clippy::undocumented_unsafe_blocks
)]
#![allow(private_bounds)]
#![cfg_attr(not(any(feature = "std", test)), no_std)]

//! BoringSSL crypto/x509 bindings.
//!
//! *WARNING* this crate is still work in progress and unstable.
//!
//! BoringSSL contains crypto/x509, an X.509 library inherited and cleaned up
//! from OpenSSL. This crate contains bindings for it. While documentation
//! is provided here, you may also need to see [the
//! documentation](https://boringssl.googlesource.com/boringssl/+/refs/heads/main/include/openssl/x509.h)
//! for the underlying C API.
//!
//! (BoringSSL also contains libpki, a much more modern and correct X.509
//! validation library which is used in Chromium. But crypto/x509 is still used
//! in many contexts and there can be valid reasons to want to use it.)

extern crate alloc;
extern crate core;

pub mod certificates;
pub mod errors;
pub(crate) mod ffi;
pub mod keys;
#[allow(dead_code)]
mod oids;
pub mod params;
pub mod store;
pub mod verify;

#[cfg(test)]
mod tests;

/// Extract library error per BoringSSL specification.
#[doc(hidden)]
#[macro_export]
macro_rules! check_lib_error {
    ($e:expr) => {
        match $e {
            1 => {}
            _ => return Err($crate::errors::PkiError::extract_lib_err()),
        }
    };
}
