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

//! TLS Session support for BoringSSL.

use alloc::vec::Vec;
use core::ptr::NonNull;

use crate::{
    call_slice_getter,
    config::ProtocolVersion,
    context::TlsContext,
    errors::Error,
    ffi::{
        Alloc,
        sanitize_slice,
        slice_into_ffi_raw_parts, //
    }, //
};

/// A TLS session.
///
/// See [RFC 8446 Section 2.2](https://datatracker.ietf.org/doc/html/rfc8446#section-2.2).
pub struct TlsSession(pub(crate) NonNull<bssl_sys::SSL_SESSION>);

// Safety: once constructed an `SSL_SESSION` is immutable and has no thread-local data.
unsafe impl Send for TlsSession {}
unsafe impl Sync for TlsSession {}

impl Drop for TlsSession {
    fn drop(&mut self) {
        unsafe {
            // Safety: self.ptr() is valid and we own a reference.
            bssl_sys::SSL_SESSION_free(self.ptr());
        }
    }
}

impl Clone for TlsSession {
    fn clone(&self) -> Self {
        unsafe {
            // Safety: self.ptr() is valid.
            bssl_sys::SSL_SESSION_up_ref(self.ptr());
        }
        Self(self.0)
    }
}

impl TlsSession {
    pub(crate) fn ptr(&self) -> *mut bssl_sys::SSL_SESSION {
        self.0.as_ptr()
    }

    /// Serializes the session into a newly allocated buffer.
    pub fn to_bytes(&self) -> Result<Vec<u8>, Error> {
        let mut out_data: *mut u8 = core::ptr::null_mut();
        let mut out_len: usize = 0;
        let rc = unsafe {
            // Safety: `self.ptr()` is still valid.
            bssl_sys::SSL_SESSION_to_bytes(self.ptr(), &raw mut out_data, &raw mut out_len)
        };
        if rc != 1 {
            return Err(Error::extract_lib_err());
        }
        let out_data = Alloc(out_data);
        let slice = unsafe {
            // Safety: out_data.0 and out_len are returned by BoringSSL and are valid.
            sanitize_slice(out_data.0, out_len).unwrap()
        };
        Ok(slice.to_vec())
    }

    /// Serializes the session for a ticket, excluding the session ID.
    pub fn to_bytes_for_ticket(&self) -> Result<Vec<u8>, Error> {
        let mut out_data: *mut u8 = core::ptr::null_mut();
        let mut out_len: usize = 0;
        let rc = unsafe {
            // Safety: `self.ptr()` is still valid.
            bssl_sys::SSL_SESSION_to_bytes_for_ticket(self.ptr(), &mut out_data, &mut out_len)
        };
        if rc != 1 {
            return Err(Error::extract_lib_err());
        }
        let out_data = Alloc(out_data);
        let slice = unsafe {
            // Safety: out_data.0 and out_len are returned by BoringSSL and are valid.
            sanitize_slice(out_data.0, out_len).unwrap()
        };
        Ok(slice.to_vec())
    }

    /// Parses a serialized session from bytes.
    pub fn from_bytes<M>(bytes: &[u8], ctx: &TlsContext<M>) -> Result<Self, Error> {
        let (ptr, len) = slice_into_ffi_raw_parts(bytes);
        let ptr = unsafe {
            // Safety: bytes is a valid slice and the context is still valid.
            bssl_sys::SSL_SESSION_from_bytes(ptr, len, ctx.ptr())
        };
        let ptr = NonNull::new(ptr).ok_or_else(|| Error::extract_lib_err())?;
        Ok(Self(ptr))
    }

    /// Get the protocol version of the session.
    pub fn get_protocol_version(&self) -> Option<ProtocolVersion> {
        let version = unsafe {
            // Safety: self.ptr() is valid.
            bssl_sys::SSL_SESSION_get_protocol_version(self.ptr())
        };
        version.try_into().ok()
    }

    /// Get the session creation time in seconds since the epoch.
    pub fn get_time(&self) -> u64 {
        unsafe {
            // Safety: self.ptr() is valid.
            bssl_sys::SSL_SESSION_get_time(self.ptr())
        }
    }

    /// Get the session timeout in seconds.
    pub fn get_timeout(&self) -> u64 {
        unsafe {
            // Safety: self.ptr() is valid.
            bssl_sys::SSL_SESSION_get_timeout(self.ptr()).into()
        }
    }

