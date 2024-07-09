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

//! Definitions of NIST elliptic curves.
//!
//! If you're looking for curve25519, see the `x25519` and `ed25519` modules.

// This module is substantially internal-only and is only public for the
// [`Curve`] trait, which is shared by ECDH and ECDSA.

use crate::{cbb_to_buffer, parse_with_cbs, scoped, sealed, Buffer, FfiSlice};
use alloc::{fmt::Debug, vec::Vec};
use core::ptr::{null, null_mut};

/// An elliptic curve.
pub trait Curve: Debug {
    #[doc(hidden)]
    fn group(_: sealed::Sealed) -> Group;

    /// Hash `data` using a hash function suitable for the curve. (I.e.
    /// SHA-256 for P-256 and SHA-384 for P-384.)
    #[doc(hidden)]
    fn hash(data: &[u8]) -> Vec<u8>;
}

/// The NIST P-256 curve, also called secp256r1.
#[derive(Debug)]
pub struct P256;

impl Curve for P256 {
    fn group(_: sealed::Sealed) -> Group {
        Group::P256
    }

    fn hash(data: &[u8]) -> Vec<u8> {
        crate::digest::Sha256::hash(data).to_vec()
    }
}

/// The NIST P-384 curve, also called secp384r1.
#[derive(Debug)]
pub struct P384;

impl Curve for P384 {
    fn group(_: sealed::Sealed) -> Group {
        Group::P384
    }

    fn hash(data: &[u8]) -> Vec<u8> {
        crate::digest::Sha384::hash(data).to_vec()
    }
}

#[derive(Copy, Clone)]
#[doc(hidden)]
pub enum Group {
    P256,
    P384,
}

impl Group {
    fn as_ffi_ptr(self) -> *const bssl_sys::EC_GROUP {
        // Safety: `group` is an address-space constant. These functions
        // cannot fail and no resources need to be released in the future.
        match self {
            Group::P256 => unsafe { bssl_sys::EC_group_p256() },
            Group::P384 => unsafe { bssl_sys::EC_group_p384() },
        }
    }
}

/// Point is a valid, finite point on some curve.
pub(crate) struct Point {
    group: *const bssl_sys::EC_GROUP,
    point: *mut bssl_sys::EC_POINT,
}

impl Point {
    /// Construct an uninitialized curve point. This is not public and all
    /// callers must ensure that the point is initialized before being returned.
    fn new(group: Group) -> Self {
        let group = group.as_ffi_ptr();
        // Safety: `group` is valid because it was constructed just above.
        let point = unsafe { bssl_sys::EC_POINT_new(group) };
        // `EC_POINT_new` only fails if out of memory, which is not a case that
        // is handled short of panicking.
        assert!(!point.is_null());
        Self { group, point }
    }

    /// Construct a point by multipling the curve's base point by the given
    /// scalar.
    unsafe fn from_scalar(group: Group, scalar: *const bssl_sys::BIGNUM) -> Option<Self> {
        let point = Self::new(group);
        // Safety: the members of `point` are valid by construction. `scalar`
        // is assumed to be valid.
        let result = unsafe {
            bssl_sys::EC_POINT_mul(
                point.group,
                point.point,
                scalar,
                /*q=*/ null(),
                /*m=*/ null(),
                /*ctx=*/ null_mut(),
            )
        };
        if result != 1 {
            return None;
        }
        if 1 == unsafe { bssl_sys::EC_POINT_is_at_infinity(point.group, point.point) } {
            return None;
        }
        Some(point)
    }

    /// Duplicate the given finite point.
    unsafe fn clone_from_ptr(
        group: *const bssl_sys::EC_GROUP,
        point: *const bssl_sys::EC_POINT,
    ) -> Point {
        assert_eq!(0, unsafe {
            bssl_sys::EC_POINT_is_at_infinity(group, point)
        });

        // Safety: we assume that the caller is passing valid pointers
        let new_point = unsafe { bssl_sys::EC_POINT_dup(point, group) };
        // `EC_POINT_dup` only fails if out of memory, which is not a case that
        // is handled short of panicking.
        assert!(!new_point.is_null());

        Self {
            group,
            point: new_point,
        }
    }

    pub fn as_ffi_ptr(&self) -> *const bssl_sys::EC_POINT {
        self.point
    }

