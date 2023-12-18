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

//! `EcKey` and `EcGroup` structs for working with elliptic curve cryptography. This module is
//! intended for internal use within this crate only, to create higher-level abstractions suitable
//! to be exposed externally.

use alloc::borrow::ToOwned;
use alloc::vec;
use alloc::vec::Vec;
use core::panic;
use core::{borrow::Borrow, fmt::Debug, ops::Deref};

use crate::{bn::BigNum, CSlice, CSliceMut, ForeignType, ForeignTypeRef};

#[derive(Debug)]
pub(crate) struct EcKey {
    ptr: *mut bssl_sys::EC_KEY,
}

// Safety: Implementation ensures `from_ptr(x).as_ptr() == x`
unsafe impl ForeignType for EcKey {
    type CType = bssl_sys::EC_KEY;

    unsafe fn from_ptr(ptr: *mut Self::CType) -> Self {
        Self { ptr }
    }

    fn as_ptr(&self) -> *mut Self::CType {
        self.ptr
    }
}

// Safety:
// - `EC_KEY`'s documentation says "A given object may be used concurrently on multiple threads by
//   non-mutating functions, provided no other thread is concurrently calling a mutating function.",
//   which matches Rust's aliasing rules.
// - `ptr(&self)` and `ptr_mut(&mut self)` ensures that only a mutable reference can get a mutable
//   `EC_KEY` pointer outside of this module.
unsafe impl Send for EcKey {}

impl Clone for EcKey {
    fn clone(&self) -> Self {
        // Safety:
        // - EcKey makes sure self.ptr is a valid pointer.
        let ptr = unsafe { bssl_sys::EC_KEY_dup(self.ptr) };
        Self { ptr }
    }
}

/// Error type returned when conversion to or from an `EcKey` failed.
pub(crate) struct ConversionFailed;

impl EcKey {
    pub fn new_by_ec_group(ec_group: &EcGroupRef) -> Self {
        // Safety: `EC_KEY_new` does not have preconditions
        let eckey = unsafe { bssl_sys::EC_KEY_new() };
        assert!(!eckey.is_null());
        // Safety:
        // - `eckey` is just allocated and doesn't have its group set yet
        // - `EcGroup` ensures the `ptr` it contains is valid
        unsafe {
            assert_eq!(
                bssl_sys::EC_KEY_set_group(eckey, ec_group.as_ptr()),
                1,
                "EC_KEY_set_group failed"
            );
        }
        // Safety: `eckey` is allocated and null-checked
        unsafe { Self::from_ptr(eckey) }
    }

    /// Try to create a public-key version of `EcKey` from the given `value`. Returns error if the
    /// slice is not a valid representation of a public key for the given curve.
    ///
    /// `curve_nid` should be a value defined in `bssl_sys::NID_*`.
    #[allow(clippy::panic)]
    pub(crate) fn try_new_public_key_from_bytes(
        ec_group: &EcGroupRef,
        value: &[u8],
    ) -> Result<Self, ConversionFailed> {
        let eckey = Self::new_by_ec_group(ec_group);
        let value_ffi = CSlice(value);

        // Safety: The input slice `value_ffi` is a CSlice from safe Rust.
        let result = unsafe {
            bssl_sys::EC_KEY_oct2key(
                eckey.ptr,
                value_ffi.as_ptr(),
                value_ffi.len(),
                core::ptr::null_mut(),
            )
        };
        match result {
            0 => Err(ConversionFailed),
            1 => Ok(eckey),
            _ => panic!("Unexpected return value {result} from EC_KEY_oct2key"),
        }
    }

    pub(crate) fn to_affine_coordinates(&self) -> (BigNum, BigNum) {
        let ecpoint = unsafe { bssl_sys::EC_KEY_get0_public_key(self.ptr) };
        let bn_x = BigNum::new();
        let bn_y = BigNum::new();

        // Safety:
        // - `EcKey` and `BigNum` structs ensures validity of their pointers.
        let result = unsafe {
            bssl_sys::EC_POINT_get_affine_coordinates(
                bssl_sys::EC_KEY_get0_group(self.ptr),
                ecpoint,
                bn_x.as_ptr(),
                bn_y.as_ptr(),
                core::ptr::null_mut(),
            )
        };
        assert_eq!(
            result, 1,
            "bssl_sys::EC_POINT_get_affine_coordinates failed"
        );
        (bn_x, bn_y)
    }

    pub(crate) fn generate(ec_group: &EcGroupRef) -> Self {
        let eckey = EcKey::new_by_ec_group(ec_group);
        // Safety: `EcKey` ensures eckey.ptr is valid.
        let result = unsafe { bssl_sys::EC_KEY_generate_key(eckey.as_ptr()) };
        assert_eq!(result, 1, "bssl_sys::EC_KEY_generate_key failed");
        eckey
    }

