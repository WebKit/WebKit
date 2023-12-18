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

extern crate alloc;

use crate::cipher::{
    BlockCipher, Cipher, CipherError, CipherInitPurpose, EvpAes128Cbc, EvpAes256Cbc,
};
use alloc::vec::Vec;

/// AES-CBC-128 Cipher implementation.
pub struct Aes128Cbc(Cipher<EvpAes128Cbc>);

impl BlockCipher for Aes128Cbc {
    type Key = [u8; 16];
    type Nonce = [u8; 16];

    fn new_encrypt(key: &Self::Key, nonce: &Self::Nonce) -> Self {
        Self(Cipher::new(key, nonce, CipherInitPurpose::Encrypt))
    }

    fn new_decrypt(key: &Self::Key, nonce: &Self::Nonce) -> Self {
        Self(Cipher::new(key, nonce, CipherInitPurpose::Decrypt))
    }

    fn encrypt_padded(self, buffer: &[u8]) -> Result<Vec<u8>, CipherError> {
        // Note: Padding is enabled because we did not disable it with `EVP_CIPHER_CTX_set_padding`
        self.0.encrypt(buffer)
    }

    fn decrypt_padded(self, buffer: &[u8]) -> Result<Vec<u8>, CipherError> {
        // Note: Padding is enabled because we did not disable it with `EVP_CIPHER_CTX_set_padding`
        self.0.decrypt(buffer)
    }
}

/// AES-CBC-256 Cipher implementation.
pub struct Aes256Cbc(Cipher<EvpAes256Cbc>);

impl BlockCipher for Aes256Cbc {
    type Key = [u8; 32];
    type Nonce = [u8; 16];

    fn new_encrypt(key: &Self::Key, nonce: &Self::Nonce) -> Self {
        Self(Cipher::new(key, nonce, CipherInitPurpose::Encrypt))
    }

    fn new_decrypt(key: &Self::Key, nonce: &Self::Nonce) -> Self {
        Self(Cipher::new(key, nonce, CipherInitPurpose::Decrypt))
    }

    fn encrypt_padded(self, buffer: &[u8]) -> Result<Vec<u8>, CipherError> {
        // Note: Padding is enabled because we did not disable it with `EVP_CIPHER_CTX_set_padding`
        self.0.encrypt(buffer)
    }

    fn decrypt_padded(self, buffer: &[u8]) -> Result<Vec<u8>, CipherError> {
        // Note: Padding is enabled because we did not disable it with `EVP_CIPHER_CTX_set_padding`
        self.0.decrypt(buffer)
    }
}

#[allow(clippy::expect_used)]
#[cfg(test)]
mod test {
    use super::*;
    use crate::test_helpers::decode_hex;

    #[test]
    fn aes_128_cbc_test_encrypt() {
        // https://github.com/google/wycheproof/blob/master/testvectors/aes_cbc_pkcs5_test.json#L30
        // tcId: 2
        let iv = decode_hex("c9ee3cd746bf208c65ca9e72a266d54f");
        let key = decode_hex("e09eaa5a3f5e56d279d5e7a03373f6ea");

        let cipher = Aes128Cbc::new_encrypt(&key, &iv);
        let msg: [u8; 16] = decode_hex("ef4eab37181f98423e53e947e7050fd0");

        let output = cipher.encrypt_padded(&msg).expect("Failed to encrypt");

        let expected_ciphertext: [u8; 32] =
            decode_hex("d1fa697f3e2e04d64f1a0da203813ca5bc226a0b1d42287b2a5b994a66eaf14a");
        assert_eq!(expected_ciphertext, &output[..]);
    }

    #[test]
    fn aes_128_cbc_test_encrypt_more_than_one_block() {
        // https://github.com/google/wycheproof/blob/master/testvectors/aes_cbc_pkcs5_test.json#L210
        // tcId: 20
        let iv = decode_hex("54f2459e40e002763144f4752cde2fb5");
        let key = decode_hex("831e664c9e3f0c3094c0b27b9d908eb2");

        let cipher = Aes128Cbc::new_encrypt(&key, &iv);
        let msg: [u8; 17] = decode_hex("26603bb76dd0a0180791c4ed4d3b058807");

        let output = cipher.encrypt_padded(&msg).expect("Failed to encrypt");

        let expected_ciphertext: [u8; 32] =
            decode_hex("8d55dc10584e243f55d2bdbb5758b7fabcd58c8d3785f01c7e3640b2a1dadcd9");
        assert_eq!(expected_ciphertext, &output[..]);
    }

