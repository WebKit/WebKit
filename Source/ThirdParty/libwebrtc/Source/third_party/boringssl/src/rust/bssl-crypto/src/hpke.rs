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

//! Hybrid Public Key Encryption
//!
//! HPKE provides public key encryption of arbitrary-length messages. It
//! establishes contexts that produce/consume an ordered sequence of
//! ciphertexts that are both encrypted and authenticated.
//!
//! See RFC 9180 for more details.
//!
//! ```
//! use bssl_crypto::hpke;
//!
//! let kem = hpke::Kem::X25519HkdfSha256;
//! let (pub_key, priv_key) = kem.generate_keypair();
//! // Distribute `pub_key` to people who want to send you messages.
//!
//! // On the sending side...
//! let params = hpke::Params::new(kem, hpke::Kdf::HkdfSha256, hpke::Aead::Aes128Gcm);
//! let info : &[u8] = b"mutual context";
//! let (mut sender_ctx, encapsulated_key) =
//!     hpke::SenderContext::new(&params, &pub_key, info).unwrap();
//! // Transmit the `encapsulated_key` to the receiver, followed by one or
//! // more ciphertexts...
//! let aad = b"associated_data";
//! let plaintext1 : &[u8] = b"plaintext1";
//! let msg1 = sender_ctx.seal(plaintext1, aad);
//! let plaintext2 : &[u8] = b"plaintext2";
//! let msg2 = sender_ctx.seal(plaintext2, aad);
//!
//! // On the receiving side...
//! let mut recipient_ctx = hpke::RecipientContext::new(
//!     &params,
//!     &priv_key,
//!     &encapsulated_key,
//!     info,
//! ).unwrap();
//!
//! let received_plaintext1 = recipient_ctx.open(&msg1, aad).unwrap();
//! assert_eq!(plaintext1, &received_plaintext1);
//! let received_plaintext2 = recipient_ctx.open(&msg2, aad).unwrap();
//! assert_eq!(plaintext2, &received_plaintext2);
//!
//! // Messages must be processed in order, so trying to `open` the second
//! // message first will fail.
//! let mut recipient_ctx = hpke::RecipientContext::new(
//!     &params,
//!     &priv_key,
//!     &encapsulated_key,
//!     info,
//! ).unwrap();
//!
//! let received_plaintext2 = recipient_ctx.open(&msg2, aad);
//! assert!(received_plaintext2.is_none());
//!
//! // There is also an interface for exporting secrets from both sender
//! // and recipient contexts.
//! let sender_export = sender_ctx.export(b"ctx", 32);
//! let recipient_export = recipient_ctx.export(b"ctx", 32);
//! assert_eq!(sender_export, recipient_export);
//! ```

use crate::{scoped, with_output_vec, with_output_vec_fallible, FfiSlice};
use alloc::vec::Vec;

/// Supported KEM algorithms with values detailed in RFC 9180.
#[derive(Clone, Copy)]
pub enum Kem {
    #[allow(missing_docs)]
    X25519HkdfSha256 = 32,
}

impl Kem {
    fn as_ffi_ptr(&self) -> *const bssl_sys::EVP_HPKE_KEM {
        // Safety: this function returns a pointer to static data.
        unsafe {
            match self {
                Kem::X25519HkdfSha256 => bssl_sys::EVP_hpke_x25519_hkdf_sha256(),
            }
        }
    }

    /// Generate a public and private key for this KEM.
    pub fn generate_keypair(&self) -> (Vec<u8>, Vec<u8>) {
        let mut key = scoped::EvpHpkeKey::new();
        // Safety: `key` and `self` must be valid and this function doesn't
        // take ownership of either.
        let ret =
            unsafe { bssl_sys::EVP_HPKE_KEY_generate(key.as_mut_ffi_ptr(), self.as_ffi_ptr()) };
        // Key generation currently never fails, and out-of-memory is not
        // handled by this crate.
        assert_eq!(ret, 1);

        fn get_value_from_key(
            key: &scoped::EvpHpkeKey,
            accessor: unsafe extern "C" fn(
                *const bssl_sys::EVP_HPKE_KEY,
                // Output buffer.
                *mut u8,
                // Number of bytes written.
                *mut usize,
                // Maximum output size.
                usize,
            ) -> core::ffi::c_int,
            max_len: usize,
        ) -> Vec<u8> {
            unsafe {
                with_output_vec(max_len, |out| {
                    let mut out_len = 0usize;
                    let ret = accessor(key.as_ffi_ptr(), out, &mut out_len, max_len);
                    // If `max_len` is correct then these functions never fail.
                    assert_eq!(ret, 1);
                    assert!(out_len <= max_len);
                    // Safety: `out_len` bytes have been written, as required.
                    out_len
                })
            }
        }

        let pub_key = get_value_from_key(
            &key,
            bssl_sys::EVP_HPKE_KEY_public_key,
            bssl_sys::EVP_HPKE_MAX_PUBLIC_KEY_LENGTH as usize,
        );
        let priv_key = get_value_from_key(
            &key,
            bssl_sys::EVP_HPKE_KEY_private_key,
            bssl_sys::EVP_HPKE_MAX_PRIVATE_KEY_LENGTH as usize,
        );
        (pub_key, priv_key)
    }
}

