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

use core::ptr::null_mut;

use bssl_x509::{
    params::CertificateVerificationParams,
    store::X509Store, //
};

use super::TlsContextBuilder;
use crate::{
    check_lib_error,
    config::ConfigurationError,
    context::{
        CertificateCache,
        SupportedMode,
        methods::HasPrivateKeyMethods, //
    },
    credentials::{
        CertificateType,
        CertificateVerificationMode,
        DistinguishedName,
        PrivateKeyDelegate,
        SignatureAlgorithm,
        TlsCredential,
        VerifyCertificate,
        cert_cb,
        early_callback::{
            EarlyCallback,
            early_select_cert_cb, //
        }, //
    },
    errors::Error,
    ffi::slice_into_ffi_raw_parts,
    has_duplicates, //
};

/// # Credentials
impl<M> TlsContextBuilder<M>
where
    M: SupportedMode,
{
    /// Set the certificate cache.
    ///
    /// This [`CertificateCache`] can be shared across multiple contexts and their associated
    /// connections.
    /// The cache is recommended for use to reduce memory footprint arising from the X.509-based
    /// authentication.
    pub fn with_certificate_cache(&mut self, cache: Option<&CertificateCache>) -> &mut Self {
        let ctx = self.ptr();
        if let Some(cache) = cache {
            unsafe {
                // Safety: `CertificateCache` is `Send + Sync`
                bssl_sys::SSL_CTX_set1_buffer_pool(ctx, cache.ptr());
            }
        } else {
            unsafe {
                // Safety: we just detach the buffer pool before any active pointer into it is created.
                bssl_sys::SSL_CTX_set1_buffer_pool(ctx, null_mut());
            }
        }
        self
    }

    /// Set the private key method.
    ///
    /// This private key method delegation may be replaced the next moment when
    /// a new TLS private key is supplied.
    pub fn with_private_key_delegate(
        &mut self,
        key_method: Option<Box<dyn PrivateKeyDelegate>>,
    ) -> &mut Self {
        let ctx = self.ptr();
        if key_method.is_some() {
            unsafe {
                // Safety: we only install our own vtable.
                bssl_sys::SSL_CTX_set_private_key_method(ctx, <M as HasPrivateKeyMethods>::METHODS);
            }
        } else {
            unsafe {
                // Safety: we only uninstall the vtable.
                bssl_sys::SSL_CTX_set_private_key_method(ctx, core::ptr::null());
            }
        }
        self.get_context_methods().private_key_methods = key_method;
        self
    }

    /// Set certificate verification mode.
    ///
    /// # Client certificate verification for servers, mutual TLS
    ///
    /// Server can choose to request a certificate from the client by setting `mode` to
    /// - [`CertificateVerificationMode::PeerCertRequested`] which may still let handshake complete
    ///   if the certificate request by the server is not fulfilled.
    /// - [`CertificateVerificationMode::PeerCertMandatory`] which will abort handshake if
    ///   the request is not fulfilled.
    pub fn with_certificate_verification_mode(
        &mut self,
        mode: CertificateVerificationMode,
    ) -> &mut Self {
        let conn = self.ptr();
        unsafe {
            // Safety: we only install our own vtable.
            bssl_sys::SSL_CTX_set_verify(conn, mode as _, None);
        }
        self
    }

    /// Set certificate verification mode and custom verifier.
    ///
    /// # Setting custom certificate verifier
    ///
    /// See [`VerifyCertificate`] for how to implement a custom verifier.
    ///
    /// # Client certificate verification for servers, mutual TLS
    ///
    /// Server can choose to request a certificate from the client by setting `mode` to
    /// - [`CertificateVerificationMode::PeerCertRequested`] which may still let handshake complete
    ///   if the certificate request by the server is not fulfilled.
    /// - [`CertificateVerificationMode::PeerCertMandatory`] which will abort handshake if
    ///   the request is not fulfilled.
    pub fn with_certificate_verifier<V>(
        &mut self,
        mode: CertificateVerificationMode,
        verifier: V,
    ) -> &mut Self
    where
        V: VerifyCertificate + 'static,
    {
        let conn = self.ptr();
        unsafe {
            // Safety: we only install our own vtable.
            bssl_sys::SSL_CTX_set_custom_verify(
                conn,
                mode as _,
                Some(cert_cb::<super::methods::RustContextMethods<M>>),
            );
        }
        self.get_context_methods().verify_certificate_methods = Some(Box::new(verifier) as _);
        self
    }

    /// Remove custom certificate verifier.
    pub fn without_certificate_verifier(&mut self, mode: CertificateVerificationMode) -> &mut Self {
        let conn = self.ptr();
        unsafe {
            // Safety: we only uninstall the vtable.
            bssl_sys::SSL_CTX_set_custom_verify(conn, mode as _, None);
        }
        self.get_context_methods().verify_certificate_methods = None;
        self
    }

    /// Set custom certificate selection callback.
    ///
    /// See [`EarlyCallback`] for its semantics.
    pub fn with_early_callback<S>(&mut self, handler: S) -> &mut Self
    where
        S: EarlyCallback<M> + 'static,
    {
        let ctx = self.ptr();
        unsafe {
            // Safety: we only install our own vtable.
            bssl_sys::SSL_CTX_set_select_certificate_cb(
                ctx,
                Some(early_select_cert_cb::<M, super::methods::RustContextMethods<M>>),
            );
        }
        self.get_context_methods().early_callback_handler = Some(Box::new(handler) as _);
        self
    }

    /// Remove custom certificate selection callback.
    pub fn without_early_callback(&mut self) -> &mut Self {
        let ctx = self.ptr();
        unsafe {
            // Safety: we only uninstall the vtable.
            bssl_sys::SSL_CTX_set_select_certificate_cb(ctx, None);
        }
        self.get_context_methods().early_callback_handler = None;
        self
    }

    /// Append `credential` to the list of credentials of this context.
    pub fn with_credential(&mut self, credential: TlsCredential) -> Result<&mut Self, Error> {
        check_lib_error!(unsafe {
            // Safety:
            // - the validity of the handle `self.0` is witnessed by `self`.
            // - the method will bump the refcount of the credential for ownership.
            bssl_sys::SSL_CTX_add1_credential(self.ptr(), credential.ptr())
        });
        Ok(self)
    }

    /// Set the list of accepted peer certificate types.
    pub fn with_accepted_peer_cert_types(
        &mut self,
        types: &[CertificateType],
    ) -> Result<&mut Self, Error> {
        let (ptr, len) = slice_into_ffi_raw_parts(types);
        check_lib_error!(unsafe {
            // Safety:
            // - `self.ptr()` is a valid `SSL_CTX` handle.
            // - `ptr` is a valid pointer to an array of `i32` representing certificate types.
            // - `len` is the number of elements in the array.
            // - The function copies the data, so the pointer only needs to be valid for the call.
            bssl_sys::SSL_CTX_set1_accepted_peer_cert_types(self.ptr(), ptr as *const _, len)
        });
        Ok(self)
    }

    /// Set the list of available client certificate types.
    pub fn with_available_client_cert_types(
        &mut self,
        types: &[CertificateType],
    ) -> Result<&mut Self, Error> {
        let (ptr, len) = slice_into_ffi_raw_parts(types);
        check_lib_error!(unsafe {
            // Safety:
            // - `self.ptr()` is a valid `SSL_CTX` handle.
            // - `ptr` is a valid pointer to an array of `i32` representing certificate types.
            // - `len` is the number of elements in the array.
            // - The function copies the data, so the pointer only needs to be valid for the call.
            bssl_sys::SSL_CTX_set1_available_client_cert_types(self.ptr(), ptr as *const _, len)
        });
        Ok(self)
    }
}

