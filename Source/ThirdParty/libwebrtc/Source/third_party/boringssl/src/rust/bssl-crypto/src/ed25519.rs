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

//! Ed25519, a signature scheme.
//!
//! Ed25519 builds a signature scheme over a curve that is isogenous to
//! curve25519. This module provides the "pure" signature scheme described in
//! <https://datatracker.ietf.org/doc/html/rfc8032>.
//!
//! ```
//! use bssl_crypto::ed25519;
//!
//! let key = ed25519::PrivateKey::generate();
//! // Publish your public key.
//! let public_key_bytes = *key.to_public().as_bytes();
//!
//! // Sign and publish some message.
//! let signed_message = b"hello world";
//! let mut sig = key.sign(signed_message);
//!
//! // Anyone with the public key can verify it.
//! let public_key = ed25519::PublicKey::from_bytes(&public_key_bytes);
//! assert!(public_key.verify(signed_message, &sig).is_ok());
//! ```

use crate::{FfiMutSlice, FfiSlice, InvalidSignatureError};

/// The length in bytes of an Ed25519 public key.
pub const PUBLIC_KEY_LEN: usize = bssl_sys::ED25519_PUBLIC_KEY_LEN as usize;

/// The length in bytes of an Ed25519 seed which is the 32-byte private key
/// representation defined in RFC 8032.
pub const SEED_LEN: usize =
    (bssl_sys::ED25519_PRIVATE_KEY_LEN - bssl_sys::ED25519_PUBLIC_KEY_LEN) as usize;

/// The length in bytes of an Ed25519 signature.
pub const SIGNATURE_LEN: usize = bssl_sys::ED25519_SIGNATURE_LEN as usize;

// The length in bytes of an Ed25519 keypair. In BoringSSL, the private key is suffixed with the
// public key, so the keypair length is the same as the private key length.
const KEYPAIR_LEN: usize = bssl_sys::ED25519_PRIVATE_KEY_LEN as usize;

/// An Ed25519 private key.
pub struct PrivateKey([u8; KEYPAIR_LEN]);

/// An Ed25519 public key used to verify a signature + message.
pub struct PublicKey([u8; PUBLIC_KEY_LEN]);

/// An Ed25519 signature created by signing a message with a private key.
pub type Signature = [u8; SIGNATURE_LEN];

impl PrivateKey {
    /// Generates a new Ed25519 keypair.
    pub fn generate() -> Self {
        let mut public_key = [0u8; PUBLIC_KEY_LEN];
        let mut private_key = [0u8; KEYPAIR_LEN];

        // Safety:
        // - Public key and private key are the correct length.
        unsafe {
            bssl_sys::ED25519_keypair(public_key.as_mut_ffi_ptr(), private_key.as_mut_ffi_ptr())
        }

        PrivateKey(private_key)
    }

    /// Returns the "seed" of this private key, as defined in RFC 8032.
    pub fn to_seed(&self) -> [u8; SEED_LEN] {
        // This code will never panic because a length 32 slice will always fit into a
        // size 32 byte array. The private key is the first 32 bytes of the keypair.
        #[allow(clippy::expect_used)]
        self.0[..SEED_LEN]
            .try_into()
            .expect("A slice of length SEED_LEN will always fit into an array of length SEED_LEN")
    }

    /// Derives a key-pair from `seed`, which is the 32-byte private key representation defined
    /// in RFC 8032.
    pub fn from_seed(seed: &[u8; SEED_LEN]) -> Self {
        let mut public_key = [0u8; PUBLIC_KEY_LEN];
        let mut private_key = [0u8; KEYPAIR_LEN];

        // Safety:
        // - Public key, private key, and seed are the correct lengths.
        unsafe {
            bssl_sys::ED25519_keypair_from_seed(
                public_key.as_mut_ffi_ptr(),
                private_key.as_mut_ffi_ptr(),
                seed.as_ffi_ptr(),
            )
        }
        PrivateKey(private_key)
    }

    /// Signs the given message and returns the signature.
    pub fn sign(&self, msg: &[u8]) -> Signature {
        let mut sig_bytes = [0u8; SIGNATURE_LEN];

        // Safety:
        // - On allocation failure we panic.
        // - Signature and private keys are always the correct length.
        let result = unsafe {
            bssl_sys::ED25519_sign(
                sig_bytes.as_mut_ffi_ptr(),
                msg.as_ffi_ptr(),
                msg.len(),
                self.0.as_ffi_ptr(),
            )
        };
        assert_eq!(result, 1, "allocation failure in bssl_sys::ED25519_sign");

        sig_bytes
    }

