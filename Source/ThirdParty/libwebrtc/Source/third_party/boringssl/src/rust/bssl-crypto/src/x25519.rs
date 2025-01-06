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

//! Diffie-Hellman over curve25519.
//!
//! X25519 is the Diffie-Hellman primitive built from curve25519. It is sometimes referred to as
//! “curve25519”, but “X25519” is a more precise name. See <http://cr.yp.to/ecdh.html> and
//! <https://tools.ietf.org/html/rfc7748>.
//!
//! ```
//! use bssl_crypto::x25519;
//!
//! // Alice generates her key pair.
//! let (alice_public_key, alice_private_key) = x25519::PrivateKey::generate();
//! // Bob generates his key pair.
//! let (bob_public_key, bob_private_key) = x25519::PrivateKey::generate();
//!
//! // If Alice obtains Bob's public key somehow, she can compute their
//! // shared key:
//! let shared_key = alice_private_key.compute_shared_key(&bob_public_key);
//!
//! // Alice can then derive a key (e.g. by using HKDF), which should include
//! // at least the two public keys. Then shen can send a message to Bob
//! // including her public key and an AEAD-protected blob. Bob can compute the
//! // same shared key given Alice's public key:
//! let shared_key2 = bob_private_key.compute_shared_key(&alice_public_key);
//! assert_eq!(shared_key, shared_key2);
//!
//! // This is an _unauthenticated_ exchange which is vulnerable to an
//! // active attacker. See, for example,
//! // http://www.noiseprotocol.org/noise.html for an example of building
//! // real protocols from a Diffie-Hellman primitive.
//! ```

use crate::{with_output_array, with_output_array_fallible, FfiSlice};

/// Number of bytes in a private key in X25519
pub const PRIVATE_KEY_LEN: usize = bssl_sys::X25519_PRIVATE_KEY_LEN as usize;
/// Number of bytes in a public key in X25519
pub const PUBLIC_KEY_LEN: usize = bssl_sys::X25519_PUBLIC_VALUE_LEN as usize;
/// Number of bytes in a shared secret derived with X25519
pub const SHARED_KEY_LEN: usize = bssl_sys::X25519_SHARED_KEY_LEN as usize;

/// X25519 public keys are simply 32-byte strings.
pub type PublicKey = [u8; PUBLIC_KEY_LEN];

/// An X25519 private key (a 32-byte string).
pub struct PrivateKey(pub [u8; PRIVATE_KEY_LEN]);

impl AsRef<[u8]> for PrivateKey {
    fn as_ref(&self) -> &[u8] {
        &self.0
    }
}

impl PrivateKey {
    /// Derive the shared key between this private key and a peer's public key.
    /// Don't use the shared key directly, rather use a KDF and also include
    /// the two public values as inputs.
    ///
    /// Will fail and produce `None` if the peer's public key is a point of
    /// small order. It is safe to react to this in non-constant time.
    pub fn compute_shared_key(&self, other_public_key: &PublicKey) -> Option<[u8; SHARED_KEY_LEN]> {
        // Safety: `X25519` indeed writes `SHARED_KEY_LEN` bytes.
        unsafe {
            with_output_array_fallible(|out, _| {
                bssl_sys::X25519(out, self.0.as_ffi_ptr(), other_public_key.as_ffi_ptr()) == 1
            })
        }
    }

    /// Generate a new key pair.
    pub fn generate() -> (PublicKey, PrivateKey) {
        let mut public_key_uninit = core::mem::MaybeUninit::<[u8; PUBLIC_KEY_LEN]>::uninit();
        let mut private_key_uninit = core::mem::MaybeUninit::<[u8; PRIVATE_KEY_LEN]>::uninit();
        // Safety:
        // - private_key_uninit and public_key_uninit are the correct length.
        unsafe {
            bssl_sys::X25519_keypair(
                public_key_uninit.as_mut_ptr() as *mut u8,
                private_key_uninit.as_mut_ptr() as *mut u8,
            );
            // Safety: Initialized by `X25519_keypair` just above.
            (
                public_key_uninit.assume_init(),
                PrivateKey(private_key_uninit.assume_init()),
            )
        }
    }

    /// Compute the public key corresponding to this private key.
    pub fn to_public(&self) -> PublicKey {
        // Safety: `X25519_public_from_private` indeed fills an entire [`PublicKey`].
        unsafe {
            with_output_array(|out, _| {
                bssl_sys::X25519_public_from_private(out, self.0.as_ffi_ptr());
            })
        }
    }
}

#[cfg(test)]
#[allow(clippy::unwrap_used)]
mod tests {
    use crate::{test_helpers::decode_hex, x25519::PrivateKey};

    #[test]
    fn known_vector() {
        // wycheproof/testvectors/x25519_test.json tcId 1
        let public_key: [u8; 32] =
            decode_hex("504a36999f489cd2fdbc08baff3d88fa00569ba986cba22548ffde80f9806829");
        let private_key = PrivateKey(decode_hex(
            "c8a9d5a91091ad851c668b0736c1c9a02936c0d3ad62670858088047ba057475",
        ));
        let expected_shared_secret: [u8; 32] =
            decode_hex("436a2c040cf45fea9b29a0cb81b1f41458f863d0d61b453d0a982720d6d61320");
        let shared_secret = private_key.compute_shared_key(&public_key).unwrap();
        assert_eq!(expected_shared_secret, shared_secret);
    }

    #[test]
    fn all_zero_public_key() {
        assert!(PrivateKey::generate()
            .1
            .compute_shared_key(&[0u8; 32])
            .is_none());
    }

    #[test]
    fn to_public() {
        // Taken from https://www.rfc-editor.org/rfc/rfc7748.html#section-6.1
        let public_key_bytes =
            decode_hex("8520f0098930a754748b7ddcb43ef75a0dbf3a0d26381af4eba4a98eaa9b4e6a");
        let private_key = PrivateKey(decode_hex(
            "77076d0a7318a57d3c16c17251b26645df4c2f87ebc0992ab177fba51db92c2a",
        ));
        assert_eq!(public_key_bytes, private_key.to_public());
    }
}
