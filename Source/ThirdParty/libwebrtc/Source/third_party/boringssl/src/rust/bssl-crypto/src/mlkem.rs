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

//! ML-KEM
//!
//! ML-KEM is a post-quantum key encapsulation mechanism, specified in
//! [FIPS 203](https://csrc.nist.gov/pubs/fips/203/final).
//! A KEM works like public-key encryption, except that the encrypted message is
//! always a random key chosen by the algorithm.
//!
//! ```
//! use bssl_crypto::mlkem;
//!
//! // Generate a key pair.
//! let (serialized_public_key, private_key, private_seed) = mlkem::PrivateKey768::generate();
//!
//! // Send `serialized_public_key` to the sender. The sender parses it:
//! let public_key = mlkem::PublicKey768::parse(&serialized_public_key).unwrap();
//!
//! // Generate and encrypt a shared secret key to that public key.
//! let (ciphertext, shared_key) = public_key.encapsulate();
//!
//! // Send `ciphertext` to the holder of the private key.
//! let shared_key2 = private_key.decapsulate(&ciphertext).unwrap();
//! assert_eq!(shared_key, shared_key2);
//!
//! // `shared_key` would then be used with an algorithm from the `aead` module
//! // to encrypt and authenticate data. The two keys may not match and so it's
//! // critical to use an authenticated encryption mechanism to confirm the key.
//! ```

use crate::{
    as_cbs, cbb_to_vec, initialized_boxed_struct, initialized_boxed_struct_fallible,
    with_output_array_fallible, FfiSlice,
};
use alloc::{boxed::Box, vec::Vec};
use core::mem::MaybeUninit;

/// An ML-KEM-768 public key.
pub struct PublicKey768(Box<bssl_sys::MLKEM768_public_key>);

/// An ML-KEM-768 private key.
pub struct PrivateKey768(Box<bssl_sys::MLKEM768_private_key>);

/// An ML-KEM-1024 public key.
///
/// Use ML-KEM-768 unless you have a good reason to need this larger size.
pub struct PublicKey1024(Box<bssl_sys::MLKEM1024_public_key>);

/// An ML-KEM-1024 private key.
///
/// Use ML-KEM-768 unless you have a good reason to need this larger size.
pub struct PrivateKey1024(Box<bssl_sys::MLKEM1024_private_key>);

/// The number of bytes in an encoded ML-KEM-768 public key.
pub const PUBLIC_KEY_BYTES_768: usize = bssl_sys::MLKEM768_PUBLIC_KEY_BYTES as usize;

/// The number of bytes in an ML-KEM seed.
pub const SEED_BYTES: usize = bssl_sys::MLKEM_SEED_BYTES as usize;

/// The number of bytes in the ML-KEM-768 ciphertext.
pub const CIPHERTEXT_BYTES_768: usize = bssl_sys::MLKEM768_CIPHERTEXT_BYTES as usize;

/// The number of bytes in an ML-KEM shared secret.
pub const SHARED_SECRET_BYTES: usize = bssl_sys::MLKEM_SHARED_SECRET_BYTES as usize;

/// The number of bytes in an encoded ML-KEM-1024 public key.
pub const PUBLIC_KEY_BYTES_1024: usize = bssl_sys::MLKEM1024_PUBLIC_KEY_BYTES as usize;

/// The number of bytes in the ML-KEM-1024 ciphertext.
pub const CIPHERTEXT_BYTES_1024: usize = bssl_sys::MLKEM1024_CIPHERTEXT_BYTES as usize;

impl PublicKey768 {
    /// Parse a public key from NIST's defined format.
    pub fn parse(encoded: &[u8]) -> Option<Self> {
        let mut cbs = as_cbs(encoded);
        unsafe {
            initialized_boxed_struct_fallible(|pub_key| {
                // Safety: `pub_key` is the correct size via the type system and
                // is fully written if this function returns 1.
                bssl_sys::MLKEM768_parse_public_key(pub_key, &mut cbs) == 1 && cbs.len == 0
            })
        }
        .map(Self)
    }

    /// Return the serialization of this public key.
    pub fn to_bytes(&self) -> Vec<u8> {
        unsafe {
            cbb_to_vec(PUBLIC_KEY_BYTES_768, |cbb| {
                let ok = bssl_sys::MLKEM768_marshal_public_key(cbb, &*self.0);
                // `MLKEM768_marshal_public_key` only fails if it cannot
                // allocate memory, but `cbb_to_vec` handles allocation.
                assert_eq!(ok, 1);
            })
        }
    }

    /// Generate a secret key and encrypt it to this public key, returning the
    /// ciphertext and the shared secret key.
    pub fn encapsulate(&self) -> (Vec<u8>, [u8; SHARED_SECRET_BYTES]) {
        let mut ciphertext = Box::new_uninit_slice(CIPHERTEXT_BYTES_768);
        let mut shared_secret = MaybeUninit::<[u8; SHARED_SECRET_BYTES]>::uninit();

        unsafe {
            // Safety: the two buffer arguments are sized correctly and
            // always fully written.
            bssl_sys::MLKEM768_encap(
                ciphertext.as_mut_ptr() as *mut u8,
                shared_secret.as_mut_ptr() as *mut u8,
                &*self.0,
            );
            (ciphertext.assume_init().into(), shared_secret.assume_init())
        }
    }
}

