// Copyright 2026 The BoringSSL Authors
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

//! X.509 certificates
//!
//! This module houses the main type [`X509Certificate`] to support serialisation and inspection
//! of X.509 certificates.
//!
//! # Parsing and unparsing
//!
//! Certificates can be serialised or deserialised between [`X509Certificate`]s and PEM- or
//! DER-encoded data.
//!
//! ```rust
//! # use bssl_x509::certificates::X509Certificate;
//! # let concatenated_pem_bytes = include_bytes!("tests/consolidated.pem");
//! # let pem = include_bytes!("tests/BoringSSLServerTest-RSA.crt");
//! let certs: Vec<X509Certificate> =
//!     X509Certificate::parse_all_from_pem(concatenated_pem_bytes).unwrap();
//! assert_eq!(certs.len(), 6);
//!
//! let cert = X509Certificate::parse_one_from_pem(pem).unwrap();
//!
//! assert_eq!(cert.not_before().unwrap(), 1769078409);
//! assert_eq!(cert.not_after().unwrap(), 64884278409);
//! let serial = cert.serial_number();
//! assert_eq!(format!("{serial}"),
//!     "457727178541466628947827494934005101068454548083");
//!
//! # use bssl_x509::certificates::GeneralName;
//! let names = cert.subject_alt_names().unwrap();
//! let sans: Vec<_> = names.iter().collect();
//! assert_eq!(sans, vec![GeneralName::Dns("www.google.com"),
//!                       GeneralName::Dns("localhost")]);
//!
//! let der: Vec<u8> = cert.to_der().unwrap();
//! ```
//!
//! # Inspecting a certificate
//!
//! One can check if a certificate has a public key that matches a [`PrivateKey`].
//! ```rust
//! # use bssl_x509::certificates::X509Certificate;
//! # use bssl_x509::keys::PrivateKey;
//! # let cert_pem = include_bytes!("tests/BoringSSLTestCA.crt");
//! # let password = b"BoringSSL is awesome!";
//! # let key_pem = include_bytes!("tests/BoringSSLTestCA.key");
//! # let cert = X509Certificate::parse_one_from_pem(cert_pem).unwrap();
//! # let key = PrivateKey::from_pem(key_pem, || password).unwrap();
//! assert!(cert.matches_private_key(&key));
//! ```
//!
//! Also it will allow inspection of common extensions of an X.509 certificate.

use alloc::{vec, vec::Vec};
use core::{
    ffi::CStr,
    fmt,
    marker::PhantomData,
    mem::{forget, transmute},
    ptr::{NonNull, null_mut},
};

use crate::{
    errors::{PemReason, PkiError, X509Error},
    ffi::{Bio, sanitize_slice, slice_into_ffi_raw_parts},
    keys::{PrivateKey, PublicKey},
};

bssl_macros::bssl_enum! {
    #[derive(Clone, Copy, Debug, PartialEq, Eq)]
    enum GeneralNameKind: u8 {
        /// Other Name
        OtherName = bssl_sys::GEN_OTHERNAME as u8,
        /// Email Address
        Email = bssl_sys::GEN_EMAIL as u8,
        /// DNS
        Dns = bssl_sys::GEN_DNS as u8,
        /// X.400
        X400 = bssl_sys::GEN_X400 as u8,
        /// Dir Name
        DirName = bssl_sys::GEN_DIRNAME as u8,
        /// EDI Party
        EdiParty = bssl_sys::GEN_EDIPARTY as u8,
        /// URI
        Uri = bssl_sys::GEN_URI as u8,
        /// IP address
        IpAddr = bssl_sys::GEN_IPADD as u8,
        /// RID
        Rid = bssl_sys::GEN_RID as u8,
    }
}

