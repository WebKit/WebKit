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
use crate::digest::Md;
use crate::digest::{Sha256, Sha512};
use crate::{CSlice, CSliceMut, ForeignTypeRef};
use alloc::vec::Vec;
use core::marker::PhantomData;

/// Implementation of HKDF-SHA-256
pub type HkdfSha256 = Hkdf<Sha256>;

/// Implementation of HKDF-SHA-512
pub type HkdfSha512 = Hkdf<Sha512>;

/// Error type returned from the HKDF-Expand operations when the output key material has
/// an invalid length
#[derive(Debug)]
pub struct InvalidLength;

/// Implementation of HKDF operations which are generic over a provided hashing functions. Type
/// aliases are provided above for convenience of commonly used hashes
pub struct Hkdf<M: Md> {
    salt: Option<Vec<u8>>,
    ikm: Vec<u8>,
    _marker: PhantomData<M>,
}

impl<M: Md> Hkdf<M> {
    /// The max length of the output key material used for expanding
    pub const MAX_OUTPUT_LENGTH: usize = M::OUTPUT_SIZE * 255;

    /// Creates a new instance of HKDF from a salt and key material
    pub fn new(salt: Option<&[u8]>, ikm: &[u8]) -> Self {
        Self {
            salt: salt.map(Vec::from),
            ikm: Vec::from(ikm),
            _marker: PhantomData,
        }
    }

    /// Computes HKDF-Expand operation from RFC 5869. The info argument for the expand is set to
    /// the concatenation of all the elements of info_components. Returns InvalidLength if the
    /// output is too large.
    pub fn expand_multi_info(
        &self,
        info_components: &[&[u8]],
        okm: &mut [u8],
    ) -> Result<(), InvalidLength> {
        self.expand(&info_components.concat(), okm)
    }

