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

#![deny(
    missing_docs,
    unsafe_op_in_unsafe_fn,
    clippy::missing_safety_doc,
    clippy::indexing_slicing,
    clippy::unwrap_used,
    clippy::panic,
    clippy::expect_used,
    clippy::undocumented_unsafe_blocks
)]

//! `bssl-tls` / `tokio` integration
//!
//! This crate provides integration helpers with `tokio` and furthermore `hyper`.
//!
//! # Example
//!
//! ```rust
//! use bssl_tls::context::TlsContextBuilder;
//! use bssl_tls_tokio::{TokioIo, TokioTlsConnection};
//! use tokio::net::TcpStream;
//! # use tokio::io::{AsyncReadExt, AsyncWriteExt};
//! # use tokio::net::TcpListener;
//! # use bssl_tls::credentials::{Certificate, TlsCredentialBuilder};
//! # use bssl_x509::{certificates::X509Certificate, keys::PrivateKey, params::Trust, store::X509StoreBuilder};
//! #
//! # #[tokio::main]
//! # async fn main() -> Result<(), Box<dyn std::error::Error>> {
//! #     const CA: &[u8] = include_bytes!("../../test-data/BoringSSLCATest.crt");
//! #     const RSA_SERVER_CERT: &[u8] = include_bytes!("../../test-data/BoringSSLServerTest-RSA.crt");
//! #     const RSA_SERVER_KEY: &[u8] = include_bytes!("../../test-data/BoringSSLServerTest-RSA.key");
//! #
//! #     // Server setup
//! #     let ca = Certificate::parse_one_from_pem(CA, None)?;
//! #     let server_cert = Certificate::parse_one_from_pem(RSA_SERVER_CERT, None)?;
//! #     let server_key = PrivateKey::from_pem(RSA_SERVER_KEY, || unreachable!())?;
//! #
//! #     let mut server_ctx_builder = TlsContextBuilder::new_tls();
//! #     let server_cred = {
//! #         let mut builder = TlsCredentialBuilder::new();
//! #         builder
//! #             .with_certificate_chain(&[server_cert, ca.clone()])?
//! #             .with_private_key(server_key)?;
//! #         builder.build()
//! #     };
//! #     server_ctx_builder.with_credential(server_cred.unwrap())?;
//! #     let server_ctx = server_ctx_builder.build();
//! #
//! #     // Client setup
//! #     let mut client_ctx_builder = TlsContextBuilder::new_tls();
//! #     let mut cert_store = X509StoreBuilder::new();
//! #     cert_store
//! #         .set_trust(Trust::SslServer)?
//! #         .add_cert(X509Certificate::parse_one_from_pem(CA)?)?;
//! #     let cert_store = cert_store.build();
//! #     client_ctx_builder.with_certificate_store(&cert_store);
//! #
//! #     // Connection
//! #     let listener = TcpListener::bind("127.0.0.1:0").await?;
//! #     let addr = listener.local_addr()?;
//! #
//! #     let server_task = tokio::spawn(async move {
//! #         let (stream, _) = listener.accept().await.unwrap();
//! #         let mut conn = server_ctx.new_server_connection().build();
//! #         conn.set_io(TokioIo(stream)).unwrap();
//! #         conn.async_handshake().await.unwrap();
//! #         
//! #         let mut tls_stream = TokioTlsConnection::new(conn);
//! #         let mut buf = [0u8; 1024];
//! #         let _ = tls_stream.read(&mut buf).await.unwrap();
//! #         tls_stream.write_all(b"hello").await.unwrap();
//! #         tls_stream.shutdown().await.unwrap();
//! #     });
//! #
//!     let stream = TcpStream::connect(addr).await?;
//!
//!     let client_ctx = client_ctx_builder.build();
//!
//!     let mut conn = client_ctx.new_client_connection().build();
//!     conn.in_handshake().unwrap().set_host("www.google.com")?;
//!     conn.set_io(TokioIo(stream))?;
//!
//!     let mut tls_stream = TokioTlsConnection::new(conn);
//!
//!     tls_stream.write_all(b"GET / HTTP/1.1\r\nHost: example.com\r\n\r\n").await?;
//!     let mut buf = [0u8; 5];
//!     tls_stream.read_exact(&mut buf).await?;
//!     assert_eq!(&buf, b"hello");
//! #     tls_stream.shutdown().await?;
//! #     server_task.await?;
//! #     Ok(())
//! # }
//! ```

