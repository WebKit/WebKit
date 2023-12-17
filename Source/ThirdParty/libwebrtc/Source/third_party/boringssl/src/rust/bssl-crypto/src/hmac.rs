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
use crate::{
    digest::{Md, Sha256, Sha512},
    CSlice, ForeignTypeRef as _,
};
use core::{
    ffi::{c_uint, c_void},
    marker::PhantomData,
    ptr,
};

/// Computes the HMAC-SHA256 of `data` as a one-shot operation.
///
/// Calculates the HMAC of data, using the given `key` and returns the result.
/// It returns the computed HMAC.
pub fn hmac_sha256(key: &[u8], data: &[u8]) -> [u8; 32] {
    hmac::<32, Sha256>(key, data)
}

/// Computes the HMAC-SHA512 of `data` as a one-shot operation.
///
/// Calculates the HMAC of data, using the given `key` and returns the result.
/// It returns the computed HMAC.
pub fn hmac_sha512(key: &[u8], data: &[u8]) -> [u8; 64] {
    hmac::<64, Sha512>(key, data)
}

/// An HMAC-SHA256 operation in progress.
pub struct HmacSha256(Hmac<32, Sha256>);

impl HmacSha256 {
    /// Creates a new HMAC operation from a fixed-length key.
    pub fn new(key: [u8; 32]) -> Self {
        Self(Hmac::new(key))
    }

    /// Creates a new HMAC operation from a variable-length key.
    pub fn new_from_slice(key: &[u8]) -> Self {
        Self(Hmac::new_from_slice(key))
    }

    /// Hashes the provided input into the HMAC operation.
    pub fn update(&mut self, data: &[u8]) {
        self.0.update(data)
    }

    /// Computes the final HMAC value, consuming the object.
    pub fn finalize(self) -> [u8; 32] {
        self.0.finalize()
    }

    /// Checks that the provided tag value matches the computed HMAC value.
    pub fn verify_slice(self, tag: &[u8]) -> Result<(), MacError> {
        self.0.verify_slice(tag)
    }

    /// Checks that the provided tag value matches the computed HMAC value.
    pub fn verify(self, tag: [u8; 32]) -> Result<(), MacError> {
        self.0.verify(tag)
    }

    /// Checks that the provided tag value matches the computed HMAC, truncated to the input tag's
    /// length.
    ///
    /// Truncating an HMAC reduces the security of the construction. Callers must ensure `tag`'s
    /// length matches the desired HMAC length and security level.
    pub fn verify_truncated_left(self, tag: &[u8]) -> Result<(), MacError> {
        self.0.verify_truncated_left(tag)
    }

    /// Resets the object to its initial state. The key is retained, but input is discarded.
    pub fn reset(&mut self) {
        // TODO(davidben): This method is a little odd. The main use case I can imagine is
        // computing multiple HMACs with the same key, while reusing the input-independent key
        // setup. However, finalize consumes the object, so you cannot actually reuse the
        // context afterwards. Moreover, even if you could, that mutates the context, so a
        // better pattern would be to copy the initial context, or to have an HmacKey type
        // that one could create contexts out of.
        self.0.reset()
    }
}

/// An HMAC-SHA512 operation in progress.
pub struct HmacSha512(Hmac<64, Sha512>);

impl HmacSha512 {
    /// Creates a new HMAC operation from a fixed-size key.
    pub fn new(key: [u8; 64]) -> Self {
        Self(Hmac::new(key))
    }

    /// Creates a new HMAC operation from a variable-length key.
    pub fn new_from_slice(key: &[u8]) -> Self {
        Self(Hmac::new_from_slice(key))
    }

    /// Hashes the provided input into the HMAC operation.
    pub fn update(&mut self, data: &[u8]) {
        self.0.update(data)
    }

    /// Computes the final HMAC value, consuming the object.
    pub fn finalize(self) -> [u8; 64] {
        self.0.finalize()
    }

    /// Checks that the provided tag value matches the computed HMAC value.
    pub fn verify_slice(self, tag: &[u8]) -> Result<(), MacError> {
        self.0.verify_slice(tag)
    }

    /// Checks that the provided tag value matches the computed HMAC value.
    pub fn verify(self, tag: [u8; 64]) -> Result<(), MacError> {
        self.0.verify(tag)
    }