    /// Create a new point from an uncompressed X9.62 representation.
    ///
    /// (X9.62 is the standard representation of an elliptic-curve point that
    /// starts with an 0x04 byte.)
    pub fn from_x962_uncompressed(group: Group, x962: &[u8]) -> Option<Self> {
        const UNCOMPRESSED: u8 =
            bssl_sys::point_conversion_form_t::POINT_CONVERSION_UNCOMPRESSED as u8;
        if x962.first()? != &UNCOMPRESSED {
            return None;
        }

        let point = Self::new(group);
        // Safety: `point` is valid by construction. `x962` is a valid memory
        // buffer.
        let result = unsafe {
            bssl_sys::EC_POINT_oct2point(
                point.group,
                point.point,
                x962.as_ffi_ptr(),
                x962.len(),
                /*bn_ctx=*/ null_mut(),
            )
        };
        if result == 1 {
            // X9.62 format cannot represent the point at infinity, so this
            // should be moot, but `Point` must never contain infinity.
            assert_eq!(0, unsafe {
                bssl_sys::EC_POINT_is_at_infinity(point.group, point.point)
            });
            Some(point)
        } else {
            None
        }
    }

    pub fn to_x962_uncompressed(&self) -> Buffer {
        // Safety: arguments are valid, `EC_KEY` ensures that the the group is
        // correct for the point, and a `Point` is always finite.
        unsafe { to_x962_uncompressed(self.group, self.point) }
    }

    pub fn from_der_subject_public_key_info(group: Group, spki: &[u8]) -> Option<Self> {
        let mut pkey = scoped::EvpPkey::from_ptr(parse_with_cbs(
            spki,
            // Safety: if called, `pkey` is the non-null result of `EVP_parse_public_key`.
            |pkey| unsafe { bssl_sys::EVP_PKEY_free(pkey) },
            // Safety: `cbs` is a valid pointer in this context.
            |cbs| unsafe { bssl_sys::EVP_parse_public_key(cbs) },
        )?);
        let ec_key = unsafe { bssl_sys::EVP_PKEY_get0_EC_KEY(pkey.as_ffi_ptr()) };
        if ec_key.is_null() {
            // Not an ECC key.
            return None;
        }
        let parsed_group = unsafe { bssl_sys::EC_KEY_get0_group(ec_key) };
        if parsed_group != group.as_ffi_ptr() {
            // ECC key for a different curve.
            return None;
        }
        let point = unsafe { bssl_sys::EC_KEY_get0_public_key(ec_key) };
        if point.is_null() {
            return None;
        }
        // Safety: `ec_key` is still owned by `pkey` and doesn't need to be freed.
        Some(unsafe { Self::clone_from_ptr(parsed_group, point) })
    }

    /// Calls `func` with an `EC_KEY` that contains a copy of this point.
    pub fn with_point_as_ec_key<F, T>(&self, func: F) -> T
    where
        F: FnOnce(*mut bssl_sys::EC_KEY) -> T,
    {
        let mut ec_key = scoped::EcKey::new();
        // Safety: `self.group` is always valid by construction and this doesn't
        // pass ownership.
        assert_eq!(1, unsafe {
            bssl_sys::EC_KEY_set_group(ec_key.as_ffi_ptr(), self.group)
        });
        // Safety: `self.point` is always valid by construction and this doesn't
        // pass ownership.
        assert_eq!(1, unsafe {
            bssl_sys::EC_KEY_set_public_key(ec_key.as_ffi_ptr(), self.point)
        });
        func(ec_key.as_ffi_ptr())
    }

    pub fn to_der_subject_public_key_info(&self) -> Buffer {
        // Safety: `ec_key` is a valid pointer in this context.
        self.with_point_as_ec_key(|ec_key| unsafe { to_der_subject_public_key_info(ec_key) })
    }
}

// Safety:
//
// An `EC_POINT` can be used concurrently from multiple threads so long as no
// mutating operations are performed. The mutating operations used here are
// `EC_POINT_mul` and `EC_POINT_oct2point` (which can be observed by setting
// `point` to be `*const` in the struct and seeing what errors trigger.
//
// Both those operations are done internally, however, before a `Point` is
// returned. So, after construction, callers cannot mutate the `EC_POINT`.
unsafe impl Sync for Point {}
unsafe impl Send for Point {}

impl Drop for Point {
    fn drop(&mut self) {
        // Safety: `self.point` must be valid because only valid `Point`s can
        // be constructed. `self.group` does not need to be freed.
        unsafe { bssl_sys::EC_POINT_free(self.point) }
    }
}

/// Key holds both a public and private key. While BoringSSL allows an `EC_KEY`
/// to also be a) empty, b) holding only a private scalar, or c) holding only
// a public key, those cases are never exposed as a `Key`.
pub(crate) struct Key(*mut bssl_sys::EC_KEY);