/// Supported General Names in a Subject Alternative Name extension.
#[non_exhaustive]
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash, PartialOrd, Ord)]
pub enum GeneralName<'a> {
    /// Domain Name System name
    Dns(&'a str),
    /// E-mail address
    Email(&'a str),
    /// Uniform Resource Identifier
    Uri(&'a str),
    /// IP address
    Ip(&'a [u8]),
}

/// A collection of `GeneralName`s.
pub struct GeneralNames(NonNull<bssl_sys::GENERAL_NAMES>);

impl GeneralNames {
    /// Get an iterator into the names.
    pub fn iter<'a>(&'a self) -> GeneralNamesIter<'a> {
        let len = unsafe {
            // Safety: `san` is still valid here.
            bssl_sys::sk_GENERAL_NAME_num(self.0.as_ptr())
        };
        GeneralNamesIter {
            data: self.0,
            len,
            cursor: 0,
            _p: PhantomData,
        }
    }
}

/// An enumeration of `GeneralName`s.
///
/// This iterator skips invalid, unsupported or incomplete name entry.
#[derive(Clone, Copy)]
pub struct GeneralNamesIter<'a> {
    data: NonNull<bssl_sys::GENERAL_NAMES>,
    len: usize,
    cursor: usize,
    _p: PhantomData<&'a ()>,
}

impl Drop for GeneralNames {
    fn drop(&mut self) {
        unsafe {
            // Safety: `self` witnesses the validity of the `GENERAL_NAMES` handle.
            bssl_sys::GENERAL_NAMES_free(self.0.as_ptr());
        }
    }
}

/// Safety: caller must ensure that `'a` outlives the input buffer.
unsafe fn extract_ia5str<'a>(ia5str: *const bssl_sys::ASN1_IA5STRING) -> Option<&'a str> {
    let str_len = unsafe {
        // Safety: `value` is still a valid `ASN1_STRING`,
        // which is a super-type of `ASN1_IA5STRING`.
        bssl_sys::ASN1_STRING_length(ia5str)
    };
    if str_len <= 0 {
        return None;
    }
    let str_len = usize::try_from(str_len).ok()?;
    let ptr = unsafe {
        // Safety: `value` is still a valid `ASN1_STRING`,
        // which is a super-type of `ASN1_IA5STRING`.
        bssl_sys::ASN1_STRING_get0_data(ia5str)
    };
    if ptr.is_null() {
        return None;
    }
    let bytes = unsafe {
        // Safety: `'a` will still outlive the input buffer and this byte slice.
        sanitize_slice(ptr, str_len)?
    };
    core::str::from_utf8(bytes).ok()
}

impl<'a> Iterator for GeneralNamesIter<'a> {
    type Item = GeneralName<'a>;

    fn next(&mut self) -> Option<Self::Item> {
        while self.cursor < self.len {
            let name = unsafe {
                // Safety: `data` is still valid.
                bssl_sys::sk_GENERAL_NAME_value(self.data.as_ptr(), self.cursor)
            };
            self.cursor += 1;
            if name.is_null() {
                continue;
            }
            let mut typ = -1;
            let value = unsafe { bssl_sys::GENERAL_NAME_get0_value(name, &raw mut typ) };
            if value.is_null() || typ < 0 {
                continue;
            }
            let Some(typ) = u8::try_from(typ)
                .ok()
                .and_then(|typ| GeneralNameKind::try_from(typ).ok())
            else {
                continue;
            };
            match typ {
                GeneralNameKind::Dns => {
                    let Some(dns) = (unsafe {
                        // Safety: `value` is still a valid `ASN1_IA5STRING` and outlived by `self`
                        extract_ia5str(value as _)
                    }) else {
                        continue;
                    };
                    return Some(GeneralName::Dns(dns));
                }
                GeneralNameKind::Email => {
                    let Some(email) = (unsafe {
                        // Safety: `value` is still a valid `ASN1_IA5STRING` and outlived by `self`
                        extract_ia5str(value as _)
                    }) else {
                        continue;
                    };
                    return Some(GeneralName::Email(email));
                }
                GeneralNameKind::Uri => {
                    let Some(uri) = (unsafe {
                        // Safety: `value` is still a valid `ASN1_IA5STRING` and outlived by `self`
                        extract_ia5str(value as _)
                    }) else {
                        continue;
                    };
                    return Some(GeneralName::Uri(uri));
                }
                GeneralNameKind::IpAddr => {
                    let len = unsafe {
                        // Safety: `value` is still a valid `ASN1_STRING`,
                        // which is a super-type of `ASN1_OCTET_STRING`.
                        bssl_sys::ASN1_STRING_length(value as _)
                    };
                    match len {
                        4 | 16 => {
                            let ptr = unsafe {
                                // Safety: `value` is still a valid `ASN1_OCTET_STRING`
                                bssl_sys::ASN1_STRING_get0_data(value as _)
                            };
                            if ptr.is_null() {
                                continue;
                            }
                            let bytes = unsafe {
                                // Safety: `self` will outlive the input buffer and this byte slice.
                                sanitize_slice(ptr, len as usize)?
                            };
                            return Some(GeneralName::Ip(bytes));
                        }
                        _ => continue,
                    }
                }
                _ => continue,
            }
        }
        None
    }
}

