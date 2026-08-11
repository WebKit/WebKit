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

//! X.509 certificate verification parameters.

use alloc::ffi::CString;
use core::{
    ffi::{c_int, c_uint, c_ulong},
    ptr::NonNull,
};

use bssl_macros::bssl_enum;

use crate::{check_lib_error, errors::PkiError, ffi::slice_into_ffi_raw_parts};

bssl_enum! {
    /// Trust settings for certificate verification.
    #[derive(Clone, Copy, PartialEq, Eq)]
    pub enum Trust: i8 {
        /// Compatibility mode.
        Compat = bssl_sys::X509_TRUST_COMPAT as i8,
        /// Trust as a client SSL certificate.
        SslClient = bssl_sys::X509_TRUST_SSL_CLIENT as i8,
        /// Trust as a server SSL certificate.
        SslServer = bssl_sys::X509_TRUST_SSL_SERVER as i8,
        /// Trust as an email certificate.
        Email = bssl_sys::X509_TRUST_EMAIL as i8,
        /// Trust as an object signing certificate.
        ObjectSign = bssl_sys::X509_TRUST_OBJECT_SIGN as i8,
        /// Trust as a Time Stamping Authority.
        Tsa = bssl_sys::X509_TRUST_TSA as i8,
    }
}

bssl_enum! {
    /// Purpose settings for certificate verification.
    #[derive(Clone, Copy, PartialEq, Eq)]
    pub enum Purpose: i8 {
        /// SSL client.
        SslClient = bssl_sys::X509_PURPOSE_SSL_CLIENT as i8,
        /// SSL server.
        SslServer = bssl_sys::X509_PURPOSE_SSL_SERVER as i8,
        /// Netscape SSL server.
        NsSslServer = bssl_sys::X509_PURPOSE_NS_SSL_SERVER as i8,
        /// S/MIME signing.
        SmimeSign = bssl_sys::X509_PURPOSE_SMIME_SIGN as i8,
        /// S/MIME encryption.
        SmimeEncrypt = bssl_sys::X509_PURPOSE_SMIME_ENCRYPT as i8,
        /// CRL signing.
        CrlSign = bssl_sys::X509_PURPOSE_CRL_SIGN as i8,
        /// Any purpose.
        Any = bssl_sys::X509_PURPOSE_ANY as i8,
        /// OCSP helper.
        OcspHelper = bssl_sys::X509_PURPOSE_OCSP_HELPER as i8,
        /// Timestamp signing.
        TimestampSign = bssl_sys::X509_PURPOSE_TIMESTAMP_SIGN as i8,
    }
}

bitflags::bitflags! {
    /// Flags for X.509 certificate verification.
    #[derive(Debug, Copy, Clone, PartialEq, Eq)]
    pub struct VerificationFlags: c_ulong {
        /// Use check time.
        const USE_CHECK_TIME = bssl_sys::X509_V_FLAG_USE_CHECK_TIME as c_ulong;
        /// Explicit policy, which mandates assertions of the policy OIDs in the final certificate
        /// chain.
        ///
        /// See [RFC 5280], §6.1.1.
        ///
        /// [RFC 5280]: <https://datatracker.ietf.org/doc/html/rfc5280#section-6.1.1>
        const EXPLICIT_POLICY = bssl_sys::X509_V_FLAG_EXPLICIT_POLICY as c_ulong;
        /// Inhibit the `anyPolicy`.
        ///
        /// See [RFC 5280], §6.1.1.
        ///
        /// [RFC 5280]: <https://datatracker.ietf.org/doc/html/rfc5280#section-6.1.1>
        const INHIBIT_ANY = bssl_sys::X509_V_FLAG_INHIBIT_ANY as c_ulong;
        /// Inhibit policy mapping.
        ///
        /// See [RFC 5280], §6.1.1.
        ///
        /// [RFC 5280]: <https://datatracker.ietf.org/doc/html/rfc5280#section-6.1.1>
        const INHIBIT_MAP = bssl_sys::X509_V_FLAG_INHIBIT_MAP as c_ulong;
        /// Treat all trusted certificates as trust anchors regardless of the
        /// [`CertificateVerificationParams::set_trust`] setting.
        const PARTIAL_CHAIN = bssl_sys::X509_V_FLAG_PARTIAL_CHAIN as c_ulong;
        /// Disable building of alternative chains, when the first built chain was rejected.
        const NO_ALT_CHAINS = bssl_sys::X509_V_FLAG_NO_ALT_CHAINS as c_ulong;
        /// Disable all time checks during certificate verification.
        const NO_CHECK_TIME = bssl_sys::X509_V_FLAG_NO_CHECK_TIME as c_ulong;
    }
}