impl Key {
    /// Construct an uninitialized key. This is not public and all
    /// callers must ensure that the key is initialized before being returned.
    fn new(group: Group) -> Self {
        let key = unsafe { bssl_sys::EC_KEY_new() };
        // `EC_KEY_new` only fails if out of memory, which is not a case that
        // is handled short of panicking.
        assert!(!key.is_null());

        // Setting the group on a fresh `EC_KEY` never fails.
        assert_eq!(1, unsafe {
            bssl_sys::EC_KEY_set_group(key, group.as_ffi_ptr())
        });

        Self(key)
    }

    pub fn as_ffi_ptr(&self) -> *const bssl_sys::EC_KEY {
        self.0
    }

    /// Generate a random private key.
    pub fn generate(group: Group) -> Self {
        let key = Self::new(group);
        // Generation only fails if out of memory, which is only handled by
        // panicking.
        assert_eq!(1, unsafe { bssl_sys::EC_KEY_generate_key(key.0) });
        // `EC_KEY_generate_key` is documented as also setting the public key.
        key
    }

    /// Construct a private key from a big-endian representation of the private
    /// scalar. The scalar must be zero padded to the correct length for the
    /// curve.
    pub fn from_big_endian(group: Group, scalar: &[u8]) -> Option<Self> {
        let key = Self::new(group);
        // Safety: `key.0` is always valid by construction.
        let result = unsafe { bssl_sys::EC_KEY_oct2priv(key.0, scalar.as_ffi_ptr(), scalar.len()) };
        if result != 1 {
            return None;
        }

        // BoringSSL allows an `EC_KEY` to have a private scalar without a
        // public point, but `Key` is never exposed in that state.

        // Safety: `key.0` is valid by construction. The returned value is
        // still owned the `EC_KEY`.
        let scalar = unsafe { bssl_sys::EC_KEY_get0_private_key(key.0) };
        assert!(!scalar.is_null());

        // Safety: `scalar` is a valid pointer.
        let point = unsafe { Point::from_scalar(group, scalar)? };
        // Safety: `key.0` is valid by construction, as is `point.point`. The
        // point is copied into the `EC_KEY` so ownership isn't being moved.
        let result = unsafe { bssl_sys::EC_KEY_set_public_key(key.0, point.point) };
        // Setting the public key should only fail if out of memory, which this
        // crate doesn't handle, or if the groups don't match, which is
        // impossible.
        assert_eq!(result, 1);

        Some(key)
    }

    pub fn to_big_endian(&self) -> Buffer {
        let mut ptr: *mut u8 = null_mut();
        // Safety: `self.0` is valid by construction. If this returns non-zero
        // then ptr holds ownership of a buffer.
        let len = unsafe { bssl_sys::EC_KEY_priv2buf(self.0, &mut ptr) };
        assert!(len != 0);
        Buffer { ptr, len }
    }

    /// Parses an ECPrivateKey structure (from RFC 5915).
    pub fn from_der_ec_private_key(group: Group, der: &[u8]) -> Option<Self> {
        let key = parse_with_cbs(
            der,
            // Safety: in this context, `key` is the non-null result of
            // `EC_KEY_parse_private_key`.
            |key| unsafe { bssl_sys::EC_KEY_free(key) },
            // Safety: `cbs` is valid per `parse_with_cbs` and `group` always
            // returns a valid pointer.
            |cbs| unsafe { bssl_sys::EC_KEY_parse_private_key(cbs, group.as_ffi_ptr()) },
        )?;
        Some(Self(key))
    }

    /// Serializes this private key as an ECPrivateKey structure from RFC 5915.
    pub fn to_der_ec_private_key(&self) -> Buffer {
        cbb_to_buffer(64, |cbb| unsafe {
            // Safety: the `EC_KEY` is always valid so `EC_KEY_marshal_private_key`
            // should only fail if out of memory, which this crate doesn't handle.
            assert_eq!(
                1,
                bssl_sys::EC_KEY_marshal_private_key(
                    cbb,
                    self.0,
                    bssl_sys::EC_PKEY_NO_PARAMETERS as u32
                )
            );
        })
    }