/// A X.509 certificate serial number
pub struct SerialNumber<'a>(NonNull<bssl_sys::ASN1_INTEGER>, PhantomData<&'a ()>);

impl<'a> SerialNumber<'a> {
    fn ptr(&self) -> *mut bssl_sys::ASN1_INTEGER {
        self.0.as_ptr()
    }

    /// Get a two's-complement, big-endian representation of the serial number.
    ///
    /// This is a big-endian integer. If the most-significant bit is set then
    /// it is negative. Positive values that would otherwise have the
    /// most-significant bit set are left padded with a zero byte to avoid this.
    pub fn as_twos_complement_bytes(&self) -> Vec<u8> {
        let serial = self.ptr();
        let len = unsafe {
            // Safety:
            // - self witnesses the validity of the X509 handle and, therefore,
            //   this handle to the integer;
            // - ASN1_INTEGER is a subtype of ASN1_STRING.
            bssl_sys::i2c_ASN1_INTEGER(serial, null_mut())
        };
        let Ok(len) = usize::try_from(len) else {
            panic!("invalid serial number")
        };
        if len == 0 {
            panic!("invalid serial number")
        }
        let mut res = vec![0; len];
        let len = unsafe {
            // Safety:
            // - self witnesses the validity of the X509 handle and, therefore,
            //   this handle to the integer.
            // - `outp` is non-null.
            let mut outp = res.as_mut_ptr();
            bssl_sys::i2c_ASN1_INTEGER(serial, &raw mut outp)
        };
        assert!(len > 0);
        res.truncate(usize::try_from(len).unwrap());
        res
    }
}

impl<'a> SerialNumber<'a> {
    /// Formats the serial number as a string using the given BoringSSL
    /// conversion function (`BN_bn2dec` or `BN_bn2hex`).
    fn fmt_with(
        &self,
        f: &mut fmt::Formatter<'_>,
        conv: unsafe extern "C" fn(*const bssl_sys::BIGNUM) -> *mut core::ffi::c_char,
        alt_prefix: &str,
    ) -> fmt::Result {
        let bn = unsafe {
            // Safety: self witnesses the validity of the ASN1_INTEGER handle.
            bssl_sys::ASN1_INTEGER_to_BN(self.ptr(), null_mut())
        };
        if bn.is_null() {
            return Err(fmt::Error);
        }

        struct BnGuard(*mut bssl_sys::BIGNUM);
        impl Drop for BnGuard {
            fn drop(&mut self) {
                unsafe { bssl_sys::BN_free(self.0) };
            }
        }
        let _bn_guard = BnGuard(bn);

        let s = unsafe {
            // Safety: `bn` is a valid BIGNUM.
            conv(bn)
        };
        if s.is_null() {
            return Err(fmt::Error);
        }

        struct StringGuard(*mut core::ffi::c_char);
        impl Drop for StringGuard {
            fn drop(&mut self) {
                unsafe { bssl_sys::OPENSSL_free(self.0 as *mut _) };
            }
        }
        let _s_guard = StringGuard(s);

        let s_str = unsafe {
            // Safety: `s` is a valid, NUL-terminated C string allocated by BoringSSL.
            CStr::from_ptr(s).to_str()
        }
        .map_err(|_| fmt::Error)?;

        let (is_nonnegative, abs_str) = if let Some(stripped) = s_str.strip_prefix('-') {
            (false, stripped)
        } else {
            (true, s_str)
        };

        let prefix = if f.alternate() { alt_prefix } else { "" };
        f.pad_integral(is_nonnegative, prefix, abs_str)
    }
}

