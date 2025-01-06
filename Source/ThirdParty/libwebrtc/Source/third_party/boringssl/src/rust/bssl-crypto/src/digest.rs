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

//! Hash functions.
//!
//! ```
//! use bssl_crypto::digest;
//!
//! // One-shot hashing.
//! let digest: [u8; 32] = digest::Sha256::hash(b"hello");
//!
//! // Incremental hashing.
//! let mut ctx = digest::Sha256::new();
//! ctx.update(b"hel");
//! ctx.update(b"lo");
//! let digest2: [u8; 32] = ctx.digest();
//!
//! assert_eq!(digest, digest2);
//!
//! // Hashing with dynamic dispatch.
//! #[cfg(feature = "std")]
//! {
//!     fn update_hash(ctx: &mut dyn std::io::Write) {
//!         ctx.write(b"hel");
//!         ctx.write(b"lo");
//!     }
//!
//!     let mut ctx = digest::Sha256::new();
//!     update_hash(&mut ctx);
//!     assert_eq!(ctx.digest(), digest);
//! }
//! ```

use crate::{sealed, FfiSlice, ForeignTypeRef};
use alloc::vec::Vec;

#[non_exhaustive]
#[doc(hidden)]
pub struct MdRef;

unsafe impl ForeignTypeRef for MdRef {
    type CType = bssl_sys::EVP_MD;
}

/// Provides the ability to hash in an algorithm-agnostic manner.
pub trait Algorithm {
    /// The size of the resulting digest.
    const OUTPUT_LEN: usize;
    /// The block length (in bytes).
    const BLOCK_LEN: usize;

    /// Gets a reference to a message digest algorithm to be used by the HKDF implementation.
    #[doc(hidden)]
    fn get_md(_: sealed::Sealed) -> &'static MdRef;

    /// Hashes a message.
    fn hash_to_vec(input: &[u8]) -> Vec<u8>;

    /// Create a new context for incremental hashing.
    fn new() -> Self;

    /// Hash the contents of `input`.
    fn update(&mut self, input: &[u8]);

    /// Finish the hashing and return the digest.
    fn digest_to_vec(self) -> Vec<u8>;
}

/// The insecure SHA-1 hash algorithm.
///
/// Some existing protocols depend on SHA-1 and so it is provided here, but it
/// does not provide collision resistance and should not be used if at all
/// avoidable. Use SHA-256 instead.
#[derive(Clone)]
pub struct InsecureSha1 {
    ctx: bssl_sys::SHA_CTX,
}

unsafe_iuf_algo!(
    InsecureSha1,
    20,
    64,
    EVP_sha1,
    SHA1,
    SHA1_Init,
    SHA1_Update,
    SHA1_Final
);

/// The SHA-256 hash algorithm.
#[derive(Clone)]
pub struct Sha256 {
    ctx: bssl_sys::SHA256_CTX,
}

unsafe_iuf_algo!(
    Sha256,
    32,
    64,
    EVP_sha256,
    SHA256,
    SHA256_Init,
    SHA256_Update,
    SHA256_Final
);

/// The SHA-384 hash algorithm.
#[derive(Clone)]
pub struct Sha384 {
    ctx: bssl_sys::SHA512_CTX,
}

unsafe_iuf_algo!(
    Sha384,
    48,
    128,
    EVP_sha384,
    SHA384,
    SHA384_Init,
    SHA384_Update,
    SHA384_Final
);

/// The SHA-512 hash algorithm.
#[derive(Clone)]
pub struct Sha512 {
    ctx: bssl_sys::SHA512_CTX,
}

unsafe_iuf_algo!(
    Sha512,
    64,
    128,
    EVP_sha512,
    SHA512,
    SHA512_Init,
    SHA512_Update,
    SHA512_Final
);

/// The SHA-512/256 hash algorithm.
#[derive(Clone)]
pub struct Sha512_256 {
    ctx: bssl_sys::SHA512_CTX,
}

unsafe_iuf_algo!(
    Sha512_256,
    32,
    128,
    EVP_sha512_256,
    SHA512_256,
    SHA512_256_Init,
    SHA512_256_Update,
    SHA512_256_Final
);

#[cfg(test)]
mod test {
    use super::*;
    use crate::test_helpers::decode_hex;

    #[test]
    fn sha256_c_type() {
        unsafe {
            assert_eq!(
                MdRef::from_ptr(bssl_sys::EVP_sha256() as *mut _).as_ptr(),
                bssl_sys::EVP_sha256() as *mut _
            )
        }
    }

    #[test]
    fn sha512_c_type() {
        unsafe {
            assert_eq!(
                MdRef::from_ptr(bssl_sys::EVP_sha512() as *mut _).as_ptr(),
                bssl_sys::EVP_sha512() as *mut _
            )
        }
    }

    #[test]
    fn sha1() {
        assert_eq!(
            decode_hex("a9993e364706816aba3e25717850c26c9cd0d89d"),
            InsecureSha1::hash(b"abc")
        );
    }

    #[test]
    fn sha256() {
        let msg: [u8; 4] = decode_hex("74ba2521");
        let expected_digest: [u8; 32] =
            decode_hex("b16aa56be3880d18cd41e68384cf1ec8c17680c45a02b1575dc1518923ae8b0e");

        assert_eq!(Sha256::hash(&msg), expected_digest);

        let mut ctx = Sha256::new();
        ctx.update(&msg);
        assert_eq!(expected_digest, ctx.digest());

        let mut ctx = Sha256::new();
        ctx.update(&msg[0..1]);
        let mut ctx2 = ctx.clone();
        ctx2.update(&msg[1..]);
        assert_eq!(expected_digest, ctx2.digest());
    }

    #[test]
    fn sha384() {
        assert_eq!(
            decode_hex("cb00753f45a35e8bb5a03d699ac65007272c32ab0eded1631a8b605a43ff5bed8086072ba1e7cc2358baeca134c825a7"),
            Sha384::hash(b"abc")
        );
    }

    #[test]
    fn sha512() {
        let msg: [u8; 4] = decode_hex("23be86d5");
        let expected_digest: [u8; 64] = decode_hex(concat!(
            "76d42c8eadea35a69990c63a762f330614a4699977f058adb988f406fb0be8f2",
            "ea3dce3a2bbd1d827b70b9b299ae6f9e5058ee97b50bd4922d6d37ddc761f8eb"
        ));

        assert_eq!(Sha512::hash(&msg), expected_digest);

        let mut ctx = Sha512::new();
        ctx.update(&msg);
        assert_eq!(expected_digest, ctx.digest());
    }

    #[test]
    fn sha512_256() {
        assert_eq!(
            decode_hex("53048e2681941ef99b2e29b76b4c7dabe4c2d0c634fc6d46e0e2f13107e7af23"),
            Sha512_256::hash(b"abc")
        );
    }
}
