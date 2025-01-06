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

use crate::cipher::{
    Cipher, CipherError, CipherInitPurpose, EvpAes128Ctr, EvpAes256Ctr, StreamCipher,
};

/// AES-CTR-128 Cipher implementation.
pub struct Aes128Ctr(Cipher<EvpAes128Ctr>);

impl StreamCipher for Aes128Ctr {
    type Key = [u8; 16];
    type Nonce = [u8; 16];

    /// Creates a new AES-128-CTR cipher instance from key material.
    fn new(key: &Self::Key, nonce: &Self::Nonce) -> Self {
        Self(Cipher::new(key, nonce, CipherInitPurpose::Encrypt))
    }

    /// Applies the keystream in-place, advancing the counter state appropriately.
    fn apply_keystream(&mut self, buffer: &mut [u8]) -> Result<(), CipherError> {
        self.0.apply_keystream_in_place(buffer)
    }
}

/// AES-CTR-256 Cipher implementation.
pub struct Aes256Ctr(Cipher<EvpAes256Ctr>);

impl StreamCipher for Aes256Ctr {
    type Key = [u8; 32];
    type Nonce = [u8; 16];

    /// Creates a new AES-256-CTR cipher instance from key material.
    fn new(key: &Self::Key, nonce: &Self::Nonce) -> Self {
        Self(Cipher::new(key, nonce, CipherInitPurpose::Encrypt))
    }

    /// Applies the keystream in-place, advancing the counter state appropriately.
    fn apply_keystream(&mut self, buffer: &mut [u8]) -> Result<(), CipherError> {
        self.0.apply_keystream_in_place(buffer)
    }
}

#[cfg(test)]
mod test {
    use super::*;
    use crate::test_helpers::decode_hex;

    #[test]
    fn aes_128_ctr_test_encrypt() {
        // https://nvlpubs.nist.gov/nistpubs/Legacy/SP/nistspecialpublication800-38a.pdf F.5.1
        let iv = decode_hex("f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff");
        let key = decode_hex("2b7e151628aed2a6abf7158809cf4f3c");

        let mut cipher = Aes128Ctr::new(&key, &iv);
        let mut block: [u8; 16];
        block = decode_hex("6bc1bee22e409f96e93d7e117393172a");

        cipher
            .apply_keystream(&mut block)
            .expect("Failed to apply keystream");

        let expected_ciphertext_1 = decode_hex("874d6191b620e3261bef6864990db6ce");
        assert_eq!(expected_ciphertext_1, block);

        block = decode_hex("ae2d8a571e03ac9c9eb76fac45af8e51");
        cipher
            .apply_keystream(&mut block)
            .expect("Failed to apply keystream");
        let expected_ciphertext_2 = decode_hex("9806f66b7970fdff8617187bb9fffdff");
        assert_eq!(expected_ciphertext_2, block);

        block = decode_hex("30c81c46a35ce411e5fbc1191a0a52ef");
        cipher
            .apply_keystream(&mut block)
            .expect("Failed to apply keystream");
        let expected_ciphertext_3 = decode_hex("5ae4df3edbd5d35e5b4f09020db03eab");
        assert_eq!(expected_ciphertext_3, block);

        block = decode_hex("f69f2445df4f9b17ad2b417be66c3710");
        cipher
            .apply_keystream(&mut block)
            .expect("Failed to apply keystream");
        let expected_ciphertext_3 = decode_hex("1e031dda2fbe03d1792170a0f3009cee");
        assert_eq!(expected_ciphertext_3, block);
    }

    #[test]
    fn aes_128_ctr_test_decrypt() {
        // https://nvlpubs.nist.gov/nistpubs/Legacy/SP/nistspecialpublication800-38a.pdf F.5.2
        let key = decode_hex("2b7e151628aed2a6abf7158809cf4f3c");
        let iv = decode_hex("f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff");
        let mut cipher = Aes128Ctr::new(&key, &iv);

        let mut block: [u8; 16];
        block = decode_hex("874d6191b620e3261bef6864990db6ce");
        cipher
            .apply_keystream(&mut block)
            .expect("Failed to apply keystream");
        let expected_plaintext_1 = decode_hex("6bc1bee22e409f96e93d7e117393172a");
        assert_eq!(expected_plaintext_1, block);

        block = decode_hex("9806f66b7970fdff8617187bb9fffdff");
        cipher
            .apply_keystream(&mut block)
            .expect("Failed to apply keystream");
        let expected_plaintext_2 = decode_hex("ae2d8a571e03ac9c9eb76fac45af8e51");
        assert_eq!(expected_plaintext_2, block);

        block = decode_hex("5ae4df3edbd5d35e5b4f09020db03eab");
        cipher
            .apply_keystream(&mut block)
            .expect("Failed to apply keystream");
        let expected_plaintext_3 = decode_hex("30c81c46a35ce411e5fbc1191a0a52ef");
        assert_eq!(expected_plaintext_3, block);

        block = decode_hex("1e031dda2fbe03d1792170a0f3009cee");
        cipher
            .apply_keystream(&mut block)
            .expect("Failed to apply keystream");
        let expected_plaintext_3 = decode_hex("f69f2445df4f9b17ad2b417be66c3710");
        assert_eq!(expected_plaintext_3, block);
    }