/// Supported KDF algorithms with values detailed in RFC 9180.
#[derive(Clone, Copy)]
pub enum Kdf {
    #[allow(missing_docs)]
    HkdfSha256 = 1,
}

/// Supported AEAD algorithms with values detailed in RFC 9180.
#[derive(Clone, Copy)]
#[allow(missing_docs)]
pub enum Aead {
    Aes128Gcm = 1,
    Aes256Gcm = 2,
    Chacha20Poly1305 = 3,
}

impl Aead {
    fn from_rfc_id(n: u16) -> Option<Aead> {
        let ret = match n {
            1 => Aead::Aes128Gcm,
            2 => Aead::Aes256Gcm,
            3 => Aead::Chacha20Poly1305,
            _ => return None,
        };
        // The mapping above must agree with the values in the enum.
        assert_eq!(n, ret as u16);
        Some(ret)
    }

    fn as_ffi_ptr(&self) -> *const bssl_sys::EVP_HPKE_AEAD {
        // Safety: these functions all return pointers to static data.
        unsafe {
            match self {
                Aead::Aes128Gcm => bssl_sys::EVP_hpke_aes_128_gcm(),
                Aead::Aes256Gcm => bssl_sys::EVP_hpke_aes_256_gcm(),
                Aead::Chacha20Poly1305 => bssl_sys::EVP_hpke_chacha20_poly1305(),
            }
        }
    }
}

/// Maximum length of the encapsulated key for all currently supported KEMs.
const MAX_ENCAPSULATED_KEY_LEN: usize = bssl_sys::EVP_HPKE_MAX_ENC_LENGTH as usize;

/// HPKE parameters, including KEM, KDF, and AEAD.
pub struct Params {
    kem: *const bssl_sys::EVP_HPKE_KEM,
    kdf: *const bssl_sys::EVP_HPKE_KDF,
    aead: *const bssl_sys::EVP_HPKE_AEAD,
}

impl Params {
    /// New `Params` from KEM, KDF, and AEAD enums.
    pub fn new(_kem: Kem, _kdf: Kdf, aead: Aead) -> Self {
        // Safety: EVP_hpke_x25519_hkdf_sha256 and EVP_hpke_hkdf_sha256 just
        // return pointers to static data.
        unsafe {
            Self {
                // Only one KEM and KDF are supported thus far.
                kem: bssl_sys::EVP_hpke_x25519_hkdf_sha256(),
                kdf: bssl_sys::EVP_hpke_hkdf_sha256(),
                aead: aead.as_ffi_ptr(),
            }
        }
    }

    /// New `Params` from KEM, KDF, and AEAD IDs as detailed in RFC 9180.
    pub fn new_from_rfc_ids(kem_id: u16, kdf_id: u16, aead_id: u16) -> Option<Self> {
        let kem = Kem::X25519HkdfSha256;
        let kdf = Kdf::HkdfSha256;
        let aead = Aead::from_rfc_id(aead_id)?;

        if kem_id != kem as u16 || kdf_id != kdf as u16 {
            return None;
        }
        Some(Self::new(kem, kdf, aead))
    }
}

/// HPKE sender context. Callers may use `seal()` to encrypt messages for the recipient.
pub struct SenderContext(scoped::EvpHpkeCtx);