    /// Computes HKDF-Expand operation from RFC 5869. Returns InvalidLength if the output is too large.
    pub fn expand(&self, info: &[u8], okm: &mut [u8]) -> Result<(), InvalidLength> {
        // extract the salt bytes from the option, or empty slice if option is None
        let salt = self.salt.as_deref().unwrap_or_default();

        //validate the output size
        (okm.len() <= Self::MAX_OUTPUT_LENGTH && !okm.is_empty())
            .then(|| {
                let mut okm_cslice = CSliceMut::from(okm);

                // Safety:
                // - We validate the output length above, so invalid length errors will never be hit
                // which leaves allocation failures as the only possible error case, in which case
                // we panic immediately
                let result = unsafe {
                    bssl_sys::HKDF(
                        okm_cslice.as_mut_ptr(),
                        okm_cslice.len(),
                        M::get_md().as_ptr(),
                        CSlice::from(self.ikm.as_slice()).as_ptr(),
                        self.ikm.as_slice().len(),
                        CSlice::from(salt).as_ptr(),
                        salt.len(),
                        CSlice::from(info).as_ptr(),
                        info.len(),
                    )
                };
                assert!(result > 0, "Allocation failure in bssl_sys::HKDF");
            })
            .ok_or(InvalidLength)
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
    use crate::hkdf::{HkdfSha256, HkdfSha512};
    use crate::test_helpers::{decode_hex, decode_hex_into_vec};
    use core::iter;

    struct Test {
        ikm: Vec<u8>,
        salt: Vec<u8>,
        info: Vec<u8>,
        okm: Vec<u8>,
    }

    #[test]
    fn hkdf_sha_256_test() {
        let ikm = decode_hex_into_vec("0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b");
        let salt = decode_hex_into_vec("000102030405060708090a0b0c");
        let info = decode_hex_into_vec("f0f1f2f3f4f5f6f7f8f9");

        let hk = HkdfSha256::new(Some(salt.as_slice()), ikm.as_slice());
        let mut okm = [0u8; 42];
        hk.expand(&info, &mut okm)
            .expect("42 is a valid length for Sha256 to output");

        let expected = decode_hex(
            "3cb25f25faacd57a90434f64d0362f2a2d2d0a90cf1a5a4c5db02d56ecc4c5bf34007208d5b887185865",
        );
        assert_eq!(okm, expected);
    }

    #[test]
    fn hkdf_sha512_test() {
        let ikm = decode_hex_into_vec("5d3db20e8238a90b62a600fa57fdb318");
        let salt = decode_hex_into_vec("1d6f3b38a1e607b5e6bcd4af1800a9d3");
        let info = decode_hex_into_vec("2bc5f39032b6fc87da69ba8711ce735b169646fd");

        let hk = HkdfSha512::new(Some(salt.as_slice()), ikm.as_slice());
        let mut okm = [0u8; 42];
        hk.expand(&info, &mut okm).expect("Should succeed");

        let expected = decode_hex(
            "8c3cf7122dcb5eb7efaf02718f1faf70bca20dcb75070e9d0871a413a6c05fc195a75aa9ffc349d70aae",
        );
        assert_eq!(okm, expected);
    }

    // Test Vectors from https://tools.ietf.org/html/rfc5869.
    #[test]
    fn test_rfc5869_sha256() {
        let tests = [
            Test {
                // Test Case 1
                ikm: decode_hex_into_vec("0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b"),
                salt: decode_hex_into_vec("000102030405060708090a0b0c"),
                info: decode_hex_into_vec("f0f1f2f3f4f5f6f7f8f9"),
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
                okm: decode_hex_into_vec(
                    "8da4e775a563c18f715f802a063c5a31b8a11f5c5ee1879ec3454e5f3c738d2d9d201395faa4b61a96c8"),
            },
        ];
        for Test {
            ikm,
            salt,
            info,
            okm,
        } in tests.iter()
        {
            let salt = if salt.is_empty() {
                None
            } else {
                Some(salt.as_slice())
            };
            let hkdf = HkdfSha256::new(salt, ikm.as_slice());
            let mut okm2 = vec![0u8; okm.len()];
            assert!(hkdf.expand(info.as_slice(), &mut okm2).is_ok());
            assert_eq!(okm2.as_slice(), okm.as_slice());
        }
    }

    #[test]
    fn test_lengths() {
        let hkdf = HkdfSha256::new(None, &[]);
        let mut longest = vec![0u8; HkdfSha256::MAX_OUTPUT_LENGTH];
        assert!(hkdf.expand(&[], &mut longest).is_ok());
        // start at 1 since 0 is an invalid length
        let lengths = 1..HkdfSha256::MAX_OUTPUT_LENGTH + 1;

        for length in lengths {
            let mut okm = vec![0u8; length];

            assert!(hkdf.expand(&[], &mut okm).is_ok());
            assert_eq!(okm.len(), length);
            assert_eq!(okm[..], longest[..length]);
        }
    }

    #[test]
    fn test_max_length() {
        let hkdf = HkdfSha256::new(Some(&[]), &[]);
        let mut okm = vec![0u8; HkdfSha256::MAX_OUTPUT_LENGTH];
        assert!(hkdf.expand(&[], &mut okm).is_ok());
    }

    #[test]
    fn test_max_length_exceeded() {
        let hkdf = HkdfSha256::new(Some(&[]), &[]);
        let mut okm = vec![0u8; HkdfSha256::MAX_OUTPUT_LENGTH + 1];
        assert!(hkdf.expand(&[], &mut okm).is_err());
    }

    #[test]
    fn test_unsupported_length() {
        let hkdf = HkdfSha256::new(Some(&[]), &[]);
        let mut okm = vec![0u8; 90000];
        assert!(hkdf.expand(&[], &mut okm).is_err());
    }

    #[test]
    fn test_expand_multi_info() {
        let info_components = &[
            &b"09090909090909090909090909090909090909090909"[..],
            &b"8a8a8a8a8a8a8a8a8a8a8a8a8a8a8a8a8a8a8a8a8a"[..],
            &b"0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0"[..],
            &b"4c4c4c4c4c4c4c4c4c4c4c4c4c4c4c4c4c4c4"[..],
            &b"1d1d1d1d1d1d1d1d1d1d1d1d1d1d1d1d1d"[..],
        ];

        let hkdf = HkdfSha256::new(None, b"some ikm here");

        // Compute HKDF-Expand on the concatenation of all the info components
        let mut oneshot_res = [0u8; 16];
        hkdf.expand(&info_components.concat(), &mut oneshot_res)
            .unwrap();

        // Now iteratively join the components of info_components until it's all 1 component. The value
        // of HKDF-Expand should be the same throughout
        let mut num_concatted = 0;
        let mut info_head = Vec::new();

        while num_concatted < info_components.len() {
            info_head.extend(info_components[num_concatted]);

            // Build the new input to be the info head followed by the remaining components
            let input: Vec<&[u8]> = iter::once(info_head.as_slice())
                .chain(info_components.iter().cloned().skip(num_concatted + 1))
                .collect();

            // Compute and compare to the one-shot answer
            let mut multipart_res = [0u8; 16];
            hkdf.expand_multi_info(&input, &mut multipart_res).unwrap();
            assert_eq!(multipart_res, oneshot_res);
            num_concatted += 1;
        }
    }
}