    pub(crate) fn try_new_public_key_from_affine_coordinates(
        ec_group: &EcGroupRef,
        x: &[u8],
        y: &[u8],
    ) -> Result<Self, ConversionFailed> {
        let bn_x = BigNum::from(x);
        let bn_y = BigNum::from(y);

        let eckey = EcKey::new_by_ec_group(ec_group);
        // Safety:
        // - Wrapper classes `EcKey` and `BigNum` ensures validity of the pointers
        let result = unsafe {
            bssl_sys::EC_KEY_set_public_key_affine_coordinates(
                eckey.as_ptr(),
                bn_x.as_ptr(),
                bn_y.as_ptr(),
            )
        };
        if result == 1 {
            Ok(eckey)
        } else {
            Err(ConversionFailed)
        }
    }

    /// Tries to convert the given bytes into a private key contained within `EcKey`.
    ///
    /// `private_key_bytes` must be padded to the size of `curve_nid`'s group order, otherwise the
    /// conversion will fail.
    pub(crate) fn try_from_raw_bytes(
        ec_group: &EcGroupRef,
        private_key_bytes: &[u8],
    ) -> Result<Self, ConversionFailed> {
        let eckey = EcKey::new_by_ec_group(ec_group);
        let private_key_bytes_ffi = CSlice(private_key_bytes);
        // Safety:
        // - `EcKey` ensures `eckey.ptr` is valid.
        // - `private_key_bytes` is a CSlice from safe-rust.
        let result = unsafe {
            bssl_sys::EC_KEY_oct2priv(
                eckey.as_ptr(),
                private_key_bytes_ffi.as_ptr(),
                private_key_bytes_ffi.len(),
            )
        };
        if result != 1 {
            return Err(ConversionFailed);
        }

        Ok(eckey)
    }

    /// Converts between the private key component of `eckey` and octet form. The octet form
    /// consists of the content octets of the `privateKey` `OCTET STRING` in an `ECPrivateKey` ASN.1
    /// structure
    pub(crate) fn to_raw_bytes(&self) -> Vec<u8> {
        let mut output = vec![0_u8; 66];
        let mut private_key_bytes_ffi = CSliceMut::from(&mut output[..]);
        // Safety:
        // - `EcKey` ensures `self.ptr` is valid.
        // - `private_key_bytes_ffi` is a CSliceMut we just allocated.
        // - 66 bytes is guaranteed to be sufficient to store an EC private key
        let num_octets_stored = unsafe {
            bssl_sys::EC_KEY_priv2oct(
                self.as_ptr(),
                private_key_bytes_ffi.as_mut_ptr(),
                private_key_bytes_ffi.len(),
            )
        };
        // Safety: `EC_KEY_priv2oct` just wrote `num_octets_stored` into the buffer.
        unsafe { output.set_len(num_octets_stored) }
        output
    }

    pub(crate) fn public_key_eq(&self, other: &Self) -> bool {
        let result = unsafe {
            bssl_sys::EC_POINT_cmp(
                bssl_sys::EC_KEY_get0_group(self.ptr),
                bssl_sys::EC_KEY_get0_public_key(self.ptr),
                bssl_sys::EC_KEY_get0_public_key(other.ptr),
                core::ptr::null_mut(),
            )
        };
        assert_ne!(result, -1, "bssl_sys::EC_POINT_cmp failed");
        result == 0
    }

    pub(crate) fn to_vec(&self) -> Vec<u8> {
        // Safety: `self.ptr` is owned by `self`
        let ecgroup = unsafe { bssl_sys::EC_KEY_get0_group(self.ptr) };
        let ecpoint = unsafe { bssl_sys::EC_KEY_get0_public_key(self.ptr) };
        let conv_form = unsafe { bssl_sys::EC_KEY_get_conv_form(self.ptr) };
        // Safety:
        // - When passing null to EC_POINT_point2oct's `buf` argument, it returns the size of the
        //   resulting buffer.
        let output_size = unsafe {
            bssl_sys::EC_POINT_point2oct(
                ecgroup,
                ecpoint,
                conv_form,
                core::ptr::null_mut(),
                0,
                core::ptr::null_mut(),
            )
        };
        assert_ne!(output_size, 0, "bssl_sys::EC_POINT_point2oct failed");
        let mut result_vec = Vec::<u8>::with_capacity(output_size);
        let buf_len = unsafe {
            bssl_sys::EC_POINT_point2oct(
                ecgroup,
                ecpoint,
                conv_form,
                result_vec.as_mut_ptr(),
                output_size,
                core::ptr::null_mut(),
            )
        };
        assert_ne!(buf_len, 0, "bssl_sys::EC_POINT_point2oct failed");
        // Safety: The length is what EC_POINT_point2oct just told us it filled into the buffer.
        unsafe { result_vec.set_len(buf_len) }
        result_vec
    }
}

