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

//! Implements the HMAC-based Key Derivation Function from
//! <https://datatracker.ietf.org/doc/html/rfc5869>.
//!
//! One-shot operation:
//!
//! ```
//! use bssl_crypto::{hkdf, hkdf::HkdfSha256};
//!
//! let key: [u8; 32] = HkdfSha256::derive(b"secret", hkdf::Salt::NonEmpty(b"salt"),
//!                                        b"info");
//! ```
//!
//! If deriving several keys that vary only in the `info` parameter, then part
//! of the computation can be shared by calculating the "pseudo-random key".
//! This is purely a performance optimisation.
//!
//! ```
//! use bssl_crypto::{hkdf, hkdf::HkdfSha256};
//!
//! let prk = HkdfSha256::extract(b"secret", hkdf::Salt::NonEmpty(b"salt"));
//! let key1 : [u8; 32] = prk.expand(b"info1");
//! let key2 : [u8; 32] = prk.expand(b"info2");
//!
//! assert_eq!(key1, HkdfSha256::derive(b"secret", hkdf::Salt::NonEmpty(b"salt"),
//!                                     b"info1"));
//! assert_eq!(key2, HkdfSha256::derive(b"secret", hkdf::Salt::NonEmpty(b"salt"),
//!                                     b"info2"));
//! ```
//!
//! The above examples assume that the size of the outputs is known at compile
//! time. (And only output lengths less than 256 bytes are supported.)
//!
//! ```compile_fail
//! use bssl_crypto::{hkdf, hkdf::HkdfSha256};
//!
//! let key: [u8; 256] = HkdfSha256::derive(b"secret", hkdf::Salt::None, b"info");
//! ```
//!
//! To use HKDF with longer, or run-time, lengths, use `derive_into` and
//! `extract_into`:
//!
//! ```
//! use bssl_crypto::{hkdf, hkdf::HkdfSha256};
//!
//! let mut out = [0u8; 50];
//! HkdfSha256::derive_into(b"secret", hkdf::Salt::None, b"info", &mut out).expect(
//!    "HKDF can't produce that much");
//!
//! assert_eq!(out, HkdfSha256::derive(b"secret", hkdf::Salt::None, b"info"));
//! ```
//!
//! To expand output from the explicit bytes of a PRK, use `Prk::new`:
//!
//! ```
//! use bssl_crypto::{digest::Sha256, digest::Algorithm, hkdf};
//!
//! let prk: [u8; Sha256::OUTPUT_LEN] = bssl_crypto::rand_array();
//! // unwrap: only fails if the input is not equal to the digest length, which
//! // cannot happen here.
//! let prk = hkdf::Prk::new::<Sha256>(&prk).unwrap();
//! let mut out = vec![0u8; 42];
//! prk.expand_into(b"info", &mut out)?;
//! # Ok::<(), hkdf::TooLong>(())
//! ```

use crate::{digest, sealed, with_output_array, FfiMutSlice, FfiSlice, ForeignTypeRef};
use core::marker::PhantomData;

/// Implementation of HKDF-SHA-256
pub type HkdfSha256 = Hkdf<digest::Sha256>;

/// Implementation of HKDF-SHA-512
pub type HkdfSha512 = Hkdf<digest::Sha512>;

/// Error type returned when too much output is requested from an HKDF operation.
#[derive(Debug)]
pub struct TooLong;

/// HKDF's optional salt values. See <https://datatracker.ietf.org/doc/html/rfc5869#section-3.1>
pub enum Salt<'a> {
    /// No salt.
    None,
    /// An explicit salt. Note that an empty value here is interpreted the same
    /// as if passing `None`.
    NonEmpty(&'a [u8]),
}

impl Salt<'_> {
    fn as_ffi_ptr(&self) -> *const u8 {
        match self {
            Salt::None => core::ptr::null(),
            Salt::NonEmpty(salt) => salt.as_ffi_ptr(),
        }
    }

    fn len(&self) -> usize {
        match self {
            Salt::None => 0,
            Salt::NonEmpty(salt) => salt.len(),
        }
    }
}

/// HKDF for any of the implemented hash functions. The aliases [`HkdfSha256`]
/// and [`HkdfSha512`] are provided for the most common cases.
pub struct Hkdf<MD: digest::Algorithm>(PhantomData<MD>);

