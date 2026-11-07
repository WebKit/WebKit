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

//! TLS I/O model

use alloc::boxed::Box;
use core::{
    ffi::c_int,
    future::poll_fn,
    pin::Pin,
    task::{
        Context,
        Poll, //
    }, //
};

use crate::{
    ReceiveBuffer,
    connection::{
        TlsConnection,
        methods::HasTlsConnectionMethod, //
    },
    context::{
        HasDatagramIo,
        HasShutdown,
        HasStreamIo, //
    },
    errors::{
        Error,
        IoError,
        TlsRetryReason,
        UnknownError, //
    },
    ffi::slice_into_ffi_raw_parts,
    io::IoStatus, //
};

impl<R, M> TlsConnection<R, M>
where
    M: HasTlsConnectionMethod,
{
    /// Check if the connection has any buffered data pending reads.
    pub fn has_pending_read(&self) -> bool {
        unsafe {
            // Safety: the validity of the handle `self.ptr()` is witnessed by `self`.
            bssl_sys::SSL_has_pending(self.ptr()) == 1
        }
    }

    pub(crate) fn take_io_err(&mut self) -> Option<Box<dyn core::error::Error + Send + Sync>> {
        let bio = self.get_connection_methods().bio.as_mut()?;
        bio.as_mut().take_io_err()
    }

    /// Extracts a pending error from either the BoringSSL library error queue or the underlying BIO
    /// transport error.
    ///
    /// This method prioritises library errors from BoringSSL's error queue over transport errors
    /// captured by the underlying Rust BIO.
    pub(crate) fn extract_pending_error(&mut self) -> Option<Error> {
        debug_assert!(
            Error::extract_lib_err().is_none(),
            "impossible error condition, we only allow custom BIO",
        );
        self.take_io_err().map(|e| Error::Io(IoError::Transport(e)))
    }

    /// Extracts a TLS error from either BoringSSL's error queue or the underlying transport error
    /// based on the `SSL_get_error` code.
    #[inline]
    pub(crate) fn extract_tls_error(&mut self, code: c_int) -> Error {
        match code {
            bssl_sys::SSL_ERROR_SSL => Error::extract_lib_err()
                .unwrap_or_else(|| Error::Unknown(UnknownError("unknown tls error"))),
            bssl_sys::SSL_ERROR_SYSCALL => self
                .extract_pending_error()
                .unwrap_or(Error::Io(IoError::EndOfStream)),
            _ => self
                .extract_pending_error()
                .unwrap_or_else(|| Error::Unknown(UnknownError("unknown tls error"))),
        }
    }

    /// Translate I/O error during reads or writes into the normal form.
    ///
    /// It is here we translate the error condition into a **soft** error [`IoStatus::Retry`].
    fn translate_io_error(&mut self, rc: c_int) -> Result<IoStatus, Error> {
        let code = unsafe {
            // Safety: inspecting the last error on an existing valid connection.
            bssl_sys::SSL_get_error(self.ptr(), rc)
        };
        match code {
            // Callers are exactly `read_inner` and `write_inner` which invoke this method
            // when `rc <= 0`.
            // Positive return codes represent bytes read/written and return directly.
            bssl_sys::SSL_ERROR_NONE => {
                unreachable!("rc cannot be positive when calling this method")
            }

            // `SSL_ERROR_ZERO_RETURN` signals peer's clean `close_notify` alert.
            // For active data reads/writes, this is a clean, protocol-level end-of-stream.
            bssl_sys::SSL_ERROR_ZERO_RETURN => Ok(IoStatus::Ok(0)),

            // Transient I/O suspension is a soft condition, represented as `IoStatus::Retry`
            // so caller can register a waker and retry with identical buffers.
            _ if let Ok(reason) = TlsRetryReason::try_from(code) => Ok(IoStatus::Retry(reason)),
            _ => Err(self.extract_tls_error(code)),
        }
    }

    fn read_inner(&mut self, buffer: &mut ReceiveBuffer<'_>) -> Result<IoStatus, Error> {
        if buffer.remaining() == 0 {
            return Ok(IoStatus::Ok(0));
        }
        let buf = unsafe {
            // Safety:
            // - the use of this pointer is outlived by this function callframe.
            // - the access to the buffer region is bounded by `buffer.remaining()` by `SSL_read`
            //   contract.
            // - there are no reads into the buffer region per `SSL_read` contract.
            buffer.head()
        };
        let num = c_int::try_from(buffer.remaining()).unwrap_or(c_int::MAX);
        let rc = unsafe {
            // Safety: the validity of the handle `self.ptr()` is witnessed by `self`.
            bssl_sys::SSL_read(self.ptr(), buf as _, num)
        };
        if rc > 0 {
            let len = rc as usize;
            unsafe {
                // Safety: BoringSSL will ensure that `len` bytes have been written.
                buffer.advance(len);
            }
            Ok(IoStatus::Ok(len))
        } else {
            self.translate_io_error(rc)
        }
    }

    fn write_inner(&mut self, buffer: &[u8]) -> Result<IoStatus, Error> {
        if buffer.is_empty() {
            return Ok(IoStatus::Ok(0));
        }
        let (ptr, len) = slice_into_ffi_raw_parts(buffer);
        let num = c_int::try_from(len).unwrap_or(c_int::MAX);
        let rc = unsafe {
            // Safety: the validity of the handle `self.ptr()` is witnessed by `self`
            bssl_sys::SSL_write(self.ptr(), ptr as _, num)
        };
        if rc > 0 {
            Ok(IoStatus::Ok(rc as usize))
        } else {
            self.translate_io_error(rc)
        }
    }
}

