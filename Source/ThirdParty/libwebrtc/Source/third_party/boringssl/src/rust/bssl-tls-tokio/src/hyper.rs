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

//! Hyper support

use crate::{
    TlsAcceptor,
    TlsConnector,
    TlsStream,
    TokioTlsConnection,
    translate_stdio_err, //
};

use std::{
    error::Error,
    fmt::Debug,
    future::Future,
    io,
    pin::Pin,
    sync::Arc,
    task::{
        Context,
        Poll, //
    },
};

use bssl_tls::{
    ReceiveBuffer,
    connection::{
        Client,
        Server, //
    },
    errors::Error as TlsError,
    io::{
        AbstractReader,
        AbstractSocket,
        AbstractSocketResult,
        AbstractWriter,
        IoStatus,
        NoAsyncContext, //
    },
};
use hyper::{
    http,
    rt::{
        Read,
        ReadBufCursor,
        Write, //
    }, //
};
use tower::Service;

/// A connector for `hyper` using `bssl-tls`.
#[derive(Clone)]
pub struct HyperBsslConnector<Inner> {
    inner: Inner,
    connector: Arc<TlsConnector>,
}

impl<Inner: Debug> std::fmt::Debug for HyperBsslConnector<Inner> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("HyperBsslConnector")
            .field("inner", &self.inner)
            .finish()
    }
}

impl<Inner> HyperBsslConnector<Inner> {
    /// Construct a new `HyperBsslConnector`.
    pub fn new(inner: Inner, connector: TlsConnector) -> Self {
        Self {
            inner,
            connector: Arc::new(connector),
        }
    }
}

impl<Inner> Service<http::Uri> for HyperBsslConnector<Inner>
where
    Inner: Service<http::Uri>,
    Inner::Response: Read + Write + Unpin + Send + Sync + 'static,
    Inner::Future: Send + 'static,
    Inner::Error: Into<Box<dyn Error + Send + Sync>>,
{
    type Response = TlsStream<Client, Inner::Response>;
    type Error = Box<dyn Error + Send + Sync>;
    type Future = Pin<Box<dyn Future<Output = Result<Self::Response, Self::Error>> + Send>>;

    fn poll_ready(&mut self, cx: &mut Context<'_>) -> Poll<Result<(), Self::Error>> {
        self.inner.poll_ready(cx).map_err(Into::into)
    }

    fn call(&mut self, uri: http::Uri) -> Self::Future {
        let domain = uri
            .host()
            .unwrap_or("")
            .trim_start_matches('[')
            .trim_end_matches(']')
            .to_string();
        if domain.is_empty() {
            return Box::pin(std::future::ready(Err("empty domain".into())));
        }
        let fut = self.inner.call(uri);
        let connector = self.connector.clone();

        Box::pin(async move {
            let stream = fut.await.map_err(Into::into)?;
            Ok(connector.hyper_connect(&domain, stream).await?)
        })
    }
}

impl TlsConnector {
    /// Connect to the given domain using the provided stream implementing Hyper I/O traits.
    pub async fn hyper_connect<S>(
        &self,
        domain: &str,
        stream: S,
    ) -> Result<TlsStream<Client, S>, TlsError>
    where
        S: Read + Write + Unpin + Send + 'static,
    {
        let mut conn = self.ctx.new_client_connection().build();
        conn.in_handshake()
            .expect("we have not started handshake")
            .set_tlsext_host_name(domain)?
            .set_host(domain)?;
        conn.set_io(HyperIo(stream))?;
        conn.async_handshake().await?;

        Ok(TlsStream::new(TokioTlsConnection::new(conn)))
    }
}

impl TlsAcceptor {
    /// Accept a new connection using the provided stream implementing Hyper I/O traits.
    pub async fn hyper_accept<S>(&self, stream: S) -> Result<TlsStream<Server, S>, TlsError>
    where
        S: Read + Write + Unpin + Send + 'static,
    {
        let mut conn = self.ctx.new_server_connection().build();
        conn.set_io(HyperIo(stream))?;
        conn.async_handshake().await?;

        Ok(TlsStream::new(TokioTlsConnection::new(conn)))
    }
}

/// IO object implementing [`hyper::rt::Read`] or [`hyper::rt::Write`] protocols.
pub struct HyperIo<T>(pub T);

