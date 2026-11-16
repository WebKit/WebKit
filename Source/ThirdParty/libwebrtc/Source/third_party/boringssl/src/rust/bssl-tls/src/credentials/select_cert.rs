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

//! TLS certificate selection hook.

use alloc::vec::Vec;
use core::{
    ffi::{
        c_int,
        c_void, //
    },
    marker::PhantomData,
    ptr::{
        NonNull,
        null, //
    },
    task::Context,
};

use bssl_crypto::FromFfiSlice;

use super::{
    CryptoBufferIterator,
    DistinguishedName,
    SignatureAlgorithm,
    get_peer_certificate_type,
    get_peer_raw_public_key, //
};
use crate::{
    CertCallback,
    abort_on_panic,
    alerts::AlertDescription,
    config::ProtocolVersion,
    connection::methods::waker_data_from_ssl,
    context::TlsContext, //
};

/// Result of certificate selection.
#[derive(Debug, Copy, Clone, PartialEq, Eq)]
pub enum CertificateSelectionResult {
    /// Certificate selection succeeded.
    Success,
    /// Certificate selection is pending (e.g. waiting for async operation).
    /// The handshake will be paused and can be retried later.
    Pending,
    /// Certificate selection failed. The handshake will be aborted with an
    /// `internal_error` alert.
    Error,
    /// Certificate selection failed. The handshake will be aborted with the
    /// specified alert.
    ErrorWithAlert(AlertDescription),
}

/// Server certificate selection.
///
/// Implementations must be `Send + Sync`. If the selector needs mutable state,
/// use interior mutability (e.g. `Mutex`).
pub trait ServerCertificateSelector<Mode>: Send + Sync {
    /// This method should return `CertificateSelectionResult` to indicate the outcome of the selection.
    fn select(
        &self,
        ctx: ServerCertificateSelectionContext<'_, Mode>,
        waker: Option<&'_ mut Context<'_>>,
    ) -> CertificateSelectionResult;
}

/// Client certificate selection.
///
/// Implementations must be `Send + Sync`. If the selector needs mutable state,
/// use interior mutability (e.g. `Mutex`).
pub trait ClientCertificateSelector<M>: Send + Sync {
    /// This method should return `CertificateSelectionResult` to indicate the outcome of the selection.
    fn select(
        &self,
        ctx: ClientCertificateSelectionContext<'_, M>,
        waker: Option<&'_ mut Context<'_>>,
    ) -> CertificateSelectionResult;
}

/// Certificate selection callback.
pub(crate) unsafe extern "C" fn select_cert_cb<M: CertCallback<Mode>, Mode>(
    ssl: *mut bssl_sys::SSL,
    _ctx: *mut c_void,
    out_alert: *mut u8,
) -> c_int {
    let is_server = unsafe {
        // Safety: `ssl` is still valid by BoringSSL invariant.
        bssl_sys::SSL_is_server(ssl) == 1
    };
    let Some(ssl) = NonNull::new(ssl) else {
        return 0;
    };
    let Some(methods) = (unsafe {
        // Safety: this callback is installed by this crate so the ex_data index must be valid.
        M::from_ssl(ssl.as_ptr())
    }) else {
        return 0;
    };
    let waker = unsafe {
        // Safety: this callback is installed by this crate so the associated data must have been
        // set up correctly.
        waker_data_from_ssl(ssl)
    };
    let waker = if let Some(waker) = &waker {
        Some(&mut core::task::Context::from_waker(waker))
    } else {
        None
    };
    if is_server {
        let ctx = ServerCertificateSelectionContext::new(ssl);
        let Some(server_cert_cb) = methods.server_cert_cb() else {
            return 1;
        };
        match abort_on_panic(move || server_cert_cb.select(ctx, waker)) {
            CertificateSelectionResult::Success => 1,
            CertificateSelectionResult::Pending => -1,
            CertificateSelectionResult::Error => 0,
            CertificateSelectionResult::ErrorWithAlert(alert) => {
                unsafe {
                    // Safety: per BoringSSL contract, this out pointer must be valid to write the
                    // alert value into.
                    out_alert.write(alert as u8);
                }
                0
            }
        }
    } else {
        let ctx = ClientCertificateSelectionContext::new(ssl);
        let Some(client_cert_cb) = methods.client_cert_cb() else {
            return 1;
        };
        match abort_on_panic(move || client_cert_cb.select(ctx, waker)) {
            CertificateSelectionResult::Success => 1,
            CertificateSelectionResult::Pending => -1,
            CertificateSelectionResult::Error => 0,
            CertificateSelectionResult::ErrorWithAlert(alert) => {
                unsafe {
                    // Safety: per BoringSSL contract, this out pointer must be valid to write the
                    // alert value into.
                    out_alert.write(alert as u8);
                }
                0
            }
        }
    }
}