impl<R, M> TlsConnection<R, M>
where
    M: HasTlsConnectionMethod,
{
    /// For `async` operations, obtain a pinned mutable reference.
    pub fn as_pin_mut(&mut self) -> Pin<&mut Self> {
        Pin::new(self)
    }

    /// For `async` operations, obtain a pinned immutable reference.
    pub fn as_pin(&self) -> Pin<&Self> {
        Pin::new(self)
    }
}

impl<R, M> TlsConnection<R, M>
where
    M: HasTlsConnectionMethod,
{
    fn do_async_io(
        mut self: Pin<&mut Self>,
        cx: &mut Context<'_>,
        sync_op: impl FnOnce(&mut TlsConnection<R, M>) -> Result<IoStatus, Error>,
    ) -> Result<Option<IoStatus>, Error> {
        self.set_waker(cx.waker());

        let reason = match sync_op(&mut *self) {
            Ok(status @ IoStatus::Ok(..)) => {
                return Ok(Some(status));
            }
            Err(e) => return Err(e),
            Ok(IoStatus::Retry(reason)) => reason,
        };
        self.get_connection_methods().set_pending_reason(reason);
        Ok(None)
    }
}

/// I/O for stream sockets
impl<R, M> TlsConnection<R, M>
where
    M: HasTlsConnectionMethod + HasStreamIo,
{
    /// Read data from the socket.
    ///
    /// This method reads up to `buffer.remaining()` bytes from `buffer`.
    pub fn poll_read(&mut self, buffer: &mut ReceiveBuffer<'_>) -> Result<IoStatus, Error> {
        self.read_inner(buffer)
    }

    /// Peek `buffer.remaining()` bytes of application data into the `buffer`.
    pub fn peek(&mut self, buffer: &mut ReceiveBuffer<'_>) -> Result<IoStatus, Error> {
        let buf = unsafe {
            // Safety:
            // - the use of this pointer is outlived by this function callframe.
            // - the access to the buffer region is bounded by `buffer.remaining()` by `SSL_peek`
            //   contract.
            // - there are no reads into the buffer region per `SSL_peek` contract.
            buffer.head()
        };
        let num = c_int::try_from(buffer.remaining()).unwrap_or(c_int::MAX);
        let rc = unsafe {
            // Safety: the validity of the handle `self.ptr()` is witnessed by `self`
            bssl_sys::SSL_peek(self.ptr(), buf as _, num)
        };
        if rc > 0 {
            let len = rc as usize;
            unsafe {
                // Safety: BoringSSL will ensure that `len` bytes have been written.
                buffer.advance(len);
            }
            Ok(IoStatus::Ok(len))
        } else {
            self.translate_io_error(rc)
        }
    }

    /// Write data to the socket.
    ///
    /// This method writes up to `buffer.len()` bytes from `buffer`.
    pub fn poll_write(&mut self, buffer: &[u8]) -> Result<IoStatus, Error> {
        self.write_inner(buffer)
    }

    /// Flush the data on the **transport**.
    ///
    /// On success, this method always reports the number of bytes moved as `0`.
    pub fn poll_flush(&mut self) -> Result<IoStatus, Error> {
        let bio = unsafe {
            // Safety: the validity of the handle `self.ptr()` is witnessed by `self`.
            bssl_sys::SSL_get_wbio(self.ptr())
        };
        if bio.is_null() {
            return Err(Error::Io(IoError::Unconfigured));
        }
        let rc = unsafe {
            // Safety: `bio` should still be valid by BoringSSL invariant.
            bssl_sys::BIO_flush(bio)
        };
        if rc == 1 {
            return Ok(IoStatus::Ok(0));
        }
        // We do not expect any SSL level error, but there could still be BIO level error.
        let bio_retry = unsafe {
            // Safety: `bio` should still be valid here.
            bssl_sys::BIO_should_retry(bio)
        };
        if bio_retry != 0 {
            return Ok(IoStatus::Retry(TlsRetryReason::WantWrite));
        }
        // Pre-emptively extract error and clear the error queue.
        if let Some(err) = self.take_io_err() {
            Err(Error::Io(IoError::Transport(err)))
        } else {
            Err(Error::Unknown(UnknownError("transport error")))
        }
    }

    /// Poll from the connection once for receiving application data.
    ///
    /// When the transport is not ready or the handshake has pending resolution,
    /// this function will register a waker and return [`None`].
    pub fn async_poll_read(
        self: Pin<&mut Self>,
        buffer: &mut ReceiveBuffer<'_>,
        cx: &mut Context<'_>,
    ) -> Result<Option<IoStatus>, Error> {
        self.do_async_io(cx, move |this| this.read_inner(buffer))
    }

    /// Poll from the connection once for writing application data.
    ///
    /// When the transport is not ready or the handshake has pending resolution,
    /// this function will register a waker and return [`None`].
    pub fn async_poll_write(
        self: Pin<&mut Self>,
        buffer: &[u8],
        cx: &mut Context<'_>,
    ) -> Result<Option<IoStatus>, Error> {
        self.do_async_io(cx, move |this| this.write_inner(buffer))
    }

    /// Poll from the connection once for flushing pending writes.
    ///
    /// When the transport is not ready or the handshake has pending resolution,
    /// this function will register a waker and return [`None`].
    pub fn async_poll_flush(
        self: Pin<&mut Self>,
        cx: &mut Context<'_>,
    ) -> Result<Option<IoStatus>, Error> {
        self.do_async_io(cx, move |this| this.poll_flush())
    }

    /// Asynchronously read application data from the TLS connection.
    ///
    /// This method will intercept [`IoStatus::Retry`] and suspend the future.
    /// The reason can be inspected by invoking [`Self::take_pending_reason`].
    pub fn async_read<'a>(
        mut self: Pin<&'a mut Self>,
        buffer: &'a mut ReceiveBuffer<'_>,
    ) -> impl 'a + Send + Future<Output = Result<IoStatus, Error>> {
        poll_fn(move |cx| match self.as_mut().async_poll_read(buffer, cx) {
            Ok(Some(status)) => Poll::Ready(Ok(status)),
            Ok(None) => Poll::Pending,
            Err(e) => Poll::Ready(Err(e)),
        })
    }

    /// Asynchronously write application data to the TLS connection.
    ///
    /// This method will intercept [`IoStatus::Retry`] and suspend the future.
    /// The reason can be inspected by invoking [`Self::take_pending_reason`].
    pub fn async_write<'a>(
        mut self: Pin<&'a mut Self>,
        buffer: &'a [u8],
    ) -> impl 'a + Send + Future<Output = Result<IoStatus, Error>> {
        poll_fn(move |cx| match self.as_mut().async_poll_write(buffer, cx) {
            Ok(Some(status)) => Poll::Ready(Ok(status)),
            Ok(None) => Poll::Pending,
            Err(e) => Poll::Ready(Err(e)),
        })
    }

    /// Asynchronously flush the underlying transport attached to the TLS connection.
    ///
    /// This method will intercept [`IoStatus::Retry`] and suspend the future.
    /// The reason can be inspected by invoking [`Self::take_pending_reason`].
    pub fn async_flush<'a>(
        mut self: Pin<&'a mut Self>,
    ) -> impl 'a + Send + Future<Output = Result<IoStatus, Error>> {
        poll_fn(move |cx| match self.as_mut().async_poll_flush(cx) {
            Ok(Some(status)) => Poll::Ready(Ok(status)),
            Ok(None) => Poll::Pending,
            Err(e) => Poll::Ready(Err(e)),
        })
    }
}