/// # Certificate verification, X.509
///
/// These methods require built-in X.509 support and are not available for
/// [`TlsExternalVerifierMode`](super::TlsExternalVerifierMode) or
/// [`DtlsExternalVerifierMode`](super::DtlsExternalVerifierMode).
impl<M> TlsContextBuilder<M>
where
    M: super::UseBuiltinX509,
{
    /// Set certificate verification parameters.
    pub fn with_certificate_verification_params(
        &mut self,
        params: &CertificateVerificationParams,
    ) -> Result<&mut Self, Error> {
        check_lib_error!(unsafe {
            // Safety:
            // - the validity of the handle `self.0` is witnessed by `self`.
            // - `SSL_CTX_set1_param` bumps the ref-count on the `params`.
            bssl_sys::SSL_CTX_set1_param(self.ptr(), params.as_ptr())
        });
        Ok(self)
    }

    /// Enable signed certificate timestamps.
    ///
    /// This method will instruct the client connections to request Signed Certificate Timestamps.
    /// See [RFC 6962] for more information.
    ///
    /// [RFC 6962]: https://datatracker.ietf.org/doc/html/rfc6962
    pub fn enable_signed_certificate_timestamps(&mut self) -> &mut Self {
        unsafe {
            // Safety: the validity of the handle `self.0` is witnessed by `self`.
            bssl_sys::SSL_CTX_enable_signed_cert_timestamps(self.ptr());
        }
        self
    }

    /// Set certificate verification store.
    pub fn with_certificate_store(&mut self, store: &X509Store) -> &mut Self {
        unsafe {
            // Safety:
            // - the validity of the handle `self.0` is witnessed by `self`.
            // - `SSL_CTX_set1_verify_cert_store` bumps the ref-count on `store`.
            assert_eq!(
                bssl_sys::SSL_CTX_set1_verify_cert_store(self.ptr(), store.as_mut_ptr()),
                1,
            );
        }
        self
    }
}