    /// Parses a PrivateKeyInfo structure (from RFC 5208).
    pub fn from_der_private_key_info(group: Group, der: &[u8]) -> Option<Self> {
        let mut pkey = scoped::EvpPkey::from_ptr(parse_with_cbs(
            der,
            // Safety: in this context, `pkey` is the non-null result of
            // `EVP_parse_private_key`.
            |pkey| unsafe { bssl_sys::EVP_PKEY_free(pkey) },
            // Safety: `cbs` is valid per `parse_with_cbs`.
            |cbs| unsafe { bssl_sys::EVP_parse_private_key(cbs) },
        )?);
        let ec_key = unsafe { bssl_sys::EVP_PKEY_get1_EC_KEY(pkey.as_ffi_ptr()) };
        if ec_key.is_null() {
            return None;
        }
        // Safety: `ec_key` is now owned by this function.
        let parsed_group = unsafe { bssl_sys::EC_KEY_get0_group(ec_key) };
        if parsed_group == group.as_ffi_ptr() {
            // Safety: parsing an EC_KEY always set the public key. It should
            // be impossible for the public key to be infinity, but double-check.
            let is_infinite = unsafe {
                bssl_sys::EC_POINT_is_at_infinity(
                    bssl_sys::EC_KEY_get0_group(ec_key),
                    bssl_sys::EC_KEY_get0_public_key(ec_key),
                )
            };
            if is_infinite == 0 {
                // Safety: `EVP_PKEY_get1_EC_KEY` returned ownership, which we can move
                // into the returned object.
                return Some(Self(ec_key));
            }
        }
        unsafe { bssl_sys::EC_KEY_free(ec_key) };
        None
    }

    /// Serializes this private key as a PrivateKeyInfo structure from RFC 5208.
    pub fn to_der_private_key_info(&self) -> Buffer {
        let mut pkey = scoped::EvpPkey::new();
        // Safety: `pkey` was just allocated above; the `EC_KEY` is valid by
        // construction. This call takes a reference to the `EC_KEY` and so
        // hasn't stolen ownership from `self`.
        assert_eq!(1, unsafe {
            bssl_sys::EVP_PKEY_set1_EC_KEY(pkey.as_ffi_ptr(), self.0)
        });
        cbb_to_buffer(64, |cbb| unsafe {
            // `EVP_marshal_private_key` should always return one because this
            // key is valid by construction.
            assert_eq!(1, bssl_sys::EVP_marshal_private_key(cbb, pkey.as_ffi_ptr()));
        })
    }

    pub fn to_point(&self) -> Point {
        // Safety: `self.0` is valid by construction.
        let group = unsafe { bssl_sys::EC_KEY_get0_group(self.0) };
        let point = unsafe { bssl_sys::EC_KEY_get0_public_key(self.0) };
        // A `Key` is never constructed without a public key.
        assert!(!point.is_null());
        // Safety: pointers are valid and `clone_from_ptr` doesn't take
        // ownership.
        unsafe { Point::clone_from_ptr(group, point) }
    }

    pub fn to_x962_uncompressed(&self) -> Buffer {
        // Safety: `self.0` is valid by construction.
        let group = unsafe { bssl_sys::EC_KEY_get0_group(self.0) };
        let point = unsafe { bssl_sys::EC_KEY_get0_public_key(self.0) };
        // Safety: arguments are valid, `EC_KEY` ensures that the the group is
        // correct for the point, and a `Key` always holds a finite public point.
        unsafe { to_x962_uncompressed(group, point) }
    }

    pub fn to_der_subject_public_key_info(&self) -> Buffer {
        // Safety: `self.0` is always valid by construction.
        unsafe { to_der_subject_public_key_info(self.0) }
    }
}

// Safety:
//
// An `EC_KEY` is safe to use from multiple threads so long as no mutating
// operations are performed. (Reference count changes don't count as mutating.)
// The mutating operations used here are:
//   * EC_KEY_generate_key
//   * EC_KEY_oct2priv
//   * EC_KEY_set_public_key
// But those are all done internally, before a `Key` is returned. So, once
// constructed, callers cannot mutate the `EC_KEY`.
unsafe impl Sync for Key {}
unsafe impl Send for Key {}

impl Drop for Key {
    fn drop(&mut self) {
        // Safety: `self.0` must be valid because only valid `Key`s can
        // be constructed.
        unsafe { bssl_sys::EC_KEY_free(self.0) }
    }
}

