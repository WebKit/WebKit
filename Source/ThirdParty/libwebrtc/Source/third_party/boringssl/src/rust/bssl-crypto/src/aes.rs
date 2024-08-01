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

//! Advanced Encryption Standard.
//!
//! AES is a 128-bit block cipher that supports key sizes of 128, 192, or 256
//! bits. (Although 192 isn't supported here.)
//!
//! Each key defines a permutation of the set of 128-bit blocks and AES can
//! perform the forward and reverse permutation. (These directions are
//! arbitrarily labeled "encryption" and "decryption".)
//!
//! AES requires relatively expensive preprocessing of keys and thus the
//! processed form of the key is represented here using [`EncryptKey`] and
//! [`DecryptKey`].
//!
//! ```
//! use bssl_crypto::aes;
//!
//! let key_bytes = bssl_crypto::rand_array();
//! let enc_key = aes::EncryptKey::new_256(&key_bytes);
//! let block = [0u8; aes::BLOCK_SIZE];
//! let mut transformed_block = enc_key.encrypt(&block);
//!
//! let dec_key = aes::DecryptKey::new_256(&key_bytes);
//! dec_key.decrypt_in_place(&mut transformed_block);
//! assert_eq!(block, transformed_block);
//! ```
//!
//! AES is a low-level primitive and must be used in a more complex construction
//! in nearly every case. See the `aead` crate for usable encryption and
//! decryption primitives.

use crate::{initialized_struct_fallible, FfiMutSlice, FfiSlice};
use core::ffi::c_uint;

/// AES block size in bytes.
pub const BLOCK_SIZE: usize = bssl_sys::AES_BLOCK_SIZE as usize;

/// A single AES block.
pub type Block = [u8; BLOCK_SIZE];

/// An initialized key which can be used for encrypting.
pub struct EncryptKey(bssl_sys::AES_KEY);

impl EncryptKey {
    /// Initializes an encryption key from an appropriately sized array of bytes
    // for AES-128 operations.
    pub fn new_128(key: &[u8; 16]) -> Self {
        new_encrypt_key(key.as_slice())
    }

    /// Initializes an encryption key from an appropriately sized array of bytes
    // for AES-256 operations.
    pub fn new_256(key: &[u8; 32]) -> Self {
        new_encrypt_key(key.as_slice())
    }

    /// Return the encrypted version of the given block.
    pub fn encrypt(&self, block: &Block) -> Block {
        let mut ret = *block;
        self.encrypt_in_place(&mut ret);
        ret
    }

    /// Replace `block` with its encrypted version.
    pub fn encrypt_in_place(&self, block: &mut Block) {
        // Safety:
        // - block is always a valid size and key is guaranteed to already be initialized.
        unsafe { bssl_sys::AES_encrypt(block.as_ffi_ptr(), block.as_mut_ffi_ptr(), &self.0) }
    }
}

/// An initialized key which can be used for decrypting
pub struct DecryptKey(bssl_sys::AES_KEY);

impl DecryptKey {
    /// Initializes a decryption key from an appropriately sized array of bytes for AES-128 operations.
    pub fn new_128(key: &[u8; 16]) -> DecryptKey {
        new_decrypt_key(key.as_slice())
    }

    /// Initializes a decryption key from an appropriately sized array of bytes for AES-256 operations.
    pub fn new_256(key: &[u8; 32]) -> DecryptKey {
        new_decrypt_key(key.as_slice())
    }

    /// Return the decrypted version of the given block.
    pub fn decrypt(&self, block: &Block) -> Block {
        let mut ret = *block;
        self.decrypt_in_place(&mut ret);
        ret
    }

    /// Replace `block` with its decrypted version.
    pub fn decrypt_in_place(&self, block: &mut Block) {
        // Safety:
        // - block is always a valid size and key is guaranteed to already be initialized.
        unsafe { bssl_sys::AES_decrypt(block.as_ffi_ptr(), block.as_mut_ffi_ptr(), &self.0) }
    }
}

/// This should only be publicly exposed by wrapper types with the correct key lengths
#[allow(clippy::unwrap_used)]
fn new_encrypt_key(key: &[u8]) -> EncryptKey {
    EncryptKey(
        unsafe {
            initialized_struct_fallible(|aes_key| {
                // The return value of this function differs from the usual BoringSSL
                // convention.
                bssl_sys::AES_set_encrypt_key(key.as_ffi_ptr(), key.len() as c_uint * 8, aes_key)
                    == 0
            })
        }
        // unwrap: this function only fails if `key` is the wrong length, which
        // must be prevented by the pub functions that call this.
        .unwrap(),
    )
}

/// This should only be publicly exposed by wrapper types with the correct key lengths.
#[allow(clippy::unwrap_used)]
fn new_decrypt_key(key: &[u8]) -> DecryptKey {
    DecryptKey(
        unsafe {
            initialized_struct_fallible(|aes_key| {
                // The return value of this function differs from the usual BoringSSL
                // convention.
                bssl_sys::AES_set_decrypt_key(key.as_ffi_ptr(), key.len() as c_uint * 8, aes_key)
                    == 0
            })
        }
        // unwrap: this function only fails if `key` is the wrong length, which
        // must be prevented by the pub functions that call this.
        .unwrap(),
    )
}

#[cfg(test)]
mod tests {
    use crate::{
        aes::{DecryptKey, EncryptKey},
        test_helpers::decode_hex,
    };

    #[test]
    fn aes_128() {
        // test data from https://nvlpubs.nist.gov/nistpubs/Legacy/SP/nistspecialpublication800-38a.pdf F.1.1
        let key = decode_hex("2b7e151628aed2a6abf7158809cf4f3c");
        let plaintext = decode_hex("6bc1bee22e409f96e93d7e117393172a");
        let ciphertext = decode_hex("3ad77bb40d7a3660a89ecaf32466ef97");
        assert_eq!(ciphertext, EncryptKey::new_128(&key).encrypt(&plaintext));
        assert_eq!(plaintext, DecryptKey::new_128(&key).decrypt(&ciphertext));
    }

    #[test]
    fn aes_256() {
        // test data from https://nvlpubs.nist.gov/nistpubs/Legacy/SP/nistspecialpublication800-38a.pdf F.1.5
        let key = decode_hex("603deb1015ca71be2b73aef0857d77811f352c073b6108d72d9810a30914dff4");
        let plaintext = decode_hex("6bc1bee22e409f96e93d7e117393172a");
        let ciphertext = decode_hex("f3eed1bdb5d2a03c064b5a7e3db181f8");
        assert_eq!(ciphertext, EncryptKey::new_256(&key).encrypt(&plaintext));
        assert_eq!(plaintext, DecryptKey::new_256(&key).decrypt(&ciphertext));
    }
}
