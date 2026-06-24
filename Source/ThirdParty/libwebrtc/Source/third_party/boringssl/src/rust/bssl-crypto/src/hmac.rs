// Copyright 2023 The BoringSSL Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

//! Hash-based message authentication from <https://datatracker.ietf.org/doc/html/rfc2104>.
//!
//! HMAC-SHA256, HMAC-SHA384 and HMAC-SHA512 are supported.
//!
//! MACs can be computed in a single shot:
//!
//! ```
//! use bssl_crypto::hmac::HmacSha256;
//!
//! let mac: [u8; 32] = HmacSha256::mac(b"key", b"hello");
//! ```
//!
//! Or they can be computed incrementally:
//!
//! ```
//! use bssl_crypto::hmac::HmacSha256;
//!
//! let key = bssl_crypto::rand_array();
//! let mut ctx = HmacSha256::new(&key);
//! ctx.update(b"hel");
//! ctx.update(b"lo");
//! let mac: [u8; 32] = ctx.digest();
//! ```
//!
//! **WARNING** comparing MACs using typical methods will often leak information
//! about the size of the matching prefix. Use the `verify` method instead.
//!
//! If you need to compute many MACs with the same key, contexts can be
//! cloned:
//!
//! ```
//! use bssl_crypto::hmac::HmacSha256;
//!
//! let key = bssl_crypto::rand_array();
//! let mut keyed_ctx = HmacSha256::new(&key);
//! let mut ctx1 = keyed_ctx.clone();
//! ctx1.update(b"foo");
//! let mut ctx2 = keyed_ctx.clone();
//! ctx2.update(b"foo");
//!
//! assert_eq!(ctx1.digest(), ctx2.digest());
//! ```

use crate::{
    digest,
    digest::{Sha256, Sha384, Sha512},
    initialized_struct, sealed, FfiMutSlice, FfiSlice, ForeignTypeRef as _, InvalidSignatureError,
};
use core::{ffi::c_uint, marker::PhantomData, ptr};

/// HMAC-SHA256.
pub struct HmacSha256(Hmac<32, Sha256>);

impl HmacSha256 {
    /// Computes the HMAC-SHA256 of `data` as a one-shot operation.
    pub fn mac(key: &[u8], data: &[u8]) -> [u8; 32] {
        hmac::<32, Sha256>(key, data)
    }

    /// Creates a new HMAC-SHA256 operation from a fixed-length key.
    pub fn new(key: &[u8; 32]) -> Self {
        Self(Hmac::new(key))
    }

    /// Creates a new HMAC-SHA256 operation from a variable-length key.
    pub fn new_from_slice(key: &[u8]) -> Self {
        Self(Hmac::new_from_slice(key))
    }

    /// Hashes the provided input into the HMAC operation.
    pub fn update(&mut self, data: &[u8]) {
        self.0.update(data)
    }

    /// Computes the final HMAC value, consuming the object.
    pub fn digest(self) -> [u8; 32] {
        self.0.digest()
    }

    /// Checks that the provided tag value matches the computed HMAC value.
    pub fn verify_slice(self, tag: &[u8]) -> Result<(), InvalidSignatureError> {
        self.0.verify_slice(tag)
    }

    /// Checks that the provided tag value matches the computed HMAC value.
    pub fn verify(self, tag: &[u8; 32]) -> Result<(), InvalidSignatureError> {
        self.0.verify(tag)
    }

    /// Checks that the provided tag value matches the computed HMAC, truncated to the input tag's
    /// length.
    ///
    /// Truncating an HMAC reduces the security of the construction. Callers must ensure `tag`'s
    /// length matches the desired HMAC length and security level.
    pub fn verify_truncated_left(self, tag: &[u8]) -> Result<(), InvalidSignatureError> {
        self.0.verify_truncated_left(tag)
    }
}

#[cfg(feature = "std")]
impl std::io::Write for HmacSha256 {
    fn write(&mut self, buf: &[u8]) -> std::io::Result<usize> {
        self.update(buf);
        Ok(buf.len())
    }