    /// Returns the [`PublicKey`] corresponding to this private key.
    pub fn to_public(&self) -> PublicKey {
        let keypair_bytes = &self.0;

        // This code will never panic because a length 32 slice will always fit into a
        // size 32 byte array. The public key is the last 32 bytes of the keypair.
        #[allow(clippy::expect_used)]
        PublicKey(
            keypair_bytes[PUBLIC_KEY_LEN..]
                .try_into()
                .expect("The slice is always the correct size for a public key"),
        )
    }
}

impl PublicKey {
    /// Builds the public key from an array of bytes.
    pub fn from_bytes(bytes: &[u8; PUBLIC_KEY_LEN]) -> Self {
        PublicKey(*bytes)
    }

    /// Returns the bytes of the public key.
    pub fn as_bytes(&self) -> &[u8; PUBLIC_KEY_LEN] {
        &self.0
    }

    /// Verifies that `signature` is a valid signature, by this key, of `msg`.
    pub fn verify(&self, msg: &[u8], signature: &Signature) -> Result<(), InvalidSignatureError> {
        let ret = unsafe {
            // Safety: `self.0` is the correct length and other buffers are valid.
            bssl_sys::ED25519_verify(
                msg.as_ffi_ptr(),
                msg.len(),
                signature.as_ffi_ptr(),
                self.0.as_ffi_ptr(),
            )
        };
        if ret == 1 {
            Ok(())
        } else {
            Err(InvalidSignatureError)
        }
    }
}

#[cfg(test)]
mod test {
    use super::*;
    use crate::test_helpers;

    #[test]
    fn gen_roundtrip() {
        let private_key = PrivateKey::generate();
        assert_ne!([0u8; 64], private_key.0);
        let seed = private_key.to_seed();
        let new_private_key = PrivateKey::from_seed(&seed);
        assert_eq!(private_key.0, new_private_key.0);
    }

    #[test]
    fn empty_msg() {
        // Test Case 1 from RFC test vectors: https://www.rfc-editor.org/rfc/rfc8032#section-7.1
        let pk = test_helpers::decode_hex(
            "d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a",
        );
        let seed = test_helpers::decode_hex(
            "9d61b19deffd5a60ba844af492ec2cc44449c5697b326919703bac031cae7f60",
        );
        let msg = [0u8; 0];
        let sig_expected  = test_helpers::decode_hex("e5564300c360ac729086e2cc806e828a84877f1eb8e5d974d873e065224901555fb8821590a33bacc61e39701cf9b46bd25bf5f0595bbe24655141438e7a100b");
        let kp = PrivateKey::from_seed(&seed);
        let sig = kp.sign(&msg);
        assert_eq!(sig_expected, sig);

        let pub_key = PublicKey::from_bytes(&pk);
        assert_eq!(pub_key.as_bytes(), kp.to_public().as_bytes());
        assert!(pub_key.verify(&msg, &sig).is_ok());
    }

    #[test]
    fn ed25519_sign_and_verify() {
        // Test Case 15 from RFC test vectors: https://www.rfc-editor.org/rfc/rfc8032#section-7.1
        let pk = test_helpers::decode_hex(
            "cf3af898467a5b7a52d33d53bc037e2642a8da996903fc252217e9c033e2f291",
        );
        let sk = test_helpers::decode_hex(
            "9acad959d216212d789a119252ebfe0c96512a23c73bd9f3b202292d6916a738",
        );
        let msg: [u8; 14] = test_helpers::decode_hex("55c7fa434f5ed8cdec2b7aeac173");
        let sig_expected  = test_helpers::decode_hex("6ee3fe81e23c60eb2312b2006b3b25e6838e02106623f844c44edb8dafd66ab0671087fd195df5b8f58a1d6e52af42908053d55c7321010092748795ef94cf06");
        let kp = PrivateKey::from_seed(&sk);

        let sig = kp.sign(&msg);
        assert_eq!(sig_expected, sig);

        let pub_key = PublicKey::from_bytes(&pk);
        assert_eq!(pub_key.as_bytes(), kp.to_public().as_bytes());
        assert!(pub_key.verify(&msg, &sig).is_ok());
    }
}
