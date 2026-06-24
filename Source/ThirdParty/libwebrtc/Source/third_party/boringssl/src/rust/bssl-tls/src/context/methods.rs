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
    Methods,
    VerifyCertificateMethods,
    context::{
        DtlsMode,
        QuicMode,
        TlsMode, //
    },
    credentials::VerifyCertificate,
    methods::drop_box_rust_methods, //
};

pub(crate) struct RustContextMethods<M> {
    pub(crate) verify_certificate_methods: Option<Box<dyn VerifyCertificate>>,
    _p: PhantomData<fn() -> M>,
}

// NOTE(@xfding): the reason we do not use `register_ex_data` for this type is because we need to
// look up the associated SSL_CTX first.
impl<M> RustContextMethods<M> {
    pub fn new() -> Self {
        Self {
            verify_certificate_methods: None,
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

impl<M: HasTlsContextMethod> VerifyCertificateMethods for RustContextMethods<M> {
    fn verify_certificate_methods(&self) -> Option<&dyn VerifyCertificate> {
        self.verify_certificate_methods.as_deref()
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

impl HasTlsContextMethod for TlsMode {
    #[inline(always)]
    fn registration() -> c_int {
        static TLS_CONTEXT_METHOD: Lazy<c_int> = Lazy::new(register_tls_context_vtable::<TlsMode>);
        *TLS_CONTEXT_METHOD
    }
}

impl HasTlsContextMethod for DtlsMode {
    #[inline(always)]
    fn registration() -> c_int {
        static TLS_CONTEXT_METHOD: Lazy<c_int> = Lazy::new(register_tls_context_vtable::<DtlsMode>);
        *TLS_CONTEXT_METHOD
    }
}

impl HasTlsContextMethod for QuicMode {
    #[inline(always)]
    fn registration() -> c_int {
        static TLS_CONTEXT_METHOD: Lazy<c_int> = Lazy::new(register_tls_context_vtable::<QuicMode>);
        *TLS_CONTEXT_METHOD
    }
}
