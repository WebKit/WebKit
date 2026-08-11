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

//! TLS context builder and context type

use alloc::boxed::Box;
use core::{
    marker::PhantomData,
    mem::forget,
    ptr::NonNull, //
};

use bssl_crypto::FfiSlice;

use crate::{
    check_lib_error,
    config::{
        CompliancePolicy,
        ConfigurationError,
        KeyExchangeGroupFlag,
        KeyExchangeGroups,
        ProtocolVersion, //
    },
    connection::{
        TlsConnectionBuilder,
        methods::HasTlsConnectionMethod, //
    },
    context::methods::HasTlsContextMethod,
    errors::Error,
    has_duplicates, //
};

pub use crate::connection::{
    Client,
    Server, //
};

mod credentials;
mod methods;
mod sessions;

/// TLS or DTLS mode
pub enum TlsMode {}

/// DTLS mode
pub enum DtlsMode {}

/// QUIC mode
pub enum QuicMode {}

pub(crate) trait HasBasicIo {}

/// A collection of supported mode of operations.
pub trait SupportedMode: HasTlsContextMethod + HasTlsConnectionMethod {}

impl SupportedMode for TlsMode {}
impl SupportedMode for DtlsMode {}
impl SupportedMode for QuicMode {}

impl HasBasicIo for TlsMode {}
impl HasBasicIo for DtlsMode {}

/// General TLS configuration
///
/// The `Mode` generic can be either [`TlsMode`] or [`QuicMode`].
/// This generic governs the kind of [`TlsConnection`] that can be constructed.
pub struct TlsContextBuilder<Mode = TlsMode> {
    ptr: NonNull<bssl_sys::SSL_CTX>,
    _p: PhantomData<fn() -> Mode>,
}

impl<M> TlsContextBuilder<M> {
    fn ptr(&self) -> *mut bssl_sys::SSL_CTX {
        self.ptr.as_ptr()
    }
}

impl<M> TlsContextBuilder<M>
where
    M: HasTlsContextMethod,
{
    fn new_inner(method: *const bssl_sys::SSL_METHOD) -> Self {
        let Some(ptr) = NonNull::new(unsafe {
            // Safety: this call only makes allocations
            bssl_sys::SSL_CTX_new(method)
        }) else {
            panic!("allocation failure")
        };
        let this = TlsContextBuilder {
            ptr,
            _p: PhantomData,
        };
        let rc = unsafe {
            // Safety: `ctx` is still valid
            bssl_sys::SSL_CTX_set_ex_data(
                ptr.as_ptr(),
                M::registration(),
                Box::into_raw(Box::new(methods::RustContextMethods::<M>::new())) as _,
            )
        };
        assert!(rc == 1);
        this
    }
}

/// # Make a TLS context builder
impl TlsContextBuilder<TlsMode> {
    /// Creates a new TLS context builder.
    pub fn new_tls() -> Self {
        Self::new_inner(unsafe {
            // Safety: this call returns a static immutable data
            bssl_sys::TLS_method()
        })
    }
}

/// # Make a DTLS context builder operating in pure DTLS mode
impl TlsContextBuilder<DtlsMode> {
    /// Creates a new DTLS context builder.
    pub fn new_dtls() -> Self {
        Self::new_inner(unsafe {
            // Safety: this call returns a static immutable data
            bssl_sys::DTLS_method()
        })
    }
}