/// I/O for datagram sockets.
impl<R, M> TlsConnection<R, M>
where
    M: HasTlsConnectionMethod + HasDatagramIo,
{
    /// Receive an application datagram from the socket.
    ///
    /// This method reads up to `buffer.len()` bytes from `buffer`.
    /// Be sure to use [`Self::dtlsv1_get_timeout`] to arm a timer and
    /// notify the connection about deadline with [`Self::dtlsv1_handle_timeout`].
    pub fn sync_recv(&mut self, buffer: &mut ReceiveBuffer<'_>) -> Result<IoStatus, Error> {
        self.read_inner(buffer)
    }

    /// Send an application datagram down the socket.
    ///
    /// This method writes up to `buffer.len()` bytes from `buffer`.
    /// Be sure to use [`Self::dtlsv1_get_timeout`] to arm a timer and
    /// notify the connection about deadline with [`Self::dtlsv1_handle_timeout`].
    pub fn sync_send(&mut self, buffer: &[u8]) -> Result<IoStatus, Error> {
        self.write_inner(buffer)
    }

    /// Poll from the connection once for receiving application datagrams.
    ///
    /// When the transport is not ready or the handshake has pending resolution,
    /// this function will register a waker and return [`None`].
    ///
    /// Be sure to use [`Self::dtlsv1_get_timeout`] to arm a timer and
    /// notify the connection about deadline with [`Self::dtlsv1_handle_timeout`].
    pub fn async_poll_recv(
        self: Pin<&mut Self>,
        buffer: &mut ReceiveBuffer<'_>,
        cx: &mut Context<'_>,
    ) -> Result<Option<IoStatus>, Error> {
        self.do_async_io(cx, move |this| this.read_inner(buffer))
    }

    /// Poll from the connection once for sending application datagrams.
    ///
    /// When the transport is not ready or the handshake has pending resolution,
    /// this function will register a waker and return [`None`].
    ///
    /// Be sure to use [`Self::dtlsv1_get_timeout`] to arm a timer and
    /// notify the connection about deadline with [`Self::dtlsv1_handle_timeout`].
    pub fn async_poll_send(
        self: Pin<&mut Self>,
        buffer: &[u8],
        cx: &mut Context<'_>,
    ) -> Result<Option<IoStatus>, Error> {
        self.do_async_io(cx, move |this| this.write_inner(buffer))
    }

    /// Asynchronously receive an application datagram.
    ///
    /// This method will intercept [`IoStatus::Retry`] and suspend the future.
    /// The reason can be inspected by invoking [`Self::take_pending_reason`].
    ///
    /// Be sure to use [`Self::dtlsv1_get_timeout`] to arm a timer and
    /// notify the connection about deadline with [`Self::dtlsv1_handle_timeout`].
    pub fn async_recv<'a>(
        mut self: Pin<&'a mut Self>,
        buffer: &'a mut ReceiveBuffer<'_>,
    ) -> impl 'a + Send + Future<Output = Result<IoStatus, Error>> {
        poll_fn(move |cx| match self.as_mut().async_poll_recv(buffer, cx) {
            Ok(Some(status)) => Poll::Ready(Ok(status)),
            Ok(None) => Poll::Pending,
            Err(e) => Poll::Ready(Err(e)),
        })
    }

    /// Asynchronously send an application datagram.
    ///
    /// This method will intercept [`IoStatus::Retry`] and suspend the future.
    /// The reason can be inspected by invoking [`Self::take_pending_reason`].
    ///
    /// Be sure to use [`Self::dtlsv1_get_timeout`] to arm a timer and
    /// notify the connection about deadline with [`Self::dtlsv1_handle_timeout`].
    pub fn async_send<'a>(
        mut self: Pin<&'a mut Self>,
        buffer: &'a [u8],
    ) -> impl 'a + Send + Future<Output = Result<IoStatus, Error>> {
        poll_fn(move |cx| match self.as_mut().async_poll_send(buffer, cx) {
            Ok(Some(status)) => Poll::Ready(Ok(status)),
            Ok(None) => Poll::Pending,
            Err(e) => Poll::Ready(Err(e)),
        })
    }
}

