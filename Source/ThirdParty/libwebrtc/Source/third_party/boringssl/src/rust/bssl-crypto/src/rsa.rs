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

//! RSA signatures.
//!
//! New protocols should not use RSA, but it's still often found in existing
//! protocols. This module implements PKCS#1 signatures (the most common type).
//!
//! Creating a signature:
//!
//! ```
//! use bssl_crypto::{digest, rsa};
//! # use bssl_crypto::rsa::TEST_PKCS8_BYTES;
//!
//! // Generating an RSA private key is slow, so this examples parses it from
//! // PKCS#8 DER.
//! let private_key = rsa::PrivateKey::from_der_private_key_info(TEST_PKCS8_BYTES).unwrap();
//! let signed_msg = b"hello world";
//! let sig = private_key.sign_pkcs1::<digest::Sha256>(signed_msg);
//! ```
//!
//! To verify a signature, publish your _public_ key:
//!
//! ```
//! # use bssl_crypto::{rsa};
//! # use bssl_crypto::rsa::TEST_PKCS8_BYTES;
//! # let private_key = rsa::PrivateKey::from_der_private_key_info(TEST_PKCS8_BYTES).unwrap();
//! let public_key_bytes = private_key.as_public().to_der_subject_public_key_info();
//! ```
//!
//! Then verify the signature from above with it:
//!
//! ```
//! # use bssl_crypto::{digest, rsa};
//! # use bssl_crypto::rsa::TEST_PKCS8_BYTES;
//! # let private_key = rsa::PrivateKey::from_der_private_key_info(TEST_PKCS8_BYTES).unwrap();
//! # let signed_msg = b"hello world";
//! # let mut sig = private_key.sign_pkcs1::<digest::Sha256>(signed_msg);
//! # let public_key_bytes = private_key.as_public().to_der_subject_public_key_info();
//! let public_key = rsa::PublicKey::from_der_subject_public_key_info(public_key_bytes.as_ref())
//!    .unwrap();
//! assert!(public_key.verify_pkcs1::<digest::Sha256>(signed_msg, sig.as_slice()).is_ok());
//! sig[0] ^= 1;
//! assert!(public_key.verify_pkcs1::<digest::Sha256>(signed_msg, sig.as_slice()).is_err());
//! ```

use crate::{
    cbb_to_buffer, digest, parse_with_cbs, scoped, sealed, with_output_vec, Buffer, FfiSlice,
    ForeignTypeRef, InvalidSignatureError,
};
use alloc::vec::Vec;
use core::ptr::null_mut;

/// An RSA public key.
pub struct PublicKey(*mut bssl_sys::RSA);

impl PublicKey {
    /// Parse a DER-encoded RSAPublicKey structure (from RFC 8017).
    pub fn from_der_rsa_public_key(der: &[u8]) -> Option<Self> {
        Some(PublicKey(parse_with_cbs(
            der,
            // Safety: `ptr` is a non-null result from `RSA_parse_public_key` here.
            |ptr| unsafe { bssl_sys::RSA_free(ptr) },
            // Safety: cbs is valid per `parse_with_cbs`.
            |cbs| unsafe { bssl_sys::RSA_parse_public_key(cbs) },
        )?))
    }

    /// Serialize to a DER-encoded RSAPublicKey structure (from RFC 8017).
    pub fn to_der_rsa_public_key(&self) -> Buffer {
        cbb_to_buffer(/*initial_capacity=*/ 300, |cbb| unsafe {
            // Safety: `self.0` is valid by construction.
            assert_eq!(1, bssl_sys::RSA_marshal_public_key(cbb, self.0))
        })
    }

    /// Parse a DER-encoded SubjectPublicKeyInfo. This format is found in,
    /// for example, X.509 certificates.
    pub fn from_der_subject_public_key_info(spki: &[u8]) -> Option<Self> {
        let mut pkey = scoped::EvpPkey::from_ptr(parse_with_cbs(
            spki,
            // Safety: `pkey` is a non-null result from `EVP_parse_public_key` here.
            |pkey| unsafe { bssl_sys::EVP_PKEY_free(pkey) },
            // Safety: cbs is valid per `parse_with_cbs`.
            |cbs| unsafe { bssl_sys::EVP_parse_public_key(cbs) },
        )?);
        let rsa = unsafe { bssl_sys::EVP_PKEY_get1_RSA(pkey.as_ffi_ptr()) };
        if !rsa.is_null() {
            // Safety: `EVP_PKEY_get1_RSA` adds a reference so we are not
            // stealing ownership from `pkey`.
            Some(PublicKey(rsa))
        } else {
            None
        }
    }

