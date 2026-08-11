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

use alloc::{
    boxed::Box,
    ffi::CString, //
};
use core::{
    ffi::CStr,
    ptr::null, //
};

use bssl_x509::{params::CertificateVerificationParams, store::X509Store};

use super::{
    Client,
    methods::HasTlsConnectionMethod, //
};
use crate::{
    check_lib_error,
    config::ConfigurationError,
    connection::{
        TlsConnection,
        TlsConnectionBuilder,
        lifecycle::{
            EstablishedTlsConnection,
            TlsConnectionInHandshake, //
        }, //
    }, //
    credentials::{
        CertificateType,
        CertificateVerificationMode,
        SignatureAlgorithm,
        TlsCredential,
        VerifyCertificate,
        cert_cb, //
    },
    errors::Error,
    ffi::slice_into_ffi_raw_parts,
    has_duplicates, //
};

impl<R, M> TlsConnectionBuilder<R, M>
where
    M: HasTlsConnectionMethod,
{
    /// Configure the certificate verification mode.
    pub fn with_certificate_verification_mode(
        &mut self,
        mode: CertificateVerificationMode,
    ) -> &mut Self {
        let ctx = self.ptr();
        unsafe {
            // Safety: this method only updates the mode value.
            bssl_sys::SSL_set_verify(ctx, mode as _, None);
        }
        self
    }

    /// Configure the certificate verifier.
    ///
    /// See [`VerifyCertificate`] for how to implement a custom verifier.
    ///
    /// If raw public key authentication, per [RFC 7250], is configured,
    /// the authentication through this mechanism will **fail** unless a certificate verifier
    /// is configured.
    ///
    /// [RFC 7250]: <https://datatracker.ietf.org/doc/html/rfc7250>
    pub fn with_certificate_verifier<V>(
        &mut self,
        mode: CertificateVerificationMode,
        verifier: V,
    ) -> &mut Self
    where
        V: VerifyCertificate + 'static,
    {
        let ctx = self.ptr();
        unsafe {
            // Safety: we only install our own vtable.
            bssl_sys::SSL_set_custom_verify(
                ctx,
                mode as _,
                Some(cert_cb::<super::methods::RustConnectionMethods<M>>),
            );
        }
        self.get_connection_methods().verify_certificate_methods = Some(Box::new(verifier) as _);
        self
    }

    /// Remove custom certificate verifier.
    pub fn without_certificate_verifier(&mut self, mode: CertificateVerificationMode) -> &mut Self {
        let ctx = self.ptr();
        unsafe {
            // Safety: we only uninstall the vtable.
            bssl_sys::SSL_set_custom_verify(ctx, mode as _, None);
        }
        self.get_connection_methods().verify_certificate_methods = None;
        self
    }
}

impl<M> TlsConnectionBuilder<Client, M>
where
    M: HasTlsConnectionMethod,
{
    /// Set the list of available client certificate types.
    pub fn with_available_client_cert_types(
        &mut self,
        types: &[CertificateType],
    ) -> Result<&mut Self, Error> {
        let (ptr, len) = slice_into_ffi_raw_parts(types);
        check_lib_error!(unsafe {
            // Safety:
            // - `self.ptr()` is a valid `SSL` handle.
            // - `ptr` is a valid pointer to an array of `i32` representing certificate types.
            // - `len` is the number of elements in the array.
            // - The function copies the data, so the pointer only needs to be valid for the call.
            bssl_sys::SSL_set1_available_client_cert_types(self.ptr(), ptr as *const _, len)
        });
        Ok(self)
    }
}

/// # Custom certificate verification
impl<M> TlsConnectionInHandshake<'_, Client, M>
where
    M: HasTlsConnectionMethod,
{
    /// Get the certificate verification mode set by [`Self::set_certificate_verification_mode`].
    pub fn get_certificate_verification_mode(&self) -> Option<CertificateVerificationMode> {
        unsafe {
            // Safety: the validity of the handle `self.0` is witnessed by `self`.
            bssl_sys::SSL_get_verify_mode(self.ptr())
        }
        .try_into()
        .ok()
    }
}

/// # Authenticating with the peer
impl<M> TlsConnectionInHandshake<'_, Client, M>
where
    M: HasTlsConnectionMethod,
{
    /// Append `credential` to the list of credentials of this connection.
    ///
    /// Earlier calls to this method appends a credential that is preferred over those added
    /// in the later calls.
    pub fn add_credential(&mut self, credential: &TlsCredential) -> Result<&mut Self, Error> {
        check_lib_error!(unsafe {
            // Safety: `credential` is still valid.
            bssl_sys::SSL_add1_credential(self.ptr(), credential.ptr())
        });
        Ok(self)
    }

    /// Clear all credentials.
    pub fn clear_credentials(&mut self) -> &mut Self {
        unsafe {
            // Safety: `credential` is still valid.
            bssl_sys::SSL_certs_clear(self.ptr());
        }
        self
    }
}

/// # Certificate verification
impl<M> TlsConnectionInHandshake<'_, Client, M> {
    /// Enable signed certificate timestamps.
    ///
    /// This method will instruct the client connections to request Signed Certificate Timestamps.
    /// See [RFC 6962] for more information.
    ///
    /// [RFC 6962]: <https://datatracker.ietf.org/doc/html/rfc6962>
    pub fn enable_signed_certificate_timestamps(&mut self) -> &mut Self {
        unsafe {
            // Safety: the validity of the handle `self.0` is witnessed by `self`.
            bssl_sys::SSL_enable_signed_cert_timestamps(self.ptr());
        }
        self
    }
}