    /// Checks that the provided tag value matches the computed HMAC, truncated to the input tag's
    /// length.
    ///
    /// Truncating an HMAC reduces the security of the construction. Callers must ensure `tag`'s
    /// length matches the desired HMAC length and security level.
    pub fn verify_truncated_left(self, tag: &[u8]) -> Result<(), MacError> {
        self.0.verify_truncated_left(tag)
    }

    /// Resets the object to its initial state. The key is retained, but input is discarded.
    pub fn reset(&mut self) {
        // TODO(davidben): This method is a little odd. The main use case I can imagine is
        // computing multiple HMACs with the same key, while reusing the input-independent key
        // setup. However, finalize consumes the object, so you cannot actually reuse the
        // context afterwards. Moreover, even if you could, that mutates the context, so a
        // better pattern would be to copy the initial context, or to have an HmacKey type
        // that one could create contexts out of.
        self.0.reset()
    }
}

/// Error type for when the output of the hmac operation is not equal to the expected value.
#[derive(Debug)]
pub struct MacError;

/// Private generically implemented function for computing hmac as a oneshot operation.
/// This should only be exposed publicly by types with the correct output size `N` which corresponds
/// to the output size of the provided generic hash function. Ideally `N` would just come from `M`,
/// but this is not possible until the Rust language can support the `min_const_generics` feature.
/// Until then we will have to pass both separately: https://github.com/rust-lang/rust/issues/60551
#[inline]
fn hmac<const N: usize, M: Md>(key: &[u8], data: &[u8]) -> [u8; N] {
    let mut out = [0_u8; N];
    let mut size: c_uint = 0;

    // Safety:
    // - buf always contains N bytes of space
    // - If NULL is returned on error we panic immediately
    let result = unsafe {
        bssl_sys::HMAC(
            M::get_md().as_ptr(),
            CSlice::from(key).as_ptr(),
            key.len(),
            CSlice::from(data).as_ptr(),
            data.len(),
            out.as_mut_ptr(),
            &mut size as *mut c_uint,
        )
    };
    assert!(!result.is_null(), "Result of bssl_sys::HMAC was null");

    out
}

/// Private generically implemented hmac  instance given a generic hash function and a length `N`,
/// where `N` is the output size of the hash function. This should only be exposed publicly by
/// wrapper types with the correct output size `N` which corresponds to the output size of the
/// provided generic hash function. Ideally `N` would just come from `M`, but this is not possible
/// until the Rust language can support the `min_const_generics` feature. Until then we will have to
/// pass both separately: https://github.com/rust-lang/rust/issues/60551
struct Hmac<const N: usize, M: Md> {
    ctx: *mut bssl_sys::HMAC_CTX,
    _marker: PhantomData<M>,
}

impl<const N: usize, M: Md> Hmac<N, M> {
    /// Creates a new HMAC operation from a fixed-length key.
    fn new(key: [u8; N]) -> Self {
        Self::new_from_slice(&key)
    }

    /// Creates a new HMAC operation from a variable-length key.
    fn new_from_slice(key: &[u8]) -> Self {
        // Safety:
        // - HMAC_CTX_new panics if allocation fails
        let ctx = unsafe { bssl_sys::HMAC_CTX_new() };
        assert!(
            !ctx.is_null(),
            "result of bssl_sys::HMAC_CTX_new() was null"
        );

        // Safety:
        // - HMAC_Init_ex must be called with a context previously created with HMAC_CTX_new,
        //   which is the line above.
        // - HMAC_Init_ex may return an error if key is null but the md is different from
        //   before. This is avoided here since key is guaranteed to be non-null.
        // - HMAC_Init_ex returns 0 on allocation failure in which case we panic
        let result = unsafe {
            bssl_sys::HMAC_Init_ex(
                ctx,
                CSlice::from(key).as_ptr() as *const c_void,
                key.len(),
                M::get_md().as_ptr(),
                ptr::null_mut(),
            )
        };
        assert!(result > 0, "Allocation failure in bssl_sys::HMAC_Init_ex");

        Self {
            ctx,
            _marker: Default::default(),
        }
    }

    /// Hashes the provided input into the HMAC operation.
    fn update(&mut self, data: &[u8]) {
        let result = unsafe {
            // Safety: HMAC_Update will always return 1, in case it doesnt we panic
            bssl_sys::HMAC_Update(self.ctx, data.as_ptr(), data.len())
        };
        assert_eq!(result, 1, "failure in bssl_sys::HMAC_Update");
    }