    fn flush(&mut self) -> std::io::Result<()> {
        Ok(())
    }
}

impl Clone for HmacSha256 {
    fn clone(&self) -> Self {
        HmacSha256(self.0.clone())
    }
}

/// HMAC-SHA384.
pub struct HmacSha384(Hmac<48, Sha384>);

impl HmacSha384 {
    /// Computes the HMAC-SHA384 of `data` as a one-shot operation.
    pub fn mac(key: &[u8], data: &[u8]) -> [u8; 48] {
        hmac::<48, Sha384>(key, data)
    }

    /// Creates a new HMAC-SHA384 operation from a fixed-size key.
    pub fn new(key: &[u8; 48]) -> Self {
        Self(Hmac::new(key))
    }

    /// Creates a new HMAC-SHA384 operation from a variable-length key.
    pub fn new_from_slice(key: &[u8]) -> Self {
        Self(Hmac::new_from_slice(key))
    }

    /// Hashes the provided input into the HMAC operation.
    pub fn update(&mut self, data: &[u8]) {
        self.0.update(data)
    }

    /// Computes the final HMAC value, consuming the object.
    pub fn digest(self) -> [u8; 48] {
        self.0.digest()
    }

    /// Checks that the provided tag value matches the computed HMAC value.
    pub fn verify_slice(self, tag: &[u8]) -> Result<(), InvalidSignatureError> {
        self.0.verify_slice(tag)
    }

    /// Checks that the provided tag value matches the computed HMAC value.
    pub fn verify(self, tag: &[u8; 48]) -> Result<(), InvalidSignatureError> {
        self.0.verify(tag)
    }

    /// Checks that the provided tag value matches the computed HMAC, truncated to the input tag's
    /// length.
    ///
    /// Truncating an HMAC reduces the security of the construction. Callers must ensure `tag`'s
    /// length matches the desired HMAC length and security level.
    pub fn verify_truncated_left(self, tag: &[u8]) -> Result<(), InvalidSignatureError> {
        self.0.verify_truncated_left(tag)
    }
}

#[cfg(feature = "std")]
impl std::io::Write for HmacSha384 {
    fn write(&mut self, buf: &[u8]) -> std::io::Result<usize> {
        self.update(buf);
        Ok(buf.len())
    }

    fn flush(&mut self) -> std::io::Result<()> {
        Ok(())
    }
}

impl Clone for HmacSha384 {
    fn clone(&self) -> Self {
        HmacSha384(self.0.clone())
    }
}

/// HMAC-SHA512.
pub struct HmacSha512(Hmac<64, Sha512>);

impl HmacSha512 {
    /// Computes the HMAC-SHA512 of `data` as a one-shot operation.
    pub fn mac(key: &[u8], data: &[u8]) -> [u8; 64] {
        hmac::<64, Sha512>(key, data)
    }

    /// Creates a new HMAC-SHA512 operation from a fixed-size key.
    pub fn new(key: &[u8; 64]) -> Self {
        Self(Hmac::new(key))
    }

    /// Creates a new HMAC-SHA512 operation from a variable-length key.
    pub fn new_from_slice(key: &[u8]) -> Self {
        Self(Hmac::new_from_slice(key))
    }

    /// Hashes the provided input into the HMAC operation.
    pub fn update(&mut self, data: &[u8]) {
        self.0.update(data)
    }

    /// Computes the final HMAC value, consuming the object.
    pub fn digest(self) -> [u8; 64] {
        self.0.digest()
    }

    /// Checks that the provided tag value matches the computed HMAC value.
    pub fn verify_slice(self, tag: &[u8]) -> Result<(), InvalidSignatureError> {
        self.0.verify_slice(tag)
    }

    /// Checks that the provided tag value matches the computed HMAC value.
    pub fn verify(self, tag: &[u8; 64]) -> Result<(), InvalidSignatureError> {
        self.0.verify(tag)
    }

