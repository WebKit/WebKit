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

//! Select Certificate Callback types

use core::{
    marker::PhantomData,
    mem::transmute,
    ptr::{
        NonNull, //
        null,
    }, //
};

use crate::{
    EarlyCallbackMethods,
    abort_on_panic,
    config::CipherSuite,
    connection::{
        Server,
        lifecycle::TlsConnectionInHandshake, //
    },
    ffi::sanitize_slice, //
};

bssl_macros::bssl_enum! {
    /// Results from selecting a certificate in the callback.
    pub enum EarlyCallbackResult: i32 {
        /// The certificate selection was successful.
        Success = bssl_sys::ssl_select_cert_result_t_ssl_select_cert_success as i32,
        /// The operation could not be immediately completed and must be reattempted at a later point.
        Retry = bssl_sys::ssl_select_cert_result_t_ssl_select_cert_retry as i32,
        /// A fatal error occurred and the handshake should be terminated.
        ErrorResult = bssl_sys::ssl_select_cert_result_t_ssl_select_cert_error as i32,
        /// Disable ECH.
        /// The callback should return this result when an encrypted `ClientHelloInner` was
        /// decrypted but should be discarded.
        /// In this case, the callback will be called again with `ClientHelloOuter` as input
        /// instead and the handshake will proceed with `retry_config`s to signal to the client that
        /// ECH is disabled.
        /// This value may only be returned when `SSL_ech_accepted` returns one.
        /// It may be useful if the `ClientHelloInner` indicated lack of ECH support in the service,
        /// for example if it is a TLS-1.2 only service.
        DisableEch = bssl_sys::ssl_select_cert_result_t_ssl_select_cert_disable_ech as i32,
    }
}

/// TLS Extension Types.
///
/// See [IANA assignment](https://www.iana.org/assignments/tls-extensiontype-values/tls-extensiontype-values.xhtml#tls-extensiontype-values-1)
#[derive(Clone, Copy, PartialEq, Eq, Hash, Debug)]
#[repr(transparent)]
pub struct ExtensionType(pub u16);

impl ExtensionType {
    /// Server Name Indication
    ///
    /// [RFC 6066, §3](https://datatracker.ietf.org/doc/html/rfc6066#section-3)
    pub const SERVER_NAME: Self = Self(bssl_sys::TLSEXT_TYPE_server_name as u16);

    /// Status Request
    ///
    /// [RFC 6066, §8](https://datatracker.ietf.org/doc/html/rfc6066#section-8)
    pub const STATUS_REQUEST: Self = Self(bssl_sys::TLSEXT_TYPE_status_request as u16);

    /// Supported Groups
    ///
    /// [RFC 8446, §4.2.7](https://datatracker.ietf.org/doc/html/rfc8446#section-4.2.7)
    pub const SUPPORTED_GROUPS: Self = Self(bssl_sys::TLSEXT_TYPE_supported_groups as u16);

    /// Signature Algorithms
    ///
    /// [RFC 8446, §4.2.3](https://datatracker.ietf.org/doc/html/rfc8446#section-4.2.3)
    pub const SIGNATURE_ALGORITHMS: Self = Self(bssl_sys::TLSEXT_TYPE_signature_algorithms as u16);

    /// Application Layer Protocol Negotiation
    ///
    /// [RFC 7301, §3](https://datatracker.ietf.org/doc/html/rfc7301#section-3)
    pub const ALPN: Self =
        Self(bssl_sys::TLSEXT_TYPE_application_layer_protocol_negotiation as u16);

    /// Key Share
    ///
    /// [RFC 8446, §4.2.8](https://datatracker.ietf.org/doc/html/rfc8446#section-4.2.8)
    pub const KEY_SHARE: Self = Self(bssl_sys::TLSEXT_TYPE_key_share as u16);

    /// Supported Versions
    ///
    /// [RFC 8446, §4.2.1](https://datatracker.ietf.org/doc/html/rfc8446#section-4.2.1)
    pub const SUPPORTED_VERSIONS: Self = Self(bssl_sys::TLSEXT_TYPE_supported_versions as u16);

    /// Pre-Shared Key
    ///
    /// [RFC 8446, §4.2.11](https://datatracker.ietf.org/doc/html/rfc8446#section-4.2.11)
    pub const PRE_SHARED_KEY: Self = Self(bssl_sys::TLSEXT_TYPE_pre_shared_key as u16);