    #[test]
    fn aes_128_cbc_test_decrypt() {
        // https://github.com/google/wycheproof/blob/master/testvectors/aes_cbc_pkcs5_test.json#L30
        // tcId: 2
        let key = decode_hex("e09eaa5a3f5e56d279d5e7a03373f6ea");
        let iv = decode_hex("c9ee3cd746bf208c65ca9e72a266d54f");
        let cipher = Aes128Cbc::new_decrypt(&key, &iv);
        let ciphertext: [u8; 32] =
            decode_hex("d1fa697f3e2e04d64f1a0da203813ca5bc226a0b1d42287b2a5b994a66eaf14a");
        let decrypted = cipher
            .decrypt_padded(&ciphertext)
            .expect("Failed to decrypt");
        let expected_plaintext: [u8; 16] = decode_hex("ef4eab37181f98423e53e947e7050fd0");
        assert_eq!(expected_plaintext, &decrypted[..]);
    }

    #[test]
    fn aes_128_cbc_test_decrypt_empty_message() {
        // https://github.com/google/wycheproof/blob/master/testvectors/aes_cbc_pkcs5_test.json#L20
        // tcId: 1
        let key = decode_hex("e34f15c7bd819930fe9d66e0c166e61c");
        let iv = decode_hex("da9520f7d3520277035173299388bee2");
        let cipher = Aes128Cbc::new_decrypt(&key, &iv);
        let ciphertext: [u8; 16] = decode_hex("b10ab60153276941361000414aed0a9d");
        let decrypted = cipher
            .decrypt_padded(&ciphertext)
            .expect("Failed to decrypt");
        let expected_plaintext: [u8; 0] = decode_hex("");
        assert_eq!(expected_plaintext, &decrypted[..]);
    }

    #[test]
    pub fn aes_256_cbc_test_encrypt() {
        // https://github.com/google/wycheproof/blob/master/testvectors/aes_cbc_pkcs5_test.json#L1412
        // tcId: 124
        let iv = decode_hex("9ec7b863ac845cad5e4673da21f5b6a9");
        let key = decode_hex("612e837843ceae7f61d49625faa7e7494f9253e20cb3adcea686512b043936cd");

        let cipher = Aes256Cbc::new_encrypt(&key, &iv);
        let msg: [u8; 16] = decode_hex("cc37fae15f745a2f40e2c8b192f2b38d");

        let output = cipher.encrypt_padded(&msg).expect("Failed to encrypt");

        let expected_ciphertext: [u8; 32] =
            decode_hex("299295be47e9f5441fe83a7a811c4aeb2650333e681e69fa6b767d28a6ccf282");
        assert_eq!(expected_ciphertext, &output[..]);
    }

    #[test]
    pub fn aes_256_cbc_test_encrypt_more_than_one_block() {
        // https://github.com/google/wycheproof/blob/master/testvectors/aes_cbc_pkcs5_test.json#L1582C24-L1582C24
        // tcId: 141
        let iv = decode_hex("4b74bd981ea9d074757c3e2ef515e5fb");
        let key = decode_hex("73216fafd0022d0d6ee27198b2272578fa8f04dd9f44467fbb6437aa45641bf7");

        let cipher = Aes256Cbc::new_encrypt(&key, &iv);
        let msg: [u8; 17] = decode_hex("d5247b8f6c3edcbfb1d591d13ece23d2f5");

        let output = cipher.encrypt_padded(&msg).expect("Failed to encrypt");

        let expected_ciphertext: [u8; 32] =
            decode_hex("fbea776fb1653635f88e2937ed2450ba4e9063e96d7cdba04928f01cb85492fe");
        assert_eq!(expected_ciphertext, &output[..]);
    }

    #[test]
    fn aes_256_cbc_test_decrypt() {
        // https://github.com/google/wycheproof/blob/master/testvectors/aes_cbc_pkcs5_test.json#L1452
        // tcId: 128
        let key = decode_hex("ea3b016bdd387dd64d837c71683808f335dbdc53598a4ea8c5f952473fafaf5f");
        let iv = decode_hex("fae3e2054113f6b3b904aadbfe59655c");
        let cipher = Aes256Cbc::new_decrypt(&key, &iv);
        let ciphertext: [u8; 16] = decode_hex("b90c326b72eb222ddb4dae47f2bc223c");
        let decrypted = cipher
            .decrypt_padded(&ciphertext)
            .expect("Failed to decrypt");
        let expected_plaintext: [u8; 2] = decode_hex("6601");
        assert_eq!(expected_plaintext, &decrypted[..]);
    }
}