    #[test]
    pub fn aes_256_ctr_test_encrypt() {
        // https://nvlpubs.nist.gov/nistpubs/Legacy/SP/nistspecialpublication800-38a.pdf F.5.5
        let key = decode_hex("603deb1015ca71be2b73aef0857d77811f352c073b6108d72d9810a30914dff4");
        let iv = decode_hex("f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff");
        let mut block: [u8; 16];
        let mut cipher = Aes256Ctr::new(&key, &iv);

        block = decode_hex("6bc1bee22e409f96e93d7e117393172a");
        cipher
            .apply_keystream(&mut block)
            .expect("Failed to apply keystream");
        let expected_ciphertext_1 = decode_hex("601ec313775789a5b7a7f504bbf3d228");
        assert_eq!(expected_ciphertext_1, block);

        block = decode_hex("ae2d8a571e03ac9c9eb76fac45af8e51");
        cipher
            .apply_keystream(&mut block)
            .expect("Failed to apply keystream");
        let expected_ciphertext_2 = decode_hex("f443e3ca4d62b59aca84e990cacaf5c5");
        assert_eq!(expected_ciphertext_2, block);

        block = decode_hex("30c81c46a35ce411e5fbc1191a0a52ef");
        cipher
            .apply_keystream(&mut block)
            .expect("Failed to apply keystream");
        let expected_ciphertext_3 = decode_hex("2b0930daa23de94ce87017ba2d84988d");
        assert_eq!(expected_ciphertext_3, block);

        block = decode_hex("f69f2445df4f9b17ad2b417be66c3710");
        cipher
            .apply_keystream(&mut block)
            .expect("Failed to apply keystream");
        let expected_ciphertext_3 = decode_hex("dfc9c58db67aada613c2dd08457941a6");
        assert_eq!(expected_ciphertext_3, block);
    }

    #[test]
    fn aes_256_ctr_test_decrypt() {
        // https://nvlpubs.nist.gov/nistpubs/Legacy/SP/nistspecialpublication800-38a.pdf F.5.6
        let key = decode_hex("603deb1015ca71be2b73aef0857d77811f352c073b6108d72d9810a30914dff4");
        let iv = decode_hex("f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff");
        let mut cipher = Aes256Ctr::new(&key, &iv);

        let mut block: [u8; 16];
        block = decode_hex("601ec313775789a5b7a7f504bbf3d228");
        cipher
            .apply_keystream(&mut block)
            .expect("Failed to apply keystream");
        let expected_plaintext_1 = decode_hex("6bc1bee22e409f96e93d7e117393172a");
        assert_eq!(expected_plaintext_1, block);

        block = decode_hex("f443e3ca4d62b59aca84e990cacaf5c5");
        cipher
            .apply_keystream(&mut block)
            .expect("Failed to apply keystream");
        let expected_plaintext_2 = decode_hex("ae2d8a571e03ac9c9eb76fac45af8e51");
        assert_eq!(expected_plaintext_2, block);

        block = decode_hex("2b0930daa23de94ce87017ba2d84988d");
        cipher
            .apply_keystream(&mut block)
            .expect("Failed to apply keystream");
        let expected_plaintext_3 = decode_hex("30c81c46a35ce411e5fbc1191a0a52ef");
        assert_eq!(expected_plaintext_3, block);

        block = decode_hex("dfc9c58db67aada613c2dd08457941a6");
        cipher
            .apply_keystream(&mut block)
            .expect("Failed to apply keystream");
        let expected_plaintext_3 = decode_hex("f69f2445df4f9b17ad2b417be66c3710");
        assert_eq!(expected_plaintext_3, block);
    }
}
