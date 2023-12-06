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

/// Block size in bytes for AES.
pub const BLOCK_SIZE: usize = bssl_sys::AES_BLOCK_SIZE as usize;

/// A single AES block.
pub type AesBlock = [u8; BLOCK_SIZE];

/// AES implementation used for encrypting/decrypting a single `AesBlock` at a time.
pub struct Aes;

impl Aes {
    /// Encrypts `block` in place.
    pub fn encrypt(key: &AesEncryptKey, block: &mut AesBlock) {
        let input = *block;
        // Safety:
        // - AesBlock is always a valid size and key is guaranteed to already be initialized.
        unsafe { bssl_sys::AES_encrypt(input.as_ptr(), block.as_mut_ptr(), &key.0) }
    }

    /// Decrypts `block` in place.
    pub fn decrypt(key: &AesDecryptKey, block: &mut AesBlock) {
        let input = *block;
        // Safety:
        // - AesBlock is always a valid size and key is guaranteed to already be initialized.
        unsafe { bssl_sys::AES_decrypt(input.as_ptr(), block.as_mut_ptr(), &key.0) }
    }
}

/// An initialized key which can be used for encrypting.
pub struct AesEncryptKey(bssl_sys::AES_KEY);

impl AesEncryptKey {
    /// Initializes an encryption key from an appropriately sized array of bytes for AES-128 operations.
    pub fn new_aes_128(key: [u8; 16]) -> AesEncryptKey {
        new_encrypt_key(key)
    }

    /// Initializes an encryption key from an appropriately sized array of bytes for AES-256 operations.
    pub fn new_aes_256(key: [u8; 32]) -> AesEncryptKey {
        new_encrypt_key(key)
    }
}

/// An initialized key which can be used for decrypting
pub struct AesDecryptKey(bssl_sys::AES_KEY);

impl AesDecryptKey {
    /// Initializes a decryption key from an appropriately sized array of bytes for AES-128 operations.
    pub fn new_aes_128(key: [u8; 16]) -> AesDecryptKey {
        new_decrypt_key(key)
    }

    /// Initializes a decryption key from an appropriately sized array of bytes for AES-256 operations.
    pub fn new_aes_256(key: [u8; 32]) -> AesDecryptKey {
        new_decrypt_key(key)
    }
}

/// Private generically implemented function for creating a new `AesEncryptKey` from an array of bytes.
/// This should only be publicly exposed by wrapper types with the correct key lengths
fn new_encrypt_key<const N: usize>(key: [u8; N]) -> AesEncryptKey {
    let mut enc_key_uninit = core::mem::MaybeUninit::uninit();

    // Safety:
    // - key is guaranteed to point to bits/8 bytes determined by the len() * 8 used below.
    // - bits is always a valid AES key size, as defined by the new_aes_* fns defined on the public
    //   key structs.
    let result = unsafe {
        bssl_sys::AES_set_encrypt_key(
            key.as_ptr(),
            key.len() as core::ffi::c_uint * 8,
            enc_key_uninit.as_mut_ptr(),
        )
    };
    assert_eq!(result, 0, "Error occurred in bssl_sys::AES_set_encrypt_key");

    // Safety:
    // - since we have checked above that initialization succeeded, this will never be UB
    let enc_key = unsafe { enc_key_uninit.assume_init() };

    AesEncryptKey(enc_key)
}

/// Private generically implemented function for creating a new `AesDecryptKey` from an array of bytes.
/// This should only be publicly exposed by wrapper types with the correct key lengths.
fn new_decrypt_key<const N: usize>(key: [u8; N]) -> AesDecryptKey {
    let mut dec_key_uninit = core::mem::MaybeUninit::uninit();

    // Safety:
    // - key is guaranteed to point to bits/8 bytes determined by the len() * 8 used below.
    // - bits is always a valid AES key size, as defined by the new_aes_* fns defined on the public
    //   key structs.
    let result = unsafe {
        bssl_sys::AES_set_decrypt_key(
            key.as_ptr(),
            key.len() as core::ffi::c_uint * 8,
            dec_key_uninit.as_mut_ptr(),
        )
    };
    assert_eq!(result, 0, "Error occurred in bssl_sys::AES_set_decrypt_key");

    // Safety:
    // - Since we have checked above that initialization succeeded, this will never be UB.
    let dec_key = unsafe { dec_key_uninit.assume_init() };

    AesDecryptKey(dec_key)
}

#[cfg(test)]
mod tests {
    use crate::aes::{Aes, AesDecryptKey, AesEncryptKey};
    use crate::test_helpers::decode_hex;