impl Drop for EcKey {
    fn drop(&mut self) {
        // Safety: `self.ptr` is owned by this struct
        unsafe { bssl_sys::EC_KEY_free(self.ptr) }
    }
}

/// Describes an elliptic curve.
#[non_exhaustive]
pub struct EcGroupRef;

// Safety: Default implementation in ForeignTypeRef ensures the preconditions
//   required by that trait holds.
unsafe impl ForeignTypeRef for EcGroupRef {
    type CType = bssl_sys::EC_GROUP;
}

impl Borrow<EcGroupRef> for EcGroup {
    fn borrow(&self) -> &EcGroupRef {
        unsafe { EcGroupRef::from_ptr(self.ptr) }
    }
}

impl ToOwned for EcGroupRef {
    type Owned = EcGroup;

    fn to_owned(&self) -> Self::Owned {
        // Safety: `EcGroupRef` is a valid pointer
        let new_ec_group = unsafe { bssl_sys::EC_GROUP_dup(self.as_ptr()) };
        assert!(!new_ec_group.is_null(), "EC_GROUP_dup failed");
        EcGroup { ptr: new_ec_group }
    }
}

impl AsRef<EcGroupRef> for EcGroup {
    fn as_ref(&self) -> &EcGroupRef {
        self.deref()
    }
}

impl PartialEq for EcGroupRef {
    fn eq(&self, other: &Self) -> bool {
        // Safety:
        // - Self and other are valid pointers since they come from `EcGroupRef`
        // - Third argument is ignored
        unsafe {
            bssl_sys::EC_GROUP_cmp(
                self.as_ptr(),
                other.as_ptr(),
                /* ignored */ core::ptr::null_mut(),
            ) == 0
        }
    }
}

impl Eq for EcGroupRef {}

pub struct EcGroup {
    ptr: *mut bssl_sys::EC_GROUP,
}

impl Deref for EcGroup {
    type Target = EcGroupRef;

    fn deref(&self) -> &Self::Target {
        unsafe { EcGroupRef::from_ptr(self.ptr) }
    }
}

impl Drop for EcGroup {
    fn drop(&mut self) {
        unsafe { bssl_sys::EC_GROUP_free(self.ptr) }
    }
}

/// An elliptic curve, used as the type parameter for [`PublicKey`] and [`PrivateKey`].
pub trait Curve: Debug {
    /// The size of the affine coordinates for this curve.
    const AFFINE_COORDINATE_SIZE: usize;

    /// Create a new [`EcGroup`] for this curve.
    fn ec_group() -> &'static EcGroupRef;
}

/// The P-224 curve, corresponding to `NID_secp224r1`.
#[derive(Debug)]
pub struct P224;

impl Curve for P224 {
    const AFFINE_COORDINATE_SIZE: usize = 28;

    fn ec_group() -> &'static EcGroupRef {
        // Safety: EC_group_p224 does not have any preconditions
        unsafe { EcGroupRef::from_ptr(bssl_sys::EC_group_p224() as *mut _) }
    }
}

/// The P-256 curve, corresponding to `NID_X9_62_prime256v1`.
#[derive(Debug)]
pub struct P256;

impl Curve for P256 {
    const AFFINE_COORDINATE_SIZE: usize = 32;

    fn ec_group() -> &'static EcGroupRef {
        // Safety: EC_group_p256 does not have any preconditions
        unsafe { EcGroupRef::from_ptr(bssl_sys::EC_group_p256() as *mut _) }
    }
}

/// The P-384 curve, corresponding to `NID_secp384r1`.
#[derive(Debug)]
pub struct P384;

impl Curve for P384 {
    const AFFINE_COORDINATE_SIZE: usize = 48;

    fn ec_group() -> &'static EcGroupRef {
        // Safety: EC_group_p384 does not have any preconditions
        unsafe { EcGroupRef::from_ptr(bssl_sys::EC_group_p384() as *mut _) }
    }
}

/// The P-521 curve, corresponding to `NID_secp521r1`.
#[derive(Debug)]
pub struct P521;

impl Curve for P521 {
    const AFFINE_COORDINATE_SIZE: usize = 66;

    fn ec_group() -> &'static EcGroupRef {
        // Safety: EC_group_p521 does not have any preconditions
        unsafe { EcGroupRef::from_ptr(bssl_sys::EC_group_p521() as *mut _) }
    }
}

#[cfg(test)]
mod test {
    use crate::ec::P521;

    use super::{Curve, EcGroupRef, P256};

    #[test]
    fn test_ec_group_clone_and_eq() {
        let group = P256::ec_group();
        let group_clone = group.to_owned();
        let group2: &EcGroupRef = &group_clone;
        assert!(group == group2);
    }

    #[test]
    fn test_ec_group_not_equal() {
        let group = P256::ec_group();
        let group2 = P521::ec_group();
        assert!(group != group2)
    }
}
