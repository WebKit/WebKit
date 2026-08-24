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

use core::{
    ffi::c_int,
    marker::PhantomData,
    ptr::null_mut, //
};

use once_cell::sync::Lazy;

use crate::{
    EarlyCallbackMethods,
    Methods,
    PrivateKeyMethods,
    VerifyCertificateMethods,
    context::{
        DtlsExternalVerifierMode,
        DtlsMode,
        QuicMode,
        TlsExternalVerifierMode, //
        TlsMode,
    },
    credentials::{
        PrivateKeyDelegate,
        VerifyCertificate,
        early_callback::EarlyCallback,
        methods::{
            complete,
            decrypt,
            sign, //
        }, //
    },
    methods::drop_box_rust_methods, //
};

pub(crate) struct RustContextMethods<M> {
    pub(crate) private_key_methods: Option<Box<dyn PrivateKeyDelegate>>,
    pub(crate) verify_certificate_methods: Option<Box<dyn VerifyCertificate>>,
    pub(crate) early_callback_handler: Option<Box<dyn EarlyCallback<M>>>,
    _p: PhantomData<fn() -> M>,
}

// NOTE(@xfding): the reason we do not use `register_ex_data` for this type is because we need to
// look up the associated SSL_CTX first.
impl<M> RustContextMethods<M> {
    pub fn new() -> Self {
        Self {
            private_key_methods: None,
            verify_certificate_methods: None,
            early_callback_handler: None,
            _p: PhantomData,
        }
    }
}

impl<M: HasTlsContextMethod> Methods for RustContextMethods<M> {
    unsafe extern "C" fn from_ssl<'a>(ssl: *mut bssl_sys::SSL) -> Option<&'a Self> {
        unsafe {
            // Safety: `ssl` must be still valid by BoringSSL invariant.
            let ctx = bssl_sys::SSL_get_SSL_CTX(ssl);
            if ctx.is_null() {
                return None;
            }
            // Safety: `ctx` is originated from `TlsContext::new_inner`.
            let methods = bssl_sys::SSL_CTX_get_ex_data(ctx, M::registration());
            // Safety: `ctx` is originated from `Box::into_raw`
            Some(&mut *(methods as *mut RustContextMethods<_>))
        }
    }
}

impl<M: HasTlsContextMethod> PrivateKeyMethods for RustContextMethods<M> {
    fn private_key_methods(&self) -> Option<&dyn PrivateKeyDelegate> {
        self.private_key_methods.as_deref()
    }
}

impl<M: HasTlsContextMethod> VerifyCertificateMethods for RustContextMethods<M> {
    fn verify_certificate_methods(&self) -> Option<&dyn VerifyCertificate> {
        self.verify_certificate_methods.as_deref()
    }
}

impl<M: HasTlsContextMethod> EarlyCallbackMethods<M> for RustContextMethods<M> {
    fn early_callback_handler(&self) -> Option<&dyn EarlyCallback<M>> {
        self.early_callback_handler.as_deref()
    }
}

fn register_tls_context_vtable<M: HasTlsContextMethod>() -> c_int {
    unsafe {
        // Safety: this a one-time registration uses only valid function pointers.
        let ret = bssl_sys::SSL_CTX_get_ex_new_index(
            0,
            null_mut(),
            null_mut(),
            None,
            Some(drop_box_rust_methods::<RustContextMethods<M>>),
        );
        if ret < 0 {
            panic!("Failed to register TLS Context ex-data")
        } else {
            ret
        }
    }
}

pub(crate) trait HasTlsContextMethod {
    fn registration() -> c_int;
}

macro_rules! impl_has_tls_context_method {
    ($($mode:ty),+ $(,)?) => {
        $(
            impl HasTlsContextMethod for $mode {
                #[inline(always)]
                fn registration() -> c_int {
                    static TLS_CONTEXT_METHOD: Lazy<c_int> =
                        Lazy::new(register_tls_context_vtable::<$mode>);
                    *TLS_CONTEXT_METHOD
                }
            }
        )+
    };
}

impl_has_tls_context_method! {
    TlsMode,
    TlsExternalVerifierMode,
    DtlsMode,
    DtlsExternalVerifierMode,
    QuicMode,
}

pub(super) trait HasPrivateKeyMethods {
    const METHODS: *const bssl_sys::SSL_PRIVATE_KEY_METHOD;
}

macro_rules! impl_private_key_methods {
    ($wrapper:ident, $($mode:ty),+ $(,)?) => {
        $(
            impl HasPrivateKeyMethods for $mode {
                const METHODS: *const bssl_sys::SSL_PRIVATE_KEY_METHOD = {
                    &bssl_sys::SSL_PRIVATE_KEY_METHOD {
                        sign: Some(sign::<$wrapper<$mode>>),
                        decrypt: Some(decrypt::<$wrapper<$mode>>),
                        complete: Some(complete::<$wrapper<$mode>>),
                    } as _
                };
            }
        )+
    };
}

impl_private_key_methods! {
    RustContextMethods,
    TlsMode,
    TlsExternalVerifierMode,
    DtlsMode,
    DtlsExternalVerifierMode,
    QuicMode,
}
