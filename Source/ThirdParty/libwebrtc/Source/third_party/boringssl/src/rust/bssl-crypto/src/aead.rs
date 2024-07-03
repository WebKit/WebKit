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

//! Authenticated Encryption with Additional Data.
//!
//! AEAD couples confidentiality and integrity in a single primitive. AEAD
//! algorithms take a key and then can seal and open individual messages. Each
//! message has a unique, per-message nonce and, optionally, additional data
//! which is authenticated but not included in the ciphertext.
//!
//! No two distinct plaintexts must ever be sealed using the same (key, nonce)
//! pair. It is up to the user of these algorithms to ensure this. For example,
//! when encrypting a stream of messages (e.g. over a TCP socket) a message
//! counter can provide distinct nonces as long as the key is randomly generated
//! for the specific connection and is distinct in each direction.
//!
//! To implement that example:
//!
//! ```
//! use bssl_crypto::aead::{Aead, Aes256Gcm};
//!
//! let key = bssl_crypto::rand_array();
//! let aead = Aes256Gcm::new(&key);
//!
//! let mut message_counter: u64 = 0;
//! let mut nonce = bssl_crypto::rand_array();
//! nonce[4..].copy_from_slice(message_counter.to_be_bytes().as_slice());
//! message_counter += 1;
//! let plaintext = b"message";
//! let ciphertext = aead.seal(&nonce, plaintext, b"");
//!
//! let decrypted = aead.open(&nonce, ciphertext.as_slice(), b"");
//! assert_eq!(plaintext, decrypted.unwrap().as_slice());
//! ```

use crate::{with_output_array, with_output_vec, with_output_vec_fallible, FfiMutSlice, FfiSlice};
use alloc::vec::Vec;

/// The error type returned when a fallible, in-place operation fails.
#[derive(Debug)]
pub struct InvalidCiphertext;

/// Authenticated Encryption with Associated Data (AEAD) algorithm trait.
pub trait Aead {
    /// The type of tags produced by this AEAD. Generally a u8 array of fixed
    /// length.
    type Tag: AsRef<[u8]>;

    /// The type of nonces used by this AEAD. Generally a u8 array of fixed
    /// length.
    type Nonce: AsRef<[u8]>;

    /// Encrypt and authenticate `plaintext`, and authenticate `ad`, returning
    /// the result as a freshly allocated [`Vec`]. The `nonce` must never
    /// be used in any sealing operation with the same key, ever again.
    fn seal(&self, nonce: &Self::Nonce, plaintext: &[u8], ad: &[u8]) -> Vec<u8>;

    /// Encrypt and authenticate `plaintext`, and authenticate `ad`, writing
    /// the ciphertext over `plaintext` and additionally returning the calculated
    /// tag, which is usually appended to the ciphertext. The `nonce` must never
    /// be used in any sealing operation with the same key, ever again.
    fn seal_in_place(&self, nonce: &Self::Nonce, plaintext: &mut [u8], ad: &[u8]) -> Self::Tag;

    /// Authenticate `ciphertext` and `ad` and, if valid, decrypt `ciphertext`,
    /// returning the original plaintext in a newly allocated [`Vec`]. The `nonce`
    /// must be the same value as given to the sealing operation that produced
    /// `ciphertext`.
    fn open(&self, nonce: &Self::Nonce, ciphertext: &[u8], ad: &[u8]) -> Option<Vec<u8>>;

    /// Authenticate `ciphertext` and `ad` using `tag` and, if valid, decrypt
    /// `ciphertext` in place. The `nonce` must be the same value as given to
    /// the sealing operation that produced `ciphertext`.
    fn open_in_place(
        &self,
        nonce: &Self::Nonce,
        ciphertext: &mut [u8],
        tag: &Self::Tag,
        ad: &[u8],
    ) -> Result<(), InvalidCiphertext>;
}

/// AES-128 in Galois Counter Mode.
pub struct Aes128Gcm(EvpAead<16, 12, 16>);
aead_algo!(Aes128Gcm, EVP_aead_aes_128_gcm, 16, 12, 16);

