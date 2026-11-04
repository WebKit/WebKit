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

//! TLS Connection lifecycle controls

use alloc::boxed::Box;
use core::{
    ffi::c_int,
    future::poll_fn,
    ops::{
        Deref,
        DerefMut, //
    },
    ptr::NonNull,
    task::Poll, //
};

use crate::{
    HandshakeCompleteMethods,
    Methods,
    abort_on_panic,
    alerts::AlertDescription,
    connection::{
        Client,
        Server,
        TlsConnection,
        TlsConnectionBuilder,
        methods::HasTlsConnectionMethod, //
    },
    context::{
        HasShutdown,
        SupportedMode,
        TlsMode, //
    },
    credentials::TlsCredential,
    errors::{
        Error,
        TlsErrorReason,
        TlsRetryReason, //
    }, //
};

/// # Connection shutdown
impl<R, M> TlsConnection<R, M> {
    /// Set whether shutting down this connection sends out a `close_notify` alert.
    pub fn set_quiet_shutdown(&mut self, quiet: bool) -> &mut Self {
        unsafe {
            // Safety: the validity of the handle `self.0` is witnessed by `self`.
            bssl_sys::SSL_set_quiet_shutdown(self.ptr(), if quiet { 1 } else { 0 });
        }
        self
    }

    /// Check whether shutting down this connection sends out a `close_notify` alert.
    pub fn get_quiet_shutdown(&self) -> bool {
        let rc = unsafe {
            // Safety: the validity of the handle `self.0` is witnessed by `self`.
            bssl_sys::SSL_get_quiet_shutdown(self.ptr())
        };
        rc == 1
    }
}

/// # Connection initialisation state
///
/// There are methods and accessors that become available only when the connection is in the right
/// state.
///
/// Please refer to [`EstablishedTlsConnection`] and [`TlsConnectionInHandshake`] for allowed
/// operations.
impl<R, M> TlsConnection<R, M> {
    /// Access handshake-related options if the connection is in handshake mode.
    pub fn in_handshake<'a>(&'a mut self) -> Option<TlsConnectionInHandshake<'a, R, M>> {
        if self.is_in_handshake() {
            Some(TlsConnectionInHandshake(self))
        } else {
            None
        }
    }

    /// Access handshake-related options if a handshake is completed and
    /// the connection is initialised.
    pub fn established<'a>(&'a mut self) -> Option<EstablishedTlsConnection<'a, R, M>> {
        (!self.is_in_handshake()).then_some(EstablishedTlsConnection(self))
    }
}

/// # Alerts
impl<R, M> TlsConnection<R, M>
where
    M: HasTlsConnectionMethod,
{
    fn translate_lifecycle_result(&mut self, rc: c_int) -> Result<Option<TlsRetryReason>, Error> {
        let code = unsafe {
            // Safety: inspecting the last error on an existing valid connection.
            bssl_sys::SSL_get_error(self.ptr(), rc)
        };
        match code {
            // Handshake or alert transmission completed cleanly; here it returns `Ok(None)` to indicate
            // that no further progress or retry is required.
            bssl_sys::SSL_ERROR_NONE => Ok(None),

            // TODO(crbug.com/42290000): This should be handled within the library.
            // A `close_notify` received mid-handshake or during fatal alert sending means the peer closed
            // the connection before completing the handshake, which is a terminal error.
            bssl_sys::SSL_ERROR_ZERO_RETURN => {
                Err(Error::TlsReason(TlsErrorReason::Sslv3AlertCloseNotify))
            }

            // The handshake paused due to pending network I/O or an asynchronous callback such as private
            // key operations.
            // Returning `Ok(Some(reason))` preserves the exact suspension reason so the caller or async
            // reactor can resolve it before driving the handshake again.
            _ if let Ok(reason) = TlsRetryReason::try_from(code) => Ok(Some(reason)),
            _ => Err(self.extract_tls_error(code)),
        }
    }

    /// Send fatal alert.
    ///
    /// This would usually lead to termination of the connection.
    pub fn send_fatal_alert(
        &mut self,
        alert: AlertDescription,
    ) -> Result<Option<TlsRetryReason>, Error> {
        let rc = unsafe {
            // Safety: `self.0` is still a valid handle and `alert` is valid by construction.
            bssl_sys::SSL_send_fatal_alert(self.ptr(), alert as u8)
        };
        self.translate_lifecycle_result(rc)
    }

    /// Send fatal alert asynchronously.
    pub fn async_send_fatal_alert<'a>(
        &'a mut self,
        alert: AlertDescription,
    ) -> impl 'a + Send + Future<Output = Result<(), Error>> {
        poll_fn(move |cx| {
            self.set_waker(cx.waker());
            match self.send_fatal_alert(alert) {
                Ok(Some(TlsRetryReason::WantRead | TlsRetryReason::WantWrite)) => Poll::Pending,
                Ok(None) => Poll::Ready(Ok(())),
                Ok(Some(reason)) => unreachable!("unexpected retry reason {reason:?}"),
                Err(e) => Poll::Ready(Err(e)),
            }
        })
    }
}

