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

use crate::CSlice;

/// The length in bytes of an Ed25519 public key.
pub const PUBLIC_KEY_LENGTH: usize = bssl_sys::ED25519_PUBLIC_KEY_LEN as usize;

/// The length in bytes of an Ed25519 seed which is the 32-byte private key representation defined
/// in RFC 8032.
pub const SEED_LENGTH: usize =
    (bssl_sys::ED25519_PRIVATE_KEY_LEN - bssl_sys::ED25519_PUBLIC_KEY_LEN) as usize;

/// The length in bytes of an Ed25519 signature.
pub const SIGNATURE_LENGTH: usize = bssl_sys::ED25519_SIGNATURE_LEN as usize;

// The length in bytes of an Ed25519 keypair. In BoringSSL, the private key is suffixed with the
// public key, so the keypair length is the same as the private key length.
const KEYPAIR_LENGTH: usize = bssl_sys::ED25519_PRIVATE_KEY_LEN as usize;

/// An Ed25519 private key.
pub struct PrivateKey([u8; KEYPAIR_LENGTH]);

/// An Ed25519 signature created by signing a message with a private key.
pub struct Signature([u8; SIGNATURE_LENGTH]);

/// An Ed25519 public key used to verify a signature + message.
pub struct PublicKey([u8; PUBLIC_KEY_LENGTH]);

/// Error returned if the verification on the signature + message fails.
#[derive(Debug)]
pub struct SignatureError;

impl PrivateKey {
    /// Generates a new Ed25519 keypair.
    pub fn generate() -> Self {
        let mut public_key = [0u8; PUBLIC_KEY_LENGTH];
        let mut private_key = [0u8; KEYPAIR_LENGTH];

        // Safety:
        // - Public key and private key are the correct length.
        unsafe { bssl_sys::ED25519_keypair(public_key.as_mut_ptr(), private_key.as_mut_ptr()) }

        PrivateKey(private_key)
    }

    /// Converts the key-pair to an array of bytes consisting of the bytes of the private key
    /// followed by the bytes of the public key.
    pub fn to_seed(&self) -> [u8; SEED_LENGTH] {
        // This code will never panic because a length 32 slice will always fit into a
        // size 32 byte array. The private key is the first 32 bytes of the keypair.
        #[allow(clippy::expect_used)]
        self.0[..SEED_LENGTH].try_into().expect(
            "A slice of length SEED_LENGTH will always fit into an array of length SEED_LENGTH",
        )
    }

    /// Builds this key-pair from `seed`, which is the 32-byte private key representation defined
    /// in RFC 8032.
    pub fn new_from_seed(seed: &[u8; SEED_LENGTH]) -> Self {
        let mut public_key = [0u8; PUBLIC_KEY_LENGTH];
        let mut private_key = [0u8; KEYPAIR_LENGTH];

        // Safety:
        // - Public key, private key, and seed are the correct lengths.
        unsafe {
            bssl_sys::ED25519_keypair_from_seed(
                public_key.as_mut_ptr(),
                private_key.as_mut_ptr(),
                seed.as_ptr(),
            )
        }
        PrivateKey(private_key)
    }

    /// Signs the given message and returns a digital signature.
    pub fn sign(&self, msg: &[u8]) -> Signature {
        let mut sig_bytes = [0u8; SIGNATURE_LENGTH];

        let msg_ffi = CSlice(msg);
        // Safety:
        // - On allocation failure we panic.
        // - Signature and private keys are always the correct length.
        let result = unsafe {
            bssl_sys::ED25519_sign(
                sig_bytes.as_mut_ptr(),
                msg_ffi.as_ptr(),
                msg_ffi.len(),
                self.0.as_ptr(),
            )
        };
        assert_eq!(result, 1, "allocation failure in bssl_sys::ED25519_sign");

        Signature(sig_bytes)
    }

    /// Returns the PublicKey of the KeyPair.
    pub fn public(&self) -> PublicKey {
        let keypair_bytes = self.0;

        // This code will never panic because a length 32 slice will always fit into a
        // size 32 byte array. The public key is the last 32 bytes of the keypair.
        #[allow(clippy::expect_used)]
        PublicKey(
            keypair_bytes[PUBLIC_KEY_LENGTH..]
                .try_into()
                .expect("The slice is always the correct size for a public key"),
        )
    }
}

impl PublicKey {
    /// Builds the public key from an array of bytes.
    pub fn from_bytes(bytes: [u8; PUBLIC_KEY_LENGTH]) -> Self {
        PublicKey(bytes)
    }

    /// Returns the bytes of the public key.
    pub fn to_bytes(&self) -> [u8; PUBLIC_KEY_LENGTH] {
        self.0
    }

    /// Succeeds if the signature is a valid signature created by this keypair, otherwise returns an Error.
    pub fn verify(&self, message: &[u8], signature: Signature) -> Result<(), SignatureError> {
        let message_cslice = CSlice::from(message);
        let ret = unsafe {
            bssl_sys::ED25519_verify(
                message_cslice.as_ptr(),
                message_cslice.len(),
                signature.0.as_ptr(),
                self.0.as_ptr(),
            )
        };
        if ret == 1 {
            Ok(())
        } else {
            Err(SignatureError)
        }
    }
}

impl Signature {
    /// Creates a signature from a byte array.
    pub fn from_bytes(bytes: [u8; SIGNATURE_LENGTH]) -> Self {
        Self(bytes)
    }

    /// Returns the bytes of the signature.
    pub fn to_bytes(&self) -> [u8; SIGNATURE_LENGTH] {
        self.0
    }
}

#[cfg(test)]
mod test {
    use super::*;
    use crate::test_helpers;

    #[test]
    fn ed25519_kp_gen_roundtrip() {
        let private_key = PrivateKey::generate();
        assert_ne!([0u8; 64], private_key.0);
        let seed = private_key.to_seed();
        let new_private_key = PrivateKey::new_from_seed(&seed);
        assert_eq!(private_key.0, new_private_key.0);
    }

    #[test]
    fn ed25519_empty_msg() {
        // Test Case 1 from RFC test vectors: https://www.rfc-editor.org/rfc/rfc8032#section-7.1
        let pk = test_helpers::decode_hex(
            "d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a",
        );
        let sk = test_helpers::decode_hex(
            "9d61b19deffd5a60ba844af492ec2cc44449c5697b326919703bac031cae7f60",
        );
        let msg = [0u8; 0];
        let sig_expected  = test_helpers::decode_hex("e5564300c360ac729086e2cc806e828a84877f1eb8e5d974d873e065224901555fb8821590a33bacc61e39701cf9b46bd25bf5f0595bbe24655141438e7a100b");
        let kp = PrivateKey::new_from_seed(&sk);
        let sig = kp.sign(&msg);
        assert_eq!(sig_expected, sig.0);

        let pub_key = PublicKey::from_bytes(pk);
        assert_eq!(pub_key.to_bytes(), kp.public().to_bytes());
        assert!(pub_key.verify(&msg, sig).is_ok());
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
        let kp = PrivateKey::new_from_seed(&sk);

        let sig = kp.sign(&msg);
        assert_eq!(sig_expected, sig.0);

        let pub_key = PublicKey::from_bytes(pk);
        assert_eq!(pub_key.to_bytes(), kp.public().to_bytes());
        assert!(pub_key.verify(&msg, sig).is_ok());
    }
}
