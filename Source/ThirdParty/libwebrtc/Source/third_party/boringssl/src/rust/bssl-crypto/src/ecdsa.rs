/* Copyright (c) 2024, Google Inc.
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

//! Elliptic Curve Digital Signature Algorithm.
//!
//! The module implements ECDSA for the NIST curves P-256 and P-384.
//!
//! ```
//! use bssl_crypto::{ecdsa, ec::P256};
//!
//! let key = ecdsa::PrivateKey::<P256>::generate();
//! // Publish your public key.
//! let public_key_bytes = key.to_der_subject_public_key_info();
//!
//! // Sign and publish some message.
//! let signed_message = b"hello world";
//! let mut sig = key.sign(signed_message);
//!
//! // Anyone with the public key can verify it.
//! let public_key = ecdsa::PublicKey::<P256>::from_der_subject_public_key_info(
//!     public_key_bytes.as_ref()).unwrap();
//! assert!(public_key.verify(signed_message, sig.as_slice()).is_ok());
//! ```

use crate::{ec, sealed, with_output_vec, Buffer, FfiSlice, InvalidSignatureError};
use alloc::vec::Vec;
use core::marker::PhantomData;

/// An ECDSA public key over the given curve.
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

    /// Parse a public key in SubjectPublicKeyInfo format.
    /// (This is found in, e.g., X.509 certificates.)
    pub fn from_der_subject_public_key_info(spki: &[u8]) -> Option<Self> {
        let point = ec::Point::from_der_subject_public_key_info(C::group(sealed::Sealed), spki)?;
        Some(Self {
            point,
            marker: PhantomData,
        })
    }

    /// Serialize this key in SubjectPublicKeyInfo format.
    pub fn to_der_subject_public_key_info(&self) -> Buffer {
        self.point.to_der_subject_public_key_info()
    }

    /// Verify `signature` as a valid signature of a digest of `signed_msg`
    /// with this public key. SHA-256 will be used to produce the digest if the
    /// curve of this public key is P-256. SHA-384 will be used to produce the
    /// digest if the curve of this public key is P-384.
    pub fn verify(&self, signed_msg: &[u8], signature: &[u8]) -> Result<(), InvalidSignatureError> {
        let digest = C::hash(signed_msg);
        let result = self.point.with_point_as_ec_key(|ec_key| unsafe {
            // Safety: `ec_key` is valid per `with_point_as_ec_key`.
            bssl_sys::ECDSA_verify(
                /*type=*/ 0,
                digest.as_slice().as_ffi_ptr(),
                digest.len(),
                signature.as_ffi_ptr(),
                signature.len(),
                ec_key,
            )
        });
        if result == 1 {
            Ok(())
        } else {
            Err(InvalidSignatureError)
        }
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

    /// Return the private key as zero-padded, big-endian bytes.
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

    /// Parse a PrivateKeyInfo structure (from RFC 5208), commonly called
    /// "PKCS#8 format". The key must be on the specified curve.
    pub fn from_der_private_key_info(der: &[u8]) -> Option<Self> {
        let key = ec::Key::from_der_private_key_info(C::group(sealed::Sealed), der)?;
        Some(Self {
            key,
            marker: PhantomData,
        })
    }

    /// Serialize this private key as a PrivateKeyInfo structure (from RFC 5208),
    /// commonly called "PKCS#8 format".
    pub fn to_der_private_key_info(&self) -> Buffer {
        self.key.to_der_private_key_info()
    }

    /// Serialize the _public_ part of this key in uncompressed X9.62 format.
    pub fn to_x962_uncompressed(&self) -> Buffer {
        self.key.to_x962_uncompressed()
    }

    /// Serialize this key in SubjectPublicKeyInfo format.
    pub fn to_der_subject_public_key_info(&self) -> Buffer {
        self.key.to_der_subject_public_key_info()
    }

    /// Return the public key corresponding to this private key.
    pub fn to_public_key(&self) -> PublicKey<C> {
        PublicKey {
            point: self.key.to_point(),
            marker: PhantomData,
        }
    }

    /// Sign a digest of `to_be_signed` using this key and return the signature.
    /// SHA-256 will be used to produce the digest if the curve of this public
    /// key is P-256. SHA-384 will be used to produce the digest if the curve
    /// of this public key is P-384.
    pub fn sign(&self, to_be_signed: &[u8]) -> Vec<u8> {
        // Safety: `self.key` is valid by construction.
        let max_size = unsafe { bssl_sys::ECDSA_size(self.key.as_ffi_ptr()) };
        // No curve can be empty.
        assert_ne!(max_size, 0);

        let digest = C::hash(to_be_signed);

        unsafe {
            with_output_vec(max_size, |out_buf| {
                let mut out_len: core::ffi::c_uint = 0;
                // Safety: `out_buf` points to at least `max_size` bytes,
                // as required.
                let result = {
                    bssl_sys::ECDSA_sign(
                        /*type=*/ 0,
                        digest.as_slice().as_ffi_ptr(),
                        digest.len(),
                        out_buf,
                        &mut out_len,
                        self.key.as_ffi_ptr(),
                    )
                };
                // Signing should never fail unless we're out of memory,
                // which this crate doesn't handle.
                assert_eq!(result, 1);
                let out_len = out_len as usize;
                assert!(out_len <= max_size);
                // Safety: `out_len` bytes have been written.
                out_len
            })
        }
    }
}

#[cfg(test)]
mod test {
    use super::*;
    use crate::ec::{P256, P384};

    fn check_curve<C: ec::Curve>() {
        let signed_message = b"hello world";
        let key = PrivateKey::<C>::generate();
        let mut sig = key.sign(signed_message);

        let public_key = PublicKey::<C>::from_der_subject_public_key_info(
            key.to_der_subject_public_key_info().as_ref(),
        )
        .unwrap();
        assert!(public_key.verify(signed_message, sig.as_slice()).is_ok());

        sig[10] ^= 1;
        assert!(public_key.verify(signed_message, sig.as_slice()).is_err());
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