#[cfg(unix)]
use std::os::fd::{
    AsRawFd,
    RawFd, //
};
use std::{
    io,
    marker::PhantomData,
    ops::{
        Deref,
        DerefMut, //
    },
    pin::Pin,
    task::{
        Context,
        Poll,
        ready, //
    },
};
#[cfg(unix)]
use tokio::io::unix::AsyncFd;
use tokio::io::{
    AsyncRead,
    AsyncWrite,
    Interest,
    ReadBuf,
    Ready, //
};

#[cfg(unix)]
use bssl_tls::io::unix::{
    StdDatagram,
    UseFd, //
};
use bssl_tls::{
    connection::{
        Client,
        Server,
        TlsConnection,
        lifecycle::ShutdownStatus, //
    },
    context::{
        TlsContext,
        TlsContextBuilder,
        TlsMode, //
    },
    errors::Error,
    io::{
        AbstractReader, AbstractSocket, AbstractSocketResult, AbstractWriter, IoStatus,
        NoAsyncContext, stdio::PollFor,
    }, //
};

#[cfg(test)]
mod tests;

/// Translates a `std::io::Error` into an `AbstractSocketResult`.
fn translate_stdio_err(err: io::Error) -> AbstractSocketResult {
    match err.kind() {
        io::ErrorKind::WouldBlock => AbstractSocketResult::Retry,
        io::ErrorKind::ConnectionReset
        | io::ErrorKind::ConnectionRefused
        | io::ErrorKind::ConnectionAborted
        | io::ErrorKind::BrokenPipe
        | io::ErrorKind::NotConnected
        | io::ErrorKind::UnexpectedEof => AbstractSocketResult::EndOfStream,
        _ => AbstractSocketResult::Err(Box::new(err)),
    }
}

/// IO object implementing [`tokio::io::AsyncRead`] or [`tokio::io::AsyncWrite`] protocols.
pub struct TokioIo<T>(pub T);

fn tokio_async_read<T: AsyncRead>(
    mut this: Pin<&mut T>,
    ctx: &mut Context<'_>,
    buffer: &mut [u8],
) -> AbstractSocketResult {
    let mut buf = ReadBuf::new(buffer);
    loop {
        return match this.as_mut().poll_read(ctx, &mut buf) {
            Poll::Ready(Ok(())) => {
                if buf.filled().is_empty() && buf.remaining() > 0 {
                    AbstractSocketResult::EndOfStream
                } else {
                    AbstractSocketResult::Ok(buf.filled().len())
                }
            }
            Poll::Pending => AbstractSocketResult::Retry,
            Poll::Ready(Err(e)) => {
                if e.kind() == io::ErrorKind::Interrupted {
                    continue;
                }
                translate_stdio_err(e)
            }
        };
    }
}

fn tokio_async_write<T: AsyncWrite>(
    mut this: Pin<&mut T>,
    ctx: &mut Context<'_>,
    buffer: &[u8],
) -> AbstractSocketResult {
    loop {
        return match this.as_mut().poll_write(ctx, buffer) {
            Poll::Ready(Ok(bytes)) => {
                if buffer.is_empty() {
                    AbstractSocketResult::Ok(0)
                } else if bytes == 0 {
                    AbstractSocketResult::EndOfStream
                } else {
                    AbstractSocketResult::Ok(bytes)
                }
            }
            Poll::Pending => AbstractSocketResult::Retry,
            Poll::Ready(Err(e)) => {
                if e.kind() == io::ErrorKind::Interrupted {
                    continue;
                }
                translate_stdio_err(e)
            }
        };
    }
}

fn tokio_async_flush<T: AsyncWrite>(
    mut this: Pin<&mut T>,
    ctx: &mut Context<'_>,
) -> AbstractSocketResult {
    loop {
        return match this.as_mut().poll_flush(ctx) {
            Poll::Ready(Ok(())) => AbstractSocketResult::Ok(0),
            Poll::Pending => AbstractSocketResult::Retry,
            Poll::Ready(Err(e)) => {
                if e.kind() == io::ErrorKind::Interrupted {
                    continue;
                }
                translate_stdio_err(e)
            }
        };
    }
}

impl<T: AsyncRead + Send + Unpin> AbstractReader for TokioIo<T> {
    fn read(
        &mut self,
        async_ctx: Option<&mut Context<'_>>,
        buffer: &mut [u8],
    ) -> AbstractSocketResult {
        let Some(ctx) = async_ctx else {
            return AbstractSocketResult::Err(Box::new(NoAsyncContext));
        };
        tokio_async_read(Pin::new(&mut self.0), ctx, buffer)
    }
}