    /// Cookie
    ///
    /// [RFC 8446, §4.2.2](https://datatracker.ietf.org/doc/html/rfc8446#section-4.2.2)
    pub const COOKIE: Self = Self(bssl_sys::TLSEXT_TYPE_cookie as u16);

    /// PSK Key Exchange Modes
    ///
    /// [RFC 8446, §4.2.9](https://datatracker.ietf.org/doc/html/rfc8446#section-4.2.9)
    pub const PSK_KEY_EXCHANGE_MODES: Self =
        Self(bssl_sys::TLSEXT_TYPE_psk_key_exchange_modes as u16);

    /// Certificate Compression
    ///
    /// [RFC 8879](https://datatracker.ietf.org/doc/html/rfc8879)
    pub const CERT_COMPRESSION: Self = Self(bssl_sys::TLSEXT_TYPE_cert_compression as u16);

    /// Session Ticket
    ///
    /// [RFC 5077](https://datatracker.ietf.org/doc/html/rfc5077)
    pub const SESSION_TICKET: Self = Self(bssl_sys::TLSEXT_TYPE_session_ticket as u16);

    /// Extended Master Secret
    ///
    /// [RFC 7627](https://datatracker.ietf.org/doc/html/rfc7627)
    pub const EXTENDED_MASTER_SECRET: Self =
        Self(bssl_sys::TLSEXT_TYPE_extended_master_secret as u16);

    /// Client Certificate Type
    ///
    /// [RFC 7250](https://datatracker.ietf.org/doc/html/rfc7250)
    pub const CLIENT_CERT_TYPE: Self = Self(bssl_sys::TLSEXT_TYPE_client_cert_type as u16);

    /// Server Certificate Type
    ///
    /// [RFC 7250](https://datatracker.ietf.org/doc/html/rfc7250)
    pub const SERVER_CERT_TYPE: Self = Self(bssl_sys::TLSEXT_TYPE_server_cert_type as u16);
}

/// A wrapper around the `ClientHello` data passed to the select certificate callback.
///
/// The structure is prescribed in [RFC 8446] §4.1.2.
///
/// [RFC 8446]: <https://datatracker.ietf.org/doc/html/rfc8446#section-4.1.2>
pub struct ClientHello<'a, M> {
    ptr: *const bssl_sys::SSL_CLIENT_HELLO,
    conn: NonNull<bssl_sys::SSL>,
    _p: PhantomData<&'a M>,
}

macro_rules! client_hello_getter {
    ($client_hello:expr, $data:ident, $len:ident) => {
        unsafe {
            // Safety: `$client_hello` is a valid pointer
            sanitize_slice($client_hello.$data, $client_hello.$len)?
        }
    };
}