/// Serialize a finite point to uncompressed X9.62 format.
///
/// Callers must ensure that the arguments are valid, that the point has the
/// specified group, and that the point is finite.
unsafe fn to_x962_uncompressed(
    group: *const bssl_sys::EC_GROUP,
    point: *const bssl_sys::EC_POINT,
) -> Buffer {
    cbb_to_buffer(65, |cbb| unsafe {
        // Safety: the caller must ensure that the arguments are valid.
        let result = bssl_sys::EC_POINT_point2cbb(
            cbb,
            group,
            point,
            bssl_sys::point_conversion_form_t::POINT_CONVERSION_UNCOMPRESSED,
            /*bn_ctx=*/ null_mut(),
        );
        // The public key is always finite, so `EC_POINT_point2cbb` only fails
        // if out of memory, which isn't handled by this crate.
        assert_eq!(result, 1);
    })
}

unsafe fn to_der_subject_public_key_info(ec_key: *mut bssl_sys::EC_KEY) -> Buffer {
    let mut pkey = scoped::EvpPkey::new();
    // Safety: this takes a reference to `ec_key` and so doesn't steal ownership.
    assert_eq!(1, unsafe {
        bssl_sys::EVP_PKEY_set1_EC_KEY(pkey.as_ffi_ptr(), ec_key)
    });
    cbb_to_buffer(65, |cbb| unsafe {
        // The arguments are valid so this will only fail if out of memory,
        // which this crate doesn't handle.
        assert_eq!(1, bssl_sys::EVP_marshal_public_key(cbb, pkey.as_ffi_ptr()));
    })
}

#[cfg(test)]
mod test {
    use super::*;

    fn test_point_format<Serialize, Parse>(serialize_func: Serialize, parse_func: Parse)
    where
        Serialize: FnOnce(&Point) -> Buffer,
        Parse: Fn(&[u8]) -> Option<Point>,
    {
        let key = Key::generate(Group::P256);
        let point = key.to_point();

        let mut vec = serialize_func(&point).as_ref().to_vec();
        let point2 = parse_func(vec.as_slice()).unwrap();
        assert_eq!(
            point.to_x962_uncompressed().as_ref(),
            point2.to_x962_uncompressed().as_ref()
        );

        assert!(parse_func(&vec.as_slice()[0..16]).is_none());

        vec[10] ^= 1;
        assert!(parse_func(vec.as_slice()).is_none());
        vec[10] ^= 1;

        assert!(parse_func(b"").is_none());
    }

    #[test]
    fn x962() {
        let x962 = b"\x04\x74\xcf\x69\xcb\xd1\x2b\x75\x07\x42\x85\xcf\x69\x6f\xc2\x56\x4b\x90\xe7\xeb\xbc\xd0\xe7\x20\x36\x86\x66\xbe\xcc\x94\x75\xa2\xa4\x4c\x2a\xf8\xa2\x56\xb8\x92\xb7\x7d\x17\xba\x97\x93\xbb\xf2\x9f\x52\x26\x7d\x90\xf9\x2c\x37\x26\x02\xbb\x4e\xd1\x89\x7c\xad\x54";
        assert!(Point::from_x962_uncompressed(Group::P256, x962).is_some());

        test_point_format(
            |point| point.to_x962_uncompressed(),
            |buf| Point::from_x962_uncompressed(Group::P256, buf),
        );
    }

    #[test]
    fn spki() {
        test_point_format(
            |point| point.to_der_subject_public_key_info(),
            |buf| Point::from_der_subject_public_key_info(Group::P256, buf),
        );
    }

    fn test_key_format<Serialize, Parse>(serialize_func: Serialize, parse_func: Parse)
    where
        Serialize: FnOnce(&Key) -> Buffer,
        Parse: Fn(&[u8]) -> Option<Key>,
    {
        let key = Key::generate(Group::P256);

        let vec = serialize_func(&key).as_ref().to_vec();
        let key2 = parse_func(vec.as_slice()).unwrap();
        assert_eq!(
            key.to_x962_uncompressed().as_ref(),
            key2.to_x962_uncompressed().as_ref()
        );

        assert!(parse_func(&vec.as_slice()[0..16]).is_none());
        assert!(parse_func(b"").is_none());
    }

    #[test]
    fn der_ec_private_key() {
        test_key_format(
            |key| key.to_der_ec_private_key(),
            |buf| Key::from_der_ec_private_key(Group::P256, buf),
        );
    }

    #[test]
    fn der_private_key_info() {
        test_key_format(
            |key| key.to_der_private_key_info(),
            |buf| Key::from_der_private_key_info(Group::P256, buf),
        );
    }

    #[test]
    fn big_endian() {
        test_key_format(
            |key| key.to_big_endian(),
            |buf| Key::from_big_endian(Group::P256, buf),
        );
    }
}