impl<T: AsyncWrite + Send + Unpin> AbstractWriter for TokioIo<T> {
    fn write(
        &mut self,
        async_ctx: Option<&mut Context<'_>>,
        buffer: &[u8],
    ) -> AbstractSocketResult {
        let Some(ctx) = async_ctx else {
            return AbstractSocketResult::Err(Box::new(NoAsyncContext));
        };
        tokio_async_write(Pin::new(&mut self.0), ctx, buffer)
    }

    fn flush(&mut self, async_ctx: Option<&mut Context<'_>>) -> AbstractSocketResult {
        let Some(ctx) = async_ctx else {
            return AbstractSocketResult::Err(Box::new(NoAsyncContext));
        };
        tokio_async_flush(Pin::new(&mut self.0), ctx)
    }
}

impl<T: AsyncRead + AsyncWrite + Send + Unpin> AbstractSocket for TokioIo<T> {}

#[cfg(unix)]
/// Reactor that operate over file descriptor.
pub struct TokioOverFd(AsyncFd<RawFd>);

#[cfg(unix)]
/// Construct a datagram IO object driven by [`tokio`].
pub fn new_std_datagram_with_tokio<T: AsRawFd>(
    inner: T,
) -> Result<StdDatagram<UseFd<T>, TokioOverFd>, io::Error> {
    let reactor = TokioOverFd::new(inner.as_raw_fd())?;
    let fd = UseFd(inner);
    Ok(StdDatagram::new(fd, reactor))
}

#[cfg(unix)]
impl TokioOverFd {
    /// A trivial constructor to signal use of `tokio` reactor and register events with
    /// file descriptors.
    pub fn new(fd: RawFd) -> Result<Self, io::Error> {
        Ok(Self(AsyncFd::try_with_interest(
            fd,
            Interest::READABLE | Interest::WRITABLE | Interest::ERROR,
        )?))
    }
}

#[cfg(unix)]
impl<T> PollFor<T> for TokioOverFd {
    fn poll_read(&mut self, async_ctx: &mut Context<'_>) -> Poll<Result<(), io::Error>> {
        match ready!(self.0.poll_read_ready_mut(async_ctx)) {
            Ok(mut guard) => {
                guard.clear_ready_matching(Ready::READABLE);
                Poll::Ready(Ok(()))
            }
            Err(e) => Poll::Ready(Err(e)),
        }
    }

    fn poll_write(&mut self, async_ctx: &mut Context<'_>) -> Poll<Result<(), io::Error>> {
        match ready!(self.0.poll_write_ready_mut(async_ctx)) {
            Ok(mut guard) => {
                guard.clear_ready_matching(Ready::WRITABLE);
                Poll::Ready(Ok(()))
            }
            Err(e) => Poll::Ready(Err(e)),
        }
    }
}

/// Wrapper for datagram sockets to satisfy orphan rule.
pub struct TokioDatagramIo<T>(pub T);

macro_rules! gen_impl_datagram {
    ($ty:ty) => {
        impl AbstractReader for TokioDatagramIo<$ty> {
            fn read(
                &mut self,
                async_ctx: Option<&mut Context<'_>>,
                buffer: &mut [u8],
            ) -> AbstractSocketResult {
                let Some(cx) = async_ctx else {
                    return AbstractSocketResult::Err(Box::new(NoAsyncContext));
                };
                let mut buf = ReadBuf::new(buffer);
                match self.0.poll_recv(cx, &mut buf) {
                    Poll::Pending => AbstractSocketResult::Retry,
                    Poll::Ready(Ok(_)) => AbstractSocketResult::Ok(buf.filled().len()),
                    Poll::Ready(Err(e)) => translate_stdio_err(e),
                }
            }
        }

        impl AbstractWriter for TokioDatagramIo<$ty> {
            fn write(
                &mut self,
                async_ctx: Option<&mut Context<'_>>,
                buf: &[u8],
            ) -> AbstractSocketResult {
                let Some(cx) = async_ctx else {
                    return AbstractSocketResult::Err(Box::new(NoAsyncContext));
                };
                match self.0.poll_send(cx, buf) {
                    Poll::Pending => AbstractSocketResult::Retry,
                    Poll::Ready(Ok(len)) => AbstractSocketResult::Ok(len),
                    Poll::Ready(Err(e)) => translate_stdio_err(e),
                }
            }

            fn flush(&mut self, _: Option<&mut Context<'_>>) -> AbstractSocketResult {
                AbstractSocketResult::Ok(0)
            }
        }

        impl AbstractSocket for TokioDatagramIo<$ty> {}
    };
}