fn hyper_async_read<T: Read>(
    mut this: Pin<&mut T>,
    ctx: &mut Context<'_>,
    buffer: &mut [u8],
) -> AbstractSocketResult {
    let mut buf = hyper::rt::ReadBuf::new(buffer);
    loop {
        return match this.as_mut().poll_read(ctx, buf.unfilled()) {
            Poll::Ready(Ok(())) => AbstractSocketResult::Ok(buf.filled().len()),
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

fn hyper_async_write<T: Write>(
    mut this: Pin<&mut T>,
    ctx: &mut Context<'_>,
    buffer: &[u8],
) -> AbstractSocketResult {
    loop {
        return match this.as_mut().poll_write(ctx, buffer) {
            Poll::Ready(Ok(bytes)) => AbstractSocketResult::Ok(bytes),
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

fn hyper_async_flush<T: Write>(
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

impl<T: Read + Send + Unpin> AbstractReader for HyperIo<T> {
    fn read(
        &mut self,
        async_ctx: Option<&mut Context<'_>>,
        buffer: &mut [u8],
    ) -> AbstractSocketResult {
        let Some(ctx) = async_ctx else {
            return AbstractSocketResult::Err(Box::new(NoAsyncContext));
        };
        hyper_async_read(Pin::new(&mut self.0), ctx, buffer)
    }
}

impl<T: Write + Send + Unpin> AbstractWriter for HyperIo<T> {
    fn write(
        &mut self,
        async_ctx: Option<&mut Context<'_>>,
        buffer: &[u8],
    ) -> AbstractSocketResult {
        let Some(ctx) = async_ctx else {
            return AbstractSocketResult::Err(Box::new(NoAsyncContext));
        };
        hyper_async_write(Pin::new(&mut self.0), ctx, buffer)
    }

    fn flush(&mut self, async_ctx: Option<&mut Context<'_>>) -> AbstractSocketResult {
        let Some(ctx) = async_ctx else {
            return AbstractSocketResult::Err(Box::new(NoAsyncContext));
        };
        hyper_async_flush(Pin::new(&mut self.0), ctx)
    }
}

impl<T: Read + Write + Send + Unpin> AbstractSocket for HyperIo<T> {}

impl<Role, S: Unpin> Read for TlsStream<Role, S> {
    fn poll_read(
        mut self: Pin<&mut Self>,
        cx: &mut Context<'_>,
        mut buf: ReadBufCursor<'_>,
    ) -> Poll<Result<(), io::Error>> {
        let mut read_buf = ReceiveBuffer::new_uninit(unsafe {
            // We will not uninitialize anything outside this buffer.
            buf.as_mut()
        });
        let status = match self
            .conn
            .inner
            .as_pin_mut()
            .async_poll_read(&mut read_buf, cx)
        {
            Ok(Some(status)) => status,
            Ok(None) => return Poll::Pending,
            Err(e) => return Poll::Ready(Err(crate::map_tls_err(e))),
        };
        match status {
            IoStatus::Ok(bytes) => {
                debug_assert_eq!(bytes, read_buf.written());
                unsafe {
                    // Safety: BoringSSL has successfully written and initialized `bytes` in the buffer.
                    buf.advance(bytes);
                }
                Poll::Ready(Ok(()))
            }
            _ => Poll::Ready(Err(io::Error::other("Unexpected I/O status"))),
        }
    }
}

impl<Role, S: Unpin> Write for TlsStream<Role, S> {
    fn poll_write(
        mut self: Pin<&mut Self>,
        cx: &mut Context<'_>,
        buf: &[u8],
    ) -> Poll<Result<usize, io::Error>> {
        let status = match self.conn.inner.as_pin_mut().async_poll_write(buf, cx) {
            Ok(Some(status)) => status,
            Ok(None) => return Poll::Pending,
            Err(e) => return Poll::Ready(Err(crate::map_tls_err(e))),
        };
        match status {
            IoStatus::Ok(bytes) => Poll::Ready(Ok(bytes)),
            _ => Poll::Ready(Err(io::Error::other("Unexpected I/O status"))),
        }
    }

    fn poll_flush(mut self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<Result<(), io::Error>> {
        let status = match self.conn.inner.as_pin_mut().async_poll_flush(cx) {
            Ok(Some(status)) => status,
            Ok(None) => return Poll::Pending,
            Err(e) => return Poll::Ready(Err(crate::map_tls_err(e))),
        };
        match status {
            IoStatus::Ok(_) => Poll::Ready(Ok(())),
            _ => Poll::Ready(Err(io::Error::other("Unexpected I/O status"))),
        }
    }

    fn poll_shutdown(
        mut self: Pin<&mut Self>,
        cx: &mut Context<'_>,
    ) -> Poll<Result<(), io::Error>> {
        match self.conn.inner.as_pin_mut().async_poll_shutdown(cx) {
            Ok(true) => Poll::Ready(Ok(())),
            Ok(false) => Poll::Pending,
            Err(e) => Poll::Ready(Err(io::Error::other(e))),
        }
    }
}
