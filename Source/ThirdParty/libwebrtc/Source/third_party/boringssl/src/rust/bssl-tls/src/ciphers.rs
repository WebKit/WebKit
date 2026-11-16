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

//! TLS Ciphers

use core::{
    ffi::CStr,
    ptr::NonNull, //
};

use crate::config::{
    CipherSuite,
    ProtocolVersion, //
};

/// A TLS cipher suite.
#[derive(Clone, Copy)]
#[repr(transparent)]
pub struct CipherDescription(pub(crate) NonNull<bssl_sys::SSL_CIPHER>);

// Safety: `SSL_CIPHER` is immutable and thread-safe.
unsafe impl Send for CipherDescription {}
unsafe impl Sync for CipherDescription {}

impl CipherDescription {
    /// Returns the cipher suite corresponding to the given RFC value.
    pub fn from_value(value: CipherSuite) -> Option<Self> {
        let ptr = unsafe {
            // Safety: the cipher code is valid by construction.
            bssl_sys::SSL_get_cipher_by_value(value.0)
        };
        Self::from_ptr(ptr)
    }

    pub(crate) fn from_ptr(ptr: *const bssl_sys::SSL_CIPHER) -> Option<Self> {
        NonNull::new(ptr as *mut _).map(Self)
    }

    pub(crate) fn ptr(self) -> *const bssl_sys::SSL_CIPHER {
        self.0.as_ptr()
    }

    /// Returns the two-byte protocol ID of the cipher suite per [IANA].
    ///
    /// [IANA]: <https://www.iana.org/assignments/tls-parameters/tls-parameters.xhtml#tls-parameters-4>
    pub fn id(&self) -> u16 {
        unsafe {
            // Safety: the cipher handle is valid.
            bssl_sys::SSL_CIPHER_get_protocol_id(self.ptr())
        }
    }

    /// Returns true if the cipher suite uses an AEAD cipher.
    pub fn is_aead(&self) -> bool {
        unsafe {
            // Safety: the cipher handle is valid.
            bssl_sys::SSL_CIPHER_is_aead(self.ptr()) != 0
        }
    }

    /// Returns true if the cipher suite uses a block cipher.
    pub fn is_block_cipher(&self) -> bool {
        unsafe {
            // Safety: the cipher handle is valid.
            bssl_sys::SSL_CIPHER_is_block_cipher(self.ptr()) != 0
        }
    }

    /// Returns the minimum protocol version required for the cipher suite.
    pub fn min_version(&self) -> ProtocolVersion {
        let version = unsafe {
            // Safety: the cipher handle is valid.
            bssl_sys::SSL_CIPHER_get_min_version(self.ptr())
        };
        let Ok(version) = ProtocolVersion::try_from(version) else {
            unreachable!("BoringSSL invariant violated")
        };
        version
    }

    /// Returns the maximum protocol version that supports the cipher suite.
    pub fn max_version(&self) -> ProtocolVersion {
        let version = unsafe {
            // Safety: the cipher handle is valid.
            bssl_sys::SSL_CIPHER_get_max_version(self.ptr())
        };
        let Ok(version) = ProtocolVersion::try_from(version) else {
            unreachable!("BoringSSL invariant violated")
        };
        version
    }

    /// Returns the standard IETF name for the cipher suite.
    pub fn standard_name(&self) -> &'static str {
        // Safety: the cipher handle is valid.
        let ptr = unsafe { bssl_sys::SSL_CIPHER_standard_name(self.ptr()) };
        // Safety: BoringSSL returns cipher names as static strings which are valid UTF-8.
        unsafe { CStr::from_ptr(ptr).to_str().unwrap() }
    }

    /// Returns a string that describes the key-exchange method used by the cipher suite.
    pub fn kx_name(&self) -> &'static str {
        // Safety: the cipher handle is valid.
        let ptr = unsafe { bssl_sys::SSL_CIPHER_get_kx_name(self.ptr()) };
        // Safety: BoringSSL returns key exchange group names as static strings
        // which are valid UTF-8.
        unsafe { CStr::from_ptr(ptr).to_str().unwrap() }
    }
}

impl core::fmt::Debug for CipherDescription {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        f.debug_struct("Cipher")
            .field("id", &self.id())
            .field("name", &self.standard_name())
            .finish()
    }
}