gen_impl_datagram!(tokio::net::UdpSocket);
#[cfg(unix)]
gen_impl_datagram!(tokio::net::UnixDatagram);

/// A wrapper around [`TlsConnection`] that implements Tokio's async I/O traits.
pub struct TokioTlsConnection<Role> {
    inner: TlsConnection<Role, TlsMode>,
}

impl<Role> TokioTlsConnection<Role> {
    /// Construct a new wrapper.
    pub fn new(inner: TlsConnection<Role, TlsMode>) -> Self {
        Self { inner }
    }

    /// Consume the wrapper and return the inner connection.
    pub fn into_inner(self) -> TlsConnection<Role, TlsMode> {
        self.inner
    }
}

impl<Role> Deref for TokioTlsConnection<Role> {
    type Target = TlsConnection<Role, TlsMode>;

    fn deref(&self) -> &Self::Target {
        &self.inner
    }
}

impl<Role> DerefMut for TokioTlsConnection<Role> {
    fn deref_mut(&mut self) -> &mut Self::Target {
        &mut self.inner
    }
}

impl<R> AsyncRead for TokioTlsConnection<R> {
    fn poll_read(
        mut self: Pin<&mut Self>,
        cx: &mut Context<'_>,
        buf: &mut ReadBuf<'_>,
    ) -> Poll<io::Result<()>> {
        // Note: This assumes `aread_inner` is made public in `bssl-tls`.
        let status = match self
            .inner
            .as_pin_mut()
            .aread_inner(buf.initialize_unfilled(), cx)
        {
            Ok(Some(status)) => status,
            Ok(None) => return Poll::Pending,
            Err(e) => return Poll::Ready(Err(io::Error::new(io::ErrorKind::Other, e))),
        };
        match status {
            IoStatus::Ok(bytes) => {
                buf.advance(bytes);
                Poll::Ready(Ok(()))
            }
            IoStatus::EndOfStream => Poll::Ready(Ok(())),
            _ => Poll::Ready(Err(io::Error::new(
                io::ErrorKind::Other,
                "Unexpected I/O status",
            ))),
        }
    }
}

impl<R> AsyncWrite for TokioTlsConnection<R> {
    fn poll_write(
        mut self: Pin<&mut Self>,
        cx: &mut Context<'_>,
        buf: &[u8],
    ) -> Poll<io::Result<usize>> {
        // Note: This assumes `awrite_inner` is made public in `bssl-tls`.
        let status = match self.inner.as_pin_mut().awrite_inner(buf, cx) {
            Ok(Some(status)) => status,
            Ok(None) => return Poll::Pending,
            Err(e) => return Poll::Ready(Err(io::Error::new(io::ErrorKind::Other, e))),
        };
        match status {
            IoStatus::Ok(bytes) => Poll::Ready(Ok(bytes)),
            IoStatus::EndOfStream => Poll::Ready(Ok(0)),
            _ => Poll::Ready(Err(io::Error::new(
                io::ErrorKind::Other,
                "Unexpected I/O status",
            ))),
        }
    }