/// # Signature algorithm preferences
impl<M> TlsContextBuilder<M> {
    /// Set a preference list of signature algorithms for verification.
    ///
    /// This method returns [`ConfigurationError::InvalidParameters`] if the list of algorithms
    /// contains duplicate entries.
    pub fn with_signature_verification_algorithm_preferences(
        &mut self,
        algs: &[SignatureAlgorithm],
    ) -> Result<&mut Self, Error> {
        let algs: &[u16] = unsafe {
            // Safety: `SignatureAlgorithm` has a `repr(u16)` and maps to preferences correctly
            // by construction.
            core::mem::transmute(algs)
        };
        if has_duplicates(algs) {
            return Err(Error::Configuration(ConfigurationError::InvalidParameters));
        }

        let (prefs, prefs_len) = slice_into_ffi_raw_parts(algs);
        check_lib_error!(unsafe {
            // Safety: the validity of the handle `self.0` is witnessed by `self`.
            bssl_sys::SSL_CTX_set_verify_algorithm_prefs(self.ptr(), prefs, prefs_len)
        });
        Ok(self)
    }

    /// Set a preference list of algorithms for signing.
    ///
    /// This method returns [`ConfigurationError::InvalidParameters`] if the list of algorithms
    /// contains duplicate entries.
    pub fn with_signing_algorithm_preferences(
        &mut self,
        algs: &[SignatureAlgorithm],
    ) -> Result<&mut Self, Error> {
        let algs: &[u16] = unsafe {
            // Safety: `SignatureAlgorithm` has a `repr(u16)` and maps to preferences correctly
            // by construction.
            core::mem::transmute(algs)
        };
        if has_duplicates(algs) {
            return Err(Error::Configuration(ConfigurationError::InvalidParameters));
        }

        let (prefs, prefs_len) = slice_into_ffi_raw_parts(algs);
        check_lib_error!(unsafe {
            // Safety: the validity of the handle `self.0` is witnessed by `self`.
            bssl_sys::SSL_CTX_set_signing_algorithm_prefs(self.ptr(), prefs, prefs_len)
        });
        Ok(self)
    }
}

/// # Certificate authorities - Server
///
/// TLS can send a list of supported certificate authorities to guide the peer in certificate
/// selection.
impl<M> TlsContextBuilder<M> {
    /// This setting advertises the list of certificate authorities names in the
    /// `certificate_authorities` extension to send the client.
    pub fn set_ca_names(
        &mut self,
        names: impl IntoIterator<Item = DistinguishedName>,
    ) -> &mut Self {
        unsafe {
            // Safety: this call only transfers the ownership of the stack.
            bssl_sys::SSL_CTX_set0_client_CAs(
                self.ptr(),
                DistinguishedName::into_crypto_buffer_stack(names),
            )
        }
        self
    }
}