impl<'a> fmt::Display for SerialNumber<'a> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        self.fmt_with(f, bssl_sys::BN_bn2dec, "")
    }
}

impl<'a> fmt::LowerHex for SerialNumber<'a> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        self.fmt_with(f, bssl_sys::BN_bn2hex, "0x")
    }
}

/// An X.509 certificate
#[repr(transparent)]
pub struct X509Certificate(NonNull<bssl_sys::X509>);

// Safety: X509 objects are reference counted and thread-safe.
unsafe impl Send for X509Certificate {}
unsafe impl Sync for X509Certificate {}

impl Clone for X509Certificate {
    fn clone(&self) -> Self {
        unsafe {
            // Safety: self witnesses the validity of the X509 handle.
            bssl_sys::X509_up_ref(self.0.as_ptr());
        }
        Self(self.0)
    }
}

impl Drop for X509Certificate {
    fn drop(&mut self) {
        unsafe {
            // Safety: this handle is taken from the builder, so it must be
            // live and valid.
            bssl_sys::X509_free(self.0.as_ptr());
        }
    }
}

impl X509Certificate {
    /// Parse an X.509 certificate from DER and return the remaining bytes.
    pub fn from_der(der: &[u8]) -> Result<(Self, &[u8]), PkiError> {
        let (mut ptr, len) = slice_into_ffi_raw_parts(der);
        let Ok(size) = len.try_into() else {
            return Err(PkiError::X509(X509Error::InvalidParameters));
        };
        let start = ptr;
        let res = unsafe {
            // Safety: the buffer `ptr` is still valid.
            bssl_sys::d2i_X509(null_mut(), &raw mut ptr, size)
        };
        if ptr.is_null() {
            return Err(PkiError::extract_lib_err());
        }
        let cert = NonNull::new(res).ok_or_else(|| PkiError::extract_lib_err())?;
        let consumed_bytes = unsafe {
            // Safety: BoringSSL ensures that the pointer points at the one byte past the last DER
            // byte.
            ptr.offset_from(start) as usize
        };
        Ok((Self(cert), &der[consumed_bytes..]))
    }

    /// Serialise an X.509 certificate into DER.
    pub fn to_der(&self) -> Result<Vec<u8>, PkiError> {
        let len = unsafe {
            // Safety: `self` holds a valid handle to `X509` object.
            bssl_sys::i2d_X509(self.ptr(), null_mut())
        };
        if len < 0 {
            return Err(PkiError::extract_lib_err());
        }
        let Ok(buf_len) = usize::try_from(len) else {
            return Err(PkiError::X509(X509Error::InvalidParameters));
        };
        let mut vec = vec![0; buf_len];
        let mut ptr = vec.as_mut_ptr();
        let len = unsafe {
            // Safety:
            // - `self` holds a valid handle to `X509` object.
            // - `ptr` points to a valid buffer allocation with a sufficient length advised by
            //   the first oracle call.
            bssl_sys::i2d_X509(self.ptr(), &raw mut ptr)
        };
        if len < 0 {
            Err(PkiError::extract_lib_err())
        } else {
            Ok(vec)
        }
    }

