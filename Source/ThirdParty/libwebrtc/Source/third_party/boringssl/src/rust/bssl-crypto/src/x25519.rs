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

//! X25519 is the Diffie-Hellman primitive built from curve25519. It is sometimes referred to as
//! “curve25519”, but “X25519” is a more precise name. See http://cr.yp.to/ecdh.html and
//! https://tools.ietf.org/html/rfc7748.

use alloc::borrow::ToOwned;

/// Number of bytes in a private key in X25519
pub const PRIVATE_KEY_LEN: usize = bssl_sys::X25519_PRIVATE_KEY_LEN as usize;
/// Number of bytes in a public key in X25519
pub const PUBLIC_KEY_LEN: usize = bssl_sys::X25519_PUBLIC_VALUE_LEN as usize;
/// Number of bytes in a shared secret derived with X25519
pub const SHARED_KEY_LEN: usize = bssl_sys::X25519_SHARED_KEY_LEN as usize;

/// Error while performing a X25519 Diffie-Hellman key exchange.
#[derive(Debug)]
pub struct DiffieHellmanError;

/// A struct containing a X25519 key pair.
pub struct PrivateKey {
    private_key: [u8; PRIVATE_KEY_LEN],
    public_key: [u8; PUBLIC_KEY_LEN],
}

impl PrivateKey {
    /// Derives a shared secrect from this private key and the given public key.
    pub fn diffie_hellman(
        &self,
        other_public_key: &PublicKey,
    ) -> Result<SharedSecret, DiffieHellmanError> {
        let mut shared_key_uninit = core::mem::MaybeUninit::<[u8; SHARED_KEY_LEN]>::uninit();
        // Safety:
        // - private_key and other_public_key are Rust 32-byte arrays
        // - shared_key_uninit is just initialized above to a 32 byte array
        let result = unsafe {
            bssl_sys::X25519(
                shared_key_uninit.as_mut_ptr() as *mut u8,
                self.private_key.as_ptr(),
                other_public_key.0.as_ptr(),
            )
        };
        if result == 1 {
            // Safety:
            // - `shared_key_uninit` is initialized by `X25519` above, and we checked that it
            //   succeeded
            let shared_key = unsafe { shared_key_uninit.assume_init() };
            Ok(crate::ecdh::SharedSecret(shared_key))
        } else {
            Err(DiffieHellmanError)
        }
    }

    /// Generate a new key pair for use in a Diffie-Hellman key exchange.
    pub fn generate() -> Self {
        let mut public_key_uninit = core::mem::MaybeUninit::<[u8; PUBLIC_KEY_LEN]>::uninit();
        let mut private_key_uninit = core::mem::MaybeUninit::<[u8; PRIVATE_KEY_LEN]>::uninit();
        // Safety:
        // - private_key_uninit and public_key_uninit are allocated to 32-bytes
        let (public_key, private_key) = unsafe {
            bssl_sys::X25519_keypair(
                public_key_uninit.as_mut_ptr() as *mut u8,
                private_key_uninit.as_mut_ptr() as *mut u8,
            );
            // Safety: Initialized by `X25519_keypair` above
            (
                public_key_uninit.assume_init(),
                private_key_uninit.assume_init(),
            )
        };
        Self {
            private_key,
            public_key,
        }
    }

    /// Tries to convert the given bytes into a private key.
    pub fn from_private_bytes(private_key_bytes: &[u8; PRIVATE_KEY_LEN]) -> Self {
        let mut public_key_uninit = core::mem::MaybeUninit::<[u8; PUBLIC_KEY_LEN]>::uninit();
        let private_key: [u8; PRIVATE_KEY_LEN] = private_key_bytes.to_owned();
        // Safety:
        // - private_key and public_key are Rust 32-byte arrays
        let public_key = unsafe {
            bssl_sys::X25519_public_from_private(
                public_key_uninit.as_mut_ptr() as *mut _,
                private_key.as_ptr(),
            );
            public_key_uninit.assume_init()
        };
        Self {
            private_key,
            public_key,
        }
    }
}

impl<'a> From<&'a PrivateKey> for PublicKey {
    fn from(value: &'a PrivateKey) -> Self {
        Self(value.public_key)
    }
}

