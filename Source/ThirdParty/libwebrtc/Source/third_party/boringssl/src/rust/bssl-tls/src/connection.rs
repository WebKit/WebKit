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

//! TLS Connection

use alloc::boxed::Box;
use core::{
    ffi::c_int,
    marker::PhantomData,
    mem::forget,
    ptr::NonNull,
    task::Waker, //
};

use crate::{
    config::{
        ConnectionMode,
        ProtocolVersion, //
    },
    connection::methods::waker_data_ref_from_ssl,
    context::TlsMode,
    errors::{
        Error,
        TlsRetryReason, //
    },
    io::IoStatus,
    sessions::TlsSession, //
};

mod credentials;
pub mod io;
pub mod lifecycle;
pub(crate) mod methods;
pub mod transport;

/// Server role - the connection runs as a server.
pub enum Server {}
/// Client role - the connection runs as a client.
pub enum Client {}

/// A TLS connection builder.
pub struct TlsConnectionBuilder<Role, Mode = TlsMode> {
    ptr: NonNull<bssl_sys::SSL>,
    _p: PhantomData<fn() -> (Role, Mode)>,
}

unsafe impl<R, M> Send for TlsConnectionBuilder<R, M> {}

impl<R, M> Drop for TlsConnectionBuilder<R, M> {
    fn drop(&mut self) {
        unsafe {
            // Safety: the validity of `self.ptr()` is witnessed by `self`.
            bssl_sys::SSL_free(self.ptr());
        }
    }
}

impl<R, M> TlsConnectionBuilder<R, M> {
    fn ptr(&mut self) -> *mut bssl_sys::SSL {
        self.ptr.as_ptr()
    }
}

impl<R, M> TlsConnectionBuilder<R, M>
where
    M: methods::HasTlsConnectionMethod,
{
    /// Finalise the connection, so that a handshake can be run.
    pub fn build(self) -> TlsConnection<R, M> {
        let ptr = self.ptr;
        forget(self);
        TlsConnection {
            ptr,
            _p: PhantomData,
        }
    }

    pub(crate) fn from_ssl(ptr: NonNull<bssl_sys::SSL>) -> Self {
        let idx = M::registration();
        let data = Box::into_raw(Box::new(methods::RustConnectionMethods::<M>::new())) as _;
        unsafe {
            // Safety:
            // - the validity of the handle `ptr` is witnessed by `self`.
            // - `M::registration` will return a valid ex-data index.
            // - `data` should be valid by non-null invariant.
            bssl_sys::SSL_set_ex_data(ptr.as_ptr(), idx, data);
            // Safety: the validity of the handle `ptr` is witnessed by `self`.
            bssl_sys::SSL_set_mode(
                ptr.as_ptr(),
                ConnectionMode::ACCEPT_MOVING_WRITE_BUFFER.bits(),
            );
        }
        Self {
            ptr,
            _p: PhantomData,
        }
    }

    fn get_connection_methods(&mut self) -> &mut methods::RustConnectionMethods<M> {
        unsafe {
            // Safety: the validity of the handle `self.0` is witnessed by
            // `self`.
            get_connection_methods(self.ptr())
        }
    }

    /// Disable session creation.
    pub fn disable_session(&mut self) -> &mut Self {
        let ptr = self.ptr();
        unsafe {
            // Safety: the validity of the handle `ptr` is witnessed by `self`.
            bssl_sys::SSL_set_mode(ptr, ConnectionMode::MODE_NO_SESSION_CREATION.bits());
        }
        self
    }

    /// Set the session for resumption.
    pub fn with_session(&mut self, session: &TlsSession) -> &mut Self {
        unsafe {
            // Safety: self.ptr and session.0 are valid.
            bssl_sys::SSL_set_session(self.ptr.as_ptr(), session.0.as_ptr());
        }
        self
    }
}

/// TLS Connection
///
/// `Role` is expected to be either [`Server`] or [`Client`] and
/// `Mode` is expected to be either [`TlsMode`] or [`QuicMode`].
/// These generics will govern the capabilities that respective TLS connection role can access,
// NOTE: any method that involves I/O must require exclusive access, enforced by requiring `&mut`.
#[repr(transparent)]
pub struct TlsConnection<Role, Mode = TlsMode> {
    ptr: NonNull<bssl_sys::SSL>,
    _p: PhantomData<fn() -> (Role, Mode)>,
}

/// Safety: all internal states of `bssl_sys::SSL` that this connection are either exclusively owned
/// or reference-counted but immutable, including `SSL_SESSION`s and `SSL_CTX`s.
unsafe impl<R, M> Send for TlsConnection<R, M> {}

impl<R, M> Drop for TlsConnection<R, M> {
    fn drop(&mut self) {
        unsafe {
            // Safety: the connection handle is still valid.
            bssl_sys::SSL_free(self.ptr());
        }
    }
}

impl<R, M> TlsConnection<R, M> {
    /// Call this method whenever I/O is performed on the connection.
    pub(crate) fn categorise_error_for_io(&self, rc: c_int) -> Result<IoStatus, Error> {
        let reason = unsafe {
            // Safety: we only want to extract the last I/O error on an existing valid connection.
            bssl_sys::SSL_get_error(self.ptr(), rc)
        };
        let res = match TlsRetryReason::try_from(reason) {
            Ok(TlsRetryReason::PeerCloseNotify) => Ok(IoStatus::EndOfStream),
            Ok(reason) => Ok(IoStatus::Retry(reason)),
            Err(_) => Err(Error::extract_lib_err()),
        };
        unsafe {
            // Safety: we only clear the error on the current thread.
            bssl_sys::ERR_clear_error();
        }
        res
    }

