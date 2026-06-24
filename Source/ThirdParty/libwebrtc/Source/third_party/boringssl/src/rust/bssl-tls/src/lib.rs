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
#![recursion_limit = "512"]

//! BoringSSL TLS bindings
//!
//! *WARNING* this crate is still work in progress.

extern crate alloc;
extern crate core;

use core::panic::AssertUnwindSafe;

pub mod alerts;
pub mod config;
pub mod connection;
pub mod context;
pub mod credentials;
pub mod errors;
mod ffi;
pub mod io;
mod methods;
pub mod sessions;
#[macro_use]
#[doc(hidden)]
mod macros;
#[cfg(test)]
#[cfg(feature = "std")]
mod tests;

pub use ffi::ReceiveBuffer;

fn has_duplicates<T: Ord + Eq>(list: &[T]) -> bool {
    let mut seen = alloc::collections::BTreeSet::new();
    list.iter().any(|elem| !seen.insert(elem))
}

pub(crate) trait Methods {
    /// Safety: `ssl` must outlive `'a` and it must be passed in from BoringSSL
    /// through vtable calls.
    unsafe extern "C" fn from_ssl<'a>(ssl: *mut bssl_sys::SSL) -> Option<&'a Self>;
}

pub(crate) trait VerifyCertificateMethods: Methods {
    fn verify_certificate_methods(&self) -> Option<&dyn credentials::VerifyCertificate>;
}

#[inline]
fn abort_on_panic<T>(work: impl FnOnce() -> T) -> T {
    let assert_unwind_safe = AssertUnwindSafe(work);
    let call = move || {
        let AssertUnwindSafe(work) = { assert_unwind_safe };
        work()
    };
    #[cfg(feature = "std")]
    let res = match std::panic::catch_unwind(call) {
        Ok(res) => res,
        Err(_) => {
            eprintln!("panic about to cross language boundary");
            std::process::abort()
        }
    };
    #[cfg(not(feature = "std"))]
    let res = call();
    res
}