impl PrivateKey768 {
    /// Generates a random public/private key pair returning a serialized public
    /// key, a private key, and a private seed value that can be used to
    /// regenerate the same private key in the future.
    pub fn generate() -> (Vec<u8>, PrivateKey768, [u8; SEED_BYTES]) {
        let mut public_key = Box::new_uninit_slice(PUBLIC_KEY_BYTES_768);
        let mut private_key = Box::new(MaybeUninit::uninit());
        let mut seed = MaybeUninit::<[u8; SEED_BYTES]>::uninit();

        unsafe {
            // Safety: the two buffer arguments are sized correctly and
            // always fully written. `private_key` is sized correctly via
            // the type system.
            bssl_sys::MLKEM768_generate_key(
                public_key.as_mut_ptr() as *mut u8,
                seed.as_mut_ptr() as *mut u8,
                private_key.as_mut_ptr(),
            );

            (
                public_key.assume_init().into(),
                Self(private_key.assume_init()),
                seed.assume_init(),
            )
        }
    }

    /// Regenerate a private key from a seed value.
    pub fn from_seed(seed: &[u8; SEED_BYTES]) -> Self {
        Self(unsafe {
            initialized_boxed_struct(|priv_key| {
                // Safety: `priv_key` is correctly sized by the type system and
                // is always fully written.
                let ok = bssl_sys::MLKEM768_private_key_from_seed(
                    priv_key,
                    seed.as_ffi_ptr(),
                    seed.len(),
                );
                // Since the seed value has the correct length, this function can
                // never fail.
                assert_eq!(ok, 1);
            })
        })
    }

    /// Derives the public key corresponding to this private key.
    pub fn to_public_key(&self) -> PublicKey768 {
        PublicKey768(unsafe {
            initialized_boxed_struct(|pub_key| {
                bssl_sys::MLKEM768_public_from_private(pub_key, &*self.0);
            })
        })
    }

    /// Decapsulates a shared secret from a ciphertext. This function only
    /// returns `None` if ciphertext is the wrong length. For invalid
    /// ciphertexts it returns a key that will always be the same for the
    /// same `ciphertext` and private key, but which appears to be random
    /// unless one has access to the private key. These alternatives occur in
    /// constant time. Any subsequent symmetric encryption using the result
    /// must use an authenticated encryption scheme in order to discover the
    /// decapsulation failure.
    pub fn decapsulate(&self, ciphertext: &[u8]) -> Option<[u8; SHARED_SECRET_BYTES]> {
        unsafe {
            with_output_array_fallible(|out, _| {
                // Safety: `out` is the correct size via the type system and is
                // always fully written if the return value is one.
                bssl_sys::MLKEM768_decap(out, ciphertext.as_ffi_ptr(), ciphertext.len(), &*self.0)
                    == 1
            })
        }
    }
}

impl PublicKey1024 {
    /// Parse a public key from NIST's defined format.
    pub fn parse(encoded: &[u8]) -> Option<Self> {
        let mut cbs = as_cbs(encoded);
        unsafe {
            initialized_boxed_struct_fallible(|pub_key| {
                // Safety: `pub_key` is the correct size via the type system and
                // is fully written if this function returns 1.
                bssl_sys::MLKEM1024_parse_public_key(pub_key, &mut cbs) == 1 && cbs.len == 0
            })
        }
        .map(Self)
    }

    /// Return the serialization of this public key.
    pub fn to_bytes(&self) -> Vec<u8> {
        unsafe {
            cbb_to_vec(PUBLIC_KEY_BYTES_1024, |cbb| {
                let ok = bssl_sys::MLKEM1024_marshal_public_key(cbb, &*self.0);
                // `MLKEM1024_marshal_public_key` only fails if it cannot
                // allocate memory, but `cbb_to_vec` handles allocation.
                assert_eq!(ok, 1);
            })
        }
    }

    /// Generate a secret key and encrypt it to this public key, returning the
    /// ciphertext and the shared secret key.
    pub fn encapsulate(&self) -> (Vec<u8>, [u8; SHARED_SECRET_BYTES]) {
        let mut ciphertext = Box::new_uninit_slice(CIPHERTEXT_BYTES_1024);
        let mut shared_secret = MaybeUninit::<[u8; SHARED_SECRET_BYTES]>::uninit();

        unsafe {
            // Safety: the two buffer arguments are sized correctly and
            // always fully written.
            bssl_sys::MLKEM1024_encap(
                ciphertext.as_mut_ptr() as *mut u8,
                shared_secret.as_mut_ptr() as *mut u8,
                &*self.0,
            );
            (ciphertext.assume_init().into(), shared_secret.assume_init())
        }
    }
}