impl<MD: digest::Algorithm> Hkdf<MD> {
    /// The maximum number of bytes of key material that can be produced.
    pub const MAX_OUTPUT_LEN: usize = MD::OUTPUT_LEN * 255;

    /// Derive key material from the given secret, salt, and info. Attempting
    /// to derive more than 255 bytes is a compile-time error, see `derive_into`
    /// for longer outputs.
    ///
    /// The semantics of the arguments are complex. See
    /// <https://datatracker.ietf.org/doc/html/rfc5869#section-3>.
    pub fn derive<const N: usize>(secret: &[u8], salt: Salt, info: &[u8]) -> [u8; N] {
        Self::extract(secret, salt).expand(info)
    }

    /// Derive key material from the given secret, salt, and info. Attempting
    /// to derive more than `MAX_OUTPUT_LEN` bytes is a run-time error.
    ///
    /// The semantics of the arguments are complex. See
    /// <https://datatracker.ietf.org/doc/html/rfc5869#section-3>.
    pub fn derive_into(
        secret: &[u8],
        salt: Salt,
        info: &[u8],
        out: &mut [u8],
    ) -> Result<(), TooLong> {
        Self::extract(secret, salt).expand_into(info, out)
    }

    /// Extract a pseudo-random key from the given secret and salt. This can
    /// be used to avoid redoing computation when computing several keys that
    /// vary only in the `info` parameter.
    pub fn extract(secret: &[u8], salt: Salt) -> Prk {
        let mut prk = [0u8; bssl_sys::EVP_MAX_MD_SIZE as usize];
        let mut prk_len = 0usize;
        let evp_md = MD::get_md(sealed::Sealed).as_ptr();
        unsafe {
            // Safety: `EVP_MAX_MD_SIZE` is the maximum output size of
            // `HKDF_extract` so it'll never overrun the buffer.
            bssl_sys::HKDF_extract(
                prk.as_mut_ffi_ptr(),
                &mut prk_len,
                evp_md,
                secret.as_ffi_ptr(),
                secret.len(),
                salt.as_ffi_ptr(),
                salt.len(),
            );
        }
        // This is documented to be always be true.
        assert!(prk_len <= prk.len());
        Prk {
            prk,
            len: prk_len,
            evp_md,
        }
    }
}

/// A pseudo-random key, an intermediate value in the HKDF computation.
pub struct Prk {
    prk: [u8; bssl_sys::EVP_MAX_MD_SIZE as usize],
    len: usize,
    evp_md: *const bssl_sys::EVP_MD,
}

#[allow(clippy::let_unit_value)]
impl Prk {
    /// Creates a Prk from bytes.
    pub fn new<MD: digest::Algorithm>(prk_bytes: &[u8]) -> Option<Self> {
        if prk_bytes.len() != MD::OUTPUT_LEN {
            return None;
        }

        let mut prk = [0u8; bssl_sys::EVP_MAX_MD_SIZE as usize];
        prk.get_mut(..MD::OUTPUT_LEN)
            // unwrap: `EVP_MAX_MD_SIZE` must be greater than the length of any
            // digest function thus this is always successful.
            .unwrap()
            .copy_from_slice(prk_bytes);

        Some(Prk {
            prk,
            len: MD::OUTPUT_LEN,
            evp_md: MD::get_md(sealed::Sealed).as_ptr(),
        })
    }

    /// Returns the bytes of the pseudorandom key.
    pub fn as_bytes(&self) -> &[u8] {
        // `self.len` must be less than the length of `self.prk` thus
        // this is always in bounds.
        &self.prk[..self.len]
    }