/// AES-256 in Galois Counter Mode.
pub struct Aes256Gcm(EvpAead<32, 12, 16>);
aead_algo!(Aes256Gcm, EVP_aead_aes_256_gcm, 32, 12, 16);

/// AES-128 in GCM-SIV mode (which is different from SIV mode!).
pub struct Aes128GcmSiv(EvpAead<16, 12, 16>);
aead_algo!(Aes128GcmSiv, EVP_aead_aes_128_gcm_siv, 16, 12, 16);

/// AES-256 in GCM-SIV mode (which is different from SIV mode!).
pub struct Aes256GcmSiv(EvpAead<32, 12, 16>);
aead_algo!(Aes256GcmSiv, EVP_aead_aes_256_gcm_siv, 32, 12, 16);

/// The AEAD built from ChaCha20 and Poly1305 as described in <https://datatracker.ietf.org/doc/html/rfc8439>.
pub struct Chacha20Poly1305(EvpAead<32, 12, 16>);
aead_algo!(Chacha20Poly1305, EVP_aead_chacha20_poly1305, 32, 12, 16);

/// Chacha20Poly1305 with an extended nonce that makes random generation of nonces safe.
pub struct XChacha20Poly1305(EvpAead<32, 24, 16>);
aead_algo!(XChacha20Poly1305, EVP_aead_xchacha20_poly1305, 32, 24, 16);

/// An internal struct that implements AEAD operations given an `EVP_AEAD`.
struct EvpAead<const KEY_LEN: usize, const NONCE_LEN: usize, const TAG_LEN: usize>(
    *mut bssl_sys::EVP_AEAD_CTX,
);