impl<R> TlsConnection<R, TlsMode> {
    /// Inspect if the connection is suspended for which reason, after invocation of I/O methods.
    pub fn take_pending_reason(&mut self) -> Option<TlsRetryReason> {
        self.get_connection_methods().take_pending_reason()
    }
}

/// A handle to the connection that is valid only during handshake.
// NOTE(@xfding): this type is strictly for configuration of the connection during the handshake,
// and no methods should be allowed to drive the TLS state machine.
#[repr(transparent)]
pub struct TlsConnectionInHandshake<'a, R, M>(pub(crate) &'a mut TlsConnection<R, M>);

impl<R, M> TlsConnectionInHandshake<'_, R, M> {
    pub(crate) fn ptr(&self) -> *mut bssl_sys::SSL {
        self.0.ptr()
    }
}

/// # Handshake
impl<R, M> TlsConnection<R, M>
where
    M: SupportedMode,
{
    /// Drive the handshake.
    ///
    /// Call this method after the initial [`Self::accept`] or [`Self::connect`],
    /// should the handshake be suspended.
    ///
    /// This method returns `Ok(None)` to signal handshake completion;
    /// otherwise, `Ok(Some(reason))` is returned and the suspension `reason` must be resolved first
    /// before this method can make progress again.
    pub fn do_handshake(&mut self) -> Result<Option<TlsRetryReason>, Error> {
        let rc = unsafe {
            // Safety: driving the handshake on a valid connection handle.
            bssl_sys::SSL_do_handshake(self.ptr())
        };
        self.translate_lifecycle_result(rc)
    }
}

impl<M> TlsConnection<Server, M>
where
    M: SupportedMode,
{
    /// Accept a connection by responding to `ClientHello` with `ServerHello`.
    ///
    /// This method returns `Ok(None)` to signal handshake completion;
    /// otherwise, given `Ok(Some(reason))` the suspension `reason` must be resolved first
    /// before calling [`Self::do_handshake`] can make progress again.
    pub fn accept(&mut self) -> Result<Option<TlsRetryReason>, Error> {
        self.do_handshake()
    }
}

impl<M> TlsConnection<Client, M>
where
    M: SupportedMode,
{
    /// Initiate a connection by sending a `ClientHello`.
    ///
    /// This method returns `Ok(None)` to signal handshake completion;
    /// otherwise, given `Ok(Some(reason))` the suspension `reason` must be resolved first
    /// before calling [`Self::do_handshake`] can make progress again.
    pub fn connect(&mut self) -> Result<Option<TlsRetryReason>, Error> {
        self.do_handshake()
    }
}

/// A handle to the connection that is valid only after initialization, or in other words after
/// handshake.
#[repr(transparent)]
pub struct EstablishedTlsConnection<'a, R, M = TlsMode>(&'a mut TlsConnection<R, M>);

impl<R, M> Deref for EstablishedTlsConnection<'_, R, M> {
    type Target = TlsConnection<R, M>;
    fn deref(&self) -> &Self::Target {
        &*self.0
    }
}

impl<R, M> DerefMut for EstablishedTlsConnection<'_, R, M> {
    fn deref_mut(&mut self) -> &mut Self::Target {
        &mut *self.0
    }
}

impl<R, M> EstablishedTlsConnection<'_, R, M>
where
    M: HasTlsConnectionMethod + HasShutdown,
{
    /// Perform shutdown on the write end.
    ///
    /// If the method returns `Ok(Some(reason))`, the shutdown will not progress until I/O makes
    /// progress.
    pub fn sync_shutdown(&mut self) -> Result<Option<TlsRetryReason>, Error> {
        // SSL_shutdown has two stages, sending close_notify and waiting for close_notify.
        // We now believe that only the sending call, aka the first call, is useful.
        // This method only sends close_notify, so it skips calling SSL_shutdown if close_notify has
        // already been sent.
        let rc = unsafe {
            // Safety: we have exclusive access to the connection state.
            bssl_sys::SSL_get_shutdown(self.ptr())
        };
        if rc & bssl_sys::SSL_SENT_SHUTDOWN != 0 {
            return Ok(None);
        }
        let rc = unsafe {
            // Safety: we have exclusive access to the connection state.
            bssl_sys::SSL_shutdown(self.ptr())
        };
        if matches!(rc, 0 | 1) {
            return Ok(None);
        }
        self.translate_lifecycle_result(rc)
    }
}

