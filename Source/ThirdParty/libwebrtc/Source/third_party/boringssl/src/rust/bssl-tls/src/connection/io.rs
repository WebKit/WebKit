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
        lifecycle::ShutdownStatus,
        methods::HasTlsConnectionMethod, //
    },
    context::{
        HasBasicIo,
        TlsMode, //
    },
    errors::{
        Error,
        IoError,
        TlsRetryReason, //
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

    fn take_io_err(&mut self) -> Option<Box<dyn core::error::Error + Send + Sync>> {
        let bio = self.get_connection_methods().bio.as_mut()?;
        bio.as_mut().take_io_err()
    }

    /// Translate I/O error into the right form.
    ///
    /// It is here we translate retry reason into a **soft** error [`IoStatus::Retry`].
    fn translate_io_error(&mut self, rc: c_int) -> Result<IoStatus, Error> {
        // Pre-emptively extract error and clear the error queue.
        let ssl_err = self.categorise_error_for_io(rc);
        if let Some(err) = self.take_io_err() {
            Err(Error::Io(IoError::Transport(err)))
        } else {
            ssl_err
        }
    }

    /// Read data from the socket.
    ///
    /// This method reads up to `buffer.len()` bytes from `buffer`.
    pub fn sync_read(&mut self, buffer: &mut ReceiveBuffer<'_>) -> Result<IoStatus, Error> {
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

    /// Peek `buffer.len()` bytes of application data into the `buffer`.
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
    pub fn sync_write(&mut self, buffer: &[u8]) -> Result<IoStatus, Error> {
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

    /// Flush the data on the **transport**.
    ///
    /// On success, this method always reports the number of bytes moved as `0`.
    pub fn flush(&mut self) -> Result<IoStatus, Error> {
        let bio = unsafe {
            // Safety: the validity of the handle `self.ptr()` is witnessed by `self`.
            bssl_sys::SSL_get_wbio(self.ptr())
        };
        if bio.is_null() {
            return Ok(IoStatus::Empty);
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
        if bio_retry != 1 {
            return Ok(IoStatus::Retry(TlsRetryReason::WantWrite));
        }
        // Pre-emptively extract error and clear the error queue.
        if let Some(err) = self.take_io_err() {
            Err(Error::Io(IoError::Transport(err)))
        } else {
            Ok(IoStatus::Ok(0))
        }
    }
}

/// Async I/O
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
    M: HasTlsConnectionMethod + HasBasicIo,
{
    fn do_async_io(
        mut self: Pin<&mut Self>,
        cx: &mut Context<'_>,
        sync_op: impl FnOnce(&mut TlsConnection<R, M>) -> Result<IoStatus, Error>,
    ) -> Result<Option<IoStatus>, Error> {
        self.set_waker(cx.waker());

        let reason = match sync_op(&mut *self) {
            Ok(
                status @ (IoStatus::Ok(..)
                | IoStatus::EndOfStream
                | IoStatus::Empty
                | IoStatus::Err),
            ) => return Ok(Some(status)),
            Err(e) => return Err(e),
            Ok(IoStatus::Retry(reason)) => reason,
        };
        self.get_connection_methods().set_pending_reason(reason);
        Ok(None)
    }
    #[doc(hidden)]
    pub fn aread_inner(
        self: Pin<&mut Self>,
        buffer: &mut [u8],
        cx: &mut Context<'_>,
    ) -> Result<Option<IoStatus>, Error> {
        let mut buffer = ReceiveBuffer::new(buffer);
        self.do_async_io(cx, move |this| this.sync_read(&mut buffer))
    }

    #[doc(hidden)]
    pub fn awrite_inner(
        self: Pin<&mut Self>,
        buffer: &[u8],
        cx: &mut Context<'_>,
    ) -> Result<Option<IoStatus>, Error> {
        self.do_async_io(cx, move |this| this.sync_write(buffer))
    }

    #[doc(hidden)]
    pub fn aflush_inner(
        self: Pin<&mut Self>,
        cx: &mut Context<'_>,
    ) -> Result<Option<IoStatus>, Error> {
        self.do_async_io(cx, move |this| this.flush())
    }

    /// Asynchronously read application data from the TLS connection.
    ///
    /// This method will intercept [`IoStatus::Retry`] and suspend the future.
    /// The reason can be inspected by invoking [`Self::take_pending_reason`].
    pub fn async_read<'a>(
        mut self: Pin<&'a mut Self>,
        buffer: &'a mut [u8],
    ) -> impl 'a + Send + Future<Output = Result<IoStatus, Error>> {
        poll_fn(move |cx| match self.as_mut().aread_inner(buffer, cx) {
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
        poll_fn(move |cx| match self.as_mut().awrite_inner(buffer, cx) {
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
        poll_fn(move |cx| match self.as_mut().aflush_inner(cx) {
            Ok(Some(status)) => Poll::Ready(Ok(status)),
            Ok(None) => Poll::Pending,
            Err(e) => Poll::Ready(Err(e)),
        })
    }

    #[doc(hidden)]
    pub fn ashutdown_inner(
        mut self: Pin<&mut Self>,
        cx: &mut Context<'_>,
    ) -> Result<Option<ShutdownStatus>, Error> {
        self.set_waker(cx.waker());
        let Some(mut conn) = self.established() else {
            return Err(Error::Io(IoError::EndOfStream));
        };
        loop {
            match conn.sync_shutdown()? {
                Some(ShutdownStatus::CloseNotifyPosted) => {}
                status => return Ok(status),
            }
        }
    }

    /// Asynchronously shut down the connection.
    pub fn async_shutdown<'a>(
        mut self: Pin<&'a mut Self>,
    ) -> impl 'a + Send + Future<Output = Result<(), Error>> {
        poll_fn(move |cx| match self.as_mut().ashutdown_inner(cx) {
            Ok(Some(ShutdownStatus::CloseNotifyReceived)) => Poll::Ready(Ok(())),
            Ok(Some(ShutdownStatus::EndOfStream)) => {
                Poll::Ready(Err(Error::Io(IoError::EndOfStream)))
            }
            Ok(Some(ShutdownStatus::RemainingApplicationData)) => Poll::Ready(Err(
                Error::TlsReason(crate::errors::TlsErrorReason::ApplicationDataOnShutdown),
            )),
            Ok(Some(ShutdownStatus::CloseNotifyPosted)) => unreachable!(),
            Ok(None) => Poll::Pending,
            Err(e) => Poll::Ready(Err(e)),
        })
    }
}

#[cfg(feature = "std")]
mod stdio;
