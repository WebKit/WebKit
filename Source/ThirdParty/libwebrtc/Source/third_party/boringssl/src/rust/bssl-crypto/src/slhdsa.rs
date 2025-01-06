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
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE. */

//! SLH-DSA-SHA2-128s.
//!
//! SLH-DSA-SHA2-128s is a post-quantum signature scheme. Its has a security
//! reduction to the underlying hash function (SHA-256), giving an extremely high
//! level of cryptoanalytic security. However, it has very large signatures (7856
//! bytes) and signing is very slow (only a few operations per core second on
//! high-powered cores). A given private key may only be used to sign
//! 2<sup>64</sup> different messages.
//!
//! ```
//! use bssl_crypto::slhdsa;
//!
//! // Generate a key pair.
//! let (public_key, private_key) = slhdsa::PrivateKey::generate();
//!
//! // Sign a message.
//! let message = b"test message";
//! let signature = private_key.sign(message);
//!
//! // Verify the signature.
//! assert!(public_key.verify(message, &signature).is_ok());
//!
//! // Signing with a context value.
//! let context = b"context";
//! let signature2 = private_key.sign_with_context(message, context)
//!    .expect("signing will always work if the context is less than 256 bytes");
//!
//! // The signature isn't value without a matching context.
//! assert!(public_key.verify(message, &signature2).is_err());
//!
//! // ... but works with the correct context.
//! assert!(public_key.verify_with_context(message, &signature2, context).is_ok());
//! ```

use crate::{with_output_vec_fallible, FfiSlice, InvalidSignatureError};
use alloc::vec::Vec;

/// The number of bytes in an SLH-DSA-SHA2-128s public key.
pub const PUBLIC_KEY_BYTES: usize = bssl_sys::SLHDSA_SHA2_128S_PUBLIC_KEY_BYTES as usize;

/// The number of bytes in an SLH-DSA-SHA2-128s private key.
pub const PRIVATE_KEY_BYTES: usize = bssl_sys::SLHDSA_SHA2_128S_PRIVATE_KEY_BYTES as usize;

/// The number of bytes in an SLH-DSA-SHA2-128s signature.
pub const SIGNATURE_BYTES: usize = bssl_sys::SLHDSA_SHA2_128S_SIGNATURE_BYTES as usize;

/// An SLH-DSA-SHA2-128s private key.
#[derive(Clone, PartialEq, Eq)]
pub struct PrivateKey([u8; PRIVATE_KEY_BYTES]);

/// An SLH-DSA-SHA2-128s public key.
#[derive(Clone, PartialEq, Eq)]
pub struct PublicKey([u8; PUBLIC_KEY_BYTES]);

impl PrivateKey {
    /// Generates a random public/private key pair.
    pub fn generate() -> (PublicKey, Self) {
        let mut public_key = [0u8; PUBLIC_KEY_BYTES];
        let mut private_key = [0u8; PRIVATE_KEY_BYTES];

        // Safety: the sizes of the arrays are correct.
        unsafe {
            bssl_sys::SLHDSA_SHA2_128S_generate_key(
                public_key.as_mut_ptr(),
                private_key.as_mut_ptr(),
            );
        }

        (PublicKey(public_key), Self(private_key))
    }

    /// Derives the public key corresponding to this private key.
    pub fn to_public_key(&self) -> PublicKey {
        let mut public_key = [0u8; PUBLIC_KEY_BYTES];

        // Safety: the sizes of the arrays are correct.
        unsafe {
            bssl_sys::SLHDSA_SHA2_128S_public_from_private(
                public_key.as_mut_ptr(),
                self.0.as_ptr(),
            );
        }

        PublicKey(public_key)
    }

    /// Signs a message using this private key.
    pub fn sign(&self, msg: &[u8]) -> Vec<u8> {
        #[allow(clippy::expect_used)]
        self.sign_with_context(msg, &[])
            .expect("Empty context should always succeed")
    }

