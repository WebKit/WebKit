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

use core::marker::PhantomData;

use crate::{CSlice, ForeignTypeRef};

/// The SHA-256 digest algorithm.
#[derive(Clone)]
pub struct Sha256 {}

/// The SHA-512 digest algorithm.
#[derive(Clone)]
pub struct Sha512 {}

/// A reference to an [`Md`], which abstracts the details of a specific hash function allowing code
/// to deal with the concept of a "hash function" without needing to know exactly which hash function
/// it is.
#[non_exhaustive]
pub struct MdRef;

unsafe impl ForeignTypeRef for MdRef {
    type CType = bssl_sys::EVP_MD;
}

/// Used internally to get a BoringSSL internal MD
pub trait Md {
    /// The output size of the hash operation.
    const OUTPUT_SIZE: usize;

    /// Gets a reference to a message digest algorithm to be used by the HKDF implementation.
    fn get_md() -> &'static MdRef;
}

impl Md for Sha256 {
    const OUTPUT_SIZE: usize = bssl_sys::SHA256_DIGEST_LENGTH as usize;

    fn get_md() -> &'static MdRef {
        // Safety:
        // - this always returns a valid pointer to an EVP_MD
        unsafe { MdRef::from_ptr(bssl_sys::EVP_sha256() as *mut _) }
    }
}

impl Sha256 {
    /// Creates a new [Digest] to compute a SHA-256 hash.
    pub fn new_digest() -> Digest<Self, { Self::OUTPUT_SIZE }> {
        // Note: This cannot be in the trait because using associated constants exprs there
        // requires nightly.
        Digest::<Self, { Self::OUTPUT_SIZE }>::new()
    }
}

impl Md for Sha512 {
    const OUTPUT_SIZE: usize = bssl_sys::SHA512_DIGEST_LENGTH as usize;

    fn get_md() -> &'static MdRef {
        // Safety:
        // - this always returns a valid pointer to an EVP_MD
        unsafe { MdRef::from_ptr(bssl_sys::EVP_sha512() as *mut _) }
    }
}

impl Sha512 {
    /// Create a new [Digest] to compute a SHA-512 hash.
    pub fn new_digest() -> Digest<Self, { Self::OUTPUT_SIZE }> {
        // Note: This cannot be in the trait because using associated constants exprs there
        // requires nightly.
        Digest::<Self, { Self::OUTPUT_SIZE }>::new()
    }
}

/// A pending digest operation.
pub struct Digest<M: Md, const OUTPUT_SIZE: usize>(bssl_sys::EVP_MD_CTX, PhantomData<M>);

impl<M: Md, const OUTPUT_SIZE: usize> Digest<M, OUTPUT_SIZE> {
    /// Creates a new Digest from the given `Md` type parameter.
    ///
    /// Panics:
    /// - If `Md::OUTPUT_SIZE` is not the same as `OUTPUT_SIZE`.
    fn new() -> Self {
        // Note: runtime assertion needed here since using {M::OUTPUT_SIZE} in return type requires
        // unstable Rust feature.
        assert_eq!(M::OUTPUT_SIZE, OUTPUT_SIZE);
        let mut md_ctx_uninit = core::mem::MaybeUninit::<bssl_sys::EVP_MD_CTX>::uninit();
        // Safety:
        // - `EVP_DigestInit` initializes `md_ctx_uninit`
        // - `MdRef` ensures the validity of `md.as_ptr`
        let result =
            unsafe { bssl_sys::EVP_DigestInit(md_ctx_uninit.as_mut_ptr(), M::get_md().as_ptr()) };
        assert_eq!(result, 1, "bssl_sys::EVP_DigestInit failed");
        // Safety:
        // - md_ctx_uninit initialized with EVP_DigestInit, and the function returned 1 (success)
        let md_ctx = unsafe { md_ctx_uninit.assume_init() };
        Self(md_ctx, PhantomData)
    }