/// Server certificate selection context.
#[repr(transparent)]
pub struct ServerCertificateSelectionContext<'a, Mode>(
    NonNull<bssl_sys::SSL>,
    PhantomData<&'a fn() -> Mode>,
);

impl<'a, M> ServerCertificateSelectionContext<'a, M> {
    pub(crate) fn new(ssl: NonNull<bssl_sys::SSL>) -> Self {
        Self(ssl, PhantomData)
    }

    /// Switch context.
    pub fn set_context(&mut self, ctx: &TlsContext<M>) {
        unsafe {
            // Safety: `self.0` is still valid by BoringSSL invariant.
            bssl_sys::SSL_set_SSL_CTX(self.0.as_ptr(), ctx.ptr());
        }
    }

    /// Append `credential` to the list of credentials of this connection.
    ///
    /// Earlier calls to this method append a credential that is preferred over those added
    /// in later calls.
    pub fn add_credential(
        &mut self,
        credential: &super::TlsCredential,
    ) -> Result<(), super::super::errors::Error> {
        crate::check_lib_error!(unsafe {
            // Safety: both `self.0` and `credential` are still valid.
            bssl_sys::SSL_add1_credential(self.0.as_ptr(), credential.ptr())
        });
        Ok(())
    }

    /// Get the peer's [`CertificateType`](super::CertificateType).
    pub fn get_peer_certificate_type(&self) -> Option<super::CertificateType> {
        get_peer_certificate_type(self.0.as_ptr())
    }

    /// Get the peer's raw public key as DER-encoded `SubjectPublicKeyInfo`.
    pub fn get_peer_raw_public_key(&self) -> Option<Vec<u8>> {
        get_peer_raw_public_key(self.0.as_ptr())
    }
}

bssl_crypto::bssl_enum! {
    /// Requested certificate types defined in [RFC 5246] §7.4.4.
    ///
    /// [RFC 5246]: <https://datatracker.ietf.org/doc/html/rfc5246#section-7.4.4>
    #[derive(Debug, Copy, Clone, PartialEq, Eq)]
    #[non_exhaustive]
    pub enum RequestedCertificateType: u8 {
        /// RSA signed certificate.
        RsaSign = 1,
        /// DSS signed certificate.
        DssSign = 2,
        /// RSA signed certificate with a static Diffie-Hellman key.
        RsaFixedDh = 3,
        /// DSS signed certificate with a static Diffie-Hellman key.
        DssFixedDh = 4,
        /// Reserved.
        RsaEphemeralDh = 5,
        /// Reserved.
        DssEphemeralDh = 6,
        /// Reserved.
        FortezzaDms = 20,
    }
}

/// Client Certificate Selection.
pub struct ClientCertificateSelectionContext<'a, Mode>(
    NonNull<bssl_sys::SSL>,
    PhantomData<&'a fn() -> Mode>,
);