    /// Derive key material for the given info parameter. Attempting
    /// to derive more than 255 bytes is a compile-time error, see `expand_into`
    /// for longer outputs.
    pub fn expand<const N: usize>(&self, info: &[u8]) -> [u8; N] {
        // This is the odd way to write a static assertion that uses a const
        // parameter in Rust. Even then, Rust cannot reference `MAX_OUTPUT_LEN`.
        // But if we safely assume that all hash functions output at least a
        // byte then 255 is a safe lower bound on `MAX_OUTPUT_LEN`.
        // A doctest at the top of the module checks that this assert is effective.
        struct StaticAssert<const N: usize, const BOUND: usize>;
        impl<const N: usize, const BOUND: usize> StaticAssert<N, BOUND> {
            const BOUNDS_CHECK: () = assert!(N < BOUND, "Large outputs not supported");
        }
        let _ = StaticAssert::<N, 256>::BOUNDS_CHECK;

        unsafe {
            with_output_array(|out, out_len| {
                // Safety: `HKDF_expand` writes exactly `out_len` bytes or else
                // returns zero. `evp_md` is valid by construction.
                let result = bssl_sys::HKDF_expand(
                    out,
                    out_len,
                    self.evp_md,
                    self.prk.as_ffi_ptr(),
                    self.len,
                    info.as_ffi_ptr(),
                    info.len(),
                );
                // The output length is known to be within bounds so the only other
                // possibily is an allocation failure, which we don't attempt to
                // handle.
                assert_eq!(result, 1);
            })
        }
    }

    /// Derive key material from the given info parameter. Attempting
    /// to derive more than the HKDF's `MAX_OUTPUT_LEN` bytes is a run-time
    /// error.
    pub fn expand_into(&self, info: &[u8], out: &mut [u8]) -> Result<(), TooLong> {
        // Safety: writes at most `out.len()` bytes into `out`.
        // `evp_md` is valid by construction.
        let result = unsafe {
            bssl_sys::HKDF_expand(
                out.as_mut_ffi_ptr(),
                out.len(),
                self.evp_md,
                self.prk.as_ffi_ptr(),
                self.len,
                info.as_ffi_ptr(),
                info.len(),
            )
        };
        if result == 1 {
            Ok(())
        } else {
            Err(TooLong)
        }
    }
}

#[cfg(test)]
#[allow(
    clippy::expect_used,
    clippy::panic,
    clippy::indexing_slicing,
    clippy::unwrap_used
)]
mod tests {
    use crate::{
        digest::Sha256,
        hkdf::{HkdfSha256, HkdfSha512, Prk, Salt},
        test_helpers::{decode_hex, decode_hex_into_vec},
    };

    #[test]
    fn sha256() {
        let ikm = decode_hex_into_vec("0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b");
        let salt_vec = decode_hex_into_vec("000102030405060708090a0b0c");
        let salt = Salt::NonEmpty(&salt_vec);
        let info = decode_hex_into_vec("f0f1f2f3f4f5f6f7f8f9");
        let okm: [u8; 42] = HkdfSha256::derive(ikm.as_slice(), salt, info.as_slice());
        let expected = decode_hex(
            "3cb25f25faacd57a90434f64d0362f2a2d2d0a90cf1a5a4c5db02d56ecc4c5bf34007208d5b887185865",
        );
        assert_eq!(okm, expected);
    }

    #[test]
    fn sha512() {
        let ikm = decode_hex_into_vec("5d3db20e8238a90b62a600fa57fdb318");
        let salt_vec = decode_hex_into_vec("1d6f3b38a1e607b5e6bcd4af1800a9d3");
        let salt = Salt::NonEmpty(&salt_vec);
        let info = decode_hex_into_vec("2bc5f39032b6fc87da69ba8711ce735b169646fd");
        let okm: [u8; 42] = HkdfSha512::derive(ikm.as_slice(), salt, info.as_slice());
        let expected = decode_hex(
            "8c3cf7122dcb5eb7efaf02718f1faf70bca20dcb75070e9d0871a413a6c05fc195a75aa9ffc349d70aae",
        );
        assert_eq!(okm, expected);
    }