impl<R, M> TlsConnection<R, M>
where
    M: SupportedMode,
{
    /// Perform asynchronous handshake, until completion or until pending on non-I/O operations.
    ///
    /// The caller needs to ensure that any pending operations during the handshake are resolved,
    /// before polling [`Self::async_handshake`] again.
    pub fn async_handshake(
        &mut self,
    ) -> impl Send + Future<Output = Result<Option<TlsRetryReason>, Error>> + '_ {
        poll_fn(move |cx| {
            self.set_waker(cx.waker());
            match self.do_handshake() {
                Ok(Some(TlsRetryReason::WantRead | TlsRetryReason::WantWrite)) => Poll::Pending,
                Ok(Some(reason)) => Poll::Ready(Ok(Some(reason))),
                Ok(None) => Poll::Ready(Ok(None)),
                Err(e) => Poll::Ready(Err(e)),
            }
        })
    }

    /// Perform asynchronous handshake, until completion, knowing that all possible pending reasons
    /// will resolve themselves.
    ///
    /// The caller needs to ensure that any pending operations due to asynchronous operations such as
    /// certificate verification and private key operations will eventually resolve and wake up
    /// the handshake task.
    /// Otherwise the handshake task will never complete.
    pub fn async_nonstop_handshake(
        &mut self,
    ) -> impl Send + Future<Output = Result<(), Error>> + '_ {
        poll_fn(move |cx| {
            self.set_waker(cx.waker());
            match self.do_handshake() {
                Ok(Some(_)) => Poll::Pending,
                Ok(None) => Poll::Ready(Ok(())),
                Err(e) => Poll::Ready(Err(e)),
            }
        })
    }
}

bssl_crypto::bssl_enum! {
    enum InfoCallbackConnectionState : i32 {
        ReadAlert = bssl_sys::SSL_CB_READ_ALERT as i32,
        WriteAlert = bssl_sys::SSL_CB_WRITE_ALERT as i32,
        HandshakeStart = bssl_sys::SSL_CB_HANDSHAKE_START as i32,
        HandshakeDone = bssl_sys::SSL_CB_HANDSHAKE_DONE as i32,
    }
}

/// A callback that allows you to inspect the handshake information when the handshake concludes
/// successfully.
pub trait HandshakeComplete: Send {
    /// The callback to be called when handshake is complete with success.
    fn handshake_complete(&mut self, hs: &HandshakeInfo);
}

/// A handle to extract handshake information, such as credential.
pub struct HandshakeInfo(NonNull<bssl_sys::SSL>);

impl HandshakeInfo {
    /// Return the selected credential for the connection.
    pub fn get_selected_credential(&self) -> Option<TlsCredential> {
        let cred = unsafe {
            // Safety: by BoringSSL invariant, `self.0` is still a valid connection handle.
            bssl_sys::SSL_get0_selected_credential(self.0.as_ptr())
        };
        NonNull::new(cred as *mut _).map(TlsCredential::from_raw_and_upref)
    }
}

unsafe extern "C" fn info_callback<M: Methods + HandshakeCompleteMethods>(
    ssl: *const bssl_sys::SSL,
    ty: c_int,
    _: c_int,
) {
    // Safety: the const-to-mut cast is safe; we only mutate our own method table, not SSL.
    let Some(ssl) = NonNull::new(ssl as *mut _) else {
        return;
    };
    let Some(state) = InfoCallbackConnectionState::try_from(ty).ok() else {
        return;
    };
    abort_on_panic(move || {
        match state {
            InfoCallbackConnectionState::HandshakeDone => {
                let Some(mut callback) = (unsafe {
                    // Safety: `ssl` is a valid handle passed in from BoringSSL.
                    M::from_ssl(ssl.as_ptr())
                })
                .and_then(|methods| methods.handshake_complete_methods()) else {
                    return;
                };
                callback.handshake_complete(&HandshakeInfo(ssl));
            }
            InfoCallbackConnectionState::ReadAlert
            | InfoCallbackConnectionState::WriteAlert
            | InfoCallbackConnectionState::HandshakeStart => {}
        }
    });
}

/// # Handshake-complete callback
impl<R, M> TlsConnectionBuilder<R, M>
where
    M: HasTlsConnectionMethod,
{
    /// Set a callback to be called when the handshake completes successfully.
    ///
    /// The callback receives a [`HandshakeInfo`] handle that can be used to
    /// inspect the result of the handshake, such as the selected credential.
    pub fn with_handshake_complete_callback(
        &mut self,
        callback: impl HandshakeComplete + 'static,
    ) -> &mut Self {
        self.as_in_handshake()
            .set_handshake_complete_callback(callback);
        self
    }
}

/// # Handshake completion notification callback
impl<R, M> TlsConnectionInHandshake<'_, R, M>
where
    M: HasTlsConnectionMethod,
{
    /// Set a callback to be called when the handshake completes successfully.
    pub fn set_handshake_complete_callback(
        &mut self,
        callback: impl HandshakeComplete + 'static,
    ) -> &mut Self {
        unsafe {
            // Safety: we only install our own callback function.
            bssl_sys::SSL_set_info_callback(
                self.ptr(),
                Some(info_callback::<super::methods::RustConnectionMethods<M>>),
            );
        }
        self.0.get_connection_methods().handshake_complete = Some(Box::new(callback));
        self
    }
}