/// # Certificate verification
impl<M> TlsConnectionInHandshake<'_, Client, M> {
    /// Set certificate verification store.
    pub fn set_certificate_store(&mut self, store: &X509Store) -> &mut Self {
        unsafe {
            // Safety:
            // - the validity of the handle `self.0` is witnessed by `self`.
            // - when pending handshake, the assignment is always successful.
            // - `SSL_set1_verify_cert_store` bumps the ref-count on the store.
            bssl_sys::SSL_set1_verify_cert_store(self.ptr(), store.as_mut_ptr());
        }
        self
    }

    /// Set a preference list of signature algorithms.
    ///
    /// This method returns [`ConfigurationError::InvalidParameters`] if the list of algorithms
    /// contains duplicate entries.
    pub fn set_certificate_verification_preferences(
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
            bssl_sys::SSL_set_verify_algorithm_prefs(self.ptr(), prefs, prefs_len)
        });
        Ok(self)
    }
}

/// # Certificate verification.
impl<M> TlsConnectionInHandshake<'_, Client, M> {
    /// Set host name.
    pub fn set_host(&mut self, host_name: &str) -> Result<&mut Self, Error> {
        let host_name = CString::new(host_name)
            .map_err(|_| Error::Configuration(ConfigurationError::InvalidString))?;
        check_lib_error!(unsafe {
            // Safety:
            // - the validity of the handle `self.0` is witnessed by `self`.
            // - the host name string has been sanitised for internal NUL-bytes and NUL-terminated.
            bssl_sys::SSL_set1_host(self.ptr(), host_name.as_ptr())
        });
        Ok(self)
    }
}

/// # Certificate verification.
impl<R, M> TlsConnectionInHandshake<'_, R, M> {
    /// Set depth of a potential certificate chain acceptable.
    pub fn set_verify_depth(&mut self, depth: u16) -> Result<&mut Self, Error> {
        let depth = depth
            .try_into()
            .map_err(|_| Error::Configuration(ConfigurationError::ValueOutOfRange))?;
        unsafe {
            // Safety: the validity of the handle `self.0` is witnessed by `self`.
            bssl_sys::SSL_set_verify_depth(self.ptr(), depth);
        }
        Ok(self)
    }

    /// Get certificate verification depth.
    ///
    /// This method returns [`None`] if the depth is set but does not fit in a [`u16`].
    pub fn get_verify_depth(&self) -> Option<u16> {
        unsafe {
            // Safety: the validity and state of the handle `self.0` is witnessed by `self`.
            bssl_sys::SSL_get_verify_depth(self.ptr()).try_into().ok()
        }
    }

    /// Set certificate verification parameters.
    pub fn set_certificate_verification_params(
        &mut self,
        params: &CertificateVerificationParams,
    ) -> Result<&mut Self, Error> {
        check_lib_error!(unsafe {
            // Safety:
            // - the validity of the handle `self.0` is witnessed by `self`.
            // - `SSL_set1_param` claims shared ownership of `params` by bumping ref-count.
            bssl_sys::SSL_set1_param(self.ptr(), params.as_ptr())
        });
        Ok(self)
    }
}

impl<'a, R, M> EstablishedTlsConnection<'a, R, M> {
    /// Export keying material from this connection into a buffer of a chosen length,
    /// as per [RFC 5705].
    ///
    /// To derive the same value, both sides of a connection must use the same output length, label
    /// and context.
    /// - In TLS 1.2 and earlier, using a zero-length context and using no context would give
    /// different output.
    /// - In TLS 1.3 and later, the output length controls the derivation so that a truncated
    /// longer export will not match a shorter export.
    ///
    /// [RFC 5705]: <https://datatracker.ietf.org/doc/html/rfc5705>
    pub fn export_keying_material(
        &self,
        label: &CStr,
        context: Option<&[u8]>,
        output: &mut [u8],
    ) -> Result<(), Error> {
        let (context, context_len, use_context) = if let Some(context) = context {
            let (context, context_len) = slice_into_ffi_raw_parts(context);
            (context, context_len, 1)
        } else {
            (null(), 0, 0)
        };
        let (output, output_len) = crate::ffi::mut_slice_into_ffi_raw_parts(output);
        check_lib_error!(unsafe {
            // Safety:
            // - the validity of the handle `self.0` is witnessed by `self`.
            bssl_sys::SSL_export_keying_material(
                self.ptr(),
                output,
                output_len,
                label.as_ptr(),
                label.count_bytes(),
                context,
                context_len,
                use_context,
            )
        });
        Ok(())
    }
}

impl<R, M> TlsConnection<R, M> {
    /// Get the peer's [`CertificateType`].
    pub fn get_peer_certificate_type(&self) -> Option<CertificateType> {
        let ty = unsafe {
            // Safety:
            // - `self.ptr()` is a valid `SSL` handle.
            bssl_sys::SSL_get_peer_cert_type(self.ptr())
        };
        ty.try_into().ok().and_then(|ty: u8| ty.try_into().ok())
    }

    /// Get the peer's raw public key as DER-encoded SubjectPublicKeyInfo.
    pub fn get_peer_raw_public_key(&self) -> Option<Vec<u8>> {
        let pkey = unsafe {
            // Safety:
            // - `self.ptr()` is a valid `SSL` handle.
            // - `pkey` does not escape the current function frame.
            bssl_sys::SSL_get0_peer_rpk(self.ptr())
        };
        if pkey.is_null() {
            return None;
        }

        let buffer = bssl_crypto::cbb_to_buffer(64, |cbb| {
            assert_eq!(
                unsafe {
                    // Safety:
                    // - `cbb` is a valid pointer to `CBB` provided by `cbb_to_buffer`.
                    // - `pkey` is a valid pointer to `EVP_PKEY`.
                    bssl_sys::EVP_marshal_public_key(cbb, pkey)
                },
                1
            );
        });
        Some(buffer.as_ref().to_vec())
    }
}