impl PrivateKey1024 {
    /// Generates a random public/private key pair returning a serialized public
    /// key, a private key, and a private seed value that can be used to
    /// regenerate the same private key in the future.
    pub fn generate() -> (Vec<u8>, PrivateKey1024, [u8; SEED_BYTES]) {
        let mut public_key = Box::new_uninit_slice(PUBLIC_KEY_BYTES_1024);
        let mut private_key = Box::new(MaybeUninit::uninit());
        let mut seed = MaybeUninit::<[u8; SEED_BYTES]>::uninit();

        unsafe {
            // Safety: the two buffer arguments are sized correctly and
            // always fully written. `private_key` is sized correctly via
            // the type system.
            bssl_sys::MLKEM1024_generate_key(
                public_key.as_mut_ptr() as *mut u8,
                seed.as_mut_ptr() as *mut u8,
                private_key.as_mut_ptr(),
            );

            (
                public_key.assume_init().into(),
                Self(private_key.assume_init()),
                seed.assume_init(),
            )
        }
    }

    /// Regenerate a private key from a seed value.
    pub fn from_seed(seed: &[u8; SEED_BYTES]) -> Self {
        Self(unsafe {
            initialized_boxed_struct(|priv_key| {
                // Safety: `priv_key` is correctly sized by the type system and
                // is always fully written.
                let ok = bssl_sys::MLKEM1024_private_key_from_seed(
                    priv_key,
                    seed.as_ffi_ptr(),
                    seed.len(),
                );
                // Since the seed value has the correct length, this function can
                // never fail.
                assert_eq!(ok, 1);
            })
        })
    }

    /// Derives the public key corresponding to this private key.
    pub fn to_public_key(&self) -> PublicKey1024 {
        PublicKey1024(unsafe {
            initialized_boxed_struct(|pub_key| {
                bssl_sys::MLKEM1024_public_from_private(pub_key, &*self.0);
            })
        })
    }

    /// Decapsulates a shared secret from a ciphertext. This function only
    /// returns `None` if ciphertext is the wrong length. For invalid
    /// ciphertexts it returns a key that will always be the same for the
    /// same `ciphertext` and private key, but which appears to be random
    /// unless one has access to the private key. These alternatives occur in
    /// constant time. Any subsequent symmetric encryption using the result
    /// must use an authenticated encryption scheme in order to discover the
    /// decapsulation failure.
    pub fn decapsulate(&self, ciphertext: &[u8]) -> Option<[u8; SHARED_SECRET_BYTES]> {
        unsafe {
            with_output_array_fallible(|out, _| {
                // Safety: `out` is the correct size via the type system and is
                // always fully written if the return value is one.
                bssl_sys::MLKEM1024_decap(out, ciphertext.as_ffi_ptr(), ciphertext.len(), &*self.0)
                    == 1
            })
        }
    }
}

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn basic_768() {
        let (serialized_public_key, _private_key, private_seed) = PrivateKey768::generate();
        let public_key = PublicKey768::parse(&serialized_public_key).unwrap();
        let (ciphertext, shared_key) = public_key.encapsulate();
        let private_key2 = PrivateKey768::from_seed(&private_seed);
        let shared_key2 = private_key2.decapsulate(&ciphertext).unwrap();
        assert_eq!(shared_key, shared_key2);
    }

    #[test]
    fn basic_1024() {
        let (serialized_public_key, _private_key, private_seed) = PrivateKey1024::generate();
        let public_key = PublicKey1024::parse(&serialized_public_key).unwrap();
        let (ciphertext, shared_key) = public_key.encapsulate();
        let private_key2 = PrivateKey1024::from_seed(&private_seed);
        let shared_key2 = private_key2.decapsulate(&ciphertext).unwrap();
        assert_eq!(shared_key, shared_key2);
    }

    #[test]
    fn wrong_length_ciphertext() {
        let (_serialized_public_key, private_key, _private_seed) = PrivateKey768::generate();
        assert!(matches!(private_key.decapsulate(&[0u8, 1, 2, 3]), None));

        let (_serialized_public_key, private_key, _private_seed) = PrivateKey1024::generate();
        assert!(matches!(private_key.decapsulate(&[0u8, 1, 2, 3]), None));
    }

    #[test]
    fn wrong_length_public_key() {
        assert!(matches!(PublicKey768::parse(&[0u8, 1, 2, 3]), None));
        assert!(matches!(PublicKey1024::parse(&[0u8, 1, 2, 3]), None));
    }

    #[test]
    fn marshal_public_key_768() {
        let (serialized_public_key, private_key, _) = PrivateKey768::generate();
        let public_key = PublicKey768::parse(&serialized_public_key).unwrap();
        assert_eq!(serialized_public_key, public_key.to_bytes());
        assert_eq!(
            serialized_public_key,
            private_key.to_public_key().to_bytes()
        );
    }

    #[test]
    fn marshal_public_key_1024() {
        let (serialized_public_key, private_key, _) = PrivateKey1024::generate();
        let public_key = PublicKey1024::parse(&serialized_public_key).unwrap();
        assert_eq!(serialized_public_key, public_key.to_bytes());
        assert_eq!(
            serialized_public_key,
            private_key.to_public_key().to_bytes()
        );
    }
}