    /// Checks that the provided tag value matches the computed HMAC, truncated to the input tag's
    /// length.
    ///
    /// Truncating an HMAC reduces the security of the construction. Callers must ensure `tag`'s
    /// length matches the desired HMAC length and security level.
    pub fn verify_truncated_left(self, tag: &[u8]) -> Result<(), InvalidSignatureError> {
        self.0.verify_truncated_left(tag)
    }
}

#[cfg(feature = "std")]
impl std::io::Write for HmacSha512 {
    fn write(&mut self, buf: &[u8]) -> std::io::Result<usize> {
        self.update(buf);
        Ok(buf.len())
    }

    fn flush(&mut self) -> std::io::Result<()> {
        Ok(())
    }
}

impl Clone for HmacSha512 {
    fn clone(&self) -> Self {
        HmacSha512(self.0.clone())
    }
}

/// Private generically implemented function for computing HMAC as a oneshot operation.
/// This should only be exposed publicly by types with the correct output size `N` which corresponds
/// to the output size of the provided generic hash function. Ideally `N` would just come from `MD`,
/// but this is not possible until the Rust language can support the `min_const_generics` feature.
/// Until then we will have to pass both separately: https://github.com/rust-lang/rust/issues/60551
#[inline]
fn hmac<const N: usize, MD: digest::Algorithm>(key: &[u8], data: &[u8]) -> [u8; N] {
    let mut out = [0_u8; N];
    let mut size: c_uint = 0;

    // Safety:
    // - buf always contains N bytes of space
    // - If NULL is returned on error we panic immediately
    let result = unsafe {
        bssl_sys::HMAC(
            MD::get_md(sealed::SealedType).as_ptr(),
            key.as_ffi_void_ptr(),
            key.len(),
            data.as_ffi_ptr(),
            data.len(),
            out.as_mut_ffi_ptr(),
            &mut size as *mut c_uint,
        )
    };
    assert_eq!(size as usize, N);
    assert!(!result.is_null(), "Result of bssl_sys::HMAC was null");

    out
}

/// Private generically implemented HMAC instance given a generic hash function and a length `N`,
/// where `N` is the output size of the hash function. This should only be exposed publicly by
/// wrapper types with the correct output size `N` which corresponds to the output size of the
/// provided generic hash function. Ideally `N` would just come from `MD`, but this is not possible
/// until the Rust language can support the `min_const_generics` feature. Until then we will have to
/// pass both separately: https://github.com/rust-lang/rust/issues/60551
struct Hmac<const N: usize, MD: digest::Algorithm> {
    // Safety: this relies on HMAC_CTX being relocatable via `memcpy`, which is
    // not generally true of BoringSSL types. This is fine to rely on only
    // because we do not allow any version skew between bssl-crypto and
    // BoringSSL. It is *not* safe to copy this code in any other project.
    ctx: bssl_sys::HMAC_CTX,
    _marker: PhantomData<MD>,
}

impl<const N: usize, MD: digest::Algorithm> Hmac<N, MD> {
    /// Creates a new HMAC operation from a fixed-length key.
    fn new(key: &[u8; N]) -> Self {
        Self::new_from_slice(key)
    }

    /// Creates a new HMAC operation from a variable-length key.
    fn new_from_slice(key: &[u8]) -> Self {
        let mut ret = Self {
            // Safety: type checking will ensure that |ctx| is the correct size
            // for `HMAC_CTX_init`.
            ctx: unsafe { initialized_struct(|ctx| bssl_sys::HMAC_CTX_init(ctx)) },
            _marker: Default::default(),
        };

        // Safety:
        // - HMAC_Init_ex must be called with an initialized context, which
        //   `HMAC_CTX_init` provides.
        // - HMAC_Init_ex may return an error if key is null but the md is different from
        //   before. This is avoided here since key is guaranteed to be non-null.
        // - HMAC_Init_ex returns 0 on allocation failure in which case we panic
        let result = unsafe {
            bssl_sys::HMAC_Init_ex(
                &mut ret.ctx,
                key.as_ffi_void_ptr(),
                key.len(),
                MD::get_md(sealed::SealedType).as_ptr(),
                ptr::null_mut(),
            )
        };
        assert!(result > 0, "Allocation failure in bssl_sys::HMAC_Init_ex");
        ret
    }

