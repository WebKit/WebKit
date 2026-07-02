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

use alloc::boxed::Box;
use core::{
    ffi::{
        c_int,
        c_long,
        c_void, //
    },
    marker::PhantomData,
    ptr::{
        NonNull,
        null_mut, //
    },
    task::Waker, //
};

use once_cell::sync::Lazy;

use crate::{
    Methods,
    VerifyCertificateMethods,
    abort_on_panic,
    context::{
        DtlsMode,
        QuicMode,
        TlsMode, //
    },
    credentials::{
        VerifyCertificate,
        VerifyCertificateTask, //
    },
    errors::TlsRetryReason,
    io::RustBioHandle,
    methods::drop_box_rust_methods, //
};

/// The associated state to the [`super::TlsConnection`].
pub(super) struct RustConnectionMethods<Mode> {
    /// A handle to a `BIO` managed by this crate.
    pub bio: Option<RustBioHandle>,
    /// Certificate verifier handle.
    pub verify_certificate_methods: Option<Box<dyn VerifyCertificate>>,
    /// A mailbox to propagate IO retrying reasons.
    pub pending_reason: Option<TlsRetryReason>,
    _p: PhantomData<fn() -> Mode>,
}

impl<M> RustConnectionMethods<M> {
    pub fn new() -> Self {
        Self {
            bio: None,
            verify_certificate_methods: None,
            pending_reason: None,
            _p: PhantomData,
        }
    }

    pub fn set_pending_reason(&mut self, reason: TlsRetryReason) {
        self.pending_reason = Some(reason);
    }

    pub fn take_pending_reason(&mut self) -> Option<TlsRetryReason> {
        self.pending_reason.take()
    }

    /// Propagate waker to the handlers.
    pub fn set_waker(&mut self, waker: &Waker) {
        if let Some(bio) = &mut self.bio {
            bio.set_waker(waker);
        }
    }
}

impl<Mode: HasTlsConnectionMethod> Methods for RustConnectionMethods<Mode> {
    unsafe extern "C" fn from_ssl<'a>(ssl: *mut bssl_sys::SSL) -> Option<&'a Self> {
        unsafe {
            // Safety: `ssl` is originated from `TlsConnection::from_ssl`.
            let methods = bssl_sys::SSL_get_ex_data(ssl, Mode::registration());
            debug_assert!(
                !methods.is_null(),
                "connection method should have been attached at construction time"
            );
            // Safety: `ctx` is originated from `Box::into_raw`
            Some(&mut *(methods as *mut RustConnectionMethods<Mode>))
        }
    }
}

impl<Mode: HasTlsConnectionMethod> VerifyCertificateMethods for RustConnectionMethods<Mode> {
    fn verify_certificate_methods(&self) -> Option<&dyn VerifyCertificate> {
        self.verify_certificate_methods.as_deref()
    }
}

// NOTE(@xfding): the reason that we are not using the `register_ex_data` macro is because
// declarative macros today cannot handle generics well enough.
fn register_tls_connection_vtable<Mode: HasTlsConnectionMethod>() -> c_int {
    let ret = unsafe {
        // Safety: this a one-time registration uses only valid function pointers.
        bssl_sys::SSL_get_ex_new_index(
            0,
            null_mut(),
            null_mut(),
            None,
            Some(drop_box_rust_methods::<RustConnectionMethods<Mode>>),
        )
    };
    if ret < 0 {
        panic!("Failed to register TLS Connection ex-data")
    } else {
        ret
    }
}

/// Safety:
/// - `ssl` must be a `SSL` object constructed by [`crate::connection::TlsConnection`].
/// - `ssl` must be exclusively owned.
pub(crate) unsafe fn waker_data_from_ssl(ssl: NonNull<bssl_sys::SSL>) -> Option<Waker> {
    unsafe {
        // Safety: `ssl` outlives `'a` and is constructed by `TlsConnection`.
        <ExDataRegistration as ExData<Option<Waker>>>::get_mut(ssl).clone()
    }
}

/// Safety:
/// - `ssl` must be a `SSL` object constructed by [`crate::connection::TlsConnection`] and
///   outlives `'a`.
/// - `ssl` must be exclusively owned.
pub(super) unsafe fn waker_data_ref_from_ssl<'a>(
    ssl: NonNull<bssl_sys::SSL>,
) -> &'a mut Option<Waker> {
    unsafe {
        // Safety: `ssl` outlives `'a` and is constructed by `TlsConnection`.
        <ExDataRegistration as ExData<Option<Waker>>>::get_mut(ssl)
    }
}

/// Safety:
/// - `ssl` must be constructed from `TlsConnection` and outlived by `'a`.
/// - `ssl` must be exclusively owned.
pub(crate) unsafe fn verify_cert_task_from_ssl<'a>(
    ssl: NonNull<bssl_sys::SSL>,
) -> &'a mut Option<Box<dyn VerifyCertificateTask>> {
    unsafe {
        // Safety: `ssl` outlives `'a` and is constructed by `TlsConnection`.
        <ExDataRegistration as ExData<Option<Box<dyn VerifyCertificateTask>>>>::get_mut(ssl)
    }
}