    /// Computes the final HMAC value, consuming the object.
    fn finalize(self) -> [u8; N] {
        let mut buf = [0_u8; N];
        let mut size: c_uint = 0;
        // Safety:
        // - hmac has a fixed size output of N which will never exceed the length of an N
        // length array
        // - on allocation failure we panic
        let result =
            unsafe { bssl_sys::HMAC_Final(self.ctx, buf.as_mut_ptr(), &mut size as *mut c_uint) };
        assert!(result > 0, "Allocation failure in bssl_sys::HMAC_Final");
        buf
    }

    /// Checks that the provided tag value matches the computed HMAC value.
    fn verify(self, tag: [u8; N]) -> Result<(), MacError> {
        self.verify_slice(&tag)
    }

    /// Checks that the provided tag value matches the computed HMAC value.
    ///
    /// Returns `Error` if `tag` is not valid or not equal in length
    /// to MAC's output.
    fn verify_slice(self, tag: &[u8]) -> Result<(), MacError> {
        tag.len().eq(&N).then_some(()).ok_or(MacError)?;
        self.verify_truncated_left(tag)
    }

    /// Checks that the provided tag value matches the computed HMAC, truncated to the input tag's
    /// length.
    ///
    /// Returns `Error` if `tag` is not valid or empty.
    ///
    /// Truncating an HMAC reduces the security of the construction. Callers must ensure `tag`'s
    /// length matches the desired HMAC length and security level.
    fn verify_truncated_left(self, tag: &[u8]) -> Result<(), MacError> {
        let len = tag.len();
        if len == 0 || len > N {
            return Err(MacError);
        }

        let result = &self.finalize()[..len];

        // Safety:
        // - if a != b is undefined, it simply returns a non-zero result
        unsafe {
            bssl_sys::CRYPTO_memcmp(
                CSlice::from(result).as_ptr() as *const c_void,
                CSlice::from(tag).as_ptr() as *const c_void,
                result.len(),
            )
        }
        .eq(&0)
        .then_some(())
        .ok_or(MacError)
    }

    /// Resets the hmac instance to its original state
    fn reset(&mut self) {
        // Passing a null ptr for the key will re-use the existing key
        // Safety:
        // - HMAC_Init_ex must be called with a context previously created with HMAC_CTX_new,
        //   which will always be the case if it is coming from self
        // - HMAC_Init_ex may return an error if key is null but the md is different from
        //   before. The MD is guaranteed to be the same because it comes from the same generic param
        // - HMAC_Init_ex returns 0 on allocation failure in which case we panic
        let result = unsafe {
            bssl_sys::HMAC_Init_ex(
                self.ctx,
                ptr::null_mut(),
                0,
                M::get_md().as_ptr(),
                ptr::null_mut(),
            )
        };
        assert!(result > 0, "Allocation failure in bssl_sys::HMAC_Init_ex");
    }
}