    // test data from https://nvlpubs.nist.gov/nistpubs/Legacy/SP/nistspecialpublication800-38a.pdf F.1.1
    #[test]
    fn aes_128_test_encrypt() {
        let key = AesEncryptKey::new_aes_128(decode_hex("2b7e151628aed2a6abf7158809cf4f3c"));
        let mut block = [0_u8; 16];

        block.copy_from_slice(&decode_hex::<16>("6bc1bee22e409f96e93d7e117393172a"));
        Aes::encrypt(&key, &mut block);
        assert_eq!(decode_hex("3ad77bb40d7a3660a89ecaf32466ef97"), block);

        block.copy_from_slice(&decode_hex::<16>("ae2d8a571e03ac9c9eb76fac45af8e51"));
        Aes::encrypt(&key, &mut block);
        assert_eq!(decode_hex("f5d3d58503b9699de785895a96fdbaaf"), block);

        block.copy_from_slice(&decode_hex::<16>("30c81c46a35ce411e5fbc1191a0a52ef"));
        Aes::encrypt(&key, &mut block);
        assert_eq!(decode_hex("43b1cd7f598ece23881b00e3ed030688"), block);

        block.copy_from_slice(&decode_hex::<16>("f69f2445df4f9b17ad2b417be66c3710"));
        Aes::encrypt(&key, &mut block);
        assert_eq!(decode_hex("7b0c785e27e8ad3f8223207104725dd4"), block);
    }

    // test data from https://nvlpubs.nist.gov/nistpubs/Legacy/SP/nistspecialpublication800-38a.pdf F.1.2
    #[test]
    fn aes_128_test_decrypt() {
        let key = AesDecryptKey::new_aes_128(decode_hex("2b7e151628aed2a6abf7158809cf4f3c"));
        let mut block = [0_u8; 16];

        block.copy_from_slice(&decode_hex::<16>("3ad77bb40d7a3660a89ecaf32466ef97"));
        Aes::decrypt(&key, &mut block);
        assert_eq!(decode_hex::<16>("6bc1bee22e409f96e93d7e117393172a"), block);

        block.copy_from_slice(&decode_hex::<16>("f5d3d58503b9699de785895a96fdbaaf"));
        Aes::decrypt(&key, &mut block);
        assert_eq!(decode_hex::<16>("ae2d8a571e03ac9c9eb76fac45af8e51"), block);

        block.copy_from_slice(&decode_hex::<16>("43b1cd7f598ece23881b00e3ed030688"));
        Aes::decrypt(&key, &mut block);
        assert_eq!(decode_hex::<16>("30c81c46a35ce411e5fbc1191a0a52ef"), block);

        block.copy_from_slice(&decode_hex::<16>("7b0c785e27e8ad3f8223207104725dd4").as_slice());
        Aes::decrypt(&key, &mut block);
        assert_eq!(decode_hex::<16>("f69f2445df4f9b17ad2b417be66c3710"), block);
    }

    // test data from https://nvlpubs.nist.gov/nistpubs/Legacy/SP/nistspecialpublication800-38a.pdf F.1.5
    #[test]
    pub fn aes_256_test_encrypt() {
        let key = AesEncryptKey::new_aes_256(decode_hex(
            "603deb1015ca71be2b73aef0857d77811f352c073b6108d72d9810a30914dff4",
        ));
        let mut block: [u8; 16];

        block = decode_hex("6bc1bee22e409f96e93d7e117393172a");
        Aes::encrypt(&key, &mut block);
        assert_eq!(decode_hex("f3eed1bdb5d2a03c064b5a7e3db181f8"), block);

        block = decode_hex("ae2d8a571e03ac9c9eb76fac45af8e51");
        Aes::encrypt(&key, &mut block);
        assert_eq!(decode_hex("591ccb10d410ed26dc5ba74a31362870"), block);

        block = decode_hex("30c81c46a35ce411e5fbc1191a0a52ef");
        Aes::encrypt(&key, &mut block);
        assert_eq!(decode_hex("b6ed21b99ca6f4f9f153e7b1beafed1d"), block);

        block = decode_hex("f69f2445df4f9b17ad2b417be66c3710");
        Aes::encrypt(&key, &mut block);
        assert_eq!(decode_hex("23304b7a39f9f3ff067d8d8f9e24ecc7"), block);
    }

    // test data from https://nvlpubs.nist.gov/nistpubs/Legacy/SP/nistspecialpublication800-38a.pdf F.1.6
    #[test]
    fn aes_256_test_decrypt() {
        let key = AesDecryptKey::new_aes_256(decode_hex(
            "603deb1015ca71be2b73aef0857d77811f352c073b6108d72d9810a30914dff4",
        ));

        let mut block: [u8; 16];

        block = decode_hex("f3eed1bdb5d2a03c064b5a7e3db181f8");
        Aes::decrypt(&key, &mut block);
        assert_eq!(decode_hex("6bc1bee22e409f96e93d7e117393172a"), block);

        block = decode_hex("591ccb10d410ed26dc5ba74a31362870");
        Aes::decrypt(&key, &mut block);
        assert_eq!(decode_hex("ae2d8a571e03ac9c9eb76fac45af8e51"), block);

        block = decode_hex("b6ed21b99ca6f4f9f153e7b1beafed1d");
        Aes::decrypt(&key, &mut block);
        assert_eq!(decode_hex("30c81c46a35ce411e5fbc1191a0a52ef"), block);

        block = decode_hex("23304b7a39f9f3ff067d8d8f9e24ecc7");
        Aes::decrypt(&key, &mut block);
        assert_eq!(decode_hex("f69f2445df4f9b17ad2b417be66c3710"), block);
    }
}