/// # Configure the context through a context builder
impl<M> TlsContextBuilder<M>
where
    M: HasTlsContextMethod,
{
    /// Builds and returns the configured TLS context.
    pub fn build(self) -> TlsContext<M> {
        let ptr = self.ptr;
        // We must disarm the drop activated by the builder and pass on the ownership.
        // Now `self` has no destructors to call.
        forget(self);
        TlsContext {
            ptr,
            _p: PhantomData,
        }
    }

    fn get_context_methods(&mut self) -> &mut methods::RustContextMethods<M> {
        let methods = unsafe {
            // Safety: the validity of the handle `self.0` is witnessed by `self`.
            bssl_sys::SSL_CTX_get_ex_data(self.ptr(), M::registration())
        };
        if methods.is_null() {
            panic!("context method goes missing")
        }
        unsafe {
            // Safety: `methods` must be constructed by `new_inner`
            &mut *(methods as *mut methods::RustContextMethods<M>)
        }
    }

    /// Set the minimum acceptable protocol.
    ///
    /// If `version` is set to `None`, the library will choose a default minimum version.
    /// - For TLS, it is TLS 1.3
    /// - For DTLS, it is DTLS 1.2
    ///
    /// This configuration fails if the protocol version is incompatible with the selected TLS
    /// flavour, such as setting TLS 1.3 for DTLS flavour.
    pub fn with_min_protocol(
        &mut self,
        version: Option<ProtocolVersion>,
    ) -> Result<&mut Self, Error> {
        let version = version.map_or(0, |ver| ver as u16);
        check_lib_error!(unsafe {
            // Safety: the validity of the handle `self.0` is witnessed by `self`
            bssl_sys::SSL_CTX_set_min_proto_version(self.ptr(), version)
        });
        Ok(self)
    }

    /// Set the maximum acceptable protocol.
    ///
    /// If `version` is set to `None`, the library will choose a default maximum version.
    /// - For TLS, it is TLS 1.3
    /// - For DTLS, it is DTLS 1.2
    ///
    /// This configuration fails if the protocol version is incompatible with the selected TLS
    /// flavour, such as setting TLS 1.3 for DTLS flavour.
    pub fn with_max_protocol(
        &mut self,
        version: Option<ProtocolVersion>,
    ) -> Result<&mut Self, Error> {
        let version = version.map_or(0, |ver| ver as u16);
        check_lib_error!(unsafe {
            // Safety: the validity of the handle `self.0` is witnessed by `self`
            bssl_sys::SSL_CTX_set_max_proto_version(self.ptr(), version)
        });
        Ok(self)
    }

    /// Set acceptable key exchange groups.
    ///
    /// This method sets up the acceptable key exchange groups and the list of groups advertised by
    /// the client.
    ///
    /// If the supplied list is empty, the context will revert back to a default list of groups.
    ///
    /// If the `flags` list is not empty, its length must match that of `groups`.
    ///
    /// This method returns [`ConfigurationError::InvalidParameters`] if the key exchange groups are not unique in the list.
    pub fn with_key_exchange_groups(
        &mut self,
        groups: &[KeyExchangeGroups],
        flags: &[KeyExchangeGroupFlag],
    ) -> Result<&mut Self, Error> {
        // TODO(@xfding): maybe we should use zerocopy here when v0.9 is finalised
        let groups: &[u16] = unsafe {
            // Safety: `KeyExchangeGroups` has the same layout as a `u16`,
            // so `&[KeyExchangeGroups]` and `&[u16]` has the same layout.
            core::mem::transmute(groups)
        };
        let flags: &[u32] = unsafe {
            // Safety: `KeyExchangeGroupFlag` has the same layout as a `u32` through
            // `repr(transparent)`.
            core::mem::transmute(flags)
        };
        if !flags.is_empty() && groups.len() != flags.len() {
            return Err(Error::Configuration(ConfigurationError::InvalidParameters));
        }
        if has_duplicates(groups) {
            return Err(Error::Configuration(
                ConfigurationError::DuplicatedParameters,
            ));
        }

        check_lib_error!(unsafe {
            // Safety: the validity of the handle `self.0` is witnessed by `self`
            if flags.is_empty() {
                bssl_sys::SSL_CTX_set1_group_ids(self.ptr(), groups.as_ffi_ptr(), groups.len())
            } else {
                bssl_sys::SSL_CTX_set1_group_ids_with_flags(
                    self.ptr(),
                    groups.as_ffi_ptr(),
                    flags.as_ffi_ptr(),
                    groups.len(),
                )
            }
        });
        Ok(self)
    }

    /// Set a list of cipher suites, in the order of descending preference.
    ///
    /// This method accepts a string that conforms to OpenSSL [cipher suite configuration] language
    /// specification.
    ///
    /// # Available cipher rules are:
    ///
    /// - `ALL` matches all ciphers, except for deprecated ciphers which must be
    ///   named explicitly.
    ///
    /// - `kRSA`, `kDHE`, `kECDHE`, and `kPSK` match ciphers using plain RSA, DHE,
    ///   ECDHE, and plain PSK key exchanges, respectively. Note that ECDHE_PSK is
    ///   matched by `kECDHE` and not `kPSK`.
    ///
    /// - `aRSA`, `aECDSA`, and `aPSK` match ciphers authenticated by RSA, ECDSA, and
    ///   a pre-shared key, respectively.
    ///
    /// - `RSA`, `DHE`, `ECDHE`, `PSK`, `ECDSA`, and `PSK` are aliases for the
    ///   corresponding `k*` or `a*` cipher rule. `RSA` is an alias for `kRSA`, not
    ///   `aRSA`.
    ///
    /// - `3DES`, `AES128`, `AES256`, `AES`, `AESGCM`, `CHACHA20` match ciphers
    ///   whose bulk cipher use the corresponding encryption scheme. Note that
    ///   `AES`, `AES128`, and `AES256` match both CBC and GCM ciphers.
    ///
    /// - `SHA1`, and its alias `SHA`, match legacy cipher suites using HMAC-SHA1.
    ///
    /// # Deprecated cipher rules:
    ///
    /// - `kEDH`, `EDH`, `kEECDH`, and `EECDH` are legacy aliases for `kDHE`, `DHE`,
    ///   `kECDHE`, and `ECDHE`, respectively.
    ///
    /// - `HIGH` is an alias for `ALL`.
    ///
    /// - `FIPS` is an alias for `HIGH`.
    ///
    /// - `SSLv3` and `TLSv1` match ciphers available in TLS 1.1 or earlier.
    ///   `TLSv1_2` matches ciphers new in TLS 1.2. This is confusing and should not
    ///   be used.
    ///
    /// [cipher suite configuration]: <https://docs.openssl.org/3.0/man1/openssl-ciphers/#cipher-list-format>
    pub fn with_ciphers(&mut self, cipher_ops: &str) -> Result<&mut Self, Error> {
        let Ok(cipher_ops) = alloc::ffi::CString::new(cipher_ops) else {
            return Err(Error::Configuration(ConfigurationError::InvalidString));
        };
        check_lib_error!(unsafe {
            // Safety:
            // - the validity of the handle `self.0` is witnessed by `self`;
            // - the string has been checked for internal NUL bytes and NUL-terminated.
            bssl_sys::SSL_CTX_set_strict_cipher_list(self.ptr(), cipher_ops.as_ptr())
        });
        Ok(self)
    }

    /// Set TLS 1.2 session lifetime in seconds.
    ///
    /// If 0 seconds are specified, BoringSSL will pick a default timeout for the connections.
    pub fn with_tls12_timeout(&mut self, seconds: u32) -> &mut Self {
        unsafe {
            // Safety: the validity of the handle `self.0` is witnessed by `self`.
            bssl_sys::SSL_CTX_set_timeout(self.ptr(), seconds)
        };
        self
    }

    /// Set TLS 1.3 session lifetime in seconds.
    ///
    /// If 0 seconds are specified, BoringSSL will pick a default timeout for the connections.
    pub fn with_tls13_timeout(&mut self, seconds: u32) -> &mut Self {
        unsafe {
            // Safety: the validity of the handle `self.0` is witnessed by `self`.
            bssl_sys::SSL_CTX_set_session_psk_dhe_timeout(self.ptr(), seconds)
        };
        self
    }

    /// Set whether shutting down the connection sends out a `close_notify` alert.
    pub fn with_quiet_shutdown(&mut self, quiet: bool) -> &mut Self {
        unsafe {
            // Safety: the validity of the handle `self.0` is witnessed by `self`.
            bssl_sys::SSL_CTX_set_quiet_shutdown(self.ptr(), if quiet { 1 } else { 0 });
        }
        self
    }

    /// Set whether only the SHA-256 hash of the peer's certificate should be saved in the session.
    ///
    /// This can save memory, ticket size and session cache space.
    pub fn with_retain_only_sha256_of_client_certs(&mut self, enable: bool) -> &mut Self {
        unsafe {
            // Safety: the validity of the handle `self.ptr` is witnessed by `self`.
            bssl_sys::SSL_CTX_set_retain_only_sha256_of_client_certs(
                self.ptr(),
                if enable { 1 } else { 0 },
            );
        }
        self
    }
}