    /// Hashes the provided input into the current digest operation.
    pub fn update(&mut self, data: &[u8]) {
        let data_ffi = CSlice(data);
        // Safety:
        // - `data` is a CSlice from safe Rust.
        let result = unsafe {
            bssl_sys::EVP_DigestUpdate(&mut self.0, data_ffi.as_ptr() as *const _, data_ffi.len())
        };
        assert_eq!(result, 1, "bssl_sys::EVP_DigestUpdate failed");
    }

    /// Computes the final digest value, consuming the object.
    #[allow(clippy::expect_used)]
    pub fn finalize(mut self) -> [u8; OUTPUT_SIZE] {
        let mut digest_uninit =
            core::mem::MaybeUninit::<[u8; bssl_sys::EVP_MAX_MD_SIZE as usize]>::uninit();
        let mut len_uninit = core::mem::MaybeUninit::<u32>::uninit();
        // Safety:
        // - `digest_uninit` is allocated to `EVP_MAX_MD_SIZE` bytes long, as required by
        //   EVP_DigestFinal_ex
        // - `self.0` is owned by `self`, and is going to be cleaned up on drop.
        let result = unsafe {
            bssl_sys::EVP_DigestFinal_ex(
                &mut self.0,
                digest_uninit.as_mut_ptr() as *mut _,
                len_uninit.as_mut_ptr(),
            )
        };
        assert_eq!(result, 1, "bssl_sys::EVP_DigestFinal_ex failed");
        // Safety:
        // - `len_uninit` is initialized by `EVP_DigestFinal_ex`, and we checked the result above
        let len = unsafe { len_uninit.assume_init() };
        assert_eq!(
            OUTPUT_SIZE, len as usize,
            "bssl_sys::EVP_DigestFinal_ex failed"
        );
        // Safety: Result of DigestFinal_ex was checked above
        let digest = unsafe { digest_uninit.assume_init() };
        digest
            .get(..OUTPUT_SIZE)
            .and_then(|digest| digest.try_into().ok())
            .expect("The length of `digest` was checked above")
    }
}

impl<M: Md, const OUTPUT_SIZE: usize> Drop for Digest<M, OUTPUT_SIZE> {
    fn drop(&mut self) {
        // Safety: `self.0` is owned by `self`, and is invalidated after `drop`.
        unsafe {
            bssl_sys::EVP_MD_CTX_cleanup(&mut self.0);
        }
    }
}

#[cfg(test)]
mod test {
    use crate::test_helpers::decode_hex;

    use super::*;

    #[test]
    fn test_sha256_c_type() {
        unsafe {
            assert_eq!(
                MdRef::from_ptr(bssl_sys::EVP_sha256() as *mut _).as_ptr(),
                bssl_sys::EVP_sha256() as *mut _
            )
        }
    }

    #[test]
    fn test_sha512_c_type() {
        unsafe {
            assert_eq!(
                MdRef::from_ptr(bssl_sys::EVP_sha512() as *mut _).as_ptr(),
                bssl_sys::EVP_sha512() as *mut _
            )
        }
    }

    #[test]
    fn test_digest_sha256() {
        let mut digest = Sha256::new_digest();
        let msg: [u8; 4] = decode_hex("74ba2521");
        digest.update(&msg);
        let expected_digest: [u8; 32] =
            decode_hex("b16aa56be3880d18cd41e68384cf1ec8c17680c45a02b1575dc1518923ae8b0e");
        assert_eq!(expected_digest, digest.finalize());
    }

    #[test]
    fn test_digest_sha512() {
        let mut digest = Sha512::new_digest();
        let msg: [u8; 4] = decode_hex("23be86d5");
        digest.update(&msg);
        let expected_digest: [u8; 64] = decode_hex(concat!(
            "76d42c8eadea35a69990c63a762f330614a4699977f058adb988f406fb0be8f2",
            "ea3dce3a2bbd1d827b70b9b299ae6f9e5058ee97b50bd4922d6d37ddc761f8eb"
        ));
        assert_eq!(expected_digest, digest.finalize());
    }

    #[test]
    #[should_panic]
    fn test_digest_wrong_size() {
        // This should not happen since we don't externally expose Digest::new
        Digest::<Sha256, 64>::new();
    }
}