#[allow(clippy::unwrap_used)]
impl<const KEY_LEN: usize, const NONCE_LEN: usize, const TAG_LEN: usize>
    EvpAead<KEY_LEN, NONCE_LEN, TAG_LEN>
{
    // Tagged unsafe because `evp_aead` must be valid.
    unsafe fn new(key: &[u8; KEY_LEN], evp_aead: *const bssl_sys::EVP_AEAD) -> Self {
        // `evp_aead` is assumed to be valid. The function will validate
        // the other lengths and return NULL on error. In that case we
        // crash the address space because that should never happen.
        let ptr =
            unsafe { bssl_sys::EVP_AEAD_CTX_new(evp_aead, key.as_ffi_ptr(), key.len(), TAG_LEN) };
        assert!(!ptr.is_null());
        Self(ptr)
    }

    fn seal(&self, nonce: &[u8; NONCE_LEN], plaintext: &[u8], ad: &[u8]) -> Vec<u8> {
        let max_output = plaintext.len() + TAG_LEN;
        unsafe {
            with_output_vec(max_output, |out_buf| {
                let mut out_len = 0usize;
                // Safety: the input buffers are all valid, with corresponding
                // ptr and length. The output buffer has at least `max_output`
                // bytes of space and that maximum is passed to
                // `EVP_AEAD_CTX_seal` as a limit.
                let result = bssl_sys::EVP_AEAD_CTX_seal(
                    self.0,
                    out_buf,
                    &mut out_len,
                    max_output,
                    nonce.as_ffi_ptr(),
                    nonce.len(),
                    plaintext.as_ffi_ptr(),
                    plaintext.len(),
                    ad.as_ffi_ptr(),
                    ad.len(),
                );
                // Sealing never fails unless there's a programmer error.
                assert_eq!(result, 1);
                // For the implemented AEADs, we should always have calculated
                // the overhead exactly.
                assert_eq!(out_len, max_output);
                // Safety: `out_len` bytes have been written to.
                out_len
            })
        }
    }

    fn seal_in_place(
        &self,
        nonce: &[u8; NONCE_LEN],
        plaintext: &mut [u8],
        ad: &[u8],
    ) -> [u8; TAG_LEN] {
        // Safety: the buffers are all valid, with corresponding ptr and length.
        // `tag_len` is passed at the maximum size of `tag` and `out_tag_len`
        // is checked to ensure that the whole output was written to.
        unsafe {
            with_output_array(|tag, tag_len| {
                let mut out_tag_len = 0usize;
                let result = bssl_sys::EVP_AEAD_CTX_seal_scatter(
                    self.0,
                    plaintext.as_mut_ffi_ptr(),
                    tag,
                    &mut out_tag_len,
                    tag_len,
                    nonce.as_ffi_ptr(),
                    nonce.len(),
                    plaintext.as_ffi_ptr(),
                    plaintext.len(),
                    /*extra_in=*/ core::ptr::null(),
                    /*extra_in_len=*/ 0,
                    ad.as_ffi_ptr(),
                    ad.len(),
                );
                // Failure indicates that one of the configured lengths was wrong.
                // Crashing is a good answer in that case.
                assert_eq!(result, 1);
                // The whole output must have been written to.
                assert_eq!(out_tag_len, TAG_LEN);
            })
        }
    }

    fn open(&self, nonce: &[u8; NONCE_LEN], ciphertext: &[u8], ad: &[u8]) -> Option<Vec<u8>> {
        if ciphertext.len() < TAG_LEN {
            return None;
        }
        let max_output = ciphertext.len() - TAG_LEN;

        unsafe {
            with_output_vec_fallible(max_output, |out_buf| {
                let mut out_len = 0usize;
                // Safety: the input buffers are all valid, with corresponding
                // ptr and length. The output buffer has at least `max_output`
                // bytes of space and that maximum is passed to
                // `EVP_AEAD_CTX_open` as a limit.
                let result = bssl_sys::EVP_AEAD_CTX_open(
                    self.0,
                    out_buf,
                    &mut out_len,
                    max_output,
                    nonce.as_ffi_ptr(),
                    nonce.len(),
                    ciphertext.as_ffi_ptr(),
                    ciphertext.len(),
                    ad.as_ffi_ptr(),
                    ad.len(),
                );
                if result == 1 {
                    // Safety: `out_len` bytes have been written to.
                    Some(out_len)
                } else {
                    None
                }
            })
        }
    }

    fn open_in_place(
        &self,
        nonce: &[u8; NONCE_LEN],
        ciphertext: &mut [u8],
        tag: &[u8; TAG_LEN],
        ad: &[u8],
    ) -> Result<(), InvalidCiphertext> {
        // Safety:
        // - The buffers are all valid, with corresponding ptr and length
        let result = unsafe {
            bssl_sys::EVP_AEAD_CTX_open_gather(
                self.0,
                ciphertext.as_mut_ffi_ptr(),
                nonce.as_ffi_ptr(),
                nonce.len(),
                ciphertext.as_ffi_ptr(),
                ciphertext.len(),
                tag.as_ffi_ptr(),
                tag.len(),
                ad.as_ffi_ptr(),
                ad.len(),
            )
        };
        if result == 1 {
            Ok(())
        } else {
            Err(InvalidCiphertext)
        }
    }
}

impl<const KEY_LEN: usize, const NONCE_LEN: usize, const TAG_LEN: usize> Drop
    for EvpAead<KEY_LEN, NONCE_LEN, TAG_LEN>
{
    fn drop(&mut self) {
        // Safety: `self.0` was initialized by `EVP_AEAD_CTX_init` because all
        // paths to create an `EvpAead` do so.
        unsafe { bssl_sys::EVP_AEAD_CTX_free(self.0) }
    }
}

#[cfg(test)]
mod test {
    use super::*;
    use crate::test_helpers::{decode_hex, decode_hex_into_vec};

    fn check_aead_invariants<
        const NONCE_LEN: usize,
        const TAG_LEN: usize,
        A: Aead<Nonce = [u8; NONCE_LEN], Tag = [u8; TAG_LEN]>,
    >(
        aead: A,
    ) {
        let plaintext = b"plaintext";
        let ad = b"additional data";
        let nonce: A::Nonce = [0u8; NONCE_LEN];

        let mut ciphertext = aead.seal(&nonce, plaintext, ad);
        let plaintext2 = aead
            .open(&nonce, ciphertext.as_slice(), ad)
            .expect("should decrypt");
        assert_eq!(plaintext, plaintext2.as_slice());

        ciphertext[0] ^= 1;
        assert!(aead.open(&nonce, ciphertext.as_slice(), ad).is_none());
        ciphertext[0] ^= 1;

        let (ciphertext_in_place, tag_slice) =
            ciphertext.as_mut_slice().split_at_mut(plaintext.len());
        let tag: [u8; TAG_LEN] = tag_slice.try_into().unwrap();
        aead.open_in_place(&nonce, ciphertext_in_place, &tag, ad)
            .expect("should decrypt");
        assert_eq!(plaintext, ciphertext_in_place);

        let tag = aead.seal_in_place(&nonce, ciphertext_in_place, ad);
        aead.open_in_place(&nonce, ciphertext_in_place, &tag, ad)
            .expect("should decrypt");
        assert_eq!(plaintext, ciphertext_in_place);

        assert!(aead.open(&nonce, b"tooshort", b"").is_none());
    }