impl<M> Drop for TlsContextBuilder<M> {
    fn drop(&mut self) {
        unsafe {
            // Safety: self.0 is created from SSL_CTX_new so it is a valid pointer
            bssl_sys::SSL_CTX_free(self.ptr());
        }
    }
}

/// A TLS context that is finalised and can be shared across connections
#[repr(transparent)]
pub struct TlsContext<M = TlsMode> {
    ptr: NonNull<bssl_sys::SSL_CTX>,
    _p: PhantomData<fn() -> M>,
}

impl<M> TlsContext<M> {
    pub(crate) fn ptr(&self) -> *mut bssl_sys::SSL_CTX {
        self.ptr.as_ptr()
    }
}

/// # Create new connections associated to a context
impl<M> TlsContext<M>
where
    M: HasTlsContextMethod + HasTlsConnectionMethod,
{
    fn new_connection(
        &self,
        compliance_policy: Option<CompliancePolicy>,
    ) -> NonNull<bssl_sys::SSL> {
        let conn = unsafe {
            // Safety: in this type-state, our SSL_CTX is effectively immutable,
            // so we can freely alias.
            bssl_sys::SSL_new(self.ptr())
        };
        if let Some(policy) = compliance_policy {
            unsafe {
                // Safety: `policy` is a valid enum value per construction.
                bssl_sys::SSL_set_compliance_policy(conn, policy as _);
            }
        }
        NonNull::new(conn).expect("allocation failure")
    }

    /// Make a new client-half connection inheriting the configuration of this context
    ///
    /// By default, this method will configure the connection to request certificates
    /// from the server.
    /// To override this default, use
    /// [`TlsConnectionBuilder::with_certificate_verification_mode`].
    pub fn new_client_connection(
        &self,
        compliance_policy: Option<CompliancePolicy>,
    ) -> Result<TlsConnectionBuilder<Client, M>, Error> {
        let conn = self.new_connection(compliance_policy);
        unsafe {
            // Safety: the connection is still valid here
            bssl_sys::SSL_set_connect_state(conn.as_ptr());
        }
        let mut builder = TlsConnectionBuilder::from_ssl(conn);
        // The safe default is that the client should perform at least
        // some certification verification.
        builder.with_certificate_verification_mode(
            crate::credentials::CertificateVerificationMode::PeerCertRequested,
        );
        Ok(builder)
    }

    /// Make a new server-half connection inheriting the configuration of this context
    pub fn new_server_connection(
        &self,
        compliance_policy: Option<CompliancePolicy>,
    ) -> Result<TlsConnectionBuilder<Server, M>, Error> {
        let conn = self.new_connection(compliance_policy);
        unsafe {
            // Safety: the connection is still valid here
            bssl_sys::SSL_set_accept_state(conn.as_ptr());
        }
        Ok(TlsConnectionBuilder::from_ssl(conn))
    }

    /// Expose the fully built BoringSSL's `SSL_CTX` pointer.
    ///
    /// # Safety
    /// - `SSL_CTX` is **not fully thread-safe**; do not invoke any BoringSSL mutating functions
    ///   on the returned handle; otherwise, it is **undefined behaviour**.
    /// - interacting with the `ex_data` of the `SSL_CTX` instance that this method returns is
    ///   **undefined behaviour**.
    /// - `self` must outlive all uses of the returned handle;
    /// - this handle must be used with functions from the BoringSSL library this crate is linked
    ///   to; otherwise, it is **undefined behaviour**.
    ///
    /// # Recommendation
    /// If the handle is used to interface BoringSSL library functions, please consider using a
    /// corresponding Rust safe binding or contact BoringSSL Authors for support.
    pub unsafe fn as_mut_ptr(&mut self) -> *mut bssl_sys::SSL_CTX {
        self.ptr()
    }
}