    /// Serialize to a DER-encoded SubjectPublicKeyInfo. This format is found
    /// in, for example, X.509 certificates.
    pub fn to_der_subject_public_key_info(&self) -> Buffer {
        let mut pkey = scoped::EvpPkey::new();
        // Safety: this takes a reference to `self.0` and so doesn't steal ownership.
        assert_eq!(1, unsafe {
            bssl_sys::EVP_PKEY_set1_RSA(pkey.as_ffi_ptr(), self.0)
        });
        cbb_to_buffer(384, |cbb| unsafe {
            // The arguments are valid so this will only fail if out of memory,
            // which this crate doesn't handle.
            assert_eq!(1, bssl_sys::EVP_marshal_public_key(cbb, pkey.as_ffi_ptr()));
        })
    }

    /// Verify that `signature` is a valid signature of a digest of
    /// `signed_msg`, by this public key. The digest of the message will be
    /// computed with the specified hash function.
    pub fn verify_pkcs1<Hash: digest::Algorithm>(
        &self,
        signed_msg: &[u8],
        signature: &[u8],
    ) -> Result<(), InvalidSignatureError> {
        let digest = Hash::hash_to_vec(signed_msg);
        // Safety: `get_md` always returns a valid pointer.
        let hash_nid = unsafe { bssl_sys::EVP_MD_nid(Hash::get_md(sealed::Sealed).as_ptr()) };
        let result = unsafe {
            // Safety: all buffers are valid and `self.0` is valid by construction.
            bssl_sys::RSA_verify(
                hash_nid,
                digest.as_slice().as_ffi_ptr(),
                digest.len(),
                signature.as_ffi_ptr(),
                signature.len(),
                self.0,
            )
        };
        if result == 1 {
            Ok(())
        } else {
            Err(InvalidSignatureError)
        }
    }
}

// Safety:
//
// An `RSA` is safe to use from multiple threads so long as no mutating
// operations are performed. (Reference count changes don't count as mutating.)
// No mutating operations are performed. `RSA_verify` takes a non-const pointer
// but the BoringSSL docs specifically say that "these functions are considered
// non-mutating for thread-safety purposes and may be used concurrently."
unsafe impl Sync for PublicKey {}
unsafe impl Send for PublicKey {}

impl Drop for PublicKey {
    fn drop(&mut self) {
        // Safety: this object owns `self.0`.
        unsafe { bssl_sys::RSA_free(self.0) }
    }
}

/// The set of supported RSA key sizes for key generation.
#[allow(missing_docs)]
pub enum KeySize {
    Rsa2048 = 2048,
    Rsa3072 = 3072,
    Rsa4096 = 4096,
}

/// An RSA private key.
pub struct PrivateKey(*mut bssl_sys::RSA);

impl PrivateKey {
    /// Generate a fresh RSA private key of the given size.
    pub fn generate(size: KeySize) -> Self {
        let e = scoped::Bignum::from_u64(bssl_sys::RSA_F4 as u64);
        let ptr = unsafe { bssl_sys::RSA_new() };
        assert!(!ptr.is_null());

        let result = unsafe {
            // Safety: `rsa` and `e` are valid and initialized, just above.
            bssl_sys::RSA_generate_key_ex(ptr, size as core::ffi::c_int, e.as_ffi_ptr(), null_mut())
        };
        assert_eq!(1, result);
        // Safety: this function owns `ptr` and thus can move ownership here.
        Self(ptr)
    }

