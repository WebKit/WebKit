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

#![deny(
    missing_docs,
    unsafe_op_in_unsafe_fn,
    clippy::indexing_slicing,
    clippy::unwrap_used,
    clippy::panic,
    clippy::expect_used
)]
#![cfg_attr(not(any(feature = "std", test)), no_std)]

//! Rust BoringSSL bindings
extern crate alloc;

extern crate core;

/// Authenticated Encryption with Additional Data algorithms.
pub mod aead;

/// AES block operations.
pub mod aes;

/// Ciphers.
pub mod cipher;

/// Hash functions.
pub mod digest;

/// Ed25519, a signature scheme.
pub mod ed25519;

/// HKDF, a hash-based key derivation function.
pub mod hkdf;

/// HMAC, a hash-based message authentication code.
pub mod hmac;

/// Random number generation.
pub mod rand;

/// X25519 elliptic curve operations.
pub mod x25519;

/// Memory-manipulation operations.
pub mod mem;

/// Elliptic curve diffie-hellman operations.
pub mod ecdh;

pub(crate) mod bn;
pub(crate) mod ec;
pub(crate) mod pkey;

#[cfg(test)]
mod test_helpers;

/// This is a helper struct which provides functions for passing slices over FFI.
struct CSlice<'a>(&'a [u8]);

impl<'a> From<&'a [u8]> for CSlice<'a> {
    fn from(value: &'a [u8]) -> Self {
        Self(value)
    }
}

impl CSlice<'_> {
    /// Returns a raw pointer to the value, which is safe to pass over FFI.
    pub fn as_ptr<T>(&self) -> *const T {
        if self.0.is_empty() {
            core::ptr::null()
        } else {
            self.0.as_ptr() as *const T
        }
    }

    pub fn len(&self) -> usize {
        self.0.len()
    }
}

/// This is a helper struct which provides functions for passing mutable slices over FFI.
struct CSliceMut<'a>(&'a mut [u8]);

impl CSliceMut<'_> {
    /// Returns a raw pointer to the value, which is safe to pass over FFI.
    pub fn as_mut_ptr<T>(&mut self) -> *mut T {
        if self.0.is_empty() {
            core::ptr::null_mut()
        } else {
            self.0.as_mut_ptr() as *mut T
        }
    }

    pub fn len(&self) -> usize {
        self.0.len()
    }
}

impl<'a> From<&'a mut [u8]> for CSliceMut<'a> {
    fn from(value: &'a mut [u8]) -> Self {
        Self(value)
    }
}

/// A helper trait implemented by types which reference borrowed foreign types.
///
/// # Safety
///
/// Implementations of `ForeignTypeRef` must guarantee the following:
///
/// - `Self::from_ptr(x).as_ptr() == x`
/// - `Self::from_ptr_mut(x).as_ptr() == x`
unsafe trait ForeignTypeRef: Sized {
    /// The raw C type.
    type CType;

    /// Constructs a shared instance of this type from its raw type.
    ///
    /// # Safety
    ///
    /// `ptr` must be a valid, immutable, instance of the type for the `'a` lifetime.
    #[inline]
    unsafe fn from_ptr<'a>(ptr: *mut Self::CType) -> &'a Self {
        debug_assert!(!ptr.is_null());
        unsafe { &*(ptr as *mut _) }
    }

    /// Constructs a mutable reference of this type from its raw type.
    ///
    /// # Safety
    ///
    /// `ptr` must be a valid, unique, instance of the type for the `'a` lifetime.
    #[inline]
    unsafe fn from_ptr_mut<'a>(ptr: *mut Self::CType) -> &'a mut Self {
        debug_assert!(!ptr.is_null());
        unsafe { &mut *(ptr as *mut _) }
    }

    /// Returns a raw pointer to the wrapped value.
    #[inline]
    fn as_ptr(&self) -> *mut Self::CType {
        self as *const _ as *mut _
    }
}

/// A helper trait implemented by types which has an owned reference to foreign types.
///
/// # Safety
///
/// Implementations of `ForeignType` must guarantee the following:
///
/// - `Self::from_ptr(x).as_ptr() == x`
unsafe trait ForeignType {
    /// The raw C type.
    type CType;

    /// Constructs an instance of this type from its raw type.
    ///
    /// # Safety
    ///
    /// - `ptr` must be a valid, immutable, instance of `CType`.
    /// - Ownership of `ptr` is passed to the implementation, and will free `ptr` when dropped.
    unsafe fn from_ptr(ptr: *mut Self::CType) -> Self;

    /// Returns a raw pointer to the wrapped value.
    fn as_ptr(&self) -> *mut Self::CType;
}
