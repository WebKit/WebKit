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

//! Elliptic Curve Diffie-Hellman operations.
//!
//! This module implements ECDH over the NIST curves P-256 and P-384.
//!
//! ```
//! use bssl_crypto::{ecdh, ec::P256};
//!
//! let alice_private_key = ecdh::PrivateKey::<P256>::generate();
//! let alice_public_key_serialized = alice_private_key.to_x962_uncompressed();
//!
//! // Somehow, Alice's public key is sent to Bob.
//! let bob_private_key = ecdh::PrivateKey::<P256>::generate();
//! let alice_public_key =
//!     ecdh::PublicKey::<P256>::from_x962_uncompressed(
//!         alice_public_key_serialized.as_ref())
//!     .unwrap();
//! let shared_key1 = bob_private_key.compute_shared_key(&alice_public_key);
//!
//! // Likewise, Alice gets Bob's public key and computes the same shared key.
//! let bob_public_key = bob_private_key.to_public_key();
//! let shared_key2 = alice_private_key.compute_shared_key(&bob_public_key);
//! assert_eq!(shared_key1, shared_key2);
//! ```

use crate::{ec, sealed, with_output_vec, Buffer};
use alloc::vec::Vec;
use core::marker::PhantomData;

/// An ECDH public key over the given curve.
pub struct PublicKey<C: ec::Curve> {
    point: ec::Point,
    marker: PhantomData<C>,
}

impl<C: ec::Curve> PublicKey<C> {
    /// Parse a public key in uncompressed X9.62 format. (This is the common
    /// format for elliptic curve points beginning with an 0x04 byte.)
    pub fn from_x962_uncompressed(x962: &[u8]) -> Option<Self> {
        let point = ec::Point::from_x962_uncompressed(C::group(sealed::Sealed), x962)?;
        Some(Self {
            point,
            marker: PhantomData,
        })
    }

    /// Serialize this key as uncompressed X9.62 format.
    pub fn to_x962_uncompressed(&self) -> Buffer {
        self.point.to_x962_uncompressed()
    }
}

/// An ECDH private key over the given curve.
pub struct PrivateKey<C: ec::Curve> {
    key: ec::Key,
    marker: PhantomData<C>,
}

impl<C: ec::Curve> PrivateKey<C> {
    /// Generate a random private key.
    pub fn generate() -> Self {
        Self {
            key: ec::Key::generate(C::group(sealed::Sealed)),
            marker: PhantomData,
        }
    }

    /// Parse a `PrivateKey` from a zero-padded, big-endian representation of the secret scalar.
    pub fn from_big_endian(scalar: &[u8]) -> Option<Self> {
        let key = ec::Key::from_big_endian(C::group(sealed::Sealed), scalar)?;
        Some(Self {
            key,
            marker: PhantomData,
        })
    }

    /// Return the private scalar as zero-padded, big-endian bytes.
    pub fn to_big_endian(&self) -> Buffer {
        self.key.to_big_endian()
    }

    /// Parse an ECPrivateKey structure (from RFC 5915). The key must be on the
    /// specified curve.
    pub fn from_der_ec_private_key(der: &[u8]) -> Option<Self> {
        let key = ec::Key::from_der_ec_private_key(C::group(sealed::Sealed), der)?;
        Some(Self {
            key,
            marker: PhantomData,
        })
    }

    /// Serialize this private key as an ECPrivateKey structure (from RFC 5915).
    pub fn to_der_ec_private_key(&self) -> Buffer {
        self.key.to_der_ec_private_key()
    }

    /// Parse a PrivateKeyInfo structure (from RFC 5208). The key must be on the
    /// specified curve.
    pub fn from_der_private_key_info(der: &[u8]) -> Option<Self> {
        let key = ec::Key::from_der_private_key_info(C::group(sealed::Sealed), der)?;
        Some(Self {
            key,
            marker: PhantomData,
        })
    }

    /// Serialize this private key as a PrivateKeyInfo structure (from RFC 5208).
    pub fn to_der_private_key_info(&self) -> Buffer {
        self.key.to_der_private_key_info()
    }

    /// Serialize the _public_ part of this key in uncompressed X9.62 format.
    pub fn to_x962_uncompressed(&self) -> Buffer {
        self.key.to_x962_uncompressed()
    }

    /// Compute the shared key between this private key and the given public key.
    /// The result should be used with a key derivation function that includes
    /// the two public keys.
    pub fn compute_shared_key(&self, other_public_key: &PublicKey<C>) -> Vec<u8> {
        // 384 bits is the largest curve supported. The buffer is sized to be
        // larger than this so that truncation of the output can be noticed.
        let max_output = 384 / 8 + 1;
        unsafe {
            with_output_vec(max_output, |out_buf| {
                // Safety:
                //   - `out_buf` points to at least `max_output` bytes, as
                //     required.
                //   - The `EC_POINT` and `EC_KEY` pointers are valid by construction.
                let num_out_bytes = bssl_sys::ECDH_compute_key(
                    out_buf as *mut core::ffi::c_void,
                    max_output,
                    other_public_key.point.as_ffi_ptr(),
                    self.key.as_ffi_ptr(),
                    None,
                );
                // Out of memory is not handled by this crate.
                assert!(num_out_bytes > 0);
                let num_out_bytes = num_out_bytes as usize;
                // If the buffer was completely filled then it was probably
                // truncated, which should never happen.
                assert!(num_out_bytes < max_output);
                num_out_bytes
            })
        }
    }

    /// Return the public key corresponding to this private key.
    pub fn to_public_key(&self) -> PublicKey<C> {
        PublicKey {
            point: self.key.to_point(),
            marker: PhantomData,
        }
    }
}

#[cfg(test)]
mod test {
    use super::*;
    use crate::ec::{P256, P384};

    fn check_curve<C: ec::Curve>() {
        let alice_private_key = PrivateKey::<C>::generate();
        let alice_public_key = alice_private_key.to_public_key();
        let alice_private_key =
            PrivateKey::<C>::from_big_endian(alice_private_key.to_big_endian().as_ref()).unwrap();
        let alice_private_key = PrivateKey::<C>::from_der_ec_private_key(
            alice_private_key.to_der_ec_private_key().as_ref(),
        )
        .unwrap();

        let bob_private_key = PrivateKey::<C>::generate();
        let bob_public_key = bob_private_key.to_public_key();

        let shared_key1 = alice_private_key.compute_shared_key(&bob_public_key);
        let shared_key2 = bob_private_key.compute_shared_key(&alice_public_key);

        assert_eq!(shared_key1, shared_key2);
    }

    #[test]
    fn p256() {
        check_curve::<P256>();
    }

    #[test]
    fn p384() {
        check_curve::<P384>();
    }
}