    // Test Vectors from https://tools.ietf.org/html/rfc5869.
    #[test]
    fn rfc5869_sha256() {
        struct Test {
            ikm: Vec<u8>,
            salt: Vec<u8>,
            info: Vec<u8>,
            prk: Vec<u8>,
            okm: Vec<u8>,
        }
        let tests = [
            Test {
                // Test Case 1
                ikm: decode_hex_into_vec("0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b"),
                salt: decode_hex_into_vec("000102030405060708090a0b0c"),
                info: decode_hex_into_vec("f0f1f2f3f4f5f6f7f8f9"),
                prk: decode_hex_into_vec(
                    "077709362c2e32df0ddc3f0dc47bba63\
                    90b6c73bb50f9c3122ec844ad7c2b3e5",
                ),
                okm: decode_hex_into_vec("3cb25f25faacd57a90434f64d0362f2a2d2d0a90cf1a5a4c5db02d56ecc4c5bf34007208d5b887185865")
            },
            Test {
                // Test Case 2
                ikm: decode_hex_into_vec(
                    "000102030405060708090a0b0c0d0e0f\
                    101112131415161718191a1b1c1d1e1f\
                    202122232425262728292a2b2c2d2e2f\
                    303132333435363738393a3b3c3d3e3f\
                    404142434445464748494a4b4c4d4e4f",
                ),
                salt: decode_hex_into_vec(
                    "606162636465666768696a6b6c6d6e6f\
                    707172737475767778797a7b7c7d7e7f\
                    808182838485868788898a8b8c8d8e8f\
                    909192939495969798999a9b9c9d9e9f\
                    a0a1a2a3a4a5a6a7a8a9aaabacadaeaf",
                ),
                info: decode_hex_into_vec(
                    "b0b1b2b3b4b5b6b7b8b9babbbcbdbebf\
                    c0c1c2c3c4c5c6c7c8c9cacbcccdcecf\
                    d0d1d2d3d4d5d6d7d8d9dadbdcdddedf\
                    e0e1e2e3e4e5e6e7e8e9eaebecedeeef\
                    f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff",
                ),
                prk: decode_hex_into_vec(
                    "06a6b88c5853361a06104c9ceb35b45c\
                    ef760014904671014a193f40c15fc244",
                ),
                okm: decode_hex_into_vec(
                    "b11e398dc80327a1c8e7f78c596a4934\
                    4f012eda2d4efad8a050cc4c19afa97c\
                    59045a99cac7827271cb41c65e590e09\
                    da3275600c2f09b8367793a9aca3db71\
                    cc30c58179ec3e87c14c01d5c1f3434f\
                    1d87",
                )
            },
            Test {
                // Test Case 3
                ikm: decode_hex_into_vec("0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b"),
                salt: Vec::new(),
                info: Vec::new(),
                prk: decode_hex_into_vec(
                    "19ef24a32c717b167f33a91d6f648bdf\
                    96596776afdb6377ac434c1c293ccb04",
                ),
                okm: decode_hex_into_vec(
                    "8da4e775a563c18f715f802a063c5a31b8a11f5c5ee1879ec3454e5f3c738d2d9d201395faa4b61a96c8"),
            },
        ];

        for Test {
            ikm,
            salt,
            info,
            prk,
            okm,
        } in tests.iter()
        {
            let salt = if salt.is_empty() {
                Salt::None
            } else {
                Salt::NonEmpty(&salt)
            };
            let mut okm2 = vec![0u8; okm.len()];
            assert!(
                HkdfSha256::derive_into(ikm.as_slice(), salt, info.as_slice(), &mut okm2).is_ok()
            );
            assert_eq!(okm2.as_slice(), okm.as_slice());

            let prk2 = Prk::new::<Sha256>(prk.as_slice()).unwrap();
            assert_eq!(prk2.as_bytes(), prk.as_slice());
            let mut okm3 = vec![0u8; okm.len()];
            let _ = prk2.expand_into(info.as_slice(), &mut okm3);
            assert_eq!(okm3.as_slice(), okm.as_slice());
        }
    }

    #[test]
    fn max_output() {
        let hkdf = HkdfSha256::extract(b"", Salt::None);
        let mut longest = vec![0u8; HkdfSha256::MAX_OUTPUT_LEN];
        assert!(hkdf.expand_into(b"", &mut longest).is_ok());

        let mut too_long = vec![0u8; HkdfSha256::MAX_OUTPUT_LEN + 1];
        assert!(hkdf.expand_into(b"", &mut too_long).is_err());
    }

    #[test]
    fn wrong_prk_len() {
        assert!(Prk::new::<Sha256>(
            decode_hex_into_vec("077709362c2e32df0ddc3f0dc47bba63").as_slice()
        )
        .is_none());
        assert!(Prk::new::<Sha256>(
            decode_hex_into_vec("077709362c2e32df0ddc3f0dc47bba6390b6c73bb50f9c3122ec844ad7c2b3e590b6c73bb50f9c3122ec844ad7c2b3e5").as_slice())
            .is_none()
        );
    }
}