    #[test]
    fn aes_128_gcm_invariants() {
        check_aead_invariants(Aes128Gcm::new(&[0u8; 16]));
    }

    #[test]
    fn aes_256_gcm_invariants() {
        check_aead_invariants(Aes256Gcm::new(&[0u8; 32]));
    }

    #[test]
    fn aes_128_gcm_siv_invariants() {
        check_aead_invariants(Aes128GcmSiv::new(&[0u8; 16]));
    }

    #[test]
    fn aes_256_gcm_siv_invariants() {
        check_aead_invariants(Aes256GcmSiv::new(&[0u8; 32]));
    }

    #[test]
    fn chacha20_poly1305_invariants() {
        check_aead_invariants(Chacha20Poly1305::new(&[0u8; 32]));
    }

    #[test]
    fn xchacha20_poly1305_invariants() {
        check_aead_invariants(XChacha20Poly1305::new(&[0u8; 32]));
    }

    struct TestCase<const KEY_LEN: usize, const NONCE_LEN: usize> {
        key: [u8; KEY_LEN],
        nonce: [u8; NONCE_LEN],
        msg: Vec<u8>,
        ad: Vec<u8>,
        ciphertext: Vec<u8>,
    }

    fn check_test_cases<
        const KEY_LEN: usize,
        const NONCE_LEN: usize,
        const TAG_LEN: usize,
        F: Fn(&[u8; KEY_LEN]) -> Box<dyn Aead<Nonce = [u8; NONCE_LEN], Tag = [u8; TAG_LEN]>>,
    >(
        new_func: F,
        test_cases: &[TestCase<KEY_LEN, NONCE_LEN>],
    ) {
        for (test_num, test) in test_cases.iter().enumerate() {
            let ctx = new_func(&test.key);
            let ciphertext = ctx.seal(&test.nonce, test.msg.as_slice(), test.ad.as_slice());
            assert_eq!(ciphertext, test.ciphertext, "Failed on test #{}", test_num);

            let plaintext = ctx
                .open(&test.nonce, ciphertext.as_slice(), test.ad.as_slice())
                .unwrap();
            assert_eq!(plaintext, test.msg, "Decrypt failed on test #{}", test_num);
        }
    }

    #[test]
    fn aes_128_gcm_siv() {
        let test_cases: &[TestCase<16, 12>] = &[
            TestCase {
                // https://github.com/google/wycheproof/blob/master/testvectors/aes_gcm_siv_test.json
                // TC1 - Empty Message
                key: decode_hex("01000000000000000000000000000000"),
                nonce: decode_hex("030000000000000000000000"),
                msg: Vec::new(),
                ad: Vec::new(),
                ciphertext: decode_hex_into_vec("dc20e2d83f25705bb49e439eca56de25"),
            },
            TestCase {
                // TC2
                key: decode_hex("01000000000000000000000000000000"),
                nonce: decode_hex("030000000000000000000000"),
                msg: decode_hex_into_vec("0100000000000000"),
                ad: Vec::new(),
                ciphertext: decode_hex_into_vec("b5d839330ac7b786578782fff6013b815b287c22493a364c"),
            },
            TestCase {
                // TC14
                key: decode_hex("01000000000000000000000000000000"),
                nonce: decode_hex("030000000000000000000000"),
                msg: decode_hex_into_vec("02000000"),
                ad: decode_hex_into_vec("010000000000000000000000"),
                ciphertext: decode_hex_into_vec("a8fe3e8707eb1f84fb28f8cb73de8e99e2f48a14"),
            },
        ];

        check_test_cases(|key| Box::new(Aes128GcmSiv::new(key)), test_cases);
    }