    /// Parse a DER-encoded RSAPrivateKey structure (from RFC 8017).
    pub fn from_der_rsa_private_key(der: &[u8]) -> Option<Self> {
        Some(PrivateKey(parse_with_cbs(
            der,
            // Safety: `ptr` is a non-null result from `RSA_parse_private_key` here.
            |ptr| unsafe { bssl_sys::RSA_free(ptr) },
            // Safety: `cbs` is valid per `parse_with_cbs`.
            |cbs| unsafe { bssl_sys::RSA_parse_private_key(cbs) },
        )?))
    }

    /// Serialize to a DER-encoded RSAPrivateKey structure (from RFC 8017).
    pub fn to_der_rsa_private_key(&self) -> Buffer {
        cbb_to_buffer(/*initial_capacity=*/ 512, |cbb| unsafe {
            // Safety: `self.0` is valid by construction.
            assert_eq!(1, bssl_sys::RSA_marshal_private_key(cbb, self.0))
        })
    }

    /// Parse a DER-encrypted PrivateKeyInfo struct (from RFC 5208). This is often called "PKCS#8 format".
    pub fn from_der_private_key_info(der: &[u8]) -> Option<Self> {
        let mut pkey = scoped::EvpPkey::from_ptr(parse_with_cbs(
            der,
            // Safety: `ptr` is a non-null result from `EVP_parse_private_key` here.
            |pkey| unsafe { bssl_sys::EVP_PKEY_free(pkey) },
            // Safety: `cbs` is valid per `parse_with_cbs`.
            |cbs| unsafe { bssl_sys::EVP_parse_private_key(cbs) },
        )?);
        // Safety: `pkey` is valid and was created just above.
        let rsa = unsafe { bssl_sys::EVP_PKEY_get1_RSA(pkey.as_ffi_ptr()) };
        if rsa.is_null() {
            return None;
        }
        Some(Self(rsa))
    }

    /// Serialize to a DER-encrypted PrivateKeyInfo struct (from RFC 5208). This is often called "PKCS#8 format".
    pub fn to_der_private_key_info(&self) -> Buffer {
        let mut pkey = scoped::EvpPkey::new();
        assert_eq!(1, unsafe {
            // Safety: `pkey` was just constructed. This takes a reference and
            // so doesn't steal ownership from `self`.
            bssl_sys::EVP_PKEY_set1_RSA(pkey.as_ffi_ptr(), self.0)
        });
        unsafe {
            cbb_to_buffer(/*initial_capacity=*/ 384, |cbb| {
                // Safety: `pkey` is valid and owned by this function.
                assert_eq!(1, bssl_sys::EVP_marshal_private_key(cbb, pkey.as_ffi_ptr()));
            })
        }
    }

    /// Compute the signature of the digest of `to_be_signed` with PKCS#1 using
    /// this private key. The specified hash function is used to compute the
    //  digest.
    pub fn sign_pkcs1<Hash: digest::Algorithm>(&self, to_be_signed: &[u8]) -> Vec<u8> {
        let digest = Hash::hash_to_vec(to_be_signed);
        // Safety: `get_md` always returns a valid pointer.
        let hash_nid = unsafe { bssl_sys::EVP_MD_nid(Hash::get_md(sealed::Sealed).as_ptr()) };
        let max_output = unsafe { bssl_sys::RSA_size(self.0) } as usize;

        unsafe {
            with_output_vec(max_output, |out_buf| {
                let mut out_len: core::ffi::c_uint = 0;
                // Safety: `out_buf` points to at least `RSA_size` bytes, as
                // required. `self.0` is valid by construction.
                let result = bssl_sys::RSA_sign(
                    hash_nid,
                    digest.as_slice().as_ffi_ptr(),
                    digest.len(),
                    out_buf,
                    &mut out_len,
                    self.0,
                );
                // `RSA_sign` should always be successful unless it's out of
                // memory, which this crate doesn't handle.
                assert_eq!(1, result);
                let out_len = out_len as usize;
                assert!(out_len <= max_output);
                // Safety: `out_len` bytes have been written.
                out_len
            })
        }
    }

    /// Return the public key corresponding to this private key.
    pub fn as_public(&self) -> PublicKey {
        // Safety: `self.0` is valid by construction and `RSA_up_ref` means
        // we we can pass an ownership reference to `PublicKey`.
        unsafe { bssl_sys::RSA_up_ref(self.0) };
        PublicKey(self.0)
    }
}