    /// Get a handle of the connection object.
    ///
    /// # Safety
    /// - `self` must outlive all uses of the returned handle;
    /// - this handle must be used with functions from the BoringSSL library this crate is linked
    ///   to; otherwise, it is **undefined behaviour**.
    pub unsafe fn as_mut_ptr(&self) -> *mut bssl_sys::SSL {
        self.ptr.as_ptr()
    }

    /// Reconstitute a borrow of the connection object.
    ///
    /// # Safety
    /// - the handle `ptr` must be sourced originally from [`TlsConnection::as_mut_ptr`]
    ///   of this crate.
    /// - the handle `*ptr` must not be aliased and accessed across threads until the borrow is
    ///   expired; otherwise, it is **undefined behaviour**.
    pub unsafe fn from_mut_ptr(ptr: &mut *mut bssl_sys::SSL) -> &mut Self {
        unsafe {
            // Safety: `TlsConnection` is a thin wrapper around `*mut bssl_sys::SSL`
            core::mem::transmute(ptr)
        }
    }
}

// TODO(@xfding): there seems to be some type inference regression, drop the turbofish when it is
// resolved.
impl<R, M> TlsConnection<R, M>
where
    M: methods::HasTlsConnectionMethod,
{
    fn get_connection_methods(&mut self) -> &mut methods::RustConnectionMethods<M> {
        unsafe {
            // Safety: the validity of the handle `self.0` is witnessed by `self`.
            get_connection_methods::<M>(self.ptr())
        }
    }
    fn get_connection_methods_ref(&self) -> &methods::RustConnectionMethods<M> {
        unsafe {
            // Safety: the validity of the handle `self.0` is witnessed by `self`.
            get_connection_methods_ref::<M>(self.ptr())
        }
    }
}

impl<R, M> TlsConnection<R, M>
where
    M: methods::HasTlsConnectionMethod,
{
    /// This method sets up waker for BIO and various handlers.
    pub(crate) fn set_waker(&mut self, waker: &Waker) {
        let waker_data = unsafe {
            // Safety:
            // - `self.ptr()` is a valid `SSL` handle created from `from_ssl` in this crate.
            // - The data was previously initialized as `Box<Option<Waker>>`.
            waker_data_ref_from_ssl(self.ptr)
        };
        if let Some(other_waker) = waker_data
            && waker.will_wake(other_waker)
        {
            // We do not have to put this waker in the mailbox.
        } else {
            *waker_data = Some(waker.clone());
        }
        self.get_connection_methods().set_waker(waker);
    }
}

/// Safety:
/// - caller must pass in the SSL handle constructed by this crate.
/// - the handle must outlives `'a` for exclusive access.
unsafe fn get_connection_methods<'a, M: methods::HasTlsConnectionMethod>(
    ssl: *mut bssl_sys::SSL,
) -> &'a mut methods::RustConnectionMethods<M> {
    let methods = unsafe {
        // Safety: this instance of SSL should be constructed by this crate.
        bssl_sys::SSL_get_ex_data(ssl, M::registration())
    };
    if methods.is_null() {
        panic!("context method is missing")
    }
    unsafe {
        // Safety: `methods` must be constructed by `new_inner`
        &mut *(methods as *mut _)
    }
}

/// Safety:
/// - caller must pass in the SSL handle constructed by this crate.
/// - the handle must outlives `'a` for shared access.
unsafe fn get_connection_methods_ref<'a, M: methods::HasTlsConnectionMethod>(
    ssl: *mut bssl_sys::SSL,
) -> &'a methods::RustConnectionMethods<M> {
    let methods = unsafe {
        // Safety: this instance of SSL should be constructed by this crate.
        bssl_sys::SSL_get_ex_data(ssl, M::registration())
    };
    if methods.is_null() {
        panic!("connection method is missing")
    }
    unsafe {
        // Safety: `methods` must be constructed by `new_inner`
        &*(methods as *const _)
    }
}

impl<R, M> TlsConnection<R, M> {
    pub(crate) fn ptr(&self) -> *mut bssl_sys::SSL {
        self.ptr.as_ptr()
    }

    /// Check if the connection is DTLS.
    pub fn is_dtls(&self) -> bool {
        unsafe {
            // Safety: the SSL handle is still valid witnessed by self
            bssl_sys::SSL_is_dtls(self.ptr()) == 1
        }
    }

    /// Check if the connection is server.
    ///
    /// This method returns `false` if it is a client connection.
    pub fn is_server(&self) -> bool {
        unsafe {
            // Safety: the SSL handle is still valid witnessed by self
            bssl_sys::SSL_is_server(self.ptr()) == 1
        }
    }

    /// Check if a handshake is underway on this connection.
    pub fn is_in_handshake(&self) -> bool {
        unsafe {
            // Safety: the SSL handle is still valid witnessed by self
            bssl_sys::SSL_in_init(self.ptr()) == 1
        }
    }

    /// Get the protocol version of this TLS connection.
    ///
    /// This method call returns `None` if the connection has not completed handshake.
    pub fn get_protocol_version(&self) -> Option<ProtocolVersion> {
        if self.is_in_handshake() {
            return None;
        }
        let ret = unsafe {
            // Safety:
            // - the handle is still valid witnessed by self;
            // - the connection has completed handshake.
            bssl_sys::SSL_version(self.ptr())
        };
        ret.try_into().ok()
    }
}