bitflags::bitflags! {
    /// Flags for X.509 host checking.
    #[derive(Debug, Copy, Clone, PartialEq, Eq)]
    pub struct HostFlags: c_uint {
        /// Disable wildcard matching for DNS names.
        const NO_WILDCARDS = bssl_sys::X509_CHECK_FLAG_NO_WILDCARDS as c_uint;
        /// Disable the subject fallback, normally enabled when subjectAltNames is missing.
        const NEVER_CHECK_SUBJECT = bssl_sys::X509_CHECK_FLAG_NEVER_CHECK_SUBJECT as c_uint;
    }
}

/// X.509 verification parameters.
pub struct CertificateVerificationParams(NonNull<bssl_sys::X509_VERIFY_PARAM>);

// Safety:
// - `X509_VERIFY_PARAM` is a configuration struct and can be sent.
// - There are no methods that uses shared access, so no data race is possible.
unsafe impl Send for CertificateVerificationParams {}
unsafe impl Sync for CertificateVerificationParams {}

impl Drop for CertificateVerificationParams {
    fn drop(&mut self) {
        unsafe {
            bssl_sys::X509_VERIFY_PARAM_free(self.0.as_ptr());
        }
    }
}

impl CertificateVerificationParams {
    /// Create a new `CertificateVerificationParams`.
    pub fn new() -> Self {
        let param = unsafe {
            // Safety: this call returns a new X509_VERIFY_PARAM or null on failure.
            bssl_sys::X509_VERIFY_PARAM_new()
        };
        let Some(param) = NonNull::new(param) else {
            panic!("allocation error")
        };
        Self(param)
    }

    pub(crate) fn ptr(&self) -> *mut bssl_sys::X509_VERIFY_PARAM {
        self.0.as_ptr()
    }

    /// Extract the handle for cross-language interoperability.
    ///
    /// # Safety
    ///
    /// The handle **must** outlive the use sites.
    /// Verify the callsite contract to honour the lifetime contracts.
    pub unsafe fn as_ptr(&self) -> *mut bssl_sys::X509_VERIFY_PARAM {
        self.ptr()
    }

    /// Set the expected DNS hostname.
    pub fn set_host(&mut self, host: &str) -> Result<&mut Self, PkiError> {
        let c_host = CString::new(host).map_err(|_| PkiError::InternalNullBytes)?;
        check_lib_error!(unsafe {
            // Safety: the string has been NUL-terminated.
            bssl_sys::X509_VERIFY_PARAM_set1_host(self.ptr(), c_host.as_ptr(), host.len())
        });
        Ok(self)
    }

    /// Add an expected DNS hostname.
    pub fn add_host(&mut self, host: &str) -> Result<&mut Self, PkiError> {
        let c_host = CString::new(host).map_err(|_| PkiError::InternalNullBytes)?;
        check_lib_error!(unsafe {
            // Safety: the string has been NUL-terminated.
            bssl_sys::X509_VERIFY_PARAM_add1_host(self.ptr(), c_host.as_ptr(), host.len())
        });
        Ok(self)
    }