impl SenderContext {
    /// Performs the SetupBaseS HPKE operation and returns a sender context
    /// plus an encapsulated shared secret for `recipient_pub_key`.
    ///
    /// Returns `None` if `recipient_pub_key` is invalid.
    ///
    /// On success, callers may use `seal()` to encrypt messages for the recipient.
    pub fn new(params: &Params, recipient_pub_key: &[u8], info: &[u8]) -> Option<(Self, Vec<u8>)> {
        let mut ctx = scoped::EvpHpkeCtx::new();
        unsafe {
            with_output_vec_fallible(MAX_ENCAPSULATED_KEY_LEN, |enc_key_buf| {
                let mut enc_key_len = 0usize;
                // Safety: EVP_HPKE_CTX_setup_sender
                // - is called with context created from EVP_HPKE_CTX_new,
                // - is called with valid buffers with corresponding pointer and length, and
                // - returns 0 on error.
                let ret = bssl_sys::EVP_HPKE_CTX_setup_sender(
                    ctx.as_mut_ffi_ptr(),
                    enc_key_buf,
                    &mut enc_key_len,
                    MAX_ENCAPSULATED_KEY_LEN,
                    params.kem,
                    params.kdf,
                    params.aead,
                    recipient_pub_key.as_ffi_ptr(),
                    recipient_pub_key.len(),
                    info.as_ffi_ptr(),
                    info.len(),
                );
                if ret == 1 {
                    Some(enc_key_len)
                } else {
                    None
                }
            })
        }
        .map(|enc_key| (Self(ctx), enc_key))
    }

    /// Seal encrypts `plaintext`, and authenticates `aad`, returning the resulting ciphertext.
    ///
    /// Note that HPKE encryption is stateful and ordered. The sender's first call to `seal()` must
    /// correspond to the recipient's first call to `open()`, etc.
    ///
    /// This function panics if adding the `plaintext` length and
    /// `bssl_sys::EVP_HPKE_CTX_max_overhead` overflows.
    pub fn seal(&mut self, plaintext: &[u8], aad: &[u8]) -> Vec<u8> {
        // Safety: EVP_HPKE_CTX_max_overhead panics if ctx is not set up as a sender.
        #[allow(clippy::expect_used)]
        let max_out_len = plaintext
            .len()
            .checked_add(unsafe { bssl_sys::EVP_HPKE_CTX_max_overhead(self.0.as_ffi_ptr()) })
            .expect("Maximum output length calculation overflow");
        unsafe {
            with_output_vec(max_out_len, |out_buf| {
                let mut out_len = 0usize;
                // Safety: EVP_HPKE_CTX_seal
                // - is called with context created from EVP_HPKE_CTX_new and
                // - is called with valid buffers with corresponding pointer and length.
                let result = bssl_sys::EVP_HPKE_CTX_seal(
                    self.0.as_mut_ffi_ptr(),
                    out_buf,
                    &mut out_len,
                    max_out_len,
                    plaintext.as_ffi_ptr(),
                    plaintext.len(),
                    aad.as_ffi_ptr(),
                    aad.len(),
                );
                assert_eq!(result, 1);
                out_len
            })
        }
    }

    /// Exports a secret of length `out_len` from the HPKE context using `context` as the context
    /// string.
    pub fn export(&mut self, context: &[u8], out_len: usize) -> Vec<u8> {
        unsafe {
            with_output_vec(out_len, |out_buf| {
                // Safety: EVP_HPKE_CTX_export
                // - is called with context created from EVP_HPKE_CTX_new,
                // - is called with valid buffers with corresponding pointer and length, and
                // - returns 0 on error, which only occurs when OOM.
                let ret = bssl_sys::EVP_HPKE_CTX_export(
                    self.0.as_mut_ffi_ptr(),
                    out_buf,
                    out_len,
                    context.as_ffi_ptr(),
                    context.len(),
                );
                assert_eq!(ret, 1);
                out_len
            })
        }
    }
}

/// HPKE recipient context. Callers may use `open()` to decrypt messages from the sender.
pub struct RecipientContext(scoped::EvpHpkeCtx);