/// Safety: at this type state, most of the underlying context is immutable,
/// while methods on shared accesses are protected behind a mutex, so they are thread-safe.
unsafe impl<M> Send for TlsContext<M> {}
unsafe impl<M> Sync for TlsContext<M> {}

impl<M> Drop for TlsContext<M> {
    fn drop(&mut self) {
        unsafe {
            // Safety: this handle is taken from the builder, so it must be
            // live and valid.
            bssl_sys::SSL_CTX_free(self.ptr());
        }
    }
}

impl<M> Clone for TlsContext<M> {
    fn clone(&self) -> Self {
        unsafe {
            // Safety: this handle is already valid by the witness of self.
            bssl_sys::SSL_CTX_up_ref(self.ptr());
            // BoringSSL will always return success on bumping reference.
        }
        TlsContext {
            ptr: self.ptr,
            _p: PhantomData,
        }
    }
}

/// A reusable certificate cache shared across [`TlsContext`]
pub struct CertificateCache(pub(crate) NonNull<bssl_sys::CRYPTO_BUFFER_POOL>);

// Safety: `CRYPTO_BUFFER_POOL` is mutex-protected
unsafe impl Send for CertificateCache {}
unsafe impl Sync for CertificateCache {}

impl CertificateCache {
    /// Construct a new certificate cache.
    pub fn new() -> Self {
        let pool = unsafe {
            // Safety: this call does not have side-effect other than allocation
            bssl_sys::CRYPTO_BUFFER_POOL_new()
        };
        Self(NonNull::new(pool).expect("allocation failure"))
    }

    pub(crate) fn ptr(&self) -> *mut bssl_sys::CRYPTO_BUFFER_POOL {
        self.0.as_ptr()
    }
}

impl Drop for CertificateCache {
    fn drop(&mut self) {
        unsafe {
            // Safety: the validity of `self.0` is witnessed by `self`
            bssl_sys::CRYPTO_BUFFER_POOL_free(self.ptr());
        }
    }
}

impl Clone for CertificateCache {
    fn clone(&self) -> Self {
        unsafe {
            // Safety: the validity of `self.0` is witnessed by `self`
            bssl_sys::CRYPTO_BUFFER_POOL_up_ref(self.ptr());
        }
        Self(self.0)
    }
}