    #[test]
    fn aes_256_gcm_siv() {
        let test_cases: &[TestCase<32, 12>] = &[
            TestCase {
                // https://github.com/google/wycheproof/blob/master/testvectors/aes_gcm_siv_test.json
                // TC77
                key: decode_hex("0100000000000000000000000000000000000000000000000000000000000000"),
                nonce: decode_hex("030000000000000000000000"),
                msg: decode_hex_into_vec("0100000000000000"),
                ad: Vec::new(),
                ciphertext: decode_hex_into_vec("c2ef328e5c71c83b843122130f7364b761e0b97427e3df28"),
            },
            TestCase {
                // TC78
                key: decode_hex("0100000000000000000000000000000000000000000000000000000000000000"),
                nonce: decode_hex("030000000000000000000000"),
                msg: decode_hex_into_vec("010000000000000000000000"),
                ad: Vec::new(),
                ciphertext: decode_hex_into_vec(
                    "9aab2aeb3faa0a34aea8e2b18ca50da9ae6559e48fd10f6e5c9ca17e",
                ),
            },
            TestCase {
                // TC89 contains associated data
                key: decode_hex("0100000000000000000000000000000000000000000000000000000000000000"),
                nonce: decode_hex("030000000000000000000000"),
                msg: decode_hex_into_vec("02000000"),
                ad: decode_hex_into_vec("010000000000000000000000"),
                ciphertext: decode_hex_into_vec("22b3f4cd1835e517741dfddccfa07fa4661b74cf"),
            },
        ];

        check_test_cases(|key| Box::new(Aes256GcmSiv::new(key)), test_cases);
    }

    #[test]
    fn aes_128_gcm() {
        let test_cases: &[TestCase<16, 12>] = &[
            TestCase {
                // TC 1 from crypto/cipher_extra/test/aes_128_gcm_tests.txt
                key: decode_hex("d480429666d48b400633921c5407d1d1"),
                nonce: decode_hex("3388c676dc754acfa66e172a"),
                msg: Vec::new(),
                ad: Vec::new(),
                ciphertext: decode_hex_into_vec("7d7daf44850921a34e636b01adeb104f"),
            },
            TestCase {
                // TC2
                key: decode_hex("3881e7be1bb3bbcaff20bdb78e5d1b67"),
                nonce: decode_hex("dcf5b7ae2d7552e2297fcfa9"),
                msg: decode_hex_into_vec("0a2714aa7d"),
                ad: decode_hex_into_vec("c60c64bbf7"),
                ciphertext: decode_hex_into_vec("5626f96ecbff4c4f1d92b0abb1d0820833d9eb83c7"),
            },
        ];

        check_test_cases(|key| Box::new(Aes128Gcm::new(key)), test_cases);
    }

    #[test]
    fn aes_256_gcm() {
        let test_cases: &[TestCase<32, 12>] = &[
            TestCase {
                // TC 1 from crypto/cipher_extra/test/aes_128_gcm_tests.txt
                key: decode_hex("e5ac4a32c67e425ac4b143c83c6f161312a97d88d634afdf9f4da5bd35223f01"),
                nonce: decode_hex("5bf11a0951f0bfc7ea5c9e58"),
                msg: Vec::new(),
                ad: Vec::new(),
                ciphertext: decode_hex_into_vec("d7cba289d6d19a5af45dc13857016bac"),
            },
            TestCase {
                // TC2
                key: decode_hex("73ad7bbbbc640c845a150f67d058b279849370cd2c1f3c67c4dd6c869213e13a"),
                nonce: decode_hex("a330a184fc245812f4820caa"),
                msg: decode_hex_into_vec("f0535fe211"),
                ad: decode_hex_into_vec("e91428be04"),
                ciphertext: decode_hex_into_vec("e9b8a896da9115ed79f26a030c14947b3e454db9e7"),
            },
        ];

        check_test_cases(|key| Box::new(Aes256Gcm::new(key)), test_cases);
    }
}