impl RecipientContext {
    /// New implements the SetupBaseR HPKE operation, which decapsulates the shared secret in
    /// `encapsulated_key` with `recipient_priv_key` and sets up a recipient context. These are
    /// stored and returned in the newly created RecipientContext.
    ///
    /// Note that `encapsulated_key` may be invalid, in which case this function will return an
    /// error.
    ///
    /// On success, callers may use `open()` to decrypt messages from the sender.
    pub fn new(
        params: &Params,
        recipient_priv_key: &[u8],
        encapsulated_key: &[u8],
        info: &[u8],
    ) -> Option<Self> {
        let mut hpke_key = scoped::EvpHpkeKey::new();

        // Safety: EVP_HPKE_KEY_init returns 0 on error.
        let result = unsafe {
            bssl_sys::EVP_HPKE_KEY_init(
                hpke_key.as_mut_ffi_ptr(),
                params.kem,
                recipient_priv_key.as_ffi_ptr(),
                recipient_priv_key.len(),
            )
        };
        if result != 1 {
            return None;
        }

        let mut ctx = scoped::EvpHpkeCtx::new();

        // Safety: EVP_HPKE_CTX_setup_recipient
        // - is called with context created from EVP_HPKE_CTX_new,
        // - is called with HPKE key created from EVP_HPKE_KEY_init,
        // - is called with valid buffers with corresponding pointer and length, and
        // - returns 0 on error.
        let result = unsafe {
            bssl_sys::EVP_HPKE_CTX_setup_recipient(
                ctx.as_mut_ffi_ptr(),
                hpke_key.as_ffi_ptr(),
                params.kdf,
                params.aead,
                encapsulated_key.as_ffi_ptr(),
                encapsulated_key.len(),
                info.as_ffi_ptr(),
                info.len(),
            )
        };
        if result == 1 {
            Some(Self(ctx))
        } else {
            None
        }
    }

    /// Open authenticates `aad` and decrypts `ciphertext`. It returns an error on failure.
    ///
    /// Note that HPKE encryption is stateful and ordered. The sender's first call to `seal()` must
    /// correspond to the recipient's first call to `open()`, etc.
    pub fn open(&mut self, ciphertext: &[u8], aad: &[u8]) -> Option<Vec<u8>> {
        let max_out_len = ciphertext.len();
        unsafe {
            with_output_vec_fallible(max_out_len, |out_buf| {
                let mut out_len = 0usize;
                // Safety: EVP_HPKE_CTX_open
                // - is called with context created from EVP_HPKE_CTX_new and
                // - is called with valid buffers with corresponding pointer and length.
                let result = bssl_sys::EVP_HPKE_CTX_open(
                    self.0.as_mut_ffi_ptr(),
                    out_buf,
                    &mut out_len,
                    max_out_len,
                    ciphertext.as_ffi_ptr(),
                    ciphertext.len(),
                    aad.as_ffi_ptr(),
                    aad.len(),
                );
                if result == 1 {
                    Some(out_len)
                } else {
                    None
                }
            })
        }
    }

    /// Exports a secret of length `out_len` from the HPKE context using `context` as the context
    /// string.
    pub fn export(&mut self, context: &[u8], out_len: usize) -> Vec<u8> {
        unsafe {
            with_output_vec(out_len, |out_buf| {
                // Safety: EVP_HPKE_CTX_export
                // - is called with context created from EVP_HPKE_CTX_new,
                // - is called with valid buffers with corresponding pointer and length, and
                // - returns 0 on error, which only occurs when OOM.
                let ret = bssl_sys::EVP_HPKE_CTX_export(
                    self.0.as_mut_ffi_ptr(),
                    out_buf,
                    out_len,
                    context.as_ffi_ptr(),
                    context.len(),
                );
                assert_eq!(ret, 1);
                out_len
            })
        }
    }
}

#[cfg(test)]
mod test {
    use super::*;
    use crate::test_helpers::decode_hex;

    struct TestVector {
        kem_id: u16,
        kdf_id: u16,
        aead_id: u16,
        info: [u8; 20],
        seed_for_testing: [u8; 32],   // skEm
        recipient_pub_key: [u8; 32],  // pkRm
        recipient_priv_key: [u8; 32], // skRm
        encapsulated_key: [u8; 32],   // enc
        plaintext: [u8; 29],          // pt
        associated_data: [u8; 7],     // aad
        ciphertext: [u8; 45],         // ct
        exporter_context: [u8; 11],
        exported_value: [u8; 32],
    }

    // https://www.rfc-editor.org/rfc/rfc9180.html#appendix-A.1
    fn x25519_hkdf_sha256_hkdf_sha256_aes_128_gcm() -> TestVector {
        TestVector {
            kem_id: 32,
            kdf_id: 1,
            aead_id: 1,
            info: decode_hex("4f6465206f6e2061204772656369616e2055726e"),
            seed_for_testing: decode_hex("52c4a758a802cd8b936eceea314432798d5baf2d7e9235dc084ab1b9cfa2f736"),
            recipient_pub_key: decode_hex("3948cfe0ad1ddb695d780e59077195da6c56506b027329794ab02bca80815c4d"),
            recipient_priv_key: decode_hex("4612c550263fc8ad58375df3f557aac531d26850903e55a9f23f21d8534e8ac8"),
            encapsulated_key: decode_hex("37fda3567bdbd628e88668c3c8d7e97d1d1253b6d4ea6d44c150f741f1bf4431"),
            plaintext: decode_hex("4265617574792069732074727574682c20747275746820626561757479"),
            associated_data: decode_hex("436f756e742d30"),
            ciphertext: decode_hex("f938558b5d72f1a23810b4be2ab4f84331acc02fc97babc53a52ae8218a355a96d8770ac83d07bea87e13c512a"),
            exporter_context: decode_hex("54657374436f6e74657874"),
            exported_value: decode_hex("e9e43065102c3836401bed8c3c3c75ae46be1639869391d62c61f1ec7af54931"),
        }
    }