    fn poll_flush(mut self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<io::Result<()>> {
        // Note: This assumes `aflush_inner` is made public in `bssl-tls`.
        let status = match self.inner.as_pin_mut().aflush_inner(cx) {
            Ok(Some(status)) => status,
            Ok(None) => return Poll::Pending,
            Err(e) => return Poll::Ready(Err(io::Error::new(io::ErrorKind::Other, e))),
        };
        match status {
            IoStatus::Ok(_) => Poll::Ready(Ok(())),
            IoStatus::EndOfStream => Poll::Ready(Ok(())),
            _ => Poll::Ready(Err(io::Error::new(
                io::ErrorKind::Other,
                "Unexpected I/O status",
            ))),
        }
    }

    fn poll_shutdown(mut self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<io::Result<()>> {
        // Note: This assumes `ashutdown_inner` is made public in `bssl-tls`.
        match self.inner.as_pin_mut().ashutdown_inner(cx) {
            Ok(Some(ShutdownStatus::CloseNotifyReceived)) => Poll::Ready(Ok(())),
            Ok(Some(ShutdownStatus::RemainingApplicationData)) => Poll::Ready(Err(io::Error::new(
                io::ErrorKind::Other,
                "caller needs to drain application data before polling on shutdown again",
            ))),
            Ok(Some(ShutdownStatus::EndOfStream)) => Poll::Ready(Err(io::Error::new(
                io::ErrorKind::UnexpectedEof,
                "unexpected eof while waiting for peek close_notify",
            ))),
            Ok(Some(ShutdownStatus::CloseNotifyPosted)) => {
                unreachable!()
            }
            Ok(None) => Poll::Pending,
            Err(e) => Poll::Ready(Err(io::Error::new(io::ErrorKind::Other, e))),
        }
    }
}

/// A wrapper around `TlsContext` for creating Tokio-based client connections.
pub struct TlsConnector {
    ctx: TlsContext,
}

impl TlsConnector {
    /// Construct a new `TlsConnector`.
    pub(crate) fn new(ctx: TlsContext) -> Self {
        Self { ctx }
    }

    /// Connect to the given domain using the provided stream.
    pub async fn connect<S>(&self, domain: &str, stream: S) -> Result<TlsStream<Client, S>, Error>
    where
        S: AsyncRead + AsyncWrite + Send + Unpin + 'static,
    {
        let mut conn = self.ctx.new_client_connection().build();
        conn.in_handshake().unwrap().set_host(domain)?;

        conn.set_io(TokioIo(stream))?;

        conn.async_handshake().await?;

        Ok(TlsStream {
            conn: TokioTlsConnection::new(conn),
            _marker: PhantomData,
        })
    }
}

/// A wrapper around `TlsContext` for creating Tokio-based server connections.
pub struct TlsAcceptor {
    ctx: TlsContext,
}

impl TlsAcceptor {
    /// Construct a new `TlsAcceptor`.
    pub(crate) fn new(ctx: TlsContext) -> Self {
        Self { ctx }
    }

    /// Accept a new connection using the provided stream.
    pub async fn accept<S>(&self, stream: S) -> Result<TlsStream<Server, S>, Error>
    where
        S: AsyncRead + AsyncWrite + Send + Unpin + 'static,
    {
        let mut conn = self.ctx.new_server_connection().build();

        conn.set_io(TokioIo(stream))?;

        conn.async_handshake().await?;

        Ok(TlsStream {
            conn: TokioTlsConnection::new(conn),
            _marker: PhantomData,
        })
    }
}

/// A TLS stream driven by Tokio I/O.
pub struct TlsStream<Role, Stream> {
    conn: TokioTlsConnection<Role>,
    _marker: PhantomData<Stream>,
}

impl<Role, S> TlsStream<Role, S> {
    /// Get a reference to the underlying `TlsConnection`.
    pub fn get_ref(&self) -> &TlsConnection<Role> {
        &self.conn
    }

    /// Get a mutable reference to the underlying `TlsConnection`.
    pub fn get_mut(&mut self) -> &mut TlsConnection<Role> {
        &mut self.conn
    }
}

impl<Role, S: Unpin> AsyncRead for TlsStream<Role, S> {
    fn poll_read(
        mut self: Pin<&mut Self>,
        cx: &mut Context<'_>,
        buf: &mut ReadBuf<'_>,
    ) -> Poll<io::Result<()>> {
        Pin::new(&mut self.conn).poll_read(cx, buf)
    }
}

impl<Role, S: Unpin> AsyncWrite for TlsStream<Role, S> {
    fn poll_write(
        mut self: Pin<&mut Self>,
        cx: &mut Context<'_>,
        buf: &[u8],
    ) -> Poll<io::Result<usize>> {
        Pin::new(&mut self.conn).poll_write(cx, buf)
    }

    fn poll_flush(mut self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<io::Result<()>> {
        Pin::new(&mut self.conn).poll_flush(cx)
    }

    fn poll_shutdown(mut self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<io::Result<()>> {
        Pin::new(&mut self.conn).poll_shutdown(cx)
    }
}

/// Extension trait for `TlsContextBuilder` to support Tokio TLS.
pub trait TokioTlsExt {
    /// Build a `TlsConnector`.
    fn build_tokio_connector(self) -> TlsConnector;
    /// Build a `TlsAcceptor`.
    fn build_tokio_acceptor(self) -> TlsAcceptor;
}

impl TokioTlsExt for TlsContextBuilder<TlsMode> {
    fn build_tokio_connector(self) -> TlsConnector {
        TlsConnector::new(self.build())
    }

    fn build_tokio_acceptor(self) -> TlsAcceptor {
        TlsAcceptor::new(self.build())
    }
}