    /// Hashes the provided input into the HMAC operation.
    fn update(&mut self, data: &[u8]) {
        // Safety: `HMAC_Update` needs an initialized context, but the only way
        // to create this object is via `new_from_slice`, which ensures that.
        let result = unsafe { bssl_sys::HMAC_Update(&mut self.ctx, data.as_ffi_ptr(), data.len()) };
        // HMAC_Update always returns 1.
        assert_eq!(result, 1, "failure in bssl_sys::HMAC_Update");
    }

    /// Computes the final HMAC value, consuming the object.
    fn digest(mut self) -> [u8; N] {
        let mut buf = [0_u8; N];
        let mut size: c_uint = 0;
        // Safety:
        // - HMAC has a fixed size output of N which will never exceed the length of an N
        // length array
        // - `HMAC_Final` needs an initialized context, but the only way
        //  to create this object is via `new_from_slice`, which ensures that.
        // - on allocation failure we panic
        let result =
            unsafe { bssl_sys::HMAC_Final(&mut self.ctx, buf.as_mut_ffi_ptr(), &mut size) };
        assert!(result > 0, "Allocation failure in bssl_sys::HMAC_Final");
        assert_eq!(size as usize, N);
        buf
    }

    /// Checks that the provided tag value matches the computed HMAC value.
    fn verify(self, tag: &[u8; N]) -> Result<(), InvalidSignatureError> {
        self.verify_slice(tag)
    }

    /// Checks that the provided tag value matches the computed HMAC value.
    ///
    /// Returns `Error` if `tag` is not valid or not equal in length
    /// to MAC's output.
    fn verify_slice(self, tag: &[u8]) -> Result<(), InvalidSignatureError> {
        if tag.len() == N {
            self.verify_truncated_left(tag)
        } else {
            Err(InvalidSignatureError)
        }
    }

    /// Checks that the provided tag value matches the computed HMAC, truncated to the input tag's
    /// length.
    ///
    /// Returns `Error` if `tag` is not valid or empty.
    ///
    /// Truncating an HMAC reduces the security of the construction. Callers must ensure `tag`'s
    /// length matches the desired HMAC length and security level.
    fn verify_truncated_left(self, tag: &[u8]) -> Result<(), InvalidSignatureError> {
        let len = tag.len();
        if len == 0 || len > N {
            return Err(InvalidSignatureError);
        }
        let calculated = self.digest();

        // Safety: both `calculated` and `tag` must be at least `len` bytes available.
        // This is true because `len` is the length of `tag` and `len` is <= N,
        // the length of `calculated`, which is checked above.
        let result = unsafe {
            bssl_sys::CRYPTO_memcmp(calculated.as_ffi_void_ptr(), tag.as_ffi_void_ptr(), len)
        };
        if result == 0 {
            Ok(())
        } else {
            Err(InvalidSignatureError)
        }
    }

    fn clone(&self) -> Self {
        let mut ret = Self {
            // Safety: type checking will ensure that |ctx| is the correct size
            // for `HMAC_CTX_init`.
            ctx: unsafe { initialized_struct(|ctx| bssl_sys::HMAC_CTX_init(ctx)) },
            _marker: Default::default(),
        };
        // Safety: `ret.ctx` is initialized and `self.ctx` is valid by
        // construction.
        let result = unsafe { bssl_sys::HMAC_CTX_copy(&mut ret.ctx, &self.ctx) };
        assert_eq!(result, 1);
        ret
    }
}