    /// Set the expected email address.
    pub fn set_email(&mut self, email: &str) -> Result<&mut Self, PkiError> {
        let c_email = CString::new(email).map_err(|_| PkiError::InternalNullBytes)?;
        check_lib_error!(unsafe {
            bssl_sys::X509_VERIFY_PARAM_set1_email(self.ptr(), c_email.as_ptr(), email.len())
        });

        Ok(self)
    }

    /// Set the expected IP address.
    pub fn set_ip(&mut self, ip: &[u8]) -> Result<&mut Self, PkiError> {
        if !matches!(ip.len(), 4 | 16) {
            return Err(PkiError::InvalidIp);
        }
        let (ip, len) = slice_into_ffi_raw_parts(ip);
        check_lib_error!(unsafe {
            // Safety: the slice pointer is not null.
            bssl_sys::X509_VERIFY_PARAM_set1_ip(self.ptr(), ip, len)
        });

        Ok(self)
    }

    /// Set the expected IP address from an ASCII string.
    pub fn set_ip_asc(&mut self, ip: &str) -> Result<&mut Self, PkiError> {
        let c_ip = CString::new(ip).map_err(|_| PkiError::InvalidIp)?;
        check_lib_error!(unsafe {
            // Safety: the string has been NUL-terminated.
            bssl_sys::X509_VERIFY_PARAM_set1_ip_asc(self.ptr(), c_ip.as_ptr())
        });

        Ok(self)
    }

    /// Set the verification depth.
    pub fn set_depth(&mut self, depth: u16) -> &mut Self {
        unsafe {
            // Safety: the validity of the handle `self.0` is witnessed by `self`.
            bssl_sys::X509_VERIFY_PARAM_set_depth(self.ptr(), depth.into());
        }
        self
    }

    /// Set the verification purpose.
    pub fn set_purpose(&mut self, purpose: Purpose) -> Result<&mut Self, PkiError> {
        check_lib_error!(unsafe {
            // Safety:
            // - the validity of the handle `self.0` is witnessed by `self`.
            // - the `purpose` value is valid per construction.
            bssl_sys::X509_VERIFY_PARAM_set_purpose(self.ptr(), purpose as c_int)
        });
        Ok(self)
    }

    /// Set the verification trust.
    pub fn set_trust(&mut self, trust: Trust) -> Result<&mut Self, PkiError> {
        check_lib_error!(unsafe {
            // Safety: the validity of the handle `self.0` is witnessed by `self`.
            bssl_sys::X509_VERIFY_PARAM_set_trust(self.ptr(), trust as c_int)
        });
        Ok(self)
    }

    /// Set time that verification will run against, specified as Unix timestamp.
    pub fn set_time_posix(&mut self, time: i64) {
        unsafe {
            // Safety: the validity of the handle `self.0` is witnessed by `self`.
            bssl_sys::X509_VERIFY_PARAM_set_time_posix(self.ptr(), time);
        }
    }

    /// Set verification flags.
    pub fn set_flags(&mut self, flags: VerificationFlags) -> Result<&mut Self, PkiError> {
        check_lib_error!(unsafe {
            // Safety: the flag bits are valid by construction.
            bssl_sys::X509_VERIFY_PARAM_set_flags(self.ptr(), flags.bits())
        });
        Ok(self)
    }

    /// Clear verification flags.
    pub fn clear_flags(&mut self, flags: VerificationFlags) -> Result<&mut Self, PkiError> {
        check_lib_error!(unsafe {
            // Safety:
            // - the validity of the handle `self.0` is witnessed by `self`.
            // - the flag bits are valid by construction.
            bssl_sys::X509_VERIFY_PARAM_clear_flags(self.ptr(), flags.bits())
        });
        Ok(self)
    }

    /// Set the host flags.
    pub fn set_hostflags(&mut self, flags: HostFlags) {
        unsafe {
            // Safety:
            // - the validity of the handle `self.0` is witnessed by `self`.
            // - the flag bits are valid by construction.
            bssl_sys::X509_VERIFY_PARAM_set_hostflags(self.ptr(), flags.bits());
        }
    }
}