impl<'a, M> ClientHello<'a, M> {
    /// Exposes a mutable reference to the associated server connection in handshake.
    ///
    /// The return handle has full capability to configure your server-side connection for the
    /// subsequent handshake.
    /// This handle can be used to set connection-specific credentials or certificates during the
    /// callback, for example.
    pub fn connection_mut(&mut self) -> TlsConnectionInHandshake<'_, Server, M> {
        unsafe {
            // Safety: `self.conn` is a `repr(transparent)` wrapper around a `NonNull<SSL>`.
            let conn_ref = transmute(&mut self.conn);
            TlsConnectionInHandshake(conn_ref)
        }
    }

    /// Get the client random bytes of length 32.
    ///
    /// This method returns [`None`] if the random bytes are absent or of wrong length.
    pub fn random(&self) -> Option<[u8; 32]> {
        let random = client_hello_getter!(*self.ptr, random, random_len);
        random.try_into().ok()
    }

    /// Get the legacy session ID bytes.
    pub fn legacy_session_id(&self) -> Option<[u8; 32]> {
        let session_id = client_hello_getter!(*self.ptr, session_id, session_id_len);
        session_id.try_into().ok()
    }

    /// Get the cipher suites bytes.
    ///
    /// This method returns [`None`] if the `cipher_suites` field is absent or contains error while
    /// parsing.
    pub fn cipher_suites(&self) -> Option<Vec<CipherSuite>> {
        let cipher_suites = client_hello_getter!(*self.ptr, cipher_suites, cipher_suites_len);
        if cipher_suites.len() % 2 != 0 {
            return None;
        }
        let mut result = Vec::with_capacity(cipher_suites.len() / 2);
        for suite in cipher_suites.chunks_exact(2) {
            let suite = u16::from_be_bytes(
                suite
                    .try_into()
                    .expect("we have checked that length of cipher_suites is multiple of 2"),
            );
            result.push(CipherSuite(suite));
        }
        Some(result)
    }

    /// Get the legacy compression methods bytes.
    pub fn legacy_compression_methods(&self) -> &'a [u8] {
        unsafe {
            // Safety: `self.ptr` is a valid pointer to `SSL_CLIENT_HELLO` provided by BoringSSL.
            sanitize_slice(
                (*self.ptr).compression_methods,
                (*self.ptr).compression_methods_len,
            )
            .unwrap_or(&[])
        }
    }

    /// Get the extensions bytes.
    pub fn extensions(&self) -> &'a [u8] {
        unsafe {
            // Safety: `self.ptr` is a valid pointer to `SSL_CLIENT_HELLO` provided by BoringSSL.
            sanitize_slice((*self.ptr).extensions, (*self.ptr).extensions_len).unwrap_or(&[])
        }
    }

    /// Extract a specific extension from the client hello.
    ///
    /// The returned slice contains the raw extension data, excluding the
    /// extension type and length headers. The format of these bytes depends
    /// on the specific extension type as defined in the relevant RFCs.
    pub fn get_extension(&self, extension_type: ExtensionType) -> Option<&'a [u8]> {
        let mut out_data = null();
        let mut out_len = 0;
        let ret = unsafe {
            // Safety: `self.ptr` is a valid pointer to `SSL_CLIENT_HELLO` provided by BoringSSL.
            bssl_sys::SSL_early_callback_ctx_extension_get(
                self.ptr,
                extension_type.0,
                &mut out_data,
                &mut out_len,
            )
        };
        if ret == 1 {
            unsafe {
                // Safety: `out_data` and `out_len` are valid if the function returns 1.
                sanitize_slice(out_data, out_len)
            }
        } else {
            None
        }
    }
}

/// Early callback handler.
///
/// This callback happens before TLS server state machine makes progress when [`ClientHello`] is
/// received.
/// A good use case is when a TLS server could make further configuration in response to the client
/// requests.
pub trait EarlyCallback<M>: Send + Sync {
    /// Decide whether a certificate can be selected.
    ///
    /// The connection handle is accessible through [`ClientHello::connection_mut`].
    fn process(&self, client_hello: &mut ClientHello<'_, M>) -> EarlyCallbackResult;
}

pub(crate) unsafe extern "C" fn early_select_cert_cb<Mode, MethodsT>(
    client_hello: *const bssl_sys::SSL_CLIENT_HELLO,
) -> bssl_sys::ssl_select_cert_result_t
where
    MethodsT: EarlyCallbackMethods<Mode>,
{
    let Some(client_hello_ptr) = NonNull::new(client_hello as *mut bssl_sys::SSL_CLIENT_HELLO)
    else {
        return bssl_sys::ssl_select_cert_result_t_ssl_select_cert_error;
    };
    let ssl = unsafe {
        // Safety: By BoringSSL invariant the `SSL_CLIENT_HELLO` handle must be valid
        // and only contains object handles that are managed by this crate or their derivatives.
        (*client_hello_ptr.as_ptr()).ssl
    };
    let Some(ssl_ptr) = NonNull::new(ssl) else {
        return bssl_sys::ssl_select_cert_result_t_ssl_select_cert_error;
    };

    let Some(methods) = (unsafe {
        // Safety: the connection handle must be originated from this crate
        MethodsT::from_ssl(ssl_ptr.as_ptr())
    }) else {
        return bssl_sys::ssl_select_cert_result_t_ssl_select_cert_error;
    };

    let Some(handler) = methods.early_callback_handler() else {
        return bssl_sys::ssl_select_cert_result_t_ssl_select_cert_success;
    };

    let mut wrapped_hello = ClientHello {
        ptr: client_hello_ptr.as_ptr(),
        conn: ssl_ptr,
        _p: PhantomData,
    };

    let res = abort_on_panic(move || handler.process(&mut wrapped_hello));
    res as bssl_sys::ssl_select_cert_result_t
}