impl<const N: usize, MD: digest::Algorithm> Drop for Hmac<N, MD> {
    fn drop(&mut self) {
        unsafe { bssl_sys::HMAC_CTX_cleanup(&mut self.ctx) }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use alloc::boxed::Box;

    #[test]
    fn hmac_sha256() {
        let expected: [u8; 32] = [
            0xb0, 0x34, 0x4c, 0x61, 0xd8, 0xdb, 0x38, 0x53, 0x5c, 0xa8, 0xaf, 0xce, 0xaf, 0xb,
            0xf1, 0x2b, 0x88, 0x1d, 0xc2, 0x0, 0xc9, 0x83, 0x3d, 0xa7, 0x26, 0xe9, 0x37, 0x6c,
            0x2e, 0x32, 0xcf, 0xf7,
        ];
        let key: [u8; 20] = [0x0b; 20];
        let data = b"Hi There";

        let mut hmac = HmacSha256::new_from_slice(&key);
        hmac.update(data);
        assert_eq!(hmac.digest(), expected);

        let mut hmac = HmacSha256::new_from_slice(&key);
        hmac.update(&data[..1]);
        hmac.update(&data[1..]);
        assert_eq!(hmac.digest(), expected);

        let mut hmac = HmacSha256::new_from_slice(&key);
        hmac.update(data);
        assert!(hmac.verify(&expected).is_ok());

        let mut hmac = HmacSha256::new_from_slice(&key);
        hmac.update(data);
        assert!(hmac.verify_truncated_left(&expected[..4]).is_ok());

        let mut hmac = HmacSha256::new_from_slice(&key);
        hmac.update(data);
        assert!(hmac.verify_truncated_left(&expected[4..8]).is_err());

        let mut hmac = HmacSha256::new_from_slice(&key);
        hmac.update(&data[..1]);
        let mut hmac2 = hmac.clone();
        let mut hmac3 = Box::new(hmac2.clone());
        hmac.update(&data[1..]);
        hmac2.update(&data[1..]);
        hmac3.update(&data[1..]);
        assert_eq!(hmac.digest(), expected);
        assert_eq!(hmac2.digest(), expected);
        assert_eq!(hmac3.digest(), expected);
    }

    #[test]
    fn hmac_sha256_fixed_size_key() {
        let expected_hmac = [
            0x19, 0x8a, 0x60, 0x7e, 0xb4, 0x4b, 0xfb, 0xc6, 0x99, 0x3, 0xa0, 0xf1, 0xcf, 0x2b,
            0xbd, 0xc5, 0xba, 0xa, 0xa3, 0xf3, 0xd9, 0xae, 0x3c, 0x1c, 0x7a, 0x3b, 0x16, 0x96,
            0xa0, 0xb6, 0x8c, 0xf7,
        ];
        let key: [u8; 32] = [0x0b; 32];
        let data = b"Hi There";

        let mut hmac = HmacSha256::new(&key);
        hmac.update(data);
        let hmac_result: [u8; 32] = hmac.digest();
        assert_eq!(&hmac_result, &expected_hmac);
    }

    #[test]
    fn hmac_sha384() {
        // We test against some Wycheproof test vectors `hmac_sha384_test.json`.
        let key = &[
            238, 141, 240, 103, 133, 125, 242, 48, 15, 167, 26, 16, 195, 9, 151, 23, 139, 179, 121,
            97, 39, 181, 236, 229, 242, 204, 193, 112, 147, 43, 224, 231, 142, 169, 176, 165, 147,
            108, 9, 21, 126, 103, 28, 231, 236, 159, 197, 16,
        ];
        let mut hmac = HmacSha384::new_from_slice(key);
        hmac.update(b"");
        let hmac_result: [u8; 48] = hmac.digest();
        assert_eq!(
            hmac_result,
            [
                166, 85, 24, 77, 175, 51, 70, 255, 198, 98, 157, 73, 60, 132, 66, 100, 78, 73, 150,
                162, 121, 158, 66, 227, 48, 111, 166, 245, 176, 150, 123, 108, 243, 166, 248, 25,
                186, 184, 155, 206, 41, 125, 29, 26, 89, 7, 178, 208
            ]
        );

        let key = &[
            151, 102, 150, 192, 220, 151, 24, 44, 167, 113, 151, 92, 57, 40, 255, 145, 104, 239,
            137, 205, 116, 12, 210, 41, 40, 88, 253, 145, 96, 104, 167, 2, 188, 29, 247, 198, 205,
            142, 225, 240, 210, 94, 97, 212, 197, 20, 204, 93,
        ];
        hmac = HmacSha384::new_from_slice(key);
        hmac.update(&[43]);
        let hmac_result: [u8; 48] = hmac.digest();
        assert_eq!(
            hmac_result,
            [
                54, 62, 137, 115, 254, 220, 247, 137, 32, 19, 223, 174, 11, 112, 101, 214, 29, 128,
                185, 140, 99, 91, 192, 158, 216, 96, 160, 20, 115, 185, 188, 208, 220, 85, 13, 191,
                102, 207, 13, 96, 31, 233, 203, 243, 174, 89, 98, 13
            ]
        );

        let key = &[
            188, 49, 11, 195, 145, 61, 159, 229, 158, 32, 18, 160, 88, 201, 225, 80, 83, 77, 37,
            97, 30, 54, 32, 108, 240, 124, 202, 239, 225, 83, 243, 142, 176, 234, 173, 153, 65,
            182, 136, 61, 251, 206, 1, 188, 181, 25, 96, 65,
        ];
        hmac = HmacSha384::new_from_slice(key);
        let msg = &[
            159, 7, 71, 215, 57, 107, 251, 224, 28, 243, 232, 83, 97, 229, 0, 133, 224, 169, 26,
            116, 144, 185, 148, 3, 29, 129, 133, 27, 114, 80, 101, 153, 63, 69, 218, 208, 214, 13,
            121, 74, 237, 236, 123, 165, 217, 214, 219, 190, 228,
        ];
        hmac.update(&msg[..20]);
        hmac.update(&msg[20..]);
        let hmac_result: [u8; 48] = hmac.digest();
        assert_eq!(
            hmac_result,
            [
                58, 134, 73, 143, 120, 195, 251, 126, 179, 183, 179, 216, 47, 103, 125, 45, 254, 1,
                22, 111, 231, 110, 35, 32, 131, 51, 77, 116, 241, 21, 136, 253, 8, 150, 55, 201,
                71, 97, 233, 207, 232, 54, 67, 96, 5, 222, 174, 247
            ]
        );
    }

    #[test]
    fn hmac_sha512() {
        let expected: [u8; 64] = [
            135, 170, 124, 222, 165, 239, 97, 157, 79, 240, 180, 36, 26, 29, 108, 176, 35, 121,
            244, 226, 206, 78, 194, 120, 122, 208, 179, 5, 69, 225, 124, 222, 218, 168, 51, 183,
            214, 184, 167, 2, 3, 139, 39, 78, 174, 163, 244, 228, 190, 157, 145, 78, 235, 97, 241,
            112, 46, 105, 108, 32, 58, 18, 104, 84,
        ];
        let key: [u8; 20] = [0x0b; 20];
        let data = b"Hi There";

        let mut hmac = HmacSha512::new_from_slice(&key);
        hmac.update(data);
        assert_eq!(hmac.digest(), expected);

        let mut hmac = HmacSha512::new_from_slice(&key);
        hmac.update(&data[..1]);
        hmac.update(&data[1..]);
        assert_eq!(hmac.digest(), expected);

        let mut hmac = HmacSha512::new_from_slice(&key);
        hmac.update(data);
        assert!(hmac.verify(&expected).is_ok());

        let mut hmac = HmacSha512::new_from_slice(&key);
        hmac.update(data);
        assert!(hmac.verify_truncated_left(&expected[..4]).is_ok());

        let mut hmac = HmacSha512::new_from_slice(&key);
        hmac.update(data);
        assert!(hmac.verify_truncated_left(&expected[4..8]).is_err());

        let mut hmac = HmacSha512::new_from_slice(&key);
        hmac.update(&data[..1]);
        let mut hmac2 = hmac.clone();
        let mut hmac3 = Box::new(hmac.clone());
        hmac.update(&data[1..]);
        hmac2.update(&data[1..]);
        hmac3.update(&data[1..]);
        assert_eq!(hmac.digest(), expected);
        assert_eq!(hmac2.digest(), expected);
        assert_eq!(hmac3.digest(), expected);
    }
}