    // https://www.rfc-editor.org/rfc/rfc9180.html#appendix-A.2
    fn x25519_hkdf_sha256_hkdf_sha256_chacha20_poly1305() -> TestVector {
        TestVector {
            kem_id: 32,
            kdf_id: 1,
            aead_id: 3,
            info: decode_hex("4f6465206f6e2061204772656369616e2055726e"),
            seed_for_testing: decode_hex("f4ec9b33b792c372c1d2c2063507b684ef925b8c75a42dbcbf57d63ccd381600"),
            recipient_pub_key: decode_hex("4310ee97d88cc1f088a5576c77ab0cf5c3ac797f3d95139c6c84b5429c59662a"),
            recipient_priv_key: decode_hex("8057991eef8f1f1af18f4a9491d16a1ce333f695d4db8e38da75975c4478e0fb"),
            encapsulated_key: decode_hex("1afa08d3dec047a643885163f1180476fa7ddb54c6a8029ea33f95796bf2ac4a"),
            plaintext: decode_hex("4265617574792069732074727574682c20747275746820626561757479"),
            associated_data: decode_hex("436f756e742d30"),
            ciphertext: decode_hex("1c5250d8034ec2b784ba2cfd69dbdb8af406cfe3ff938e131f0def8c8b60b4db21993c62ce81883d2dd1b51a28"),
            exporter_context: decode_hex("54657374436f6e74657874"),
            exported_value: decode_hex("5acb09211139c43b3090489a9da433e8a30ee7188ba8b0a9a1ccf0c229283e53"),
        }
    }

    #[test]
    fn all_algorithms() {
        let kems = vec![Kem::X25519HkdfSha256];
        let kdfs = vec![Kdf::HkdfSha256];
        let aeads = vec![Aead::Aes128Gcm, Aead::Aes256Gcm, Aead::Chacha20Poly1305];
        let plaintext: &[u8] = b"plaintext";
        let aad: &[u8] = b"aad";
        let info: &[u8] = b"info";

        for kem in &kems {
            let (pub_key, priv_key) = kem.generate_keypair();
            for kdf in &kdfs {
                for aead in &aeads {
                    let params =
                        Params::new_from_rfc_ids(*kem as u16, *kdf as u16, *aead as u16).unwrap();

                    let (mut send_ctx, encapsulated_key) =
                        SenderContext::new(&params, &pub_key, info).unwrap();
                    let mut recv_ctx =
                        RecipientContext::new(&params, &priv_key, &encapsulated_key, info).unwrap();
                    assert_eq!(
                        plaintext,
                        recv_ctx
                            .open(send_ctx.seal(plaintext, aad).as_ref(), aad)
                            .unwrap()
                    );
                    assert_eq!(
                        plaintext,
                        recv_ctx
                            .open(send_ctx.seal(plaintext, aad).as_ref(), aad)
                            .unwrap()
                    );
                    assert!(recv_ctx.open(b"nonsense", aad).is_none());
                }
            }
        }
    }

    fn new_sender_context_for_testing(
        params: &Params,
        recipient_pub_key: &[u8],
        info: &[u8],
        seed_for_testing: &[u8],
    ) -> (SenderContext, Vec<u8>) {
        let mut ctx = scoped::EvpHpkeCtx::new();

        let encapsulated_key = unsafe {
            with_output_vec_fallible(MAX_ENCAPSULATED_KEY_LEN, |enc_key_buf| {
                let mut enc_key_len = 0usize;
                // Safety: EVP_HPKE_CTX_setup_sender_with_seed_for_testing
                // - is called with context created from EVP_HPKE_CTX_new,
                // - is called with valid buffers with corresponding pointer and length, and
                // - returns 0 on error.
                let result = bssl_sys::EVP_HPKE_CTX_setup_sender_with_seed_for_testing(
                    ctx.as_mut_ffi_ptr(),
                    enc_key_buf,
                    &mut enc_key_len,
                    MAX_ENCAPSULATED_KEY_LEN,
                    params.kem,
                    params.kdf,
                    params.aead,
                    recipient_pub_key.as_ffi_ptr(),
                    recipient_pub_key.len(),
                    info.as_ffi_ptr(),
                    info.len(),
                    seed_for_testing.as_ffi_ptr(),
                    seed_for_testing.len(),
                );
                if result == 1 {
                    Some(enc_key_len)
                } else {
                    None
                }
            })
        }
        .unwrap();
        (SenderContext(ctx), encapsulated_key)
    }