pub(crate) struct ExDataRegistration;

pub(crate) trait ExData<T: Default> {
    /// Initialise the ex-data slot for type `T`
    ///
    /// Safety:
    /// - Caller must ensure exclusive access to `ssl` handle.
    /// - Caller must ensure that `ssl` is constructed by [`super::TlsConnection`].
    unsafe fn init(ssl: NonNull<bssl_sys::SSL>);
    /// Take a mutable reference to the ex_data.
    ///
    /// Safety:
    /// - Caller must ensure exclusive access to `ssl` handle.
    /// - Caller must ensure that `ssl` is constructed by [`super::TlsConnection`].
    /// - Caller must ensure that `ssl` outlives `'a`.
    /// - Caller must ensure that the underlying ex-data shall never be aliased.
    unsafe fn get_mut<'a>(ssl: NonNull<bssl_sys::SSL>) -> &'a mut T;
}

// NOTE(@xfding): the reason we have this macro is because Rust `static`s do not support generics.
macro_rules! register_ex_data {
    ($T:ty) => {
        const _: () = {
            fn _assert()
            where
                $T: Sized,
            {
            }
            unsafe extern "C" fn destructor(
                _parent: *mut c_void,
                ptr: *mut c_void,
                _ad: *mut bssl_sys::CRYPTO_EX_DATA,
                _index: c_int,
                _argl: c_long,
                _argp: *mut c_void,
            ) {
                abort_on_panic(|| unsafe {
                    if ptr.is_null() {
                        return;
                    }
                    // Safety: this ex_data must be registered by `TlsConnection`, so the type must have
                    // been `T`.
                    let _ = Box::from_raw(ptr as *mut $T);
                });
            }
            fn register() -> c_int {
                let ret = unsafe {
                    // Safety: this a one-time registration uses only valid function pointers.
                    bssl_sys::SSL_get_ex_new_index(
                        0,
                        null_mut(),
                        null_mut(),
                        None,
                        Some(destructor),
                    )
                };
                if ret < 0 {
                    panic!("Failed to register TLS Connection waker ex-data")
                } else {
                    ret
                }
            }

            static REGISTER: Lazy<c_int> = Lazy::new(register);
            impl ExData<$T> for ExDataRegistration {
                unsafe fn init(ptr: NonNull<bssl_sys::SSL>) {
                    unsafe {
                        bssl_sys::SSL_set_ex_data(
                            ptr.as_ptr(),
                            *REGISTER,
                            Box::into_raw(Box::new(<$T>::default())) as _,
                        );
                    }
                }
                unsafe fn get_mut<'a>(ssl: NonNull<bssl_sys::SSL>) -> &'a mut $T {
                    if let Some(mut data) = unsafe {
                        // Safety: `ssl` outlives `'a`.
                        NonNull::new(bssl_sys::SSL_get_ex_data(ssl.as_ptr(), *REGISTER) as *mut $T)
                    } {
                        unsafe {
                            // Safety: `ssl` is still alive and constructed by `TlsConnection`.
                            return data.as_mut();
                        }
                    }
                    unsafe {
                        // Safety: `ssl` is constructed by `TlsConnection`.
                        <Self as ExData<$T>>::init(ssl);
                    }
                    let mut data = unsafe {
                        // Safety: `ssl` outlives `'a`.
                        NonNull::new(bssl_sys::SSL_get_ex_data(ssl.as_ptr(), *REGISTER) as *mut $T)
                            .unwrap()
                    };
                    unsafe {
                        // Safety: `ssl` is still alive and constructed by `TlsConnection`.
                        data.as_mut()
                    }
                }
            }
        };
    };
}

register_ex_data!(Option<Waker>);
register_ex_data!(Option<Box<dyn VerifyCertificateTask>>);

pub(crate) trait HasTlsConnectionMethod {
    fn registration() -> c_int;
}

impl HasTlsConnectionMethod for TlsMode {
    #[inline(always)]
    fn registration() -> c_int {
        static TLS_CONTEXT_METHOD: Lazy<c_int> =
            Lazy::new(register_tls_connection_vtable::<TlsMode>);
        *TLS_CONTEXT_METHOD
    }
}

impl HasTlsConnectionMethod for DtlsMode {
    #[inline(always)]
    fn registration() -> c_int {
        static TLS_CONTEXT_METHOD: Lazy<c_int> =
            Lazy::new(register_tls_connection_vtable::<DtlsMode>);
        *TLS_CONTEXT_METHOD
    }
}

impl HasTlsConnectionMethod for QuicMode {
    #[inline(always)]
    fn registration() -> c_int {
        static TLS_CONTEXT_METHOD: Lazy<c_int> =
            Lazy::new(register_tls_connection_vtable::<QuicMode>);
        *TLS_CONTEXT_METHOD
    }
}