/// A public key for X25519 elliptic curve.
#[derive(Debug, PartialEq, Eq)]
pub struct PublicKey([u8; PUBLIC_KEY_LEN]);

impl PublicKey {
    /// Converts this public key to its byte representation.
    pub fn to_bytes(&self) -> [u8; PUBLIC_KEY_LEN] {
        self.0
    }

    /// Returns a reference to the byte representation of this public key.
    pub fn as_bytes(&self) -> &[u8; PUBLIC_KEY_LEN] {
        &self.0
    }
}

impl From<&[u8; 32]> for PublicKey {
    fn from(value: &[u8; 32]) -> Self {
        Self(*value)
    }
}

/// Shared secret derived from a Diffie-Hellman key exchange. Don't use the shared key directly,
/// rather use a KDF and also include the two public values as inputs.
type SharedSecret = crate::ecdh::SharedSecret<SHARED_KEY_LEN>;

#[cfg(test)]
#[allow(clippy::unwrap_used)]
mod tests {
    use crate::{
        test_helpers::decode_hex,
        x25519::{PrivateKey, PublicKey},
    };

    #[test]
    fn x25519_test_diffie_hellman() {
        // wycheproof/testvectors/x25519_test.json tcId 1
        let public_key_bytes: [u8; 32] =
            decode_hex("504a36999f489cd2fdbc08baff3d88fa00569ba986cba22548ffde80f9806829");
        let private_key =
            decode_hex("c8a9d5a91091ad851c668b0736c1c9a02936c0d3ad62670858088047ba057475");
        let expected_shared_secret: [u8; 32] =
            decode_hex("436a2c040cf45fea9b29a0cb81b1f41458f863d0d61b453d0a982720d6d61320");
        let public_key = PublicKey::from(&public_key_bytes);
        let private_key = PrivateKey::from_private_bytes(&private_key);

        let shared_secret = private_key.diffie_hellman(&public_key).unwrap();
        assert_eq!(expected_shared_secret, shared_secret.to_bytes());
    }

    #[test]
    fn x25519_generate_diffie_hellman_matches() {
        let private_key_1 = PrivateKey::generate();
        let private_key_2 = PrivateKey::generate();
        let public_key_1 = PublicKey::from(&private_key_1);
        let public_key_2 = PublicKey::from(&private_key_2);

        let diffie_hellman_1 = private_key_1.diffie_hellman(&public_key_2).unwrap();
        let diffie_hellman_2 = private_key_2.diffie_hellman(&public_key_1).unwrap();

        assert_eq!(diffie_hellman_1.to_bytes(), diffie_hellman_2.to_bytes());
    }

    #[test]
    fn x25519_test_diffie_hellman_zero_public_key() {
        // wycheproof/testvectors/x25519_test.json tcId 32
        let public_key_bytes =
            decode_hex("0000000000000000000000000000000000000000000000000000000000000000");
        let private_key =
            decode_hex("88227494038f2bb811d47805bcdf04a2ac585ada7f2f23389bfd4658f9ddd45e");
        let public_key = PublicKey::from(&public_key_bytes);
        let private_key = PrivateKey::from_private_bytes(&private_key);

        let shared_secret = private_key.diffie_hellman(&public_key);
        assert!(shared_secret.is_err());
    }

    #[test]
    fn x25519_public_key_byte_conversion() {
        let public_key_bytes =
            decode_hex("504a36999f489cd2fdbc08baff3d88fa00569ba986cba22548ffde80f9806829");
        let public_key = PublicKey::from(&public_key_bytes);
        assert_eq!(public_key_bytes, public_key.to_bytes());
    }

    #[test]
    fn x25519_test_public_key_from_private_key() {
        // Taken from https://www.rfc-editor.org/rfc/rfc7748.html#section-6.1
        let public_key_bytes =
            decode_hex("8520f0098930a754748b7ddcb43ef75a0dbf3a0d26381af4eba4a98eaa9b4e6a");
        let private_key_bytes =
            decode_hex("77076d0a7318a57d3c16c17251b26645df4c2f87ebc0992ab177fba51db92c2a");
        let private_key = PrivateKey::from_private_bytes(&private_key_bytes);

        assert_eq!(
            PublicKey::from(&public_key_bytes),
            PublicKey::from(&private_key)
        );
    }
}