    /// Identify all PEM structures in the input buffer and parse until the end
    /// or the first error encountered.
    pub fn parse_all_from_pem(pem: &[u8]) -> Result<Vec<Self>, PkiError> {
        let mut bio = match Bio::from_bytes(pem) {
            Ok(bio) => bio,
            Err(_) => return Err(PkiError::X509(X509Error::PemTooLong)),
        };
        let mut res = Vec::new();
        loop {
            let (cert, eof) = match Self::parse_one(&mut bio) {
                Ok(res) => res,
                Err(PkiError::X509(X509Error::Pem(PemReason::NoStartLine))) if !res.is_empty() => {
                    return Ok(res);
                }
                Err(e) => return Err(e),
            };
            if let Some(cert) = cert {
                res.push(cert);
            }
            if eof {
                break;
            }
        }
        Ok(res)
    }

    /// Parse one X.509 certificate from the PEM.
    pub fn parse_one_from_pem(pem: &[u8]) -> Result<Self, PkiError> {
        let mut bio = match Bio::from_bytes(pem) {
            Ok(bio) => bio,
            Err(_) => return Err(PkiError::X509(X509Error::PemTooLong)),
        };
        let (cert, _) = Self::parse_one(&mut bio)?;
        if let Some(cert) = cert {
            Ok(cert)
        } else {
            Err(PkiError::X509(X509Error::NoPemBlocks))
        }
    }

    pub(crate) fn parse_one(bio: &mut Bio<'_>) -> Result<(Option<Self>, bool), PkiError> {
        let mut x509 = null_mut();
        unsafe {
            // Safety:
            // - the BIO is still valid;
            // - the X509 pointer is not null, so the function will allocate a new structure;
            // - the return value can be discarded since we provided a location to hold the handle,
            //   whose lifetime will be managed from this function.
            bssl_sys::PEM_read_bio_X509(bio.ptr(), &raw mut x509, None, null_mut());
        }
        let eof = unsafe {
            // Safety: the BIO is still valid.
            bssl_sys::BIO_eof(bio.ptr()) != 0
        };
        if let Some(x509) = NonNull::new(x509) {
            // Safety: BoringSSL guarantees that we still have a valid handle here.
            Ok((Some(Self(x509)), eof))
        } else {
            Err(PkiError::extract_lib_err())
        }
    }

    pub(crate) fn ptr(&self) -> *mut bssl_sys::X509 {
        self.0.as_ptr()
    }

    /// Take a **borrowed** raw certificate handle constructed by BoringSSL.
    ///
    /// **Use this only for cross-language interoperability.**
    ///
    /// # Safety
    ///
    /// The handle `x509` **must** be constructed by BoringSSL and valid.
    /// This method increments the ref-count of the handle.
    /// One should use this especially in case that the handle is returned from a
    /// `*_get0_*` method from BoringSSL or [`bssl_sys`] in particular.
    pub unsafe fn from_borrowed_raw(x509: NonNull<bssl_sys::X509>) -> Self {
        unsafe {
            // Safety: `cert` is valid and owned by the list.
            bssl_sys::X509_up_ref(x509.as_ptr());
        }
        Self(x509)
    }

    /// Take a raw certificate handle constructed by BoringSSL.
    ///
    /// **Use this only for cross-language interoperability.**
    ///
    /// # Safety
    ///
    /// The handle `x509` **must** be constructed by BoringSSL and valid.
    /// Unlike [`Self::from_borrowed_raw`], this method does **not** increment the ref-count
    /// of the handle.
    /// One should use this especially in case that the handle is returned from a
    /// `*_get1_*` method from BoringSSL or [`bssl_sys`] in particular; or the ownership of the
    /// handle has been transferred to the caller a priori.
    pub unsafe fn from_raw(x509: NonNull<bssl_sys::X509>) -> Self {
        Self(x509)
    }

