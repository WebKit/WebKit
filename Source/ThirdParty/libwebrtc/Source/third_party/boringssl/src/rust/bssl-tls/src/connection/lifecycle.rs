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

use alloc::{
    boxed::Box,
    string::ToString, //
};
use core::{
    future::poll_fn,
    ops::{
        Deref,
        DerefMut, //
    },
    task::Poll, //
};

use crate::{
    alerts::AlertDescription,
    check_tls_error,
    connection::{
        Client,
        Server,
        TlsConnection,
        methods::HasTlsConnectionMethod, //
    },
    context::{
        HasBasicIo,
        SupportedMode,
        TlsMode, //
    },
    errors::{
        Error,
        TlsErrorReason,
        TlsRetryReason, //
    },
    io::IoStatus, //
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
        let session = unsafe {
            // Safety: the validity of the handle `self.0` is witnessed by `self`.
            bssl_sys::SSL_get_session(self.ptr())
        };
        if session.is_null() {
            return None;
        }
        Some(EstablishedTlsConnection(self))
    }
}

bssl_macros::bssl_enum! {
    /// TLS data-pending reasons
    pub enum TlsPendingData: i32 {
        /// TLS connection wants to read more data.
        WantRead = bssl_sys::SSL_READING as i32,
        /// TLS connection wants to write more data.
        WantWrite = bssl_sys::SSL_WRITING as i32,
    }
}

/// # Connection state
///
/// When operations on [`TlsConnection`] return with pending status,
/// there will be reasons why the operations should be retried.
impl<R, M> TlsConnection<R, M> {
    /// Check the connection if it needs additional data.
    pub fn wants_data(&self) -> Option<TlsPendingData> {
        let code = unsafe {
            // Safety: the validity of the handle is witnessed by `self`.
            bssl_sys::SSL_want(self.ptr())
        };
        let code = i32::try_from(code).ok()?;
        TlsPendingData::try_from(code).ok()
    }
}

/// # Alerts
impl<R, M> TlsConnection<R, M>
where
    M: HasTlsConnectionMethod,
{
    /// Send fatal alert.
    ///
    /// This would usually lead to termination of the connection.
    pub fn send_fatal_alert(
        &mut self,
        alert: AlertDescription,
    ) -> Result<Option<TlsRetryReason>, Error> {
        Ok(check_tls_error!(self.ptr(), {
            // Safety: `self.0` is still a valid handle and `alert` is valid by construction.
            bssl_sys::SSL_send_fatal_alert(self.ptr(), alert as u8)
        }))
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
#[repr(transparent)]
pub struct TlsConnectionInHandshake<'a, R, M>(pub(crate) &'a mut TlsConnection<R, M>);

impl<R, M> Deref for TlsConnectionInHandshake<'_, R, M> {
    type Target = TlsConnection<R, M>;
    fn deref(&self) -> &Self::Target {
        &*self.0
    }
}

impl<R, M> DerefMut for TlsConnectionInHandshake<'_, R, M> {
    fn deref_mut(&mut self) -> &mut Self::Target {
        &mut *self.0
    }
}

/// # Handshake
impl<R, M> TlsConnectionInHandshake<'_, R, M>
where
    M: HasTlsConnectionMethod,
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
        let conn = self.ptr();
        Ok(check_tls_error!(conn, bssl_sys::SSL_do_handshake(conn)))
    }
}

impl<M> TlsConnectionInHandshake<'_, Server, M>
where
    M: HasTlsConnectionMethod,
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

impl<M> TlsConnectionInHandshake<'_, Client, M>
where
    M: HasTlsConnectionMethod,
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

impl<'a, R, M> EstablishedTlsConnection<'a, R, M> {
    /// Get the current session.
    pub fn get_session(&self) -> Option<crate::sessions::TlsSession> {
        let session = unsafe {
            // Safety: self.ptr() is valid.
            bssl_sys::SSL_get1_session(self.ptr())
        };
        core::ptr::NonNull::new(session).map(crate::sessions::TlsSession)
    }
}

impl<R, M> EstablishedTlsConnection<'_, R, M>
where
    M: HasTlsConnectionMethod + HasBasicIo,
{
    /// Perform synchronising shutdown.
    ///
    /// If the method returns `Ok(None)`, the shutdown will not progress until I/O makes progress.
    ///
    /// # Shutdown protocol
    /// A live connection can be actively shut down by calling this method at most two times.
    /// The first call will send `close_notify` down the transport.
    /// On `Ok` the first call is considered successful with the following return value.
    /// - [`ShutdownStatus::CloseNotifyReceived`] signifies that a `close_notify` is received from the peer, too.
    /// - [`ShutdownStatus::CloseNotifyPosted`] signifies that a `close_notify` from our end is sent but that from the peer
    ///   has not arrived.
    ///
    /// In case of no reception of peer `close_notify`, it is necessary to call this method again.
    /// There are two possible outcomes.
    /// - [`ShutdownStatus::RemainingApplicationData`] signifies that there are pending application data.
    ///   Process it until the stream ends.
    /// - [`ShutdownStatus::CloseNotifyReceived`] signifies that a `close_notify` is received from the peer, too.
    ///   The connection is then in terminal state.
    /// To process the remaining application data, normal reading should continue until the end of
    /// stream, at which [`Self::sync_shutdown`] can be called again to set the connection to the terminal state.
    pub fn sync_shutdown(&mut self) -> Result<Option<ShutdownStatus>, Error> {
        let rc = unsafe {
            // Safety: we have exclusive access to the connection state.
            bssl_sys::SSL_shutdown(self.ptr())
        };
        if self.is_write_closed() {
            return Ok(Some(ShutdownStatus::EndOfStream));
        }
        match rc {
            0 => Ok(Some(ShutdownStatus::CloseNotifyPosted)),
            1 => Ok(Some(ShutdownStatus::CloseNotifyReceived)),
            _ => match self.categorise_error_for_io(rc) {
                Ok(IoStatus::Ok(_)) => unreachable!(),
                Ok(IoStatus::Empty | IoStatus::EndOfStream) => {
                    Ok(Some(ShutdownStatus::EndOfStream))
                }
                Ok(IoStatus::Retry(TlsRetryReason::WantRead | TlsRetryReason::WantWrite)) => {
                    Ok(None)
                }
                Ok(IoStatus::Retry(reason)) => panic!("unexpected retry reason {reason:?}"),
                Err(Error::TlsReason(TlsErrorReason::ApplicationDataOnShutdown)) => {
                    Ok(Some(ShutdownStatus::RemainingApplicationData))
                }
                Err(Error::Library(0, _, _)) => Ok(Some(ShutdownStatus::CloseNotifyReceived)),
                Ok(IoStatus::Err) => Err(Error::Unknown(Box::new("transport error".to_string()))),
                Err(e) => Err(e),
            },
        }
    }
}

impl<R, M> TlsConnectionInHandshake<'_, R, M>
where
    M: SupportedMode,
{
    /// Perform asynchronous handshake, until completion or until pending on non-I/O operations.
    ///
    /// The caller needs to ensure that any pending operations during the handshake are resolved,
    /// before polling [`async_handshake`] again.
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

/// Shutdown progress
pub enum ShutdownStatus {
    /// `close_notify` has been sent.
    CloseNotifyPosted,
    /// Peer `close_notify` has been received. The connection is now in terminal state.
    CloseNotifyReceived,
    /// There are remaining application data. Consume them first before calling `shutdown` again.
    RemainingApplicationData,
    /// The read half of the connection reaches the end of the stream.
    EndOfStream,
}