impl<'a, M> ClientCertificateSelectionContext<'a, M> {
    pub(crate) fn new(ssl: NonNull<bssl_sys::SSL>) -> Self {
        Self(ssl, PhantomData)
    }

    pub(crate) fn ptr(&self) -> *mut bssl_sys::SSL {
        self.0.as_ptr()
    }

    /// Protocol version selected.
    ///
    /// This method returns [`None`] if the protocol version is unrecognised.
    pub fn protocol_version(&self) -> Option<ProtocolVersion> {
        ProtocolVersion::try_from(unsafe {
            // Safety: `self.0` is still valid by BoringSSL invariant.
            bssl_sys::SSL_version(self.ptr())
        })
        .ok()
    }
    /// Get verification algorithm from the peer.
    ///
    /// If a signature algorithm is unrecognised, it will be reported as an `Err` value.
    /// This method might return an empty list if the protocol version is not TLS 1.3.
    pub fn get_tls13_peer_verification_algorithms(&self) -> Vec<Result<SignatureAlgorithm, u16>> {
        let mut algs = null();
        let len = unsafe {
            // Safety: `self.0` is still valid by BoringSSL invariant.
            bssl_sys::SSL_get0_peer_verify_algorithms(self.ptr(), &raw mut algs)
        };
        let slice = unsafe {
            // Safety:
            // - `slice` is only live within this call.
            // - `slice` is generated by BoringSSL.
            u16::from_ffi_ptr(algs, len)
        };
        slice
            .iter()
            .copied()
            .map(SignatureAlgorithm::try_from)
            .collect()
    }

    /// Get TLS 1.2 certificate types.
    ///
    /// If a certificate type is unrecognised, it will be reported as an `Err` value.
    /// This method returns an empty list if the protocol version is TLS 1.3.
    pub fn get_tls12_certificate_types(&self) -> Vec<Result<RequestedCertificateType, u8>> {
        let mut types = null();
        let len = unsafe {
            // Safety: `self.0` is still valid by BoringSSL invariant.
            bssl_sys::SSL_get0_certificate_types(self.ptr(), &raw mut types)
        };
        let slice = unsafe {
            // Safety:
            // - `types` is only live within this call.
            // - `types` is generated by BoringSSL.
            u8::from_ffi_ptr(types, len)
        };
        slice
            .iter()
            .copied()
            .map(RequestedCertificateType::try_from)
            .collect()
    }

    /// Get server requested CAs for client authentication.
    ///
    /// These names are sent by the server and they are the acceptable CAs by server.
    pub fn get_server_ca_list(&self) -> Vec<DistinguishedName> {
        let names = unsafe {
            // Safety: `self.ptr()` is still valid by BoringSSL invariant.
            bssl_sys::SSL_get0_server_requested_CAs(self.ptr())
        };
        let names: CryptoBufferIterator<'_, DistinguishedName> = unsafe {
            // Safety: `names` is owned by SSL and only live within this call.
            CryptoBufferIterator::new(names)
        };
        names.collect()
    }

    /// Append `credential` to the list of credentials of this connection.
    ///
    /// Earlier calls to this method append a credential that is preferred over those added
    /// in later calls.
    pub fn add_credential(
        &mut self,
        credential: &super::TlsCredential,
    ) -> Result<(), super::super::errors::Error> {
        crate::check_lib_error!(unsafe {
            // Safety: both `self.0` and `credential` are still valid.
            bssl_sys::SSL_add1_credential(self.0.as_ptr(), credential.ptr())
        });
        Ok(())
    }

    /// Get the peer's [`CertificateType`](super::CertificateType).
    pub fn get_peer_certificate_type(&self) -> Option<super::CertificateType> {
        super::get_peer_certificate_type(self.ptr())
    }

    /// Get the peer's raw public key as DER-encoded `SubjectPublicKeyInfo`.
    pub fn get_peer_raw_public_key(&self) -> Option<Vec<u8>> {
        super::get_peer_raw_public_key(self.ptr())
    }
}