    #[test]
    fn seal_with_vector() {
        for test in vec![
            x25519_hkdf_sha256_hkdf_sha256_aes_128_gcm(),
            x25519_hkdf_sha256_hkdf_sha256_chacha20_poly1305(),
        ] {
            let params = Params::new_from_rfc_ids(test.kem_id, test.kdf_id, test.aead_id).unwrap();

            let (mut ctx, encapsulated_key) = new_sender_context_for_testing(
                &params,
                &test.recipient_pub_key,
                &test.info,
                &test.seed_for_testing,
            );

            assert_eq!(encapsulated_key, test.encapsulated_key.to_vec());

            let ciphertext = ctx.seal(&test.plaintext, &test.associated_data);
            assert_eq!(&ciphertext, test.ciphertext.as_ref());
        }
    }

    #[test]
    fn open_with_vector() {
        for test in vec![
            x25519_hkdf_sha256_hkdf_sha256_aes_128_gcm(),
            x25519_hkdf_sha256_hkdf_sha256_chacha20_poly1305(),
        ] {
            let params = Params::new_from_rfc_ids(test.kem_id, test.kdf_id, test.aead_id).unwrap();

            let mut ctx = RecipientContext::new(
                &params,
                &test.recipient_priv_key,
                &test.encapsulated_key,
                &test.info,
            )
            .unwrap();

            let plaintext = ctx.open(&test.ciphertext, &test.associated_data).unwrap();
            assert_eq!(&plaintext, test.plaintext.as_ref());
        }
    }

    #[test]
    fn export_with_vector() {
        for test in vec![
            x25519_hkdf_sha256_hkdf_sha256_aes_128_gcm(),
            x25519_hkdf_sha256_hkdf_sha256_chacha20_poly1305(),
        ] {
            let params = Params::new_from_rfc_ids(test.kem_id, test.kdf_id, test.aead_id).unwrap();

            let (mut sender_ctx, _encapsulated_key) = new_sender_context_for_testing(
                &params,
                &test.recipient_pub_key,
                &test.info,
                &test.seed_for_testing,
            );
            assert_eq!(
                test.exported_value.as_ref(),
                sender_ctx.export(&test.exporter_context, test.exported_value.len())
            );

            let mut recipient_ctx = RecipientContext::new(
                &params,
                &test.recipient_priv_key,
                &test.encapsulated_key,
                &test.info,
            ).unwrap();
            assert_eq!(
                test.exported_value.as_ref(),
                recipient_ctx.export(&test.exporter_context, test.exported_value.len())
            );
        }
    }

    #[test]
    fn disallowed_params_fail() {
        let vec: TestVector = x25519_hkdf_sha256_hkdf_sha256_aes_128_gcm();

        assert!(Params::new_from_rfc_ids(0, vec.kdf_id, vec.aead_id).is_none());
        assert!(Params::new_from_rfc_ids(vec.kem_id, 0, vec.aead_id).is_none());
        assert!(Params::new_from_rfc_ids(vec.kem_id, vec.kdf_id, 0).is_none());
    }

    #[test]
    fn bad_recipient_pub_key_fails() {
        let vec: TestVector = x25519_hkdf_sha256_hkdf_sha256_aes_128_gcm();
        let params = Params::new_from_rfc_ids(vec.kem_id, vec.kdf_id, vec.aead_id).unwrap();

        assert!(SenderContext::new(&params, b"", &vec.info).is_none());
    }

    #[test]
    fn bad_recipient_priv_key_fails() {
        let vec: TestVector = x25519_hkdf_sha256_hkdf_sha256_aes_128_gcm();
        let params = Params::new_from_rfc_ids(vec.kem_id, vec.kdf_id, vec.aead_id).unwrap();

        assert!(RecipientContext::new(&params, b"", &vec.encapsulated_key, &vec.info).is_none());
    }
}