impl<const N: usize, M: Md> Drop for Hmac<N, M> {
    fn drop(&mut self) {
        unsafe { bssl_sys::HMAC_CTX_free(self.ctx) }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn hmac_sha256_test() {
        let expected_hmac = [
            0xb0, 0x34, 0x4c, 0x61, 0xd8, 0xdb, 0x38, 0x53, 0x5c, 0xa8, 0xaf, 0xce, 0xaf, 0xb,
            0xf1, 0x2b, 0x88, 0x1d, 0xc2, 0x0, 0xc9, 0x83, 0x3d, 0xa7, 0x26, 0xe9, 0x37, 0x6c,
            0x2e, 0x32, 0xcf, 0xf7,
        ];

        let key: [u8; 20] = [0x0b; 20];
        let data = b"Hi There";

        let mut hmac = HmacSha256::new_from_slice(&key);
        hmac.update(data);
        let hmac_result: [u8; 32] = hmac.finalize();

        assert_eq!(&hmac_result, &expected_hmac);
    }

    #[test]
    fn hmac_sha256_reset_test() {
        let expected_hmac = [
            0xb0, 0x34, 0x4c, 0x61, 0xd8, 0xdb, 0x38, 0x53, 0x5c, 0xa8, 0xaf, 0xce, 0xaf, 0xb,
            0xf1, 0x2b, 0x88, 0x1d, 0xc2, 0x0, 0xc9, 0x83, 0x3d, 0xa7, 0x26, 0xe9, 0x37, 0x6c,
            0x2e, 0x32, 0xcf, 0xf7,
        ];

        let key: [u8; 20] = [0x0b; 20];
        let data = b"Hi There";
        let incorrect_data = b"This data does not match the expected mac";

        let mut hmac = HmacSha256::new_from_slice(&key);
        hmac.update(incorrect_data);
        hmac.reset();

        // hmac should be back to original state, so now when we update with the correct data it
        // should work
        hmac.update(data);
        let hmac_result: [u8; 32] = hmac.finalize();
        assert_eq!(&hmac_result, &expected_hmac);
    }

    #[test]
    fn hmac_sha256_fixed_size_key_test() {
        let expected_hmac = [
            0x19, 0x8a, 0x60, 0x7e, 0xb4, 0x4b, 0xfb, 0xc6, 0x99, 0x3, 0xa0, 0xf1, 0xcf, 0x2b,
            0xbd, 0xc5, 0xba, 0xa, 0xa3, 0xf3, 0xd9, 0xae, 0x3c, 0x1c, 0x7a, 0x3b, 0x16, 0x96,
            0xa0, 0xb6, 0x8c, 0xf7,
        ];

        let key: [u8; 32] = [0x0b; 32];
        let data = b"Hi There";

        let mut hmac = HmacSha256::new(key);
        hmac.update(data);
        let hmac_result: [u8; 32] = hmac.finalize();
        assert_eq!(&hmac_result, &expected_hmac);
    }

    #[test]
    fn hmac_sha256_update_test() {
        let expected_hmac = [
            0xb0, 0x34, 0x4c, 0x61, 0xd8, 0xdb, 0x38, 0x53, 0x5c, 0xa8, 0xaf, 0xce, 0xaf, 0xb,
            0xf1, 0x2b, 0x88, 0x1d, 0xc2, 0x0, 0xc9, 0x83, 0x3d, 0xa7, 0x26, 0xe9, 0x37, 0x6c,
            0x2e, 0x32, 0xcf, 0xf7,
        ];
        let key: [u8; 20] = [0x0b; 20];
        let data = b"Hi There";
        let mut hmac: HmacSha256 = HmacSha256::new_from_slice(&key);
        hmac.update(data);
        let result = hmac.finalize();
        assert_eq!(&result, &expected_hmac);
        assert_eq!(result.len(), 32);
    }

    #[test]
    fn hmac_sha256_test_big_buffer() {
        let expected_hmac = [
            0xb0, 0x34, 0x4c, 0x61, 0xd8, 0xdb, 0x38, 0x53, 0x5c, 0xa8, 0xaf, 0xce, 0xaf, 0xb,
            0xf1, 0x2b, 0x88, 0x1d, 0xc2, 0x0, 0xc9, 0x83, 0x3d, 0xa7, 0x26, 0xe9, 0x37, 0x6c,
            0x2e, 0x32, 0xcf, 0xf7,
        ];
        let key: [u8; 20] = [0x0b; 20];
        let data = b"Hi There";
        let hmac_result = hmac_sha256(&key, data);
        assert_eq!(&hmac_result, &expected_hmac);
    }

    #[test]
    fn hmac_sha256_update_chunks_test() {
        let expected_hmac = [
            0xb0, 0x34, 0x4c, 0x61, 0xd8, 0xdb, 0x38, 0x53, 0x5c, 0xa8, 0xaf, 0xce, 0xaf, 0xb,
            0xf1, 0x2b, 0x88, 0x1d, 0xc2, 0x0, 0xc9, 0x83, 0x3d, 0xa7, 0x26, 0xe9, 0x37, 0x6c,
            0x2e, 0x32, 0xcf, 0xf7,
        ];
        let key: [u8; 20] = [0x0b; 20];
        let mut hmac = HmacSha256::new_from_slice(&key);
        hmac.update(b"Hi");
        hmac.update(b" There");
        let result = hmac.finalize();
        assert_eq!(&result, &expected_hmac);
    }

    #[test]
    fn hmac_sha256_verify_test() {
        let expected_hmac = [
            0xb0, 0x34, 0x4c, 0x61, 0xd8, 0xdb, 0x38, 0x53, 0x5c, 0xa8, 0xaf, 0xce, 0xaf, 0xb,
            0xf1, 0x2b, 0x88, 0x1d, 0xc2, 0x0, 0xc9, 0x83, 0x3d, 0xa7, 0x26, 0xe9, 0x37, 0x6c,
            0x2e, 0x32, 0xcf, 0xf7,
        ];
        let key: [u8; 20] = [0x0b; 20];
        let data = b"Hi There";
        let mut hmac: HmacSha256 = HmacSha256::new_from_slice(&key);
        hmac.update(data);
        assert!(hmac.verify(expected_hmac).is_ok())
    }
}