// Safety:
//
// An `RSA` is safe to use from multiple threads so long as no mutating
// operations are performed. (Reference count changes don't count as mutating.)
// No mutating operations are performed. `RSA_sign` takes a non-const pointer
// but the BoringSSL docs specifically say that "these functions are considered
// non-mutating for thread-safety purposes and may be used concurrently."
unsafe impl Sync for PrivateKey {}
unsafe impl Send for PrivateKey {}

impl Drop for PrivateKey {
    fn drop(&mut self) {
        // Safety: `self.0` is always owned by this struct.
        unsafe { bssl_sys::RSA_free(self.0) }
    }
}

#[cfg(test)]
mod test {
    use super::*;
    use crate::digest;

    #[test]
    fn sign_and_verify() {
        let key = PrivateKey::from_der_private_key_info(TEST_PKCS8_BYTES).unwrap();
        let signed_msg = b"hello world";
        let sig = key.sign_pkcs1::<digest::Sha256>(signed_msg);
        assert!(key
            .as_public()
            .verify_pkcs1::<digest::Sha256>(signed_msg, &sig)
            .is_ok());
    }
}

// RSA generation is slow, but serialized keys are large. In order to use this
// in doctests, it's included here and public, but undocumented.
#[doc(hidden)]
pub const TEST_PKCS8_BYTES: &[u8] = b"\x30\x82\x04\xbd\x02\x01\x00\x30\x0d\x06\x09\x2a\x86\x48\x86\xf7\x0d\x01\x01\x01\x05\x00\x04\x82\x04\xa7\x30\x82\x04\xa3\x02\x01\x00\x02\x82\x01\x01\x00\x98\x3f\xf5\xc4\x89\xb7\x6f\x12\xc8\xb5\x82\xaa\x98\xe6\x75\x39\xc4\x44\x46\xa0\x45\x62\x42\x43\x21\x81\xa0\x53\x17\x47\xb3\xdc\xfc\x3b\x76\x03\xd6\xd4\xce\x5e\x9d\x22\xe5\xa3\x59\xa2\x47\x0c\xe4\x82\x33\x7a\x21\xa5\x61\x2a\x77\xa2\x6b\xfa\xa3\x45\x41\x50\xc2\xf7\x0d\xe1\xa6\x3a\x83\x5b\xe6\xb8\x1f\x24\x1e\x24\x89\xf8\x8d\xde\x5f\xf1\x50\x27\x0f\x2b\xbe\x58\xaa\x64\x67\xef\x22\x57\x1e\xf4\x3f\x2e\xba\x4b\x2f\xc3\x5e\x67\xcc\xc3\xf6\xdd\x6b\x31\x58\xb9\xbd\x7b\xf9\x23\xac\xf2\xa9\xb6\x8f\x88\x75\x0f\x73\xdf\xd2\x14\xaa\x41\x28\x5c\x9a\xd6\xc4\xab\x6f\xb0\x53\xb9\x0a\x2c\xfb\x56\x6e\x56\x94\xaa\x1a\x25\x29\x3b\x01\x0c\x7e\x44\x1b\xe1\x76\x12\x73\xc4\x16\x62\x64\x3d\xe6\xf7\x9f\x69\x3f\xc9\x3b\x75\xd6\x80\xee\x87\x68\x83\xde\x2d\x18\xe4\x26\xdd\x1a\x02\xd8\xd2\x1d\xb6\xf1\x71\xf5\x63\x62\x0c\xd7\x35\x21\xc6\x75\xb4\xd5\x0f\x89\x08\x17\x13\x24\x07\xc2\x7c\x73\xe2\x17\x00\x12\x8a\xc9\x39\xdb\xf0\xc8\x6f\x1f\xf7\x99\xed\x8c\x67\x9c\xf2\x30\x5c\xd0\xd0\x0d\xc1\x15\x07\xa3\x1d\xf5\xd4\x92\x82\xfd\x9c\x5a\x11\x69\x3b\x02\x03\x01\x00\x01\x02\x82\x01\x00\x44\xe1\x5a\xfd\x8a\x18\xd5\x45\xb8\x4c\x76\x4b\x5c\x55\x97\x5f\x85\x2e\x26\x8d\xc8\x16\x46\x48\x3c\xd6\x7a\x84\x5d\x19\xf1\x83\xdf\x11\xbf\xb8\xc8\xef\x0a\x56\xbf\xdc\xd3\xeb\xed\x57\x7f\xb1\x93\x88\x5c\x65\xba\xe7\x29\x68\x9f\x2b\x7a\x92\xb0\x5f\x5a\xc7\x81\x0d\x68\xd8\x57\xee\x4d\x13\xbc\xf4\x3c\x12\x89\x18\x9a\xdb\x3a\xc4\x0a\xc0\x10\x35\x3b\xa5\xdc\xbe\x1c\x88\xc4\x84\xea\x12\x64\x4c\xb8\x71\x19\x93\x7e\x8e\x73\x1d\x9f\x04\x61\xa1\x97\x27\x82\x2e\xb6\x4d\x6a\x4f\xfb\xa4\xe5\xa7\x54\x94\xb5\xf1\x41\xc8\xa4\x3d\xa1\xe6\x4a\xf0\xdb\xbb\xc2\x91\x26\x9a\x0f\xbf\xdd\x57\x1e\x83\x5c\x9a\x7b\x28\x53\x1d\x2d\x44\x91\x1f\x02\x81\x7b\x6f\xb5\xf7\x48\x7d\xa0\x12\x22\xdb\xbf\xd9\x04\x17\xe4\x97\xf2\xac\x32\xf8\x70\xfa\x75\xe3\x5a\xb0\xef\x1f\x2d\x24\xb9\x26\x83\x33\xe7\x3c\x3c\xfb\x0b\xd8\x70\x33\x76\xb1\x1c\x1d\x38\x06\x0a\xdb\xbd\xd2\x34\x5e\xe6\xb1\x6f\x5d\x8f\x18\xac\x94\xd2\x0d\xee\x39\x0b\xa3\xb4\xcf\xf1\xe1\x91\x30\xcb\xce\xa5\x2f\xa9\xcc\x4f\xee\xe4\xdd\xee\x8a\x77\x0e\xd1\xbd\xcc\xb0\x11\x55\x15\x5e\x99\xf1\x02\x81\x81\x00\xd1\x75\x33\xe4\x31\xc2\xfc\x09\x6c\xf6\x04\x97\xc7\xa3\xb1\x88\x36\x26\xd8\x4e\x86\x2d\xb8\x99\x68\x97\xd8\x0b\xc6\xc3\xe7\x58\x49\xc3\x41\xcd\xcd\x33\x09\xa0\x90\xb2\x77\xfa\xa3\xb6\x71\x09\x33\x43\x0a\x6a\xd8\xc3\x36\xaf\xa9\x11\x54\x64\x77\x82\xf4\xf1\xe0\x12\x5a\xb8\x9f\x5a\x04\xb3\x29\xd4\xc6\xba\x4c\xdc\x04\x97\xfb\xb6\x7e\x1b\x89\x09\x0c\x8a\xb8\x6c\x9f\x2b\x91\x0d\x34\x18\x39\xf3\x38\xf9\xe6\xed\x29\x48\x30\xe4\x3c\x09\x15\x33\xe0\xb8\x2f\xd8\xfa\xf2\x6d\x1f\xf1\xee\x02\xc2\xb4\xf9\xf4\x63\x4b\xa5\x02\x81\x81\x00\xba\x14\x89\xff\x65\xb5\xe6\x52\x45\x23\x37\x5e\x0c\x62\xde\xe9\x7f\xa9\x05\xee\x28\x0d\x91\xb1\x99\xd6\x8b\xf8\x58\x50\x8b\xb1\xee\x57\xbd\x2b\x7b\xf0\x25\x03\xeb\xbc\x87\x73\xc8\xbf\x57\x16\xda\x49\x7a\x79\x82\x25\x99\x46\x9c\xb3\xd2\xd5\xb0\xae\xec\xeb\xbc\xd2\x4b\xae\xd0\x0a\x54\xcd\xad\x44\x90\x74\x79\xa2\x34\x73\x8a\x3a\x6c\x0b\x13\x20\x5d\xa4\xcc\x7b\xb4\x64\xcf\x61\x6e\xdf\xc1\x8c\xd4\x84\x22\xf1\x19\x32\x6d\xf1\x6f\xe1\x1e\xa5\xf6\x20\x6c\xc6\xa8\x9c\x4d\x8d\x59\xdf\x90\x71\x67\x1a\x48\xa3\x4b\x5f\x02\x81\x81\x00\x97\x4d\x8f\x7f\x7e\x86\xb8\x23\x62\xe7\x50\x28\x07\xd9\x72\x4b\xcf\xba\x3d\xb4\x73\x6e\xa1\x93\x87\x9f\x70\x3c\x09\x87\xc8\x1c\xd9\xa3\xc7\x6c\x0f\x97\x97\x93\xba\x12\x81\x62\xb7\x51\xf9\xd3\x48\x89\x5c\x04\x14\xb2\xe7\x54\xfa\xce\xfe\xe4\x58\x04\x6c\x46\x30\xb3\x71\x7f\x3d\xf4\xfb\xc2\x24\x2c\x84\xa5\x5d\x11\xed\xeb\x8f\xb3\xa2\xe2\xe7\x19\x77\x4a\xd9\xaf\xf5\x46\xb6\x50\x10\x5a\x93\xb9\xe3\x65\x79\xef\xc5\x4b\x55\xad\xf8\xc4\x22\xe1\xc7\xa9\xa5\x3e\x9a\xff\xf5\xde\x06\x98\x04\xbc\x7b\x98\xb7\x75\xe6\xd5\x02\x81\x80\x0a\x7f\x38\x1d\xa9\x2e\x2e\xb4\xfb\x63\x76\x2f\x1f\x01\xc0\xd3\x69\x39\x2e\xb5\x75\x9a\xf6\x5a\x0f\x74\x93\xe6\xc9\x8c\x99\xa4\xca\xee\x36\x24\xaa\xd4\x2c\x32\x61\x6c\xfc\x33\x22\xe2\xf0\x55\xc0\xb0\x9e\x71\x16\x4f\x6a\xab\x1a\x11\xe6\xd5\xd9\x26\xb5\x04\xc3\x5d\x15\x99\xe1\xf0\x83\x42\x2b\x01\x10\x29\x11\xe7\x7d\x8f\xfa\xff\x3a\xb3\x11\x3c\x25\x2c\x33\xc0\xd2\xb7\x51\x1f\x8c\xf2\xa0\x67\x82\x61\x85\xdb\x15\xf1\xcb\x53\xf0\x5c\xc1\xae\xd9\x08\x91\x3a\x4f\xae\xa9\x8d\x4c\xc1\x98\xd3\x5c\xde\x95\xb4\x68\x7f\x02\x81\x80\x7d\x3e\x6b\x2c\x16\xe8\x17\x2c\x27\x9c\xc5\xc5\xfb\x30\x1a\xf7\x32\x53\x93\xfe\xc1\xa0\x5d\xac\x7d\x6f\xba\x1b\x56\x7e\x34\xf6\xa7\x91\x1f\x39\x84\x1c\x94\x58\x13\xe2\xb9\xec\xb6\x24\xfe\x76\x35\x1b\xcc\x4f\x8e\x0d\x88\x5b\x5a\x6f\xb6\xa2\x0b\xc3\xb6\x98\x2d\xca\xce\xce\x26\xb4\x36\x37\x42\xa4\xc0\xa9\x85\x57\x4b\x6b\xc2\xed\x14\x96\xe5\xbc\x2b\x83\x32\xe9\x83\x24\x7f\x85\x74\x09\x3c\xfa\x45\xfd\x21\xeb\xd8\xa3\x02\xd2\x70\x0a\x9a\x9d\x7d\xe4\x39\xc4\x59\xc8\x16\x6f\xce\xd5\x1d\xea\x91\x4d\x12\x78\xc3\x30";