    /// Take a reference to a raw certificate handle constructed by BoringSSL.
    ///
    /// **Use this only for cross-language interoperability.**
    ///
    /// # Safety
    ///
    /// The handle `x509` **must** be constructed by BoringSSL and valid.
    /// More over, `x509` must outlive the returned reference.
    pub unsafe fn from_raw_ref(x509: &NonNull<bssl_sys::X509>) -> &Self {
        unsafe {
            // Safety: `Self` is a transparent wrapper around `NonNull<bssl_sys::X509>`.
            transmute(x509)
        }
    }

    /// This method discharges the ownership.
    ///
    /// Use this method only for cross-language interoperability.
    pub fn into_raw(self) -> *mut bssl_sys::X509 {
        let ptr = self.ptr();
        forget(self);
        ptr
    }

    /// Get notBefore time as POSIX timestamp.
    ///
    /// This method returns `None` if the certificate does not have a valid
    /// time value in this field.
    pub fn not_before(&self) -> Option<i64> {
        let time = unsafe {
            // Safety: self witnesses the validity of the X509 handle.
            bssl_sys::X509_get0_notBefore(self.ptr())
        };
        if time.is_null() {
            return None;
        }
        let mut time_val = 0;
        let rc = unsafe {
            // Safety: both `time` and `time_val` are valid pointers to live objects.
            bssl_sys::ASN1_TIME_to_posix(time, &raw mut time_val)
        };
        (rc == 1).then_some(time_val)
    }

    /// Get notAfter time of the certificate as POSIX timestamp.
    ///
    /// This method returns `None` if the certificate does not have a valid
    /// time value in this field.
    pub fn not_after(&self) -> Option<i64> {
        let time = unsafe {
            // Safety: self witnesses the validity of the X509 handle.
            bssl_sys::X509_get0_notAfter(self.ptr())
        };
        if time.is_null() {
            return None;
        }
        let mut time_val = 0;
        let rc = unsafe {
            // Safety: both `time` and `time_val` are valid pointers to live objects.
            bssl_sys::ASN1_TIME_to_posix(time, &raw mut time_val)
        };
        (rc == 1).then_some(time_val)
    }

    /// Get the serial number of the certificate.
    ///
    /// This method returns `None` if the certificate does not have a valid
    /// serial number.
    pub fn serial_number(&self) -> SerialNumber<'_> {
        let serial = unsafe {
            // Safety: self witnesses the validity of the X509 handle.
            bssl_sys::X509_get0_serialNumber(self.ptr())
        };
        let serial = NonNull::new(serial as _).expect("non-null serial number");
        SerialNumber(serial, PhantomData)
    }

    /// Check if the private key matches the public key in the certificate.
    pub fn matches_private_key(&self, key: &PrivateKey) -> bool {
        unsafe {
            // Safety:
            // - `self.ptr()` is a valid pointer to an `X509` object.
            // - `key.ptr()` is a valid pointer to an `EVP_PKEY` object.
            bssl_sys::X509_check_private_key(self.ptr(), key.ptr()) == 1
        }
    }

    /// Enumerate Subject Alternative Names.
    pub fn subject_alt_names(&self) -> Option<GeneralNames> {
        let san = unsafe {
            // Safety: `self` witnesses the validity of the `X509` handle.
            bssl_sys::X509_get_ext_d2i(
                self.ptr(),
                bssl_sys::NID_subject_alt_name,
                null_mut(),
                null_mut(),
            )
        };
        let san = san as *mut bssl_sys::GENERAL_NAMES;
        NonNull::new(san).map(GeneralNames)
    }

    /// Get the public key of the certificate.
    pub fn public_key(&self) -> Option<PublicKey> {
        let key = unsafe {
            // Safety:
            // - `self` witnesses the validity of the `X509` handle.
            // - this call will bump the ref-count for us.
            bssl_sys::X509_get_pubkey(self.ptr())
        };
        NonNull::new(key).map(PublicKey)
    }
}