impl<R, M> TlsConnection<R, M>
where
    M: HasTlsConnectionMethod + HasShutdown,
{
    /// Poll from the connection once for shutting down the connection.
    ///
    /// When the transport is not ready or the shutdown has pending resolution,
    /// this function will register a waker and return `false`.
    /// Otherwise `true` signifies a successful notification of closing write end.
    pub fn async_poll_shutdown(
        mut self: Pin<&mut Self>,
        cx: &mut Context<'_>,
    ) -> Result<bool, Error> {
        self.set_waker(cx.waker());
        let Some(mut conn) = self.established() else {
            return Err(Error::Io(IoError::EndOfStream));
        };
        match conn.sync_shutdown()? {
            None => Ok(true),
            Some(TlsRetryReason::WantRead | TlsRetryReason::WantWrite) => Ok(false),
            Some(reason) => panic!("unexpected retry reason {reason:?}"),
        }
    }

    /// Asynchronously shut down the connection.
    ///
    /// The returned future completes when the notification of a closing write end is sent.
    pub fn async_shutdown<'a>(
        mut self: Pin<&'a mut Self>,
    ) -> impl 'a + Send + Future<Output = Result<(), Error>> {
        poll_fn(move |cx| match self.as_mut().async_poll_shutdown(cx) {
            Ok(true) => Poll::Ready(Ok(())),
            Ok(false) => Poll::Pending,
            Err(e) => Poll::Ready(Err(e)),
        })
    }
}

#[cfg(feature = "std")]
mod stdio;