    /// Get the peer certificates as a list of DER-encoded certificates.
    pub fn get_peer_certificates(&self) -> Result<Vec<Vec<u8>>, Error> {
        let sk = unsafe {
            // Safety: self.ptr() is valid.
            bssl_sys::SSL_SESSION_get0_peer_certificates(self.ptr())
        };
        if sk.is_null() {
            return Ok(Vec::new());
        }
        let len = unsafe {
            // Safety: `sk` is valid.
            bssl_sys::sk_CRYPTO_BUFFER_num(sk)
        };
        let mut res = Vec::new();
        for i in 0..len {
            let buf = unsafe {
                // Safety: `sk` is valid and `i` is in bounds.
                bssl_sys::sk_CRYPTO_BUFFER_value(sk, i)
            };
            if buf.is_null() {
                continue;
            }
            let (data, len) = unsafe {
                // Safety: `buf` is valid.
                (
                    bssl_sys::CRYPTO_BUFFER_data(buf),
                    bssl_sys::CRYPTO_BUFFER_len(buf),
                )
            };
            let Some(slice) = (unsafe {
                // Safety: data and len are valid.
                sanitize_slice(data, len)
            }) else {
                continue;
            };
            res.push(slice.to_vec());
        }
        Ok(res)
    }

    /// Get the signed certificate timestamp list, if any.
    pub fn get_signed_cert_timestamp_list(&self) -> Option<&[u8]> {
        call_slice_getter!(
            bssl_sys::SSL_SESSION_get0_signed_cert_timestamp_list,
            self.ptr()
        )
    }

    /// Get the OCSP response, if any.
    ///
    /// See [RFC 8446 §4.4.2.1](https://datatracker.ietf.org/doc/html/rfc8446#section-4.4.2.1).
    pub fn get_ocsp_response(&self) -> Option<&[u8]> {
        call_slice_getter!(bssl_sys::SSL_SESSION_get0_ocsp_response, self.ptr())
    }

    /// Check if the session should be single use.
    pub fn should_be_single_use(&self) -> bool {
        unsafe {
            // Safety: self.ptr() is valid.
            bssl_sys::SSL_SESSION_should_be_single_use(self.ptr()) == 1
        }
    }

    /// Check if the session is resumable.
    pub fn is_resumable(&self) -> bool {
        unsafe {
            // Safety: self.ptr() is valid.
            bssl_sys::SSL_SESSION_is_resumable(self.ptr()) == 1
        }
    }

    /// Check if the session has a ticket.
    pub fn has_ticket(&self) -> bool {
        unsafe {
            // Safety: self.ptr() is valid.
            bssl_sys::SSL_SESSION_has_ticket(self.ptr()) == 1
        }
    }

    /// Get the ticket, if any.
    pub fn get_ticket(&self) -> Option<&[u8]> {
        call_slice_getter!(bssl_sys::SSL_SESSION_get0_ticket, self.ptr())
    }

    /// Check if the session has a peer SHA256.
    pub fn has_peer_sha256(&self) -> bool {
        unsafe {
            // Safety: self.ptr() is valid.
            bssl_sys::SSL_SESSION_has_peer_sha256(self.ptr()) == 1
        }
    }

    /// Get the peer SHA256, if any.
    pub fn get_peer_sha256(&self) -> Option<&[u8]> {
        call_slice_getter!(bssl_sys::SSL_SESSION_get0_peer_sha256, self.ptr())
    }

    /// Check if the session is resumable across names.
    pub fn is_resumable_across_names(&self) -> bool {
        unsafe {
            // Safety: self.ptr() is valid.
            bssl_sys::SSL_SESSION_is_resumable_across_names(self.ptr()) == 1
        }
    }

    /// Get the Raw Public Key offered by the peer in the `ClientHello`.
    pub fn get_peer_raw_public_key(&self) -> Option<Vec<u8>> {
        let pkey = unsafe {
            // Safety:
            // - `self.ptr()` is a valid `SSL` handle.
            // - `pkey` does not escape the current function frame.
            NonNull::new(bssl_sys::SSL_SESSION_get0_peer_rpk(self.ptr()))?
        };
        Some(crate::credentials::marshal_evp_into_spki(pkey))
    }

    /// Check if the session is early data capable.
    pub fn early_data_capable(&self) -> bool {
        unsafe {
            // Safety: self.ptr() is valid.
            bssl_sys::SSL_SESSION_early_data_capable(self.ptr()) == 1
        }
    }
}