    /// Signs a message using this private key and the given context.
    ///
    /// This function returns None if `context` is longer than 255 bytes.
    pub fn sign_with_context(&self, msg: &[u8], context: &[u8]) -> Option<Vec<u8>> {
        // Safety: the FFI function always writes exactly `SIGNATURE_BYTES` to
        // the first argument if it succeeds. The size of the array passed as
        // the second argument is correct.
        unsafe {
            with_output_vec_fallible(SIGNATURE_BYTES, |signature| {
                if bssl_sys::SLHDSA_SHA2_128S_sign(
                    signature,
                    self.0.as_ptr(),
                    msg.as_ffi_ptr(),
                    msg.len(),
                    context.as_ffi_ptr(),
                    context.len(),
                ) == 1
                {
                    Some(SIGNATURE_BYTES)
                } else {
                    None
                }
            })
        }
    }
}

impl AsRef<[u8]> for PrivateKey {
    /// Returns the bytes of the public key.
    fn as_ref(&self) -> &[u8] {
        &self.0
    }
}

impl From<&[u8; PRIVATE_KEY_BYTES]> for PrivateKey {
    fn from(bytes: &[u8; PRIVATE_KEY_BYTES]) -> Self {
        Self(*bytes)
    }
}

impl PublicKey {
    /// Verifies a signature for a given message using this public key.
    pub fn verify(&self, msg: &[u8], signature: &[u8]) -> Result<(), InvalidSignatureError> {
        self.verify_with_context(msg, signature, &[])
    }

    /// Verifies a signature for a given message using this public key and the given context.
    pub fn verify_with_context(
        &self,
        msg: &[u8],
        signature: &[u8],
        context: &[u8],
    ) -> Result<(), InvalidSignatureError> {
        // Safety: the size of the array passed as the third argument is correct.
        let ok = unsafe {
            bssl_sys::SLHDSA_SHA2_128S_verify(
                signature.as_ffi_ptr(),
                signature.len(),
                self.0.as_ptr(),
                msg.as_ffi_ptr(),
                msg.len(),
                context.as_ffi_ptr(),
                context.len(),
            )
        };

        if ok == 1 {
            Ok(())
        } else {
            Err(InvalidSignatureError)
        }
    }
}

impl AsRef<[u8]> for PublicKey {
    /// Returns the bytes of the public key.
    fn as_ref(&self) -> &[u8] {
        &self.0
    }
}

impl From<&[u8; PUBLIC_KEY_BYTES]> for PublicKey {
    fn from(bytes: &[u8; PUBLIC_KEY_BYTES]) -> Self {
        Self(*bytes)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_key_generation_and_signature() {
        let (public_key, private_key) = PrivateKey::generate();
        let msg = b"test message";
        let sig = private_key.sign(msg);

        assert!(public_key.verify(msg, &sig).is_ok());

        let mut invalid_sig = sig.clone();
        invalid_sig[0] ^= 1;
        assert!(public_key.verify(msg, &invalid_sig).is_err());
    }

    #[test]
    fn test_sign_and_verify_with_context() {
        let (public_key, private_key) = PrivateKey::generate();
        let msg = b"test message";
        let context = b"test context";

        let sig = private_key.sign_with_context(msg, context).unwrap();
        assert!(public_key.verify_with_context(msg, &sig, context).is_ok());
        assert!(public_key
            .verify_with_context(msg, &sig, b"wrong context")
            .is_err());
    }

    #[test]
    fn test_public_key_from_private() {
        let (public_key, private_key) = PrivateKey::generate();
        let derived_public_key = private_key.to_public_key();

        assert_eq!(public_key.0, derived_public_key.0);
    }

    #[test]
    fn test_empty_message_and_context() {
        let (public_key, private_key) = PrivateKey::generate();
        let msg = b"";
        let context = b"";

        let sig = private_key.sign_with_context(msg, context).unwrap();
        assert!(public_key.verify_with_context(msg, &sig, context).is_ok());
    }

    #[test]
    fn test_max_context_length() {
        let (public_key, private_key) = PrivateKey::generate();
        let msg = b"test message";
        let context = vec![0u8; 255]; // Maximum allowed context length

        let sig = private_key.sign_with_context(msg, &context).unwrap();
        assert!(public_key.verify_with_context(msg, &sig, &context).is_ok());

        let too_long_context = vec![0u8; 256];
        assert!(private_key
            .sign_with_context(msg, &too_long_context)
            .is_none());
    }
}
